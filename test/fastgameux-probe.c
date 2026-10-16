#include <windows.h>
#include <objbase.h>
#include <stdio.h>

static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)
#define VT(obj) (*(void ***)(obj))
#define CALL(obj, idx, ret, ...) (((ret (WINAPI *)(void *, ...))VT(obj)[idx])(obj, ##__VA_ARGS__))

static const GUID CLSID_GE = {0x9A5EA990, 0x3034, 0x4D6F, {0x91, 0x28, 0x01, 0xF3, 0xC6, 0x10, 0x22, 0xBC}};
static const GUID IID_GE2 = {0x86874AA7, 0xA1ED, 0x450d, {0xA7, 0xEB, 0xB8, 0x9E, 0x20, 0xB2, 0xFF, 0xF3}};

int main(void)
{
    void *ge2 = NULL;
    BOOL access = FALSE;
    HRESULT hr;
    HMODULE mod;
    HRESULT (WINAPI *can_unload)(void);

    CoInitialize(NULL);
    hr = CoCreateInstance(&CLSID_GE, NULL, CLSCTX_INPROC_SERVER, &IID_GE2, &ge2);
    CHECK(hr == S_OK && ge2, "create %lx", hr);
    if (!ge2) return 1;
    hr = CALL(ge2, 5, HRESULT, L"C:\\game.exe", &access); CHECK(hr == S_OK && access, "CheckAccess %lx", hr);
    hr = CALL(ge2, 5, HRESULT, L"C:\\game.exe", NULL); CHECK(hr == E_INVALIDARG, "CheckAccess NULL %lx", hr);
    mod = GetModuleHandleA("gameux.dll");
    can_unload = mod ? (void *)GetProcAddress(mod, "DllCanUnloadNow") : NULL;
    CHECK(can_unload && can_unload() == S_OK, "DllCanUnloadNow");
    printf(fails ? "RESULT: FAIL (%d)\n" : "RESULT: PASS\n", fails);
    return fails != 0;
}
