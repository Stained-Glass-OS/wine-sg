/* the type library written by Scriptlet.TypeLib */
#define COBJMACROS
#include <windows.h>
#include <oleauto.h>
#include <stdio.h>

static int fails;
static const GUID libid = {0x1e1d9d0c, 0x3b33, 0x4c1a, {0x93, 0xc2, 0x8f, 0x4f, 0x9b, 0x2a, 0x0e, 0x11}};
static const GUID clsid = {0x6b5c0d7e, 0x5a1f, 0x4c7e, {0x8d, 0x2b, 0x0b, 0x8c, 0x6d, 0x3a, 0x9e, 0x11}};
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

int main(void)
{
    ITypeLib *lib = NULL;
    ITypeInfo *ti = NULL;
    TYPEATTR *attr;
    TLIBATTR *tla;
    BSTR name = NULL, doc = NULL;
    HRESULT hr;
    UINT count;

    CoInitialize(NULL);
    hr = LoadTypeLibEx(L"C:\\fastscrobj.tlb", REGKIND_NONE, &lib);
    CHECK(hr == S_OK, "LoadTypeLibEx %lx", hr);
    if (!lib) return 1;
    count = ITypeLib_GetTypeInfoCount(lib);
    CHECK(count == 2, "type info count %u", count);
    hr = ITypeLib_GetLibAttr(lib, &tla);
    CHECK(hr == S_OK && tla->wMajorVerNum == 2 && tla->wMinorVerNum == 3, "version");
    CHECK(IsEqualGUID(&tla->guid, &libid), "libid");
    ITypeLib_ReleaseTLibAttr(lib, tla);
    hr = ITypeLib_GetDocumentation(lib, -1, &name, &doc, NULL, NULL);
    CHECK(hr == S_OK && !wcscmp(name, L"SgScrobj") && doc && !wcscmp(doc, L"SG scrobj typelib"), "lib doc %ls / %ls", name, doc);
    SysFreeString(name); SysFreeString(doc);

    /* the interface: add, res, comp, fsoName, plain get+put, ro get = 7 functions after the 7 of IDispatch */
    hr = ITypeLib_GetTypeInfo(lib, 0, &ti);
    CHECK(hr == S_OK, "GetTypeInfo(0)");
    if (ti)
    {
        ITypeInfo_GetTypeAttr(ti, &attr);
        CHECK(attr->typekind == TKIND_DISPATCH && attr->cFuncs == 14, "interface kind %d funcs %u", attr->typekind, attr->cFuncs);
        ITypeInfo_ReleaseTypeAttr(ti, attr);
        name = NULL;
        ITypeInfo_GetDocumentation(ti, MEMBERID_NIL, &name, NULL, NULL, NULL);
        CHECK(name && !wcscmp(name, L"IScrobjTest"), "interface name %ls", name);
        SysFreeString(name);
        ITypeInfo_Release(ti);
    }
    hr = ITypeLib_GetTypeInfo(lib, 1, &ti);
    if (ti)
    {
        ITypeInfo_GetTypeAttr(ti, &attr);
        CHECK(attr->typekind == TKIND_COCLASS && attr->cImplTypes == 1, "coclass kind %d impl %u", attr->typekind, attr->cImplTypes);
        CHECK(IsEqualGUID(&attr->guid, &clsid), "coclass guid");
        ITypeInfo_ReleaseTypeAttr(ti, attr);
        ITypeInfo_Release(ti);
    }
    ITypeLib_Release(lib);
    printf("RESULT: %s\n", fails ? "FAIL" : "PASS");
    return fails;
}
