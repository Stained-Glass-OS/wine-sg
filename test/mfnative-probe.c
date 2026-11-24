/* Probe for test/mfnative-gate.sh (wine-sg 8903-8905): Media Foundation's sources and audio decoders as
 * Windows has them.
 *  - The MPEG-4, ASF and MP3 sources hand out their audio as it is in the file: the source reader's native
 *    type is MFAudioFormat_AAC / WMAudioV8 / MP3 (Wine's gave decoded PCM), and a PCM type asked of the
 *    reader, or of a Media Session's topology, is decoded by the decoder MFT it adds.
 *  - The MP3 decoder (CLSID_CMP3DecMediaObject) is an MFT as well as a DMO ("MP3 Decoder MFT", found by
 *    MFTEnumEx for MP3 -> PCM), with Windows' types and its one-input-sample-at-a-time rule.
 *  - The WMA decoder MFT decodes samples fed by hand: the file's samples as the source gives them, and
 *    the same bytes cut into block-sized samples.
 * The fixtures are made with the transcode API from a 1 kHz tone; every decode must give that tone back. */
#define COBJMACROS
#define _WIN32_WINNT 0x0a00
#define WINVER 0x0a00
#include <windows.h>
#include <initguid.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mferror.h>
#include <mfreadwrite.h>
#include <mftransform.h>
#include <dmo.h>
#include <wmcontainer.h>
#include <wmcodecdsp.h>
#include <math.h>
#include <stdio.h>

DEFINE_GUID(CLSID_CMP3DecMediaObject_, 0xbbeea841, 0x0a63, 0x4f52, 0xa7, 0xab, 0xa9, 0xb3, 0xa8, 0x4e, 0xd3, 0x8a);
DEFINE_GUID(CLSID_CWMADecMediaObject_, 0x2eeb4adf, 0x4578, 0x4d10, 0xbc, 0xa7, 0xbb, 0x95, 0x5f, 0x56, 0x32, 0x0a);

static int failures;
#define CHECK(c, ...) do { if (c) printf("ok    " __VA_ARGS__); else { failures++; printf("FAIL  " __VA_ARGS__); } } while (0)

#define RATE 44100
#define SECONDS 2
#define TONE 1000.0

static void write_wav(const WCHAR *path)
{
    DWORD n = RATE * SECONDS * 2, size = n * 2, i, written;
    short *pcm = malloc(size);
    BYTE header[44];
    HANDLE h;

    for (i = 0; i < n; i += 2)
        pcm[i] = pcm[i + 1] = (short)(12000 * sin(2 * 3.14159265358979 * TONE * (i / 2) / RATE));
    memcpy(header, "RIFF", 4); *(DWORD *)(header + 4) = 36 + size; memcpy(header + 8, "WAVEfmt ", 8);
    *(DWORD *)(header + 16) = 16; *(WORD *)(header + 20) = 1; *(WORD *)(header + 22) = 2;
    *(DWORD *)(header + 24) = RATE; *(DWORD *)(header + 28) = RATE * 4; *(WORD *)(header + 32) = 4; *(WORD *)(header + 34) = 16;
    memcpy(header + 36, "data", 4); *(DWORD *)(header + 40) = size;
    h = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    WriteFile(h, header, 44, &written, NULL);
    WriteFile(h, pcm, size, &written, NULL);
    CloseHandle(h);
    free(pcm);
}

/* the tone in 16-bit PCM (or 32-bit float when is_float) */
static double tone_of_any(const void *data, UINT32 frames, UINT32 channels, UINT32 rate, BOOL is_float)
{
    UINT32 i, crossings = 0, start = frames / 4, end = frames * 3 / 4;
    double energy = 0, prev = 0, v;
    if (end <= start + 2) return 0;
    for (i = start; i < end; i++)
    {
        v = is_float ? ((const float *)data)[i * channels] * 32767.0 : ((const short *)data)[i * channels];
        if (i > start && (prev < 0) != (v < 0)) crossings++;
        energy += v * v;
        prev = v;
    }
    if (energy / (end - start) < 1000.0 * 1000.0) return 0;
    return crossings * (double)rate / 2.0 / (end - start);
}

struct pcm { BYTE *data; DWORD len; };

static void pcm_add(struct pcm *pcm, IMFSample *sample)
{
    IMFMediaBuffer *b;
    BYTE *p;
    DWORD len;

    if (FAILED(IMFSample_ConvertToContiguousBuffer(sample, &b))) return;
    IMFMediaBuffer_Lock(b, &p, NULL, &len);
    pcm->data = realloc(pcm->data, pcm->len + len);
    memcpy(pcm->data + pcm->len, p, len);
    pcm->len += len;
    IMFMediaBuffer_Unlock(b);
    IMFMediaBuffer_Release(b);
}

static IMFMediaSource *open_source(const WCHAR *path)
{
    IMFSourceResolver *resolver;
    MF_OBJECT_TYPE type;
    IUnknown *object = NULL;
    IMFMediaSource *source = NULL;
    HRESULT hr;

    MFCreateSourceResolver(&resolver);
    hr = IMFSourceResolver_CreateObjectFromURL(resolver, path, MF_RESOLUTION_MEDIASOURCE, NULL, &type, &object);
    IMFSourceResolver_Release(resolver);
    if (FAILED(hr)) { printf("      source resolver %#lx\n", hr); return NULL; }
    IUnknown_QueryInterface(object, &IID_IMFMediaSource, (void **)&source);
    IUnknown_Release(object);
    return source;
}

