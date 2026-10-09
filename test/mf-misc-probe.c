/* mf's small stubs (patches/sg/2834), run by test/mf-misc-gate.sh: the audio
 * renderer's IMFAudioPolicy (grouping parameter, display name, icon path) and
 * OnClockSetRate, MFEnumDeviceSources for video capture (no devices: an empty
 * list), the class factories' LockServer and the EVR stream's GetService for
 * an unknown service (E_NOTIMPL and FIXMEs before). The audio part needs a
 * render device; without one it prints a note.
 *
 *   mf-misc-probe.exe */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <mfapi.h>
#include <mferror.h>
#include <mfidl.h>
#include <stdio.h>
#include <string.h>
#include <evr.h>

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}
#define CHECKF(ok, ...) do { char _b[256]; snprintf(_b, sizeof(_b), __VA_ARGS__); check(ok, _b); } while (0)

/* IMFAudioPolicy is not in every set of headers: its vtable by hand */
DEFINE_GUID(IID_IMFAudioPolicy_sg, 0xa0638c2b, 0x6465, 0x4395, 0x9a, 0xe7, 0xa3, 0x21, 0xa9, 0xfd, 0x28, 0x56);
typedef struct IMFAudioPolicy IMFAudioPolicy;
typedef struct
{
    HRESULT (WINAPI *QueryInterface)(IMFAudioPolicy *, REFIID, void **);
    ULONG (WINAPI *AddRef)(IMFAudioPolicy *);
    ULONG (WINAPI *Release)(IMFAudioPolicy *);
    HRESULT (WINAPI *SetGroupingParam)(IMFAudioPolicy *, REFGUID);
    HRESULT (WINAPI *GetGroupingParam)(IMFAudioPolicy *, GUID *);
    HRESULT (WINAPI *SetDisplayName)(IMFAudioPolicy *, const WCHAR *);
    HRESULT (WINAPI *GetDisplayName)(IMFAudioPolicy *, WCHAR **);
    HRESULT (WINAPI *SetIconPath)(IMFAudioPolicy *, const WCHAR *);
    HRESULT (WINAPI *GetIconPath)(IMFAudioPolicy *, WCHAR **);
} IMFAudioPolicyVtbl;
struct IMFAudioPolicy { const IMFAudioPolicyVtbl *lpVtbl; };
#define IMFAudioPolicy_GetGroupingParam(p, a) (p)->lpVtbl->GetGroupingParam(p, a)
#define IMFAudioPolicy_SetGroupingParam(p, a) (p)->lpVtbl->SetGroupingParam(p, a)
#define IMFAudioPolicy_GetDisplayName(p, a) (p)->lpVtbl->GetDisplayName(p, a)
#define IMFAudioPolicy_SetDisplayName(p, a) (p)->lpVtbl->SetDisplayName(p, a)
#define IMFAudioPolicy_GetIconPath(p, a) (p)->lpVtbl->GetIconPath(p, a)
#define IMFAudioPolicy_SetIconPath(p, a) (p)->lpVtbl->SetIconPath(p, a)
#define IMFAudioPolicy_Release(p) (p)->lpVtbl->Release(p)

DEFINE_GUID(clsid_rw_factory, 0x48e2ed0f, 0x98c2, 0x4a37, 0xbe, 0xd5, 0x16, 0x63, 0x12, 0xdd, 0xd8, 0x3f);
DEFINE_GUID(TEST_GROUP, 0x12345678, 0x1234, 0x4321, 0x91, 0x82, 0x73, 0x64, 0x55, 0x46, 0x37, 0x28);

static void test_enum_sources(void)
{
    IMFAttributes *attrs;
    IMFActivate **sources = (IMFActivate **)0x1234;
    UINT32 count = 77;
    HRESULT hr;

    MFCreateAttributes(&attrs, 1);
    hr = MFEnumDeviceSources(attrs, &sources, &count);
    CHECKF(hr == MF_E_ATTRIBUTENOTFOUND, "MFEnumDeviceSources without a source type is MF_E_ATTRIBUTENOTFOUND (%#lx)", hr);
    IMFAttributes_SetGUID(attrs, &MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, &MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID);
    hr = MFEnumDeviceSources(attrs, &sources, &count);
    CHECKF(hr == S_OK && count == 0 && sources == NULL,
           "video capture: S_OK, no sources, a NULL array (%#lx, %u, %p)", hr, count, sources);
    IMFAttributes_Release(attrs);
}

