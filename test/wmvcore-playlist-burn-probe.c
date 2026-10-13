/* Probe for patches/sg/4587: IWMReaderPlaylistBurn of the WM sync reader:
 * InitPlaylistBurn checks the files asynchronously and sends
 * WMT_INIT_PLAYLIST_BURN to the status callback; GetInitResults gives S_OK
 * for each file that can be copied and an error for one that cannot be read;
 * EndPlaylistBurn ends it. Table driven; lines "ok"/"FAIL", last
 * "RESULT: PASS|FAIL". */
#define COBJMACROS
#include <windows.h>
#include <wmsdk.h>
#include <stdio.h>

static const GUID iid_burn = {0xf28c0300,0x9baa,0x4477,{0xa8,0x46,0x17,0x44,0xd9,0xcb,0xf5,0x33}};
static const GUID iid_status = {0x6d7cdc70,0x9888,0x11d3,{0x8e,0xdc,0x00,0xc0,0x4f,0x61,0x09,0xcf}};

static int failures;
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

static HANDLE done_event;
static LONG status_calls;
static WMT_STATUS got_status;
static HRESULT got_hr;
static void *got_context;
static DWORD got_thread;

static HRESULT WINAPI cb_qi(IWMStatusCallback *iface, REFIID iid, void **out)
{
    if (IsEqualGUID(iid, &IID_IUnknown) || IsEqualGUID(iid, &iid_status)) { *out = iface; return S_OK; }
    *out = NULL;
    return E_NOINTERFACE;
}
static ULONG WINAPI cb_addref(IWMStatusCallback *iface) { return 2; }
static ULONG WINAPI cb_release(IWMStatusCallback *iface) { return 1; }
static HRESULT WINAPI cb_status(IWMStatusCallback *iface, WMT_STATUS status, HRESULT hr, WMT_ATTR_DATATYPE type,
        BYTE *value, void *context)
{
    InterlockedIncrement(&status_calls);
    got_status = status; got_hr = hr; got_context = context; got_thread = GetCurrentThreadId();
    SetEvent(done_event);
    return S_OK;
}
static IWMStatusCallbackVtbl cb_vtbl = {cb_qi, cb_addref, cb_release, cb_status};
static IWMStatusCallback callback = {&cb_vtbl};

int main(int argc, char **argv)
{
    IWMSyncReader *reader;
    IWMReaderPlaylistBurn *burn;
    WCHAR dir[MAX_PATH], present[MAX_PATH], missing[MAX_PATH];
    const WCHAR *files[3];
    HRESULT hrs[3], hr;
    HANDLE f;
    DWORD r;

    HRESULT (WINAPI *p_create)(IUnknown *, DWORD, IWMSyncReader **);

    CoInitialize(NULL);
    p_create = (void *)GetProcAddress(LoadLibraryA("wmvcore.dll"), "WMCreateSyncReader");
    hr = p_create ? p_create(NULL, 0, &reader) : E_FAIL;
    check(hr == S_OK, "WMCreateSyncReader %#lx", hr);
    if (FAILED(hr)) { printf("RESULT: FAIL\n"); return 1; }
    hr = IWMSyncReader_QueryInterface(reader, &iid_burn, (void **)&burn);
    check(hr == S_OK, "IWMReaderPlaylistBurn %#lx", hr);
    if (FAILED(hr)) { printf("RESULT: FAIL\n"); return 1; }

    GetTempPathW(MAX_PATH, dir);
    wsprintfW(present, L"%ssg-burn-present.wma", dir);
    wsprintfW(missing, L"%ssg-burn-missing.wma", dir);
    f = CreateFileW(present, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    WriteFile(f, "data", 4, &r, NULL);
    CloseHandle(f);
    DeleteFileW(missing);
    done_event = CreateEventW(NULL, FALSE, FALSE, NULL);

    hr = IWMReaderPlaylistBurn_GetInitResults(burn, 1, hrs);
    check(FAILED(hr), "GetInitResults before any check fails: %#lx", hr);
    files[0] = present; files[1] = missing; files[2] = present;
    check(IWMReaderPlaylistBurn_InitPlaylistBurn(burn, 0, files, &callback, NULL) == E_INVALIDARG, "no files");
    check(IWMReaderPlaylistBurn_InitPlaylistBurn(burn, 3, files, NULL, NULL) == E_INVALIDARG, "no callback");

    hr = IWMReaderPlaylistBurn_InitPlaylistBurn(burn, 3, files, &callback, (void *)0x1234);
    check(hr == S_OK, "InitPlaylistBurn %#lx", hr);
    r = WaitForSingleObject(done_event, 10000);
    check(!r && status_calls == 1 && got_status == WMT_INIT_PLAYLIST_BURN && got_hr == S_OK && got_context == (void *)0x1234,
          "WMT_INIT_PLAYLIST_BURN sent: %lu, %ld calls, status %d, %#lx, context %p", r, status_calls, got_status, got_hr, got_context);
    check(got_thread != GetCurrentThreadId(), "from another thread (asynchronous)");

    hrs[0] = hrs[1] = hrs[2] = 0xdead;
    hr = IWMReaderPlaylistBurn_GetInitResults(burn, 3, hrs);
    check(hr == S_OK && hrs[0] == S_OK && hrs[1] == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND) && hrs[2] == S_OK,
          "GetInitResults: %#lx, files %#lx %#lx %#lx", hr, hrs[0], hrs[1], hrs[2]);
    hr = IWMReaderPlaylistBurn_GetInitResults(burn, 2, hrs);
    check(hr == E_INVALIDARG, "GetInitResults with another count: %#lx", hr);
    check(IWMReaderPlaylistBurn_Cancel(burn) == S_OK, "Cancel");
    check(IWMReaderPlaylistBurn_EndPlaylistBurn(burn, S_OK) == S_OK, "EndPlaylistBurn");
    hr = IWMReaderPlaylistBurn_GetInitResults(burn, 3, hrs);
    check(FAILED(hr), "GetInitResults after EndPlaylistBurn fails: %#lx", hr);

    IWMReaderPlaylistBurn_Release(burn);
    IWMSyncReader_Release(reader);
    DeleteFileW(present);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
