/* wmvcore writer with a profile (patch 2865), run by test/wmvw-writer-gate.sh:
 * IWMWriter SetProfile, SetOutputFilename, the inputs (count, formats,
 * properties, SetInputProps), the state errors of an unconfigured writer, and
 * what BeginWriting checks before it needs an encoder (which is not available
 * here: it then returns E_NOTIMPL).
 *
 *   wmvw-writer-probe.exe */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <wmsdk.h>
#include <nserror.h>
#include <stdio.h>
#include <string.h>

#ifndef ASF_E_BUFFERTOOSMALL
#define ASF_E_BUFFERTOOSMALL ((HRESULT)0xc00d07d1)
#endif
static const GUID WMMEDIATYPE_Script = {0x73636d64, 0x0000, 0x0010, {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}};
static const GUID WMMEDIATYPE_Image = {0x34a50fd8, 0x8aa5, 0x4386, {0x81, 0xfe, 0xa0, 0xef, 0xe0, 0x48, 0x8e, 0x31}};
static const GUID WMFORMAT_WaveFormatEx = {0x05589f81, 0xc356, 0x11ce, {0xbf, 0x01, 0x00, 0xaa, 0x00, 0x55, 0x59, 0x5a}};
static const GUID WMFORMAT_VideoInfo = {0x05589f80, 0xc356, 0x11ce, {0xbf, 0x01, 0x00, 0xaa, 0x00, 0x55, 0x59, 0x5a}};
static const GUID WMSCRIPTTYPE_TwoStrings = {0x82f38a70, 0xc29f, 0x11d1, {0x97, 0xad, 0x00, 0xa0, 0xc9, 0x5e, 0xa8, 0x50}};

#define E_INVALIDPROFILE ((HRESULT)0xc00d0bc6)
#define E_NOT_CONFIGURED ((HRESULT)0xc00d0bbc)
#define E_INVALID_INPUT_FORMAT ((HRESULT)0xc00d0bb8)

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}
#define CHECKF(ok, ...) do { char _b[400]; snprintf(_b, sizeof(_b), __VA_ARGS__); check(ok, _b); } while (0)
#define CHECK_HR(hr, exp, what) do { HRESULT _h = (hr); CHECKF(_h == (HRESULT)(exp), "%s = %08lx (got %08lx)", what, (unsigned long)(exp), (unsigned long)_h); } while (0)

/* a sink that does nothing, to have an output */
typedef struct { IWMWriterSink iface; LONG ref; } Sink;
static HRESULT WINAPI sink_QI(IWMWriterSink *i, REFIID r, void **o) { if (IsEqualGUID(r, &IID_IUnknown) || IsEqualGUID(r, &IID_IWMWriterSink)) { *o = i; IWMWriterSink_AddRef(i); return S_OK; } *o = NULL; return E_NOINTERFACE; }
static ULONG WINAPI sink_AddRef(IWMWriterSink *i) { return InterlockedIncrement(&((Sink *)i)->ref); }
static ULONG WINAPI sink_Release(IWMWriterSink *i) { return InterlockedDecrement(&((Sink *)i)->ref); }
static HRESULT WINAPI sink_OnHeader(IWMWriterSink *i, INSSBuffer *b) { return S_OK; }
static HRESULT WINAPI sink_IsRealTime(IWMWriterSink *i, BOOL *b) { *b = FALSE; return S_OK; }
static HRESULT WINAPI sink_AllocateDataUnit(IWMWriterSink *i, DWORD s, INSSBuffer **b) { return E_NOTIMPL; }
static HRESULT WINAPI sink_OnDataUnit(IWMWriterSink *i, INSSBuffer *b) { return S_OK; }
static HRESULT WINAPI sink_OnEndWriting(IWMWriterSink *i) { return S_OK; }
static IWMWriterSinkVtbl sink_vtbl = { sink_QI, sink_AddRef, sink_Release, sink_OnHeader, sink_IsRealTime, sink_AllocateDataUnit, sink_OnDataUnit, sink_OnEndWriting };
static Sink the_sink = { { &sink_vtbl }, 1 };

static HRESULT get_name(IWMInputMediaProps *p, WCHAR *b, WORD *l) { return IWMInputMediaProps_GetConnectionName(p, b, l); }

