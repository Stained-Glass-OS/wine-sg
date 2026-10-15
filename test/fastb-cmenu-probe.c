/* shell32 context menu helpers: IShellExtInit::Initialize and IObjectWithSite of the item menu, the New menu,
 * the background menu's help text (patch 3702) */
#define COBJMACROS
#include <windows.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <shobjidl.h>
#include <stdio.h>
#include <wchar.h>

static int failed;
#define CHECK(c, ...) do { if (!(c)) { failed++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

/* the mingw headers leave the IUnknown methods out of some C vtables: call by slot */
struct cm_vtbl
{
    HRESULT (WINAPI *QueryInterface)(void *, REFIID, void **);
    ULONG (WINAPI *AddRef)(void *);
    ULONG (WINAPI *Release)(void *);
    HRESULT (WINAPI *QueryContextMenu)(void *, HMENU, UINT, UINT, UINT, UINT);
    HRESULT (WINAPI *InvokeCommand)(void *, void *);
    HRESULT (WINAPI *GetCommandString)(void *, UINT_PTR, UINT, UINT *, char *, UINT);
    HRESULT (WINAPI *HandleMenuMsg)(void *, UINT, WPARAM, LPARAM);
    HRESULT (WINAPI *HandleMenuMsg2)(void *, UINT, WPARAM, LPARAM, LRESULT *);
};
#define CM(p) (*(struct cm_vtbl **)(p))

/* a site that counts its references */
struct site { IUnknown IUnknown_iface; LONG ref; };
static HRESULT WINAPI site_QI(IUnknown *i, REFIID iid, void **out)
{
    if (IsEqualGUID(iid, &IID_IUnknown)) { *out = i; IUnknown_AddRef(i); return S_OK; }
    *out = NULL;
    return E_NOINTERFACE;
}
static ULONG WINAPI site_AddRef(IUnknown *i) { return InterlockedIncrement(&CONTAINING_RECORD(i, struct site, IUnknown_iface)->ref); }
static ULONG WINAPI site_Release(IUnknown *i) { return InterlockedDecrement(&CONTAINING_RECORD(i, struct site, IUnknown_iface)->ref); }
static const IUnknownVtbl site_vtbl = { site_QI, site_AddRef, site_Release };

static void test_site(IObjectWithSite *ows, const char *what)
{
    struct site site = { { (IUnknownVtbl *)&site_vtbl }, 1 };
    IUnknown *got = NULL;
    HRESULT hr;

    hr = IObjectWithSite_GetSite(ows, &IID_IUnknown, (void **)&got);
    CHECK(hr == E_FAIL && !got, "%s: GetSite before SetSite %#lx", what, hr);
    hr = IObjectWithSite_GetSite(ows, &IID_IUnknown, NULL);
    CHECK(hr == E_POINTER, "%s: GetSite with no pointer %#lx", what, hr);
    hr = IObjectWithSite_SetSite(ows, &site.IUnknown_iface);
    CHECK(hr == S_OK, "%s: SetSite %#lx", what, hr);
    CHECK(site.ref == 2, "%s: the menu holds the site: ref %ld", what, site.ref);
    hr = IObjectWithSite_GetSite(ows, &IID_IUnknown, (void **)&got);
    CHECK(hr == S_OK && got == &site.IUnknown_iface, "%s: GetSite %#lx %p", what, hr, got);
    if (got) IUnknown_Release(got);
    hr = IObjectWithSite_GetSite(ows, &IID_IShellFolder, (void **)&got);
    CHECK(hr == E_NOINTERFACE && !got, "%s: GetSite for an interface the site lacks %#lx", what, hr);
    hr = IObjectWithSite_SetSite(ows, NULL);
    CHECK(hr == S_OK && site.ref == 1, "%s: SetSite(NULL) lets go: %#lx ref %ld", what, hr, site.ref);
    hr = IObjectWithSite_GetSite(ows, &IID_IUnknown, (void **)&got);
    CHECK(hr == E_FAIL, "%s: GetSite after SetSite(NULL) %#lx", what, hr);
}

