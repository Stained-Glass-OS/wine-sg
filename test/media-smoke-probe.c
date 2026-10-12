/* Smoke probe for the multimedia gap patches 4000-4027: "probe.exe SECTION"
 * loads the DLL of SECTION and calls a few of the functions those patches
 * implemented, printing "ok"/"FAIL" lines and "RESULT: PASS|FAIL". */
#define COBJMACROS
#define INITGUID
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <objbase.h>
#include <mmsystem.h>
#include <mmreg.h>
#include <msacm.h>
#include <dshow.h>
#include <dmo.h>
#include <medparam.h>
#include <dsound.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <physicalmonitorenumerationapi.h>
#include <highlevelmonitorconfigurationapi.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <spatialaudioclient.h>
#include <vfw.h>
#include <mmstream.h>
#include <amstream.h>
#include <austream.h>

static int failures;

#define check(cond, ...) do { if (cond) printf("ok   "); else { printf("FAIL "); ++failures; } printf(__VA_ARGS__); printf("\n"); } while (0)

DEFINE_GUID(CLSID_wine_AviDest, 0xe2510970,0xf137,0x11ce,0x8b,0x67,0x00,0xaa,0x00,0xa3,0xf1,0xa6);
DEFINE_GUID(IID_wine_IConfigAviMux, 0x5acd6aa0,0xf482,0x11ce,0x8b,0x67,0x00,0xaa,0x00,0xa3,0xf1,0xa6);
DEFINE_GUID(CLSID_wine_DVDNavigator, 0x9b8c4620,0x2c1a,0x11d0,0x84,0x93,0x00,0xa0,0x24,0x38,0xad,0x48);
DEFINE_GUID(IID_wine_IDvdInfo2, 0x34151510,0xeec0,0x11d2,0x82,0x01,0x00,0xa0,0xc9,0xd7,0x48,0x42);
DEFINE_GUID(CLSID_wine_WMAsfReader, 0x187463a0,0x5bb7,0x11d3,0xac,0xbe,0x00,0x80,0xc7,0x5e,0x24,0x6e);
DEFINE_GUID(IID_wine_ISTMCInterop, 0xddb0472d,0xc911,0x4a1f,0x86,0xd9,0xdc,0x3d,0x71,0xa9,0x5f,0x5a);
DEFINE_GUID(IID_wine_ISTMC, 0x99fa3ff4,0x1742,0x42a6,0x90,0x2e,0x08,0x7d,0x41,0xf9,0x65,0xec);
DEFINE_GUID(IID_wine_IInspectable, 0xaf86e2e0,0xb12d,0x4c6a,0x9c,0x5a,0xd7,0xaa,0x65,0x10,0x1e,0x90);
DEFINE_GUID(IID_wine_IActivationFactory, 0x00000035,0x0000,0x0000,0xc0,0x00,0x00,0x00,0x00,0x00,0x00,0x46);
DEFINE_GUID(CLSID_wine_MFMediaEngineClassFactory, 0xb44392da,0x499b,0x446b,0xa4,0xcb,0x00,0x5f,0xea,0xd0,0xe6,0xd5);
DEFINE_GUID(IID_wine_ISpatialAudioClient, 0xbbf8e066,0xaaaa,0x49be,0x9a,0x4d,0xfd,0x2a,0x85,0x8e,0xa2,0x7f);
DEFINE_GUID(IID_wine_IAudioClock2, 0x6f49ff73,0x6727,0x49ac,0xa0,0x08,0xd9,0x8c,0xf5,0xe7,0x00,0x48);

/* vtable slot call for interfaces the headers here do not describe */
typedef HRESULT (WINAPI *method0)(void *);
typedef HRESULT (WINAPI *method1)(void *, void *);
typedef HRESULT (WINAPI *method3)(void *, void *, void *, void *);
#define SLOT(obj, n) ((*(void ***)(obj))[n])
static void release(void *obj) { if (obj) ((ULONG (WINAPI *)(void *))SLOT(obj, 2))(obj); }

typedef HRESULT (WINAPI *pRoGetActivationFactory)(void *, REFIID, void **);
typedef HRESULT (WINAPI *pWindowsCreateString)(const WCHAR *, UINT32, void **);
typedef const WCHAR *(WINAPI *pWindowsGetStringRawBuffer)(void *, UINT32 *);
typedef HRESULT (WINAPI *pWindowsDeleteString)(void *);
typedef HRESULT (WINAPI *pRoInitialize)(int);
static pRoGetActivationFactory RoGetActivationFactory_;
static pWindowsCreateString WindowsCreateString_;
static pWindowsGetStringRawBuffer WindowsGetStringRawBuffer_;
static pWindowsDeleteString WindowsDeleteString_;

static void init_winrt(void)
{
    HMODULE combase = LoadLibraryA("combase.dll");
    RoGetActivationFactory_ = (void *)GetProcAddress(combase, "RoGetActivationFactory");
    WindowsCreateString_ = (void *)GetProcAddress(combase, "WindowsCreateString");
    WindowsGetStringRawBuffer_ = (void *)GetProcAddress(combase, "WindowsGetStringRawBuffer");
    WindowsDeleteString_ = (void *)GetProcAddress(combase, "WindowsDeleteString");
    ((pRoInitialize)GetProcAddress(combase, "RoInitialize"))(1);
}

static void *get_factory(const WCHAR *name, REFIID iid)
{
    void *hstr = NULL, *factory = NULL;
    HRESULT hr;

    WindowsCreateString_(name, wcslen(name), &hstr);
    hr = RoGetActivationFactory_(hstr, iid, &factory);
    WindowsDeleteString_(hstr);
    check(hr == S_OK && factory, "activation factory %ls: %#lx", name, hr);
    return SUCCEEDED(hr) ? factory : NULL;
}

