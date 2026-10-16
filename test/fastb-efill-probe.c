/* shell32 (3706): IExplorerBrowser::FillFromObject / RemoveAll and the results folder (IResultsFolder) */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <stdio.h>

static int failed;
#define CHECK(c, ...) do { if (!(c)) { failed++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static void pump(void)
{
    MSG msg;
    int i;
    for (i = 0; i < 5; i++)
    {
        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
        Sleep(20);
    }
}

static WCHAR base[MAX_PATH];

static void mkfile(const WCHAR *rel, const char *data)
{
    WCHAR path[MAX_PATH];
    DWORD w;
    HANDLE h;
    wsprintfW(path, L"%s\\%s", base, rel);
    h = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    if (h != INVALID_HANDLE_VALUE) { WriteFile(h, data, lstrlenA(data), &w, NULL); CloseHandle(h); }
}

static PIDLIST_ABSOLUTE rel_pidl(const WCHAR *rel)
{
    WCHAR path[MAX_PATH];
    PIDLIST_ABSOLUTE p = NULL;
    wsprintfW(path, L"%s\\%s", base, rel);
    SHParseDisplayName(path, NULL, &p, 0, NULL);
    return p;
}

static IExplorerBrowser *make_eb(HWND hwnd, DWORD options)
{
    IExplorerBrowser *eb = NULL;
    RECT rc = { 0, 0, 300, 200 };
    FOLDERSETTINGS fs = { FVM_DETAILS, 0 };
    HRESULT hr = CoCreateInstance(&CLSID_ExplorerBrowser, NULL, CLSCTX_INPROC_SERVER, &IID_IExplorerBrowser, (void **)&eb);
    CHECK(hr == S_OK, "create %#lx", hr);
    hr = IExplorerBrowser_Initialize(eb, hwnd, &rc, &fs);
    CHECK(hr == S_OK, "Initialize %#lx", hr);
    if (options) IExplorerBrowser_SetOptions(eb, options);
    return eb;
}

static int view_count(IExplorerBrowser *eb)
{
    IFolderView2 *fv = NULL;
    int n = -1;
    if (SUCCEEDED(IExplorerBrowser_GetCurrentView(eb, &IID_IFolderView2, (void **)&fv)))
    {
        IFolderView2_ItemCount(fv, SVGIO_ALLVIEW, &n);
        IFolderView2_Release(fv);
    }
    return n;
}

static int selected_count(IExplorerBrowser *eb)
{
    IFolderView2 *fv = NULL;
    int n = -1;
    if (SUCCEEDED(IExplorerBrowser_GetCurrentView(eb, &IID_IFolderView2, (void **)&fv)))
    {
        IFolderView2_ItemCount(fv, SVGIO_SELECTION, &n);
        IFolderView2_Release(fv);
    }
    return n;
}

/* the names of the items of the view, joined with '|' in view order */
static void view_names(IExplorerBrowser *eb, WCHAR *out, size_t size)
{
    IFolderView2 *fv = NULL;
    IShellFolder *folder = NULL;
    int i, n = 0;

    out[0] = 0;
    if (FAILED(IExplorerBrowser_GetCurrentView(eb, &IID_IFolderView2, (void **)&fv))) return;
    IFolderView2_GetFolder(fv, &IID_IShellFolder, (void **)&folder);
    IFolderView2_ItemCount(fv, SVGIO_ALLVIEW, &n);
    for (i = 0; i < n && folder; i++)
    {
        PITEMID_CHILD child = NULL;
        STRRET sr;
        WCHAR name[MAX_PATH];
        if (SUCCEEDED(IFolderView2_Item(fv, i, &child)) && SUCCEEDED(IShellFolder_GetDisplayNameOf(folder, child, SHGDN_INFOLDER, &sr))
                && SUCCEEDED(StrRetToBufW(&sr, child, name, MAX_PATH)))
        {
            if (out[0]) lstrcatW(out, L"|");
            lstrcatW(out, name);
        }
        ILFree(child);
    }
    if (folder) IShellFolder_Release(folder);
    IFolderView2_Release(fv);
}

