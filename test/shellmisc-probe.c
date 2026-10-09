/* Shell and user odds and ends (patches/sg/1686), run by
 * test/shellmisc-gate.sh on Xvfb: SetWindowThemeAttribute(WTA_NONCLIENT)
 * hides the caption text and icon and the system menu; RegisterGPNotification
 * sets its event when the Policies key changes or RefreshPolicy is called;
 * EnterCriticalPolicySection holds others off until Leave;
 * SHQueryUserNotificationState sees a full-screen window and notifications
 * turned off; CancelDC checks its DC. These were stubs. */
#include <windows.h>
#include <uxtheme.h>
#include <userenv.h>
#include <shellapi.h>
#include <stdio.h>

#ifndef WTNCA_NODRAWCAPTION
typedef struct { DWORD dwFlags; DWORD dwMask; } WTA_OPTIONS;
#define WTNCA_NODRAWCAPTION 1
#define WTNCA_NODRAWICON    2
#define WTNCA_NOSYSMENU     4
#endif

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

static HRESULT (WINAPI *pSetWindowThemeAttribute)(HWND, int, void *, DWORD);
static BOOL (WINAPI *pRefreshPolicy)(BOOL);
static BOOL (WINAPI *pRegisterGPNotification)(HANDLE, BOOL);
static BOOL (WINAPI *pUnregisterGPNotification)(HANDLE);
static HANDLE (WINAPI *pEnterCriticalPolicySection)(BOOL);
static BOOL (WINAPI *pLeaveCriticalPolicySection)(HANDLE);
static HRESULT (WINAPI *pSHQueryUserNotificationState)(int *);

static void pump(int ms)
{
    DWORD end = GetTickCount() + ms;
    MSG msg;
    while ((int)(end - GetTickCount()) > 0)
    {
        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) DispatchMessageW(&msg);
        Sleep(10);
    }
}

/* a hash of the caption strip's pixels, as the frame draws them */
static DWORD caption_hash(HWND hwnd)
{
    HDC wdc, mdc;
    HBITMAP bmp, old;
    RECT rc;
    int x, y, w, h;
    DWORD hash = 0;

    RedrawWindow(hwnd, NULL, NULL, RDW_FRAME | RDW_INVALIDATE | RDW_UPDATENOW);
    pump(100);
    GetWindowRect(hwnd, &rc);
    w = rc.right - rc.left;
    h = GetSystemMetrics(SM_CYCAPTION) + GetSystemMetrics(SM_CYFRAME);
    wdc = GetWindowDC(hwnd);
    mdc = CreateCompatibleDC(wdc);
    bmp = CreateCompatibleBitmap(wdc, w, h);
    old = SelectObject(mdc, bmp);
    BitBlt(mdc, 0, 0, w, h, wdc, 0, 0, SRCCOPY);
    for (y = 0; y < h; y++)
        for (x = 0; x < w / 2; x++)
            hash = hash * 31 + GetPixel(mdc, x, y);
    SelectObject(mdc, old);
    DeleteObject(bmp);
    DeleteDC(mdc);
    ReleaseDC(hwnd, wdc);
    return hash;
}

static HANDLE section_entered;
static DWORD WINAPI enter_section(void *arg)
{
    HANDLE h = pEnterCriticalPolicySection(FALSE);
    SetEvent(section_entered);
    if (h) pLeaveCriticalPolicySection(h);
    return 0;
}

