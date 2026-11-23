/* Probe for the MP3 encoder MFT (wine-sg 9404; test/mp3enc-gate.sh).
 * CLSID_MP3ACMCodecWrapper as Microsoft documents it ("MP3 Audio Encoder", Windows 8 and later): output
 * type first, the standard's bit rates, MF_MT_USER_DATA the MPEGLAYER3WAVEFORMAT tail; then what programs
 * do with it: NAudio's MediaFoundationEncoder.EncodeToMp3 lists the types with
 * MFTranscodeGetAudioOutputAvailableTypes and writes an .mp3 through a sink writer. The written file is
 * read back with a source reader and must still be the 1 kHz tone. */
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

DEFINE_GUID(CLSID_MP3Enc_, 0x11103421, 0x354c, 0x4cca, 0xa7, 0xa3, 0x1a, 0xff, 0x9a, 0x5b, 0x67, 0x01);
DEFINE_GUID(CLSID_MP3Dec_, 0xbbeea841, 0x0a63, 0x4f52, 0xa7, 0xab, 0xa9, 0xb3, 0xa8, 0x4e, 0xd3, 0x8a);
#include <mediaobj.h>
#include <dmo.h>

static int failures;
#define CHECK(c, ...) do { if (c) printf("ok    " __VA_ARGS__); else { failures++; printf("FAIL  " __VA_ARGS__); } } while (0)

#define RATE 44100
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


static IMFMediaType *mp3_type(UINT32 kbps)
{
    IMFMediaType *t;
    MFCreateMediaType(&t);
    IMFMediaType_SetGUID(t, &MF_MT_MAJOR_TYPE, &MFMediaType_Audio);
    IMFMediaType_SetGUID(t, &MF_MT_SUBTYPE, &MFAudioFormat_MP3);
    IMFMediaType_SetUINT32(t, &MF_MT_AUDIO_NUM_CHANNELS, 2);
    IMFMediaType_SetUINT32(t, &MF_MT_AUDIO_SAMPLES_PER_SECOND, RATE);
    IMFMediaType_SetUINT32(t, &MF_MT_AUDIO_AVG_BYTES_PER_SECOND, kbps * 1000 / 8);
    return t;
}

