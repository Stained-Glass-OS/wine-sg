/* shell32: ExtractAssociatedIconExW and the desktop folder's SetNameOf (patch 3303), run by test/fastshmisc-gate.sh */
#define COBJMACROS
#include <windows.h>
#include <shlobj.h>
#include <shellapi.h>
#include <shlwapi.h>
#include <stdio.h>
#include <string.h>

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}
#define CHECKF(ok, ...) do { char _b[300]; snprintf(_b, sizeof(_b), __VA_ARGS__); check(ok, _b); } while (0)

typedef HICON (WINAPI *EXTRACT)(HINSTANCE, WCHAR *, WORD *, WORD *);

struct find { int index; WORD id; int found; };
static BOOL CALLBACK find_proc(HMODULE m, LPCWSTR type, LPWSTR name, LONG_PTR p)
{
    struct find *f = (struct find *)p;
    if (f->index-- > 0) return TRUE;
    f->id = IS_INTRESOURCE(name) ? (WORD)(ULONG_PTR)name : 0;
    f->found = 1;
    return FALSE;
}
static WORD group_id(const WCHAR *path, int index)
{
    struct find f = { index, 0, 0 };
    HMODULE m = LoadLibraryExW(path, NULL, LOAD_LIBRARY_AS_DATAFILE);
    EnumResourceNamesW(m, (LPCWSTR)RT_GROUP_ICON, find_proc, (LONG_PTR)&f);
    FreeLibrary(m);
    return f.found ? f.id : 0xffff;
}

static void set_value(const WCHAR *key, const WCHAR *name, const WCHAR *value)
{
    HKEY hkey;
    WCHAR path[300];
    swprintf(path, 300, L"Software\\Classes\\%ls", key);
    RegCreateKeyExW(HKEY_LOCAL_MACHINE, path, 0, NULL, 0, KEY_WRITE, NULL, &hkey, NULL);
    RegSetValueExW(hkey, name, 0, REG_SZ, (const BYTE *)value, (lstrlenW(value) + 1) * sizeof(WCHAR));
    RegCloseKey(hkey);
}

static BOOL exists(const WCHAR *dir, const WCHAR *name)
{
    WCHAR path[MAX_PATH];
    swprintf(path, MAX_PATH, L"%ls\\%ls", dir, name);
    return GetFileAttributesW(path) != INVALID_FILE_ATTRIBUTES;
}

static void make_file(const WCHAR *dir, const WCHAR *name)
{
    WCHAR path[MAX_PATH];
    HANDLE h;
    swprintf(path, MAX_PATH, L"%ls\\%ls", dir, name);
    h = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    if (h != INVALID_HANDLE_VALUE) CloseHandle(h);
}

static LPITEMIDLIST child_pidl(IShellFolder *desktop, const WCHAR *name)
{
    LPITEMIDLIST pidl = NULL;
    ULONG eaten = 0, attrs = 0;
    HRESULT hr = IShellFolder_ParseDisplayName(desktop, NULL, NULL, (WCHAR *)name, &eaten, &pidl, &attrs);
    return SUCCEEDED(hr) ? pidl : NULL;
}

static void name_of(IShellFolder *desktop, LPCITEMIDLIST pidl, WCHAR *out)
{
    STRRET str;
    out[0] = 0;
    if (SUCCEEDED(IShellFolder_GetDisplayNameOf(desktop, pidl, SHGDN_FORPARSING | SHGDN_INFOLDER, &str)))
        StrRetToBufW(&str, pidl, out, MAX_PATH);
}

