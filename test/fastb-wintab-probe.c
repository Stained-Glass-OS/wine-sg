/* wintab32, fast mode (patch 3546): manager handles, context enumeration, config replacement, extensions, save/restore.
   There is no tablet in the test display, so contexts cannot be opened here. */
#include <windows.h>
#include <stdio.h>

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

typedef void *HANDLE_;
static HANDLE (WINAPI *pWTMgrOpen)(HWND, UINT);
static BOOL (WINAPI *pWTMgrClose)(HANDLE);
static BOOL (WINAPI *pWTMgrContextEnum)(HANDLE, BOOL (WINAPI *)(HANDLE, LPARAM), LPARAM);
static HWND (WINAPI *pWTMgrContextOwner)(HANDLE, HANDLE);
static HANDLE (WINAPI *pWTMgrDefContext)(HANDLE, BOOL);
static BOOL (WINAPI *pWTMgrConfigReplaceExA)(HANDLE, BOOL, const char *, const char *);
static BOOL (WINAPI *pWTConfig)(HANDLE, HWND);
static BOOL (WINAPI *pWTExtGet)(HANDLE, UINT, void *);
static BOOL (WINAPI *pWTMgrExt)(HANDLE, UINT, void *);
static BOOL (WINAPI *pWTSave)(HANDLE, void *);
static HANDLE (WINAPI *pWTRestore)(HWND, void *, BOOL);

static BOOL WINAPI enum_cb(HANDLE ctx, LPARAM lparam) { (*(int *)lparam)++; return TRUE; }

int main(void)
{
    HMODULE wintab = LoadLibraryA("wintab32.dll");
    HANDLE mgr, mgr2;
    HWND hwnd;
    int n = 0;
    char buf[2048];

    if (!wintab) { printf("FAIL  wintab32.dll\nRESULT: FAIL\n"); return 1; }
#define GET(n) p##n = (void *)GetProcAddress(wintab, #n)
    GET(WTMgrOpen); GET(WTMgrClose); GET(WTMgrContextEnum); GET(WTMgrContextOwner); GET(WTMgrDefContext);
    GET(WTMgrConfigReplaceExA); GET(WTConfig); GET(WTExtGet); GET(WTMgrExt); GET(WTSave); GET(WTRestore);

    hwnd = CreateWindowA("static", "mgr", WS_POPUP, 0, 0, 10, 10, NULL, NULL, NULL, NULL);
    check(hwnd != NULL, "window");

    SetLastError(0);
    check(pWTMgrOpen((HWND)0x1234, 0x7ff0) == NULL && GetLastError() == ERROR_INVALID_WINDOW_HANDLE, "manager needs a window");
    mgr = pWTMgrOpen(hwnd, 0x7ff0);
    mgr2 = pWTMgrOpen(hwnd, 0x7ff0);
    check(mgr != NULL && mgr2 != NULL && mgr != mgr2, "two managers");
    check(pWTMgrContextEnum(mgr, enum_cb, (LPARAM)&n) && n == 0, "no contexts yet");
    check(!pWTMgrContextEnum((HANDLE)0x5, enum_cb, (LPARAM)&n), "enumeration needs a manager");
    check(pWTMgrContextOwner(mgr, (HANDLE)0xc00) == NULL, "no owner for an unknown context");
    check(!pWTMgrDefContext(mgr, FALSE) || TRUE, "default context call returns");
    check(pWTMgrClose(mgr2), "close a manager");
    check(!pWTMgrClose(mgr2), "close twice");

    /* the configuration replacement is called by WTConfig; IsChild(HWND, HWND) has the right shape */
    check(!pWTConfig((HANDLE)hwnd, hwnd), "WTConfig without replacement");
    check(!pWTMgrConfigReplaceExA((HANDLE)7, TRUE, "user32.dll", "IsChild"), "replacement needs a manager");
    check(!pWTMgrConfigReplaceExA(mgr, TRUE, "user32.dll", "NoSuchFunction"), "unknown function");
    check(pWTMgrConfigReplaceExA(mgr, TRUE, "user32.dll", "IsChild"), "install the replacement");
    check(!pWTMgrConfigReplaceExA(mgr, TRUE, "user32.dll", "IsChild"), "only one replacement");
    {
        HWND child = CreateWindowA("static", "child", WS_CHILD, 0, 0, 5, 5, hwnd, NULL, NULL, NULL);
        check(pWTConfig((HANDLE)hwnd, child) == TRUE, "WTConfig calls the replacement");
        check(pWTConfig((HANDLE)child, hwnd) == FALSE, "and returns its result");
        DestroyWindow(child);
    }
    check(pWTMgrConfigReplaceExA(mgr, FALSE, NULL, NULL), "remove the replacement");
    check(!pWTConfig((HANDLE)hwnd, hwnd), "WTConfig without it again");
    check(!pWTMgrConfigReplaceExA(mgr, FALSE, NULL, NULL), "nothing to remove");

    /* no extensions, no contexts */
    check(!pWTExtGet((HANDLE)0xc00, 1, buf) && !pWTMgrExt(mgr, 1, buf), "extensions are not supported");
    check(!pWTSave((HANDLE)0xc00, buf), "WTSave of an unknown context");
    memset(buf, 0, sizeof(buf));
    check(pWTRestore(hwnd, buf, FALSE) == NULL, "WTRestore of garbage");
    check(pWTRestore(hwnd, NULL, FALSE) == NULL, "WTRestore without data");

    check(pWTMgrClose(mgr), "close the manager");
    DestroyWindow(hwnd);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures ? 1 : 0;
}
