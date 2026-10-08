/* A window's property store and AppUserModelIDs group taskbar buttons
 * (patches/sg/1640).
 *
 * SHGetPropertyStoreForWindow returned a store that kept nothing (GetValue
 * E_NOTIMPL, SetValue S_OK doing nothing), so programs that give a window an
 * AppUserModelID (Chromium's app windows, Java and Python programs sharing
 * one executable, Electron) had it ignored, and the taskbar grouped buttons
 * by executable alone. The probe sets and reads properties (also from
 * another process) and counts the buttons the taskbar shows for its
 * windows: one program's two windows with different IDs are two buttons,
 * with the same ID one, and another process with that ID as its explicit
 * one (SetCurrentProcessExplicitAppUserModelID) joins them.
 */
#define COBJMACROS
#include <windows.h>
#include <shobjidl.h>
#include <propkey.h>
#include <propvarutil.h>
#include <shlobj.h>
#include <stdio.h>
#include <string.h>

static int failures;
static HWND found;
static int found_count;
static HWND targets[3];

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
        while (PeekMessageA(&msg, 0, 0, 0, PM_REMOVE)) { TranslateMessage(&msg); DispatchMessageA(&msg); }
        Sleep(20);
    } while ((LONG)(end - GetTickCount()) > 0);
}

static BOOL CALLBACK count_buttons(HWND child, LPARAM lp)
{
    HWND id = (HWND)GetWindowLongPtrA(child, GWLP_ID);
    int i;
    if (!IsWindowVisible(child)) return TRUE;
    for (i = 0; i < 3; i++) if (targets[i] && id == targets[i]) found_count++;
    return TRUE;
}

static int buttons(void)
{
    HWND tray = FindWindowA("Shell_TrayWnd", NULL);
    found_count = 0;
    EnumChildWindows(tray, count_buttons, 0);
    return found_count;
}

/* waits for the taskbar to show this many buttons for the windows */
static int wait_buttons(int want)
{
    int i, n = -1;
    for (i = 0; i < 40; i++)
    {
        pump(150);
        if ((n = buttons()) == want) break;
    }
    return n;
}

static HRESULT set_id(HWND hwnd, const WCHAR *id)
{
    IPropertyStore *store;
    PROPVARIANT var;
    HRESULT hr;

    if ((hr = SHGetPropertyStoreForWindow(hwnd, &IID_IPropertyStore, (void **)&store)) != S_OK) return hr;
    PropVariantInit(&var);
    if (id)
    {
        var.vt = VT_LPWSTR;
        var.pwszVal = (WCHAR *)id;
    }
    hr = IPropertyStore_SetValue(store, &PKEY_AppUserModel_ID, &var);
    if (hr == S_OK) hr = IPropertyStore_Commit(store);
    IPropertyStore_Release(store);
    return hr;
}

static HWND make_window(const char *title, int x)
{
    return CreateWindowA("SGAppIdProbe", title, WS_OVERLAPPEDWINDOW | WS_VISIBLE, x, 100, 260, 160,
                         NULL, NULL, GetModuleHandleA(NULL), NULL);
}

static void register_class(void)
{
    WNDCLASSA wc = { 0, DefWindowProcA, 0, 0, NULL, NULL, NULL, (HBRUSH)(COLOR_WINDOW + 1), NULL, "SGAppIdProbe" };
    wc.hInstance = GetModuleHandleA(NULL);
    RegisterClassA(&wc);
}

