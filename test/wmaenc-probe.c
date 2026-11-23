/* Probe for the Windows Media Audio encoder (wine-sg 9403; test/wmaenc-gate.sh).
 * CLSID_CWMAEncMediaObject as Microsoft documents it ("WMAudio Encoder") and as Wine's own conformance
 * test checks it (an MFT and a DMO, input type first, block-sized output samples), then what programs do
 * with it: NAudio's MediaFoundationEncoder.EncodeToWma lists the types with
 * MFTranscodeGetAudioOutputAvailableTypes and writes a .wma through a sink writer (mf.dll's ASF sink).
 * The written file is read back by a source reader (GStreamer's ASF demuxer and WMA decoder) and must
 * still hold the 1 kHz tone; its header (mf.dll's ContentInfo) must say WMA 8. */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mferror.h>
#include <mftransform.h>
#include <mfreadwrite.h>
#include <wmcodecdsp.h>
#include <math.h>
#include <stdio.h>

DEFINE_GUID(CLSID_WMAEnc_, 0x70f598e9, 0xf4ab, 0x495a, 0x99, 0xe2, 0xa7, 0xc4, 0xd3, 0xd8, 0x9a, 0xbf);
DEFINE_GUID(CLSID_WMADec_, 0x2eeb4adf, 0x4578, 0x4d10, 0xbc, 0xa7, 0xbb, 0x95, 0x5f, 0x56, 0x32, 0x0a);
#include <mediaobj.h>
#include <dmo.h>
#include <wmcontainer.h>

static int failures;
#define CHECK(c, ...) do { if (c) printf("ok    " __VA_ARGS__); else { failures++; printf("FAIL  " __VA_ARGS__); } } while (0)

#ifndef RATE
#define RATE 22050
#endif
#define SECONDS 3
#define TONE 1000.0

static short *make_tone(UINT32 channels, UINT32 *bytes)
{
    UINT32 i, c, n = RATE * SECONDS;
    short *pcm = malloc(n * channels * 2);
    for (i = 0; i < n; i++)
        for (c = 0; c < channels; c++)
            pcm[i * channels + c] = (short)(12000 * sin(2 * 3.14159265358979 * TONE * i / RATE));
    *bytes = n * channels * 2;
    return pcm;
}

static IMFSample *make_sample(const void *data, DWORD size, LONGLONG time, LONGLONG duration, BOOL set_time, BOOL set_duration)
{
    IMFMediaBuffer *buffer;
    IMFSample *sample;
    BYTE *dst;
    MFCreateMemoryBuffer(size ? size : 1, &buffer);
    if (data)
    {
        IMFMediaBuffer_Lock(buffer, &dst, NULL, NULL);
        memcpy(dst, data, size);
        IMFMediaBuffer_Unlock(buffer);
        IMFMediaBuffer_SetCurrentLength(buffer, size);
    }
    MFCreateSample(&sample);
    IMFSample_AddBuffer(sample, buffer);
    IMFMediaBuffer_Release(buffer);
    if (set_time) IMFSample_SetSampleTime(sample, time);
    if (set_duration) IMFSample_SetSampleDuration(sample, duration);
    return sample;
}

static IMFMediaType *pcm_type(UINT32 channels)
{
    IMFMediaType *t;
    MFCreateMediaType(&t);
    IMFMediaType_SetGUID(t, &MF_MT_MAJOR_TYPE, &MFMediaType_Audio);
    IMFMediaType_SetGUID(t, &MF_MT_SUBTYPE, &MFAudioFormat_PCM);
    IMFMediaType_SetUINT32(t, &MF_MT_AUDIO_NUM_CHANNELS, channels);
    IMFMediaType_SetUINT32(t, &MF_MT_AUDIO_BITS_PER_SAMPLE, 16);
    IMFMediaType_SetUINT32(t, &MF_MT_AUDIO_SAMPLES_PER_SECOND, RATE);
    return t;
}