/* IMFTranscodeProfile: not in mingw-w64's mfidl.h */
typedef struct
{
    HRESULT (WINAPI *QueryInterface)(IMFTranscodeProfile *, REFIID, void **);
    ULONG (WINAPI *AddRef)(IMFTranscodeProfile *);
    ULONG (WINAPI *Release)(IMFTranscodeProfile *);
    HRESULT (WINAPI *SetAudioAttributes)(IMFTranscodeProfile *, IMFAttributes *);
    HRESULT (WINAPI *GetAudioAttributes)(IMFTranscodeProfile *, IMFAttributes **);
    HRESULT (WINAPI *SetVideoAttributes)(IMFTranscodeProfile *, IMFAttributes *);
    HRESULT (WINAPI *GetVideoAttributes)(IMFTranscodeProfile *, IMFAttributes **);
    HRESULT (WINAPI *SetContainerAttributes)(IMFTranscodeProfile *, IMFAttributes *);
    HRESULT (WINAPI *GetContainerAttributes)(IMFTranscodeProfile *, IMFAttributes **);
} profile_vtbl;
#define PVT(p) (*(const profile_vtbl **)(p))
#define IMFTranscodeProfile_SetAudioAttributes(p, a) PVT(p)->SetAudioAttributes(p, a)
#define IMFTranscodeProfile_SetVideoAttributes(p, a) PVT(p)->SetVideoAttributes(p, a)
#define IMFTranscodeProfile_SetContainerAttributes(p, a) PVT(p)->SetContainerAttributes(p, a)
#define IMFTranscodeProfile_Release(p) PVT(p)->Release(p)

static IMFAttributes *audio_type_for(const GUID *subtype, UINT32 want_rate)
{
    IMFCollection *types = NULL;
    IMFAttributes *found = NULL;
    DWORD count = 0, i;

    if (FAILED(MFTranscodeGetAudioOutputAvailableTypes(subtype, MFT_ENUM_FLAG_ALL, NULL, &types))) return NULL;
    IMFCollection_GetElementCount(types, &count);
    for (i = 0; i < count && !found; i++)
    {
        IUnknown *u; IMFAttributes *a; UINT32 rate = 0, ch = 0;
        IMFCollection_GetElement(types, i, &u);
        IUnknown_QueryInterface(u, &IID_IMFAttributes, (void **)&a);
        IUnknown_Release(u);
        IMFAttributes_GetUINT32(a, &MF_MT_AUDIO_SAMPLES_PER_SECOND, &rate);
        IMFAttributes_GetUINT32(a, &MF_MT_AUDIO_NUM_CHANNELS, &ch);
        if (rate == want_rate && ch == 2) found = a; else IMFAttributes_Release(a);
    }
    IMFCollection_Release(types);
    return found;
}

static IMFTranscodeProfile *make_profile(const GUID *audio_subtype, const GUID *container, UINT32 rate)
{
    IMFTranscodeProfile *profile;
    IMFAttributes *audio = audio_type_for(audio_subtype, rate), *c;

    if (!audio) return NULL;
    MFCreateTranscodeProfile(&profile);
    IMFTranscodeProfile_SetAudioAttributes(profile, audio);
    IMFAttributes_Release(audio);
    if (container)
    {
        MFCreateAttributes(&c, 1);
        IMFAttributes_SetGUID(c, &MF_TRANSCODE_CONTAINERTYPE, container);
        IMFTranscodeProfile_SetContainerAttributes(profile, c);
        IMFAttributes_Release(c);
    }
    return profile;
}

/* runs a session to its end: MESessionEnded, then Close and MESessionClosed */
static HRESULT run_session(IMFTopology *topology)
{
    IMFMediaSession *session;
    PROPVARIANT start;
    DWORD deadline = GetTickCount() + 60000;
    BOOL ended = FALSE, closed = FALSE;
    HRESULT hr, status = S_OK;

    if (FAILED(hr = MFCreateMediaSession(NULL, &session))) return hr;
    hr = IMFMediaSession_SetTopology(session, 0, topology);
    PropVariantInit(&start);
    if (SUCCEEDED(hr)) hr = IMFMediaSession_Start(session, &GUID_NULL, &start);
    while (SUCCEEDED(hr) && !closed && GetTickCount() < deadline)
    {
        IMFMediaEvent *event;
        MediaEventType type;
        HRESULT ev_hr;

        if (FAILED(IMFMediaSession_GetEvent(session, MF_EVENT_FLAG_NO_WAIT, &event)))
        {
            Sleep(10);
            continue;
        }
        IMFMediaEvent_GetType(event, &type);
        IMFMediaEvent_GetStatus(event, &ev_hr);
        IMFMediaEvent_Release(event);
        if (FAILED(ev_hr) && SUCCEEDED(status)) { status = ev_hr; printf("      session event %lu failed %#lx\n", type, ev_hr); }
        if (type == MESessionEnded || (FAILED(ev_hr) && !ended))
        {
            ended = TRUE;
            IMFMediaSession_Close(session);
        }
        if (type == MESessionClosed) closed = TRUE;
    }
    if (SUCCEEDED(hr) && !closed) hr = HRESULT_FROM_WIN32(ERROR_TIMEOUT);
    IMFMediaSession_Shutdown(session);
    IMFMediaSession_Release(session);
    return FAILED(hr) ? hr : status;
}


