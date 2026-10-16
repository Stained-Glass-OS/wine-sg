#include <windows.h>
#include <stdio.h>
static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)
typedef HRESULT (WINAPI *open_t)(HANDLE *);
typedef HRESULT (WINAPI *close_t)(HANDLE);
typedef HRESULT (WINAPI *keys_t)(HANDLE, const GUID *, UINT *, GUID **);
int main(void)
{
    HMODULE m = LoadLibraryA("sppc.dll");
    open_t op = (open_t)GetProcAddress(m, "SLOpen");
    close_t cl = (close_t)GetProcAddress(m, "SLClose");
    keys_t keys = (keys_t)GetProcAddress(m, "SLGetInstalledProductKeyIds");
    HANDLE a = NULL, b = NULL; UINT n; GUID *ids; HRESULT hr;
    CHECK(op(NULL) == E_INVALIDARG, "SLOpen(NULL)");
    CHECK(op(&a) == S_OK && a && a != (HANDLE)0xdeadbeef, "open a %p", a);
    CHECK(op(&b) == S_OK && b && b != a, "open b %p", b);
    hr = keys(a, NULL, &n, &ids); CHECK(hr == S_OK || hr == (HRESULT)0xC004F012, "valid handle %08lx", hr);
    CHECK(keys((HANDLE)0xdeadbeef, NULL, &n, &ids) == E_INVALIDARG, "bogus handle");
    CHECK(keys(NULL, NULL, &n, &ids) == E_INVALIDARG, "NULL handle");
    CHECK(cl(a) == S_OK, "close a");
    CHECK(cl(a) == E_INVALIDARG, "close a twice");
    CHECK(keys(a, NULL, &n, &ids) == E_INVALIDARG, "closed handle refused");
    hr = keys(b, NULL, &n, &ids); CHECK(hr == S_OK || hr == (HRESULT)0xC004F012, "other handle still valid %08lx", hr);
    CHECK(cl(b) == S_OK, "close b");
    CHECK(cl(NULL) == E_INVALIDARG, "close NULL");
    CHECK(cl((HANDLE)0xdeadbeef) == E_INVALIDARG, "close bogus");
    printf(fails ? "RESULT: FAIL\n" : "RESULT: PASS\n");
    return fails != 0;
}