static void test_factories_and_evr(void)
{
    HRESULT (WINAPI *get_class_object)(REFCLSID, REFIID, void **);
    IClassFactory *factory = NULL;
    HMODULE rw = LoadLibraryA("mfreadwrite.dll");
    IMFMediaSink *sink = NULL;
    IMFStreamSink *stream = NULL;
    IMFGetService *gs = NULL;
    GUID unknown_service = {0x5eed0001, 0x1234, 0x4321, {1, 2, 3, 4, 5, 6, 7, 8}};
    void *obj = (void *)1;
    HRESULT hr;

    get_class_object = rw ? (void *)GetProcAddress(rw, "DllGetClassObject") : NULL;
    if (get_class_object && SUCCEEDED(get_class_object(&clsid_rw_factory, &IID_IClassFactory, (void **)&factory)))
    {
        hr = IClassFactory_LockServer(factory, TRUE);
        CHECKF(hr == S_OK, "mfreadwrite LockServer(TRUE) is S_OK (%#lx)", hr);
        hr = IClassFactory_LockServer(factory, FALSE);
        CHECKF(hr == S_OK, "mfreadwrite LockServer(FALSE) is S_OK (%#lx)", hr);
        IClassFactory_Release(factory);
    }
    else printf("note  no mfreadwrite class factory: LockServer not checked\n");

    hr = MFCreateVideoRenderer(&IID_IMFMediaSink, (void **)&sink);
    if (hr != S_OK) { printf("note  MFCreateVideoRenderer failed (%#lx): the EVR part is skipped\n", hr); return; }
    hr = IMFMediaSink_GetStreamSinkByIndex(sink, 0, &stream);
    if (hr == S_OK)
    {
        hr = IMFStreamSink_QueryInterface(stream, &IID_IMFGetService, (void **)&gs);
        if (hr == S_OK)
        {
            hr = IMFGetService_GetService(gs, &unknown_service, &IID_IUnknown, &obj);
            CHECKF(hr == MF_E_UNSUPPORTED_SERVICE, "EVR stream: an unknown service is MF_E_UNSUPPORTED_SERVICE (%#lx)", hr);
            IMFGetService_Release(gs);
        }
        else printf("note  the EVR stream has no IMFGetService (%#lx)\n", hr);
        IMFStreamSink_Release(stream);
    }
    else printf("note  no EVR stream sink (%#lx)\n", hr);
    IMFMediaSink_Shutdown(sink);
    IMFMediaSink_Release(sink);
}