static BOOL make_fixture(const WCHAR *wav, const GUID *subtype, const GUID *container, const WCHAR *path)
{
    IMFTranscodeProfile *profile = make_profile(subtype, container, RATE);
    IMFMediaSource *source = open_source(wav);
    IMFTopology *topology = NULL;
    HRESULT hr = E_FAIL;

    DeleteFileW(path);
    if (profile && source && SUCCEEDED(hr = MFCreateTranscodeTopology(source, path, profile, &topology)))
    {
        hr = run_session(topology);
        IMFTopology_Release(topology);
    }
    if (source) { IMFMediaSource_Shutdown(source); IMFMediaSource_Release(source); }
    if (profile) IMFTranscodeProfile_Release(profile);
    return SUCCEEDED(hr) && GetFileAttributesW(path) != INVALID_FILE_ATTRIBUTES;
}

static const char *subtype_name(const GUID *g)
{
    if (IsEqualGUID(g, &MFAudioFormat_AAC)) return "AAC";
    if (IsEqualGUID(g, &MFAudioFormat_WMAudioV8)) return "WMAudioV8";
    if (IsEqualGUID(g, &MFAudioFormat_MP3)) return "MP3";
    if (IsEqualGUID(g, &MFAudioFormat_PCM)) return "PCM";
    if (IsEqualGUID(g, &MFAudioFormat_Float)) return "Float";
    return "other";
}

static IMFMediaType *pcm16_type(void)
{
    IMFMediaType *type;
    MFCreateMediaType(&type);
    IMFMediaType_SetGUID(type, &MF_MT_MAJOR_TYPE, &MFMediaType_Audio);
    IMFMediaType_SetGUID(type, &MF_MT_SUBTYPE, &MFAudioFormat_PCM);
    IMFMediaType_SetUINT32(type, &MF_MT_AUDIO_BITS_PER_SAMPLE, 16);
    return type;
}

/* 1. the source reader: the native type, then PCM asked of it */
static void test_reader(const WCHAR *path, const GUID *want, const char *what)
{
    IMFSourceReader *reader;
    IMFMediaType *type;
    GUID subtype = {0};
    UINT32 rate = 0, channels = 0;
    struct pcm pcm = {0};
    DWORD flags;
    HRESULT hr;

    hr = MFCreateSourceReaderFromURL(path, NULL, &reader);
    CHECK(hr == S_OK, "%s: a source reader %#lx\n", what, hr);
    if (FAILED(hr)) return;
    hr = IMFSourceReader_GetNativeMediaType(reader, MF_SOURCE_READER_FIRST_AUDIO_STREAM, 0, &type);
    if (SUCCEEDED(hr))
    {
        IMFMediaType_GetGUID(type, &MF_MT_SUBTYPE, &subtype);
        IMFMediaType_GetUINT32(type, &MF_MT_AUDIO_SAMPLES_PER_SECOND, &rate);
        IMFMediaType_GetUINT32(type, &MF_MT_AUDIO_NUM_CHANNELS, &channels);
        IMFMediaType_Release(type);
    }
    CHECK(IsEqualGUID(&subtype, want), "%s: the native type is %s (got %s)\n", what, subtype_name(want), subtype_name(&subtype));
    CHECK(rate == RATE && channels == 2, "%s: native type %u Hz, %u channels\n", what, rate, channels);
    hr = IMFSourceReader_GetCurrentMediaType(reader, MF_SOURCE_READER_FIRST_AUDIO_STREAM, &type);
    if (SUCCEEDED(hr))
    {
        IMFMediaType_GetGUID(type, &MF_MT_SUBTYPE, &subtype);
        IMFMediaType_Release(type);
    }
    CHECK(SUCCEEDED(hr) && IsEqualGUID(&subtype, want), "%s: and the current type at first\n", what);

    type = pcm16_type();
    hr = IMFSourceReader_SetCurrentMediaType(reader, MF_SOURCE_READER_FIRST_AUDIO_STREAM, NULL, type);
    IMFMediaType_Release(type);
    CHECK(hr == S_OK, "%s: PCM asked of the reader: a decoder %#lx\n", what, hr);
    for (;;)
    {
        IMFSample *s = NULL; LONGLONG ts;
        hr = IMFSourceReader_ReadSample(reader, MF_SOURCE_READER_FIRST_AUDIO_STREAM, 0, NULL, &flags, &ts, &s);
        if (FAILED(hr) || (flags & MF_SOURCE_READERF_ENDOFSTREAM) || !s) { if (s) IMFSample_Release(s); break; }
        pcm_add(&pcm, s);
        IMFSample_Release(s);
    }
    IMFSourceReader_Release(reader);
    {
        double tone = pcm.data ? tone_of_any(pcm.data, pcm.len / 4, 2, RATE, FALSE) : 0;
        CHECK(pcm.len > RATE * 4 * SECONDS * 9 / 10 && fabs(tone - TONE) < TONE * 0.03,
              "%s: decoded by the reader: %lu bytes, the %.0f Hz tone\n", what, pcm.len, tone);
    }
    free(pcm.data);
}

