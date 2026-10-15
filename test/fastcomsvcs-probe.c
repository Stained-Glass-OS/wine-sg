#define COBJMACROS
#include <windows.h>
#include <ole2.h>
#include <comsvcs.h>
#include <stdio.h>

static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static const GUID CLSID_SPGM = {0x2a005c11,0xa5de,0x11cf,{0x9e,0x66,0x00,0xaa,0x00,0xa3,0xf4,0x64}};
static const GUID IID_SPGM = {0x2a005c0d,0xa5de,0x11cf,{0x9e,0x66,0x00,0xaa,0x00,0xa3,0xf4,0x64}};


int main(void)
{
    ISharedPropertyGroupManager *mgr;
    ISharedPropertyGroup *g1, *g2;
    ISharedProperty *p1, *p2;
    LONG iso = 0, rel = 1;
    VARIANT_BOOL exists = VARIANT_TRUE;
    VARIANT v;
    HRESULT hr;
    IUnknown *enum_unk;
    IEnumVARIANT *en;
    ULONG got;
    IUnknown *ctx = (IUnknown *)1;
    HMODULE mod;
    HRESULT (WINAPI *pGetObjectContext)(IUnknown **);

    CoInitialize(NULL);
    hr = CoCreateInstance(&CLSID_SPGM, NULL, CLSCTX_INPROC_SERVER, &IID_SPGM, (void **)&mgr);
    CHECK(hr == S_OK, "manager %08lx", hr);
    if (hr == S_OK)
    {
        BSTR name = SysAllocString(L"grp");
        hr = ISharedPropertyGroupManager_CreatePropertyGroup(mgr, name, &iso, &rel, &exists, &g1);
        CHECK(hr == S_OK && exists == VARIANT_FALSE && g1, "create %08lx", hr);
        iso = 1; rel = 0;
        hr = ISharedPropertyGroupManager_CreatePropertyGroup(mgr, name, &iso, &rel, &exists, &g2);
        CHECK(hr == S_OK && exists == VARIANT_TRUE && g2 == g1 && iso == 0 && rel == 1, "second create %08lx exists %d iso %ld rel %ld", hr, exists, iso, rel);
        ISharedPropertyGroup_Release(g2);
        hr = ISharedPropertyGroupManager_get_Group(mgr, name, &g2);
        CHECK(hr == S_OK && g2 == g1, "get group");
        ISharedPropertyGroup_Release(g2);
        SysFreeString(name);
        name = SysAllocString(L"nogroup");
        hr = ISharedPropertyGroupManager_get_Group(mgr, name, &g2);
        CHECK(FAILED(hr) && !g2, "missing group %08lx", hr);
        SysFreeString(name);

        name = SysAllocString(L"counter");
        hr = ISharedPropertyGroup_CreateProperty(g1, name, &exists, &p1);
        CHECK(hr == S_OK && exists == VARIANT_FALSE, "create prop %08lx", hr);
        VariantInit(&v);
        hr = ISharedProperty_get_Value(p1, &v);
        CHECK(hr == S_OK && V_VT(&v) == VT_I4 && V_I4(&v) == 0, "initial value");
        V_VT(&v) = VT_I4; V_I4(&v) = 41;
        hr = ISharedProperty_put_Value(p1, v);
        CHECK(hr == S_OK, "put");
        hr = ISharedPropertyGroup_CreateProperty(g1, name, &exists, &p2);
        CHECK(hr == S_OK && exists == VARIANT_TRUE, "second create prop");
        VariantInit(&v);
        ISharedProperty_get_Value(p2, &v);
        CHECK(V_VT(&v) == VT_I4 && V_I4(&v) == 41, "shared value");
        ISharedProperty_Release(p2);
        hr = ISharedPropertyGroup_get_Property(g1, name, &p2);
        CHECK(hr == S_OK, "get property");
        ISharedProperty_Release(p2);
        SysFreeString(name);

        hr = ISharedPropertyGroup_CreatePropertyByPosition(g1, 5, &exists, &p2);
        CHECK(hr == S_OK && exists == VARIANT_FALSE, "by position");
        ISharedProperty_Release(p2);
        hr = ISharedPropertyGroup_get_PropertyByPosition(g1, 5, &p2);
        CHECK(hr == S_OK, "get by position");
        ISharedProperty_Release(p2);
        hr = ISharedPropertyGroup_get_PropertyByPosition(g1, 3, &p2);
        CHECK(FAILED(hr), "empty position");
        ISharedProperty_Release(p1);

        /* automation */
        {
            IDispatch *disp;
            UINT n = 0;
            DISPID id;
            LPOLESTR nm = (LPOLESTR)L"CreateProperty";
            ISharedPropertyGroup_QueryInterface(g1, &IID_IDispatch, (void **)&disp);
            hr = IDispatch_GetTypeInfoCount(disp, &n);
            CHECK(hr == S_OK && n == 1, "typeinfo count");
            hr = IDispatch_GetIDsOfNames(disp, &IID_NULL, &nm, 1, LOCALE_USER_DEFAULT, &id);
            CHECK(hr == S_OK && id == 3, "group dispid %08lx %ld", hr, id);
            IDispatch_Release(disp);
        }

        hr = ISharedPropertyGroupManager_get__NewEnum(mgr, &enum_unk);
        CHECK(hr == S_OK, "newenum");
        hr = IUnknown_QueryInterface(enum_unk, &IID_IEnumVARIANT, (void **)&en);
        CHECK(hr == S_OK, "enum variant");
        VariantInit(&v);
        hr = IEnumVARIANT_Next(en, 1, &v, &got);
        CHECK(hr == S_OK && got == 1 && V_VT(&v) == VT_BSTR && !wcscmp(V_BSTR(&v), L"grp"), "enum item");
        VariantClear(&v);
        hr = IEnumVARIANT_Next(en, 1, &v, &got);
        CHECK(hr == S_FALSE && got == 0, "enum end");
        IEnumVARIANT_Release(en);
        IUnknown_Release(enum_unk);

        ISharedPropertyGroup_Release(g1);
        ISharedPropertyGroupManager_Release(mgr);
    }

    mod = LoadLibraryA("comsvcs.dll");
    pGetObjectContext = (void *)GetProcAddress(mod, "GetObjectContext");
    CHECK(pGetObjectContext != NULL, "GetObjectContext export");
    if (pGetObjectContext)
    {
        hr = pGetObjectContext(&ctx);
        CHECK(hr == CONTEXT_E_NOCONTEXT && ctx == NULL, "GetObjectContext %08lx", hr);
    }

    CoUninitialize();
    printf(fails ? "RESULT: FAIL (%d)\n" : "RESULT: PASS\n", fails);
    return fails != 0;
}