static inline IMFMediaType *aac_type(UINT32 channels, UINT32 bytes, int payload)
{
    IMFMediaType *t;
    MFCreateMediaType(&t);
    IMFMediaType_SetGUID(t, &MF_MT_MAJOR_TYPE, &MFMediaType_Audio);
    IMFMediaType_SetGUID(t, &MF_MT_SUBTYPE, &MFAudioFormat_AAC);
    IMFMediaType_SetUINT32(t, &MF_MT_AUDIO_NUM_CHANNELS, channels);
    IMFMediaType_SetUINT32(t, &MF_MT_AUDIO_BITS_PER_SAMPLE, 16);
    IMFMediaType_SetUINT32(t, &MF_MT_AUDIO_SAMPLES_PER_SECOND, RATE);
    IMFMediaType_SetUINT32(t, &MF_MT_AUDIO_AVG_BYTES_PER_SECOND, bytes);
    if (payload >= 0) IMFMediaType_SetUINT32(t, &MF_MT_AAC_PAYLOAD_TYPE, payload);
    return t;
}

/* the dominant frequency by zero crossings of channel 0, over the middle of the signal */
static double tone_of(const short *pcm, UINT32 frames, UINT32 channels, UINT32 rate)
{
    UINT32 i, crossings = 0, start = frames / 4, end = frames * 3 / 4;
    double energy = 0;
    if (end <= start + 2) return 0;
    for (i = start + 1; i < end; i++)
    {
        if ((pcm[(i - 1) * channels] < 0) != (pcm[i * channels] < 0)) crossings++;
        energy += (double)pcm[i * channels] * pcm[i * channels];
    }
    if (energy / (end - start) < 1000.0 * 1000.0) return 0; /* silence */
    return crossings * (double)rate / 2.0 / (end - start);
}

struct frames { BYTE *data; DWORD *sizes; LONGLONG *times, *durations; UINT32 count; size_t total; };

static void frames_add(struct frames *f, const BYTE *data, DWORD size, LONGLONG time, LONGLONG duration)
{
    f->data = realloc(f->data, f->total + size);
    memcpy(f->data + f->total, data, size);
    f->total += size;
    f->sizes = realloc(f->sizes, (f->count + 1) * sizeof(DWORD));
    f->times = realloc(f->times, (f->count + 1) * sizeof(LONGLONG));
    f->durations = realloc(f->durations, (f->count + 1) * sizeof(LONGLONG));
    f->sizes[f->count] = size; f->times[f->count] = time; f->durations[f->count] = duration;
    f->count++;
}

static HRESULT drain_output(IMFTransform *t, DWORD size, struct frames *f, int *unclean)
{
    MFT_OUTPUT_DATA_BUFFER out;
    DWORD status, len;
    HRESULT hr;
    for (;;)
    {
        IMFMediaBuffer *b;
        LONGLONG time = -1, duration = -1;
        UINT32 clean = 0;
        BYTE *p;
        memset(&out, 0, sizeof(out));
        out.pSample = make_sample(NULL, size, 0, 0, FALSE, FALSE);
        hr = IMFTransform_ProcessOutput(t, 0, 1, &out, &status);
        if (FAILED(hr)) { IMFSample_Release(out.pSample); if (out.pEvents) IMFCollection_Release(out.pEvents); return hr; }
        IMFSample_GetSampleTime(out.pSample, &time);
        IMFSample_GetSampleDuration(out.pSample, &duration);
        IMFSample_GetUINT32(out.pSample, &MFSampleExtension_CleanPoint, &clean);
        if (!clean && unclean) (*unclean)++;
        IMFSample_ConvertToContiguousBuffer(out.pSample, &b);
        IMFMediaBuffer_Lock(b, &p, NULL, &len);
        frames_add(f, p, len, time, duration);
        IMFMediaBuffer_Unlock(b);
        IMFMediaBuffer_Release(b);
        IMFSample_Release(out.pSample);
    }
}