/* 2. a Media Session: source -> sample grabber asking PCM; the topology loader adds the decoder */
struct grabber { IMFSampleGrabberSinkCallback iface; LONG ref; struct pcm pcm; CRITICAL_SECTION cs; };
static struct grabber *grabber_impl(IMFSampleGrabberSinkCallback *iface) { return CONTAINING_RECORD(iface, struct grabber, iface); }
static HRESULT WINAPI gb_QueryInterface(IMFSampleGrabberSinkCallback *iface, REFIID riid, void **out)
{
    if (IsEqualIID(riid, &IID_IMFSampleGrabberSinkCallback) || IsEqualIID(riid, &IID_IMFClockStateSink) || IsEqualIID(riid, &IID_IUnknown))
    { *out = iface; IMFSampleGrabberSinkCallback_AddRef(iface); return S_OK; }
    *out = NULL; return E_NOINTERFACE;
}
static ULONG WINAPI gb_AddRef(IMFSampleGrabberSinkCallback *iface) { return InterlockedIncrement(&grabber_impl(iface)->ref); }
static ULONG WINAPI gb_Release(IMFSampleGrabberSinkCallback *iface) { return InterlockedDecrement(&grabber_impl(iface)->ref); }
static HRESULT WINAPI gb_OnClockStart(IMFSampleGrabberSinkCallback *iface, MFTIME t, LONGLONG o) { return S_OK; }
static HRESULT WINAPI gb_OnClockStop(IMFSampleGrabberSinkCallback *iface, MFTIME t) { return S_OK; }
static HRESULT WINAPI gb_OnClockPause(IMFSampleGrabberSinkCallback *iface, MFTIME t) { return S_OK; }
static HRESULT WINAPI gb_OnClockRestart(IMFSampleGrabberSinkCallback *iface, MFTIME t) { return S_OK; }
static HRESULT WINAPI gb_OnClockSetRate(IMFSampleGrabberSinkCallback *iface, MFTIME t, float r) { return S_OK; }
static HRESULT WINAPI gb_OnSetPresentationClock(IMFSampleGrabberSinkCallback *iface, IMFPresentationClock *c) { return S_OK; }
static HRESULT WINAPI gb_OnProcessSample(IMFSampleGrabberSinkCallback *iface, REFGUID major, DWORD flags,
        LONGLONG time, LONGLONG duration, const BYTE *data, DWORD size)
{
    struct grabber *g = grabber_impl(iface);
    EnterCriticalSection(&g->cs);
    g->pcm.data = realloc(g->pcm.data, g->pcm.len + size);
    memcpy(g->pcm.data + g->pcm.len, data, size);
    g->pcm.len += size;
    LeaveCriticalSection(&g->cs);
    return S_OK;
}
static HRESULT WINAPI gb_OnShutdown(IMFSampleGrabberSinkCallback *iface) { return S_OK; }
static const IMFSampleGrabberSinkCallbackVtbl gb_vtbl =
{
    gb_QueryInterface, gb_AddRef, gb_Release, gb_OnClockStart, gb_OnClockStop, gb_OnClockPause, gb_OnClockRestart,
    gb_OnClockSetRate, gb_OnSetPresentationClock, gb_OnProcessSample, gb_OnShutdown,
};

static void test_session(const WCHAR *path, const char *what)
{
    struct grabber g = {{(IMFSampleGrabberSinkCallbackVtbl *)&gb_vtbl}, 1};
    IMFPresentationDescriptor *pd = NULL;
    IMFStreamDescriptor *sd = NULL;
    IMFTopologyNode *src_node, *sink_node;
    IMFMediaSource *source = open_source(path);
    IMFActivate *sink = NULL;
    IMFTopology *topology;
    IMFMediaType *type;
    BOOL selected;
    HRESULT hr;

    InitializeCriticalSection(&g.cs);
    CHECK(source != NULL, "%s: a source from the resolver\n", what);
    if (!source) return;
    IMFMediaSource_CreatePresentationDescriptor(source, &pd);
    IMFPresentationDescriptor_GetStreamDescriptorByIndex(pd, 0, &selected, &sd);
    type = pcm16_type();
    IMFMediaType_SetUINT32(type, &MF_MT_AUDIO_NUM_CHANNELS, 2);
    IMFMediaType_SetUINT32(type, &MF_MT_AUDIO_SAMPLES_PER_SECOND, RATE);
    IMFMediaType_SetUINT32(type, &MF_MT_AUDIO_BLOCK_ALIGNMENT, 4);
    IMFMediaType_SetUINT32(type, &MF_MT_AUDIO_AVG_BYTES_PER_SECOND, RATE * 4);
    IMFMediaType_SetUINT32(type, &MF_MT_ALL_SAMPLES_INDEPENDENT, 1);
    hr = MFCreateSampleGrabberSinkActivate(type, &g.iface, &sink);
    IMFMediaType_Release(type);
    IMFActivate_SetUINT32(sink, &MF_SAMPLEGRABBERSINK_IGNORE_CLOCK, 1);
    MFCreateTopology(&topology);
    MFCreateTopologyNode(MF_TOPOLOGY_SOURCESTREAM_NODE, &src_node);
    IMFTopologyNode_SetUnknown(src_node, &MF_TOPONODE_SOURCE, (IUnknown *)source);
    IMFTopologyNode_SetUnknown(src_node, &MF_TOPONODE_PRESENTATION_DESCRIPTOR, (IUnknown *)pd);
    IMFTopologyNode_SetUnknown(src_node, &MF_TOPONODE_STREAM_DESCRIPTOR, (IUnknown *)sd);
    MFCreateTopologyNode(MF_TOPOLOGY_OUTPUT_NODE, &sink_node);
    IMFTopologyNode_SetObject(sink_node, (IUnknown *)sink);
    IMFTopology_AddNode(topology, src_node);
    IMFTopology_AddNode(topology, sink_node);
    IMFTopologyNode_ConnectOutput(src_node, 0, sink_node, 0);
    hr = run_session(topology);
    CHECK(hr == S_OK, "%s: a Media Session to a PCM sample grabber runs to its end %#lx\n", what, hr);
    {
        double tone = g.pcm.data ? tone_of_any(g.pcm.data, g.pcm.len / 4, 2, RATE, FALSE) : 0;
        CHECK(g.pcm.len > RATE * 4 * SECONDS * 9 / 10 && fabs(tone - TONE) < TONE * 0.03,
              "%s: the session's decoder: %lu bytes, the %.0f Hz tone\n", what, g.pcm.len, tone);
    }
    IMFTopologyNode_Release(src_node);
    IMFTopologyNode_Release(sink_node);
    IMFTopology_Release(topology);
    IMFActivate_ShutdownObject(sink);
    IMFActivate_Release(sink);
    IMFStreamDescriptor_Release(sd);
    IMFPresentationDescriptor_Release(pd);
    IMFMediaSource_Shutdown(source);
    IMFMediaSource_Release(source);
    free(g.pcm.data);
}

