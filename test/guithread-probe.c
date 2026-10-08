/* Small stubs that answered wrongly (patches/sg/1622), under Xvfb:
 *
 *  - IsGUIThread said TRUE for every thread: a thread with no message
 *    queue is not a GUI thread until it is converted (TRUE), after which
 *    it is;
 *  - uxtheme SetPreferredAppMode (135) kept nothing and gave 0;
 *    IsDarkModeAllowedForApp (139) was missing; AllowDarkModeForWindow
 *    (133) / IsDarkModeAllowedForWindow (137) kept nothing;
 *  - SHAppBarMessage ABM_SETAUTOHIDEBAR(EX) registered nothing and
 *    ABM_GETAUTOHIDEBAR found no program's bar;
 *  - AppPolicyGet* answered for any handle, even a bad one;
 *  - GetProfileType took a NULL pointer; NetGetAadJoinInformation failed
 *    with ERROR_CALL_NOT_IMPLEMENTED instead of "joined to nothing".
 */
#include <windows.h>
#include <shellapi.h>
#include <stdio.h>

#ifndef ABM_SETAUTOHIDEBAREX
#define ABM_GETAUTOHIDEBAREX 0x0b
#define ABM_SETAUTOHIDEBAREX 0x0c
#endif

static int failures;

static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) failures++;
}

static BOOL (WINAPI *pIsGUIThread)(BOOL);
static BOOL gui_before, gui_convert, gui_after;

static DWORD WINAPI fresh_thread(void *arg)
{
    gui_before = pIsGUIThread(FALSE);
    gui_convert = pIsGUIThread(TRUE);
    gui_after = pIsGUIThread(FALSE);
    return 0;
}

static HWND make_window(void)
{
    return CreateWindowExA(WS_EX_TOOLWINDOW, "static", "probe", WS_POPUP, 0, 0, 50, 50, NULL, NULL, NULL, NULL);
}

static UINT_PTR appbar(DWORD msg, HWND hwnd, UINT edge, LPARAM lparam)
{
    APPBARDATA abd;

    memset(&abd, 0, sizeof(abd));
    abd.cbSize = sizeof(abd);
    abd.hWnd = hwnd;
    abd.uEdge = edge;
    abd.uCallbackMessage = WM_USER + 1;
    abd.lParam = lparam;
    SetRect(&abd.rc, 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN));
    return SHAppBarMessage(msg, &abd);
}