static void test_wma(void)
{
    MFT_REGISTER_TYPE_INFO in = {MFMediaType_Audio, MFAudioFormat_PCM}, out = {MFMediaType_Audio, MFAudioFormat_WMAudioV8};
    IMFActivate **acts = NULL;
    UINT32 count = 0, i, found = 0, block = 0, bytes = 0, bad_size = 0, bad_time = 0;
    IMFTransform *t = NULL, *d = NULL;
    IMFMediaType *type, *cur = NULL, *pcm;
    MFT_OUTPUT_STREAM_INFO oinfo;
    MFT_INPUT_STREAM_INFO iinfo;
    struct frames f = {0}, dec = {0};
    WCHAR *name = NULL;
    IUnknown *unk;
    UINT32 pcm_bytes;
    short *tone_pcm;


    HRESULT hr;

    hr = MFTEnumEx(MFT_CATEGORY_AUDIO_ENCODER, MFT_ENUM_FLAG_SYNCMFT | MFT_ENUM_FLAG_LOCALMFT, &in, &out, &acts, &count);
    for (i = 0; i < count; i++)
    {
        GUID clsid;
        if (SUCCEEDED(IMFActivate_GetGUID(acts[i], &MFT_TRANSFORM_CLSID_Attribute, &clsid)) && IsEqualGUID(&clsid, &CLSID_WMAEnc_)) found = 1;
        IMFActivate_Release(acts[i]);
    }
    CoTaskMemFree(acts);
    CHECK(hr == S_OK && found, "MFTEnumEx finds CLSID_CWMAEncMediaObject for PCM -> WMAudioV8 (%u MFTs)\n", count);
    hr = MFTGetInfo(CLSID_WMAEnc_, &name, NULL, NULL, NULL, NULL, NULL);
    CHECK(hr == S_OK && name && !wcscmp(name, L"WMAudio Encoder MFT"), "MFT name %ls\n", name ? name : L"(none)");
    CoTaskMemFree(name);
    {
        WCHAR dmo_name[80] = {0};
        hr = DMOGetName(&CLSID_WMAEnc_, dmo_name);
        CHECK(hr == S_OK && !wcscmp(dmo_name, L"WMAudio Encoder DMO"), "DMO name %ls\n", dmo_name);
    }

    hr = CoCreateInstance(&CLSID_WMAEnc_, NULL, CLSCTX_INPROC_SERVER, &IID_IMFTransform, (void **)&t);
    CHECK(hr == S_OK, "CoCreateInstance %#lx\n", hr);
    if (!t) return;
    CHECK(SUCCEEDED(IMFTransform_QueryInterface(t, &IID_IMediaObject, (void **)&unk)), "it is a DMO too\n");
    if (unk) IUnknown_Release(unk);
    CHECK(SUCCEEDED(IMFTransform_QueryInterface(t, &IID_IPropertyStore, (void **)&unk)), "with a property store\n");
    if (unk) IUnknown_Release(unk);
    CHECK(IMFTransform_GetInputStreamInfo(t, 0, &iinfo) == MF_E_TRANSFORM_TYPE_NOT_SET, "no stream info before the types\n");
    

    pcm = pcm_type(2);
    IMFMediaType_SetUINT32(pcm, &MF_MT_AUDIO_BLOCK_ALIGNMENT, 4);
    IMFMediaType_SetUINT32(pcm, &MF_MT_AUDIO_AVG_BYTES_PER_SECOND, RATE * 4);
    hr = IMFTransform_SetInputType(t, 0, pcm, 0);
    CHECK(hr == S_OK, "SetInputType (input first) %#lx\n", hr);
    for (count = 0; SUCCEEDED(IMFTransform_GetOutputAvailableType(t, 0, count, &type)); count++)
    {
        UINT32 b = 0, by = 0, ch = 0, r = 0, ud = 0;
        GUID sub;
        IMFMediaType_GetGUID(type, &MF_MT_SUBTYPE, &sub);
        IMFMediaType_GetUINT32(type, &MF_MT_AUDIO_BLOCK_ALIGNMENT, &b);
        IMFMediaType_GetUINT32(type, &MF_MT_AUDIO_AVG_BYTES_PER_SECOND, &by);
        IMFMediaType_GetUINT32(type, &MF_MT_AUDIO_NUM_CHANNELS, &ch);
        IMFMediaType_GetUINT32(type, &MF_MT_AUDIO_SAMPLES_PER_SECOND, &r);
        IMFMediaType_GetBlobSize(type, &MF_MT_USER_DATA, &ud);
        if (!IsEqualGUID(&sub, &MFAudioFormat_WMAudioV8) || !b || !by || ch != 2 || r != RATE || ud != 10) bad_size++;
        if (!cur && by >= 3500 && by <= 6500) { cur = type; IMFMediaType_AddRef(cur); }
        IMFMediaType_Release(type);
    }
    CHECK(count >= 3 && !bad_size, "%u WMA 8 output types for the input (%u malformed)\n", count, bad_size);
    CHECK(cur != NULL, "one at about 32 kbps\n");
    if (!cur) { IMFTransform_Release(t); IMFMediaType_Release(pcm); return; }
    IMFMediaType_GetUINT32(cur, &MF_MT_AUDIO_BLOCK_ALIGNMENT, &block);
    hr = IMFTransform_SetOutputType(t, 0, cur, 0);
    CHECK(hr == S_OK, "SetOutputType %#lx\n", hr);
    IMFMediaType_Release(cur);
    hr = IMFTransform_GetOutputCurrentType(t, 0, &cur);
    if (SUCCEEDED(hr))
    {
        UINT32 cb = 0;
        IMFMediaType_GetUINT32(cur, &MF_MT_AUDIO_BLOCK_ALIGNMENT, &cb);
        IMFMediaType_GetUINT32(cur, &MF_MT_AUDIO_AVG_BYTES_PER_SECOND, &bytes);
        CHECK(cb == block, "the current type keeps the block size %u (%u)\n", block, cb);
        {
            BYTE ud[16] = {0};
            UINT32 len = 0, k, nonzero = 0;
            IMFMediaType_GetBlob(cur, &MF_MT_USER_DATA, ud, sizeof(ud), &len);
            for (k = 0; k < len; k++) nonzero |= ud[k];
            CHECK(len == 10 && nonzero, "and codec data a decoder can use (%u bytes, encode options %02x)\n", len, ud[4]);
        }
    }
    memset(&oinfo, 0, sizeof(oinfo));
    IMFTransform_GetOutputStreamInfo(t, 0, &oinfo);
    CHECK(oinfo.cbSize == block && oinfo.cbAlignment == 1, "output buffers are one block (%lu)\n", oinfo.cbSize);
    IMFTransform_GetInputStreamInfo(t, 0, &iinfo);
    CHECK(iinfo.cbSize == 4 && iinfo.cbAlignment == 1, "input buffers are whole sample frames (%lu)\n", iinfo.cbSize);

    tone_pcm = make_tone(2, &pcm_bytes);
    for (i = 0; i < SECONDS * 10; i++)
    {
        DWORD piece = pcm_bytes / (SECONDS * 10);
        IMFSample *s = make_sample((BYTE *)tone_pcm + i * piece, piece, i * 1000000LL, 1000000, TRUE, TRUE);
        hr = IMFTransform_ProcessInput(t, 0, s, 0);
        IMFSample_Release(s);
        if (FAILED(hr)) { CHECK(0, "ProcessInput %u %#lx\n", i, hr); break; }
        drain_output(t, block, &f, NULL);
    }
    IMFTransform_ProcessMessage(t, MFT_MESSAGE_COMMAND_DRAIN, 0);
    hr = drain_output(t, block, &f, NULL);
    CHECK(hr == MF_E_TRANSFORM_NEED_MORE_INPUT, "drained %#lx\n", hr);
    for (i = 0; i < f.count; i++)
    {
        if (f.sizes[i] != block) bad_size++;
        if (i && f.times[i] != f.times[i - 1] + f.durations[i - 1]) bad_time++;
    }
    CHECK(f.count > 50 && !bad_size, "%u output samples of one block (%u) each (%u not, first %lu)\n", f.count, block, bad_size,
          f.count ? f.sizes[0] : 0);
    CHECK(f.count && f.times[0] == 0 && !bad_time, "back to back from 0 (%u gaps)\n", bad_time);
    CHECK(f.total > 0 && f.total < (size_t)bytes * (SECONDS + 1), "%u bytes at %u bytes/s\n", (unsigned)f.total, bytes);

    if (d) IMFTransform_Release(d);
    if (cur) IMFMediaType_Release(cur);
    IMFMediaType_Release(pcm);
    IMFTransform_Release(t);
    free(tone_pcm); free(f.data); free(f.sizes); free(f.times); free(f.durations);
    free(dec.data); free(dec.sizes); free(dec.times); free(dec.durations);
}