static void fill_pcm(WAVEFORMATEX *wfx, WORD bits, WORD channels, DWORD rate)
{
    wfx->wFormatTag = WAVE_FORMAT_PCM;
    wfx->nChannels = channels;
    wfx->nSamplesPerSec = rate;
    wfx->wBitsPerSample = bits;
    wfx->nBlockAlign = channels * bits / 8;
    wfx->nAvgBytesPerSec = rate * wfx->nBlockAlign;
    wfx->cbSize = 0;
}

static void test_dsdmo(void)
{
    IMediaObjectInPlace *inplace;
    void *params;
    IMediaObject *dmo;
    DMO_MEDIA_TYPE mt = {0};
    WAVEFORMATEX wfx;
    MP_DATA value = 0;
    short samples[2048];
    unsigned int i, changed = 0;
    HRESULT hr;

    hr = CoCreateInstance(&GUID_DSFX_STANDARD_GARGLE, NULL, CLSCTX_INPROC_SERVER, &IID_IMediaObject, (void **)&dmo);
    check(hr == S_OK, "create gargle: %#lx", hr);
    if (FAILED(hr)) return;

    hr = IMediaObject_QueryInterface(dmo, &IID_IMediaParams, &params);
    check(hr == S_OK, "IMediaParams: %#lx", hr);
    if (SUCCEEDED(hr))
    {
        /* IMediaParams: GetParam 3, SetParam 4 (the vtable of some headers differs) */
        HRESULT (WINAPI *get_param)(void *, DWORD, MP_DATA *) = SLOT(params, 3);
        HRESULT (WINAPI *set_param)(void *, DWORD, MP_DATA) = SLOT(params, 4);

        hr = set_param(params, 0, 100.0f);
        check(hr == S_OK, "SetParam(rate 100): %#lx", hr);
        hr = get_param(params, 0, &value);
        check(hr == S_OK && value == 100.0f, "GetParam: %#lx %f", hr, value);
        hr = set_param(params, 0, 5000.0f);
        check(hr == E_INVALIDARG, "SetParam out of range: %#lx", hr);
        release(params);
    }

    fill_pcm(&wfx, 16, 2, 44100);
    mt.majortype = MEDIATYPE_Audio;
    mt.subtype = MEDIASUBTYPE_PCM;
    mt.formattype = FORMAT_WaveFormatEx;
    mt.cbFormat = sizeof(wfx);
    mt.pbFormat = (BYTE *)&wfx;
    hr = IMediaObject_SetInputType(dmo, 0, &mt, 0);
    check(hr == S_OK, "SetInputType: %#lx", hr);
    hr = IMediaObject_SetOutputType(dmo, 0, &mt, 0);
    check(hr == S_OK, "SetOutputType: %#lx", hr);

    hr = IMediaObject_QueryInterface(dmo, &IID_IMediaObjectInPlace, (void **)&inplace);
    check(hr == S_OK, "IMediaObjectInPlace: %#lx", hr);
    if (SUCCEEDED(hr))
    {
        for (i = 0; i < ARRAYSIZE(samples); ++i) samples[i] = 10000;
        hr = IMediaObjectInPlace_Process(inplace, sizeof(samples), (BYTE *)samples, 0, DMO_INPLACE_NORMAL);
        check(hr == S_OK, "Process: %#lx", hr);
        for (i = 0; i < ARRAYSIZE(samples); ++i) if (samples[i] != 10000) ++changed;
        check(changed > ARRAYSIZE(samples) / 4, "gargle modulated %u of %u samples", changed, (unsigned)ARRAYSIZE(samples));
        IMediaObjectInPlace_Release(inplace);
    }
    IMediaObject_Release(dmo);
}

static void test_qcap(void)
{
    ICaptureGraphBuilder2 *builder;
    IBaseFilter *mux = NULL;
    IFileSinkFilter *sink = NULL;
    void *config;
    LONG stream = 0;
    BOOL old = TRUE;
    HRESULT hr;

    hr = CoCreateInstance(&CLSID_CaptureGraphBuilder2, NULL, CLSCTX_INPROC_SERVER, &IID_ICaptureGraphBuilder2, (void **)&builder);
    check(hr == S_OK, "create capture graph builder: %#lx", hr);
    if (FAILED(hr)) return;
    hr = ICaptureGraphBuilder2_SetOutputFileName(builder, &MEDIASUBTYPE_Avi, L"smoke.avi", &mux, &sink);
    check(hr == S_OK && mux && sink, "SetOutputFileName: %#lx %p %p", hr, mux, sink);
    if (mux) IBaseFilter_Release(mux);
    if (sink) IFileSinkFilter_Release(sink);
    ICaptureGraphBuilder2_Release(builder);

    hr = CoCreateInstance(&CLSID_wine_AviDest, NULL, CLSCTX_INPROC_SERVER, &IID_wine_IConfigAviMux, &config);
    check(hr == S_OK, "create AVI mux: %#lx", hr);
    if (FAILED(hr)) return;
    /* IConfigAviMux: SetMasterStream 3, GetMasterStream 4, SetOutputCompatibilityIndex 5, Get 6 */
    hr = ((HRESULT (WINAPI *)(void *, LONG))SLOT(config, 3))(config, 0);
    check(hr == S_OK, "SetMasterStream: %#lx", hr);
    hr = ((method1)SLOT(config, 4))(config, &stream);
    check(hr == S_OK && stream == 0, "GetMasterStream: %#lx %ld", hr, stream);
    hr = ((HRESULT (WINAPI *)(void *, BOOL))SLOT(config, 5))(config, FALSE);
    check(hr == S_OK, "SetOutputCompatibilityIndex: %#lx", hr);
    hr = ((method1)SLOT(config, 6))(config, &old);
    check(hr == S_OK && !old, "GetOutputCompatibilityIndex: %#lx %d", hr, old);
    release(config);
}

