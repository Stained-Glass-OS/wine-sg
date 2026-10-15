#define COBJMACROS
#include <windows.h>
#include <shlobj.h>
#include <stdio.h>
#include <wchar.h>

static int failures, checks;
#define CHECK(c, ...) do { checks++; if (!(c)) { failures++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)
static HMODULE h;
#define ORD(n) GetProcAddress(h, (LPCSTR)(n))

static LRESULT CALLBACK wp(HWND w, UINT m, WPARAM a, LPARAM b) { return m == WM_USER ? 42 : DefWindowProcW(w, m, a, b); }

int main(void)
{
    HRESULT (WINAPI *strw)(STRRET *, const ITEMIDLIST *, WCHAR **) = NULL;
    HRESULT (WINAPI *strbuf)(STRRET *, const ITEMIDLIST *, WCHAR *, UINT) = NULL;
    HRESULT (WINAPI *strbstr)(STRRET *, const ITEMIDLIST *, BSTR *) = NULL;
    STRRET sr;
    WCHAR buf[64], *s;
    BSTR b;
    HRESULT hr;
    int i;

    CoInitialize(NULL);
    h = LoadLibraryW(L"shcore.dll");
    strw = (void *)ORD(143); strbstr = (void *)ORD(144); strbuf = (void *)ORD(145);
    CHECK(strw && strbuf && strbstr, "ordinals");

    memset(&sr, 0, sizeof(sr)); sr.uType = STRRET_CSTR; strcpy(sr.cStr, "hello");
    hr = strw(&sr, NULL, &s);
    CHECK(hr == S_OK && !wcscmp(s, L"hello"), "cstr");
    CoTaskMemFree(s);
    sr.uType = STRRET_WSTR; sr.pOleStr = CoTaskMemAlloc(8 * 2); wcscpy(sr.pOleStr, L"wide");
    hr = strbuf(&sr, NULL, buf, 64);
    CHECK(hr == S_OK && !wcscmp(buf, L"wide"), "wstr to buf");
    {
        char item[16] = "xxabc";
        sr.uType = STRRET_OFFSET; sr.uOffset = 2;
        hr = strbstr(&sr, (ITEMIDLIST *)item, &b);
        CHECK(hr == S_OK && !wcscmp(b, L"abc"), "offset bstr");
        SysFreeString(b);
        hr = strw(&sr, NULL, &s);
        CHECK(FAILED(hr) && !s, "offset without pidl");
    }
    strcpy(sr.cStr, "truncated"); sr.uType = STRRET_CSTR;
    hr = strbuf(&sr, NULL, buf, 4);
    CHECK(hr == S_OK && !wcscmp(buf, L"tru"), "truncation %ls", buf);

    {
        int (WINAPI *a2w)(UINT, const char *, WCHAR *, int) = (void *)ORD(150);
        int (WINAPI *w2a)(UINT, const WCHAR *, char *, int) = (void *)ORD(151);
        char ab[16];
        i = a2w(CP_ACP, "abc", buf, 64);
        CHECK(i == 4 && !wcscmp(buf, L"abc"), "a2w");
        i = a2w(CP_ACP, "abcdef", buf, 3);
        CHECK(i == 3 && buf[2] == 0, "a2w truncate %d", i);
        i = w2a(CP_UTF8, L"h\x00e9", ab, 16);
        CHECK(i == 4 && (unsigned char)ab[1] == 0xc3, "w2a utf8 %d", i);
        CHECK(a2w(CP_ACP, "x", NULL, 5) == 0, "a2w null dest");
    }
    {
        BOOL (WINAPI *net)(const WCHAR *) = (void *)ORD(170);
        WCHAR *(WINAPI *root)(WCHAR *, int) = (void *)ORD(172);
        char *(WINAPI *roota)(char *, int) = (void *)ORD(173);
        UINT (WINAPI *dtype)(int) = (void *)ORD(174);
        char ra[8];
        CHECK(net(L"\\\\server\\share"), "unc is network");
        CHECK(!net(L"C:\\x"), "C: not network");
        CHECK(!net(L"relative"), "relative");
        CHECK(!wcscmp(root(buf, 2), L"C:\\"), "root w");
        CHECK(!strcmp(roota(ra, 3), "D:\\"), "root a");
        CHECK(dtype(2) == GetDriveTypeW(L"C:\\"), "drive type");
    }
    {
        int (WINAPI *mk)(HWND, const WCHAR *, SECURITY_ATTRIBUTES *) = (void *)ORD(290);
        int (WINAPI *mka)(HWND, const char *, SECURITY_ATTRIBUTES *) = (void *)ORD(291);
        WCHAR tmp[MAX_PATH], dir[MAX_PATH], deep[MAX_PATH];
        char da[MAX_PATH];
        GetTempPathW(MAX_PATH, tmp);
        swprintf(dir, MAX_PATH, L"%lssgshc%lu", tmp, GetCurrentProcessId());
        swprintf(deep, MAX_PATH, L"%ls\\a\\b\\c", dir);
        { int r = mk(NULL, deep, NULL); CHECK(r == ERROR_SUCCESS, "create deep %d %ls", r, deep); }
        CHECK(GetFileAttributesW(deep) != INVALID_FILE_ATTRIBUTES, "deep exists");
        CHECK(mk(NULL, deep, NULL) == ERROR_ALREADY_EXISTS, "already exists");
        CHECK(mk(NULL, L"relative\\dir", NULL) == ERROR_BAD_PATHNAME, "relative path");
        {
            WCHAR file[MAX_PATH];
            HANDLE f;
            swprintf(file, MAX_PATH, L"%ls\\afile", dir);
            f = CreateFileW(file, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL); CloseHandle(f);
            CHECK(mk(NULL, file, NULL) == ERROR_FILE_EXISTS, "file exists");
            DeleteFileW(file);
        }
        WideCharToMultiByte(CP_ACP, 0, dir, -1, da, MAX_PATH, NULL, NULL);
        strcat(da, "\\x\\y");
        CHECK(mka(NULL, da, NULL) == ERROR_SUCCESS, "ansi create");
        RemoveDirectoryW(deep); swprintf(deep, MAX_PATH, L"%ls\\a\\b", dir); RemoveDirectoryW(deep);
        swprintf(deep, MAX_PATH, L"%ls\\a", dir); RemoveDirectoryW(deep);
        swprintf(deep, MAX_PATH, L"%ls\\x\\y", dir); RemoveDirectoryW(deep);
        swprintf(deep, MAX_PATH, L"%ls\\x", dir); RemoveDirectoryW(deep);
        RemoveDirectoryW(dir);
    }
    {
        HANDLE (WINAPI *alloc)(const void *, DWORD, DWORD) = (void *)ORD(182);
        void *(WINAPI *lock)(HANDLE, DWORD) = (void *)ORD(184);
        BOOL (WINAPI *unlock)(void *) = (void *)ORD(186);
        BOOL (WINAPI *freeit)(HANDLE, DWORD) = (void *)ORD(187);
        HANDLE sh = alloc("shared!", 8, GetCurrentProcessId());
        char *p;
        CHECK(sh != NULL, "alloc shared");
        p = lock(sh, GetCurrentProcessId());
        CHECK(p && !strcmp(p, "shared!"), "lock shared");
        if (p) { p[0] = 'S'; CHECK(unlock(p), "unlock"); }
        p = lock(sh, GetCurrentProcessId());
        CHECK(p && p[0] == 'S', "write persisted");
        if (p) unlock(p);
        CHECK(freeit(sh, GetCurrentProcessId()), "free shared");
    }
    {
        BOOL (WINAPI *g)(const WCHAR *, GUID *) = (void *)ORD(200);
        GUID guid;
        CHECK(g(L"{00000001-0002-0003-0405-060708090a0b}", &guid) && guid.Data1 == 1, "guid");
        CHECK(!g(L"junk", &guid), "bad guid");
        CHECK(!g(NULL, &guid), "null guid");
    }
    {
        DWORD (WINAPI *gv)(HKEY, const WCHAR *, const WCHAR *, DWORD *, void *, DWORD *) = (void *)ORD(125);
        DWORD (WINAPI *clsidkey)(const GUID *, const WCHAR *, BOOL, BOOL, HKEY *) = (void *)ORD(120);
        HRESULT (WINAPI *loadui)(HKEY, const WCHAR *, WCHAR *, DWORD) = (void *)ORD(126);
        HKEY k; DWORD type, sz = sizeof(buf);
        GUID g = { 0x7a1b2c3d, 1, 2, { 3, 4, 5, 6, 7, 8, 9, 10 } };
        RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\SgShcoreProbe", 0, NULL, 0, KEY_WRITE, NULL, &k, NULL);
        RegSetValueExW(k, L"v", 0, REG_SZ, (const BYTE *)L"val", 8);
        RegCloseKey(k);
        CHECK(gv(HKEY_CURRENT_USER, L"Software\\SgShcoreProbe", L"v", &type, buf, &sz) == 0 && !wcscmp(buf, L"val"), "goodboot");
        CHECK(clsidkey(&g, L"InprocServer32", TRUE, TRUE, &k) == 0, "clsid create");
        RegCloseKey(k);
        CHECK(clsidkey(&g, L"InprocServer32", TRUE, FALSE, &k) == 0, "clsid open");
        RegCloseKey(k);
        CHECK(clsidkey(&g, L"nope", TRUE, FALSE, &k) != 0, "clsid missing");
        RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\SgShcoreProbe", 0, KEY_READ, &k);
        buf[0] = 1;
        CHECK(loadui(k, L"v", buf, 64) == S_OK && !wcscmp(buf, L"val"), "loadreguistring %ls", buf);
        CHECK(FAILED(loadui(k, L"missing", buf, 64)), "loadui missing");
        RegCloseKey(k);
        RegDeleteTreeW(HKEY_CURRENT_USER, L"Software\\SgShcoreProbe");
        {
            WCHAR path[100];
            LONG r;
            StringFromGUID2(&g, buf, 64);
            swprintf(path, 100, L"Software\\Classes\\CLSID\\%s", buf);
            r = RegDeleteTreeW(HKEY_CURRENT_USER, path);
            (void)r;
        }
    }
    {
        HWND (WINAPI *mkw)(WNDPROC, HWND, DWORD, DWORD, HMENU, LONG_PTR) = (void *)ORD(188);
        HWND w = mkw(wp, NULL, 0, WS_POPUP, NULL, 7);
        CHECK(w != NULL, "worker window");
        if (w)
        {
            WCHAR cls[32];
            GetClassNameW(w, cls, 32);
            CHECK(!wcscmp(cls, L"WorkerW"), "class %ls", cls);
            CHECK(SendMessageW(w, WM_USER, 0, 0) == 42, "worker proc installed");
            DestroyWindow(w);
        }
    }
    {
        HRESULT (WINAPI *gc)(IUnknown *, CLSID *) = (void *)ORD(142);
        CLSID c;
        CHECK(FAILED(gc(NULL, &c)) && IsEqualGUID(&c, &GUID_NULL), "getclassid null");
    }
    printf("%d checks, %d failures\n", checks, failures);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
