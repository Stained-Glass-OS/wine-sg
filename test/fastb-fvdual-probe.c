/* shell32 IShellFolderViewDual3 on a folder view (patch 3687) */
#define COBJMACROS
#include <windows.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <shobjidl.h>
#include <shldisp.h>
#include <stdio.h>

static int failed;
#define CHECK(c, ...) do { if (!(c)) { failed++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static HWND browser_hwnd;
typedef struct { IShellBrowser IShellBrowser_iface; } browser_t;
static browser_t browser;
static HRESULT WINAPI sb_QI(IShellBrowser *i, REFIID r, void **o)
{
    if (IsEqualIID(r, &IID_IUnknown) || IsEqualIID(r, &IID_IOleWindow) || IsEqualIID(r, &IID_IShellBrowser)) { *o = i; return S_OK; }
    *o = NULL; return E_NOINTERFACE;
}
static ULONG WINAPI sb_AddRef(IShellBrowser *i) { return 2; }
static ULONG WINAPI sb_Release(IShellBrowser *i) { return 1; }
static HRESULT WINAPI sb_GetWindow(IShellBrowser *i, HWND *h) { *h = browser_hwnd; return S_OK; }
static HRESULT WINAPI sb_Ctx(IShellBrowser *i, BOOL b) { return E_NOTIMPL; }
static HRESULT WINAPI sb_Insert(IShellBrowser *i, HMENU m, LPOLEMENUGROUPWIDTHS w) { return E_NOTIMPL; }
static HRESULT WINAPI sb_Set(IShellBrowser *i, HMENU m, HOLEMENU h, HWND w) { return E_NOTIMPL; }
static HRESULT WINAPI sb_Remove(IShellBrowser *i, HMENU m) { return E_NOTIMPL; }
static HRESULT WINAPI sb_Status(IShellBrowser *i, LPCOLESTR t) { return E_NOTIMPL; }
static HRESULT WINAPI sb_EnableModeless(IShellBrowser *i, BOOL b) { return E_NOTIMPL; }
static HRESULT WINAPI sb_Translate(IShellBrowser *i, MSG *m, WORD w) { return E_NOTIMPL; }
static HRESULT WINAPI sb_Browse(IShellBrowser *i, PCUIDLIST_RELATIVE p, UINT f) { return E_NOTIMPL; }
static HRESULT WINAPI sb_GetViewState(IShellBrowser *i, DWORD m, IStream **s) { return E_FAIL; }
static HRESULT WINAPI sb_Ctrl(IShellBrowser *i, UINT id, HWND *h) { return E_NOTIMPL; }
static HRESULT WINAPI sb_Send(IShellBrowser *i, UINT id, UINT m, WPARAM w, LPARAM l, LRESULT *r) { return E_NOTIMPL; }
static HRESULT WINAPI sb_QueryActive(IShellBrowser *i, IShellView **v) { return E_NOTIMPL; }
static HRESULT WINAPI sb_OnActive(IShellBrowser *i, IShellView *v) { return E_NOTIMPL; }
static HRESULT WINAPI sb_SetToolbar(IShellBrowser *i, LPTBBUTTONSB b, UINT n, UINT f) { return E_NOTIMPL; }
static const IShellBrowserVtbl sbvtbl = { sb_QI, sb_AddRef, sb_Release, sb_GetWindow, sb_Ctx, sb_Insert, sb_Set, sb_Remove,
    sb_Status, sb_EnableModeless, sb_Translate, sb_Browse, sb_GetViewState, sb_Ctrl, sb_Send, sb_QueryActive, sb_OnActive, sb_SetToolbar };