static void test_dxva2(void)
{
    HMONITOR monitor = MonitorFromPoint((POINT){0, 0}, MONITOR_DEFAULTTOPRIMARY);
    PHYSICAL_MONITOR physical[4];
    DWORD count = 0, min, cur, max;
    BOOL ret;

    ret = GetNumberOfPhysicalMonitorsFromHMONITOR(monitor, &count);
    check(ret && count >= 1 && count <= 4, "number of physical monitors: %d %lu", ret, count);
    if (!ret || !count || count > 4) return;
    ret = GetPhysicalMonitorsFromHMONITOR(monitor, count, physical);
    check(ret, "GetPhysicalMonitorsFromHMONITOR: %lu", GetLastError());
    if (!ret) return;
    SetLastError(0xdeadbeef);
    ret = GetMonitorBrightness(physical[0].hPhysicalMonitor, &min, &cur, &max);
    check(!ret && GetLastError() != 0xdeadbeef, "GetMonitorBrightness fails with an error: %lu", GetLastError());
    SetLastError(0xdeadbeef);
    ret = GetMonitorBrightness((HANDLE)0xdead, &min, &cur, &max);
    check(!ret && GetLastError() != 0xdeadbeef, "bad handle: %lu", GetLastError());
    ret = DestroyPhysicalMonitors(count, physical);
    check(ret, "DestroyPhysicalMonitors");
}

static void test_devenum(void)
{
    ICreateDevEnum *devenum;
    IEnumMoniker *enummon = NULL;
    IMoniker *moniker = NULL, *empty = (void *)1;
    ULARGE_INTEGER size = {{0}};
    IStream *stream;
    HRESULT hr;

    hr = CoCreateInstance(&CLSID_SystemDeviceEnum, NULL, CLSCTX_INPROC_SERVER, &IID_ICreateDevEnum, (void **)&devenum);
    check(hr == S_OK, "create device enumerator: %#lx", hr);
    if (FAILED(hr)) return;
    hr = ICreateDevEnum_CreateClassEnumerator(devenum, &CLSID_LegacyAmFilterCategory, &enummon, 0);
    check(hr == S_OK, "filter category: %#lx", hr);
    if (hr == S_OK && IEnumMoniker_Next(enummon, 1, &moniker, NULL) == S_OK)
    {
        IEnumMoniker *clone = NULL;

        hr = IEnumMoniker_Clone(enummon, &clone);
        check(hr == S_OK && clone, "enumerator Clone: %#lx", hr);
        if (clone) IEnumMoniker_Release(clone);
        hr = IMoniker_IsDirty(moniker);
        check(hr == S_FALSE, "IsDirty: %#lx", hr);
        hr = IMoniker_GetSizeMax(moniker, &size);
        check(hr == S_OK && size.QuadPart > 8, "GetSizeMax: %#lx %lu", hr, (ULONG)size.QuadPart);
        hr = IMoniker_Enum(moniker, TRUE, (IEnumMoniker **)&empty);
        check(hr == S_OK && !empty, "Enum: %#lx %p", hr, empty);
        CreateStreamOnHGlobal(NULL, TRUE, &stream);
        hr = IMoniker_Save(moniker, stream, FALSE);
        check(hr == S_OK, "Save: %#lx", hr);
        IStream_Seek(stream, (LARGE_INTEGER){{0}}, STREAM_SEEK_SET, NULL);
        hr = IMoniker_Load(moniker, stream);
        check(hr == S_OK, "Load: %#lx", hr);
        IStream_Release(stream);
        IMoniker_Release(moniker);
    }
    else check(0, "no filter moniker");
    if (enummon) IEnumMoniker_Release(enummon);
    ICreateDevEnum_Release(devenum);
}

static void test_qasf(void)
{
    IClassFactory *factory;
    IBaseFilter *filter;
    HRESULT hr;

    hr = CoGetClassObject(&CLSID_wine_WMAsfReader, CLSCTX_INPROC_SERVER, NULL, &IID_IClassFactory, (void **)&factory);
    check(hr == S_OK, "ASF reader class object: %#lx", hr);
    if (FAILED(hr)) return;
    hr = IClassFactory_LockServer(factory, TRUE);
    check(hr == S_OK, "LockServer(TRUE): %#lx", hr);
    hr = IClassFactory_LockServer(factory, FALSE);
    check(hr == S_OK, "LockServer(FALSE): %#lx", hr);
    hr = IClassFactory_CreateInstance(factory, NULL, &IID_IBaseFilter, (void **)&filter);
    check(hr == S_OK, "create ASF reader: %#lx", hr);
    if (SUCCEEDED(hr)) IBaseFilter_Release(filter);
    IClassFactory_Release(factory);
}

