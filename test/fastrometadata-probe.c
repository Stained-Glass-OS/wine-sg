#include <windows.h>
#include <oleauto.h>
#include <stdio.h>

static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)
#define VT(obj) (*(void ***)(obj))
#define CALL(obj, idx, ret, ...) (((ret (WINAPI *)(void *, ...))VT(obj)[idx])(obj, ##__VA_ARGS__))

/* CLSID_CorMetaDataDispenser e5cb7a31-7512-11d2-89ce-0080c792e5d8, IID_IMetaDataDispenserEx 31bcfce2-dafb-11d2-9f81-00c04f79a0a3 */
static const GUID clsid = {0xe5cb7a31, 0x7512, 0x11d2, {0x89, 0xce, 0x00, 0x80, 0xc7, 0x92, 0xe5, 0xd8}};
static const GUID iid = {0x31bcfce2, 0xdafb, 0x11d2, {0x9f, 0x81, 0x00, 0xc0, 0x4f, 0x79, 0xa0, 0xa3}};

int main(void)
{
    HMODULE mod = LoadLibraryA("rometadata.dll");
    HRESULT (WINAPI *get_dispenser)(const GUID *, const GUID *, void **) = (void *)GetProcAddress(mod, "MetaDataGetDispenser");
    void *d = NULL;
    GUID opt = {1, 2, 3, {4, 5, 6, 7, 8, 9, 10, 11}};
    VARIANT v;
    WCHAR dir[MAX_PATH];
    DWORD len = 0;
    HRESULT hr;

    CHECK(get_dispenser != NULL, "export");
    if (!get_dispenser) return 1;
    hr = get_dispenser(&clsid, &iid, &d); CHECK(hr == S_OK && d, "dispenser %lx", hr);
    VariantInit(&v); V_VT(&v) = VT_I4; V_I4(&v) = 99;
    hr = CALL(d, 6, HRESULT, &opt, &v); CHECK(hr == S_OK, "SetOption %lx", hr);
    VariantInit(&v);
    hr = CALL(d, 7, HRESULT, &opt, &v); CHECK(hr == S_OK && V_VT(&v) == VT_I4 && V_I4(&v) == 99, "GetOption %lx", hr);
    hr = CALL(d, 9, HRESULT, dir, MAX_PATH, &len); CHECK(hr == S_OK && len > 10 && wcsstr(dir, L"v4.0.30319"), "GetCORSystemDirectory %lx %ls", hr, dir);
    printf(fails ? "RESULT: FAIL (%d)\n" : "RESULT: PASS\n", fails);
    return fails != 0;
}
