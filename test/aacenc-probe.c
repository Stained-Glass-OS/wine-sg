/* Probe for the Media Foundation AAC encoder (wine-sg 9400; test/aacenc-gate.sh).
 * The documented contract of CLSID_AACMFTEncoder ("AAC Encoder", Microsoft Learn), then what programs do
 * with it: NAudio's MediaFoundationEncoder lists the encoder's types with
 * MFTranscodeGetAudioOutputAvailableTypes and writes an .m4a/.mp4 through a sink writer; screen recorders
 * (ScreenRecorderLib) do the same with a video stream beside it. The encoded audio is decoded again
 * (CLSID_MSAACDecMFT, and a source reader on the written file) and must still be the 1 kHz tone. */
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

DEFINE_GUID(CLSID_AACMFTEncoder_, 0x93af0c51, 0x2275, 0x45d2, 0xa3, 0x5b, 0xf2, 0xba, 0x21, 0xca, 0xed, 0x00);
DEFINE_GUID(CLSID_MSAACDecMFT_, 0x32d186a7, 0x218f, 0x4c75, 0x88, 0x76, 0xdd, 0x77, 0x27, 0x3a, 0x89, 0x99);

static int failures;
#define CHECK(c, ...) do { if (c) printf("ok    " __VA_ARGS__); else { failures++; printf("FAIL  " __VA_ARGS__); } } while (0)

#define RATE 44100
#define SECONDS 2
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

static IMFMediaType *aac_type(UINT32 channels, UINT32 bytes, int payload)
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

static IMFTransform *create_encoder(void)
{
    IMFTransform *t = NULL;
    CoCreateInstance(&CLSID_AACMFTEncoder_, NULL, CLSCTX_INPROC_SERVER, &IID_IMFTransform, (void **)&t);
    return t;
}

static void test_registration(void)
{
    MFT_REGISTER_TYPE_INFO in = {MFMediaType_Audio, MFAudioFormat_PCM}, out = {MFMediaType_Audio, MFAudioFormat_AAC};
    IMFActivate **acts = NULL;
    UINT32 count = 0, i, found = 0;
    WCHAR *name = NULL;
    HRESULT hr;

    hr = MFTEnumEx(MFT_CATEGORY_AUDIO_ENCODER, MFT_ENUM_FLAG_SYNCMFT | MFT_ENUM_FLAG_LOCALMFT, &in, &out, &acts, &count);
    for (i = 0; i < count; i++)
    {
        GUID clsid;
        if (SUCCEEDED(IMFActivate_GetGUID(acts[i], &MFT_TRANSFORM_CLSID_Attribute, &clsid)) && IsEqualGUID(&clsid, &CLSID_AACMFTEncoder_)) found = 1;
        IMFActivate_Release(acts[i]);
    }
    CoTaskMemFree(acts);
    CHECK(hr == S_OK && found, "MFTEnumEx finds CLSID_AACMFTEncoder for PCM -> AAC (hr %#lx, %u MFTs)\n", hr, count);
    hr = MFTGetInfo(CLSID_AACMFTEncoder_, &name, NULL, NULL, NULL, NULL, NULL);
    CHECK(hr == S_OK && name && !wcscmp(name, L"Microsoft AAC Audio Encoder MFT"), "MFTGetInfo name %ls\n", name ? name : L"(none)");
    CoTaskMemFree(name);
}