/* NAudio MediaFoundationEncoder.EncodeToWma: the available types, then a sink writer to a .wma */
static void test_sink_writer(void)
{
    IMFCollection *types = NULL;
    IMFSinkWriter *writer = NULL;
    IMFSourceReader *reader = NULL;
    IMFMediaType *out = NULL, *in, *native;
    DWORD stream = 0, count = 0, flags, total = 0, i;
    WCHAR path[MAX_PATH];
    UINT32 bytes;
    short *pcm, *decoded = NULL;
    HRESULT hr;

    printf("-- sink writer (.wma)\n");
    hr = MFTranscodeGetAudioOutputAvailableTypes(&MFAudioFormat_WMAudioV8, MFT_ENUM_FLAG_ALL, NULL, &types);
    if (SUCCEEDED(hr)) IMFCollection_GetElementCount(types, &count);
    CHECK(hr == S_OK && count > 0, "MFTranscodeGetAudioOutputAvailableTypes(WMAudioV8): %lu types (%#lx)\n", count, hr);
    for (i = 0; i < count && !out; i++)
    {
        IUnknown *u; IMFMediaType *t; UINT32 rate = 0, ch = 0;
        IMFCollection_GetElement(types, i, &u);
        IUnknown_QueryInterface(u, &IID_IMFMediaType, (void **)&t);
        IUnknown_Release(u);
        IMFMediaType_GetUINT32(t, &MF_MT_AUDIO_SAMPLES_PER_SECOND, &rate);
        IMFMediaType_GetUINT32(t, &MF_MT_AUDIO_NUM_CHANNELS, &ch);
        if (rate == RATE && ch == 2) out = t; else IMFMediaType_Release(t);
    }
    if (types) IMFCollection_Release(types);
    CHECK(out != NULL, "a %u Hz stereo type is listed\n", RATE);
    if (!out) return;

    GetTempPathW(MAX_PATH, path);
    wcscat(path, L"wmaenc-probe.wma");
#ifdef KEEP_FILE
    wcscpy(path, L"Z:\\var\\tmp\\gmedia\\wma-out.wma");
#endif
    DeleteFileW(path);
    hr = MFCreateSinkWriterFromURL(path, NULL, NULL, &writer);
    CHECK(hr == S_OK, "MFCreateSinkWriterFromURL(.wma) %#lx\n", hr);
    if (FAILED(hr)) { IMFMediaType_Release(out); return; }
    hr = IMFSinkWriter_AddStream(writer, out, &stream);
    CHECK(hr == S_OK, "AddStream(WMA) %#lx\n", hr);
    in = pcm_type(2);
    IMFMediaType_SetUINT32(in, &MF_MT_AUDIO_BLOCK_ALIGNMENT, 4);
    IMFMediaType_SetUINT32(in, &MF_MT_AUDIO_AVG_BYTES_PER_SECOND, RATE * 4);
    hr = IMFSinkWriter_SetInputMediaType(writer, stream, in, NULL);
    CHECK(hr == S_OK, "SetInputMediaType(PCM): the writer finds the encoder %#lx\n", hr);
    IMFMediaType_Release(in);
    IMFMediaType_Release(out);
    if (SUCCEEDED(hr)) hr = IMFSinkWriter_BeginWriting(writer);
    CHECK(hr == S_OK, "BeginWriting %#lx\n", hr);
    pcm = make_tone(2, &bytes);
#ifndef CHUNK
    for (i = 0; SUCCEEDED(hr) && i < SECONDS * 10; i++)
    {
        DWORD piece = bytes / (SECONDS * 10);
        IMFSample *s = make_sample((BYTE *)pcm + i * piece, piece, i * 1000000LL, 1000000, TRUE, TRUE);
#else
    for (i = 0; SUCCEEDED(hr) && i * CHUNK < bytes; i++)
    {
        DWORD piece = min(CHUNK, bytes - i * CHUNK);
        IMFSample *s = make_sample((BYTE *)pcm + i * CHUNK, piece, (LONGLONG)i * CHUNK / 4 * 10000000 / RATE,
                (LONGLONG)piece / 4 * 10000000 / RATE, TRUE, TRUE);
#endif
        hr = IMFSinkWriter_WriteSample(writer, stream, s);
        IMFSample_Release(s);
    }
    CHECK(hr == S_OK, "WriteSample %#lx\n", hr);
    hr = IMFSinkWriter_Finalize(writer);
    CHECK(hr == S_OK, "Finalize %#lx\n", hr);
    IMFSinkWriter_Release(writer);
    free(pcm);

    hr = MFCreateSourceReaderFromURL(path, NULL, &reader);
    CHECK(hr == S_OK, "the .wma opens in a source reader %#lx\n", hr);
    if (FAILED(hr)) return;
    /* the file's stream is WMA 8: its ASF header, read with mf.dll's ContentInfo (Wine's source reader hands
     * out decoded audio as the native type) */
    {
        HANDLE h = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
        DWORD got = 0;
        BYTE head[8192];
        IMFMediaBuffer *b;
        IMFASFContentInfo *info;
        IMFASFProfile *profile;
        IMFASFStreamConfig *sc;
        GUID sub = {0};
        BYTE *p;

        ReadFile(h, head, sizeof(head), &got, NULL);
        CloseHandle(h);
        MFCreateMemoryBuffer(got, &b);
        IMFMediaBuffer_Lock(b, &p, NULL, NULL); memcpy(p, head, got); IMFMediaBuffer_Unlock(b);
        IMFMediaBuffer_SetCurrentLength(b, got);
        MFCreateASFContentInfo(&info);
        if (SUCCEEDED(IMFASFContentInfo_ParseHeader(info, b, 0)) && SUCCEEDED(IMFASFContentInfo_GetProfile(info, &profile)))
        {
            WORD n;
            if (SUCCEEDED(IMFASFProfile_GetStream(profile, 0, &n, &sc)))
            {
                if (SUCCEEDED(IMFASFStreamConfig_GetMediaType(sc, &native)))
                {
                    IMFMediaType_GetGUID(native, &MF_MT_SUBTYPE, &sub);
                    IMFMediaType_Release(native);
                }
                IMFASFStreamConfig_Release(sc);
            }
            IMFASFProfile_Release(profile);
        }
        IMFASFContentInfo_Release(info);
        IMFMediaBuffer_Release(b);
        CHECK(IsEqualGUID(&sub, &MFAudioFormat_WMAudioV8), "its audio stream is WMA 8 (%08lx)\n", sub.Data1);
    }
    in = pcm_type(2);
    hr = IMFSourceReader_SetCurrentMediaType(reader, MF_SOURCE_READER_FIRST_AUDIO_STREAM, NULL, in);
    IMFMediaType_Release(in);
    CHECK(hr == S_OK, "read as PCM %#lx\n", hr);
    for (;;)
    {
        IMFSample *s = NULL; LONGLONG ts; IMFMediaBuffer *b; BYTE *p; DWORD len;
        hr = IMFSourceReader_ReadSample(reader, MF_SOURCE_READER_FIRST_AUDIO_STREAM, 0, NULL, &flags, &ts, &s);
        if (FAILED(hr) || (flags & MF_SOURCE_READERF_ENDOFSTREAM) || !s) { if (s) IMFSample_Release(s); break; }
        IMFSample_ConvertToContiguousBuffer(s, &b);
        IMFMediaBuffer_Lock(b, &p, NULL, &len);
        decoded = realloc(decoded, total + len);
        memcpy((BYTE *)decoded + total, p, len);
        total += len;
        IMFMediaBuffer_Unlock(b);
        IMFMediaBuffer_Release(b);
        IMFSample_Release(s);
    }
    IMFSourceReader_Release(reader);
    CHECK(total > RATE * 4 * SECONDS * 9 / 10, "the file holds %lu PCM bytes (~%u)\n", total, RATE * 4 * SECONDS);
    {
        double tone = decoded ? tone_of(decoded, total / 4, 2, RATE) : 0;
        CHECK(fabs(tone - TONE) < TONE * 0.03, "and it is the %.0f Hz tone\n", tone);
    }
    free(decoded);
#ifndef KEEP_FILE
    DeleteFileW(path);
#endif
}

int main(void)
{
    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    MFStartup(MF_VERSION, MFSTARTUP_FULL);
#ifndef ONLY_SINK
    test_wma();
#endif
    test_sink_writer();
    MFShutdown();
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
