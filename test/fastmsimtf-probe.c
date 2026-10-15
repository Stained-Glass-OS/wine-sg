#include <windows.h>
#include <stdio.h>

static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)
#define VT(obj) (*(void ***)(obj))
#define CALL(obj, idx, ret, ...) (((ret (WINAPI *)(void *, ...))VT(obj)[idx])(obj, ##__VA_ARGS__))

static const GUID CLSID_CActiveIMM_ = {0x4955dd33, 0xb159, 0x11d0, {0x8f, 0xcf, 0x00, 0xaa, 0x00, 0x6b, 0xcc, 0x59}};
static const GUID IID_IActiveIMMApp_ = {0x08c0e040, 0x62d1, 0x11d1, {0x93, 0x26, 0x00, 0x60, 0xb0, 0x67, 0xb8, 0x6e}};
static const GUID IID_CF = {0x00000001, 0, 0, {0xc0, 0, 0, 0, 0, 0, 0, 0x46}};

int main(void)
{
    HMODULE mod = LoadLibraryA("msimtf.dll");
    HRESULT (WINAPI *get_class)(const GUID *, const GUID *, void **) = mod ? (void *)GetProcAddress(mod, "DllGetClassObject") : NULL;
    void *cf = NULL, *imm = NULL, *en = NULL, *clone = NULL;
    HRESULT hr;
    UINT cp = 0;
    WORD lang = 0;
    HIMC ctx[4];
    ULONG fetched = 99;
    HKL hkl = (HKL)(ULONG_PTR)0x04090409;

    CHECK(get_class != NULL, "dll");
    if (!get_class) return 1;
    hr = get_class(&CLSID_CActiveIMM_, &IID_CF, &cf); CHECK(hr == S_OK, "class object %lx", hr);
    if (hr != S_OK) return 1;
    hr = CALL(cf, 3, HRESULT, NULL, &IID_IActiveIMMApp_, &imm); CHECK(hr == S_OK && imm, "create %lx", hr);
    if (!imm) return 1;

    hr = CALL(imm, 64, HRESULT, hkl, &cp); CHECK(hr == S_OK && cp == 1252, "GetCodePageA %lx %u", hr, cp);
    hr = CALL(imm, 65, HRESULT, hkl, &lang); CHECK(hr == S_OK && lang == 0x409, "GetLangId %lx %x", hr, lang);
    hr = CALL(imm, 65, HRESULT, hkl, NULL); CHECK(hr == E_INVALIDARG, "GetLangId NULL %lx", hr);
    hr = CALL(imm, 60, HRESULT, TRUE); CHECK(hr == S_OK, "Activate %lx", hr);

    hr = CALL(imm, 70, HRESULT, GetCurrentThreadId(), &en); CHECK(hr == S_OK && en, "EnumInputContext %lx", hr);
    if (en)
    {
        hr = CALL(en, 4, HRESULT, 4, ctx, &fetched); CHECK(hr == S_OK || hr == S_FALSE, "Next %lx", hr);
        CHECK(fetched <= 4, "fetched %lu", fetched);
        hr = CALL(en, 5, HRESULT); CHECK(hr == S_OK, "Reset %lx", hr);
        hr = CALL(en, 3, HRESULT, &clone); CHECK(hr == S_OK && clone, "Clone %lx", hr);
    }
    printf(fails ? "RESULT: FAIL (%d)\n" : "RESULT: PASS\n", fails);
    return fails != 0;
}