static IWMProfile *build_profile(IWMProfileManager *mgr, WORD *removed_later)
{
    static BYTE wma[18 + 4] = {0x61, 0x01, 2, 0, 0x44, 0xac, 0, 0, 0x10, 0xb1, 2, 0, 0x4a, 0x2e, 16, 0, 4, 0, 1, 2, 3, 4};
    static BYTE vih[88];
    IWMProfile *p;
    IWMStreamConfig *c;
    IWMMediaProps *props;
    WM_MEDIA_TYPE mt = {0};

    IWMProfileManager_CreateEmptyProfile(mgr, WMT_VER_9_0, &p);

    IWMProfile_CreateNewStream(p, &WMMEDIATYPE_Audio, &c);
    mt.majortype = WMMEDIATYPE_Audio; mt.subtype = WMMEDIASUBTYPE_WMAudioV9; mt.bFixedSizeSamples = TRUE; mt.lSampleSize = 0x2e4a;
    mt.formattype = WMFORMAT_WaveFormatEx; mt.cbFormat = sizeof(wma); mt.pbFormat = wma;
    IWMStreamConfig_QueryInterface(c, &IID_IWMMediaProps, (void **)&props);
    IWMMediaProps_SetMediaType(props, &mt);
    IWMMediaProps_Release(props);
    IWMStreamConfig_SetConnectionName(c, L"Audio409");
    IWMStreamConfig_SetBitrate(c, 64000);
    IWMProfile_AddStream(p, c);
    IWMStreamConfig_Release(c);

    IWMProfile_CreateNewStream(p, &WMMEDIATYPE_Video, &c);
    memset(vih, 0, sizeof(vih));
    vih[8] = 0x40; vih[9] = 1;          /* rcSource.right = 320 */
    vih[12] = 240;                      /* rcSource.bottom */
    vih[48] = 40; vih[52] = 0x40; vih[53] = 1; vih[56] = 240; vih[60] = 1; vih[62] = 24;
    vih[40] = 0x15; vih[41] = 0x16; vih[42] = 0x0a;   /* AvgTimePerFrame 666133 */
    memset(&mt, 0, sizeof(mt));
    mt.majortype = WMMEDIATYPE_Video; mt.subtype = WMMEDIASUBTYPE_WVC1; mt.bTemporalCompression = TRUE;
    mt.formattype = WMFORMAT_VideoInfo; mt.cbFormat = sizeof(vih); mt.pbFormat = vih;
    IWMStreamConfig_QueryInterface(c, &IID_IWMMediaProps, (void **)&props);
    IWMMediaProps_SetMediaType(props, &mt);
    IWMMediaProps_Release(props);
    IWMStreamConfig_SetConnectionName(c, L"Video409");
    IWMProfile_AddStream(p, c);
    IWMStreamConfig_Release(c);

    IWMProfile_CreateNewStream(p, &WMMEDIATYPE_Script, &c);
    IWMProfile_AddStream(p, c);
    IWMStreamConfig_Release(c);

    IWMProfile_CreateNewStream(p, &WMMEDIATYPE_Image, &c);
    IWMStreamConfig_GetStreamNumber(c, removed_later);
    IWMProfile_AddStream(p, c);
    IWMStreamConfig_Release(c);
    return p;
}

static void check_format(IWMInputMediaProps *props, const char *what, const GUID *major, const GUID *sub, DWORD cb)
{
    DWORD size = 0;
    WM_MEDIA_TYPE *mt;
    GUID type;

    IWMInputMediaProps_GetType(props, &type);
    IWMInputMediaProps_GetMediaType(props, NULL, &size);
    CHECKF(IsEqualGUID(&type, major) && size == sizeof(WM_MEDIA_TYPE) + cb, "%s: type and size %lu (%lu)", what, (DWORD)(sizeof(WM_MEDIA_TYPE) + cb), size);
    mt = malloc(size);
    IWMInputMediaProps_GetMediaType(props, mt, &size);
    CHECKF(IsEqualGUID(&mt->subtype, sub), "%s: subtype", what);
    free(mt);
}

static WM_MEDIA_TYPE *get_mt(IWMInputMediaProps *props)
{
    DWORD size = 0;
    WM_MEDIA_TYPE *mt;
    IWMInputMediaProps_GetMediaType(props, NULL, &size);
    mt = malloc(size);
    IWMInputMediaProps_GetMediaType(props, mt, &size);
    return mt;
}