/* the file's compressed samples, as the source reader gives them with the native type */
struct packets { IMFSample *s[4096]; UINT32 n; IMFMediaType *type; };

static BOOL read_packets(const WCHAR *path, struct packets *pk)
{
    IMFSourceReader *reader;
    IMFMediaType *type;
    DWORD flags;
    HRESULT hr;

    memset(pk, 0, sizeof(*pk));
    if (FAILED(MFCreateSourceReaderFromURL(path, NULL, &reader))) return FALSE;
    if (FAILED(IMFSourceReader_GetNativeMediaType(reader, MF_SOURCE_READER_FIRST_AUDIO_STREAM, 0, &type)))
    {
        IMFSourceReader_Release(reader);
        return FALSE;
    }
    IMFSourceReader_SetCurrentMediaType(reader, MF_SOURCE_READER_FIRST_AUDIO_STREAM, NULL, type);
    pk->type = type;
    while (pk->n < ARRAYSIZE(pk->s))
    {
        IMFSample *s = NULL; LONGLONG ts;
        hr = IMFSourceReader_ReadSample(reader, MF_SOURCE_READER_FIRST_AUDIO_STREAM, 0, NULL, &flags, &ts, &s);
        if (FAILED(hr) || (flags & MF_SOURCE_READERF_ENDOFSTREAM) || !s) { if (s) IMFSample_Release(s); break; }
        pk->s[pk->n++] = s;
    }
    IMFSourceReader_Release(reader);
    return pk->n > 0;
}

static void free_packets(struct packets *pk)
{
    while (pk->n) { pk->n--; IMFSample_Release(pk->s[pk->n]); }
    if (pk->type) IMFMediaType_Release(pk->type);
}

/* everything ProcessOutput gives until it needs more input */
static HRESULT drain_output(IMFTransform *t, struct pcm *pcm, UINT32 *incomplete)
{
    MFT_OUTPUT_STREAM_INFO info;
    HRESULT hr;

    IMFTransform_GetOutputStreamInfo(t, 0, &info);
    for (;;)
    {
        MFT_OUTPUT_DATA_BUFFER out = {0};
        IMFMediaBuffer *b;
        DWORD status;

        MFCreateSample(&out.pSample);
        MFCreateMemoryBuffer(info.cbSize ? info.cbSize : 65536, &b);
        IMFSample_AddBuffer(out.pSample, b);
        IMFMediaBuffer_Release(b);
        hr = IMFTransform_ProcessOutput(t, 0, 1, &out, &status);
        if (hr == S_OK)
        {
            pcm_add(pcm, out.pSample);
            if (incomplete && (out.dwStatus & MFT_OUTPUT_DATA_BUFFER_INCOMPLETE)) (*incomplete)++;
        }
        IMFSample_Release(out.pSample);
        if (out.pEvents) IMFCollection_Release(out.pEvents);
        if (hr != S_OK) return hr == MF_E_TRANSFORM_NEED_MORE_INPUT ? S_OK : hr;
    }
}

