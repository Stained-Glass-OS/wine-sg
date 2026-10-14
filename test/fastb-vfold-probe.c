/* shell32 virtual folders: BindToStorage, GetDetailsEx and IPersistFolder::Initialize (patch 3701) */
#define COBJMACROS
#define INITGUID
#include <windows.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <shobjidl.h>
#include <stdio.h>
#include <wchar.h>
#include <string.h>

static int failed;
#define CHECK(c, ...) do { if (!(c)) { failed++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static const GUID CLSID_MyComputer_ = { 0x20d04fe0, 0x3aea, 0x1069, { 0xa2, 0xd8, 0x08, 0x00, 0x2b, 0x30, 0x30, 0x9d } };
static const GUID CLSID_ControlPanel_ = { 0x21ec2020, 0x3aea, 0x1069, { 0xa2, 0xdd, 0x08, 0x00, 0x2b, 0x30, 0x30, 0x9d } };
static const GUID CLSID_NetworkPlaces_ = { 0x208d2c60, 0x3aea, 0x1069, { 0xa2, 0xd7, 0x08, 0x00, 0x2b, 0x30, 0x30, 0x9d } };
static const GUID CLSID_ShellDesktop_ = { 0x00021400, 0x0000, 0x0000, { 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46 } };
DEFINE_GUID(FMTID_Storage_, 0xb725f130, 0x47ef, 0x101a, 0xa5, 0xf1, 0x02, 0x60, 0x8c, 0x9e, 0xeb, 0xac);
static const SHCOLUMNID name_id = { { 0xb725f130, 0x47ef, 0x101a, { 0xa5, 0xf1, 0x02, 0x60, 0x8c, 0x9e, 0xeb, 0xac } }, 10 };
static const SHCOLUMNID size_id = { { 0xb725f130, 0x47ef, 0x101a, { 0xa5, 0xf1, 0x02, 0x60, 0x8c, 0x9e, 0xeb, 0xac } }, 12 };
static const SHCOLUMNID write_id = { { 0xb725f130, 0x47ef, 0x101a, { 0xa5, 0xf1, 0x02, 0x60, 0x8c, 0x9e, 0xeb, 0xac } }, 14 };

static ITEMIDLIST *first_item(IShellFolder2 *sf, SHCONTF flags)
{
    IEnumIDList *e = NULL;
    ITEMIDLIST *id = NULL;

    if (SUCCEEDED(IShellFolder2_EnumObjects(sf, NULL, flags, &e)) && e)
    {
        if (IEnumIDList_Next(e, 1, &id, NULL) != S_OK) id = NULL;
        IEnumIDList_Release(e);
    }
    return id;
}

static void check_name(IShellFolder2 *sf, ITEMIDLIST *id, const char *what)
{
    VARIANT v;
    HRESULT hr;

    VariantInit(&v);
    hr = IShellFolder2_GetDetailsEx(sf, id, &name_id, &v);
    CHECK(hr == S_OK && V_VT(&v) == VT_BSTR && V_BSTR(&v) && V_BSTR(&v)[0], "%s: name hr %#lx vt %d", what, hr, V_VT(&v));
    VariantClear(&v);
}

static void check_init(const CLSID *clsid, const char *what, BOOL fixed)
{
    IPersistFolder2 *pf = NULL;
    ITEMIDLIST *mine = NULL, *cur = NULL, *other;
    HRESULT hr;

    hr = CoCreateInstance(clsid, NULL, CLSCTX_INPROC_SERVER, &IID_IPersistFolder2, (void **)&pf);
    CHECK(hr == S_OK && pf, "%s: create %#lx", what, hr);
    if (!pf) return;
    hr = IPersistFolder2_Initialize(pf, NULL);
    CHECK(hr == E_INVALIDARG, "%s: Initialize(NULL) %#lx", what, hr);
    if (fixed)
    {
        /* the desktop keeps its place */
        SHGetSpecialFolderLocation(NULL, CSIDL_DESKTOP, &mine);
        hr = IPersistFolder2_Initialize(pf, mine);
        CHECK(hr == S_OK, "%s: Initialize(desktop) %#lx", what, hr);
        SHGetSpecialFolderLocation(NULL, CSIDL_DRIVES, &other);
        hr = IPersistFolder2_Initialize(pf, other);
        CHECK(hr == E_INVALIDARG, "%s: Initialize(somewhere else) %#lx", what, hr);
        ILFree(other);
    }
    else
    {
        /* the place it is given */
        SHGetSpecialFolderLocation(NULL, CSIDL_BITBUCKET, &mine);
        hr = IPersistFolder2_Initialize(pf, mine);
        CHECK(hr == S_OK, "%s: Initialize %#lx", what, hr);
        hr = IPersistFolder2_GetCurFolder(pf, &cur);
        CHECK(hr == S_OK && cur && ILIsEqual(cur, mine), "%s: GetCurFolder is the given place (%#lx)", what, hr);
    }
    ILFree(mine);
    ILFree(cur);
    IPersistFolder2_Release(pf);
}