static void test_types(void)
{
    static const BYTE expect_user_data[] = {0x00,0x00,0x29,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x12,0x08};
    static const GUID *const out_required[] = {&MF_MT_MAJOR_TYPE, &MF_MT_SUBTYPE, &MF_MT_AUDIO_NUM_CHANNELS,
            &MF_MT_AUDIO_BITS_PER_SAMPLE, &MF_MT_AUDIO_SAMPLES_PER_SECOND, &MF_MT_AUDIO_AVG_BYTES_PER_SECOND};
    static const GUID *const in_required[] = {&MF_MT_MAJOR_TYPE, &MF_MT_SUBTYPE, &MF_MT_AUDIO_NUM_CHANNELS,
            &MF_MT_AUDIO_BITS_PER_SAMPLE, &MF_MT_AUDIO_SAMPLES_PER_SECOND};
    IMFTransform *t = create_encoder();
    MFT_OUTPUT_STREAM_INFO oinfo;
    IMFMediaType *type, *cur;
    UINT32 v, size = 0, i, n;
    BYTE blob[64];
    HRESULT hr;
    IUnknown *unk;

    CHECK(t != NULL, "CoCreateInstance(CLSID_AACMFTEncoder)\n");
    if (!t) return;
    CHECK(IMFTransform_QueryInterface(t, &IID_IMediaObject, (void **)&unk) == E_NOINTERFACE, "no IMediaObject (an MFT only)\n");

    for (n = 0; SUCCEEDED(IMFTransform_GetOutputAvailableType(t, 0, n, &type)); n++) IMFMediaType_Release(type);
    CHECK(n >= 4, "%u output types listed before any type is set\n", n);

    for (i = 0; i < ARRAYSIZE(out_required); i++)
    {
        type = aac_type(1, 12000, -1);
        IMFMediaType_DeleteItem(type, out_required[i]);
        hr = IMFTransform_SetOutputType(t, 0, type, MFT_SET_TYPE_TEST_ONLY);
        CHECK(FAILED(hr), "output type without required attribute %u refused (%#lx)\n", i, hr);
        IMFMediaType_Release(type);
    }
    type = aac_type(1, 13000, -1);
    CHECK(FAILED(IMFTransform_SetOutputType(t, 0, type, 0)), "13000 bytes/s is not an AAC encoder rate\n");
    IMFMediaType_Release(type);

    type = aac_type(1, 12000, -1);
    hr = IMFTransform_SetOutputType(t, 0, type, 0);
    CHECK(hr == S_OK, "SetOutputType first (types in either order) %#lx\n", hr);
    IMFMediaType_Release(type);
    hr = IMFTransform_GetOutputCurrentType(t, 0, &cur);
    if (SUCCEEDED(hr))
    {
        memset(blob, 0, sizeof(blob));
        hr = IMFMediaType_GetBlob(cur, &MF_MT_USER_DATA, blob, sizeof(blob), &size);
        CHECK(hr == S_OK && size == sizeof(expect_user_data) && !memcmp(blob, expect_user_data, size),
              "MF_MT_USER_DATA is the HEAACWAVEINFO tail + AudioSpecificConfig (%u bytes, %02x %02x)\n", size, blob[12], blob[13]);
        CHECK(SUCCEEDED(IMFMediaType_GetUINT32(cur, &MF_MT_AAC_AUDIO_PROFILE_LEVEL_INDICATION, &v)) && v == 0x29, "profile level 0x29\n");
        CHECK(SUCCEEDED(IMFMediaType_GetUINT32(cur, &MF_MT_AUDIO_BLOCK_ALIGNMENT, &v)) && v == 1, "output block alignment 1\n");
        CHECK(SUCCEEDED(IMFMediaType_GetUINT32(cur, &MF_MT_AVG_BITRATE, &v)) && v == 96000, "output bit rate 96000\n");
        IMFMediaType_Release(cur);
    }
    else CHECK(0, "GetOutputCurrentType %#lx\n", hr);

    for (i = 0; i < ARRAYSIZE(in_required); i++)
    {
        type = pcm_type(1);
        IMFMediaType_DeleteItem(type, in_required[i]);
        hr = IMFTransform_SetInputType(t, 0, type, MFT_SET_TYPE_TEST_ONLY);
        CHECK(FAILED(hr), "input type without required attribute %u refused (%#lx)\n", i, hr);
        IMFMediaType_Release(type);
    }
    type = pcm_type(2);
    CHECK(FAILED(IMFTransform_SetInputType(t, 0, type, 0)), "input channels must match the output type\n");
    IMFMediaType_Release(type);
    type = pcm_type(1);
    hr = IMFTransform_SetInputType(t, 0, type, 0);
    CHECK(hr == S_OK, "SetInputType %#lx\n", hr);
    IMFMediaType_Release(type);
    if (SUCCEEDED(IMFTransform_GetInputCurrentType(t, 0, &cur)))
    {
        CHECK(SUCCEEDED(IMFMediaType_GetUINT32(cur, &MF_MT_AUDIO_BLOCK_ALIGNMENT, &v)) && v == 2, "input block alignment derived\n");
        CHECK(SUCCEEDED(IMFMediaType_GetUINT32(cur, &MF_MT_AUDIO_AVG_BYTES_PER_SECOND, &v)) && v == RATE * 2, "input byte rate derived\n");
        IMFMediaType_Release(cur);
    }
    memset(&oinfo, 0, sizeof(oinfo));
    IMFTransform_GetOutputStreamInfo(t, 0, &oinfo);
    CHECK(oinfo.cbSize == 0x600, "output buffer size %#lx\n", oinfo.cbSize);
    {
        short pcm[2048] = {0};
        IMFSample *s = make_sample(pcm, sizeof(pcm), 0, 0, FALSE, TRUE);
        hr = IMFTransform_ProcessInput(t, 0, s, 0);
        CHECK(hr == MF_E_NO_SAMPLE_TIMESTAMP, "input without a time: MF_E_NO_SAMPLE_TIMESTAMP (%#lx)\n", hr);
        IMFSample_Release(s);
        s = make_sample(pcm, sizeof(pcm), 0, 0, TRUE, FALSE);
        hr = IMFTransform_ProcessInput(t, 0, s, 0);
        CHECK(hr == MF_E_NO_SAMPLE_DURATION, "input without a duration: MF_E_NO_SAMPLE_DURATION (%#lx)\n", hr);
        IMFSample_Release(s);
    }
    IMFTransform_Release(t);
}

