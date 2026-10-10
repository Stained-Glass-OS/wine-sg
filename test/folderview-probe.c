/* shell32 batch (patches/sg/2042), run by test/folderview-gate.sh. Families: the
 * IFolderView / IFolderView2 methods of a shell view of a folder with three
 * files (shown through an ExplorerBrowser): item positions, default spacing,
 * auto arrange, selecting and positioning items, GetItem (as an IShellItem),
 * GetVisibleItem, GetSelection, GetSelectionState; and SHCreateItemWithParent.
 *
 *   folderview-probe.exe */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <commctrl.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}
static void checkhr(HRESULT hr, HRESULT want, const char *what)
{
    char buf[200];
    snprintf(buf, sizeof(buf), "%s (hr %08lx, want %08lx)", what, (unsigned long)hr, (unsigned long)want);
    check(hr == want, buf);
}

DEFINE_GUID(SG_CLSID_ExplorerBrowser, 0x71f96385, 0xddd6, 0x48d3, 0xa0, 0xc1, 0xae, 0x06, 0xe8, 0xb0, 0x55, 0xfb);

static void pump(void)
{
    MSG msg;
    int i;
    for (i = 0; i < 20; i++)
    {
        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
        Sleep(10);
    }
}

static WCHAR dir[MAX_PATH];

static void make_files(void)
{
    static const WCHAR *names[] = { L"a.txt", L"b.txt", L"c.txt" };
    WCHAR path[MAX_PATH];
    int i;
    GetTempPathW(MAX_PATH, dir);
    wcscat(dir, L"sg-folderview");
    CreateDirectoryW(dir, NULL);
    for (i = 0; i < 3; i++)
    {
        HANDLE h;
        swprintf(path, MAX_PATH, L"%ls\\%ls", dir, names[i]);
        h = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
        CloseHandle(h);
    }
}

static void remove_files(void)
{
    static const WCHAR *names[] = { L"a.txt", L"b.txt", L"c.txt" };
    WCHAR path[MAX_PATH];
    int i;
    for (i = 0; i < 3; i++)
    {
        swprintf(path, MAX_PATH, L"%ls\\%ls", dir, names[i]);
        DeleteFileW(path);
    }
    RemoveDirectoryW(dir);
}

static WCHAR *item_name(IShellItem *item)
{
    WCHAR *name = NULL;
    if (item) IShellItem_GetDisplayName(item, SIGDN_NORMALDISPLAY, &name);
    return name;
}

