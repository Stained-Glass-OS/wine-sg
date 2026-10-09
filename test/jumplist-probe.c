/* Jump lists (patches/sg/1664), run by test/jumplist-gate.sh in the
 * shell's desktop:
 *  - EnumerableObjectCollection (IObjectCollection/IObjectArray);
 *  - ICustomDestinationList: the order of calls (E_UNEXPECTED outside
 *    BeginList/CommitList), a custom category of a document and a link,
 *    the Recent known category, tasks (with a separator), what is refused
 *    (links-only tasks, unknown categories), CommitList, DeleteList;
 *  - SHAddToRecentDocs with SHARD_APPIDINFO puts documents on the
 *    program's list: IApplicationDocumentLists gives them most recent or
 *    most used first; IApplicationDestinations removes one, then all;
 *  - the taskbar's view (shell32's SGJumpList*): headings, entries and the
 *    separator in order, and a task run from it;
 *  - the taskbar: the program's button's menu has the list, and choosing
 *    a task (by its access key) runs it.
 * These were stubs (E_NOTIMPL; no EnumerableObjectCollection at all). */
#define COBJMACROS
#include <windows.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <propkey.h>
#include <propvarutil.h>
#include <stdio.h>

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

static const CLSID CLSID_EOC = {0x2d3468c1,0x36a7,0x43b6,{0xac,0x24,0xd3,0xf0,0x2f,0xd9,0x60,0x7a}};
static const CLSID CLSID_DestList = {0x77f10cf0,0x3db5,0x4966,{0xb5,0x20,0xb7,0xc5,0x4f,0xd3,0x5e,0xd6}};
static const CLSID CLSID_AppDest = {0x86c14003,0x4d6b,0x4ef3,{0xa7,0xb4,0x05,0x06,0x66,0x3b,0x2e,0x68}};
static const CLSID CLSID_AppDocs = {0x86bec222,0x30f2,0x47e0,{0x9f,0x25,0x60,0xd1,0x1c,0xd7,0x5c,0x28}};

#define APPID L"StainedGlass.JumpListProbe"

static WCHAR self[MAX_PATH], dir[MAX_PATH];

static IShellItem *item_for(const WCHAR *path)
{
    IShellItem *item = NULL;
    SHCreateItemFromParsingName(path, NULL, &IID_IShellItem, (void **)&item);
    return item;
}

static void make_file(const WCHAR *path)
{
    HANDLE h = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    DWORD written;
    WriteFile(h, "doc", 3, &written, NULL);
    CloseHandle(h);
}

static IShellLinkW *task(const WCHAR *title, const WCHAR *args, BOOL separator)
{
    IShellLinkW *link;
    IPropertyStore *store;
    PROPVARIANT pv;

    CoCreateInstance(&CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, &IID_IShellLinkW, (void **)&link);
    IShellLinkW_SetPath(link, self);
    if (args) IShellLinkW_SetArguments(link, args);
    IShellLinkW_QueryInterface(link, &IID_IPropertyStore, (void **)&store);
    if (separator)
    {
        PropVariantInit(&pv);
        pv.vt = VT_BOOL;
        pv.boolVal = VARIANT_TRUE;
        IPropertyStore_SetValue(store, &PKEY_AppUserModel_IsDestListSeparator, &pv);
    }
    else
    {
        PropVariantInit(&pv);
        pv.vt = VT_LPWSTR;
        pv.pwszVal = (WCHAR *)title;
        IPropertyStore_SetValue(store, &PKEY_Title, &pv);
    }
    IPropertyStore_Commit(store);
    IPropertyStore_Release(store);
    return link;
}

static UINT array_count(IUnknown *array)
{
    IObjectArray *a;
    UINT n = 0;
    if (SUCCEEDED(IUnknown_QueryInterface(array, &IID_IObjectArray, (void **)&a)))
    {
        IObjectArray_GetCount(a, &n);
        IObjectArray_Release(a);
    }
    return n;
}

static BOOL first_is(IObjectArray *a, const WCHAR *path)
{
    IShellItem *item;
    WCHAR *name;
    BOOL ok = FALSE;

    if (FAILED(IObjectArray_GetAt(a, 0, &IID_IShellItem, (void **)&item))) return FALSE;
    if (SUCCEEDED(IShellItem_GetDisplayName(item, SIGDN_FILESYSPATH, &name)))
    {
        ok = !lstrcmpiW(name, path);
        CoTaskMemFree(name);
    }
    IShellItem_Release(item);
    return ok;
}

