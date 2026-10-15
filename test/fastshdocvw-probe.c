#include <windows.h>
#include <shlobj.h>
#include <string.h>
#include <stdio.h>

static int fails;
#define CHECK(c, ...) do { if (!(c)) { printf("FAIL %s:%d ", #c, __LINE__); printf(__VA_ARGS__); printf("\n"); fails++; } } while (0)

static int has_bytes(const void *buf, UINT size, const void *what, UINT len)
{
    UINT i;
    for (i = 0; i + len <= size; i++)
        if (!memcmp((const char *)buf + i, what, len)) return 1;
    return 0;
}

int main(void)
{
    HRESULT (WINAPI *parse)(DWORD, LPCWSTR, LPBC, LPITEMIDLIST *);
    HMODULE m;
    LPITEMIDLIST pidl;
    WCHAR path[MAX_PATH];
    HRESULT hr;

    CoInitialize(NULL);
    m = LoadLibraryA("shdocvw.dll");
    CHECK(m != NULL, "load");
    parse = (void *)GetProcAddress(m, (LPCSTR)218);
    CHECK(parse != NULL, "ordinal 218");
    if (!parse) { printf("RESULT: FAIL\n"); return 1; }

    pidl = (LPITEMIDLIST)0x1;
    hr = parse(CP_ACP, L"C:\\windows", NULL, &pidl);
    CHECK(hr == S_OK && pidl && pidl != (LPITEMIDLIST)1, "windows dir %lx", hr);
    if (hr == S_OK)
    {
        CHECK(SHGetPathFromIDListW(pidl, path) && !lstrcmpiW(path, L"C:\\windows"), "path %ls", path);
        CoTaskMemFree(pidl);
    }
    pidl = (LPITEMIDLIST)0x1;
    hr = parse(CP_ACP, L"C:\\no\\such\\dir", NULL, &pidl);
    CHECK(FAILED(hr) && !pidl, "missing %lx %p", hr, pidl);
    hr = parse(CP_ACP, NULL, NULL, &pidl);
    CHECK(hr == E_INVALIDARG && !pidl, "null name %lx", hr);
    hr = parse(CP_ACP, L"C:\\windows", NULL, NULL);
    CHECK(hr == E_INVALIDARG, "null out %lx", hr);

    /* addresses: the desktop folder hands them to ordinal 218; this used to recurse until the stack overflowed */
    pidl = NULL;
    hr = SHParseDisplayName(L"http://www.winehq.org/", NULL, &pidl, 0, NULL);
    CHECK(hr == S_OK && pidl, "http url via SHParseDisplayName %lx", hr);
    if (pidl)
    {
        CHECK(has_bytes(pidl, ILGetSize(pidl), L"www.winehq.org", 28), "http pidl holds the address");
        CHECK(ILGetSize(pidl) > 40, "http pidl holds the address (%u)", ILGetSize(pidl));
        CoTaskMemFree(pidl);
    }
    pidl = NULL;
    hr = SHParseDisplayName(L"about:blank", NULL, &pidl, 0, NULL);
    CHECK(hr == S_OK && pidl, "about: via SHParseDisplayName %lx", hr);
    CoTaskMemFree(pidl);
    pidl = NULL;
    hr = SHParseDisplayName(L"file:///C:/windows", NULL, &pidl, 0, NULL);
    CHECK(hr == S_OK && pidl, "file url via SHParseDisplayName %lx", hr);
    if (pidl)
    {
        path[0] = 0;
        CHECK(SHGetPathFromIDListW(pidl, path) && !lstrcmpiW(path, L"C:\\windows"), "file url path %ls", path);
        CoTaskMemFree(pidl);
    }
    pidl = NULL;
    hr = parse(CP_ACP, L"file:///C:/no/such/dir", NULL, &pidl);
    CHECK(FAILED(hr) && !pidl, "missing file url %lx", hr);
    pidl = NULL;
    hr = parse(CP_ACP, L"ftp://ftp.example.org/x", NULL, &pidl);
    CHECK(hr == S_OK && pidl && ILGetSize(pidl) > 40, "ftp url %lx", hr);
    CoTaskMemFree(pidl);
    printf("RESULT: %s\n", fails ? "FAIL" : "PASS");
    return fails != 0;
}