static void test_mp3(void)
{
    MFT_REGISTER_TYPE_INFO in = {MFMediaType_Audio, MFAudioFormat_PCM}, out = {MFMediaType_Audio, MFAudioFormat_MP3};
    IMFActivate **acts = NULL;
    UINT32 count = 0, i, found = 0, size = 0, bad = 0, bad_time = 0;
    IMFTransform *t = NULL, *d = NULL;
    IMFMediaType *type, *cur = NULL, *pcm;
    struct frames f = {0}, dec = {0};
    WCHAR *name = NULL;
    BYTE ud[32];
    UINT32 pcm_bytes;
    short *tone_pcm;
    size_t off = 0;
    HRESULT hr;

    hr = MFTEnumEx(MFT_CATEGORY_AUDIO_ENCODER, MFT_ENUM_FLAG_SYNCMFT | MFT_ENUM_FLAG_LOCALMFT, &in, &out, &acts, &count);
    for (i = 0; i < count; i++)
    {
        GUID clsid;
        if (SUCCEEDED(IMFActivate_GetGUID(acts[i], &MFT_TRANSFORM_CLSID_Attribute, &clsid)) && IsEqualGUID(&clsid, &CLSID_MP3Enc_)) found = 1;
        IMFActivate_Release(acts[i]);
    }
    CoTaskMemFree(acts);
    CHECK(hr == S_OK && found, "MFTEnumEx finds CLSID_MP3ACMCodecWrapper for PCM -> MP3 (%u MFTs)\n", count);
    hr = MFTGetInfo(CLSID_MP3Enc_, &name, NULL, NULL, NULL, NULL, NULL);
    CHECK(hr == S_OK && name && !wcscmp(name, L"MP3 Encoder ACM Wrapper MFT"), "MFT name %ls\n", name ? name : L"(none)");
    CoTaskMemFree(name);
    hr = CoCreateInstance(&CLSID_MP3Enc_, NULL, CLSCTX_INPROC_SERVER, &IID_IMFTransform, (void **)&t);
    CHECK(hr == S_OK, "CoCreateInstance %#lx\n", hr);
    if (!t) return;

    pcm = pcm_type(2);
    IMFMediaType_SetUINT32(pcm, &MF_MT_AUDIO_BLOCK_ALIGNMENT, 4);
    IMFMediaType_SetUINT32(pcm, &MF_MT_AUDIO_AVG_BYTES_PER_SECOND, RATE * 4);
    hr = IMFTransform_SetInputType(t, 0, pcm, 0);
    CHECK(hr == MF_E_TRANSFORM_TYPE_NOT_SET, "the output type is set first (%#lx)\n", hr);
    for (count = 0; SUCCEEDED(IMFTransform_GetOutputAvailableType(t, 0, count, &type)); count++) IMFMediaType_Release(type);
    CHECK(count >= 14, "%u output types (rates x channels x bit rates)\n", count);
    type = mp3_type(100);
    CHECK(FAILED(IMFTransform_SetOutputType(t, 0, type, 0)), "100 kbps is not a layer 3 bit rate\n");
    IMFMediaType_Release(type);
    type = mp3_type(128);
    hr = IMFTransform_SetOutputType(t, 0, type, 0);
    CHECK(hr == S_OK, "SetOutputType 128 kbps %#lx\n", hr);
    IMFMediaType_Release(type);
    if (SUCCEEDED(IMFTransform_GetOutputCurrentType(t, 0, &cur)))
    {
        hr = IMFMediaType_GetBlob(cur, &MF_MT_USER_DATA, ud, sizeof(ud), &size);
        CHECK(hr == S_OK && size == 12 && *(WORD *)ud == 1 && *(WORD *)(ud + 6) == 417 && *(WORD *)(ud + 8) == 1,
              "MF_MT_USER_DATA is MPEGLAYER3WAVEFORMAT's tail (%u bytes, block %u)\n", size, size >= 8 ? *(WORD *)(ud + 6) : 0);
    }
    hr = IMFTransform_SetInputType(t, 0, pcm, 0);
    CHECK(hr == S_OK, "SetInputType %#lx\n", hr);

    tone_pcm = make_tone(2, &pcm_bytes);
    for (i = 0; i < SECONDS * 10; i++)
    {
        DWORD piece = pcm_bytes / (SECONDS * 10);
        IMFSample *s = make_sample((BYTE *)tone_pcm + i * piece, piece, i * 1000000LL, 1000000, TRUE, TRUE);
        hr = IMFTransform_ProcessInput(t, 0, s, 0);
        IMFSample_Release(s);
        if (FAILED(hr)) { CHECK(0, "ProcessInput %u %#lx\n", i, hr); break; }
        drain_output(t, 1441, &f, NULL);
    }
    IMFTransform_ProcessMessage(t, MFT_MESSAGE_COMMAND_DRAIN, 0);
    hr = drain_output(t, 1441, &f, NULL);
    CHECK(hr == MF_E_TRANSFORM_NEED_MORE_INPUT, "drained %#lx\n", hr);
    for (i = 0; i < f.count; i++)
    {
        const BYTE *h = f.data + off;
        if (f.sizes[i] < 417 || f.sizes[i] > 418 || h[0] != 0xff || (h[1] & 0xfe) != 0xfa) bad++;
        if (i && f.times[i] != f.times[i - 1] + f.durations[i - 1]) bad_time++;
        off += f.sizes[i];
    }
    CHECK(f.count + 3 >= RATE * SECONDS / 1152 && f.count <= RATE * SECONDS / 1152 + 3 && !bad,
          "%u output samples, one MPEG-1 layer 3 frame each (%u not)\n", f.count, bad);
    CHECK(f.count && f.times[0] == 0 && !bad_time && (f.durations[0] == 261224 || f.durations[0] == 261225),
          "1152 samples long, back to back from 0 (%u gaps)\n", bad_time);

    if (d) IMFTransform_Release(d);
    if (cur) IMFMediaType_Release(cur);
    IMFMediaType_Release(pcm);
    IMFTransform_Release(t);
    free(tone_pcm); free(f.data); free(f.sizes); free(f.times); free(f.durations);
    free(dec.data); free(dec.sizes); free(dec.times); free(dec.durations);
}

