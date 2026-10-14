/* shell32 Recycle Bin folder and its menu (patch 3700) and SHCreateFileExtractIconW */
#define COBJMACROS
#include <windows.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <shobjidl.h>
#include <stdio.h>
#include <wchar.h>

/* the mingw headers declare the C vtable of IExtractIconW without the IUnknown methods */
struct eiw_vtbl
{
    HRESULT (WINAPI *QueryInterface)(void *, REFIID, void **);
    ULONG (WINAPI *AddRef)(void *);
    ULONG (WINAPI *Release)(void *);
    HRESULT (WINAPI *GetIconLocation)(void *, UINT, WCHAR *, UINT, int *, UINT *);
    HRESULT (WINAPI *Extract)(void *, const WCHAR *, UINT, HICON *, HICON *, UINT);
};
#define EIW_VT(p) (*(struct eiw_vtbl **)(p))
static int failed;
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#define CHECK(c, ...) do { if (!(c)) { failed++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static void trash(const WCHAR *path)
{
    WCHAR from[MAX_PATH + 2] = { 0 };
    SHFILEOPSTRUCTW op = { 0 };

    wcscpy(from, path);
    op.wFunc = FO_DELETE;
    op.pFrom = from;
    op.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION | FOF_SILENT | FOF_NOERRORUI;
    CHECK(!SHFileOperationW(&op) && !op.fAnyOperationsAborted, "trash %ls", path);
}

static ITEMIDLIST *find_item(IShellFolder2 *bin, const WCHAR *name)
{
    IEnumIDList *e = NULL;
    ITEMIDLIST *id, *found = NULL;
    STRRET sr;
    WCHAR buf[MAX_PATH];

    if (FAILED(IShellFolder2_EnumObjects(bin, NULL, SHCONTF_NONFOLDERS | SHCONTF_FOLDERS, &e)) || !e) return NULL;
    while (!found && IEnumIDList_Next(e, 1, &id, NULL) == S_OK)
    {
        if (SUCCEEDED(IShellFolder2_GetDisplayNameOf(bin, id, SHGDN_NORMAL, &sr))
                && SUCCEEDED(StrRetToBufW(&sr, id, buf, MAX_PATH)) && !lstrcmpiW(buf, name))
            found = ILClone(id);
        CoTaskMemFree(id);
    }
    IEnumIDList_Release(e);
    return found;
}

static void test_icon(void)
{
    IExtractIconW *ei = NULL;
    IUnknown *unk = NULL;
    WCHAR file[MAX_PATH];
    int index = -1;
    UINT flags = 0;
    HRESULT hr;

    hr = SHCreateFileExtractIconW(L"a.txt", FILE_ATTRIBUTE_NORMAL, &IID_IExtractIconW, (void **)&ei);
    CHECK(hr == S_OK && ei, "SHCreateFileExtractIconW %#lx", hr);
    if (ei)
    {
        hr = EIW_VT(ei)->GetIconLocation(ei, 0, file, MAX_PATH, &index, &flags);
        CHECK(SUCCEEDED(hr) && file[0], "GetIconLocation %#lx %ls", hr, file);
        EIW_VT(ei)->Release(ei);
    }
    hr = SHCreateFileExtractIconW(L"folder", FILE_ATTRIBUTE_DIRECTORY, &IID_IExtractIconA, (void **)&unk);
    CHECK(hr == S_OK && unk, "directory, ANSI %#lx", hr);
    if (unk) IUnknown_Release(unk);
    hr = SHCreateFileExtractIconW(L"a.txt", 0, &IID_IShellFolder, (void **)&unk);
    CHECK(hr == E_NOINTERFACE && !unk, "other interface %#lx", hr);
    hr = SHCreateFileExtractIconW(NULL, 0, &IID_IExtractIconW, (void **)&unk);
    CHECK(hr == E_INVALIDARG, "no file %#lx", hr);
}