int main(void)
{
    EXTRACT extract = (EXTRACT)GetProcAddress(GetModuleHandleA("shell32.dll"), "ExtractAssociatedIconExW");
    WCHAR path[MAX_PATH], sys[MAX_PATH], shell32[MAX_PATH], desk[MAX_PATH], buf[MAX_PATH];
    WORD index, id;
    HICON icon;
    IShellFolder *desktop;
    LPITEMIDLIST pidl, out;
    HRESULT hr;

    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    if (!extract) { check(0, "shell32 exports ExtractAssociatedIconExW"); return 1; }
    GetSystemDirectoryW(sys, MAX_PATH);
    swprintf(shell32, MAX_PATH, L"%ls\\shell32.dll", sys);

    /* an executable module: its own icon, the path stays, the id is the group's */
    lstrcpyW(path, shell32);
    index = 3; id = 0xabcd;
    icon = extract(NULL, path, &index, &id);
    CHECKF(icon > (HICON)1, "an icon of shell32 itself");
    CHECKF(!lstrcmpiW(path, shell32) && index == 3, "the path and index stay");
    CHECKF(id == group_id(shell32, 3), "the id is the third icon group's: %u vs %u", id, group_id(shell32, 3));
    if (icon > (HICON)1) DestroyIcon(icon);

    /* the id pointer and the index pointer are optional */
    lstrcpyW(path, shell32);
    index = 0;
    icon = extract(NULL, path, &index, NULL);
    CHECKF(icon > (HICON)1, "a NULL id pointer");
    if (icon > (HICON)1) DestroyIcon(icon);
    lstrcpyW(path, shell32);
    id = 0xabcd;
    icon = extract(NULL, path, NULL, &id);
    CHECKF(icon > (HICON)1 && id == group_id(shell32, 0), "a NULL index pointer is index 0 (%u)", id);
    if (icon > (HICON)1) DestroyIcon(icon);

    /* a name that is not there: the generic icon of shell32, which the path now names */
    lstrcpyW(path, L"C:\\no\\such\\file.xyz123");
    index = 5; id = 0xabcd;
    icon = extract(NULL, path, &index, &id);
    CHECKF(icon > (HICON)1 && StrStrIW(path, L"shell32.dll") != NULL && index == 0 && id == group_id(path, index),
            "a missing file gets a shell32 icon (%ls, index %u, id %u)", path, index, id);
    if (icon > (HICON)1) DestroyIcon(icon);

    /* the desktop folder's SetNameOf */
    SHGetDesktopFolder(&desktop);
    SHGetSpecialFolderPathW(NULL, desk, CSIDL_DESKTOPDIRECTORY, TRUE);
    make_file(desk, L"fast-a.txt");
    make_file(desk, L"fast-b.txt");
    make_file(desk, L"fast-c.dat");
    swprintf(buf, MAX_PATH, L"%ls\\fast-dir", desk);
    CreateDirectoryW(buf, NULL);

    pidl = child_pidl(desktop, L"fast-a.txt");
    CHECKF(pidl != NULL, "the file's item");
    if (pidl)
    {
        STRRET str;
        WCHAR shown[MAX_PATH];
        BOOL hidden;

        IShellFolder_GetDisplayNameOf(desktop, pidl, SHGDN_INFOLDER, &str);
        StrRetToBufW(&str, pidl, shown, MAX_PATH);
        hidden = !wcscmp(shown, L"fast-a");

        out = NULL;
        hr = IShellFolder_SetNameOf(desktop, NULL, pidl, L"fast-renamed", SHGDN_INFOLDER, &out);
        CHECKF(hr == S_OK, "rename by display name (%#lx)", hr);
        CHECKF(hidden ? exists(desk, L"fast-renamed.txt") : exists(desk, L"fast-renamed"),
                "the extension is %s: %s", hidden ? "kept (it is not shown)" : "as typed (it is shown)", hidden ? "fast-renamed.txt" : "fast-renamed");
        CHECKF(!exists(desk, L"fast-a.txt"), "the old name is gone");
        name_of(desktop, out, buf);
        CHECKF(out != NULL && !wcsncmp(buf, L"fast-renamed", 12), "the new item names the file (%ls)", buf);
        if (out) ILFree(out);
        ILFree(pidl);
    }

    pidl = child_pidl(desktop, L"fast-b.txt");
    if (pidl)
    {
        out = NULL;
        hr = IShellFolder_SetNameOf(desktop, NULL, pidl, L"fast-parsed.log", SHGDN_FORPARSING, &out);
        CHECKF(hr == S_OK && exists(desk, L"fast-parsed.log") && !exists(desk, L"fast-b.txt"), "rename by parsing name keeps exactly the name (%#lx)", hr);
        if (out) ILFree(out);
        hr = IShellFolder_SetNameOf(desktop, NULL, pidl, L"fast-x.log", SHGDN_FORPARSING, NULL);
        CHECKF(hr != S_OK && !exists(desk, L"fast-x.log"), "a name whose source is gone fails (%#lx)", hr);
        ILFree(pidl);
    }
    pidl = child_pidl(desktop, L"fast-parsed.log");
    if (pidl)
    {
        hr = IShellFolder_SetNameOf(desktop, NULL, pidl, L"bad*name", SHGDN_FORPARSING, NULL);
        CHECKF(hr == HRESULT_FROM_WIN32(ERROR_CANCELLED) && exists(desk, L"fast-parsed.log"), "characters a name cannot have: cancelled (%#lx)", hr);
        hr = IShellFolder_SetNameOf(desktop, NULL, pidl, L"fast-c.dat", SHGDN_FORPARSING, NULL);
        CHECKF(hr == HRESULT_FROM_WIN32(ERROR_ALREADY_EXISTS) && exists(desk, L"fast-c.dat") && exists(desk, L"fast-parsed.log"), "a name that is taken fails (%#lx)", hr);
        hr = IShellFolder_SetNameOf(desktop, NULL, pidl, NULL, SHGDN_FORPARSING, NULL);
        CHECKF(hr == E_INVALIDARG, "no name (%#lx)", hr);
        ILFree(pidl);
    }
    /* with extensions hidden, the new name keeps the extension that is not shown */
    {
        HKEY hkey;
        DWORD one = 1;
        RegCreateKeyExW(HKEY_CURRENT_USER, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced", 0, NULL, 0, KEY_WRITE, NULL, &hkey, NULL);
        RegSetValueExW(hkey, L"HideFileExt", 0, REG_DWORD, (BYTE *)&one, sizeof(one));
        RegCloseKey(hkey);
        pidl = child_pidl(desktop, L"fast-c.dat");
        if (pidl)
        {
            out = NULL;
            hr = IShellFolder_SetNameOf(desktop, NULL, pidl, L"fast-hidden", SHGDN_INFOLDER, &out);
            CHECKF(hr == S_OK && exists(desk, L"fast-hidden.dat") && !exists(desk, L"fast-c.dat"), "a hidden extension is kept (%#lx)", hr);
            if (out) ILFree(out);
            ILFree(pidl);
            swprintf(buf, MAX_PATH, L"%ls\\fast-hidden.dat", desk); DeleteFileW(buf);
        }
        RegOpenKeyExW(HKEY_CURRENT_USER, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced", 0, KEY_WRITE, &hkey);
        RegDeleteValueW(hkey, L"HideFileExt");
        RegCloseKey(hkey);
    }
    pidl = child_pidl(desktop, L"fast-dir");
    if (pidl)
    {
        out = NULL;
        hr = IShellFolder_SetNameOf(desktop, NULL, pidl, L"fast-dir2", SHGDN_NORMAL, &out);
        CHECKF(hr == S_OK && exists(desk, L"fast-dir2") && !exists(desk, L"fast-dir"), "a folder is renamed (%#lx)", hr);
        if (out) ILFree(out);
        ILFree(pidl);
    }
    {
        LPITEMIDLIST mycomp = child_pidl(desktop, L"::{20D04FE0-3AEA-1069-A2D8-08002B30309D}");
        if (mycomp)
        {
            hr = IShellFolder_SetNameOf(desktop, NULL, mycomp, L"Renamed", SHGDN_NORMAL, NULL);
            CHECKF(FAILED(hr), "My Computer has no file to rename (%#lx)", hr);
            ILFree(mycomp);
        }
        else check(0, "the My Computer item");
    }
    hr = IShellFolder_SetNameOf(desktop, NULL, (LPCITEMIDLIST)NULL, L"x", SHGDN_NORMAL, NULL);
    CHECKF(hr == E_INVALIDARG, "no item (%#lx)", hr);

    swprintf(buf, MAX_PATH, L"%ls\\fast-dir2", desk);
    RemoveDirectoryW(buf);
    swprintf(buf, MAX_PATH, L"%ls\\fast-parsed.log", desk); DeleteFileW(buf);
    swprintf(buf, MAX_PATH, L"%ls\\fast-renamed.txt", desk); DeleteFileW(buf);
    swprintf(buf, MAX_PATH, L"%ls\\fast-renamed", desk); DeleteFileW(buf);
    swprintf(buf, MAX_PATH, L"%ls\\fast-c.dat", desk); DeleteFileW(buf);
    IShellFolder_Release(desktop);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
