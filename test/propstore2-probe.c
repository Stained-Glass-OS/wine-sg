/* propsys batch (patches/sg/2052), run by test/propstore2-gate.sh: the
 * memory property store as IPersistSerializedPropStorage (the storage bytes
 * and loading them back), PropVariantToVariant for a class id, a file time
 * and a type it cannot convert, and PropVariantCompareEx for a float against
 * a double.
 *
 *   propstore2-probe.exe */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <propsys.h>
#include <propvarutil.h>
#include <stdio.h>
#include <string.h>

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}
#define CHECK(expr) check((expr), #expr)

static const GUID fmt_a = { 0x11111111, 0x2222, 0x3333, { 1, 2, 3, 4, 5, 6, 7, 8 } };
static const GUID fmt_b = { 0xaaaaaaaa, 0xbbbb, 0xcccc, { 8, 7, 6, 5, 4, 3, 2, 1 } };

typedef HRESULT (WINAPI *p2v_t)(const PROPVARIANT *, VARIANT *);
int main(void)
{
    p2v_t PropVariantToVariant = (p2v_t)GetProcAddress(LoadLibraryA("propsys.dll"), "PropVariantToVariant");
    IPropertyStore *store, *store2;
    IPersistSerializedPropStorage *sps, *sps2;
    PROPERTYKEY k1 = { fmt_a, 2 }, k2 = { fmt_a, 7 }, k3 = { fmt_b, 4 };
    PROPVARIANT pv, back;
    VARIANT var;
    SERIALIZEDPROPSTORAGE *bytes;
    DWORD size, count, dword;
    FILETIME ft = { 0x12345678, 0x01d00000 };
    HRESULT hr;
    BYTE *raw;

    CoInitialize(NULL);
    CHECK(PSCreateMemoryPropertyStore(&IID_IPropertyStore, (void **)&store) == S_OK);
    CHECK(IPropertyStore_QueryInterface(store, &IID_IPersistSerializedPropStorage, (void **)&sps) == S_OK);
    CHECK(IPersistSerializedPropStorage_SetFlags(sps, 0) == S_OK);

    /* an empty store has no bytes */
    bytes = (void *)1; size = 77;
    CHECK(IPersistSerializedPropStorage_GetPropertyStorage(sps, &bytes, &size) == S_OK && size == 0);
    CoTaskMemFree(bytes);
    CHECK(IPersistSerializedPropStorage_GetPropertyStorage(sps, NULL, &size) == E_POINTER);
    CHECK(IPersistSerializedPropStorage_GetPropertyStorage(sps, &bytes, NULL) == E_POINTER);

    PropVariantInit(&pv);
    pv.vt = VT_I4; pv.lVal = -42;
    IPropertyStore_SetValue(store, &k1, &pv);
    pv.vt = VT_LPWSTR; pv.pwszVal = (WCHAR *)L"hello";
    IPropertyStore_SetValue(store, &k2, &pv);
    pv.vt = VT_FILETIME; pv.filetime = ft;
    IPropertyStore_SetValue(store, &k3, &pv);

    bytes = NULL; size = 0;
    CHECK(IPersistSerializedPropStorage_GetPropertyStorage(sps, &bytes, &size) == S_OK && bytes && size > 40);
    raw = (BYTE *)bytes;
    memcpy(&dword, raw + 4, 4);
    CHECK(dword == 0x53505331);                                /* "1SPS" */
    memcpy(&dword, raw, 4);
    CHECK(dword > 24 && dword < size);                         /* the first block's size */
    CHECK(!memcmp(raw + 8, &fmt_a, sizeof(GUID)));
    memcpy(&dword, raw + size - 4, 4);
    CHECK(dword == 0);                                         /* ended by a zero size */

    /* loaded into another store, it comes back as it was */
    CHECK(PSCreateMemoryPropertyStore(&IID_IPropertyStore, (void **)&store2) == S_OK);
    IPropertyStore_QueryInterface(store2, &IID_IPersistSerializedPropStorage, (void **)&sps2);
    pv.vt = VT_I4; pv.lVal = 5;
    IPropertyStore_SetValue(store2, &k1, &pv);               /* replaced, not merged */
    CHECK(IPersistSerializedPropStorage_SetPropertyStorage(sps2, bytes, size) == S_OK);
    count = 99;
    IPropertyStore_GetCount(store2, &count);
    CHECK(count == 3);
    PropVariantInit(&back);
    CHECK(IPropertyStore_GetValue(store2, &k1, &back) == S_OK && back.vt == VT_I4 && back.lVal == -42);
    PropVariantClear(&back);
    CHECK(IPropertyStore_GetValue(store2, &k2, &back) == S_OK && back.vt == VT_LPWSTR && !wcscmp(back.pwszVal, L"hello"));
    PropVariantClear(&back);
    CHECK(IPropertyStore_GetValue(store2, &k3, &back) == S_OK && back.vt == VT_FILETIME &&
          back.filetime.dwLowDateTime == ft.dwLowDateTime && back.filetime.dwHighDateTime == ft.dwHighDateTime);
    PropVariantClear(&back);

    /* bad input */
    {
        BYTE bad[40];
        memcpy(bad, raw, sizeof(bad));
        bad[4] ^= 0xff;                                        /* the version */
        CHECK(IPersistSerializedPropStorage_SetPropertyStorage(sps2, (SERIALIZEDPROPSTORAGE *)bad, sizeof(bad)) == E_INVALIDARG);
    }
    CHECK(IPersistSerializedPropStorage_SetPropertyStorage(sps2, NULL, 4) == E_POINTER);
    CHECK(IPersistSerializedPropStorage_SetPropertyStorage(sps2, NULL, 0) == S_OK);
    count = 99;
    IPropertyStore_GetCount(store2, &count);
    CHECK(count == 0);
    CoTaskMemFree(bytes);
    IPersistSerializedPropStorage_Release(sps2);
    IPropertyStore_Release(store2);
    IPersistSerializedPropStorage_Release(sps);
    IPropertyStore_Release(store);

    /* PropVariantToVariant */
    PropVariantInit(&pv);
    VariantInit(&var);
    pv.vt = VT_CLSID; pv.puuid = (GUID *)&fmt_a;
    hr = PropVariantToVariant(&pv, &var);
    CHECK(hr == 39 && V_VT(&var) == VT_BSTR && !wcscmp(V_BSTR(&var), L"{11111111-2222-3333-0102-030405060708}"));
    VariantClear(&var);
    pv.vt = VT_FILETIME; pv.filetime = ft;
    hr = PropVariantToVariant(&pv, &var);
    CHECK(hr == S_OK && V_VT(&var) == VT_DATE);
    {
        FILETIME back_ft = { 0 };
        SYSTEMTIME st;
        VariantTimeToSystemTime(V_DATE(&var), &st);
        SystemTimeToFileTime(&st, &back_ft);
        FileTimeToSystemTime(&ft, &st);
        CHECK(back_ft.dwHighDateTime == ft.dwHighDateTime);
    }
    pv.vt = 0xdead;
    memset(&var, 0xcc, sizeof(var));
    CHECK(PropVariantToVariant(&pv, &var) == E_OUTOFMEMORY && V_VT(&var) == VT_EMPTY);
    pv.vt = VT_ILLEGAL;
    CHECK(PropVariantToVariant(&pv, &var) == E_OUTOFMEMORY);

    /* floats and doubles */
    {
        PROPVARIANT r4, r8, r8b;
        r4.vt = VT_R4; r4.fltVal = 1.5f;
        r8.vt = VT_R8; r8.dblVal = 1.5;
        r8b.vt = VT_R8; r8b.dblVal = 2.25;
        CHECK(PropVariantCompareEx(&r4, &r8, 0, 0) == 0);
        CHECK(PropVariantCompareEx(&r8, &r4, 0, 0) == 0);
        CHECK(PropVariantCompareEx(&r4, &r8b, 0, 0) == -1);
        CHECK(PropVariantCompareEx(&r8b, &r4, 0, 0) == 1);
    }

    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures ? 1 : 0;
}
