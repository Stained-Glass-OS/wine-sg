/* Icon overlay handlers (patch 6245): the registered handlers in sorted order, at most 15; an item's
 * overlay from the first handler (by priority) that says it is a member; SHGetIconOverlayIndex; the
 * file view's items carry the overlay.  6248: a handler that cannot be made takes no slot, and COM
 * gives the loader's error for a server it cannot load (missing; 32-bit in a 64-bit process). */
#define COBJMACROS
#include <windows.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <commctrl.h>
#include <stdio.h>

static const GUID IID_Overlay = { 0x7d688a70, 0xc613, 0x11d0, { 0x99, 0x9b, 0x00, 0xc0, 0x4f, 0xd6, 0x55, 0xe1 } };
typedef struct { void **vtbl; } obj;
#define SLOT(o, n) (((obj *)(o))->vtbl[n])
typedef HRESULT (WINAPI *ov_fn)(void *, LPCITEMIDLIST, int *);

static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static LPITEMIDLIST child_of(IShellFolder *folder, const WCHAR *name)
{
    LPITEMIDLIST pidl = NULL;
    IShellFolder_ParseDisplayName(folder, NULL, NULL, (WCHAR *)name, NULL, &pidl, NULL);
    return pidl;
}

static HWND find_list(HWND parent)
{
    HWND child = NULL, found;
    WCHAR cls[64];
    while ((child = FindWindowExW(parent, child, NULL, NULL)))
    {
        GetClassNameW(child, cls, 64);
        if (!lstrcmpW(cls, L"SysListView32")) return child;
        if ((found = find_list(child))) return found;
    }
    return NULL;
}

static void pump(int ms)
{
    DWORD end = GetTickCount() + ms;
    MSG msg;
    while ((int)(end - GetTickCount()) > 0)
    {
        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
        Sleep(10);
    }
}