int main(void)
{
    WCHAR dir[MAX_PATH], file[MAX_PATH];
    ITEMIDLIST *dir_pidl = NULL, *a_pidl = NULL, *b_pidl = NULL;
    IShellFolder *desktop, *folder = NULL;
    IShellView *view = NULL;
    IShellFolderViewDual3 *dual = NULL;
    FOLDERSETTINGS fs = { FVM_DETAILS, 0 };
    RECT rc = { 0, 0, 400, 300 };
    HWND list;
    HRESULT hr;
    int i;

    CoInitialize(NULL);
    browser.IShellBrowser_iface.lpVtbl = (IShellBrowserVtbl *)&sbvtbl;
    GetTempPathW(MAX_PATH, dir);
    wsprintfW(dir + lstrlenW(dir), L"sgfv_%lu", GetCurrentProcessId());
    CreateDirectoryW(dir, NULL);
    for (i = 0; i < 3; i++)
    {
        HANDLE h;
        wsprintfW(file, L"%s\\%c.txt", dir, 'a' + i);
        h = CreateFileW(file, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
        CloseHandle(h);
    }
    browser_hwnd = CreateWindowExW(0, L"Static", L"fv", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 0, 0, 500, 400, NULL, NULL, NULL, NULL);

    SHGetDesktopFolder(&desktop);
    SHParseDisplayName(dir, NULL, &dir_pidl, 0, NULL);
    IShellFolder_BindToObject(desktop, dir_pidl, NULL, &IID_IShellFolder, (void **)&folder);
    hr = IShellFolder_CreateViewObject(folder, browser_hwnd, &IID_IShellView, (void **)&view);
    CHECK(hr == S_OK && view, "CreateViewObject %#lx", hr);
    if (!view) return 1;
    hr = IShellView_CreateViewWindow(view, NULL, &fs, (IShellBrowser *)&browser, &rc, &list);
    CHECK(hr == S_OK, "CreateViewWindow %#lx", hr);
    if (hr != S_OK) return 1;
    {
        MSG msg;
        int n;
        for (n = 0; n < 50; n++) { while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) { TranslateMessage(&msg); DispatchMessageW(&msg); } Sleep(20); }
    }
    {
        IDispatch *disp = NULL;
        hr = IShellView_GetItemObject(view, SVGIO_BACKGROUND, &IID_IDispatch, (void **)&disp);
        CHECK(hr == S_OK && disp, "GetItemObject(IDispatch) %#lx", hr);
        hr = disp ? IDispatch_QueryInterface(disp, &IID_IShellFolderViewDual3, (void **)&dual) : E_FAIL;
        if (disp) IDispatch_Release(disp);
    }
    CHECK(hr == S_OK && dual, "QI dual %#lx", hr);
    if (!dual) return 1;

    /* the folder */
    {
        Folder *f = NULL;
        FolderItem *self = NULL;
        BSTR path = NULL;

        hr = IShellFolderViewDual3_get_Folder(dual, &f);
        CHECK(hr == S_OK && f, "get_Folder %#lx", hr);
        if (f)
        {
            hr = Folder2_get_Self((Folder2 *)f, &self);
            if (hr == S_OK && self) { FolderItem_get_Path(self, &path); }
            CHECK(path && !lstrcmpiW(path, dir), "folder path %ls vs %ls", path ? path : L"(null)", dir);
            SysFreeString(path);
            if (self) FolderItem_Release(self);
            Folder_Release(f);
        }
    }
    /* select b.txt by item */
    {
        FolderItems *sel = NULL;
        FolderItem *focus = NULL;
        LONG count = -1;
        UINT mode = 99;
        VARIANT v;
        Folder *f = NULL;
        FolderItems *all = NULL;
        FolderItem *second = NULL;
        BSTR name = NULL;

        hr = IShellFolderViewDual3_SelectedItems(dual, &sel);
        CHECK(hr == S_OK && sel, "SelectedItems %#lx", hr);
        if (sel) { FolderItems_get_Count(sel, &count); CHECK(count == 0, "nothing selected: %ld", count); FolderItems_Release(sel); }

        IShellFolderViewDual3_get_Folder(dual, &f);
        Folder_Items(f, &all);
        V_VT(&v) = VT_I4; V_I4(&v) = 1;
        FolderItems_Item(all, v, &second);
        CHECK(second != NULL, "second item");
        V_VT(&v) = VT_DISPATCH; V_DISPATCH(&v) = (IDispatch *)second;
        hr = IShellFolderViewDual3_SelectItem(dual, &v, SVSI_SELECT | SVSI_FOCUSED | SVSI_ENSUREVISIBLE);
        CHECK(hr == S_OK, "SelectItem by item %#lx", hr);
        sel = NULL;
        IShellFolderViewDual3_SelectedItems(dual, &sel);
        count = -1;
        if (sel) { FolderItems_get_Count(sel, &count); }
        CHECK(count == 1, "one selected: %ld", count);
        if (sel)
        {
            FolderItem *it = NULL;
            V_VT(&v) = VT_I4; V_I4(&v) = 0;
            FolderItems_Item(sel, v, &it);
            if (it) { FolderItem_get_Name(it, &name); CHECK(name && !lstrcmpiW(name, L"b.txt"), "selected %ls", name ? name : L"?"); SysFreeString(name); FolderItem_Release(it); }
            FolderItems_Release(sel);
        }
        hr = IShellFolderViewDual3_get_FocusedItem(dual, &focus);
        CHECK(hr == S_OK && focus, "FocusedItem %#lx", hr);
        if (focus) { name = NULL; FolderItem_get_Name(focus, &name); CHECK(name && !lstrcmpiW(name, L"b.txt"), "focused %ls", name ? name : L"?"); SysFreeString(name); FolderItem_Release(focus); }

        hr = IShellFolderViewDual3_SelectItemRelative(dual, 1);
        CHECK(hr == S_OK, "SelectItemRelative %#lx", hr);
        sel = NULL; IShellFolderViewDual3_SelectedItems(dual, &sel);
        if (sel)
        {
            FolderItem *it = NULL;
            V_VT(&v) = VT_I4; V_I4(&v) = 0;
            FolderItems_Item(sel, v, &it);
            name = NULL;
            if (it) { FolderItem_get_Name(it, &name); }
            CHECK(name && !lstrcmpiW(name, L"c.txt"), "next selected %ls", name ? name : L"?");
            SysFreeString(name);
            if (it) FolderItem_Release(it);
            FolderItems_Release(sel);
        }
        hr = IShellFolderViewDual3_SelectItemRelative(dual, 5);
        CHECK(hr == S_FALSE, "SelectItemRelative past the end %#lx", hr);
        V_VT(&v) = VT_I4; V_I4(&v) = 0;
        hr = IShellFolderViewDual3_SelectItem(dual, &v, SVSI_SELECT | SVSI_DESELECTOTHERS);
        CHECK(hr == S_OK, "SelectItem by index %#lx", hr);
        sel = NULL; IShellFolderViewDual3_SelectedItems(dual, &sel);
        count = -1; if (sel) { FolderItems_get_Count(sel, &count); FolderItems_Release(sel); }
        CHECK(count == 1, "one selected by index: %ld", count);

        /* view mode and size */
        hr = IShellFolderViewDual3_put_CurrentViewMode(dual, FVM_LIST);
        CHECK(hr == S_OK, "put_CurrentViewMode %#lx", hr);
        hr = IShellFolderViewDual3_get_CurrentViewMode(dual, &mode);
        CHECK(hr == S_OK && mode == FVM_LIST, "mode %u", mode);
        {
            int size = 0;
            hr = IShellFolderViewDual3_get_IconSize(dual, &size);
            CHECK(hr == S_OK && size > 0, "IconSize %#lx %d", hr, size);
        }
        {
            DWORD flags = 0;
            hr = IShellFolderViewDual3_put_FolderFlags(dual, FWF_NOCOLUMNHEADER | FWF_SNAPTOGRID);
            CHECK(hr == S_OK, "put_FolderFlags %#lx", hr);
            hr = IShellFolderViewDual3_get_FolderFlags(dual, &flags);
            CHECK(hr == S_OK && (flags & FWF_SNAPTOGRID), "FolderFlags %#lx", flags);
        }
        {
            LONG opts = -1;
            hr = IShellFolderViewDual3_get_ViewOptions(dual, &opts);
            CHECK(hr == S_OK && opts != -1, "ViewOptions %#lx %ld", hr, opts);
        }
        {
            BSTR cols = NULL;
            hr = IShellFolderViewDual3_get_SortColumns(dual, &cols);
            CHECK(hr == S_OK && cols && !wcsncmp(cols, L"prop:", 5), "SortColumns %#lx %ls", hr, cols ? cols : L"(null)");
            if (hr == S_OK && cols)
            {
                BSTR flip = SysAllocString(L"prop:-System.Size;");
                hr = IShellFolderViewDual3_put_SortColumns(dual, flip);
                SysFreeString(flip);
                SysFreeString(cols); cols = NULL;
                IShellFolderViewDual3_get_SortColumns(dual, &cols);
                CHECK(cols && wcsstr(cols, L"-") && wcsstr(cols, L"System.Size"), "sorted by size descending: %ls (put %#lx)", cols ? cols : L"?", hr);
            }
            SysFreeString(cols);
        }
        {
            IFolderView2 *fv2 = NULL;
            int cols = 0;
            hr = IShellView_QueryInterface(view, &IID_IFolderView2, (void **)&fv2);
            if (fv2)
            {
                hr = IFolderView2_GetSortColumnCount(fv2, &cols);
                CHECK(hr == S_OK && cols == 1, "GetSortColumnCount %#lx %d", hr, cols);
                hr = IFolderView2_DoRename(fv2);
                CHECK(hr == S_OK, "DoRename %#lx", hr);
                CHECK(FindWindowExW(list, NULL, L"Edit", NULL) != NULL || FindWindowExW(GetWindow(list, GW_CHILD), NULL, L"Edit", NULL) != NULL
                      || FindWindowW(L"Edit", NULL) != NULL, "label edit control");
                IFolderView2_Release(fv2);
            }
        }
        if (second) FolderItem_Release(second);
        if (all) FolderItems_Release(all);
        if (f) Folder_Release(f);
    }

    IShellFolderViewDual3_Release(dual);
    IShellView_DestroyViewWindow(view);
    IShellView_Release(view);
    DestroyWindow(browser_hwnd);
    for (i = 0; i < 3; i++) { wsprintfW(file, L"%s\\%c.txt", dir, 'a' + i); DeleteFileW(file); }
    RemoveDirectoryW(dir);
    printf(failed ? "RESULT: FAIL (%d)\n" : "RESULT: PASS\n", failed);
    return failed != 0;
}