/* AAC frames -> PCM with Microsoft's AAC decoder MFT, returns the tone */
static double decode_frames(struct frames *f, IMFMediaType *aac, UINT32 channels)
{
    IMFTransform *d = NULL;
    IMFMediaType *pcm;
    struct frames out = {0};
    UINT32 i;
    size_t off = 0;
    double tone;

    if (FAILED(CoCreateInstance(&CLSID_MSAACDecMFT_, NULL, CLSCTX_INPROC_SERVER, &IID_IMFTransform, (void **)&d))) return -1;
    if (FAILED(IMFTransform_SetInputType(d, 0, aac, 0))) { IMFTransform_Release(d); return -2; }
    pcm = pcm_type(channels);
    IMFMediaType_SetUINT32(pcm, &MF_MT_AUDIO_BLOCK_ALIGNMENT, 2 * channels);
    IMFMediaType_SetUINT32(pcm, &MF_MT_AUDIO_AVG_BYTES_PER_SECOND, RATE * 2 * channels);
    if (FAILED(IMFTransform_SetOutputType(d, 0, pcm, 0))) { IMFMediaType_Release(pcm); IMFTransform_Release(d); return -3; }
    IMFMediaType_Release(pcm);
    for (i = 0; i < f->count; i++)
    {
        IMFSample *s = make_sample(f->data + off, f->sizes[i], f->times[i], f->durations[i], TRUE, TRUE);
        off += f->sizes[i];
        while (IMFTransform_ProcessInput(d, 0, s, 0) == MF_E_NOTACCEPTING)
            if (FAILED(drain_output(d, 0x10000, &out, NULL)) && 0) break;
        IMFSample_Release(s);
        drain_output(d, 0x10000, &out, NULL);
    }
    IMFTransform_ProcessMessage(d, MFT_MESSAGE_COMMAND_DRAIN, 0);
    drain_output(d, 0x10000, &out, NULL);
    IMFTransform_Release(d);
    tone = tone_of((short *)out.data, out.total / (2 * channels), channels, RATE);
    printf("      decoded %u PCM bytes\n", (unsigned)out.total);
    return tone;
}