/* the file the "copy" command of the menu puts on the clipboard */
static BOOL copied_name(IContextMenu *cm, WCHAR *name)
{
    CMINVOKECOMMANDINFO ci = { sizeof(ci), 0, NULL, "copy", NULL, NULL, SW_SHOWNORMAL };
    FORMATETC fmt = { CF_HDROP, NULL, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
    IDataObject *clip = NULL;
    STGMEDIUM med;
    BOOL ok = FALSE;

    name[0] = 0;
    OleSetClipboard(NULL);
    if (FAILED(CM(cm)->InvokeCommand(cm, &ci))) return FALSE;
    if (SUCCEEDED(OleGetClipboard(&clip)) && clip)
    {
        if (SUCCEEDED(IDataObject_GetData(clip, &fmt, &med)))
        {
            ok = DragQueryFileW(med.hGlobal, 0, name, MAX_PATH) > 0;
            ReleaseStgMedium(&med);
        }
        IDataObject_Release(clip);
    }
    return ok;
}

int main(void)
{
    static const GUID clsid_newmenu = { 0xd969a300, 0xe7ff, 0x11d0, { 0xa9, 0x3b, 0x00, 0xa0, 0xc9, 0x0f, 0x27, 0x19 } };
    WCHAR dir[MAX_PATH], file[MAX_PATH], file2[MAX_PATH];
    IShellFolder *desktop = NULL, *sf = NULL;
    ITEMIDLIST *dirpidl, *item, *item2 = NULL;
    IEnumIDList *e = NULL;
    IContextMenu *cm = NULL;
    IShellExtInit *init = NULL;
    IObjectWithSite *ows = NULL;
    IDataObject *data = NULL;
    HRESULT hr;
    HANDLE h;
    HMENU menu;
    DWORD n;

    setvbuf(stdout, NULL, _IONBF, 0);
    OleInitialize(NULL);
    SHGetDesktopFolder(&desktop);

    GetTempPathW(MAX_PATH, dir);
    lstrcatW(dir, L"sgcmenu-probe");
    CreateDirectoryW(dir, NULL);
    swprintf(file, MAX_PATH, L"%ls\\one.txt", dir);
    swprintf(file2, MAX_PATH, L"%ls\\two.txt", dir);
    h = CreateFileW(file, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL); WriteFile(h, "1", 1, &n, NULL); CloseHandle(h);
    h = CreateFileW(file2, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL); WriteFile(h, "2", 1, &n, NULL); CloseHandle(h);
    dirpidl = ILCreateFromPathW(dir);
    hr = IShellFolder_BindToObject(desktop, dirpidl, NULL, &IID_IShellFolder, (void **)&sf);
    CHECK(hr == S_OK && sf, "bind the directory %#lx", hr);
    if (!sf) return 1;
    IShellFolder_EnumObjects(sf, NULL, SHCONTF_NONFOLDERS, &e);
    IEnumIDList_Next(e, 1, &item, NULL);
    IEnumIDList_Next(e, 1, &item2, NULL);
    IEnumIDList_Release(e);
    CHECK(item && item2, "two files");
    if (item && item2)
    {
        STRRET sr;
        WCHAR first[MAX_PATH] = L"";

        if (SUCCEEDED(IShellFolder_GetDisplayNameOf(sf, item, SHGDN_INFOLDER, &sr))) StrRetToBufW(&sr, item, first, MAX_PATH);
        if (!lstrcmpiW(first, L"two.txt")) { ITEMIDLIST *t = item; item = item2; item2 = t; }
    }

    /* the item menu: IObjectWithSite */
    {
        const ITEMIDLIST *items[1] = { item };

        hr = IShellFolder_GetUIObjectOf(sf, NULL, 1, items, &IID_IContextMenu, NULL, (void **)&cm);
        CHECK(hr == S_OK && cm, "item menu %#lx", hr);
        hr = IUnknown_QueryInterface(cm, &IID_IObjectWithSite, (void **)&ows);
        CHECK(hr == S_OK && ows, "item menu has IObjectWithSite %#lx", hr);
        if (ows) { test_site(ows, "item menu"); IObjectWithSite_Release(ows); }

        /* IShellExtInit::Initialize */
        hr = IUnknown_QueryInterface(cm, &IID_IShellExtInit, (void **)&init);
        CHECK(hr == S_OK && init, "item menu has IShellExtInit %#lx", hr);
        if (init)
        {
            const ITEMIDLIST *items2[1] = { item2 };

            hr = IShellExtInit_Initialize(init, NULL, NULL, NULL);
            CHECK(hr == E_INVALIDARG, "Initialize with nothing %#lx", hr);
            hr = IShellFolder_GetUIObjectOf(sf, NULL, 1, items2, &IID_IDataObject, NULL, (void **)&data);
            /* (the first file is the one the menu was made for) */
            CHECK(hr == S_OK && data, "data object of the second file %#lx", hr);
            WCHAR copied[MAX_PATH];

            CHECK(copied_name(cm, copied) && StrStrIW(copied, L"one.txt"), "the menu is for the first file: %ls", copied);
            hr = IShellExtInit_Initialize(init, NULL, data, NULL);
            CHECK(hr == S_OK, "Initialize with a data object %#lx", hr);
            CHECK(copied_name(cm, copied) && StrStrIW(copied, L"two.txt"), "the menu is now for the second file: %ls", copied);
            menu = CreatePopupMenu();
            hr = CM(cm)->QueryContextMenu(cm, menu, 0, 1, 0x7fff, CMF_NORMAL);
            CHECK(SUCCEEDED(hr) && GetMenuItemCount(menu) > 2, "menu of the new item: %#lx, %d items", hr, GetMenuItemCount(menu));
            DestroyMenu(menu);
            hr = IShellExtInit_Initialize(init, dirpidl, NULL, NULL);
            CHECK(hr == S_OK, "Initialize with a folder %#lx", hr);
            if (data) IDataObject_Release(data);
            IShellExtInit_Release(init);
        }
        IUnknown_Release(cm);
    }

    /* the background menu: help text */
    cm = NULL;
    hr = IShellFolder_CreateViewObject(sf, NULL, &IID_IContextMenu, (void **)&cm);
    CHECK(hr == S_OK && cm, "background menu %#lx", hr);
    if (cm)
    {
        int i, seen = 0;
        WCHAR verb[64], help[128];

        menu = CreatePopupMenu();
        hr = CM(cm)->QueryContextMenu(cm, menu, 0, 1, 0x7fff, CMF_NORMAL);
        CHECK(SUCCEEDED(hr), "background QueryContextMenu %#lx", hr);
        for (i = 0; i < 64; i++)
        {
            UINT id = GetMenuItemID(menu, i);

            if (id == (UINT)-1 || id == 0) continue;
            verb[0] = 0;
            if (CM(cm)->GetCommandString(cm, id - 1, GCS_VERBW, NULL, (char *)verb, 64) != S_OK) continue;
            if (wcscmp(verb, L"paste") && wcscmp(verb, L"properties")) continue;
            help[0] = 0;
            hr = CM(cm)->GetCommandString(cm, id - 1, GCS_HELPTEXTW, NULL, (char *)help, 128);
            CHECK(hr == S_OK && help[0], "help text of %ls: %#lx", verb, hr);
            seen++;
        }
        CHECK(seen >= 1, "found a command with a help text (%d)", seen);
        hr = CM(cm)->GetCommandString(cm, 0x3fff, GCS_HELPTEXTW, NULL, (char *)help, 128);
        CHECK(FAILED(hr), "help text of an unknown command %#lx", hr);
        DestroyMenu(menu);
        IUnknown_Release(cm);
    }

    /* the New menu */
    hr = CoCreateInstance(&clsid_newmenu, NULL, CLSCTX_INPROC_SERVER, &IID_IShellExtInit, (void **)&init);
    CHECK(hr == S_OK && init, "create the New menu %#lx", hr);
    if (init)
    {
        void *nm = NULL;
        LRESULT res = 7;
        WCHAR text[64] = { 0 };

        hr = IShellExtInit_Initialize(init, dirpidl, NULL, NULL);
        CHECK(hr == S_OK, "New menu Initialize %#lx", hr);
        IShellExtInit_QueryInterface(init, &IID_IObjectWithSite, (void **)&ows);
        CHECK(ows != NULL, "New menu has IObjectWithSite");
        if (ows) { test_site(ows, "New menu"); IObjectWithSite_Release(ows); }
        IShellExtInit_QueryInterface(init, &IID_IContextMenu3, &nm);
        CHECK(nm != NULL, "New menu has IContextMenu3");
        if (nm)
        {
            menu = CreatePopupMenu();
            hr = CM(nm)->QueryContextMenu(nm, menu, 0, 1, 0x7fff, CMF_NORMAL);
            CHECK(SUCCEEDED(hr), "New menu QueryContextMenu %#lx", hr);
            hr = CM(nm)->GetCommandString(nm, 1, GCS_VALIDATEW, NULL, NULL, 0);
            if (hr == S_OK)
            {
                hr = CM(nm)->GetCommandString(nm, 1, GCS_HELPTEXTW, NULL, (char *)text, 64);
                CHECK(hr == S_OK && text[0], "New menu help text %#lx", hr);
            }
            hr = CM(nm)->GetCommandString(nm, 40, GCS_VALIDATEW, NULL, NULL, 0);
            CHECK(hr == S_FALSE, "an unknown New command is not valid %#lx", hr);
            hr = CM(nm)->GetCommandString(nm, 40, GCS_HELPTEXTW, NULL, (char *)text, 64);
            CHECK(hr == E_INVALIDARG, "help text of an unknown New command %#lx", hr);
            hr = CM(nm)->HandleMenuMsg(nm, WM_INITMENUPOPUP, 0, 0);
            CHECK(hr == S_OK, "New menu HandleMenuMsg %#lx", hr);
            hr = CM(nm)->HandleMenuMsg2(nm, WM_MENUCHAR, 0, 0, &res);
            CHECK(hr == S_OK && res == MAKELRESULT(0, MNC_IGNORE), "New menu WM_MENUCHAR %#lx result %#Ix", hr, res);
            hr = CM(nm)->HandleMenuMsg(nm, WM_USER + 9, 0, 0);
            CHECK(hr == E_NOTIMPL, "New menu other message %#lx", hr);
            DestroyMenu(menu);
            CM(nm)->Release(nm);
        }
        IShellExtInit_Release(init);
    }

    ILFree(item);
    ILFree(item2);
    DeleteFileW(file);
    DeleteFileW(file2);
    RemoveDirectoryW(dir);
    ILFree(dirpidl);
    IShellFolder_Release(sf);
    IShellFolder_Release(desktop);
    printf(failed ? "RESULT: FAIL (%d)\n" : "RESULT: PASS\n", failed);
    return failed != 0;
}