int main(void)
{
    IExplorerBrowser *peb = NULL;
    IFolderView2 *fv = NULL;
    IShellFolder *folder = NULL;
    HWND hwnd;
    RECT rc = { 0, 0, 400, 300 };
    PIDLIST_ABSOLUTE abs = NULL;
    PITEMID_CHILD pidl[3] = { 0 }, bogus;
    POINT pt, pt2, def, apt[2];
    HRESULT hr;
    int i, count = 0, idx;
    IShellItem *item = NULL;
    IShellItemArray *arr = NULL;
    DWORD n = 0, flags;
    WCHAR *name;

    CoInitialize(NULL);
    make_files();
    hwnd = CreateWindowExW(0, L"static", NULL, WS_POPUP | WS_VISIBLE, 0, 0, 500, 400, NULL, NULL, NULL, NULL);
    hr = CoCreateInstance(&SG_CLSID_ExplorerBrowser, NULL, CLSCTX_INPROC_SERVER, &IID_IExplorerBrowser, (void **)&peb);
    checkhr(hr, S_OK, "create the ExplorerBrowser");
    if (!peb) { printf("RESULT: FAIL\n"); return 1; }
    IExplorerBrowser_Initialize(peb, hwnd, &rc, NULL);
    SHParseDisplayName(dir, NULL, &abs, 0, NULL);
    SHBindToObject(NULL, abs, NULL, &IID_IShellFolder, (void **)&folder);
    check(abs && folder, "the folder of the files");
    hr = IExplorerBrowser_BrowseToObject(peb, (IUnknown *)folder, SBSP_DEFBROWSER);
    checkhr(hr, S_OK, "BrowseToObject(folder)");
    pump();
    hr = IExplorerBrowser_GetCurrentView(peb, &IID_IFolderView2, (void **)&fv);
    checkhr(hr, S_OK, "the view as IFolderView2");
    if (!fv) { printf("RESULT: FAIL\n"); return 1; }

    IFolderView2_SetCurrentViewMode(fv, FVM_ICON);
    pump();
    IFolderView2_ItemCount(fv, SVGIO_ALLVIEW, &count);
    check(count == 3, "three items");
    for (i = 0; i < 3; i++) IFolderView2_Item(fv, i, &pidl[i]);
    check(pidl[0] && pidl[1] && pidl[2], "the items' pidls");

    /* GetItem */
    hr = IFolderView2_GetItem(fv, 1, &IID_IShellItem, (void **)&item);
    checkhr(hr, S_OK, "GetItem(1) as an IShellItem");
    name = item_name(item);
    check(name && !wcscmp(name, L"b.txt"), "the item is b.txt");
    CoTaskMemFree(name);
    if (item) IShellItem_Release(item);
    checkhr(IFolderView2_GetItem(fv, 7, &IID_IShellItem, (void **)&item), E_INVALIDARG, "GetItem(7)");
    checkhr(IFolderView2_GetItem(fv, 0, &IID_IShellItem, NULL), E_POINTER, "GetItem to NULL");

    /* positions */
    hr = IFolderView2_GetItemPosition(fv, pidl[0], &pt);
    checkhr(hr, S_OK, "GetItemPosition(a)");
    hr = IFolderView2_GetItemPosition(fv, pidl[1], &pt2);
    check(hr == S_OK && (pt.x != pt2.x || pt.y != pt2.y), "another item is somewhere else");
    bogus = ILCreateFromPathW(L"C:\\no\\such\\item.bin");
    {
        PITEMID_CHILD other = ILFindLastID(bogus);
        checkhr(IFolderView2_GetItemPosition(fv, other, &pt2), E_INVALIDARG, "an item that is not in the view");
    }
    ILFree(bogus);
    checkhr(IFolderView2_GetItemPosition(fv, NULL, &pt2), E_INVALIDARG, "no item");
    checkhr(IFolderView2_GetItemPosition(fv, pidl[0], NULL), E_INVALIDARG, "no point");

    hr = IFolderView2_GetDefaultSpacing(fv, &def);
    check(hr == S_OK && def.x == GetSystemMetrics(SM_CXICONSPACING) && def.y == GetSystemMetrics(SM_CYICONSPACING),
          "the default spacing is the system's icon spacing");
    checkhr(IFolderView2_GetDefaultSpacing(fv, NULL), E_INVALIDARG, "default spacing to nothing");

    /* the list of this view arranges itself unless told otherwise: items can be put somewhere else only without it */
    {
        IShellView *sv = NULL;
        HWND view = NULL, list;
        if (SUCCEEDED(IFolderView2_QueryInterface(fv, &IID_IShellView, (void **)&sv)) && sv)
        {
            IShellView_GetWindow(sv, &view);
            list = view ? FindWindowExW(view, NULL, L"SysListView32", NULL) : NULL;
            if (list) SetWindowLongW(list, GWL_STYLE, GetWindowLongW(list, GWL_STYLE) & ~LVS_AUTOARRANGE);
            IShellView_Release(sv);
        }
    }

    /* auto arrange */
    IFolderView2_SetCurrentFolderFlags(fv, FWF_AUTOARRANGE, 0);
    checkhr(IFolderView2_GetAutoArrange(fv), S_FALSE, "not auto arranged");
    IFolderView2_SetCurrentFolderFlags(fv, FWF_AUTOARRANGE, FWF_AUTOARRANGE);
    checkhr(IFolderView2_GetAutoArrange(fv), S_OK, "auto arranged");
    IFolderView2_SetCurrentFolderFlags(fv, FWF_AUTOARRANGE, 0);

    /* visible items */
    idx = 99;
    hr = IFolderView2_GetVisibleItem(fv, -1, FALSE, &idx);
    check(hr == S_OK && idx == 0, "the first visible item");
    hr = IFolderView2_GetVisibleItem(fv, 0, FALSE, &idx);
    check(hr == S_OK && idx == 1, "the next one");
    hr = IFolderView2_GetVisibleItem(fv, 2, FALSE, &idx);
    check(hr == S_FALSE && idx == -1, "none after the last");
    hr = IFolderView2_GetVisibleItem(fv, 2, TRUE, &idx);
    if (idx != 1) printf("   previous of 2: hr %08lx idx %d\n", (unsigned long)hr, idx);
    check(hr == S_OK && idx == 1, "the one before the last");
    checkhr(IFolderView2_GetVisibleItem(fv, 0, FALSE, NULL), E_POINTER, "no result pointer");

    /* selection */
    IFolderView2_SelectItem(fv, 0, SVSI_DESELECTOTHERS);
    hr = IFolderView2_GetSelection(fv, FALSE, &arr);
    check(hr == S_FALSE && arr == NULL, "nothing selected, nothing asked");
    hr = IFolderView2_GetSelection(fv, TRUE, &arr);
    count = 0;
    if (arr) IShellItemArray_GetCount(arr, &n);
    check(hr == S_OK && arr && n == 1, "nothing selected gives the folder");
    if (arr)
    {
        IShellItem *only = NULL;
        IShellItemArray_GetItemAt(arr, 0, &only);
        name = item_name(only);
        check(name && !wcscmp(name, L"sg-folderview"), "and it is the folder");
        CoTaskMemFree(name);
        if (only) IShellItem_Release(only);
        IShellItemArray_Release(arr);
        arr = NULL;
    }
    IFolderView2_SelectItem(fv, 1, SVSI_SELECT | SVSI_DESELECTOTHERS | SVSI_FOCUSED);
    hr = IFolderView2_GetSelection(fv, FALSE, &arr);
    n = 0;
    if (arr) IShellItemArray_GetCount(arr, &n);
    check(hr == S_OK && n == 1, "one item selected");
    if (arr)
    {
        IShellItem *only = NULL;
        IShellItemArray_GetItemAt(arr, 0, &only);
        name = item_name(only);
        check(name && !wcscmp(name, L"b.txt"), "and it is b.txt");
        CoTaskMemFree(name);
        if (only) IShellItem_Release(only);
        IShellItemArray_Release(arr);
        arr = NULL;
    }
    flags = 0;
    hr = IFolderView2_GetSelectionState(fv, pidl[1], &flags);
    check(hr == S_OK && (flags & SVSI_SELECT) && (flags & SVSI_FOCUSED), "b.txt is selected and focused");
    flags = 99;
    hr = IFolderView2_GetSelectionState(fv, pidl[2], &flags);
    check(hr == S_OK && flags == 0, "c.txt is neither");
    checkhr(IFolderView2_GetSelectionState(fv, NULL, &flags), E_INVALIDARG, "no item for the state");

    /* verbs */
    hr = IFolderView2_InvokeVerbOnSelection(fv, "sg-no-such-verb");
    check(FAILED(hr), "an unknown verb fails");
    IFolderView2_SelectItem(fv, 0, SVSI_DESELECTOTHERS);
    {
        int n2 = 0, i2;
        for (i2 = 0; i2 < 3; i2++) IFolderView2_SelectItem(fv, i2, SVSI_DESELECT);
        (void)n2;
    }
    hr = IFolderView2_InvokeVerbOnSelection(fv, "open");
    checkhr(hr, S_FALSE, "a verb on an empty selection does nothing");

    /* select and position */
    apt[0].x = 300; apt[0].y = 200;
    apt[1].x = 40; apt[1].y = 120;
    {
        PCUITEMID_CHILD two[2] = { pidl[0], pidl[2] };
        hr = IFolderView2_SelectAndPositionItems(fv, 2, two, apt, SVSI_SELECT | SVSI_DESELECTOTHERS);
        checkhr(hr, S_OK, "SelectAndPositionItems");
        IFolderView2_GetItemPosition(fv, pidl[0], &pt);
        check(pt.x == 300 && pt.y == 200, "the first item is where it was put");
        IFolderView2_GetItemPosition(fv, pidl[2], &pt);
        check(pt.x == 40 && pt.y == 120, "so is the second");
        IFolderView2_GetSelectionState(fv, pidl[0], &flags);
        check(flags & SVSI_SELECT, "the first is selected");
        IFolderView2_GetSelectionState(fv, pidl[2], &flags);
        check(flags & SVSI_SELECT, "and the second");
        IFolderView2_GetSelectionState(fv, pidl[1], &flags);
        check(!(flags & SVSI_SELECT), "the one before is not any more");
        hr = IFolderView2_SelectAndPositionItems(fv, 2, two, NULL, SVSI_SELECT);
        checkhr(hr, S_OK, "without positions");
    }
    {
        PCUITEMID_CHILD none[1] = { ILFindLastID(abs) };
        checkhr(IFolderView2_SelectAndPositionItems(fv, 1, none, NULL, SVSI_SELECT), E_INVALIDARG, "an item that is not in the view");
    }
    checkhr(IFolderView2_SelectAndPositionItems(fv, 1, NULL, NULL, SVSI_SELECT), E_INVALIDARG, "items without an array");

    /* SHCreateItemWithParent */
    {
        typedef HRESULT (WINAPI *create_t)(PCIDLIST_ABSOLUTE, IShellFolder *, PCUITEMID_CHILD, REFIID, void **);
        create_t create = (create_t)GetProcAddress(GetModuleHandleW(L"shell32.dll"), "SHCreateItemWithParent");
        IShellItem *it = NULL;
        check(create != NULL, "SHCreateItemWithParent is exported");
        if (create)
        {
            hr = create(NULL, folder, pidl[2], &IID_IShellItem, (void **)&it);
            checkhr(hr, S_OK, "SHCreateItemWithParent(folder, pidl)");
            name = item_name(it);
            check(name && !wcscmp(name, L"c.txt"), "the item is c.txt");
            CoTaskMemFree(name);
            if (it) IShellItem_Release(it);
            it = NULL;
            hr = create(abs, NULL, pidl[0], &IID_IShellItem, (void **)&it);
            checkhr(hr, S_OK, "SHCreateItemWithParent(parent pidl, pidl)");
            name = item_name(it);
            check(name && !wcscmp(name, L"a.txt"), "the item is a.txt");
            CoTaskMemFree(name);
            if (it) IShellItem_Release(it);
            checkhr(create(NULL, NULL, pidl[0], &IID_IShellItem, (void **)&it), E_INVALIDARG, "no parent");
            checkhr(create(NULL, folder, NULL, &IID_IShellItem, (void **)&it), E_INVALIDARG, "no item");
            checkhr(create(NULL, folder, pidl[0], &IID_IShellItem, NULL), E_POINTER, "no result pointer");
        }
    }

    for (i = 0; i < 3; i++) ILFree(pidl[i]);
    IFolderView2_Release(fv);
    IExplorerBrowser_Destroy(peb);
    IExplorerBrowser_Release(peb);
    IShellFolder_Release(folder);
    ILFree(abs);
    DestroyWindow(hwnd);
    remove_files();
    printf("%d checks failed\n", failures);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