static void test_mmdevapi(void)
{
    IMMDeviceEnumerator *enumerator;
    IAudioClient *client;
    IMMDevice *device;
    void *spatial = NULL, *clock2 = NULL;
    IAudioClock *clock;
    WAVEFORMATEX *mix;
    UINT64 pos = 1, qpc;
    HRESULT hr;

    hr = CoCreateInstance(&CLSID_MMDeviceEnumerator, NULL, CLSCTX_INPROC_SERVER, &IID_IMMDeviceEnumerator, (void **)&enumerator);
    check(hr == S_OK, "device enumerator: %#lx", hr);
    if (FAILED(hr)) return;
    if (IMMDeviceEnumerator_GetDefaultAudioEndpoint(enumerator, eRender, eMultimedia, &device) != S_OK)
    {
        printf("skip no render endpoint (no sound device here)\n");
        IMMDeviceEnumerator_Release(enumerator);
        return;
    }
    hr = IMMDevice_Activate(device, &IID_wine_ISpatialAudioClient, CLSCTX_INPROC_SERVER, NULL, &spatial);
    check(hr == S_OK, "ISpatialAudioClient: %#lx", hr);
    if (spatial)
    {
        /* IsSpatialAudioStreamAvailable is slot 9; an unknown stream IID is not available */
        hr = ((HRESULT (WINAPI *)(void *, REFIID, const PROPVARIANT *))SLOT(spatial, 9))(spatial, &IID_IUnknown, NULL);
        check(hr == SPTLAUDCLNT_E_STREAM_NOT_AVAILABLE, "IsSpatialAudioStreamAvailable(IUnknown): %#lx", hr);
        release(spatial);
    }
    hr = IMMDevice_Activate(device, &IID_IAudioClient, CLSCTX_INPROC_SERVER, NULL, (void **)&client);
    if (hr == S_OK && IAudioClient_GetMixFormat(client, &mix) == S_OK &&
        IAudioClient_Initialize(client, AUDCLNT_SHAREMODE_SHARED, 0, 1000000, 0, mix, NULL) == S_OK)
    {
        hr = IAudioClient_GetService(client, &IID_IAudioClock, (void **)&clock);
        if (hr == S_OK)
        {
            hr = IAudioClock_QueryInterface(clock, &IID_wine_IAudioClock2, &clock2);
            check(hr == S_OK, "IAudioClock2: %#lx", hr);
            if (clock2)
            {
                hr = ((HRESULT (WINAPI *)(void *, UINT64 *, UINT64 *))SLOT(clock2, 3))(clock2, &pos, &qpc);
                check(hr == S_OK, "GetDevicePosition: %#lx %lu", hr, (ULONG)pos);
                release(clock2);
            }
            IAudioClock_Release(clock);
        }
        CoTaskMemFree(mix);
    }
    else printf("skip shared stream did not initialize (%#lx)\n", hr);
    if (client) IAudioClient_Release(client);
    IMMDevice_Release(device);
    IMMDeviceEnumerator_Release(enumerator);
}

static void test_gamebar(void)
{
    void *factory, *name = NULL;
    const WCHAR *str;
    int trust = -1;
    HRESULT hr;

    init_winrt();
    if (!(factory = get_factory(L"Windows.Gaming.UI.GameBar", &IID_wine_IActivationFactory))) return;
    hr = ((method1)SLOT(factory, 4))(factory, &name);
    str = name ? WindowsGetStringRawBuffer_(name, NULL) : L"";
    check(hr == S_OK && !wcscmp(str, L"Windows.Gaming.UI.GameBar"), "GetRuntimeClassName: %#lx %ls", hr, str);
    if (name) WindowsDeleteString_(name);
    hr = ((method1)SLOT(factory, 5))(factory, &trust);
    check(hr == S_OK && trust == 0, "GetTrustLevel: %#lx %d", hr, trust);
    release(factory);
}

static void test_mediacontrol(void)
{
    void *interop, *controls = NULL, *updater = NULL, *video = NULL, *title = NULL, *got = NULL;
    const WCHAR *str;
    HWND hwnd;
    HRESULT hr;

    init_winrt();
    if (!(interop = get_factory(L"Windows.Media.SystemMediaTransportControls", &IID_wine_ISTMCInterop))) return;
    hwnd = CreateWindowA("static", "smoke", WS_POPUP, 0, 0, 10, 10, NULL, NULL, NULL, NULL);
    check(hwnd != NULL, "window: %lu", GetLastError());
    hr = ((method3)SLOT(interop, 6))(interop, hwnd, (void *)&IID_wine_ISTMC, &controls);
    check(hr == S_OK, "GetForWindow: %#lx", hr);
    if (controls)
    {
        hr = ((method1)SLOT(controls, 8))(controls, &updater);
        check(hr == S_OK, "get_DisplayUpdater: %#lx", hr);
    }
    if (updater)
    {
        hr = ((HRESULT (WINAPI *)(void *, int))SLOT(updater, 7))(updater, 2 /* Video */);
        check(hr == S_OK, "put_Type(Video): %#lx", hr);
        hr = ((method1)SLOT(updater, 13))(updater, &video);
        check(hr == S_OK && video, "get_VideoProperties: %#lx", hr);
    }
    if (video)
    {
        WindowsCreateString_(L"smoke title", 11, &title);
        hr = ((method1)SLOT(video, 7))(video, title);
        check(hr == S_OK, "put_Title: %#lx", hr);
        hr = ((method1)SLOT(video, 6))(video, &got);
        str = got ? WindowsGetStringRawBuffer_(got, NULL) : L"";
        check(hr == S_OK && !wcscmp(str, L"smoke title"), "get_Title: %#lx %ls", hr, str);
        WindowsDeleteString_(title);
        if (got) WindowsDeleteString_(got);
        release(video);
    }
    release(updater);
    release(controls);
    release(interop);
    DestroyWindow(hwnd);
}

DEFINE_GUID(SMOKE_FORMAT, 0x12345678,0x1234,0x1234,0x12,0x34,0x12,0x34,0x12,0x34,0x12,0x34);