static void test_encode(int payload, UINT32 channels)
{
    IMFTransform *t = create_encoder();
    IMFMediaType *type, *cur;
    struct frames f = {0};
    UINT32 bytes, i, bad_time = 0, too_big = 0, bad_adts = 0;
    size_t off = 0;
    int unclean = 0;
    short *pcm;
    HRESULT hr;
    IMFSample *s;
    double tone;

    printf("-- encode %u channel(s), payload type %d\n", channels, payload);
    if (!t) { CHECK(0, "no encoder\n"); return; }
    type = pcm_type(channels);
    hr = IMFTransform_SetInputType(t, 0, type, 0);   /* input first this time */
    IMFMediaType_Release(type);
    type = aac_type(channels, 16000, payload);
    hr = SUCCEEDED(hr) ? IMFTransform_SetOutputType(t, 0, type, 0) : hr;
    IMFMediaType_Release(type);
    CHECK(hr == S_OK, "types set input first %#lx\n", hr);
    if (FAILED(hr)) { IMFTransform_Release(t); return; }

    pcm = make_tone(channels, &bytes);
    /* in 100 ms pieces, as a capture or a file reader hands them */
    for (i = 0; i < SECONDS * 10; i++)
    {
        DWORD piece = bytes / (SECONDS * 10);
        s = make_sample((BYTE *)pcm + i * piece, piece, i * 1000000LL, 1000000, TRUE, TRUE);
        hr = IMFTransform_ProcessInput(t, 0, s, 0);
        IMFSample_Release(s);
        if (FAILED(hr)) { CHECK(0, "ProcessInput %u: %#lx\n", i, hr); break; }
        hr = drain_output(t, 0x600 + 7, &f, &unclean);
        if (hr != MF_E_TRANSFORM_NEED_MORE_INPUT) { CHECK(0, "ProcessOutput %#lx\n", hr); break; }
    }
    hr = IMFTransform_ProcessMessage(t, MFT_MESSAGE_COMMAND_DRAIN, 0);
    CHECK(hr == S_OK, "drain %#lx\n", hr);
    hr = drain_output(t, 0x600 + 7, &f, &unclean);
    CHECK(hr == MF_E_TRANSFORM_NEED_MORE_INPUT, "after the drain: MF_E_TRANSFORM_NEED_MORE_INPUT (%#lx)\n", hr);

    for (i = 0; i < f.count; i++)
    {
        if (f.durations[i] != 232199 && f.durations[i] != 232200) bad_time++;
        else if (i && f.times[i] != f.times[i - 1] + f.durations[i - 1]) bad_time++;
        if (f.sizes[i] > 0x600 + (payload == 1 ? 7 : 0)) too_big++;
        if (payload == 1)
        {
            const BYTE *h = f.data + off;
            DWORD len = ((h[3] & 3) << 11) | (h[4] << 3) | (h[5] >> 5);
            if (h[0] != 0xff || (h[1] & 0xf6) != 0xf0 || len != f.sizes[i] || ((h[2] >> 2) & 0xf) != 4) bad_adts++;
        }
        off += f.sizes[i];
    }
    CHECK(f.count >= 86 && f.count <= 90, "%u frames of 1024 samples for %u s\n", f.count, SECONDS);
    CHECK(f.count && f.times[0] == 0, "first frame at the input's time (%lld)\n", f.count ? (long long)f.times[0] : -1LL);
    CHECK(!bad_time, "frames are 1024 samples long and back to back (%u bad)\n", bad_time);
    CHECK(!unclean, "every frame is a clean point (%d not)\n", unclean);
    CHECK(!too_big, "no frame larger than the output buffer size (%u)\n", too_big);
    CHECK(f.total > bytes / 40 && f.total < bytes / 5, "compressed %u -> %u bytes\n", bytes, (unsigned)f.total);
    if (payload == 1) CHECK(!bad_adts, "every frame has an ADTS header with its length (%u bad)\n", bad_adts);

    if (payload != 1 && SUCCEEDED(IMFTransform_GetOutputCurrentType(t, 0, &cur)))
    {
        tone = decode_frames(&f, cur, channels);
        CHECK(fabs(tone - TONE) < TONE * 0.03, "decoded again it is a %.0f Hz tone\n", tone);
        IMFMediaType_Release(cur);
    }
    IMFTransform_Release(t);
    free(pcm); free(f.data); free(f.sizes); free(f.times); free(f.durations);
}