int main(int argc, char **argv)
{
    HMODULE shell32 = LoadLibraryA("shell32.dll");
    HRESULT (WINAPI *pSetCurrentProcessExplicitAppUserModelID)(const WCHAR *) =
        (void *)GetProcAddress(shell32, "SetCurrentProcessExplicitAppUserModelID");
    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    IPropertyStore *store;
    PROPERTYKEY key;
    PROPVARIANT var;
    HWND a, b, c = NULL;
    char cmd[MAX_PATH + 64], out[256];
    DWORD count;
    HRESULT hr;
    int n;

    CoInitialize(NULL);
    register_class();

    if (argc >= 3 && !strcmp(argv[1], "read"))
    {
        /* another process reads the window's ID */
        HWND hwnd = (HWND)(ULONG_PTR)strtoull(argv[2], NULL, 16);
        hr = SHGetPropertyStoreForWindow(hwnd, &IID_IPropertyStore, (void **)&store);
        if (hr != S_OK) { printf("read: hr %#lx\n", hr); return 1; }
        hr = IPropertyStore_GetValue(store, &PKEY_AppUserModel_ID, &var);
        printf("read: hr %#lx vt %u id %ls\n", hr, var.vt, var.vt == VT_LPWSTR ? var.pwszVal : L"");
        PropVariantClear(&var);
        IPropertyStore_Release(store);
        return 0;
    }
    if (argc >= 3 && !strcmp(argv[1], "window"))
    {
        /* another process whose explicit ID is argv[2] */
        WCHAR id[64];
        MultiByteToWideChar(CP_ACP, 0, argv[2], -1, id, 64);
        pSetCurrentProcessExplicitAppUserModelID(id);
        make_window("AppId Three", 650);
        pump(30000);
        return 0;
    }

    a = make_window("AppId One", 50);
    b = make_window("AppId Two", 350);
    targets[0] = a;
    targets[1] = b;

    n = wait_buttons(1);
    printf("no IDs: %d button(s)\n", n);
    check(n == 1, "one program's two windows: one combined button");

    hr = set_id(a, L"SG.Test.One");
    check(hr == S_OK, "SetValue(System.AppUserModel.ID) succeeds");
    n = wait_buttons(2);
    printf("A has an ID: %d button(s)\n", n);
    check(n == 2, "a window given its own ID gets a button of its own");

    /* read back here, and from another process */
    hr = SHGetPropertyStoreForWindow(a, &IID_IPropertyStore, (void **)&store);
    PropVariantInit(&var);
    hr = IPropertyStore_GetValue(store, &PKEY_AppUserModel_ID, &var);
    check(hr == S_OK && var.vt == VT_LPWSTR && !wcscmp(var.pwszVal, L"SG.Test.One"), "GetValue reads the ID back");
    PropVariantClear(&var);
    count = 0;
    IPropertyStore_GetCount(store, &count);
    memset(&key, 0, sizeof(key));
    hr = IPropertyStore_GetAt(store, 0, &key);
    check(count == 1 && hr == S_OK && IsEqualPropertyKey(key, PKEY_AppUserModel_ID), "GetCount and GetAt list it");
    var.vt = VT_BOOL;
    var.boolVal = VARIANT_TRUE;
    hr = IPropertyStore_SetValue(store, &PKEY_AppUserModel_PreventPinning, &var);
    PropVariantInit(&var);
    IPropertyStore_GetValue(store, &PKEY_AppUserModel_PreventPinning, &var);
    check(hr == S_OK && var.vt == VT_BOOL && var.boolVal == VARIANT_TRUE, "a VT_BOOL property is kept too");
    IPropertyStore_Release(store);

    sprintf(cmd, "\"%s\" read %p", argv[0], a);
    {
        SECURITY_ATTRIBUTES sa = { sizeof(sa), NULL, TRUE };
        HANDLE rd, wr;
        DWORD got = 0;
        CreatePipe(&rd, &wr, &sa, 0);
        si.dwFlags = STARTF_USESTDHANDLES;
        si.hStdOutput = wr;
        si.hStdError = wr;
        CreateProcessA(NULL, cmd, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi);
        CloseHandle(wr);
        WaitForSingleObject(pi.hProcess, 20000);
        memset(out, 0, sizeof(out));
        ReadFile(rd, out, sizeof(out) - 1, &got, NULL);
        CloseHandle(rd);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        printf("%s", out);
        check(strstr(out, "id SG.Test.One") != NULL, "another process reads the window's ID");
    }

    hr = set_id(b, L"SG.Test.One");
    n = wait_buttons(1);
    printf("both One: %d button(s)\n", n);
    check(hr == S_OK && n == 1, "two windows with the same ID: one combined button");

    hr = set_id(b, L"SG.Test.Two");
    n = wait_buttons(2);
    check(hr == S_OK && n == 2, "different IDs: two buttons");

    /* another process, whose explicit ID is One, joins A's group */
    si.dwFlags = 0;
    sprintf(cmd, "\"%s\" window SG.Test.One", argv[0]);
    CreateProcessA(NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi);
    for (n = 0; n < 100 && !(c = FindWindowA("SGAppIdProbe", "AppId Three")); n++) pump(100);
    targets[2] = c;
    n = wait_buttons(2);
    printf("third process with explicit ID One: %d button(s)\n", n);
    check(c && n == 2, "another process with the same explicit ID shares the group");
    TerminateProcess(pi.hProcess, 0);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    targets[2] = NULL;

    hr = set_id(a, NULL);
    hr = set_id(b, NULL);
    n = wait_buttons(1);
    check(hr == S_OK && n == 1, "VT_EMPTY takes the IDs away: one program's button again");
    hr = SHGetPropertyStoreForWindow(a, &IID_IPropertyStore, (void **)&store);
    PropVariantInit(&var);
    IPropertyStore_GetValue(store, &PKEY_AppUserModel_ID, &var);
    check(var.vt == VT_EMPTY, "... and GetValue gives VT_EMPTY");
    var.vt = VT_LPWSTR;
    {
        static WCHAR long_id[140];
        int k;
        for (k = 0; k < 129; k++) long_id[k] = 'a' + k % 26;
        var.pwszVal = long_id;
    }
    check(IPropertyStore_SetValue(store, &PKEY_AppUserModel_ID, &var) == E_INVALIDARG, "an ID over 128 characters is refused");
    IPropertyStore_Release(store);

    store = (void *)0xdeadbeef;
    hr = SHGetPropertyStoreForWindow((HWND)0xdeadbeef, &IID_IPropertyStore, (void **)&store);
    check(hr == E_INVALIDARG && !store, "no store for a window that is not there");

    DestroyWindow(a);
    DestroyWindow(b);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
