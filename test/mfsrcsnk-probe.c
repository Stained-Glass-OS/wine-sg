/* mfsrcsnk's WAVE media sink members that were stubs (patches/sg/2872), run by
 * test/mfsrcsnk-gate.sh: the stream sink's PlaceMarker and Flush, the media
 * type handler (types, support check, current type that reaches the file
 * header) and OnClockSetRate, before and after Shutdown, and the sink class
 * factory.
 *
 *   mfsrcsnk-probe.exe */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <mfapi.h>
#include <mferror.h>
#include <mfidl.h>
#include <stdio.h>
#include <string.h>

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#endif
static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}
#define CHECKF(ok, ...) do { char _b[256]; snprintf(_b, sizeof(_b), __VA_ARGS__); check(ok, _b); } while (0)

/* IMFSinkClassFactory is private to the implementation's headers */
DEFINE_GUID(IID_IMFSinkClassFactory_sg, 0x37aa1c3b, 0x620f, 0x477e, 0xbe, 0xf9, 0xac, 0x4a, 0xa8, 0x5b, 0xe9, 0x5d);
DEFINE_GUID(CLSID_MFWAVESinkClassFactory_sg, 0x36f99745, 0x23c9, 0x4c9c, 0x8d, 0xd5, 0xcc, 0x31, 0xce, 0x96, 0x43, 0x90);
typedef struct IMFSinkClassFactory IMFSinkClassFactory;
typedef struct
{
    HRESULT (WINAPI *QueryInterface)(IMFSinkClassFactory *, REFIID, void **);
    ULONG (WINAPI *AddRef)(IMFSinkClassFactory *);
    ULONG (WINAPI *Release)(IMFSinkClassFactory *);
    HRESULT (WINAPI *CreateMediaSink)(IMFSinkClassFactory *, IMFByteStream *, IMFMediaType *, IMFMediaType *, IMFMediaSink **);
} IMFSinkClassFactoryVtbl;
struct IMFSinkClassFactory { const IMFSinkClassFactoryVtbl *lpVtbl; };

static IMFMediaType *make_type(const GUID *major, const GUID *subtype, UINT32 channels, UINT32 rate, UINT32 bits)
{
    IMFMediaType *t;

    MFCreateMediaType(&t);
    IMFMediaType_SetGUID(t, &MF_MT_MAJOR_TYPE, major);
    IMFMediaType_SetGUID(t, &MF_MT_SUBTYPE, subtype);
    if (channels) IMFMediaType_SetUINT32(t, &MF_MT_AUDIO_NUM_CHANNELS, channels);
    if (rate) IMFMediaType_SetUINT32(t, &MF_MT_AUDIO_SAMPLES_PER_SECOND, rate);
    if (bits) IMFMediaType_SetUINT32(t, &MF_MT_AUDIO_BITS_PER_SAMPLE, bits);
    return t;
}

static int type_is(IMFMediaType *t, UINT32 channels, UINT32 rate, UINT32 bits)
{
    UINT32 c = 0, r = 0, b = 0;

    IMFMediaType_GetUINT32(t, &MF_MT_AUDIO_NUM_CHANNELS, &c);
    IMFMediaType_GetUINT32(t, &MF_MT_AUDIO_SAMPLES_PER_SECOND, &r);
    IMFMediaType_GetUINT32(t, &MF_MT_AUDIO_BITS_PER_SAMPLE, &b);
    return c == channels && r == rate && b == bits;
}

/* the fmt chunk written at the start of the file: channels, rate, bits */
static int header_format(IMFByteStream *bs, UINT32 *channels, UINT32 *rate, UINT32 *bits)
{
    BYTE buf[256];
    ULONG got = 0;
    int i;

    IMFByteStream_SetCurrentPosition(bs, 0);
    if (FAILED(IMFByteStream_Read(bs, buf, sizeof(buf), &got))) return 0;
    for (i = 0; i + 24 <= (int)got; i++)
    {
        if (!memcmp(buf + i, "fmt ", 4))
        {
            *channels = *(WORD *)(buf + i + 10);
            *rate = *(DWORD *)(buf + i + 12);
            *bits = *(WORD *)(buf + i + 22);
            return 1;
        }
    }
    return 0;
}