static void test_mfplat(void)
{
    AM_MEDIA_TYPE am = {0}, out = {0};
    IMFMediaType *type;
    BYTE blob[24];
    HRESULT hr;
    int i;

    for (i = 0; i < sizeof(blob); ++i) blob[i] = i * 7;
    MFStartup(MF_VERSION, MFSTARTUP_FULL);
    am.majortype = MEDIATYPE_Video;
    am.subtype = MEDIASUBTYPE_RGB32;
    am.formattype = SMOKE_FORMAT;
    am.cbFormat = sizeof(blob);
    am.pbFormat = blob;
    MFCreateMediaType(&type);
    hr = MFInitMediaTypeFromAMMediaType(type, &am);
    check(hr == S_OK, "MFInitMediaTypeFromAMMediaType(unknown format): %#lx", hr);
    hr = MFInitAMMediaTypeFromMFMediaType(type, GUID_NULL, &out);
    check(hr == S_OK, "MFInitAMMediaTypeFromMFMediaType: %#lx", hr);
    check(IsEqualGUID(&out.formattype, &SMOKE_FORMAT) && out.cbFormat == sizeof(blob)
            && out.pbFormat && !memcmp(out.pbFormat, blob, sizeof(blob)), "format block came back");
    CoTaskMemFree(out.pbFormat);
    IMFMediaType_Release(type);
    MFShutdown();
}

static void test_mf(void)
{
    HRESULT (WINAPI *create_profile)(void **) = (void *)GetProcAddress(LoadLibraryA("mf.dll"), "MFCreateTranscodeProfile");
    HRESULT (WINAPI *create_wma)(IMFMediaType *, IMFAttributes *, IMFActivate **) =
            (void *)GetProcAddress(GetModuleHandleA("mf.dll"), "MFCreateWMAEncoderActivate");
    void *profile = NULL;
    IMFAttributes *attr, *got = NULL;
    IMFMediaSession *session = NULL;
    IMFActivate *activate = NULL;
    IMFMediaType *type;
    UINT32 value = 0;
    HRESULT hr;

    MFStartup(MF_VERSION, MFSTARTUP_FULL);
    hr = create_profile(&profile);
    check(hr == S_OK, "MFCreateTranscodeProfile: %#lx", hr);
    if (SUCCEEDED(hr))
    {
        MFCreateAttributes(&attr, 1);
        IMFAttributes_SetUINT32(attr, &MF_MT_AUDIO_NUM_CHANNELS, 2);
        /* IMFTranscodeProfile: SetAudioAttributes 3, GetAudioAttributes 4 */
        hr = ((method1)SLOT(profile, 3))(profile, attr);
        check(hr == S_OK, "SetAudioAttributes: %#lx", hr);
        hr = ((method1)SLOT(profile, 4))(profile, &got);
        if (got) IMFAttributes_GetUINT32(got, &MF_MT_AUDIO_NUM_CHANNELS, &value);
        check(hr == S_OK && value == 2, "GetAudioAttributes: %#lx %u", hr, value);
        if (got) IMFAttributes_Release(got);
        IMFAttributes_Release(attr);
        release(profile);
    }
    hr = MFCreatePMPMediaSession(0, NULL, &session, NULL);
    check(hr == S_OK && session, "MFCreatePMPMediaSession: %#lx", hr);
    if (session)
    {
        IMFMediaSession_Shutdown(session);
        IMFMediaSession_Release(session);
    }
    MFCreateMediaType(&type);
    IMFMediaType_SetGUID(type, &MF_MT_MAJOR_TYPE, &MFMediaType_Audio);
    IMFMediaType_SetGUID(type, &MF_MT_SUBTYPE, &MFAudioFormat_WMAudioV8);
    hr = create_wma(type, NULL, &activate);
    check(hr == S_OK && activate, "MFCreateWMAEncoderActivate: %#lx", hr);
    if (activate) IMFActivate_Release(activate);
    IMFMediaType_Release(type);
    MFShutdown();
}

static void test_mfreadwrite(void)
{
    IMFSourceReaderEx *reader = NULL;
    IMFSourceReader *base;
    IMFMediaType *native;
    DWORD flags = 0xdeadbeef;
    HRESULT hr;

    MFStartup(MF_VERSION, MFSTARTUP_FULL);
    hr = MFCreateSourceReaderFromURL(L"smoke.wav", NULL, &base);
    check(hr == S_OK, "source reader on a WAV file: %#lx", hr);
    if (SUCCEEDED(hr))
    {
        hr = IMFSourceReader_QueryInterface(base, &IID_IMFSourceReaderEx, (void **)&reader);
        check(hr == S_OK, "IMFSourceReaderEx: %#lx", hr);
        if (reader && IMFSourceReaderEx_GetNativeMediaType(reader, 0, 0, &native) == S_OK)
        {
            hr = IMFSourceReaderEx_SetNativeMediaType(reader, 0, native, &flags);
            check(hr == S_OK && flags != 0xdeadbeef, "SetNativeMediaType: %#lx %#lx", hr, flags);
            hr = IMFSourceReaderEx_RemoveAllTransformsForStream(reader, 0);
            check(hr == S_OK, "RemoveAllTransformsForStream: %#lx", hr);
            IMFMediaType_Release(native);
        }
        else check(0, "no native type");
        if (reader) IMFSourceReaderEx_Release(reader);
        IMFSourceReader_Release(base);
    }
    MFShutdown();
}

static void test_qdvd(void)
{
    ULONGLONG id = 0, id2 = 1;
    WCHAR path[MAX_PATH];
    void *info;
    HRESULT hr;

    hr = CoCreateInstance(&CLSID_wine_DVDNavigator, NULL, CLSCTX_INPROC_SERVER, &IID_wine_IDvdInfo2, &info);
    check(hr == S_OK, "DVD navigator: %#lx", hr);
    if (FAILED(hr)) return;
    GetFullPathNameW(L"dvd", MAX_PATH, path, NULL);
    /* IDvdInfo2::GetDiscID is slot 31 */
    hr = ((HRESULT (WINAPI *)(void *, const WCHAR *, ULONGLONG *))SLOT(info, 31))(info, path, &id);
    check(hr == S_OK && id, "GetDiscID: %#lx %08lx%08lx", hr, (ULONG)(id >> 32), (ULONG)id);
    hr = ((HRESULT (WINAPI *)(void *, const WCHAR *, ULONGLONG *))SLOT(info, 31))(info, path, &id2);
    check(hr == S_OK && id == id2, "GetDiscID is stable: %#lx", hr);
    hr = ((HRESULT (WINAPI *)(void *, const WCHAR *, ULONGLONG *))SLOT(info, 31))(info, L"C:\\no-such-disc", &id2);
    check(FAILED(hr), "GetDiscID of a missing disc fails: %#lx", hr);
    release(info);
}