int main(void)
{
    IShellFolder *desktop, *folder = NULL;
    LPITEMIDLIST dir, a, b, c, d;
    WCHAR shell32[MAX_PATH];
    void *ov = NULL;
    int index, image, icon, i;
    HRESULT hr;
    const WCHAR *(*last_path)(void);
    HMODULE dll;

    setvbuf(stdout, NULL, _IONBF, 0);
    CoInitialize(NULL);
    SHGetDesktopFolder(&desktop);
    SHParseDisplayName(L"C:\\ovtest", NULL, &dir, 0, NULL);
    IShellFolder_BindToObject(desktop, dir, NULL, &IID_IShellFolder, (void **)&folder);
    if (!folder) { printf("no folder\nRESULT: FAIL\n"); return 1; }
    a = child_of(folder, L"x.ova"); b = child_of(folder, L"y.ovb"); c = child_of(folder, L"z.txt");
    d = child_of(folder, L"w.ovc");
    IShellFolder_QueryInterface(folder, &IID_Overlay, &ov);
    CHECK(ov != NULL, "the folder answers IShellIconOverlay");
    if (!ov) { printf("RESULT: FAIL\n"); return 1; }

    index = -7;
    hr = ((ov_fn)SLOT(ov, 3))(ov, a, &index);
    CHECK(hr == S_OK && index == 1, "a member of the first handler (four spaces sort first): %#lx %d", hr, index);
    index = -7;
    hr = ((ov_fn)SLOT(ov, 3))(ov, b, &index);
    CHECK(hr == S_OK && index == 15, "the handler after one that failed has the 15th slot: %#lx %d", hr, index);
    index = -7;
    hr = ((ov_fn)SLOT(ov, 3))(ov, d, &index);
    CHECK(hr == S_FALSE, "the 16th handler is not loaded: %#lx %d", hr, index);
    hr = ((ov_fn)SLOT(ov, 3))(ov, c, &index);
    CHECK(hr == S_FALSE, "no handler marks it: %#lx", hr);

    dll = GetModuleHandleW(L"overlayid.dll");
    last_path = dll ? (void *)GetProcAddress(dll, "last_path") : NULL;
    CHECK(last_path && !lstrcmpiW(last_path(), L"C:\\ovtest\\z.txt"), "IsMemberOf gets the file's path: %ls",
          last_path ? last_path() : L"(not loaded)");

    GetSystemDirectoryW(shell32, MAX_PATH);
    lstrcatW(shell32, L"\\shell32.dll");
    image = SHGetIconOverlayIndexW(shell32, 1);
    CHECK(image >= 0, "SHGetIconOverlayIndex of a loaded handler's picture: %d", image);
    CHECK(SHGetIconOverlayIndexW(shell32, 4) == -1, "a picture of no loaded handler: %d", SHGetIconOverlayIndexW(shell32, 4));
    {
        static const GUID missing = { 0x7b1d0e11, 0x2b6d, 0x4c11, { 0x9a, 0x01, 0x0e, 0x57, 0xa1, 0x10, 0x0e, 0x0e } };
        static const GUID x86 = { 0x7b1d0e11, 0x2b6d, 0x4c11, { 0x9a, 0x01, 0x0e, 0x57, 0xa1, 0x10, 0x0e, 0x0d } };
        void *unk = NULL;
        hr = CoCreateInstance(&missing, NULL, CLSCTX_INPROC_SERVER, &IID_IUnknown, &unk);
        CHECK(hr == HRESULT_FROM_WIN32(ERROR_MOD_NOT_FOUND), "a missing in-process server: %#lx", hr);
        hr = CoCreateInstance(&x86, NULL, CLSCTX_INPROC_SERVER, &IID_IUnknown, &unk);
        CHECK(hr == HRESULT_FROM_WIN32(ERROR_BAD_EXE_FORMAT), "a 32-bit in-process server in a 64-bit process: %#lx", hr);
    }
    icon = -7;
    hr = ((ov_fn)SLOT(ov, 4))(ov, a, &icon);
    CHECK(hr == S_OK && icon == image, "GetOverlayIconIndex: %#lx %d (%d)", hr, icon, image);

    /* the file view */
    {
        IExplorerBrowser *browser = NULL;
        FOLDERSETTINGS fs = { FVM_DETAILS, 0 };
        RECT rc = { 0, 0, 600, 400 };
        HWND top = CreateWindowExW(0, L"STATIC", L"overlayid", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 0, 0, 640, 480,
                                   NULL, NULL, NULL, NULL), list;
        LVFINDINFOW find = { LVFI_STRING, L"x.ova" };

        hr = CoCreateInstance(&CLSID_ExplorerBrowser, NULL, CLSCTX_INPROC_SERVER, &IID_IExplorerBrowser, (void **)&browser);
        if (browser)
        {
            IExplorerBrowser_Initialize(browser, top, &rc, &fs);
            IExplorerBrowser_BrowseToIDList(browser, dir, 0);
            pump(1500);
            list = find_list(top);
            find.psz = L"x.ova";
            i = list ? (int)SendMessageW(list, LVM_FINDITEMW, -1, (LPARAM)&find) : -1;
            if (i < 0 && list)  /* names may be shown without extensions */
            {
                find.psz = L"x";
                i = (int)SendMessageW(list, LVM_FINDITEMW, -1, (LPARAM)&find);
            }
            index = i >= 0 ? (int)SendMessageW(list, LVM_GETITEMSTATE, i, LVIS_OVERLAYMASK) : -1;
            CHECK(i >= 0 && index == INDEXTOOVERLAYMASK(1), "the view's item carries overlay 1: item %d state %#x", i, index);
            find.psz = L"z.txt";
            i = list ? (int)SendMessageW(list, LVM_FINDITEMW, -1, (LPARAM)&find) : -1;
            if (i < 0 && list) { find.psz = L"z"; i = (int)SendMessageW(list, LVM_FINDITEMW, -1, (LPARAM)&find); }
            index = i >= 0 ? (int)SendMessageW(list, LVM_GETITEMSTATE, i, LVIS_OVERLAYMASK) : -1;
            CHECK(i >= 0 && index == 0, "an unmarked item has none: item %d state %#x", i, index);
            IExplorerBrowser_Destroy(browser);
        }
        else CHECK(0, "no ExplorerBrowser: %#lx", hr);
    }
    printf("RESULT: %s\n", fails ? "FAIL" : "PASS");
    return fails != 0;
}