int main(void)
{
    IMFMediaType *pcm44, *pcm48, *video, *mp3, *nochannels, *cur, *cur2, *out;
    IMFMediaTypeHandler *th;
    IMFStreamSink *ss;
    IMFClockStateSink *cs;
    IMFByteStream *bs;
    IMFMediaSink *sink;
    IMFMediaEvent *ev;
    IMFSample *sample;
    IMFMediaBuffer *buffer;
    PROPVARIANT ctx;
    DWORD n, type;
    UINT32 c, r, b;
    HRESULT hr;
    int i;
    static const MFSTREAMSINK_MARKER_TYPE marker_types[] = {MFSTREAMSINK_MARKER_DEFAULT, MFSTREAMSINK_MARKER_ENDOFSEGMENT,
                                                            MFSTREAMSINK_MARKER_TICK, MFSTREAMSINK_MARKER_EVENT};

    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    MFStartup(MF_VERSION, MFSTARTUP_FULL);

    pcm44 = make_type(&MFMediaType_Audio, &MFAudioFormat_PCM, 2, 44100, 16);
    pcm48 = make_type(&MFMediaType_Audio, &MFAudioFormat_PCM, 1, 48000, 8);
    video = make_type(&MFMediaType_Video, &MFVideoFormat_RGB32, 0, 0, 0);
    mp3 = make_type(&MFMediaType_Audio, &MFAudioFormat_MP3, 2, 44100, 16);
    nochannels = make_type(&MFMediaType_Audio, &MFAudioFormat_PCM, 0, 44100, 16);

    hr = MFCreateTempFile(MF_ACCESSMODE_READWRITE, MF_OPENMODE_DELETE_IF_EXIST, 0, &bs);
    if (hr != S_OK) { printf("FAIL  MFCreateTempFile (%#lx)\n", hr); return 1; }
    {
        HRESULT (WINAPI *create_wave_sink)(IMFByteStream *, IMFMediaType *, IMFMediaSink **);
        HMODULE srcsnk = LoadLibraryA("mfsrcsnk.dll");

        create_wave_sink = srcsnk ? (void *)GetProcAddress(srcsnk, "MFCreateWAVEMediaSink") : NULL;
        if (!create_wave_sink) { puts("FAIL  mfsrcsnk exports MFCreateWAVEMediaSink"); return 1; }
        hr = create_wave_sink(bs, pcm44, &sink);
    }
    if (hr != S_OK) { printf("FAIL  MFCreateWAVEMediaSink (%#lx)\n", hr); return 1; }
    IMFMediaSink_GetStreamSinkByIndex(sink, 0, &ss);
    IMFStreamSink_GetMediaTypeHandler(ss, &th);
    IMFMediaSink_QueryInterface(sink, &IID_IMFClockStateSink, (void **)&cs);

    /* type handler */
    hr = IMFMediaTypeHandler_GetMediaTypeCount(th, &n);
    CHECKF(hr == S_OK && n == 1, "GetMediaTypeCount is 1 (%#lx, %lu)", hr, n);
    hr = IMFMediaTypeHandler_GetMediaTypeCount(th, NULL);
    CHECKF(hr == E_POINTER, "GetMediaTypeCount(NULL) is E_POINTER (%#lx)", hr);
    cur = NULL;
    hr = IMFMediaTypeHandler_GetCurrentMediaType(th, &cur);
    CHECKF(hr == S_OK && cur && type_is(cur, 2, 44100, 16), "GetCurrentMediaType is the type the sink was made with (%#lx)", hr);
    if (cur)
    {
        IMFMediaType_SetUINT32(cur, &MF_MT_AUDIO_NUM_CHANNELS, 6);
        cur2 = NULL;
        IMFMediaTypeHandler_GetCurrentMediaType(th, &cur2);
        CHECKF(cur2 && type_is(cur2, 2, 44100, 16), "the current type is handed out as a copy");
        if (cur2) IMFMediaType_Release(cur2);
        IMFMediaType_Release(cur);
    }
    hr = IMFMediaTypeHandler_GetCurrentMediaType(th, NULL);
    CHECKF(hr == E_POINTER, "GetCurrentMediaType(NULL) is E_POINTER (%#lx)", hr);
    cur = NULL;
    hr = IMFMediaTypeHandler_GetMediaTypeByIndex(th, 0, &cur);
    CHECKF(hr == S_OK && cur && type_is(cur, 2, 44100, 16), "GetMediaTypeByIndex(0) is the current type (%#lx)", hr);
    if (cur) IMFMediaType_Release(cur);
    hr = IMFMediaTypeHandler_GetMediaTypeByIndex(th, 1, &cur);
    CHECKF(hr == MF_E_NO_MORE_TYPES, "GetMediaTypeByIndex(1) is MF_E_NO_MORE_TYPES (%#lx)", hr);
    hr = IMFMediaTypeHandler_GetMediaTypeByIndex(th, 0, NULL);
    CHECKF(hr == E_POINTER, "GetMediaTypeByIndex(NULL) is E_POINTER (%#lx)", hr);
    {
        struct { const char *name; IMFMediaType *t; HRESULT hr; } support[] = {
            {"48 kHz mono PCM", pcm48, S_OK}, {"44.1 kHz stereo PCM", pcm44, S_OK},
            {"a video type", video, MF_E_INVALIDMEDIATYPE}, {"MP3 audio", mp3, MF_E_INVALIDMEDIATYPE},
            {"PCM without a channel count", nochannels, MF_E_INVALIDMEDIATYPE}, {"NULL", NULL, E_POINTER},
        };
        for (i = 0; i < ARRAY_SIZE(support); i++)
        {
            out = (IMFMediaType *)1;
            hr = IMFMediaTypeHandler_IsMediaTypeSupported(th, support[i].t, &out);
            CHECKF(hr == support[i].hr && out == NULL, "IsMediaTypeSupported(%s) is %#lx, no closest type (%#lx)",
                   support[i].name, support[i].hr, hr);
        }
        hr = IMFMediaTypeHandler_IsMediaTypeSupported(th, pcm48, NULL);
        CHECKF(hr == S_OK, "IsMediaTypeSupported without an out parameter is S_OK (%#lx)", hr);
    }
    hr = IMFMediaTypeHandler_SetCurrentMediaType(th, NULL);
    CHECKF(hr == E_POINTER, "SetCurrentMediaType(NULL) is E_POINTER (%#lx)", hr);
    hr = IMFMediaTypeHandler_SetCurrentMediaType(th, video);
    CHECKF(hr == MF_E_INVALIDMEDIATYPE, "SetCurrentMediaType(video) is MF_E_INVALIDMEDIATYPE (%#lx)", hr);
    cur = NULL;
    IMFMediaTypeHandler_GetCurrentMediaType(th, &cur);
    CHECKF(cur && type_is(cur, 2, 44100, 16), "a refused type leaves the current type alone");
    if (cur) IMFMediaType_Release(cur);
    hr = IMFMediaTypeHandler_SetCurrentMediaType(th, pcm48);
    CHECKF(hr == S_OK, "SetCurrentMediaType(48 kHz mono 8 bit) is S_OK (%#lx)", hr);
    cur = NULL;
    IMFMediaTypeHandler_GetCurrentMediaType(th, &cur);
    CHECKF(cur && type_is(cur, 1, 48000, 8), "the new type is the current type");
    if (cur) IMFMediaType_Release(cur);

    /* markers, flush, rate */
    for (i = 0; i < ARRAY_SIZE(marker_types); i++)
    {
        ctx.vt = VT_I4;
        ctx.lVal = 100 + i;
        hr = IMFStreamSink_PlaceMarker(ss, marker_types[i], NULL, &ctx);
        ev = NULL;
        type = 0;
        if (hr == S_OK && IMFStreamSink_GetEvent(ss, MF_EVENT_FLAG_NO_WAIT, &ev) == S_OK)
        {
            PROPVARIANT v;
            IMFMediaEvent_GetType(ev, &type);
            PropVariantInit(&v);
            IMFMediaEvent_GetValue(ev, &v);
            CHECKF(type == MEStreamSinkMarker && v.vt == VT_I4 && v.lVal == 100 + i,
                   "PlaceMarker(type %d) queues MEStreamSinkMarker carrying the context (%#lx, %lu)", marker_types[i], hr, type);
            IMFMediaEvent_Release(ev);
        }
        else
            CHECKF(0, "PlaceMarker(type %d) queues an event (%#lx)", marker_types[i], hr);
    }
    hr = IMFStreamSink_PlaceMarker(ss, MFSTREAMSINK_MARKER_DEFAULT, NULL, NULL);
    CHECKF(hr == S_OK, "PlaceMarker without a context is S_OK (%#lx)", hr);
    IMFStreamSink_GetEvent(ss, MF_EVENT_FLAG_NO_WAIT, &ev);
    if (ev) IMFMediaEvent_Release(ev);
    hr = IMFStreamSink_PlaceMarker(ss, 4, NULL, NULL);
    CHECKF(hr == E_INVALIDARG, "PlaceMarker(type 4) is E_INVALIDARG (%#lx)", hr);
    hr = IMFStreamSink_PlaceMarker(ss, -1, NULL, NULL);
    CHECKF(hr == E_INVALIDARG, "PlaceMarker(type -1) is E_INVALIDARG (%#lx)", hr);
    hr = IMFStreamSink_Flush(ss);
    CHECKF(hr == S_OK, "Flush is S_OK (%#lx)", hr);
    hr = IMFClockStateSink_OnClockSetRate(cs, 0, 2.0f);
    CHECKF(hr == S_OK, "OnClockSetRate(2.0) is S_OK (%#lx)", hr);
    hr = IMFClockStateSink_OnClockSetRate(cs, 0, 0.5f);
    CHECKF(hr == S_OK, "OnClockSetRate(0.5) is S_OK (%#lx)", hr);

    /* a sample writes the header, with the format set above; after that the type is fixed */
    MFCreateSample(&sample);
    MFCreateMemoryBuffer(64, &buffer);
    IMFMediaBuffer_SetCurrentLength(buffer, 64);
    IMFSample_AddBuffer(sample, buffer);
    hr = IMFStreamSink_ProcessSample(ss, sample);
    CHECKF(hr == S_OK, "ProcessSample is S_OK (%#lx)", hr);
    c = r = b = 0;
    check(header_format(bs, &c, &r, &b) && c == 1 && r == 48000 && b == 8, "the file header carries the type set with SetCurrentMediaType");
    hr = IMFMediaTypeHandler_SetCurrentMediaType(th, pcm44);
    CHECKF(hr == MF_E_INVALIDREQUEST, "SetCurrentMediaType after data was written is MF_E_INVALIDREQUEST (%#lx)", hr);
    IMFMediaBuffer_Release(buffer);
    IMFSample_Release(sample);

    /* the class factory */
    {
        HRESULT (WINAPI *get_class_object)(REFCLSID, REFIID, void **);
        HMODULE mod = LoadLibraryA("mfsrcsnk.dll");
        IClassFactory *cf = NULL;
        IMFSinkClassFactory *scf = NULL;
        IMFMediaSink *s2 = NULL;

        get_class_object = mod ? (void *)GetProcAddress(mod, "DllGetClassObject") : NULL;
        if (get_class_object && get_class_object(&CLSID_MFWAVESinkClassFactory_sg, &IID_IClassFactory, (void **)&cf) == S_OK
                && IClassFactory_CreateInstance(cf, NULL, &IID_IMFSinkClassFactory_sg, (void **)&scf) == S_OK)
        {
            hr = IClassFactory_LockServer(cf, TRUE);
            CHECKF(hr == S_OK, "LockServer is S_OK (%#lx)", hr);
            IClassFactory_LockServer(cf, FALSE);
            hr = scf->lpVtbl->CreateMediaSink(scf, bs, NULL, pcm44, &s2);
            CHECKF(hr == S_OK && s2, "the WAVE sink class factory makes a sink from an audio type (%#lx)", hr);
            if (s2) IMFMediaSink_Release(s2);
            hr = scf->lpVtbl->CreateMediaSink(scf, bs, NULL, NULL, &s2);
            CHECKF(hr == E_POINTER, "CreateMediaSink without an audio type is E_POINTER (%#lx)", hr);
            scf->lpVtbl->Release(scf);
        }
        else
            check(0, "the WAVE sink class factory can be created");
        if (cf) IClassFactory_Release(cf);
    }

    /* after Shutdown */
    IMFMediaSink_Shutdown(sink);
    {
        HRESULT r[8];
        static const char *names[] = {"PlaceMarker", "Flush", "GetMediaTypeCount", "GetMediaTypeByIndex",
                                      "GetCurrentMediaType", "SetCurrentMediaType", "IsMediaTypeSupported", "OnClockSetRate"};

        r[0] = IMFStreamSink_PlaceMarker(ss, MFSTREAMSINK_MARKER_DEFAULT, NULL, NULL);
        r[1] = IMFStreamSink_Flush(ss);
        r[2] = IMFMediaTypeHandler_GetMediaTypeCount(th, &n);
        r[3] = IMFMediaTypeHandler_GetMediaTypeByIndex(th, 0, &cur);
        r[4] = IMFMediaTypeHandler_GetCurrentMediaType(th, &cur);
        r[5] = IMFMediaTypeHandler_SetCurrentMediaType(th, pcm44);
        r[6] = IMFMediaTypeHandler_IsMediaTypeSupported(th, pcm44, &out);
        for (i = 0; i < 7; i++)
            CHECKF(r[i] == MF_E_STREAMSINK_REMOVED, "after Shutdown: %s is MF_E_STREAMSINK_REMOVED (%#lx)", names[i], r[i]);
        r[7] = IMFClockStateSink_OnClockSetRate(cs, 0, 1.0f);
        CHECKF(r[7] == MF_E_SHUTDOWN, "after Shutdown: %s is MF_E_SHUTDOWN (%#lx)", names[7], r[7]);
    }

    IMFClockStateSink_Release(cs);
    IMFMediaTypeHandler_Release(th);
    IMFStreamSink_Release(ss);
    IMFMediaSink_Release(sink);
    IMFByteStream_Release(bs);
    MFShutdown();
    printf("%s\n", failures ? "RESULT: FAIL" : "RESULT: PASS");
    return failures != 0;
}