/* NAudio MediaFoundationEncoder: the types, then a sink writer to an .mp4 */
static void test_sink_writer(void)
{
    IMFCollection *types = NULL;
    IMFSinkWriter *writer = NULL;
    IMFSourceReader *reader = NULL;
    IMFMediaType *out = NULL, *in;
    DWORD stream = 0, count = 0, flags, total = 0, i;
    WCHAR path[MAX_PATH];
    UINT32 bytes;
    short *pcm, *decoded = NULL;
    HRESULT hr;

    printf("-- sink writer (.mp4)\n");
    hr = MFTranscodeGetAudioOutputAvailableTypes(&MFAudioFormat_AAC, MFT_ENUM_FLAG_ALL, NULL, &types);
    if (SUCCEEDED(hr)) IMFCollection_GetElementCount(types, &count);
    CHECK(hr == S_OK && count >= 4, "MFTranscodeGetAudioOutputAvailableTypes(AAC): %lu types (%#lx)\n", count, hr);
    for (i = 0; i < count && !out; i++)
    {
        IUnknown *u; IMFMediaType *t; UINT32 rate = 0, ch = 0, b = 0;
        IMFCollection_GetElement(types, i, &u);
        IUnknown_QueryInterface(u, &IID_IMFMediaType, (void **)&t);
        IUnknown_Release(u);
        IMFMediaType_GetUINT32(t, &MF_MT_AUDIO_SAMPLES_PER_SECOND, &rate);
        IMFMediaType_GetUINT32(t, &MF_MT_AUDIO_NUM_CHANNELS, &ch);
        IMFMediaType_GetUINT32(t, &MF_MT_AUDIO_AVG_BYTES_PER_SECOND, &b);
        if (rate == RATE && ch == 2 && b == 20000) out = t; else IMFMediaType_Release(t);
    }
    if (types) IMFCollection_Release(types);
    CHECK(out != NULL, "a 44.1 kHz stereo 160 kbps type is listed\n");
    if (!out) return;

    GetTempPathW(MAX_PATH, path);
    wcscat(path, L"aacenc-probe.mp4");
#ifdef KEEP_FILE
    wcscpy(path, L"Z:\\var\\tmp\\gmedia\\aac-out.mp4");
#endif
    DeleteFileW(path);
    hr = MFCreateSinkWriterFromURL(path, NULL, NULL, &writer);
    CHECK(hr == S_OK, "MFCreateSinkWriterFromURL(.mp4) %#lx\n", hr);
    if (FAILED(hr)) { IMFMediaType_Release(out); return; }
    hr = IMFSinkWriter_AddStream(writer, out, &stream);
    CHECK(hr == S_OK, "AddStream(AAC) %#lx\n", hr);
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
    CHECK(hr == S_OK, "the .mp4 opens in a source reader %#lx\n", hr);
    if (FAILED(hr)) return;
    /* the file's sample description is AAC ("mp4a" with its "esds"): Wine's source reader hands out
     * decoded audio as the native type, so the file itself is looked at */
    {
        HANDLE h = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
        DWORD size = GetFileSize(h, NULL), got = 0, i, mp4a = 0, esds = 0;
        BYTE *data = malloc(size + 1);
        ReadFile(h, data, size, &got, NULL);
        CloseHandle(h);
        for (i = 0; i + 4 <= got; i++)
        {
            if (!memcmp(data + i, "mp4a", 4)) mp4a = 1;
            if (!memcmp(data + i, "esds", 4)) esds = 1;
        }
        free(data);
        CHECK(mp4a && esds, "its audio stream is AAC (mp4a %lu, esds %lu, %lu bytes)\n", mp4a, esds, got);
    }
    in = pcm_type(2);
    hr = IMFSourceReader_SetCurrentMediaType(reader, 0, NULL, in);
    IMFMediaType_Release(in);
    CHECK(hr == S_OK, "read as PCM %#lx\n", hr);
    for (;;)
    {
        IMFSample *s = NULL; LONGLONG ts; IMFMediaBuffer *b; BYTE *p; DWORD len;
        hr = IMFSourceReader_ReadSample(reader, 0, 0, NULL, &flags, &ts, &s);
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
    test_registration();
    test_types();
    test_encode(0, 1);
    test_encode(0, 2);
    test_encode(1, 2);
#endif
    test_sink_writer();
    MFShutdown();
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