/* NAudio MediaFoundationEncoder.EncodeToMp3: the available types, then a sink writer to an .mp3 */
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

    printf("-- sink writer (.mp3)\n");
    hr = MFTranscodeGetAudioOutputAvailableTypes(&MFAudioFormat_MP3, MFT_ENUM_FLAG_ALL, NULL, &types);
    if (SUCCEEDED(hr)) IMFCollection_GetElementCount(types, &count);
    CHECK(hr == S_OK && count > 0, "MFTranscodeGetAudioOutputAvailableTypes(MP3): %lu types (%#lx)\n", count, hr);
    for (i = 0; i < count && !out; i++)
    {
        IUnknown *u; IMFMediaType *t; UINT32 rate = 0, ch = 0;
        IMFCollection_GetElement(types, i, &u);
        IUnknown_QueryInterface(u, &IID_IMFMediaType, (void **)&t);
        IUnknown_Release(u);
        IMFMediaType_GetUINT32(t, &MF_MT_AUDIO_SAMPLES_PER_SECOND, &rate);
        IMFMediaType_GetUINT32(t, &MF_MT_AUDIO_NUM_CHANNELS, &ch);
        UINT32 b = 0; IMFMediaType_GetUINT32(t, &MF_MT_AUDIO_AVG_BYTES_PER_SECOND, &b); if (rate == RATE && ch == 2 && b == 16000) out = t; else IMFMediaType_Release(t);
    }
    if (types) IMFCollection_Release(types);
    CHECK(out != NULL, "a %u Hz stereo type is listed\n", RATE);
    if (!out) return;

    GetTempPathW(MAX_PATH, path);
    wcscat(path, L"mp3enc-probe.mp3");
    DeleteFileW(path);
    hr = MFCreateSinkWriterFromURL(path, NULL, NULL, &writer);
    CHECK(hr == S_OK, "MFCreateSinkWriterFromURL(.mp3) %#lx\n", hr);
    if (FAILED(hr)) { IMFMediaType_Release(out); return; }
    hr = IMFSinkWriter_AddStream(writer, out, &stream);
    CHECK(hr == S_OK, "AddStream(MP3) %#lx\n", hr);
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
    for (i = 0; SUCCEEDED(hr) && i < SECONDS * 10; i++)
    {
        DWORD piece = bytes / (SECONDS * 10);
        IMFSample *s = make_sample((BYTE *)pcm + i * piece, piece, i * 1000000LL, 1000000, TRUE, TRUE);
        hr = IMFSinkWriter_WriteSample(writer, stream, s);
        IMFSample_Release(s);
    }
    CHECK(hr == S_OK, "WriteSample %#lx\n", hr);
    hr = IMFSinkWriter_Finalize(writer);
    CHECK(hr == S_OK, "Finalize %#lx\n", hr);
    IMFSinkWriter_Release(writer);
    free(pcm);

    hr = MFCreateSourceReaderFromURL(path, NULL, &reader);
    CHECK(hr == S_OK, "the .mp3 opens in a source reader %#lx\n", hr);
    if (FAILED(hr)) return;
    /* the file is MP3 (an ID3 tag or a frame first): Wine's source reader hands out decoded audio as the
     * native type, so the file itself is looked at */
    {
        HANDLE h = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
        BYTE head[16] = {0};
        DWORD got = 0;
        ReadFile(h, head, sizeof(head), &got, NULL);
        CloseHandle(h);
        CHECK(got >= 4 && (!memcmp(head, "ID3", 3) || (head[0] == 0xff && (head[1] & 0xe0) == 0xe0)),
              "its audio stream is MP3 (%02x %02x %02x)\n", head[0], head[1], head[2]);
    }
    (void)native;
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
    DeleteFileW(path);
}

int main(void)
{
    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    MFStartup(MF_VERSION, MFSTARTUP_FULL);
    test_mp3();
    test_sink_writer();
    MFShutdown();
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