int main(void)
{
    HMODULE ux = LoadLibraryA("uxtheme.dll"), kb = GetModuleHandleA("kernelbase.dll");
    HMODULE ue = LoadLibraryA("userenv.dll"), na = LoadLibraryA("netapi32.dll");
    int (WINAPI *pSetPreferredAppMode)(int) = (void *)GetProcAddress(ux, MAKEINTRESOURCEA(135));
    BOOLEAN (WINAPI *pIsDarkModeAllowedForApp)(void) = (void *)GetProcAddress(ux, MAKEINTRESOURCEA(139));
    BOOLEAN (WINAPI *pAllowDarkModeForWindow)(HWND, BOOLEAN) = (void *)GetProcAddress(ux, MAKEINTRESOURCEA(133));
    BOOLEAN (WINAPI *pIsDarkModeAllowedForWindow)(HWND) = (void *)GetProcAddress(ux, MAKEINTRESOURCEA(137));
    BOOLEAN (WINAPI *pShouldAppsUseDarkMode)(void) = (void *)GetProcAddress(ux, MAKEINTRESOURCEA(132));
    LONG (WINAPI *pAppPolicyGetWindowingModel)(HANDLE, int *) = (void *)GetProcAddress(kb, "AppPolicyGetWindowingModel");
    LONG (WINAPI *pAppPolicyGetThreadInitializationType)(HANDLE, int *) = (void *)GetProcAddress(kb, "AppPolicyGetThreadInitializationType");
    LONG (WINAPI *pAppPolicyGetProcessTerminationMethod)(HANDLE, int *) = (void *)GetProcAddress(kb, "AppPolicyGetProcessTerminationMethod");
    BOOL (WINAPI *pGetProfileType)(DWORD *) = (void *)GetProcAddress(ue, "GetProfileType");
    HRESULT (WINAPI *pNetGetAadJoinInformation)(const WCHAR *, void **) = (void *)GetProcAddress(na, "NetGetAadJoinInformation");
    HANDLE thread, effective = (HANDLE)~(ULONG_PTR)5;
    HWND w1, w2;
    UINT_PTR r;
    DWORD flags;
    void *info;
    int mode, i;
    LONG err;

    pIsGUIThread = (void *)GetProcAddress(GetModuleHandleA("user32.dll"), "IsGUIThread");

    /* IsGUIThread, on a thread that has done nothing with windows */
    thread = CreateThread(NULL, 0, fresh_thread, NULL, 0, NULL);
    WaitForSingleObject(thread, 10000);
    CloseHandle(thread);
    printf("fresh thread: before %d convert %d after %d\n", gui_before, gui_convert, gui_after);
    check(!gui_before, "IsGUIThread(FALSE): a thread with no queue is not a GUI thread");
    check(gui_convert && gui_after, "IsGUIThread(TRUE) converts it, and then it is one");

    /* uxtheme app and window dark mode */
    check(pSetPreferredAppMode && pIsDarkModeAllowedForApp && pAllowDarkModeForWindow && pIsDarkModeAllowedForWindow,
          "uxtheme exports ordinals 133, 135, 137 and 139");
    if (pSetPreferredAppMode && pIsDarkModeAllowedForApp)
    {
        mode = pSetPreferredAppMode(2);
        check(mode == 0, "SetPreferredAppMode gives the old mode (default first)");
        check(pIsDarkModeAllowedForApp(), "... force dark: dark mode is allowed for the app");
        mode = pSetPreferredAppMode(3);
        check(mode == 2, "... and then the mode that was set");
        check(!pIsDarkModeAllowedForApp(), "... force light: it is not");
        pSetPreferredAppMode(1);
        check(pIsDarkModeAllowedForApp() == (pShouldAppsUseDarkMode() != 0), "... allow dark: as the user chose");
    }

    w1 = make_window();
    w2 = make_window();
    check(w1 && w2, "windows made");
    if (pAllowDarkModeForWindow && pIsDarkModeAllowedForWindow)
    {
        check(!pIsDarkModeAllowedForWindow(w1), "a new window is not allowed dark mode");
        check(pAllowDarkModeForWindow(w1, TRUE), "AllowDarkModeForWindow(TRUE)");
        check(pIsDarkModeAllowedForWindow(w1), "... and IsDarkModeAllowedForWindow says so");
        check(!pIsDarkModeAllowedForWindow(w2), "... for that window only");
        pAllowDarkModeForWindow(w1, FALSE);
        check(!pIsDarkModeAllowedForWindow(w1), "AllowDarkModeForWindow(FALSE) takes it back");
        check(!pAllowDarkModeForWindow((HWND)0xdead, TRUE), "a bad window is refused");
    }

    /* appbars that hide themselves */
    for (i = 0; i < 40 && !FindWindowA("WineAppBar", NULL); i++) Sleep(250);
    check(appbar(ABM_NEW, w1, 0, 0) && appbar(ABM_NEW, w2, 0, 0), "ABM_NEW");
    r = appbar(ABM_SETAUTOHIDEBAR, w1, ABE_LEFT, TRUE);
    check(r == TRUE, "ABM_SETAUTOHIDEBAR registers an auto-hide bar on the left");
    r = appbar(ABM_GETAUTOHIDEBAR, NULL, ABE_LEFT, 0);
    printf("auto-hide bar on the left: %p (w1 %p)\n", (void *)r, w1);
    check(r == (UINT_PTR)w1, "ABM_GETAUTOHIDEBAR finds it");
    check(appbar(ABM_GETAUTOHIDEBAREX, NULL, ABE_LEFT, 0) == (UINT_PTR)w1, "... and ABM_GETAUTOHIDEBAREX");
    check(!appbar(ABM_SETAUTOHIDEBAR, w2, ABE_LEFT, TRUE), "a second bar on that edge is refused");
    check(appbar(ABM_SETAUTOHIDEBAREX, w2, ABE_TOP, TRUE) == TRUE, "ABM_SETAUTOHIDEBAREX on another edge");
    check(appbar(ABM_GETAUTOHIDEBAR, NULL, ABE_TOP, 0) == (UINT_PTR)w2, "... is found there");
    check(!appbar(ABM_SETAUTOHIDEBAR, w2, ABE_LEFT, FALSE), "unregistering another program's bar fails");
    check(appbar(ABM_SETAUTOHIDEBAR, w1, ABE_LEFT, FALSE) == TRUE, "unregistering its own succeeds");
    check(appbar(ABM_GETAUTOHIDEBAR, NULL, ABE_LEFT, 0) == 0, "... and the edge has none");
    appbar(ABM_REMOVE, w2, 0, 0);
    check(appbar(ABM_GETAUTOHIDEBAR, NULL, ABE_TOP, 0) == 0, "ABM_REMOVE unregisters a bar's auto-hide");
    appbar(ABM_REMOVE, w1, 0, 0);
    DestroyWindow(w1);
    DestroyWindow(w2);

    /* app policies: a desktop program's answers, for real tokens only */
    if (pAppPolicyGetWindowingModel && pAppPolicyGetThreadInitializationType && pAppPolicyGetProcessTerminationMethod)
    {
        HANDLE token;

        mode = -1;
        err = pAppPolicyGetWindowingModel(effective, &mode);
        check(!err && mode == 2, "AppPolicyGetWindowingModel: ClassicDesktop (effective token pseudo-handle)");
        mode = -1;
        err = pAppPolicyGetThreadInitializationType(effective, &mode);
        check(!err && mode == 0, "AppPolicyGetThreadInitializationType: None");
        OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token);
        mode = -1;
        err = pAppPolicyGetProcessTerminationMethod(token, &mode);
        check(!err && mode == 0, "AppPolicyGetProcessTerminationMethod: ExitProcess (a process token)");
        CloseHandle(token);
        mode = -1;
        err = pAppPolicyGetWindowingModel((HANDLE)0x1234, &mode);
        printf("bad token: err %ld mode %d\n", err, mode);
        check(err != 0 && mode == -1, "a bad token handle is an error");
    }
    else check(0, "AppPolicy exports");

    /* profile type, Entra join */
    check(pGetProfileType && pGetProfileType(&flags) && flags == 0, "GetProfileType: a local profile");
    SetLastError(0xdeadbeef);
    check(pGetProfileType && !pGetProfileType(NULL) && GetLastError() == ERROR_INVALID_PARAMETER,
          "GetProfileType(NULL) fails with ERROR_INVALID_PARAMETER");
    info = (void *)0xdeadbeef;
    if (pNetGetAadJoinInformation)
    {
        HRESULT hr = pNetGetAadJoinInformation(NULL, &info);
        printf("NetGetAadJoinInformation: %#lx %p\n", hr, info);
        check(hr == S_OK && !info, "NetGetAadJoinInformation: S_OK, joined to nothing");
    }
    else check(0, "NetGetAadJoinInformation export");

    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