static void test_wmvcore(void)
{
    HMODULE mod = LoadLibraryA("wmvcore.dll");
    HRESULT (WINAPI *validate)(BYTE *, DWORD *) = (void *)GetProcAddress(mod, "WMValidateData");
    HRESULT (WINAPI *offline)(const WCHAR *, const WCHAR *, BOOL *) = (void *)GetProcAddress(mod, "WMIsAvailableOffline");
    static const BYTE header[16] = {0x30,0x26,0xb2,0x75,0x8e,0x66,0xcf,0x11,0xa6,0xd9,0x00,0xaa,0x00,0x62,0xce,0x6c};
    BYTE data[64] = {0};
    DWORD size = sizeof(data);
    BOOL available = FALSE;
    HRESULT hr;

    check(validate && offline, "exports");
    if (!validate || !offline) return;
    memcpy(data, header, sizeof(header));
    hr = validate(data, &size);
    check(hr == S_OK, "WMValidateData(ASF header): %#lx", hr);
    memset(data, 0x55, sizeof(data));
    size = sizeof(data);
    hr = validate(data, &size);
    check(FAILED(hr), "WMValidateData(junk): %#lx", hr);
    hr = offline(L"smoke.wav", L"", &available);
    check(hr == S_OK && available, "WMIsAvailableOffline(local file): %#lx %d", hr, available);
}

static void test_msdmo(void)
{
    HMODULE mod = LoadLibraryA("msdmo.dll");
    HRESULT (WINAPI *to_str)(WCHAR *, REFGUID) = (void *)GetProcAddress(mod, "DMOGuidToStrW");
    HRESULT (WINAPI *to_guid)(const WCHAR *, GUID *) = (void *)GetProcAddress(mod, "DMOStrToGuidW");
    HRESULT (WINAPI *to_str_a)(char *, REFGUID) = (void *)GetProcAddress(mod, "DMOGuidToStrA");
    WCHAR str[64];
    char stra[64];
    GUID guid;
    HRESULT hr;

    check(to_str && to_guid && to_str_a, "exports");
    if (!to_str || !to_guid || !to_str_a) return;
    hr = to_str(str, &GUID_DSFX_STANDARD_ECHO);
    check(hr == S_OK && wcslen(str) == 36 && str[0] != '{', "DMOGuidToStrW: %#lx %ls", hr, str);
    hr = to_guid(str, &guid);
    check(hr == S_OK && IsEqualGUID(&guid, &GUID_DSFX_STANDARD_ECHO), "DMOStrToGuidW round trip: %#lx", hr);
    hr = to_str_a(stra, &GUID_DSFX_STANDARD_ECHO);
    check(hr == S_OK && strlen(stra) == 36, "DMOGuidToStrA: %#lx %s", hr, stra);
}

static void test_mcistr(const char *cmd, MCIERROR expect, const char *expect_ret)
{
    char ret[128] = "";
    MCIERROR err = mciSendStringA(cmd, ret, sizeof(ret), NULL);

    if (expect_ret)
        check(err == expect && !strcmp(ret, expect_ret), "\"%s\" -> %lu \"%s\"", cmd, err, ret);
    else
        check(err == expect, "\"%s\" -> %lu \"%s\"", cmd, err, ret);
}

static void test_mciqtz32(void)
{
    test_mcistr("open smoke.wav type mpegvideo alias q", 0, NULL);
    test_mcistr("status q number of tracks", 0, "1");
    test_mcistr("status q media present", 0, "true");
    test_mcistr("set q speed 500", 0, NULL);
    test_mcistr("seek q to end", 0, NULL);
    test_mcistr("set q door open", MCIERR_UNSUPPORTED_FUNCTION, NULL);
    test_mcistr("set q audio all off", 0, NULL);
    test_mcistr("setaudio q volume to 500", 0, NULL);
    test_mcistr("info q product", 0, NULL);
    test_mcistr("close q", 0, NULL);
}

static void test_mciavi32(void)
{
    test_mcistr("open smoke.avi type avivideo alias a", 0, NULL);
    test_mcistr("setaudio a volume to 500", 0, NULL);
    test_mcistr("setvideo a off", 0, NULL);
    test_mcistr("realize a", 0, NULL);
    test_mcistr("cue a to 1", 0, NULL);
    test_mcistr("signal a at 1", 0, NULL);
    test_mcistr("close a", 0, NULL);
}

