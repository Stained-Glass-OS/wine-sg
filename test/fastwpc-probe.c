#include <windows.h>
#include <stdio.h>

static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)
#define VT(obj) (*(void ***)(obj))
#define CALL(obj, idx, ret, ...) (((ret (WINAPI *)(void *, ...))VT(obj)[idx])(obj, ##__VA_ARGS__))

static const GUID CLSID_WPC = {0xe77cc89b, 0x7401, 0x4c04, {0x8c, 0xed, 0x14, 0x9d, 0xb3, 0x5a, 0xdd, 0x04}};
static const GUID IID_WPC = {0x28b4d88b, 0xe072, 0x49e6, {0x80, 0x4d, 0x26, 0xed, 0xbe, 0x21, 0xa7, 0xb9}};
static const GUID IID_CF = {0x00000001, 0, 0, {0xc0, 0, 0, 0, 0, 0, 0, 0x46}};

int main(void)
{
    HMODULE mod = LoadLibraryA("wpc.dll");
    HRESULT (WINAPI *get_class)(const GUID *, const GUID *, void **) = mod ? (void *)GetProcAddress(mod, "DllGetClassObject") : NULL;
    void *cf = NULL, *wpc = NULL, *games = NULL;
    int vis = -1;
    DWORD restr = 99, reasons = 99;
    GUID app = {5};
    HRESULT hr;

    CHECK(get_class != NULL, "dll");
    if (!get_class) return 1;
    hr = get_class(&CLSID_WPC, &IID_CF, &cf); CHECK(hr == S_OK, "class %lx", hr);
    hr = CALL(cf, 3, HRESULT, NULL, &IID_WPC, &wpc); CHECK(hr == S_OK && wpc, "create %lx", hr);
    if (!wpc) return 1;
    hr = CALL(wpc, 3, HRESULT, &vis); CHECK(hr == S_OK && vis == 1, "GetVisibility %lx %d", hr, vis);
    hr = CALL(wpc, 7, HRESULT, L"S-1-5-32-544", &games); CHECK(hr == S_OK && games, "GetGamesSettings %lx", hr);
    if (games)
    {
        hr = CALL(games, 5, HRESULT, &restr); CHECK(hr == S_OK && restr == 0, "GetRestrictions %lx %lu", hr, restr);
        hr = CALL(games, 6, HRESULT, app, &reasons); CHECK(hr == S_OK && reasons == 0, "IsBlocked %lx", hr);
    }
    printf(fails ? "RESULT: FAIL (%d)\n" : "RESULT: PASS\n", fails);
    return fails != 0;
}