/* feeds the packets (or, with block, the same bytes cut into block-sized samples) */
static double feed_by_hand(IMFTransform *t, struct packets *pk, UINT32 block, UINT32 rate, BOOL is_float, DWORD *bytes)
{
    struct pcm pcm = {0};
    BYTE *all = NULL, *p;
    DWORD total = 0, len, off, i;
    double tone;
    HRESULT hr = S_OK;

    if (!block)
    {
        for (i = 0; i < pk->n && SUCCEEDED(hr); i++)
        {
            hr = IMFTransform_ProcessInput(t, 0, pk->s[i], 0);
            if (hr == MF_E_NOTACCEPTING) { drain_output(t, &pcm, NULL); hr = IMFTransform_ProcessInput(t, 0, pk->s[i], 0); }
            if (SUCCEEDED(hr)) hr = drain_output(t, &pcm, NULL);
        }
    }
    else
    {
        for (i = 0; i < pk->n; i++)
        {
            IMFMediaBuffer *b;
            IMFSample_ConvertToContiguousBuffer(pk->s[i], &b);
            IMFMediaBuffer_Lock(b, &p, NULL, &len);
            all = realloc(all, total + len);
            memcpy(all + total, p, len);
            total += len;
            IMFMediaBuffer_Unlock(b);
            IMFMediaBuffer_Release(b);
        }
        for (off = 0; off + block <= total && SUCCEEDED(hr); off += block)
        {
            IMFSample *s; IMFMediaBuffer *b;
            MFCreateSample(&s);
            MFCreateMemoryBuffer(block, &b);
            IMFMediaBuffer_Lock(b, &p, NULL, NULL); memcpy(p, all + off, block); IMFMediaBuffer_Unlock(b);
            IMFMediaBuffer_SetCurrentLength(b, block);
            IMFSample_AddBuffer(s, b);
            IMFSample_SetSampleTime(s, MulDiv(off, 10000000, total) * (LONGLONG)SECONDS);
            IMFMediaBuffer_Release(b);
            hr = IMFTransform_ProcessInput(t, 0, s, 0);
            if (SUCCEEDED(hr)) hr = drain_output(t, &pcm, NULL);
            IMFSample_Release(s);
        }
        free(all);
    }
    IMFTransform_ProcessMessage(t, MFT_MESSAGE_COMMAND_DRAIN, 0);
    drain_output(t, &pcm, NULL);
    if (FAILED(hr)) printf("      by hand: %#lx\n", hr);
    tone = pcm.data ? tone_of_any(pcm.data, pcm.len / (is_float ? 8 : 4), 2, rate, is_float) : 0;
    *bytes = pcm.len;
    free(pcm.data);
    return tone;
}

static IMFTransform *decoder_with_types(const GUID *clsid, IMFMediaType *in, const GUID *out_subtype)
{
    IMFTransform *t;
    IMFMediaType *out;
    DWORD i;
    HRESULT hr;

    if (FAILED(CoCreateInstance(clsid, NULL, CLSCTX_INPROC_SERVER, &IID_IMFTransform, (void **)&t))) return NULL;
    if (FAILED(hr = IMFTransform_SetInputType(t, 0, in, 0))) { printf("      SetInputType %#lx\n", hr); IMFTransform_Release(t); return NULL; }
    for (i = 0; SUCCEEDED(IMFTransform_GetOutputAvailableType(t, 0, i, &out)); i++)
    {
        GUID sub; UINT32 ch = 0, bits = 0;
        IMFMediaType_GetGUID(out, &MF_MT_SUBTYPE, &sub);
        IMFMediaType_GetUINT32(out, &MF_MT_AUDIO_NUM_CHANNELS, &ch);
        IMFMediaType_GetUINT32(out, &MF_MT_AUDIO_BITS_PER_SAMPLE, &bits);
        if (IsEqualGUID(&sub, out_subtype) && ch == 2 && (bits == 16 || bits == 32))
        {
            hr = IMFTransform_SetOutputType(t, 0, out, 0);
            IMFMediaType_Release(out);
            if (SUCCEEDED(hr)) return t;
            printf("      SetOutputType %#lx\n", hr);
            break;
        }
        IMFMediaType_Release(out);
    }
    IMFTransform_Release(t);
    return NULL;
}

/* the stream's type as the ASF header gives it (IMFASFContentInfo / IMFASFProfile), as a program
 * using the WMContainer objects reads it */
static IMFMediaType *asf_header_type(const WCHAR *path)
{
    HANDLE h = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    DWORD size = GetFileSize(h, NULL), got = 0;
    IMFASFContentInfo *info;
    IMFASFProfile *profile;
    IMFASFStreamConfig *sc;
    IMFMediaType *type = NULL;
    IMFMediaBuffer *b;
    BYTE *p;
    WORD n;

    MFCreateMemoryBuffer(size, &b);
    IMFMediaBuffer_Lock(b, &p, NULL, NULL);
    ReadFile(h, p, size, &got, NULL);
    IMFMediaBuffer_Unlock(b);
    CloseHandle(h);
    IMFMediaBuffer_SetCurrentLength(b, got);
    MFCreateASFContentInfo(&info);
    if (SUCCEEDED(IMFASFContentInfo_ParseHeader(info, b, 0)) && SUCCEEDED(IMFASFContentInfo_GetProfile(info, &profile)))
    {
        if (SUCCEEDED(IMFASFProfile_GetStream(profile, 0, &n, &sc)))
        {
            IMFASFStreamConfig_GetMediaType(sc, &type);
            IMFASFStreamConfig_Release(sc);
        }
        IMFASFProfile_Release(profile);
    }
    IMFASFContentInfo_Release(info);
    IMFMediaBuffer_Release(b);
    return type;
}

