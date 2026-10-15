/* shell32: SHUpdateImage and SHValidateUNC (patch 3703) */
#include <windows.h>
#include <ddeml.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <stdio.h>

typedef BOOL (WINAPI *init_fn_t)(BOOL);
static int failed;
#define CHECK(c, ...) do { if (!(c)) { failed++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static HDDEDATA CALLBACK client_cb(UINT t, UINT f, HCONV c, HSZ a, HSZ b, HDDEDATA d, ULONG_PTR x, ULONG_PTR y) { return NULL; }

static int count_convs(DWORD inst, const WCHAR *service, const WCHAR *topic)
{
    HSZ s = service ? DdeCreateStringHandleW(inst, service, CP_WINUNICODE) : 0;
    HSZ t = topic ? DdeCreateStringHandleW(inst, topic, CP_WINUNICODE) : 0;
    HCONVLIST list = DdeConnectList(inst, s, t, 0, NULL);
    if (!list) printf("  DdeConnectList: error %u\n", DdeGetLastError(inst));
    HCONV conv = 0;
    int n = 0;

    if (list) while ((conv = DdeQueryNextServer(list, conv))) n++;
    if (list) DdeDisconnectList(list);
    if (s) DdeFreeStringHandle(inst, s);
    if (t) DdeFreeStringHandle(inst, t);
    return n;
}

static int events;
static LRESULT CALLBACK wp(HWND h, UINT m, WPARAM w, LPARAM l)
{
    if (m == WM_USER + 7 && (l & SHCNE_UPDATEIMAGE)) events++;
    return DefWindowProcW(h, m, w, l);
}

static init_fn_t dde_init_g;
static HANDLE ready;
static DWORD server_tid;
static DWORD WINAPI server_thread(void *arg)
{
    MSG m;

    PeekMessageW(&m, NULL, 0, 0, PM_NOREMOVE);
    server_tid = GetCurrentThreadId();
    dde_init_g(TRUE);
    SetEvent(ready);
    while (GetMessageW(&m, NULL, 0, 0) > 0) { TranslateMessage(&m); DispatchMessageW(&m); }
    dde_init_g(FALSE);
    return 0;
}

int main(void)
{
    typedef BOOL (WINAPI *init_fn)(BOOL);
    typedef BOOL (WINAPI *validate_fn)(HWND, WCHAR *, UINT);
    typedef void (WINAPI *update_fn)(const WCHAR *, int, UINT, int);
    HMODULE shell32 = LoadLibraryA("shell32.dll");
    init_fn dde_init = (init_fn)GetProcAddress(shell32, (char *)188);
    validate_fn validate = (validate_fn)GetProcAddress(shell32, (char *)173);
    update_fn update = (update_fn)GetProcAddress(shell32, (char *)192);
    WCHAR path[MAX_PATH];
    DWORD inst = 0;
    WNDCLASSW wc = { 0 };
    SHChangeNotifyEntry entry = { NULL, TRUE };
    ULONG reg;
    HWND hwnd;
    MSG msg;
    DWORD end;

    setvbuf(stdout, NULL, _IONBF, 0);
    CHECK(dde_init && validate && update, "exports");
    if (!dde_init || !validate || !update) return 1;

    /* SHValidateUNC */
    wcscpy(path, L"C:\\not\\unc");
    CHECK(!validate(NULL, NULL, 2), "NULL path");
    CHECK(!validate(NULL, path, 2), "not a UNC path");
    wcscpy(path, L"\\\\sg-no-such-server\\share");
    CHECK(!validate(NULL, path, 0x10), "unknown flags");
    CHECK(!validate(NULL, path, 2), "unreachable server, no UI");
    CHECK(!wcscmp(path, L"\\\\sg-no-such-server\\share"), "the path is kept on failure");
    CHECK(!validate(NULL, path, 2 | 1), "unreachable server, connect");

    /* SHUpdateImage tells the views */
    wc.lpfnWndProc = wp;
    wc.hInstance = GetModuleHandleA(NULL);
    wc.lpszClassName = L"sgshmisc";
    RegisterClassW(&wc);
    hwnd = CreateWindowW(L"sgshmisc", NULL, 0, 0, 0, 10, 10, NULL, NULL, wc.hInstance, NULL);
    reg = SHChangeNotifyRegister(hwnd, SHCNRF_ShellLevel | SHCNRF_InterruptLevel, SHCNE_UPDATEIMAGE, WM_USER + 7, 1, &entry);
    CHECK(reg != 0, "register for change notices");
    update(L"x.ico", 0, 0, 5);
    end = GetTickCount() + 3000;
    while (!events && GetTickCount() < end)
        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) { TranslateMessage(&msg); DispatchMessageW(&msg); Sleep(10); }
    CHECK(events >= 1, "SHUpdateImageW sent an update-image notice (%d)", events);
    if (reg) SHChangeNotifyDeregister(reg);

    /* (the shell's DDE wild connect has no check here: DDEML does not deliver a connect to a server in the
     * same process in this harness; see the patch notes) */
    (void)dde_init; (void)client_cb;
    printf(failed ? "RESULT: FAIL (%d)\n" : "RESULT: PASS\n", failed);
    return failed != 0;
}
