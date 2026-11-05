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

/* another thread: does the view's list answer a message within a second? */
static HWND probe_list;
static LONG probe_ok = -1;
static DWORD WINAPI responsive_thread(void *arg)
{
    DWORD_PTR result;
    Sleep(200);
    probe_ok = SendMessageTimeoutW(probe_list, WM_NULL, 0, 0, SMTO_NORMAL, 1000, &result) ? 1 : 0;
    return 0;
}

static int item_state(HWND list, const WCHAR *name, const WCHAR *bare)
{
    LVFINDINFOW find = { LVFI_STRING, name };
    int i = list ? (int)SendMessageW(list, LVM_FINDITEMW, -1, (LPARAM)&find) : -1;
    if (i < 0 && list) { find.psz = bare; i = (int)SendMessageW(list, LVM_FINDITEMW, -1, (LPARAM)&find); }
    return i >= 0 ? (int)SendMessageW(list, LVM_GETITEMSTATE, i, LVIS_OVERLAYMASK) : -1;
}

/* 6254: how many pixels of two items' icons differ, as the list draws them (WM_PRINTCLIENT) */
static int icon_diff(HWND list, int a, int b)
{
    RECT ra = { LVIR_ICON }, rb = { LVIR_ICON }, client;
    BITMAPINFO bmi = {{ sizeof(BITMAPINFOHEADER) }};
    DWORD *bits;
    HBITMAP bmp, old;
    HDC dc;
    int x, y, w, h, diff = 0;

    if (a < 0 || b < 0) return -1;
    SendMessageW(list, LVM_GETITEMRECT, a, (LPARAM)&ra);
    SendMessageW(list, LVM_GETITEMRECT, b, (LPARAM)&rb);
    GetClientRect(list, &client);
    bmi.bmiHeader.biWidth = client.right;
    bmi.bmiHeader.biHeight = -client.bottom;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    dc = CreateCompatibleDC(NULL);
    if (!(bmp = CreateDIBSection(dc, &bmi, DIB_RGB_COLORS, (void **)&bits, NULL, 0))) { DeleteDC(dc); return -1; }
    old = SelectObject(dc, bmp);
    SendMessageW(list, WM_PRINTCLIENT, (WPARAM)dc, PRF_CLIENT | PRF_ERASEBKGND);
    GdiFlush();
    w = min(ra.right - ra.left, rb.right - rb.left);
    h = min(ra.bottom - ra.top, rb.bottom - rb.top);
    for (y = 0; y < h; y++)
        for (x = 0; x < w; x++)
        {
            int ya = ra.top + y, xa = ra.left + x, yb = rb.top + y, xb = rb.left + x;
            if (ya < 0 || yb < 0 || ya >= client.bottom || yb >= client.bottom || xa >= client.right || xb >= client.right) continue;
            if ((bits[ya * client.right + xa] & 0xffffff) != (bits[yb * client.right + xb] & 0xffffff)) diff++;
        }
    SelectObject(dc, old);
    DeleteObject(bmp);
    DeleteDC(dc);
    return diff;
}

static int find_row(HWND list, const WCHAR *name, const WCHAR *bare)
{
    LVFINDINFOW find = { LVFI_STRING, name };
    int i = (int)SendMessageW(list, LVM_FINDITEMW, -1, (LPARAM)&find);
    if (i < 0) { find.psz = bare; i = (int)SendMessageW(list, LVM_FINDITEMW, -1, (LPARAM)&find); }
    return i;
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
    CHECK(hr == S_OK && index == 15, "the handler after one that failed (and the slow one) has the 15th slot: %#lx %d", hr, index);
    index = -7;
    hr = ((ov_fn)SLOT(ov, 3))(ov, d, &index);
    CHECK(hr == S_FALSE, "the 16th handler is not loaded: %#lx %d", hr, index);
    hr = ((ov_fn)SLOT(ov, 3))(ov, c, &index);
    CHECK(hr == S_FALSE, "no handler marks it: %#lx", hr);

    dll = GetModuleHandleW(L"overlayid.dll");
    last_path = dll ? (void *)GetProcAddress(dll, "last_path") : NULL;
    CHECK(last_path && !lstrcmpiW(last_path(), L"C:\\ovtest\\z.txt"), "IsMemberOf gets the file's path: %ls",
          last_path ? last_path() : L"(not loaded)");
    {
        DWORD (*last_attrib)(void) = dll ? (void *)GetProcAddress(dll, "last_attrib") : NULL;
        DWORD want = GetFileAttributesW(L"C:\\ovtest\\z.txt");
        CHECK(last_attrib && last_attrib() == want, "IsMemberOf gets the file's attributes: %#lx, want %#lx",
              last_attrib ? last_attrib() : 0xdeadUL, want);
    }

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
            pump(300);
            list = find_list(top);
            /* 6251: the slow handler is asked beside the view, which keeps answering */
            if (list)
            {
                HANDLE t;
                DWORD start = GetTickCount(), took;
                probe_list = list;
                t = CreateThread(NULL, 0, responsive_thread, NULL, 0, NULL);
                InvalidateRect(list, NULL, TRUE);
                pump(1500);
                index = item_state(list, L"v.ovs", L"v");
                took = GetTickCount() - start;
                WaitForSingleObject(t, 5000);
                CloseHandle(t);
                CHECK(probe_ok == 1, "the view answers while a handler takes 5 s: %ld", probe_ok);
                CHECK(took < 4000, "and its thread is not held: %lu ms", took);
                CHECK(index == 0, "the slow handler's item has no overlay yet: %#x", index);
                pump(6500);
                index = item_state(list, L"v.ovs", L"v");
                CHECK(index == INDEXTOOVERLAYMASK(14), "it has it when the answer comes: %#x", index);
            }
            else CHECK(0, "no list");
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
            /* 6254: the overlay is drawn: the marked item's icon differs from an unmarked one of the
             * same type, which two unmarked ones do not -- in details and in large icons */
            if (list)
            {
                IFolderView *fv = NULL;
                int marked = find_row(list, L"x.ova", L"x"), q = find_row(list, L"q.ovq", L"q"), r = find_row(list, L"r.ovr", L"r");
                int d1 = icon_diff(list, marked, q), d0 = icon_diff(list, q, r);
                printf("details: %d pixels of the marked icon differ, %d of an unmarked one\n", d1, d0);
                CHECK(d0 == 0 && d1 > 4, "details: the overlay is drawn on the marked item (%d pixels differ; unmarked %d)", d1, d0);
                IExplorerBrowser_GetCurrentView(browser, &IID_IFolderView, (void **)&fv);
                if (fv)
                {
                    IFolderView_SetCurrentViewMode(fv, FVM_ICON);
                    pump(1500);
                    list = find_list(top);
                    marked = find_row(list, L"x.ova", L"x"); q = find_row(list, L"q.ovq", L"q"); r = find_row(list, L"r.ovr", L"r");
                    d1 = icon_diff(list, marked, q); d0 = icon_diff(list, q, r);
                    printf("large icons: %d pixels of the marked icon differ, %d of an unmarked one\n", d1, d0);
                    CHECK(d0 == 0 && d1 > 4, "large icons: the overlay is drawn on the marked item (%d pixels differ; unmarked %d)", d1, d0);
                    IFolderView_Release(fv);
                }
                else CHECK(0, "no IFolderView");
            }
            IExplorerBrowser_Destroy(browser);
        }
        else CHECK(0, "no ExplorerBrowser: %#lx", hr);
    }
    printf("RESULT: %s\n", fails ? "FAIL" : "PASS");
    return fails != 0;
}