/* 3. the WMA decoder MFT, fed by hand */
static void test_wma_by_hand(const WCHAR *path)
{
    struct packets pk;
    IMFTransform *t;
    UINT32 block = 0;
    DWORD bytes;
    double tone;

    printf("-- WMA decoder MFT by hand\n");
    CHECK(read_packets(path, &pk), "WMA: the file's compressed samples (%u)\n", pk.n);
    if (!pk.n) return;
    IMFMediaType_GetUINT32(pk.type, &MF_MT_AUDIO_BLOCK_ALIGNMENT, &block);
    t = decoder_with_types(&CLSID_CWMADecMediaObject_, pk.type, &MFAudioFormat_PCM);
    CHECK(t != NULL, "WMA: the decoder takes the source's type and PCM\n");
    if (t)
    {
        tone = feed_by_hand(t, &pk, 0, RATE, FALSE, &bytes);
        CHECK(bytes > RATE * 4 * SECONDS * 9 / 10 && fabs(tone - TONE) < TONE * 0.03,
              "WMA: the source's samples fed by hand: %lu bytes, the %.0f Hz tone\n", bytes, tone);
        IMFTransform_Release(t);
    }
    t = decoder_with_types(&CLSID_CWMADecMediaObject_, pk.type, &MFAudioFormat_PCM);
    if (t && block)
    {
        tone = feed_by_hand(t, &pk, block, RATE, FALSE, &bytes);
        CHECK(bytes > RATE * 4 * SECONDS * 9 / 10 && fabs(tone - TONE) < TONE * 0.03,
              "WMA: block-sized samples (%u bytes) fed by hand: %lu bytes, the %.0f Hz tone\n", block, bytes, tone);
    }
    else CHECK(0, "WMA: a second decoder, block %u\n", block);
    if (t) IMFTransform_Release(t);
    {
        IMFMediaType *header = asf_header_type(path);
        CHECK(header != NULL, "WMA: the stream's type from the ASF header\n");
        t = header ? decoder_with_types(&CLSID_CWMADecMediaObject_, header, &MFAudioFormat_PCM) : NULL;
        CHECK(t != NULL, "WMA: the decoder takes the header's type\n");
        if (t && block)
        {
            tone = feed_by_hand(t, &pk, block, RATE, FALSE, &bytes);
            CHECK(bytes > RATE * 4 * SECONDS * 9 / 10 && fabs(tone - TONE) < TONE * 0.03,
                  "WMA: with the header's type, block-sized samples: %lu bytes, the %.0f Hz tone\n", bytes, tone);
        }
        if (t) IMFTransform_Release(t);
        if (header) IMFMediaType_Release(header);
    }
    free_packets(&pk);
}

/* 4. the MP3 decoder as an MFT */
static void test_mp3_mft(const WCHAR *path)
{
    MFT_REGISTER_TYPE_INFO in_info = {MFMediaType_Audio, MFAudioFormat_MP3}, out_info = {MFMediaType_Audio, MFAudioFormat_PCM};
    IMFActivate **activates = NULL;
    UINT32 count = 0, i, n;
    BOOL found = FALSE;
    struct packets pk;
    IMFTransform *t = NULL;
    IMediaObject *dmo = NULL;
    IMFMediaType *type;
    WCHAR *name = NULL;
    DWORD bytes;
    double tone;
    HRESULT hr;

    printf("-- MP3 decoder MFT\n");
    hr = MFTEnumEx(MFT_CATEGORY_AUDIO_DECODER, MFT_ENUM_FLAG_SYNCMFT | MFT_ENUM_FLAG_LOCALMFT | MFT_ENUM_FLAG_SORTANDFILTER,
            &in_info, &out_info, &activates, &count);
    for (i = 0; i < count; i++)
    {
        GUID clsid;
        if (SUCCEEDED(IMFActivate_GetGUID(activates[i], &MFT_TRANSFORM_CLSID_Attribute, &clsid))
                && IsEqualGUID(&clsid, &CLSID_CMP3DecMediaObject_))
        {
            found = TRUE;
            IMFActivate_GetAllocatedString(activates[i], &MFT_FRIENDLY_NAME_Attribute, &name, &n);
        }
        IMFActivate_Release(activates[i]);
    }
    CoTaskMemFree(activates);
    CHECK(found, "MFTEnumEx (audio decoders, MP3 -> PCM) finds CLSID_CMP3DecMediaObject (%u found)\n", count);
    CHECK(name && !wcscmp(name, L"MP3 Decoder MFT"), "named \"MP3 Decoder MFT\" (%ls)\n", name ? name : L"none");
    CoTaskMemFree(name);

    hr = CoCreateInstance(&CLSID_CMP3DecMediaObject_, NULL, CLSCTX_INPROC_SERVER, &IID_IMFTransform, (void **)&t);
    CHECK(hr == S_OK, "it is an IMFTransform %#lx\n", hr);
    if (FAILED(hr)) return;
    hr = IMFTransform_QueryInterface(t, &IID_IMediaObject, (void **)&dmo);
    CHECK(hr == S_OK, "and an IMediaObject, the same object %#lx\n", hr);
    if (dmo) IMediaObject_Release(dmo);
    hr = IMFTransform_GetOutputAvailableType(t, 0, 0, &type);
    CHECK(hr == MF_E_TRANSFORM_TYPE_NOT_SET, "no output types before an input type (%#lx)\n", hr);
    for (n = 0; SUCCEEDED(IMFTransform_GetInputAvailableType(t, 0, n, &type)); n++)
    {
        GUID sub = {0};
        IMFMediaType_GetGUID(type, &MF_MT_SUBTYPE, &sub);
        if (!IsEqualGUID(&sub, &MFAudioFormat_MP3)) n += 100;
        IMFMediaType_Release(type);
    }
    CHECK(n == 1, "one input type, MP3 (%u)\n", n);

    CHECK(read_packets(path, &pk), "MP3: the file's compressed samples (%u)\n", pk.n);
    if (!pk.n) { IMFTransform_Release(t); return; }
    hr = IMFTransform_SetInputType(t, 0, pk.type, 0);
    CHECK(hr == S_OK, "it takes the source's MP3 type %#lx\n", hr);
    for (n = 0; SUCCEEDED(IMFTransform_GetOutputAvailableType(t, 0, n, &type)); n++)
    {
        GUID sub = {0}; UINT32 ch = 0, bits = 0, rate = 0;
        IMFMediaType_GetGUID(type, &MF_MT_SUBTYPE, &sub);
        IMFMediaType_GetUINT32(type, &MF_MT_AUDIO_NUM_CHANNELS, &ch);
        IMFMediaType_GetUINT32(type, &MF_MT_AUDIO_BITS_PER_SAMPLE, &bits);
        IMFMediaType_GetUINT32(type, &MF_MT_AUDIO_SAMPLES_PER_SECOND, &rate);
        if (n == 0 && !(IsEqualGUID(&sub, &MFAudioFormat_Float) && bits == 32 && ch == 2 && rate == RATE)) n += 100;
        if (n == 1 && !(IsEqualGUID(&sub, &MFAudioFormat_PCM) && bits == 16 && ch == 2)) n += 100;
        if (n == 5 && !(IsEqualGUID(&sub, &MFAudioFormat_PCM) && bits == 8 && ch == 1)) n += 100;
        IMFMediaType_Release(type);
    }
    CHECK(n == 6, "six output types: float, PCM 16, PCM 8, stereo then mono, at the input's rate (%u)\n", n);
    IMFTransform_Release(t);

    t = decoder_with_types(&CLSID_CMP3DecMediaObject_, pk.type, &MFAudioFormat_PCM);
    CHECK(t != NULL, "MP3: PCM 16 stereo set as the output\n");
    if (t)
    {
        MFT_OUTPUT_STREAM_INFO info = {0};
        IMFTransform_GetOutputStreamInfo(t, 0, &info);
        CHECK(info.cbSize == 1152 * 4, "output blocks of 1152 frames (%lu bytes)\n", info.cbSize);
        hr = IMFTransform_ProcessInput(t, 0, pk.s[0], 0);
        CHECK(hr == S_OK, "ProcessInput %#lx\n", hr);
        hr = IMFTransform_ProcessInput(t, 0, pk.s[1], 0);
        CHECK(hr == MF_E_NOTACCEPTING, "a second input sample before its output is read: MF_E_NOTACCEPTING (%#lx)\n", hr);
        IMFTransform_ProcessMessage(t, MFT_MESSAGE_COMMAND_FLUSH, 0);
        tone = feed_by_hand(t, &pk, 0, RATE, FALSE, &bytes);
        CHECK(bytes > RATE * 4 * SECONDS * 9 / 10 && fabs(tone - TONE) < TONE * 0.03,
              "MP3: the source's samples decoded by the MFT: %lu bytes, the %.0f Hz tone\n", bytes, tone);
        IMFTransform_Release(t);
    }
    t = decoder_with_types(&CLSID_CMP3DecMediaObject_, pk.type, &MFAudioFormat_Float);
    if (t)
    {
        tone = feed_by_hand(t, &pk, 0, RATE, TRUE, &bytes);
        CHECK(bytes > RATE * 8 * SECONDS * 9 / 10 && fabs(tone - TONE) < TONE * 0.03,
              "MP3: as float: %lu bytes, the %.0f Hz tone\n", bytes, tone);
        IMFTransform_Release(t);
    }
    else CHECK(0, "MP3: float output\n");
    free_packets(&pk);
}

