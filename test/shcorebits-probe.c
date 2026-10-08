/* shcore functions that were stubs (patches/sg/1620): the process's
 * explicit AppUserModelID, scale factors, SHReleaseThreadRef, IsOS answers
 * and scale change registrations. The gate runs it at 144 dpi. */
#define COBJMACROS
#include <windows.h>
#include <shellscalingapi.h>
#include <shlwapi.h>
#include <stdio.h>
#include <wchar.h>

typedef HRESULT (WINAPI *set_aumid_t)(const WCHAR *);
typedef HRESULT (WINAPI *get_aumid_t)(WCHAR **);
typedef HRESULT (WINAPI *scale_monitor_t)(HMONITOR, DEVICE_SCALE_FACTOR *);
typedef DEVICE_SCALE_FACTOR (WINAPI *scale_device_t)(DISPLAY_DEVICE_TYPE);
typedef HRESULT (WINAPI *thread_ref_t)(IUnknown *);
typedef HRESULT (WINAPI *get_thread_ref_t)(IUnknown **);
typedef HRESULT (WINAPI *release_ref_t)(void);
typedef HRESULT (WINAPI *reg_event_t)(HANDLE, DWORD_PTR *);
typedef HRESULT (WINAPI *unreg_event_t)(DWORD_PTR);
typedef BOOL (WINAPI *isos_t)(DWORD);
typedef HRESULT (WINAPI *awareness_t)(int);

static int failures;
static LONG refs = 1;

static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) failures++;
}

static HRESULT WINAPI unk_qi(IUnknown *iface, REFIID iid, void **obj) { *obj = iface; return S_OK; }
static ULONG WINAPI unk_addref(IUnknown *iface) { return InterlockedIncrement(&refs); }
static ULONG WINAPI unk_release(IUnknown *iface) { return InterlockedDecrement(&refs); }
static IUnknownVtbl unk_vtbl = { unk_qi, unk_addref, unk_release };
static IUnknown unk = { &unk_vtbl };

int main(int argc, char **argv)
{
    HMODULE shcore = LoadLibraryA("shcore.dll");
    set_aumid_t pSet = (void *)GetProcAddress(shcore, "SetCurrentProcessExplicitAppUserModelID");
    get_aumid_t pGet = (void *)GetProcAddress(shcore, "GetCurrentProcessExplicitAppUserModelID");
    scale_monitor_t pGetScaleFactorForMonitor = (void *)GetProcAddress(shcore, "GetScaleFactorForMonitor");
    scale_device_t pGetScaleFactorForDevice = (void *)GetProcAddress(shcore, "GetScaleFactorForDevice");
    thread_ref_t pSHSetThreadRef = (void *)GetProcAddress(shcore, "SHSetThreadRef");
    get_thread_ref_t pSHGetThreadRef = (void *)GetProcAddress(shcore, "SHGetThreadRef");
    release_ref_t pSHReleaseThreadRef = (void *)GetProcAddress(shcore, "SHReleaseThreadRef");
    reg_event_t pRegisterScaleChangeEvent = (void *)GetProcAddress(shcore, "RegisterScaleChangeEvent");
    unreg_event_t pUnregisterScaleChangeEvent = (void *)GetProcAddress(shcore, "UnregisterScaleChangeEvent");
    isos_t pIsOS = (void *)GetProcAddress(shcore, "IsOS");
    awareness_t pSetProcessDpiAwareness = (void *)GetProcAddress(shcore, "SetProcessDpiAwareness");
    WCHAR *aumid = NULL, longid[200];
    DEVICE_SCALE_FACTOR scale = 0;
    IUnknown *out = NULL;
    DWORD_PTR cookie = 0;
    HANDLE event;
    HRESULT hr;
    UINT expect = argc > 1 ? atoi(argv[1]) : 0;
    static const POINT origin;
    int i;

    pSetProcessDpiAwareness(2 /* PROCESS_PER_MONITOR_DPI_AWARE */);
    CoInitialize(NULL);

    hr = pGet(&aumid);
    check(hr == E_FAIL && !aumid, "no explicit AppUserModelID yet: E_FAIL");
    hr = pSet(L"StainedGlass.Probe.App");
    check(hr == S_OK, "SetCurrentProcessExplicitAppUserModelID");
    hr = pGet(&aumid);
    check(hr == S_OK && aumid && !wcscmp(aumid, L"StainedGlass.Probe.App"), "... and GetCurrentProcessExplicitAppUserModelID reads it back");
    CoTaskMemFree(aumid);
    for (i = 0; i < 199; i++) longid[i] = 'a';
    longid[199] = 0;
    check(pSet(longid) == E_INVALIDARG, "an ID over 128 characters is refused");

    hr = pGetScaleFactorForMonitor(MonitorFromPoint(origin, MONITOR_DEFAULTTOPRIMARY), &scale);
    printf("scale factor %d (expect %u)\n", scale, expect);
    if (expect) check(hr == S_OK && scale == expect, "GetScaleFactorForMonitor follows the display's DPI");
    check(pGetScaleFactorForDevice(DEVICE_PRIMARY) == scale, "GetScaleFactorForDevice(DEVICE_PRIMARY) agrees");

    pSHSetThreadRef(&unk);
    hr = pSHReleaseThreadRef();
    check(hr == S_OK && refs == 0, "SHReleaseThreadRef releases the thread's reference");
    check(pSHGetThreadRef(&out) == E_NOINTERFACE, "... and the thread has none after");

    check(pIsOS(OS_FASTUSERSWITCHING) && pIsOS(OS_WELCOMELOGONUI), "IsOS: fast user switching, welcome screen");
    check(!pIsOS(OS_TERMINALCLIENT) && !pIsOS(OS_APPLIANCE), "IsOS: not a remote session, not an appliance");

    event = CreateEventW(NULL, FALSE, FALSE, NULL);
    hr = pRegisterScaleChangeEvent(event, &cookie);
    check(hr == S_OK && cookie, "RegisterScaleChangeEvent");
    check(pUnregisterScaleChangeEvent(cookie) == S_OK, "UnregisterScaleChangeEvent");
    check(pUnregisterScaleChangeEvent(cookie) == E_INVALIDARG, "... twice: E_INVALIDARG");

    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
