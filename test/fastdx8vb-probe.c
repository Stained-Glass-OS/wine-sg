#define COBJMACROS
#include <windows.h>
#include <objbase.h>
#include <stdio.h>

static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

int main(void)
{
    HMODULE m = LoadLibraryA("dx8vb.dll");
    HRESULT (WINAPI *getclass)(REFCLSID, REFIID, void **) = (void *)GetProcAddress(m, "DllGetClassObject");
    IClassFactory *cf = NULL;
    HRESULT hr;
    IUnknown *unk;
    static const GUID bogus = { 0x12345678, 1, 2, { 3, 4, 5, 6, 7, 8, 9, 10 } };

    CHECK(m && getclass, "dx8vb loads");
    hr = getclass(&bogus, &IID_IClassFactory, (void **)&cf);
    CHECK(hr == CLASS_E_CLASSNOTAVAILABLE, "unknown class %lx", hr);
    hr = getclass(&GUID_NULL, &IID_IClassFactory, (void **)&cf);
    CHECK(hr == S_OK && cf, "factory %lx", hr);
    if (cf)
    {
        CHECK(IClassFactory_LockServer(cf, TRUE) == S_OK, "lock");
        CHECK(IClassFactory_LockServer(cf, FALSE) == S_OK, "unlock");
        CHECK(IClassFactory_LockServer(cf, FALSE) == S_OK, "unbalanced unlock is harmless");
        hr = IClassFactory_QueryInterface(cf, &IID_IUnknown, (void **)&unk);
        CHECK(hr == S_OK, "QI IUnknown");
        if (hr == S_OK) IUnknown_Release(unk);
        CHECK(IClassFactory_Release(cf) == 0, "release");
    }
    hr = getclass(&GUID_NULL, &bogus, (void **)&cf);
    CHECK(hr == E_NOINTERFACE, "unsupported interface %lx", hr);
    printf(fails ? "RESULT: FAIL (%d)\n" : "RESULT: PASS\n", fails);
    return fails != 0;
}