int main(void)
{
    WCHAR dir[MAX_PATH], wav[MAX_PATH], mp4[MAX_PATH], wma[MAX_PATH], mp3[MAX_PATH];
    BOOL have_mp4, have_wma, have_mp3;

    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    MFStartup(MF_VERSION, MFSTARTUP_FULL);
    GetTempPathW(MAX_PATH, dir);
    swprintf(wav, MAX_PATH, L"%lsmfnative.wav", dir);
    swprintf(mp4, MAX_PATH, L"%lsmfnative.mp4", dir);
    swprintf(wma, MAX_PATH, L"%lsmfnative.wma", dir);
    swprintf(mp3, MAX_PATH, L"%lsmfnative.mp3", dir);
    write_wav(wav);
    have_mp4 = make_fixture(wav, &MFAudioFormat_AAC, &MFTranscodeContainerType_MPEG4, mp4);
    have_wma = make_fixture(wav, &MFAudioFormat_WMAudioV8, &MFTranscodeContainerType_ASF, wma);
    have_mp3 = make_fixture(wav, &MFAudioFormat_MP3, &MFTranscodeContainerType_MP3, mp3);
    CHECK(have_mp4 && have_wma && have_mp3, "fixtures: AAC in MPEG-4 %d, WMA in ASF %d, MP3 %d\n", have_mp4, have_wma, have_mp3);

    printf("-- source reader\n");
    if (have_mp4) test_reader(mp4, &MFAudioFormat_AAC, "MPEG-4");
    if (have_wma) test_reader(wma, &MFAudioFormat_WMAudioV8, "ASF");
    if (have_mp3) test_reader(mp3, &MFAudioFormat_MP3, "MP3");
    printf("-- Media Session\n");
    if (have_mp4) test_session(mp4, "MPEG-4");
    if (have_wma) test_session(wma, "ASF");
    if (have_mp3) test_session(mp3, "MP3");
    if (have_wma) test_wma_by_hand(wma);
    if (have_mp3) test_mp3_mft(mp3);

    DeleteFileW(wav); DeleteFileW(mp4); DeleteFileW(wma); DeleteFileW(mp3);
    MFShutdown();
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