int main(void)
{
    WCHAR dir[MAX_PATH], file[MAX_PATH], deskfile[MAX_PATH];
    IShellFolder *desktop = NULL;
    IShellFolder2 *sf = NULL;
    ITEMIDLIST *dirpidl = NULL, *item, *id;
    IStream *stream = NULL;
    VARIANT v;
    HRESULT hr;
    HANDLE h;
    DWORD n;
    char buf[16] = { 0 };
    ULONG got;
    char *unix_path;

    setvbuf(stdout, NULL, _IONBF, 0);
    OleInitialize(NULL);
    SHGetDesktopFolder(&desktop);

    /* never touch anything outside the scratch home */
    SHGetFolderPathW(NULL, CSIDL_DESKTOPDIRECTORY, NULL, 0, deskfile);
    {
        typedef char *(CDECL *get_unix_fn)(const WCHAR *);
        get_unix_fn get_unix = (get_unix_fn)GetProcAddress(GetModuleHandleA("kernel32.dll"), "wine_get_unix_file_name");
        unix_path = get_unix ? get_unix(deskfile) : NULL;
        if (!unix_path || strncmp(unix_path, "/var/tmp/", 9))
        {
            printf("REFUSING: desktop is %s\nRESULT: FAIL (unsafe)\n", unix_path ? unix_path : "(unknown)");
            return 2;
        }
    }

    /* the file system folder */
    GetTempPathW(MAX_PATH, dir);
    lstrcatW(dir, L"sgvfold-probe");
    CreateDirectoryW(dir, NULL);
    swprintf(file, MAX_PATH, L"%ls\\sgv.txt", dir);
    h = CreateFileW(file, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    WriteFile(h, "hello", 5, &n, NULL);
    CloseHandle(h);
    dirpidl = ILCreateFromPathW(dir);
    hr = IShellFolder_BindToObject(desktop, dirpidl, NULL, &IID_IShellFolder2, (void **)&sf);
    CHECK(hr == S_OK && sf, "bind the directory %#lx", hr);
    if (sf)
    {
        item = first_item(sf, SHCONTF_NONFOLDERS);
        CHECK(item != NULL, "the file");
        VariantInit(&v);
        hr = IShellFolder2_GetDetailsEx(sf, item, &size_id, &v);
        CHECK(hr == S_OK && V_VT(&v) == VT_UI8 && V_UI8(&v) == 5, "FS size: hr %#lx vt %d", hr, V_VT(&v));
        VariantClear(&v);
        hr = IShellFolder2_GetDetailsEx(sf, item, &write_id, &v);
        CHECK(hr == S_OK && V_VT(&v) == VT_DATE && V_DATE(&v) > 40000.0, "FS write time: hr %#lx vt %d", hr, V_VT(&v));
        VariantClear(&v);
        check_name(sf, item, "FS");
        {
            SHCOLUMNID bad = { { 0 }, 77 };
            hr = IShellFolder2_GetDetailsEx(sf, item, &bad, &v);
            CHECK(FAILED(hr), "FS unknown property %#lx", hr);
        }
        hr = IShellFolder2_BindToStorage(sf, item, NULL, &IID_IStream, (void **)&stream);
        CHECK(hr == S_OK && stream, "FS BindToStorage %#lx", hr);
        if (stream)
        {
            got = 0;
            IStream_Read(stream, buf, 5, &got);
            CHECK(got == 5 && !memcmp(buf, "hello", 5), "FS stream content");
            IStream_Release(stream);
        }
        ILFree(item);
        IShellFolder2_Release(sf);
        sf = NULL;
    }

    /* the desktop */
    hr = IShellFolder_QueryInterface(desktop, &IID_IShellFolder2, (void **)&sf);
    CHECK(hr == S_OK && sf, "desktop folder2 %#lx", hr);
    if (sf)
    {
        swprintf(file, MAX_PATH, L"%ls\\sgdesk.txt", deskfile);
        h = CreateFileW(file, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
        WriteFile(h, "desk!", 5, &n, NULL);
        CloseHandle(h);
        id = ILCreateFromPathW(file);
        item = id ? ILClone(ILFindLastID(id)) : NULL;
        CHECK(item != NULL, "desktop file pidl");
        if (item)
        {
            hr = IShellFolder2_BindToStorage(sf, item, NULL, &IID_IStream, (void **)&stream);
            CHECK(hr == S_OK && stream, "desktop BindToStorage %#lx", hr);
            if (stream)
            {
                got = 0;
                memset(buf, 0, sizeof(buf));
                IStream_Read(stream, buf, 5, &got);
                CHECK(got == 5 && !memcmp(buf, "desk!", 5), "desktop stream content '%s'", buf);
                IStream_Release(stream);
            }
            hr = IShellFolder2_BindToStorage(sf, item, NULL, &IID_IShellFolder, (void **)&stream);
            CHECK(hr == E_NOINTERFACE, "desktop file as a folder %#lx", hr);
            VariantInit(&v);
            hr = IShellFolder2_GetDetailsEx(sf, item, &size_id, &v);
            CHECK(hr == S_OK && V_VT(&v) == VT_UI8 && V_UI8(&v) == 5, "desktop file size: hr %#lx vt %d", hr, V_VT(&v));
            VariantClear(&v);
            check_name(sf, item, "desktop");
            ILFree(item);
        }
        ILFree(id);
        DeleteFileW(file);
        hr = IShellFolder2_BindToStorage(sf, NULL, NULL, &IID_IStream, (void **)&stream);
        CHECK(hr == E_INVALIDARG, "desktop BindToStorage(NULL) %#lx", hr);
        IShellFolder2_Release(sf);
        sf = NULL;
    }

    /* This PC, Control Panel, Network */
    {
        static const int csidls[] = { CSIDL_DRIVES, CSIDL_CONTROLS, CSIDL_NETWORK };
        static const char *names[] = { "This PC", "Control Panel", "Network" };
        int i;

        for (i = 0; i < 3; i++)
        {
            ITEMIDLIST *pidl = NULL;

            SHGetSpecialFolderLocation(NULL, csidls[i], &pidl);
            hr = IShellFolder_BindToObject(desktop, pidl, NULL, &IID_IShellFolder2, (void **)&sf);
            CHECK(hr == S_OK && sf, "%s: bind %#lx", names[i], hr);
            if (sf)
            {
                item = first_item(sf, SHCONTF_FOLDERS | SHCONTF_NONFOLDERS);
                if (item)
                {
                    check_name(sf, item, names[i]);
                    if (i == 0)
                    {
                        /* a property that only the column of the folder has */
                        SHCOLUMNID type_id = { { 0xb725f130, 0x47ef, 0x101a, { 0xa5, 0xf1, 0x02, 0x60, 0x8c, 0x9e, 0xeb, 0xac } }, 4 };

                        VariantInit(&v);
                        hr = IShellFolder2_GetDetailsEx(sf, item, &type_id, &v);
                        CHECK(hr == S_OK && V_VT(&v) == VT_BSTR && V_BSTR(&v) && V_BSTR(&v)[0], "This PC: type column hr %#lx vt %d", hr, V_VT(&v));
                        VariantClear(&v);
                    }
                    stream = NULL;
                    hr = IShellFolder2_BindToStorage(sf, item, NULL, &IID_IShellFolder, (void **)&stream);
                    CHECK(FAILED(hr) && !stream, "%s: an item has no storage as a folder %#lx", names[i], hr);
                    hr = IShellFolder2_BindToStorage(sf, item, NULL, &IID_IStream, (void **)&stream);
                    CHECK(FAILED(hr) && !stream, "%s: this item is no file %#lx", names[i], hr);
                    ILFree(item);
                }
                else if (i != 2) CHECK(0, "%s: no items", names[i]);
                hr = IShellFolder2_BindToStorage(sf, NULL, NULL, &IID_IStream, (void **)&stream);
                CHECK(hr == E_INVALIDARG, "%s: BindToStorage(NULL) %#lx", names[i], hr);
                if (i == 2)
                {
                    ITEMIDLIST *p = NULL;
                    hr = IShellFolder2_ParseDisplayName(sf, NULL, NULL, (WCHAR *)L"no-such-computer-sg", NULL, &p, NULL);
                    CHECK(hr == HRESULT_FROM_WIN32(ERROR_BAD_NET_NAME) && !p, "Network: ParseDisplayName of a stranger %#lx", hr);
                }
                IShellFolder2_Release(sf);
                sf = NULL;
            }
            ILFree(pidl);
        }
    }

    check_init(&CLSID_MyComputer_, "MyComputer", FALSE);
    check_init(&CLSID_ControlPanel_, "ControlPanel", FALSE);
    check_init(&CLSID_NetworkPlaces_, "NetworkPlaces", FALSE);
    check_init(&CLSID_ShellDesktop_, "ShellDesktop", TRUE);

    ILFree(dirpidl);
    DeleteFileW(file);
    swprintf(file, MAX_PATH, L"%ls\\sgv.txt", dir);
    DeleteFileW(file);
    RemoveDirectoryW(dir);
    IShellFolder_Release(desktop);
    printf(failed ? "RESULT: FAIL (%d)\n" : "RESULT: PASS\n", failed);
    return failed != 0;
}