int main(void)
{
    HRESULT (WINAPI *create_mgr)(IWMProfileManager **);
    HRESULT (WINAPI *create_writer)(IUnknown *, IWMWriter **);
    IWMProfileManager *mgr = NULL;
    IWMWriter *writer = NULL;
    IWMWriterAdvanced *adv;
    IWMProfile *profile;
    IWMInputMediaProps *props, *props2;
    WM_MEDIA_TYPE *mt;
    INSSBuffer *buf;
    WORD removed, len;
    WCHAR name[32];
    DWORD n, max, i;
    HRESULT hr;
    HMODULE mod;
    unsigned int k;

    CoInitialize(NULL);
    mod = LoadLibraryW(L"wmvcore.dll");
    create_mgr = (void *)GetProcAddress(mod, "WMCreateProfileManager");
    create_writer = (void *)GetProcAddress(mod, "WMCreateWriter");
    if (!create_mgr || !create_writer) { check(0, "wmvcore.dll exports"); goto out; }
    create_mgr(&mgr);
    hr = create_writer(NULL, &writer);
    CHECKF(hr == S_OK && writer, "WMCreateWriter (%08lx)", hr);
    if (!writer || !mgr) goto out;

    /* a writer without a profile */
    CHECK_HR(IWMWriter_GetInputCount(writer, NULL), E_INVALIDARG, "GetInputCount(NULL)");
    CHECK_HR(IWMWriter_GetInputCount(writer, &n), E_INVALIDPROFILE, "GetInputCount (no profile)");
    CHECK_HR(IWMWriter_GetInputProps(writer, 0, &props), E_INVALIDPROFILE, "GetInputProps (no profile)");
    CHECK_HR(IWMWriter_GetInputFormatCount(writer, 0, &n), E_INVALIDPROFILE, "GetInputFormatCount (no profile)");
    CHECK_HR(IWMWriter_GetInputFormat(writer, 0, 0, &props), E_INVALIDPROFILE, "GetInputFormat (no profile)");
    CHECK_HR(IWMWriter_SetInputProps(writer, 0, (IWMInputMediaProps *)1), E_INVALIDPROFILE, "SetInputProps (no profile)");
    CHECK_HR(IWMWriter_BeginWriting(writer), E_INVALIDPROFILE, "BeginWriting (no profile)");
    CHECK_HR(IWMWriter_EndWriting(writer), NS_E_INVALID_REQUEST, "EndWriting (not writing)");
    CHECK_HR(IWMWriter_Flush(writer), NS_E_INVALID_REQUEST, "Flush (not writing)");
    CHECK_HR(IWMWriter_SetProfile(writer, NULL), E_INVALIDARG, "SetProfile(NULL)");
    CHECK_HR(IWMWriter_SetProfileByID(writer, NULL), E_INVALIDARG, "SetProfileByID(NULL)");
    CHECK_HR(IWMWriter_SetOutputFilename(writer, NULL), E_INVALIDARG, "SetOutputFilename(NULL)");

    profile = build_profile(mgr, &removed);
    CHECK_HR(IWMWriter_SetProfile(writer, profile), S_OK, "SetProfile");
    /* the writer works on its own copy */
    IWMProfile_RemoveStreamByNumber(profile, removed);
    CHECK_HR(IWMWriter_GetInputCount(writer, &n), S_OK, "GetInputCount");
    CHECKF(n == 4, "four inputs, one per stream, even after the profile changed (%lu)", n);
    CHECK_HR(IWMWriter_GetInputProps(writer, 4, &props), E_INVALIDARG, "GetInputProps(4)");
    CHECK_HR(IWMWriter_GetInputProps(writer, 0, NULL), E_INVALIDARG, "GetInputProps(NULL)");
    CHECK_HR(IWMWriter_GetInputFormatCount(writer, 4, &n), E_INVALIDARG, "GetInputFormatCount(4)");
    CHECK_HR(IWMWriter_GetInputFormatCount(writer, 0, NULL), E_INVALIDARG, "GetInputFormatCount(NULL)");
    CHECK_HR(IWMWriter_GetInputFormat(writer, 4, 0, &props), E_INVALIDARG, "GetInputFormat(4, 0)");
    CHECK_HR(IWMWriter_SetInputProps(writer, 4, (IWMInputMediaProps *)1), E_INVALIDARG, "SetInputProps(4)");
    CHECK_HR(IWMWriter_SetInputProps(writer, 0, NULL), E_INVALIDARG, "SetInputProps(NULL)");

    /* format counts */
    {
        static const DWORD counts[4] = {1, 5, 1, 0};
        for (i = 0; i < 4; i++)
        {
            n = 99;
            hr = IWMWriter_GetInputFormatCount(writer, i, &n);
            CHECKF(hr == S_OK && n == counts[i], "input %lu accepts %lu formats (%08lx, %lu)", i, counts[i], hr, n);
        }
    }
    CHECK_HR(IWMWriter_GetInputFormat(writer, 0, 1, &props), E_INVALIDARG, "GetInputFormat(0, 1)");
    CHECK_HR(IWMWriter_GetInputFormat(writer, 3, 0, &props), E_INVALIDARG, "GetInputFormat(3, 0) (no formats)");
    CHECK_HR(IWMWriter_GetInputFormat(writer, 0, 0, NULL), E_INVALIDARG, "GetInputFormat(NULL)");

    /* audio: PCM at the stream's channels and rate */
    CHECK_HR(IWMWriter_GetInputFormat(writer, 0, 0, &props), S_OK, "GetInputFormat(0, 0)");
    check_format(props, "audio format", &WMMEDIATYPE_Audio, &WMMEDIASUBTYPE_PCM, 18);
    mt = get_mt(props);
    CHECKF(IsEqualGUID(&mt->formattype, &WMFORMAT_WaveFormatEx) && mt->bFixedSizeSamples && !mt->bTemporalCompression && mt->lSampleSize == 4
            && mt->pbFormat[0] == 1 && mt->pbFormat[2] == 2 && mt->pbFormat[4] == 0x44 && mt->pbFormat[5] == 0xac
            && mt->pbFormat[8] == 0x10 && mt->pbFormat[9] == 0xb1 && mt->pbFormat[10] == 2 && mt->pbFormat[12] == 4 && mt->pbFormat[14] == 16
            && mt->pbFormat[16] == 0, "audio format: PCM, 2 channels, 44100 Hz, 16 bits, 176400 bytes/s, block 4");
    free(mt);
    len = 0;
    CHECK_HR(get_name(props, NULL, &len), S_OK, "connection name length");
    CHECKF(len == 9, "connection name length 9 (%u)", len);
    len = 9;
    CHECK_HR(get_name(props, name, &len), S_OK, "connection name");
    CHECKF(!wcscmp(name, L"Audio409"), "connection name Audio409 (%ls)", name);
    len = 3;
    CHECK_HR(get_name(props, name, &len), ASF_E_BUFFERTOOSMALL, "connection name short buffer");
    len = 1;
    CHECK_HR(IWMInputMediaProps_GetGroupName(props, name, &len), S_OK, "group name");
    CHECKF(!name[0], "group name is empty");
    IWMInputMediaProps_Release(props);

    /* video: the five uncompressed formats in the stream's size */
    {
        static const struct { DWORD fourcc; WORD bits; DWORD image; } vf[5] = {
            {0x30323449, 12, 115200}, {0x32315659, 12, 115200}, {0x32595559, 16, 153600}, {0, 24, 230400}, {0, 32, 307200},
        };
        for (i = 0; i < 5; i++)
        {
            CHECK_HR(IWMWriter_GetInputFormat(writer, 1, i, &props), S_OK, "GetInputFormat(1, i)");
            mt = get_mt(props);
            CHECKF(IsEqualGUID(&mt->formattype, &WMFORMAT_VideoInfo) && mt->cbFormat == 88 && !mt->bTemporalCompression
                    && mt->lSampleSize == vf[i].image && *(DWORD *)(mt->pbFormat + 52) == 320 && *(DWORD *)(mt->pbFormat + 56) == 240
                    && *(WORD *)(mt->pbFormat + 62) == vf[i].bits && *(DWORD *)(mt->pbFormat + 64) == vf[i].fourcc
                    && *(DWORD *)(mt->pbFormat + 68) == vf[i].image && *(DWORD *)(mt->pbFormat + 40) == 0x000a1615
                    && *(DWORD *)(mt->pbFormat + 8) == 320 && *(DWORD *)(mt->pbFormat + 12) == 240,
                    "video format %lu: %ubit, fourcc %08lx, 320x240, image %lu, frame time and rectangle kept", i, vf[i].bits, vf[i].fourcc, vf[i].image);
            free(mt);
            IWMInputMediaProps_Release(props);
        }
    }
    /* script */
    CHECK_HR(IWMWriter_GetInputFormat(writer, 2, 0, &props), S_OK, "GetInputFormat(2, 0)");
    check_format(props, "script format", &WMMEDIATYPE_Script, &WMSCRIPTTYPE_TwoStrings, 16);
    IWMInputMediaProps_Release(props);

    /* defaults and SetInputProps */
    CHECK_HR(IWMWriter_GetInputProps(writer, 1, &props), S_OK, "GetInputProps(1) before SetInputProps");
    mt = get_mt(props);
    CHECKF(*(DWORD *)(mt->pbFormat + 64) == 0x30323449, "default is the first format (I420)");
    free(mt);
    IWMInputMediaProps_Release(props);
    CHECK_HR(IWMWriter_GetInputProps(writer, 3, &props), NS_E_INVALID_INPUT_FORMAT, "GetInputProps(3) with no formats");

    IWMWriter_GetInputFormat(writer, 1, 1, &props);
    CHECK_HR(IWMWriter_SetInputProps(writer, 1, props), S_OK, "SetInputProps(1, YV12)");
    IWMInputMediaProps_Release(props);
    IWMWriter_GetInputProps(writer, 1, &props);
    mt = get_mt(props);
    CHECKF(*(DWORD *)(mt->pbFormat + 64) == 0x32315659, "GetInputProps returns the chosen format (YV12)");
    /* a format of another input, a changed size, and a wrong stream */
    CHECK_HR(IWMWriter_SetInputProps(writer, 0, props), E_INVALID_INPUT_FORMAT, "SetInputProps(0, video format)");
    IWMInputMediaProps_Release(props);
    IWMWriter_GetInputFormat(writer, 0, 0, &props);
    CHECK_HR(IWMInputMediaProps_GetType(props, &(GUID){0}), S_OK, "props GetType");
    {
        WM_MEDIA_TYPE *m2 = get_mt(props);
        m2->pbFormat[4] = 0x22;   /* 8738 Hz */
        CHECK_HR(IWMInputMediaProps_SetMediaType(props, m2), S_OK, "props SetMediaType");
        CHECK_HR(IWMWriter_SetInputProps(writer, 0, props), E_INVALID_INPUT_FORMAT, "SetInputProps(0, other sample rate)");
        free(m2);
    }
    IWMInputMediaProps_Release(props);
    IWMWriter_GetInputFormat(writer, 1, 3, &props);
    {
        WM_MEDIA_TYPE *m2 = get_mt(props);
        m2->pbFormat[52] = 0x20;  /* width 288 */
        IWMInputMediaProps_SetMediaType(props, m2);
        CHECK_HR(IWMWriter_SetInputProps(writer, 1, props), E_INVALID_INPUT_FORMAT, "SetInputProps(1, other width)");
        free(m2);
    }
    IWMInputMediaProps_Release(props);
    IWMWriter_GetInputProps(writer, 1, &props);
    mt = get_mt(props);
    CHECKF(*(DWORD *)(mt->pbFormat + 64) == 0x32315659, "a refused SetInputProps keeps the previous format");
    free(mt);
    IWMInputMediaProps_Release(props);
    CHECK_HR(IWMWriter_GetInputFormat(writer, 0, 0, &props2), S_OK, "GetInputFormat(0, 0)");
    CHECK_HR(IWMWriter_SetInputProps(writer, 0, props2), S_OK, "SetInputProps(0, PCM)");
    IWMInputMediaProps_Release(props2);

    /* BeginWriting: what it checks before it needs the encoder */
    CHECK_HR(IWMWriter_BeginWriting(writer), E_NOT_CONFIGURED, "BeginWriting (no output)");
    CHECK_HR(IWMWriter_SetOutputFilename(writer, L"C:\\wmvw-writer-test.wmv"), S_OK, "SetOutputFilename");
    CHECK_HR(IWMWriter_BeginWriting(writer), E_INVALID_INPUT_FORMAT, "BeginWriting (script input has no format set)");
    IWMWriter_GetInputFormat(writer, 2, 0, &props2);
    CHECK_HR(IWMWriter_SetInputProps(writer, 2, props2), S_OK, "SetInputProps(2, script)");
    IWMInputMediaProps_Release(props2);
    CHECK_HR(IWMWriter_BeginWriting(writer), E_NOTIMPL, "BeginWriting (everything set; no encoder here)");
    CHECK_HR(IWMWriter_EndWriting(writer), NS_E_INVALID_REQUEST, "EndWriting (never started)");
    CHECK_HR(IWMWriter_WriteSample(writer, 0, 0, 0, NULL), E_INVALIDARG, "WriteSample(NULL sample)");

    /* AllocateSample and WriteSample */
    CHECK_HR(IWMWriter_AllocateSample(writer, 100, NULL), E_INVALIDARG, "AllocateSample(NULL)");
    CHECK_HR(IWMWriter_AllocateSample(writer, 100, &buf), S_OK, "AllocateSample(100)");
    max = 0;
    INSSBuffer_GetMaxLength(buf, &max);
    CHECKF(max == 100, "sample capacity 100 (%lu)", max);
    CHECK_HR(INSSBuffer_SetLength(buf, 100), S_OK, "SetLength(100)");
    CHECK_HR(INSSBuffer_SetLength(buf, 101), E_INVALIDARG, "SetLength(101)");
    CHECK_HR(IWMWriter_WriteSample(writer, 0, 0, 0, buf), NS_E_INVALID_REQUEST, "WriteSample (not writing)");
    IWMWriter_QueryInterface(writer, &IID_IWMWriterAdvanced, (void **)&adv);
    CHECK_HR(IWMWriterAdvanced_WriteStreamSample(adv, 1, 0, 0, 0, 0, buf), NS_E_INVALID_REQUEST, "WriteStreamSample (not writing)");
    CHECK_HR(IWMWriterAdvanced_WriteStreamSample(adv, 1, 0, 0, 0, 0, NULL), E_INVALIDARG, "WriteStreamSample(NULL)");
    INSSBuffer_Release(buf);

    /* a sink is an output too */
    {
        IWMWriter *w2 = NULL;
        IWMWriterAdvanced *a2;
        create_writer(NULL, &w2);
        IWMWriter_SetProfile(w2, profile);
        IWMWriter_QueryInterface(w2, &IID_IWMWriterAdvanced, (void **)&a2);
        for (k = 0; k < 1; k++)
        {
            CHECK_HR(IWMWriter_GetInputFormat(w2, 0, 0, &props2), S_OK, "second writer: GetInputFormat");
            IWMWriter_SetInputProps(w2, 0, props2);
            IWMInputMediaProps_Release(props2);
            IWMWriter_GetInputFormat(w2, 1, 0, &props2);
            IWMWriter_SetInputProps(w2, 1, props2);
            IWMInputMediaProps_Release(props2);
            IWMWriter_GetInputFormat(w2, 2, 0, &props2);
            IWMWriter_SetInputProps(w2, 2, props2);
            IWMInputMediaProps_Release(props2);
        }
        CHECK_HR(IWMWriter_BeginWriting(w2), E_NOT_CONFIGURED, "second writer: BeginWriting (no output)");
        IWMWriterAdvanced_AddSink(a2, &the_sink.iface);
        CHECK_HR(IWMWriter_BeginWriting(w2), E_NOTIMPL, "second writer: BeginWriting with a sink only (no encoder here)");
        IWMWriterAdvanced_RemoveSink(a2, &the_sink.iface);
        IWMWriterAdvanced_Release(a2);
        IWMWriter_Release(w2);
    }
    IWMWriterAdvanced_Release(adv);

    /* SetProfile again resets the inputs */
    {
        IWMProfile *empty;
        IWMProfileManager_CreateEmptyProfile(mgr, WMT_VER_9_0, &empty);
        CHECK_HR(IWMWriter_SetProfile(writer, empty), S_OK, "SetProfile(empty)");
        n = 99;
        CHECK_HR(IWMWriter_GetInputCount(writer, &n), S_OK, "GetInputCount (empty profile)");
        CHECKF(n == 0, "no inputs (%lu)", n);
        IWMProfile_Release(empty);
    }
    IWMProfile_Release(profile);
    IWMWriter_Release(writer);
    IWMProfileManager_Release(mgr);
out:
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
