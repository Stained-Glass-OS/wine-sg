/* shell32 batch (3689): SHRegDeleteKeyW, class factory LockServer, FS BindToStorage, shell link GetSizeMax */
#define COBJMACROS
#include <windows.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <stdio.h>

static int failed;
#define CHECK(c, ...) do { if (!(c)) { failed++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

int main(void)
{
    HRESULT (WINAPI *pDel)(HKEY, LPCWSTR) = (void *)GetProcAddress(GetModuleHandleA("shell32.dll"), (LPCSTR)512);
    HKEY k;
    IClassFactory *cf = NULL;
    IPersistStream *ps = NULL;
    IShellLinkW *link = NULL;
    HRESULT hr;
    WCHAR path[MAX_PATH], dir[MAX_PATH];

    CoInitialize(NULL);
    /* SHRegDeleteKeyW removes the key and its subkeys */
    RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\SGDel\\a\\b", 0, NULL, 0, KEY_WRITE, NULL, &k, NULL); RegCloseKey(k);
    CHECK(pDel(HKEY_CURRENT_USER, L"Software\\SGDel") == 0, "SHRegDeleteKeyW");
    CHECK(RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\SGDel", 0, KEY_READ, &k) != 0, "key gone");
    CHECK(pDel(HKEY_CURRENT_USER, L"Software\\SGDel") != 0, "deleting a missing key fails");

    hr = CoGetClassObject(&CLSID_ShellLink, CLSCTX_INPROC_SERVER, NULL, &IID_IClassFactory, (void **)&cf);
    CHECK(hr == S_OK && cf, "class factory %#lx", hr);
    if (cf)
    {
        CHECK(IClassFactory_LockServer(cf, TRUE) == S_OK, "LockServer(TRUE)");
        CHECK(IClassFactory_LockServer(cf, FALSE) == S_OK, "LockServer(FALSE)");
        IClassFactory_Release(cf);
    }

    /* link size */
    hr = CoCreateInstance(&CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, &IID_IShellLinkW, (void **)&link);
    CHECK(hr == S_OK, "link");
    if (link)
    {
        ULARGE_INTEGER size = {{0}};
        IStream *stm; STATSTG st;
        IShellLinkW_SetPath(link, L"C:\\windows\\notepad.exe");
        IShellLinkW_SetArguments(link, L"/p file.txt");
        IShellLinkW_QueryInterface(link, &IID_IPersistStream, (void **)&ps);
        hr = IPersistStream_GetSizeMax(ps, &size);
        CHECK(hr == S_OK && size.QuadPart > 76, "GetSizeMax %#lx %lu", hr, (unsigned long)size.QuadPart);
        CreateStreamOnHGlobal(NULL, TRUE, &stm);
        IPersistStream_Save(ps, stm, FALSE);
        IStream_Stat(stm, &st, STATFLAG_NONAME);
        CHECK(st.cbSize.QuadPart == size.QuadPart, "size equals saved size %lu vs %lu", (unsigned long)st.cbSize.QuadPart, (unsigned long)size.QuadPart);
        CHECK(IPersistStream_GetSizeMax(ps, NULL) == E_INVALIDARG, "NULL");
        IStream_Release(stm);
        IPersistStream_Release(ps);
        IShellLinkW_Release(link);
    }

    /* BindToStorage of a file: its bytes as a stream */
    GetTempPathW(MAX_PATH, dir);
    wsprintfW(dir + lstrlenW(dir), L"sgbs_%lu", GetCurrentProcessId());
    CreateDirectoryW(dir, NULL);
    wsprintfW(path, L"%s\\f.txt", dir);
    {
        HANDLE h = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
        DWORD w;
        WriteFile(h, "hello stream", 12, &w, NULL);
        CloseHandle(h);
    }
    {
        IShellFolder *desktop, *folder = NULL;
        ITEMIDLIST *dpidl = NULL, *fpidl = NULL;
        IStream *stream = NULL;
        char buf[32] = {0};
        ULONG got = 0;

        SHGetDesktopFolder(&desktop);
        SHParseDisplayName(dir, NULL, &dpidl, 0, NULL);
        IShellFolder_BindToObject(desktop, dpidl, NULL, &IID_IShellFolder, (void **)&folder);
        SHParseDisplayName(path, NULL, &fpidl, 0, NULL);
        hr = IShellFolder_BindToStorage(folder, ILFindLastID(fpidl), NULL, &IID_IStream, (void **)&stream);
        CHECK(hr == S_OK && stream, "BindToStorage %#lx", hr);
        if (stream)
        {
            IStream_Read(stream, buf, 31, &got);
            CHECK(got == 12 && !memcmp(buf, "hello stream", 12), "stream content %lu '%s'", got, buf);
            IStream_Release(stream);
        }
        stream = NULL;
        hr = IShellFolder_BindToStorage(folder, ILFindLastID(fpidl), NULL, &IID_IStorage, (void **)&stream);
        CHECK(FAILED(hr) && !stream, "no storage interface %#lx", hr);
        IShellFolder_Release(folder);
        IShellFolder_Release(desktop);
        ILFree(dpidl); ILFree(fpidl);
    }
    DeleteFileW(path);
    RemoveDirectoryW(dir);
    printf(failed ? "RESULT: FAIL (%d)\n" : "RESULT: PASS\n", failed);
    return failed != 0;
}