static void test_msacm32(void)
{
    WAVEFORMATEX src, dst;
    ACMSTREAMHEADER header = {sizeof(header)};
    BYTE in[8], out[12];
    HACMSTREAM stream;
    DWORD count = 1;
    MMRESULT mr;

    mr = acmMetrics(NULL, ACM_METRIC_COUNT_HARDWARE, &count);
    check(mr == MMSYSERR_NOERROR && count == 0, "ACM_METRIC_COUNT_HARDWARE: %u %lu", mr, count);
    fill_pcm(&src, 16, 2, 44100);
    fill_pcm(&dst, 24, 2, 44100);
    mr = acmStreamOpen(&stream, NULL, &src, &dst, NULL, 0, 0, 0);
    check(mr == MMSYSERR_NOERROR, "open 16 -> 24 bit stream: %u", mr);
    if (mr) return;
    ((short *)in)[0] = 0x1234; ((short *)in)[1] = -2; ((short *)in)[2] = 0x7fff; ((short *)in)[3] = 0;
    header.pbSrc = in; header.cbSrcLength = sizeof(in);
    header.pbDst = out; header.cbDstLength = sizeof(out);
    acmStreamPrepareHeader(stream, &header, 0);
    mr = acmStreamConvert(stream, &header, 0);
    check(mr == MMSYSERR_NOERROR && header.cbDstLengthUsed == 12, "convert: %u %lu", mr, header.cbDstLengthUsed);
    check(out[0] == 0 && out[1] == 0x34 && out[2] == 0x12 && out[4] == 0xfe && out[5] == 0xff,
          "24 bit samples %02x%02x%02x %02x%02x%02x", out[2], out[1], out[0], out[5], out[4], out[3]);
    acmStreamUnprepareHeader(stream, &header, 0);
    mr = acmStreamMessage(stream, 5, 0, 0);
    check(mr == MMSYSERR_INVALPARAM, "acmStreamMessage(system message): %u", mr);
    mr = acmStreamMessage(stream, ACMDM_USER + 1, 0, 0);
    check(mr != MMSYSERR_ERROR && mr != MMSYSERR_INVALPARAM, "acmStreamMessage(driver message) reaches the driver: %u", mr);
    acmStreamClose(stream, 0);
}

static void test_misc(void)
{
    static const char *dlls[] = {"dmime.dll", "dinput.dll", "dinput8.dll", "dxdiagn.dll", "wmp.dll", "mciwave.dll",
                                 "xinputuap.dll", "dpwsockx.dll", "wmphoto.dll", "mfmediaengine.dll", "mfmp4srcsnk.dll"};
    unsigned int i;

    for (i = 0; i < ARRAYSIZE(dlls); ++i)
        check(LoadLibraryA(dlls[i]) != NULL, "load %s", dlls[i]);
    check(GetProcAddress(GetModuleHandleA("xinputuap.dll"), "XInputGetAudioDeviceIds") != NULL, "XInputGetAudioDeviceIds export");
    check(GetProcAddress(GetModuleHandleA("mfmediaengine.dll"), "DllGetActivationFactory") != NULL, "mfmediaengine DllGetActivationFactory");
    check(GetProcAddress(GetModuleHandleA("dpwsockx.dll"), "SPInit") != NULL, "dpwsockx SPInit");
    test_mcistr("open smoke.wav type waveaudio alias w", 0, NULL);
    test_mcistr("save w smoke-copy.wav wait", 0, NULL);
    test_mcistr("close w", 0, NULL);
    check(GetFileAttributesA("smoke-copy.wav") != INVALID_FILE_ATTRIBUTES, "mciwave saved the copy");
    check(GetFileAttributesA("smoke.wav") != INVALID_FILE_ATTRIBUTES, "the original is still there");
}

static void test_avifil32(void)
{
    AVISTREAMINFOW info;
    PAVISTREAM stream;
    PAVIFILE file;
    HRESULT hr;

    AVIFileInit();
    hr = AVIFileOpenW(&file, L"smoke.avi", OF_READWRITE, NULL);
    check(hr == AVIERR_OK, "AVIFileOpen(read/write): %#lx", hr);
    if (hr == AVIERR_OK)
    {
        hr = AVIFileGetStream(file, &stream, streamtypeVIDEO, 0);
        check(hr == AVIERR_OK, "AVIFileGetStream: %#lx", hr);
        if (hr == AVIERR_OK)
        {
            AVIStreamInfoW(stream, &info, sizeof(info));
            wcscpy(info.szName, L"smoke video");
            info.wLanguage = 0x409;
            hr = stream->lpVtbl->SetInfo(stream, &info, sizeof(info));
            check(hr == AVIERR_OK, "SetInfo: %#lx", hr);
            memset(&info, 0, sizeof(info));
            AVIStreamInfoW(stream, &info, sizeof(info));
            check(!wcscmp(info.szName, L"smoke video") && info.wLanguage == 0x409 && info.dwEditCount == 1,
                  "info changed: %ls %#x %lu", info.szName, info.wLanguage, info.dwEditCount);
            hr = stream->lpVtbl->SetInfo(stream, &info, sizeof(info) - 1);
            check(hr == AVIERR_BADSIZE, "SetInfo(short): %#lx", hr);
            if (info.dwLength == 4)
            {
                /* a frame added to the existing file goes after its frames */
                BYTE frame[768];

                memset(frame, 0x77, sizeof(frame));
                hr = AVIStreamWrite(stream, 4, 1, frame, sizeof(frame), AVIIF_KEYFRAME, NULL, NULL);
                check(hr == AVIERR_OK, "AVIStreamWrite(frame 4): %#lx", hr);
            }
            AVIStreamRelease(stream);
        }
        AVIFileRelease(file);
    }
    hr = AVIFileOpenW(&file, L"smoke.avi", OF_READ, NULL);
    check(hr == AVIERR_OK, "reopen after saving: %#lx", hr);
    if (hr == AVIERR_OK)
    {
        if (AVIFileGetStream(file, &stream, streamtypeVIDEO, 0) == AVIERR_OK)
        {
            BYTE frame[768];
            LONG bytes = 0, samples = 0;

            AVIStreamInfoW(stream, &info, sizeof(info));
            /* the longer header moved the frames (the generated file has no header room) */
            check(!wcscmp(info.szName, L"smoke video") && info.dwLength >= 5, "saved info: %ls, %lu frames",
                  info.szName, info.dwLength);
            memset(frame, 0, sizeof(frame));
            hr = AVIStreamRead(stream, 3, 1, frame, sizeof(frame), &bytes, &samples);
            check(hr == AVIERR_OK && bytes == sizeof(frame) && frame[0] == 0x40 && frame[1] == 0x80 && frame[767] == 0xc0,
                  "frame 3 after the move: %#lx %ld %02x%02x", hr, bytes, frame[0], frame[1]);
            hr = AVIStreamRead(stream, 4, 1, frame, sizeof(frame), &bytes, &samples);
            check(hr == AVIERR_OK && frame[0] == 0x77 && frame[767] == 0x77, "added frame 4: %#lx %02x", hr, frame[0]);
            hr = stream->lpVtbl->SetInfo(stream, &info, sizeof(info));
            check(hr == AVIERR_READONLY, "SetInfo on a read-only file: %#lx", hr);
            AVIStreamRelease(stream);
        }
        AVIFileRelease(file);
    }
    AVIFileExit();
}

