/* msctf TF_CreateCategoryMgr and TF_CreateDisplayAttributeMgr (patches/sg/2243). */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <msctf.h>
#include <stdio.h>

static int fails;
#define CHECK(c, ...) do { if (c) printf("PASS  %s\n", #c); else { printf("FAIL  %s: ", #c); printf(__VA_ARGS__); printf("\n"); fails++; } } while (0)

int main(void)
{
    HMODULE msctf;
    HRESULT (WINAPI *pCat)(ITfCategoryMgr **), (WINAPI *pDam)(ITfDisplayAttributeMgr **);
    ITfCategoryMgr *cat = (void *)1, *cat2;
    ITfDisplayAttributeMgr *dam = (void *)1;
    TfGuidAtom atom = 0, atom2 = 0;
    GUID guid = { 0x4d5a1b0e, 0x6cf2, 0x4d1e, { 1, 2, 3, 4, 5, 6, 7, 8 } }, back;
    HRESULT hr;
    ULONG ref;

    CoInitialize(NULL);
    msctf = LoadLibraryA("msctf.dll");
    pCat = (void *)GetProcAddress(msctf, "TF_CreateCategoryMgr");
    pDam = (void *)GetProcAddress(msctf, "TF_CreateDisplayAttributeMgr");
    CHECK(pCat && pDam, "exports present");
    if (!pCat || !pDam) { printf("RESULT: FAIL\n"); return 1; }

    hr = pCat(NULL);
    CHECK(hr == E_INVALIDARG, "NULL: %#lx", hr);
    hr = pCat(&cat);
    CHECK(hr == S_OK && cat, "category manager: %#lx", hr);
    if (hr == S_OK)
    {
        hr = ITfCategoryMgr_RegisterGUID(cat, &guid, &atom);
        CHECK(hr == S_OK && atom, "RegisterGUID %#lx atom %lu", hr, atom);
        hr = ITfCategoryMgr_GetGUID(cat, atom, &back);
        CHECK(hr == S_OK && IsEqualGUID(&back, &guid), "GetGUID %#lx", hr);
        pCat(&cat2);
        hr = ITfCategoryMgr_RegisterGUID(cat2, &guid, &atom2);
        CHECK(hr == S_OK && atom2 == atom, "a second manager sees the same atom: %lu / %lu", atom2, atom);
        ITfCategoryMgr_Release(cat2);
        ref = ITfCategoryMgr_Release(cat);
        CHECK(ref == 0, "released: %lu", ref);
    }

    hr = pDam(NULL);
    CHECK(hr == E_INVALIDARG, "NULL: %#lx", hr);
    hr = pDam(&dam);
    CHECK(hr == S_OK && dam, "display attribute manager: %#lx", hr);
    if (hr == S_OK)
    {
        IEnumTfDisplayAttributeInfo *en = NULL;
        hr = ITfDisplayAttributeMgr_EnumDisplayAttributeInfo(dam, &en);
        CHECK(hr == S_OK || hr == E_NOTIMPL, "EnumDisplayAttributeInfo reaches the object: %#lx", hr);
        if (en) IEnumTfDisplayAttributeInfo_Release(en);
        ref = ITfDisplayAttributeMgr_Release(dam);
        CHECK(ref == 0, "released: %lu", ref);
    }
    CoUninitialize();
    printf("RESULT: %s\n", fails ? "FAIL" : "PASS");
    return fails != 0;
}