int main(void)
{
    IMFMediaSink *sink;
    IMFGetService *gs;
    IMFAudioPolicy *policy;
    IMFClockStateSink *clock_sink;
    GUID guid;
    WCHAR *str;
    HRESULT hr;

    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    hr = MFStartup(MF_VERSION, MFSTARTUP_FULL);
    check(hr == S_OK, "MFStartup");
    test_enum_sources();
    test_factories_and_evr();
    hr = MFCreateAudioRenderer(NULL, &sink);
    if (hr != S_OK)
    {
        printf("note  MFCreateAudioRenderer failed (%#lx): no audio render device, the audio part is skipped\n", hr);
        printf("%s\n", failures ? "RESULT: FAIL" : "RESULT: PASS");
        return failures != 0;
    }
    hr = IMFMediaSink_QueryInterface(sink, &IID_IMFGetService, (void **)&gs);
    check(hr == S_OK, "the sink has IMFGetService");
    hr = IMFGetService_GetService(gs, &MR_AUDIO_POLICY_SERVICE, &IID_IMFAudioPolicy_sg, (void **)&policy);
    check(hr == S_OK, "MR_AUDIO_POLICY_SERVICE gives IMFAudioPolicy");
    if (hr != S_OK) return 1;

    memset(&guid, 0xaa, sizeof(guid));
    hr = IMFAudioPolicy_GetGroupingParam(policy, &guid);
    CHECKF(hr == S_OK && IsEqualGUID(&guid, &GUID_NULL), "the grouping parameter starts as GUID_NULL (%#lx)", hr);
    hr = IMFAudioPolicy_SetGroupingParam(policy, &TEST_GROUP);
    CHECKF(hr == S_OK, "SetGroupingParam (%#lx)", hr);
    memset(&guid, 0xaa, sizeof(guid));
    hr = IMFAudioPolicy_GetGroupingParam(policy, &guid);
    CHECKF(hr == S_OK && IsEqualGUID(&guid, &TEST_GROUP), "GetGroupingParam returns it (%#lx)", hr);
    hr = IMFAudioPolicy_GetGroupingParam(policy, NULL);
    CHECKF(hr == E_POINTER, "GetGroupingParam(NULL) is E_POINTER (%#lx)", hr);

    str = NULL;
    hr = IMFAudioPolicy_GetDisplayName(policy, &str);
    CHECKF(hr == S_OK && str && !str[0], "the display name starts empty (%#lx)", hr);
    CoTaskMemFree(str);
    hr = IMFAudioPolicy_SetDisplayName(policy, L"SG test renderer");
    CHECKF(hr == S_OK, "SetDisplayName (%#lx)", hr);
    str = NULL;
    hr = IMFAudioPolicy_GetDisplayName(policy, &str);
    CHECKF(hr == S_OK && str && !wcscmp(str, L"SG test renderer"), "GetDisplayName returns it (%#lx, %ls)", hr, str ? str : L"(null)");
    CoTaskMemFree(str);
    hr = IMFAudioPolicy_SetDisplayName(policy, L"another");
    str = NULL;
    IMFAudioPolicy_GetDisplayName(policy, &str);
    CHECKF(str && !wcscmp(str, L"another"), "a second SetDisplayName replaces the first");
    CoTaskMemFree(str);
    hr = IMFAudioPolicy_GetDisplayName(policy, NULL);
    CHECKF(hr == E_POINTER, "GetDisplayName(NULL) is E_POINTER (%#lx)", hr);

    str = NULL;
    hr = IMFAudioPolicy_GetIconPath(policy, &str);
    CHECKF(hr == S_OK && str && !str[0], "the icon path starts empty (%#lx)", hr);
    CoTaskMemFree(str);
    hr = IMFAudioPolicy_SetIconPath(policy, L"C:\\icons\\sg.ico");
    CHECKF(hr == S_OK, "SetIconPath (%#lx)", hr);
    str = NULL;
    hr = IMFAudioPolicy_GetIconPath(policy, &str);
    CHECKF(hr == S_OK && str && !wcscmp(str, L"C:\\icons\\sg.ico"), "GetIconPath returns it (%#lx)", hr);
    CoTaskMemFree(str);
    hr = IMFAudioPolicy_GetIconPath(policy, NULL);
    CHECKF(hr == E_POINTER, "GetIconPath(NULL) is E_POINTER (%#lx)", hr);

    hr = IMFMediaSink_QueryInterface(sink, &IID_IMFClockStateSink, (void **)&clock_sink);
    check(hr == S_OK, "the sink has IMFClockStateSink");
    if (hr == S_OK)
    {
        hr = IMFClockStateSink_OnClockSetRate(clock_sink, 0, 1.0f);
        CHECKF(hr == S_OK, "OnClockSetRate(1.0) is S_OK (%#lx)", hr);
        hr = IMFClockStateSink_OnClockSetRate(clock_sink, 0, 2.0f);
        CHECKF(hr == S_OK, "OnClockSetRate(2.0) is S_OK (%#lx)", hr);
    }

    IMFMediaSink_Shutdown(sink);
    hr = IMFAudioPolicy_GetGroupingParam(policy, &guid);
    CHECKF(hr == MF_E_SHUTDOWN, "GetGroupingParam after Shutdown is MF_E_SHUTDOWN (%#lx)", hr);
    hr = IMFAudioPolicy_SetDisplayName(policy, L"x");
    CHECKF(hr == MF_E_SHUTDOWN, "SetDisplayName after Shutdown is MF_E_SHUTDOWN (%#lx)", hr);
    if (clock_sink)
    {
        hr = IMFClockStateSink_OnClockSetRate(clock_sink, 0, 1.0f);
        CHECKF(hr == MF_E_SHUTDOWN, "OnClockSetRate after Shutdown is MF_E_SHUTDOWN (%#lx)", hr);
        IMFClockStateSink_Release(clock_sink);
    }

    IMFAudioPolicy_Release(policy);
    IMFGetService_Release(gs);
    IMFMediaSink_Release(sink);
    MFShutdown();
    printf("%s\n", failures ? "RESULT: FAIL" : "RESULT: PASS");
    return failures != 0;
}