static void test_msvfw32(void)
{
    WCHAR device[64] = L"";
    HWND hwnd;
    LRESULT ret;

    hwnd = MCIWndCreateW(NULL, GetModuleHandleW(NULL), WS_POPUP | MCIWNDF_NOERRORDLG, NULL);
    check(hwnd != NULL, "MCIWndCreate");
    if (!hwnd) return;
    ret = SendMessageW(hwnd, MCIWNDM_NEWW, 0, (LPARAM)L"waveaudio");
    check(!ret, "MCIWNDM_NEW(waveaudio): %ld", (long)ret);
    SendMessageW(hwnd, MCIWNDM_GETDEVICEW, ARRAYSIZE(device), (LPARAM)device);
    check(!_wcsicmp(device, L"waveaudio"), "device after new: %ls", device);
    check(SendMessageW(hwnd, MCIWNDM_GETDEVICEID, 0, 0) != 0, "a device is open");
    ret = SendMessageW(hwnd, MCIWNDM_NEWW, 0, 0);
    check(!ret, "MCIWNDM_NEW(current device): %ld", (long)ret);
    DestroyWindow(hwnd);
}

static void test_amstream(void)
{
    IAMMultiMediaStream *mmstream;
    IAudioStreamSample *sample;
    IAudioMediaStream *audio;
    IMediaStream *stream;
    IAudioData *data;
    WAVEFORMATEX wfx;
    BYTE buffer[4096];
    HRESULT hr;

    hr = CoCreateInstance(&CLSID_AMMultiMediaStream, NULL, CLSCTX_INPROC_SERVER, &IID_IAMMultiMediaStream, (void **)&mmstream);
    check(hr == S_OK, "multimedia stream: %#lx", hr);
    if (FAILED(hr)) return;
    IAMMultiMediaStream_Initialize(mmstream, STREAMTYPE_READ, 0, NULL);
    hr = IAMMultiMediaStream_AddMediaStream(mmstream, NULL, &MSPID_PrimaryAudio, 0, &stream);
    check(hr == S_OK, "audio stream: %#lx", hr);
    if (SUCCEEDED(hr))
    {
        IMediaStream_QueryInterface(stream, &IID_IAudioMediaStream, (void **)&audio);
        CoCreateInstance(&CLSID_AMAudioData, NULL, CLSCTX_INPROC_SERVER, &IID_IAudioData, (void **)&data);
        fill_pcm(&wfx, 16, 2, 44100);
        IAudioData_SetFormat(data, &wfx);
        IAudioData_SetBuffer(data, sizeof(buffer), buffer, 0);
        hr = IAudioMediaStream_CreateSample(audio, data, 0, &sample);
        check(hr == S_OK, "CreateSample: %#lx", hr);
        if (SUCCEEDED(hr))
        {
            hr = IAudioStreamSample_Update(sample, SSUPDATE_ASYNC | SSUPDATE_CONTINUOUS, NULL, NULL, 0);
            check(hr == MS_E_NOTRUNNING, "continuous Update of a stopped stream: %#lx", hr);
            hr = IAudioStreamSample_Update(sample, 0x80, NULL, NULL, 0);
            check(hr == E_NOTIMPL, "Update with an unknown flag: %#lx", hr);
            IAudioStreamSample_Release(sample);
        }
        IAudioData_Release(data);
        IAudioMediaStream_Release(audio);
        IMediaStream_Release(stream);
    }
    IAMMultiMediaStream_Release(mmstream);
}

int main(int argc, char **argv)
{
    static const struct { const char *name; void (*test)(void); } tests[] =
    {
        {"dsdmo", test_dsdmo}, {"qcap", test_qcap}, {"dxva2", test_dxva2}, {"devenum", test_devenum},
        {"qasf", test_qasf}, {"mmdevapi", test_mmdevapi}, {"gamebar", test_gamebar},
        {"mediacontrol", test_mediacontrol}, {"mfplat", test_mfplat}, {"mf", test_mf},
        {"mfreadwrite", test_mfreadwrite}, {"qdvd", test_qdvd},
        {"wmvcore", test_wmvcore}, {"msdmo", test_msdmo}, {"mciqtz32", test_mciqtz32},
        {"mciavi32", test_mciavi32}, {"msacm32", test_msacm32}, {"misc", test_misc},
        {"avifil32", test_avifil32}, {"msvfw32", test_msvfw32}, {"amstream", test_amstream},
    };
    unsigned int i;

    if (argc < 2) return 2;
    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    for (i = 0; i < ARRAYSIZE(tests); ++i)
        if (!strcmp(argv[1], tests[i].name)) break;
    if (i == ARRAYSIZE(tests)) { printf("unknown section %s\n", argv[1]); return 2; }
    tests[i].test();
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
