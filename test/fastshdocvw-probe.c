#include <windows.h>
#include <shlobj.h>
#include <stdio.h>

static int fails;
#define CHECK(c, ...) do { if (!(c)) { printf("FAIL %s:%d ", #c, __LINE__); printf(__VA_ARGS__); printf("\n"); fails++; } } while (0)

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
    printf("RESULT: %s\n", fails ? "FAIL" : "PASS");
    return fails != 0;
}