int main(void)
{
    WCHAR dir[MAX_PATH], file[MAX_PATH], sub[MAX_PATH], inner[MAX_PATH];
    IShellFolder2 *bin = NULL;
    IShellFolder *desktop = NULL;
    ITEMIDLIST *binpidl = NULL, *item, *dirItem, *p2 = NULL;
    IContextMenu *cm = NULL;
    IContextMenu2 *cm2 = NULL;
    IUnknown *unk;
    IStream *stream = NULL;
    HRESULT hr;
    HANDLE h;
    DWORD n;
    char buf[16] = { 0 };

    setvbuf(stdout, NULL, _IONBF, 0);
    OleInitialize(NULL);
    test_icon();

    GetTempPathW(MAX_PATH, dir);
    lstrcatW(dir, L"sgbin-probe");
    CreateDirectoryW(dir, NULL);
    swprintf(file, MAX_PATH, L"%ls\\sgbin1.txt", dir);
    swprintf(sub, MAX_PATH, L"%ls\\sgbindir", dir);
    swprintf(inner, MAX_PATH, L"%ls\\inside.txt", sub);
    h = CreateFileW(file, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    WriteFile(h, "hello", 5, &n, NULL);
    CloseHandle(h);
    CreateDirectoryW(sub, NULL);
    h = CreateFileW(inner, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    WriteFile(h, "inner", 5, &n, NULL);
    CloseHandle(h);
    trash(file);
    trash(sub);
    swprintf(inner, MAX_PATH, L"%ls\\sgbin2.txt", dir);
    h = CreateFileW(inner, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    WriteFile(h, "twelve bytes", 12, &n, NULL);
    CloseHandle(h);
    trash(inner);

    SHGetDesktopFolder(&desktop);
    hr = SHGetFolderLocation(NULL, CSIDL_BITBUCKET, NULL, 0, &binpidl);
    CHECK(hr == S_OK, "bitbucket location %#lx", hr);
    hr = IShellFolder_BindToObject(desktop, binpidl, NULL, &IID_IShellFolder2, (void **)&bin);
    CHECK(hr == S_OK && bin, "bind to the bin %#lx", hr);
    if (!bin) return 1;

    item = find_item(bin, L"sgbin1.txt");
    dirItem = find_item(bin, L"sgbindir");
    CHECK(item != NULL, "the deleted file is in the bin");
    CHECK(dirItem != NULL, "the deleted folder is in the bin");
    if (!item || !dirItem) return 1;

    /* CompareIDs: the column decides, a tie goes to the name */
    {
        ITEMIDLIST *item2 = find_item(bin, L"sgbin2.txt");

        CHECK(item2 != NULL, "second file in the bin");
        if (item2)
        {
            hr = IShellFolder2_CompareIDs(bin, 0, item, item2);
            CHECK((short)HRESULT_CODE(hr) < 0, "by name, 1 before 2: %#lx", hr);
            hr = IShellFolder2_CompareIDs(bin, 0, item2, item);
            CHECK((short)HRESULT_CODE(hr) > 0, "by name, 2 after 1: %#lx", hr);
            hr = IShellFolder2_CompareIDs(bin, 3, item, item2);
            CHECK((short)HRESULT_CODE(hr) < 0, "by size, 5 before 12: %#lx", hr);
            hr = IShellFolder2_CompareIDs(bin, 3, item2, item);
            CHECK((short)HRESULT_CODE(hr) > 0, "by size, 12 after 5: %#lx", hr);
            hr = IShellFolder2_CompareIDs(bin, 3, item, item);
            CHECK(hr == 0, "an item with itself: %#lx", hr);
            hr = IShellFolder2_CompareIDs(bin, 1, item, item2);
            CHECK(SUCCEEDED(hr), "by the place deleted from: %#lx", hr);
            ILFree(item2);
        }
    }

    /* ParseDisplayName */
    hr = IShellFolder2_ParseDisplayName(bin, NULL, NULL, (WCHAR *)L"sgbin1.txt", &n, &p2, NULL);
    CHECK(hr == S_OK && p2 && n == 10, "ParseDisplayName by name %#lx eaten %lu", hr, n);
    if (p2) { CHECK(IShellFolder2_CompareIDs(bin, 0, p2, item) == 0, "the same item"); ILFree(p2); p2 = NULL; }
    {
        SFGAOF attrs = SFGAO_CANDELETE | SFGAO_FILESYSTEM;
        hr = IShellFolder2_ParseDisplayName(bin, NULL, NULL, file, NULL, &p2, &attrs);
        CHECK(hr == S_OK && p2, "ParseDisplayName by the old path %#lx", hr);
        CHECK(attrs & SFGAO_CANDELETE, "attributes %#lx", attrs);
        if (p2) { ILFree(p2); p2 = NULL; }
    }
    hr = IShellFolder2_ParseDisplayName(bin, NULL, NULL, (WCHAR *)L"no such item.txt", NULL, &p2, NULL);
    CHECK(hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND) && !p2, "missing item %#lx", hr);
    hr = IShellFolder2_ParseDisplayName(bin, NULL, NULL, NULL, NULL, &p2, NULL);
    CHECK(hr == E_INVALIDARG, "no name %#lx", hr);

    /* BindToStorage: the content of the file */
    hr = IShellFolder2_BindToStorage(bin, item, NULL, &IID_IStream, (void **)&stream);
    CHECK(hr == S_OK && stream, "BindToStorage %#lx", hr);
    if (stream)
    {
        ULONG got = 0;
        IStream_Read(stream, buf, 5, &got);
        CHECK(got == 5 && !memcmp(buf, "hello", 5), "stream content %lu '%s'", got, buf);
        IStream_Release(stream);
    }
    unk = NULL;
    hr = IShellFolder2_BindToStorage(bin, item, NULL, &IID_IShellFolder, (void **)&unk);
    CHECK(hr == E_NOINTERFACE && !unk, "BindToStorage as a folder %#lx", hr);
    hr = IShellFolder2_BindToStorage(bin, dirItem, NULL, &IID_IStream, (void **)&unk);
    CHECK(FAILED(hr) && !unk, "a deleted folder has no stream %#lx", hr);

    /* BindToObject: a deleted folder opens, a file does not */
    {
        IShellFolder *sf = NULL;
        IEnumIDList *e = NULL;
        ITEMIDLIST *id;
        int found = 0;

        hr = IShellFolder2_BindToObject(bin, item, NULL, &IID_IShellFolder, (void **)&sf);
        CHECK(FAILED(hr) && !sf, "a file is no folder %#lx", hr);
        hr = IShellFolder2_BindToObject(bin, dirItem, NULL, &IID_IShellFolder, (void **)&sf);
        CHECK(hr == S_OK && sf, "BindToObject of a deleted folder %#lx", hr);
        if (sf)
        {
            hr = IShellFolder_EnumObjects(sf, NULL, SHCONTF_NONFOLDERS | SHCONTF_FOLDERS, &e);
            while (hr == S_OK && IEnumIDList_Next(e, 1, &id, NULL) == S_OK) { found++; CoTaskMemFree(id); }
            CHECK(found == 1, "the deleted folder holds %d item(s)", found);
            if (e) IEnumIDList_Release(e);
            IShellFolder_Release(sf);
        }
    }

    /* GetUIObjectOf and the menu */
    {
        const ITEMIDLIST *items[1] = { item };
        HMENU menu = CreatePopupMenu();
        char name[64];
        WCHAR wname[128];

        hr = IShellFolder2_GetUIObjectOf(bin, NULL, 1, items, &IID_IContextMenu, NULL, (void **)&cm);
        CHECK(hr == S_OK && cm, "GetUIObjectOf IContextMenu %#lx", hr);
        if (cm)
        {
            hr = IContextMenu_QueryInterface(cm, &IID_IContextMenu2, (void **)&cm2);
            CHECK(hr == S_OK && cm2, "the menu has IContextMenu2 %#lx", hr);
            if (cm2) IContextMenu2_Release(cm2);
            hr = IContextMenu_QueryInterface(cm, &IID_IUnknown, (void **)&unk);
            CHECK(hr == S_OK, "the menu has IUnknown %#lx", hr);
            if (unk) IUnknown_Release(unk);
            unk = NULL;
            hr = IContextMenu_QueryInterface(cm, &IID_IShellFolder, (void **)&unk);
            CHECK(hr == E_NOINTERFACE && !unk, "the menu is no folder %#lx", hr);

            hr = IContextMenu_QueryContextMenu(cm, menu, 0, 1, 0x7fff, CMF_DEFAULTONLY);
            CHECK(SUCCEEDED(hr) && HRESULT_CODE(hr) == 0 && GetMenuItemCount(menu) == 0, "default only: %#lx, %d items", hr, GetMenuItemCount(menu));
            hr = IContextMenu_QueryContextMenu(cm, menu, 0, 1, 0x7fff, CMF_NORMAL);
            CHECK(SUCCEEDED(hr) && HRESULT_CODE(hr) > 0 && GetMenuItemCount(menu) == 2, "normal: %#lx, %d items", hr, GetMenuItemCount(menu));

            memset(name, 0, sizeof(name));
            hr = IContextMenu_GetCommandString(cm, 301, GCS_VERBA, NULL, name, sizeof(name));
            CHECK(hr == S_OK && !strcmp(name, "undelete"), "restore verb %#lx '%s'", hr, name);
            hr = IContextMenu_GetCommandString(cm, 302, GCS_VERBW, NULL, (char *)wname, ARRAY_SIZE(wname));
            CHECK(hr == S_OK && !wcscmp(wname, L"delete"), "erase verb %#lx", hr);
            wname[0] = 0;
            hr = IContextMenu_GetCommandString(cm, 301, GCS_HELPTEXTW, NULL, (char *)wname, ARRAY_SIZE(wname));
            CHECK(hr == S_OK && wname[0], "restore help %#lx", hr);
            hr = IContextMenu_GetCommandString(cm, 302, GCS_HELPTEXTA, NULL, name, sizeof(name));
            CHECK(hr == S_OK && name[0], "erase help %#lx", hr);
            hr = IContextMenu_GetCommandString(cm, 301, GCS_VALIDATEA, NULL, NULL, 0);
            CHECK(hr == S_OK, "validate 301 %#lx", hr);
            hr = IContextMenu_GetCommandString(cm, 5, GCS_VALIDATEA, NULL, NULL, 0);
            CHECK(hr == S_FALSE, "validate an unknown command %#lx", hr);
            hr = IContextMenu_GetCommandString(cm, (UINT_PTR)"undelete", GCS_VALIDATEA, NULL, NULL, 0);
            CHECK(hr == S_OK, "validate a verb name %#lx", hr);
            hr = IContextMenu_GetCommandString(cm, 5, GCS_VERBA, NULL, name, sizeof(name));
            CHECK(hr == E_INVALIDARG, "verb of an unknown command %#lx", hr);

            hr = IContextMenu_QueryInterface(cm, &IID_IContextMenu2, (void **)&cm2);
            if (cm2)
            {
                hr = IContextMenu2_HandleMenuMsg(cm2, WM_INITMENUPOPUP, (WPARAM)menu, 0);
                CHECK(hr == S_OK, "HandleMenuMsg init popup %#lx", hr);
                hr = IContextMenu2_HandleMenuMsg(cm2, WM_DRAWITEM, 0, 0);
                CHECK(hr == S_OK, "HandleMenuMsg draw item %#lx", hr);
                hr = IContextMenu2_HandleMenuMsg(cm2, WM_USER + 5, 0, 0);
                CHECK(hr == E_NOTIMPL, "HandleMenuMsg other message %#lx", hr);
                IContextMenu2_Release(cm2);
            }
            IContextMenu_Release(cm);
        }
        DestroyMenu(menu);

        /* a menu for no item does not crash */
        cm = NULL;
        hr = IShellFolder2_GetUIObjectOf(bin, NULL, 0, NULL, &IID_IContextMenu, NULL, (void **)&cm);
        CHECK(hr == S_OK && cm, "menu for no item %#lx", hr);
        if (cm) IContextMenu_Release(cm);

        /* the icon of the item and a refusal */
        {
            IExtractIconW *ei = NULL;
            WCHAR loc[MAX_PATH];
            int idx;
            UINT fl;

            hr = IShellFolder2_GetUIObjectOf(bin, NULL, 1, items, &IID_IExtractIconW, NULL, (void **)&ei);
            CHECK(hr == S_OK && ei, "icon of the item %#lx", hr);
            if (ei)
            {
                hr = EIW_VT(ei)->GetIconLocation(ei, 0, loc, MAX_PATH, &idx, &fl);
                CHECK(SUCCEEDED(hr), "icon location %#lx", hr);
                EIW_VT(ei)->Release(ei);
            }
            unk = NULL;
            hr = IShellFolder2_GetUIObjectOf(bin, NULL, 1, items, &IID_IShellLinkW, NULL, (void **)&unk);
            CHECK(hr == E_NOINTERFACE && !unk, "other UI object %#lx", hr);
        }
    }

    /* GetDetailsEx */
    {
        VARIANT v;
        SHCOLUMNID id;
        int i;
        UINT col_size = 3, col_name = 0, col_del = 2, col_from = 1;

        for (i = 0; i < 6; i++)
        {
            VariantInit(&v);
            hr = IShellFolder2_MapColumnToSCID(bin, i, &id);
            CHECK(hr == S_OK, "MapColumnToSCID %d %#lx", i, hr);
            hr = IShellFolder2_GetDetailsEx(bin, item, &id, &v);
            CHECK(hr == S_OK, "GetDetailsEx column %d: %#lx", i, hr);
            if (i == col_size) CHECK(V_VT(&v) == VT_UI8 && V_UI8(&v) == 5, "size: vt %d", V_VT(&v));
            else if (i == col_del) CHECK(V_VT(&v) == VT_DATE && V_DATE(&v) > 40000.0, "date deleted: vt %d %f", V_VT(&v), V_DATE(&v));
            else if (i == col_name) CHECK(V_VT(&v) == VT_BSTR && !wcscmp(V_BSTR(&v), L"sgbin1.txt"), "name: vt %d", V_VT(&v));
            else if (i == col_from) CHECK(V_VT(&v) == VT_BSTR && V_BSTR(&v) && wcsstr(V_BSTR(&v), L"sgbin-probe"), "deleted from: vt %d", V_VT(&v));
            VariantClear(&v);
        }
        id.fmtid = GUID_NULL;
        id.pid = 99;
        VariantInit(&v);
        hr = IShellFolder2_GetDetailsEx(bin, item, &id, &v);
        CHECK(FAILED(hr), "unknown column %#lx", hr);
    }

    /* SHUpdateRecycleBinIcon notifies the views of the bin */
    {
        typedef HRESULT (WINAPI *update_fn)(void);
        update_fn update = (update_fn)GetProcAddress(GetModuleHandleA("shell32.dll"), "SHUpdateRecycleBinIcon");

        CHECK(update != NULL, "SHUpdateRecycleBinIcon export");
        if (update) CHECK(update() == S_OK, "SHUpdateRecycleBinIcon");
    }

    ILFree(item);
    ILFree(dirItem);
    IShellFolder2_Release(bin);
    IShellFolder_Release(desktop);
    ILFree(binpidl);
    {
        SHEmptyRecycleBinW(NULL, NULL, SHERB_NOCONFIRMATION | SHERB_NOPROGRESSUI | SHERB_NOSOUND);
        RemoveDirectoryW(dir);
    }
    printf(failed ? "RESULT: FAIL (%d)\n" : "RESULT: PASS\n", failed);
    return failed != 0;
}