static BOOL has_name(const WCHAR *names, const WCHAR *name)
{
    WCHAR pat[MAX_PATH];
    wsprintfW(pat, L"|%s|", name);
    {
        WCHAR all[4096];
        wsprintfW(all, L"|%s|", names);
        return wcsstr(all, pat) != NULL;
    }
}

int main(void)
{
    HWND hwnd;
    IExplorerBrowser *eb, *eb2;
    IShellItemArray *arr = NULL;
    IShellItem *item = NULL;
    PIDLIST_ABSOLUTE pa_f1, pb_g1, pa, pb, pa_f2;
    PCIDLIST_ABSOLUTE two[2];
    IResultsFolder *rf = NULL;
    IFolderView2 *fv = NULL;
    IDataObject *dobj = NULL;
    IShellFolder *folder = NULL;
    HRESULT hr;
    WCHAR names[4096], tmp[MAX_PATH];
    int n;

    OleInitialize(NULL);
    GetTempPathW(MAX_PATH, base);
    lstrcatW(base, L"sgefill");
    CreateDirectoryW(base, NULL);
    wsprintfW(tmp, L"%s\\a", base); CreateDirectoryW(tmp, NULL);
    wsprintfW(tmp, L"%s\\a\\d1", base); CreateDirectoryW(tmp, NULL);
    wsprintfW(tmp, L"%s\\b", base); CreateDirectoryW(tmp, NULL);
    mkfile(L"a\\f1.txt", "one");
    mkfile(L"a\\f2.txt", "two two");
    mkfile(L"b\\g1.txt", "three");
    pa = rel_pidl(L"a"); pb = rel_pidl(L"b");
    pa_f1 = rel_pidl(L"a\\f1.txt"); pa_f2 = rel_pidl(L"a\\f2.txt"); pb_g1 = rel_pidl(L"b\\g1.txt");
    CHECK(pa && pb && pa_f1 && pa_f2 && pb_g1, "pidls");

    hwnd = CreateWindowW(L"STATIC", L"fill", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 0, 0, 400, 300, NULL, NULL, NULL, NULL);
    eb = make_eb(hwnd, 0);

    hr = IExplorerBrowser_RemoveAll(eb);
    CHECK(hr == E_UNEXPECTED, "RemoveAll before a fill %#lx", hr);
    hr = IExplorerBrowser_FillFromObject(eb, NULL, 0);
    CHECK(hr == E_INVALIDARG, "FillFromObject(NULL) %#lx", hr);
    hr = IExplorerBrowser_FillFromObject(eb, (IUnknown *)eb, 0);
    CHECK(hr == E_NOINTERFACE, "an object that is no source %#lx", hr);
    hr = IExplorerBrowser_RemoveAll(eb);
    CHECK(hr == E_UNEXPECTED, "RemoveAll after a failed fill %#lx", hr);

    /* from an array of items of two folders */
    two[0] = pa_f1; two[1] = pb_g1;
    hr = SHCreateShellItemArrayFromIDLists(2, two, &arr);
    CHECK(hr == S_OK && arr, "item array %#lx", hr);
    hr = IExplorerBrowser_FillFromObject(eb, (IUnknown *)arr, 0);
    CHECK(hr == S_OK, "FillFromObject(array) %#lx", hr);
    pump();
    n = view_count(eb);
    CHECK(n == 2, "two items in the view: %d", n);
    view_names(eb, names, ARRAYSIZE(names));
    CHECK(has_name(names, L"f1.txt") && has_name(names, L"g1.txt"), "names: %ls", names);

    /* more: the children of folder a (f1 is there already) */
    hr = SHCreateItemFromIDList(pa, &IID_IShellItem, (void **)&item);
    hr = IExplorerBrowser_FillFromObject(eb, (IUnknown *)item, 0);
    CHECK(hr == S_OK, "FillFromObject(folder item) %#lx", hr);
    pump();
    view_names(eb, names, ARRAYSIZE(names));
    n = view_count(eb);
    CHECK(n == 4 && has_name(names, L"f1.txt") && has_name(names, L"f2.txt") && has_name(names, L"d1") && has_name(names, L"g1.txt"),
          "four items, no duplicate: %d %ls", n, names);
    IShellItem_Release(item);

    /* the folder of the view */
    IExplorerBrowser_GetCurrentView(eb, &IID_IFolderView2, (void **)&fv);
    hr = IFolderView2_GetFolder(fv, &IID_IResultsFolder, (void **)&rf);
    CHECK(hr == S_OK && rf, "the view's folder is a results folder %#lx", hr);
    hr = IFolderView2_GetFolder(fv, &IID_IShellFolder, (void **)&folder);
    {
        /* what the items are and what they can do: asked of the folders they really live in */
        PITEMID_CHILD child = NULL;
        STRRET sr;
        SFGAOF attrs;
        int i, folders = 0, files = 0, idx_d1 = -1;
        IContextMenu *cm = NULL;
        HMENU menu;
        IShellFolder2 *f2 = NULL;
        SHELLDETAILS sd;

        for (i = 0; i < 4; i++)
        {
            if (FAILED(IFolderView2_Item(fv, i, &child))) continue;
            attrs = SFGAO_FOLDER;
            IShellFolder_GetAttributesOf(folder, 1, (PCUITEMID_CHILD_ARRAY)&child, &attrs);
            if (attrs & SFGAO_FOLDER) { folders++; idx_d1 = i; } else files++;
            ILFree(child);
        }
        CHECK(folders == 1 && files == 3, "attributes: %d folders %d files", folders, files);
        IFolderView2_Item(fv, idx_d1 >= 0 ? idx_d1 : 0, &child);
        hr = IShellFolder_GetDisplayNameOf(folder, child, SHGDN_FORPARSING, &sr);
        CHECK(hr == S_OK, "GetDisplayNameOf %#lx", hr);
        if (SUCCEEDED(hr))
        {
            StrRetToBufW(&sr, child, tmp, MAX_PATH);
            CHECK(wcsstr(tmp, L"d1") != NULL, "parsing name: %ls", tmp);
        }
        hr = IShellFolder_GetUIObjectOf(folder, hwnd, 1, (PCUITEMID_CHILD_ARRAY)&child, &IID_IContextMenu, NULL, (void **)&cm);
        CHECK(hr == S_OK && cm, "context menu of an item %#lx", hr);
        if (cm)
        {
            menu = CreatePopupMenu();
            hr = IContextMenu_QueryContextMenu(cm, menu, 0, 1, 0x7fff, CMF_NORMAL);
            CHECK(SUCCEEDED(hr) && GetMenuItemCount(menu) > 0, "menu has entries: %d", GetMenuItemCount(menu));
            DestroyMenu(menu);
            IContextMenu_Release(cm);
        }
        hr = IShellFolder_GetUIObjectOf(folder, hwnd, 1, (PCUITEMID_CHILD_ARRAY)&child, &IID_IDataObject, NULL, (void **)&dobj);
        CHECK(hr == S_OK && dobj, "data object of an item %#lx", hr);
        if (dobj)
        {
            FORMATETC fmt = { 0, NULL, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
            STGMEDIUM med;
            fmt.cfFormat = RegisterClipboardFormatW(L"Shell IDList Array");
            hr = IDataObject_GetData(dobj, &fmt, &med);
            CHECK(hr == S_OK, "CF_SHELLIDLIST %#lx", hr);
            if (SUCCEEDED(hr))
            {
                CIDA *cida = GlobalLock(med.hGlobal);
                CHECK(cida && cida->cidl == 1, "one item in the list");
                GlobalUnlock(med.hGlobal);
                ReleaseStgMedium(&med);
            }
            fmt.cfFormat = CF_HDROP;
            hr = IDataObject_QueryGetData(dobj, &fmt);
            CHECK(hr == S_OK, "CF_HDROP offered %#lx", hr);
            IDataObject_Release(dobj);
        }
        ILFree(child);
        /* the columns come from the folder of the items */
        hr = IShellFolder_QueryInterface(folder, &IID_IShellFolder2, (void **)&f2);
        CHECK(hr == S_OK, "IShellFolder2 %#lx", hr);
        if (f2)
        {
            memset(&sd, 0, sizeof(sd));
            hr = IShellFolder2_GetDetailsOf(f2, NULL, 0, &sd);
            if (SUCCEEDED(hr)) StrRetToBufW(&sd.str, NULL, tmp, MAX_PATH);
            CHECK(hr == S_OK && !lstrcmpiW(tmp, L"Name"), "column 0 header: %#lx %ls", hr, tmp);
            hr = IShellFolder2_GetDetailsOf(f2, NULL, 1, &sd);
            CHECK(hr == S_OK, "column 1 header %#lx", hr);
            {
                SHCOLUMNID scid = {{0}};
                hr = IShellFolder2_MapColumnToSCID(f2, 1, &scid);
                CHECK(hr == S_OK && scid.pid != 0, "MapColumnToSCID(1) %#lx pid %lu", hr, scid.pid);
            }
            IFolderView2_Item(fv, 0, &child);
            hr = IShellFolder2_GetDetailsOf(f2, child, 0, &sd);
            CHECK(hr == S_OK && sd.str.uType == STRRET_WSTR, "column 0 of an item %#lx type %u", hr, sd.str.uType);
            if (hr == S_OK) CoTaskMemFree(sd.str.pOleStr);
            ILFree(child);
            IShellFolder2_Release(f2);
        }
    }
    if (folder) IShellFolder_Release(folder);

    /* IResultsFolder used directly: the view follows */
    if (rf)
    {
        PITEMID_CHILD added = NULL;
        SHCreateItemFromIDList(pb_g1, &IID_IShellItem, (void **)&item);
        hr = IResultsFolder_AddItem(rf, item);
        CHECK(hr == S_OK, "AddItem (existing) %#lx", hr);
        IShellItem_Release(item);
        n = view_count(eb);
        CHECK(n == 4, "an item is in the folder once: %d", n);
        hr = IResultsFolder_RemoveIDList(rf, pb_g1);
        CHECK(hr == S_OK, "RemoveIDList %#lx", hr);
        pump();
        n = view_count(eb);
        CHECK(n == 3, "the view lost the item: %d", n);
        hr = IResultsFolder_RemoveIDList(rf, pb_g1);
        CHECK(hr == S_FALSE, "RemoveIDList again %#lx", hr);
        hr = IResultsFolder_AddIDList(rf, pb_g1, &added);
        CHECK(hr == S_OK && added, "AddIDList %#lx", hr);
        ILFree(added);
        pump();
        n = view_count(eb);
        CHECK(n == 4, "the view got it back: %d", n);
        hr = IResultsFolder_AddIDList(rf, NULL, NULL);
        CHECK(hr == E_INVALIDARG, "AddIDList(NULL) %#lx", hr);
        SHCreateItemFromIDList(pa_f2, &IID_IShellItem, (void **)&item);
        hr = IResultsFolder_RemoveItem(rf, item);
        CHECK(hr == S_OK, "RemoveItem %#lx", hr);
        IShellItem_Release(item);
        pump();
        n = view_count(eb);
        CHECK(n == 3, "RemoveItem: %d", n);
        IResultsFolder_Release(rf);
    }
    if (fv) IFolderView2_Release(fv);

    /* RemoveAll */
    hr = IExplorerBrowser_RemoveAll(eb);
    CHECK(hr == S_OK, "RemoveAll %#lx", hr);
    pump();
    n = view_count(eb);
    CHECK(n == 0, "view emptied: %d", n);
    hr = IExplorerBrowser_FillFromObject(eb, (IUnknown *)arr, 0);
    pump();
    n = view_count(eb);
    CHECK(hr == S_OK && n == 2, "filled again: %#lx %d", hr, n);

    /* browsing away leaves the results */
    hr = IExplorerBrowser_BrowseToIDList(eb, pb, 0);
    CHECK(hr == S_OK, "browse to b %#lx", hr);
    pump();
    view_names(eb, names, ARRAYSIZE(names));
    CHECK(n == 2 && has_name(names, L"g1.txt") && !has_name(names, L"f1.txt"), "now showing folder b: %ls", names);
    hr = IExplorerBrowser_RemoveAll(eb);
    CHECK(hr == E_UNEXPECTED, "RemoveAll after leaving the results %#lx", hr);
    IShellItemArray_Release(arr);
    IExplorerBrowser_Destroy(eb);
    IExplorerBrowser_Release(eb);

    /* a data object whose items are selected among their peers */
    eb2 = make_eb(hwnd, 0);
    {
        PCUITEMID_CHILD one = ILFindLastID(pa_f2);
        hr = SHCreateDataObject(pa, 1, &one, NULL, &IID_IDataObject, (void **)&dobj);
        CHECK(hr == S_OK, "data object %#lx", hr);
    }
    hr = IExplorerBrowser_FillFromObject(eb2, (IUnknown *)dobj, EBF_SELECTFROMDATAOBJECT);
    CHECK(hr == S_OK, "FillFromObject(data object, select) %#lx", hr);
    pump();
    n = view_count(eb2);
    view_names(eb2, names, ARRAYSIZE(names));
    CHECK(n == 3 && has_name(names, L"f1.txt") && has_name(names, L"f2.txt") && has_name(names, L"d1"), "the peers are shown: %d %ls", n, names);
    CHECK(selected_count(eb2) == 1, "only the item of the data object is selected: %d", selected_count(eb2));
    IDataObject_Release(dobj);
    hr = IExplorerBrowser_FillFromObject(eb2, (IUnknown *)eb2, EBF_SELECTFROMDATAOBJECT);
    CHECK(hr == E_INVALIDARG, "select flag with something that is no data object %#lx", hr);
    IExplorerBrowser_Destroy(eb2);
    IExplorerBrowser_Release(eb2);

    /* EBO_NAVIGATEONCE keeps the browser in the results */
    eb = make_eb(hwnd, EBO_NAVIGATEONCE);
    SHCreateItemFromIDList(pa, &IID_IShellItem, (void **)&item);
    hr = IExplorerBrowser_FillFromObject(eb, (IUnknown *)item, EBF_NODROPTARGET);
    CHECK(hr == S_OK, "fill %#lx", hr);
    pump();
    hr = IExplorerBrowser_BrowseToIDList(eb, pb, 0);
    CHECK(hr == E_FAIL, "navigate once: browsing out of the results fails %#lx", hr);
    n = view_count(eb);
    CHECK(n == 3, "still the three items of a: %d", n);
    IShellItem_Release(item);
    IExplorerBrowser_Destroy(eb);
    IExplorerBrowser_Release(eb);

    DestroyWindow(hwnd);
    ILFree(pa); ILFree(pb); ILFree(pa_f1); ILFree(pa_f2); ILFree(pb_g1);
    wsprintfW(tmp, L"%s\\a\\f1.txt", base); DeleteFileW(tmp);
    wsprintfW(tmp, L"%s\\a\\f2.txt", base); DeleteFileW(tmp);
    wsprintfW(tmp, L"%s\\b\\g1.txt", base); DeleteFileW(tmp);
    wsprintfW(tmp, L"%s\\a\\d1", base); RemoveDirectoryW(tmp);
    wsprintfW(tmp, L"%s\\a", base); RemoveDirectoryW(tmp);
    wsprintfW(tmp, L"%s\\b", base); RemoveDirectoryW(tmp);
    RemoveDirectoryW(base);
    printf("failures: %d\nRESULT: %s\n", failed, failed ? "FAIL" : "PASS");
    OleUninitialize();
    return failed != 0;
}