static void add_recent(const WCHAR *path)
{
    SHARDAPPIDINFO info;
    info.psi = item_for(path);
    info.pszAppID = APPID;
    SHAddToRecentDocs(SHARD_APPIDINFO, &info);
    IShellItem_Release(info.psi);
}

static BOOL wait_file(const WCHAR *path, DWORD ms)
{
    DWORD end = GetTickCount() + ms;
    MSG msg;
    while ((LONG)(end - GetTickCount()) > 0)
    {
        if (GetFileAttributesW(path) != INVALID_FILE_ATTRIBUTES) return TRUE;
        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) DispatchMessageW(&msg);
        Sleep(100);
    }
    return GetFileAttributesW(path) != INVALID_FILE_ATTRIBUTES;
}

static HWND found_button;
static BOOL CALLBACK find_button(HWND child, LPARAM lp)
{
    if ((HWND)GetWindowLongPtrW(child, GWLP_ID) == (HWND)lp && IsWindowVisible(child)) { found_button = child; return FALSE; }
    return TRUE;
}

int wmain(int argc, WCHAR **argv)
{
    WCHAR doc1[MAX_PATH], doc2[MAX_PATH], doc3[MAX_PATH], marker[MAX_PATH], args[MAX_PATH * 2], title[MAX_PATH];
    HRESULT (WINAPI *jl_load)(const WCHAR *, const WCHAR *, void **);
    UINT (WINAPI *jl_count)(void *);
    BOOL (WINAPI *jl_entry)(void *, UINT, int *, WCHAR *, UINT);
    HRESULT (WINAPI *jl_invoke)(void *, UINT, HWND);
    void (WINAPI *jl_free)(void *);
    IObjectCollection *coll, *items, *tasks;
    ICustomDestinationList *list;
    IApplicationDocumentLists *docs;
    IApplicationDestinations *dests;
    IObjectArray *removed = NULL, *got;
    IShellItem *item;
    IShellLinkW *link, *t1, *t2, *t3;
    IUnknown *unk;
    void *view = NULL;
    UINT n = 0, slots = 0, i;
    int kind;
    HMODULE shell32;
    HRESULT hr;
    HWND hwnd, tray, menu;

    if (argc > 2 && !lstrcmpW(argv[1], L"marker"))
    {
        make_file(argv[2]);
        return 0;
    }
    SetCurrentProcessExplicitAppUserModelID(APPID);
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    GetModuleFileNameW(NULL, self, MAX_PATH);
    GetCurrentDirectoryW(MAX_PATH, dir);
    if (dir[0] && dir[lstrlenW(dir) - 1] == '\\') dir[lstrlenW(dir) - 1] = 0;
    swprintf(doc1, MAX_PATH, L"%ls\\jl-one.txt", dir);
    swprintf(doc2, MAX_PATH, L"%ls\\jl-two.txt", dir);
    swprintf(doc3, MAX_PATH, L"%ls\\jl-three.txt", dir);
    make_file(doc1); make_file(doc2); make_file(doc3);

    /* the collection */
    hr = CoCreateInstance(&CLSID_EOC, NULL, CLSCTX_INPROC_SERVER, &IID_IObjectCollection, (void **)&coll);
    check(hr == S_OK, "EnumerableObjectCollection is created");
    if (FAILED(hr)) goto done;
    item = item_for(doc1);
    IObjectCollection_AddObject(coll, (IUnknown *)item);
    IObjectCollection_AddObject(coll, (IUnknown *)item);
    IObjectCollection_GetCount(coll, &n);
    check(n == 2, "two objects added");
    check(IObjectCollection_GetAt(coll, 1, &IID_IShellItem, (void **)&unk) == S_OK && unk == (IUnknown *)item,
          "GetAt gives them back");
    IUnknown_Release(unk);
    check(IObjectCollection_GetAt(coll, 2, &IID_IUnknown, (void **)&unk) == E_INVALIDARG, "past the end: E_INVALIDARG");
    IObjectCollection_RemoveObjectAt(coll, 0);
    IObjectCollection_GetCount(coll, &n);
    check(n == 1, "RemoveObjectAt");
    IObjectCollection_Clear(coll);
    IObjectCollection_GetCount(coll, &n);
    check(n == 0, "Clear");
    IShellItem_Release(item);

    /* the custom list */
    hr = CoCreateInstance(&CLSID_DestList, NULL, CLSCTX_INPROC_SERVER, &IID_ICustomDestinationList, (void **)&list);
    check(hr == S_OK, "DestinationList is created");
    if (FAILED(hr)) goto done;
    check(ICustomDestinationList_SetAppID(list, APPID) == S_OK, "SetAppID");
    check(ICustomDestinationList_AppendKnownCategory(list, KDC_RECENT) == E_UNEXPECTED,
          "appending before BeginList: E_UNEXPECTED");
    check(ICustomDestinationList_CommitList(list) == E_UNEXPECTED, "committing before BeginList: E_UNEXPECTED");
    hr = ICustomDestinationList_BeginList(list, &slots, &IID_IObjectArray, (void **)&removed);
    check(hr == S_OK && slots >= 1 && removed && array_count((IUnknown *)removed) == 0,
          "BeginList: slots, and nothing removed");
    if (removed) IObjectArray_Release(removed);

    CoCreateInstance(&CLSID_EOC, NULL, CLSCTX_INPROC_SERVER, &IID_IObjectCollection, (void **)&items);
    item = item_for(doc1);
    IObjectCollection_AddObject(items, (IUnknown *)item);
    link = task(L"Link destination", L"marker nothing", FALSE);
    IObjectCollection_AddObject(items, (IUnknown *)link);
    check(ICustomDestinationList_AppendCategory(list, L"Pinned documents", (IObjectArray *)items) == S_OK,
          "a custom category of a document and a link");
    check(ICustomDestinationList_AppendCategory(list, NULL, (IObjectArray *)items) == E_INVALIDARG,
          "a category without a name: E_INVALIDARG");
    check(ICustomDestinationList_AppendKnownCategory(list, KDC_RECENT) == S_OK, "the Recent category");
    check(ICustomDestinationList_AppendKnownCategory(list, 7) == E_INVALIDARG, "an unknown category: E_INVALIDARG");
    check(ICustomDestinationList_AddUserTasks(list, (IObjectArray *)items) == E_INVALIDARG,
          "tasks are links only: E_INVALIDARG");

    swprintf(marker, MAX_PATH, L"%ls\\jl-task-ran", dir);
    swprintf(args, ARRAYSIZE(args), L"marker \"%ls\"", marker);
    CoCreateInstance(&CLSID_EOC, NULL, CLSCTX_INPROC_SERVER, &IID_IObjectCollection, (void **)&tasks);
    t1 = task(L"Run the &Zebra task", args, FALSE);
    t2 = task(NULL, NULL, TRUE);
    t3 = task(L"Second task", L"marker other", FALSE);
    IObjectCollection_AddObject(tasks, (IUnknown *)t1);
    IObjectCollection_AddObject(tasks, (IUnknown *)t2);
    IObjectCollection_AddObject(tasks, (IUnknown *)t3);
    check(ICustomDestinationList_AddUserTasks(list, (IObjectArray *)tasks) == S_OK, "three tasks, one a separator");
    check(ICustomDestinationList_CommitList(list) == S_OK, "CommitList");
    check(ICustomDestinationList_CommitList(list) == E_UNEXPECTED, "committed: the list is closed");

    /* the program's documents */
    add_recent(doc2);
    add_recent(doc3);
    Sleep(20);
    add_recent(doc2);
    add_recent(doc2);
    Sleep(20);
    add_recent(doc1);
    hr = CoCreateInstance(&CLSID_AppDocs, NULL, CLSCTX_INPROC_SERVER, &IID_IApplicationDocumentLists, (void **)&docs);
    check(hr == S_OK, "ApplicationDocumentLists is created");
    if (FAILED(hr)) goto done;
    IApplicationDocumentLists_SetAppID(docs, APPID);
    hr = IApplicationDocumentLists_GetList(docs, ADLT_RECENT, 0, &IID_IObjectArray, (void **)&got);
    check(hr == S_OK && array_count((IUnknown *)got) == 3 && first_is(got, doc1),
          "Recent: three documents, the last opened first");
    if (hr == S_OK) IObjectArray_Release(got);
    hr = IApplicationDocumentLists_GetList(docs, ADLT_FREQUENT, 1, &IID_IObjectArray, (void **)&got);
    check(hr == S_OK && array_count((IUnknown *)got) == 1 && first_is(got, doc2),
          "Frequent, one asked for: the most opened");
    if (hr == S_OK) IObjectArray_Release(got);

    hr = CoCreateInstance(&CLSID_AppDest, NULL, CLSCTX_INPROC_SERVER, &IID_IApplicationDestinations, (void **)&dests);
    check(hr == S_OK, "ApplicationDestinations is created");
    if (FAILED(hr)) goto done;
    IApplicationDestinations_SetAppID(dests, APPID);
    check(IApplicationDestinations_RemoveDestination(dests, (IUnknown *)item) == S_OK, "RemoveDestination");
    IApplicationDocumentLists_GetList(docs, ADLT_RECENT, 0, &IID_IObjectArray, (void **)&got);
    check(array_count((IUnknown *)got) == 2 && first_is(got, doc2), "the removed document is gone from Recent");
    IObjectArray_Release(got);

    /* the taskbar's view */
    shell32 = GetModuleHandleW(L"shell32.dll");
    jl_load = (void *)GetProcAddress(shell32, "SGJumpListLoad");
    jl_count = (void *)GetProcAddress(shell32, "SGJumpListCount");
    jl_entry = (void *)GetProcAddress(shell32, "SGJumpListEntry");
    jl_invoke = (void *)GetProcAddress(shell32, "SGJumpListInvoke");
    jl_free = (void *)GetProcAddress(shell32, "SGJumpListFree");
    check(jl_load && jl_count && jl_entry && jl_invoke && jl_free, "the taskbar's view");
    if (!jl_load) goto done;
    check(jl_load(APPID, self, &view) == S_OK && view, "the list loads");
    n = jl_count(view);
    {
        static const struct { int kind; const WCHAR *title; } want[] =
        {
            { 0, L"Pinned documents" }, { 1, L"jl-one.txt" }, { 1, L"Link destination" },
            { 0, L"Recent" }, { 1, L"jl-two.txt" }, { 1, L"jl-three.txt" },
            { 0, L"Tasks" }, { 1, L"Run the &Zebra task" }, { 2, L"" }, { 1, L"Second task" },
        };
        BOOL same = n == ARRAYSIZE(want);
        for (i = 0; same && i < n; i++)
        {
            title[0] = 0;
            jl_entry(view, i, &kind, title, ARRAYSIZE(title));
            if (kind != want[i].kind || (kind != 2 && wcsncmp(title, want[i].title, wcslen(want[i].title))))
            {
                printf("      entry %u: %d %ls\n", i, kind, title);
                same = FALSE;
            }
        }
        check(same, "headings, documents, links, tasks and the separator, in order");
    }
    DeleteFileW(marker);
    check(jl_invoke(view, 7, NULL) == S_OK && wait_file(marker, 15000), "a task runs from the view");
    jl_free(view);

    /* the taskbar: the button's menu */
    DeleteFileW(marker);
    hwnd = CreateWindowExW(0, L"static", L"Jump list probe", WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                           100, 100, 300, 200, NULL, NULL, NULL, NULL);
    tray = FindWindowW(L"Shell_TrayWnd", NULL);
    for (i = 0; i < 50 && !found_button; i++)
    {
        MSG msg;
        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) DispatchMessageW(&msg);
        if (tray) EnumChildWindows(tray, find_button, (LPARAM)hwnd);
        Sleep(100);
    }
    check(found_button != NULL, "the window has a taskbar button");
    if (found_button)
    {
        PostMessageW(tray, WM_CONTEXTMENU, (WPARAM)found_button, -1);
        for (i = 0, menu = NULL; i < 50 && !menu; i++)
        {
            Sleep(100);
            menu = FindWindowW(L"#32768", NULL);
        }
        check(menu != NULL, "its menu opens");
        if (menu)
        {
            Sleep(300);
            PostMessageW(menu, WM_CHAR, 'z', 0);
            check(wait_file(marker, 15000), "choosing the task in the button's menu runs it");
        }
    }

    check(IApplicationDestinations_RemoveAllDestinations(dests) == S_OK, "RemoveAllDestinations");
    IApplicationDocumentLists_GetList(docs, ADLT_RECENT, 0, &IID_IObjectArray, (void **)&got);
    check(array_count((IUnknown *)got) == 0, "no documents left");
    IObjectArray_Release(got);
    check(ICustomDestinationList_DeleteList(list, APPID) == S_OK, "DeleteList");
    jl_load(APPID, self, &view);
    check(jl_count(view) == 0, "nothing left in the view");
    jl_free(view);

done:
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
