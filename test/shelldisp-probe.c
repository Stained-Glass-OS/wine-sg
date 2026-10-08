/* Shell.Application's shell commands, and CascadeWindows/TileWindows
 * (patches/sg/1646).
 *
 * IShellDispatch's MinimizeAll, UndoMinimizeALL, ToggleDesktop, FileRun,
 * CascadeWindows, TileVertically/Horizontally, GetSystemInformation,
 * IsRestricted, ExplorerPolicy, GetSetting, CanStartStopService,
 * ServiceStop/Start, Windows and FindFiles were E_NOTIMPL; the taskbar
 * ignored the WM_COMMAND shell commands programs send it ("Shell command
 * 419 is not supported"), and user32's CascadeWindows and TileWindows were
 * stubs. The probe runs beside explorer's taskbar (the shell's desktop).
 */
#define COBJMACROS
#include <windows.h>
#include <shlobj.h>
#include <stdio.h>
#include <string.h>

static int failures;
static HWND wins[3];

static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

static void pump(DWORD ms)
{
    DWORD end = GetTickCount() + ms;
    MSG msg;
    do
    {
        while (PeekMessageW(&msg, 0, 0, 0, PM_REMOVE)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
        Sleep(20);
    } while ((LONG)(end - GetTickCount()) > 0);
}

/* late-bound call by name */
static HRESULT call(IDispatch *disp, const WCHAR *name, UINT argc, VARIANT *argv, VARIANT *result)
{
    DISPPARAMS params = { argv, NULL, argc, 0 };
    DISPID id;
    HRESULT hr;
    WCHAR *n = (WCHAR *)name;

    if (result) VariantInit(result);
    if (FAILED(hr = IDispatch_GetIDsOfNames(disp, &IID_NULL, &n, 1, 0, &id))) return hr;
    return IDispatch_Invoke(disp, id, &IID_NULL, 0, DISPATCH_METHOD | DISPATCH_PROPERTYGET, &params, result, NULL, NULL);
}

static VARIANT bstr(const WCHAR *s) { VARIANT v; V_VT(&v) = VT_BSTR; V_BSTR(&v) = SysAllocString(s); return v; }

static int count_iconic(void)
{
    int i, n = 0;
    for (i = 0; i < 3; i++) if (IsIconic(wins[i])) n++;
    return n;
}

static BOOL wait_iconic(int want)
{
    int i;
    for (i = 0; i < 60; i++) { pump(100); if (count_iconic() == want) return TRUE; }
    return FALSE;
}

static HWND find_window_titled(const WCHAR *title)
{
    int i;
    HWND hwnd = NULL;
    for (i = 0; i < 80 && !(hwnd = FindWindowW(NULL, title)); i++) pump(100);
    return hwnd;
}

int main(void)
{
    WNDCLASSW wc = { 0, DefWindowProcW, 0, 0, NULL, NULL, NULL, (HBRUSH)(COLOR_WINDOW + 1), NULL, L"SGDispProbe" };
    IDispatch *shell, *windows = NULL;
    VARIANT r, args[2];
    RECT rc[3], work;
    HRESULT hr;
    HWND run;
    int i;
    HKEY key;

    CoInitialize(NULL);
    wc.hInstance = GetModuleHandleW(NULL);
    RegisterClassW(&wc);
    for (i = 0; i < 3; i++)
    {
        WCHAR title[32];
        swprintf(title, 32, L"Arrange %d", i);
        wins[i] = CreateWindowW(L"SGDispProbe", title, WS_OVERLAPPEDWINDOW | WS_VISIBLE, 100 + 20 * i, 100, 300, 200,
                                NULL, NULL, wc.hInstance, NULL);
    }
    pump(1500);
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);

    /* user32 */
    i = TileWindows(NULL, MDITILE_VERTICAL, NULL, 3, wins);
    pump(500);
    for (i = 0; i < 3; i++) GetWindowRect(wins[i], &rc[i]);
    printf("tiled: %ld,%ld %ld,%ld %ld,%ld width %ld\n", rc[0].left, rc[0].top, rc[1].left, rc[1].top, rc[2].left, rc[2].top,
           rc[0].right - rc[0].left);
    check(rc[0].top == rc[1].top && rc[1].top == rc[2].top && rc[0].left < rc[1].left && rc[1].left < rc[2].left &&
          abs((rc[0].right - rc[0].left) - (work.right - work.left) / 3) <= 2, "TileWindows(MDITILE_VERTICAL): side by side, a third each");
    TileWindows(NULL, MDITILE_HORIZONTAL, NULL, 3, wins);
    pump(500);
    for (i = 0; i < 3; i++) GetWindowRect(wins[i], &rc[i]);
    check(rc[0].left == rc[1].left && rc[0].top < rc[1].top && rc[1].top < rc[2].top, "TileWindows(MDITILE_HORIZONTAL): one above the other");
    i = CascadeWindows(NULL, MDITILE_ZORDER, NULL, 3, wins);
    pump(500);
    for (i = 0; i < 3; i++) GetWindowRect(wins[i], &rc[i]);
    printf("cascaded: %ld,%ld %ld,%ld %ld,%ld\n", rc[0].left, rc[0].top, rc[1].left, rc[1].top, rc[2].left, rc[2].top);
    check(rc[1].left - rc[2].left == rc[0].left - rc[1].left && rc[0].left > rc[1].left && rc[0].top > rc[1].top,
          "CascadeWindows: each down and right of the next");

    hr = CoCreateInstance(&CLSID_Shell, NULL, CLSCTX_INPROC_SERVER, &IID_IDispatch, (void **)&shell);
    check(hr == S_OK, "Shell.Application");
    if (hr != S_OK) { printf("RESULT: FAIL\n"); return 1; }

    /* the taskbar's commands */
    hr = call(shell, L"MinimizeAll", 0, NULL, NULL);
    check(hr == S_OK && wait_iconic(3), "MinimizeAll minimizes the windows (through the taskbar)");
    hr = call(shell, L"UndoMinimizeALL", 0, NULL, NULL);
    check(hr == S_OK && wait_iconic(0), "UndoMinimizeALL brings them back");
    SetForegroundWindow(wins[0]);
    pump(300);
    hr = call(shell, L"ToggleDesktop", 0, NULL, NULL);
    check(hr == S_OK && wait_iconic(3), "ToggleDesktop shows the desktop");
    hr = call(shell, L"ToggleDesktop", 0, NULL, NULL);
    check(hr == S_OK && wait_iconic(0), "... and again puts the windows back");
    hr = call(shell, L"TileVertically", 0, NULL, NULL);
    pump(800);
    for (i = 0; i < 3; i++) GetWindowRect(wins[i], &rc[i]);
    check(hr == S_OK && rc[0].top == rc[1].top && rc[1].top == rc[2].top && rc[0].left != rc[1].left,
          "TileVertically arranges the windows");

    hr = call(shell, L"FileRun", 0, NULL, NULL);
    run = find_window_titled(L"Run");
    check(hr == S_OK && run, "FileRun shows the Run dialog");
    if (run) PostMessageW(run, WM_CLOSE, 0, 0);

    /* information */
    args[0] = bstr(L"DoubleClickTime");
    hr = call(shell, L"GetSystemInformation", 1, args, &r);
    check(hr == S_OK && V_VT(&r) == VT_I4 && V_I4(&r) == GetDoubleClickTime(), "GetSystemInformation(DoubleClickTime)");
    args[0] = bstr(L"PhysicalMemoryInstalled");
    hr = call(shell, L"GetSystemInformation", 1, args, &r);
    check(hr == S_OK && V_VT(&r) == VT_R8 && V_R8(&r) > 100e6, "GetSystemInformation(PhysicalMemoryInstalled)");
    args[0] = bstr(L"ProcessorArchitecture");
    hr = call(shell, L"GetSystemInformation", 1, args, &r);
    check(hr == S_OK && V_VT(&r) == VT_I4 && V_I4(&r) == PROCESSOR_ARCHITECTURE_AMD64, "GetSystemInformation(ProcessorArchitecture)");

    RegCreateKeyW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Policies\\Explorer", &key);
    i = 1;
    RegSetValueExW(key, L"NoRun", 0, REG_DWORD, (BYTE *)&i, sizeof(i));
    args[1] = bstr(L"Explorer");  /* reversed: the first argument last */
    args[0] = bstr(L"NoRun");
    hr = call(shell, L"IsRestricted", 2, args, &r);
    check(hr == S_OK && V_VT(&r) == VT_I4 && V_I4(&r) == 1, "IsRestricted(Explorer, NoRun)");
    args[0] = bstr(L"NoRun");
    hr = call(shell, L"ExplorerPolicy", 1, args, &r);
    check(hr == S_OK && V_VT(&r) == VT_I4 && V_I4(&r) == 1, "ExplorerPolicy(NoRun)");
    RegDeleteValueW(key, L"NoRun");
    RegCloseKey(key);

    V_VT(&args[0]) = VT_I4;
    V_I4(&args[0]) = SSF_SHOWEXTENSIONS;
    hr = call(shell, L"GetSetting", 1, args, &r);
    {
        SHELLFLAGSTATE sfs = { 0 };
        SHGetSettings(&sfs, SSF_SHOWEXTENSIONS);
        check(hr == S_OK && V_VT(&r) == VT_BOOL && !!V_BOOL(&r) == !!sfs.fShowExtensions, "GetSetting(SSF_SHOWEXTENSIONS)");
    }

    args[0] = bstr(L"Spooler");
    hr = call(shell, L"CanStartStopService", 1, args, &r);
    check(hr == S_OK && V_VT(&r) == VT_BOOL && V_BOOL(&r) == VARIANT_TRUE, "CanStartStopService(Spooler)");

    hr = call(shell, L"Windows", 0, NULL, &r);
    check(hr == S_OK && V_VT(&r) == VT_DISPATCH && V_DISPATCH(&r), "Windows(): the ShellWindows collection");
    if (hr == S_OK && V_VT(&r) == VT_DISPATCH && V_DISPATCH(&r)) windows = V_DISPATCH(&r);
    if (windows) IDispatch_Release(windows);

    hr = call(shell, L"FindFiles", 0, NULL, NULL);
    {
        HWND fe = NULL;
        for (i = 0; i < 100 && !(fe = FindWindowW(L"ExplorerWClass", NULL)); i++) pump(100);
        check(hr == S_OK && fe, "FindFiles opens File Explorer to search");
        if (fe) PostMessageW(fe, WM_CLOSE, 0, 0);
    }

    IDispatch_Release(shell);
    for (i = 0; i < 3; i++) DestroyWindow(wins[i]);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