int main(void)
{
    HMODULE ux = LoadLibraryW(L"uxtheme.dll"), ue = LoadLibraryW(L"userenv.dll"), sh = LoadLibraryW(L"shell32.dll");
    WTA_OPTIONS opts;
    HWND hwnd, full;
    HANDLE event, section, thread;
    HKEY key;
    DWORD plain, hidden, zero = 0;
    LRESULT hit;
    RECT rc;
    int state = -1;

    pSetWindowThemeAttribute = (void *)GetProcAddress(ux, "SetWindowThemeAttribute");
    pRefreshPolicy = (void *)GetProcAddress(ue, "RefreshPolicy");
    pRegisterGPNotification = (void *)GetProcAddress(ue, "RegisterGPNotification");
    pUnregisterGPNotification = (void *)GetProcAddress(ue, "UnregisterGPNotification");
    pEnterCriticalPolicySection = (void *)GetProcAddress(ue, "EnterCriticalPolicySection");
    pLeaveCriticalPolicySection = (void *)GetProcAddress(ue, "LeaveCriticalPolicySection");
    pSHQueryUserNotificationState = (void *)GetProcAddress(sh, "SHQueryUserNotificationState");

    {
        WNDCLASSW wc = { 0 };
        wc.lpfnWndProc = DefWindowProcW;
        wc.lpszClassName = L"SgShellMisc";
        wc.hIcon = LoadIconW(NULL, (LPCWSTR)IDI_APPLICATION);
        wc.hbrBackground = GetStockObject(WHITE_BRUSH);
        RegisterClassW(&wc);
    }
    hwnd = CreateWindowW(L"SgShellMisc", L"WWWWWWWW Stained Glass caption WWWWWWWW", WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                         100, 100, 500, 200, NULL, NULL, NULL, NULL);
    pump(300);

    /* SetWindowThemeAttribute */
    opts.dwFlags = opts.dwMask = WTNCA_NODRAWCAPTION;
    check(pSetWindowThemeAttribute(hwnd, 2, &opts, sizeof(opts)) == E_INVALIDARG, "an unknown attribute type: E_INVALIDARG");
    check(pSetWindowThemeAttribute(hwnd, 1, &opts, 4) == E_INVALIDARG, "a short WTA_OPTIONS: E_INVALIDARG");
    GetWindowRect(hwnd, &rc);
    hit = SendMessageW(hwnd, WM_NCHITTEST, 0,
                       MAKELPARAM(rc.left + GetSystemMetrics(SM_CXFRAME) + 6, rc.top + GetSystemMetrics(SM_CYFRAME) + 6));
    if (hit != HTSYSMENU) printf("      hit %Id\n", hit);
    check(hit == HTSYSMENU, "before: the icon is the system menu");
    plain = caption_hash(hwnd);
    opts.dwFlags = opts.dwMask = WTNCA_NODRAWCAPTION | WTNCA_NODRAWICON | WTNCA_NOSYSMENU;
    check(pSetWindowThemeAttribute(hwnd, 1, &opts, sizeof(opts)) == S_OK, "WTA_NONCLIENT: no caption, no icon, no system menu");
    hidden = caption_hash(hwnd);
    if (hidden == plain) printf("      hash %08lx\n", plain);
    check(hidden != plain, "the title bar is drawn without its text and icon");
    hit = SendMessageW(hwnd, WM_NCHITTEST, 0,
                       MAKELPARAM(rc.left + GetSystemMetrics(SM_CXFRAME) + 6, rc.top + GetSystemMetrics(SM_CYFRAME) + 6));
    check(hit == HTCAPTION, "and where the icon was is caption: no system menu");
    opts.dwFlags = 0;
    check(pSetWindowThemeAttribute(hwnd, 1, &opts, sizeof(opts)) == S_OK && caption_hash(hwnd) == plain,
          "flags cleared through the mask: drawn as before");

    /* Group Policy notification */
    event = CreateEventW(NULL, TRUE, FALSE, NULL);
    check(!pRegisterGPNotification(NULL, FALSE), "RegisterGPNotification(NULL): FALSE");
    check(pRegisterGPNotification(event, FALSE), "RegisterGPNotification (user)");
    RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\Policies\\SgProbe", 0, NULL, 0, KEY_ALL_ACCESS, NULL, &key, NULL);
    RegSetValueExW(key, L"Setting", 0, REG_DWORD, (BYTE *)&zero, sizeof(zero));
    RegCloseKey(key);
    check(WaitForSingleObject(event, 5000) == WAIT_OBJECT_0, "a change under HKCU\\Software\\Policies sets the event");
    ResetEvent(event);
    check(pRefreshPolicy(FALSE), "RefreshPolicy(FALSE)");
    check(WaitForSingleObject(event, 5000) == WAIT_OBJECT_0, "and the refresh sets it");
    check(pUnregisterGPNotification(event), "UnregisterGPNotification");
    check(!pUnregisterGPNotification(event), "again: FALSE");
    RegDeleteTreeW(HKEY_CURRENT_USER, L"Software\\Policies\\SgProbe");

    /* the policy section */
    section = pEnterCriticalPolicySection(FALSE);
    check(section != NULL, "EnterCriticalPolicySection");
    section_entered = CreateEventW(NULL, TRUE, FALSE, NULL);
    thread = CreateThread(NULL, 0, enter_section, NULL, 0, NULL);
    check(WaitForSingleObject(section_entered, 500) == WAIT_TIMEOUT, "another thread waits for it");
    check(pLeaveCriticalPolicySection(section), "LeaveCriticalPolicySection");
    check(WaitForSingleObject(section_entered, 5000) == WAIT_OBJECT_0, "then it enters");
    WaitForSingleObject(thread, 5000);

    /* SHQueryUserNotificationState */
    SetForegroundWindow(hwnd);
    pump(200);
    check(pSHQueryUserNotificationState(NULL) == E_INVALIDARG, "SHQueryUserNotificationState(NULL): E_INVALIDARG");
    check(pSHQueryUserNotificationState(&state) == S_OK && state == 5 /* QUNS_ACCEPTS_NOTIFICATIONS */,
          "a window in front: QUNS_ACCEPTS_NOTIFICATIONS");
    full = CreateWindowExW(WS_EX_TOPMOST, L"STATIC", L"full", WS_POPUP | WS_VISIBLE, 0, 0,
                           GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN), NULL, NULL, NULL, NULL);
    SetForegroundWindow(full);
    pump(300);
    state = -1;
    pSHQueryUserNotificationState(&state);
    check(GetForegroundWindow() == full && state == 2 /* QUNS_BUSY */, "a full-screen window in front: QUNS_BUSY");
    DestroyWindow(full);
    SetForegroundWindow(hwnd);
    pump(200);
    RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Notifications\\Settings", 0,
                    NULL, 0, KEY_ALL_ACCESS, NULL, &key, NULL);
    RegSetValueExW(key, L"NOC_GLOBAL_SETTING_TOASTS_ENABLED", 0, REG_DWORD, (BYTE *)&zero, sizeof(zero));
    state = -1;
    pSHQueryUserNotificationState(&state);
    check(state == 6 /* QUNS_QUIET_TIME */, "notifications turned off: QUNS_QUIET_TIME");
    RegDeleteValueW(key, L"NOC_GLOBAL_SETTING_TOASTS_ENABLED");
    RegCloseKey(key);

    /* CancelDC */
    SetLastError(0xdeadbeef);
    check(!CancelDC(NULL) && GetLastError() == ERROR_INVALID_HANDLE, "CancelDC(NULL): ERROR_INVALID_HANDLE");
    check(!CancelDC((HDC)0xdead0), "CancelDC(a bad handle): FALSE");
    {
        HDC dc = GetDC(hwnd);
        check(CancelDC(dc), "CancelDC(a window's DC): TRUE");
        ReleaseDC(hwnd, dc);
    }

    DestroyWindow(hwnd);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
