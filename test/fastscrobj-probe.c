/* the type library written by Scriptlet.TypeLib */
#define COBJMACROS
#include <windows.h>
#include <oleauto.h>
#include <stdio.h>

static int fails;
#include <ocidl.h>
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
    CHECK(count == 3, "type info count %u", count);
    hr = ITypeLib_GetLibAttr(lib, &tla);
    CHECK(hr == S_OK && tla->wMajorVerNum == 2 && tla->wMinorVerNum == 3, "version");
    CHECK(IsEqualGUID(&tla->guid, &libid), "libid");
    ITypeLib_ReleaseTLibAttr(lib, tla);
    hr = ITypeLib_GetDocumentation(lib, -1, &name, &doc, NULL, NULL);
    CHECK(hr == S_OK && !wcscmp(name, L"SgScrobj") && doc && !wcscmp(doc, L"SG scrobj typelib"), "lib doc %ls / %ls", name, doc);
    SysFreeString(name); SysFreeString(doc);

    /* the interface: add, res, comp, fsoName, fire, plain get+put, ro get = 8 functions after the 7 of IDispatch */
    hr = ITypeLib_GetTypeInfo(lib, 0, &ti);
    CHECK(hr == S_OK, "GetTypeInfo(0)");
    if (ti)
    {
        ITypeInfo_GetTypeAttr(ti, &attr);
        CHECK(attr->typekind == TKIND_DISPATCH && attr->cFuncs == 15, "interface kind %d funcs %u", attr->typekind, attr->cFuncs);
        ITypeInfo_ReleaseTypeAttr(ti, attr);
        name = NULL;
        ITypeInfo_GetDocumentation(ti, MEMBERID_NIL, &name, NULL, NULL, NULL);
        CHECK(name && !wcscmp(name, L"IScrobjTest"), "interface name %ls", name);
        SysFreeString(name);
        ITypeInfo_Release(ti);
    }
    /* the events: a dispinterface with one method, onx */
    {
        UINT i, found = 0;
        for (i = 0; i < count; i++)
        {
            ITypeInfo *e = NULL;
            if (FAILED(ITypeLib_GetTypeInfo(lib, i, &e)) || !e) continue;
            if (SUCCEEDED(ITypeInfo_GetTypeAttr(e, &attr)))
            {
                if (attr->typekind == TKIND_DISPATCH && attr->cFuncs == 8)    /* the 7 of IDispatch and onx */
                {
                    UINT k;
                    for (k = 0; k < attr->cFuncs && !found; k++)
                    {
                        FUNCDESC *fd;
                        if (SUCCEEDED(ITypeInfo_GetFuncDesc(e, k, &fd)))
                        {
                            BSTR fname = NULL; UINT n = 0;
                            ITypeInfo_GetNames(e, fd->memid, &fname, 1, &n);
                            found = fname && !wcscmp(fname, L"onx") && fd->memid == 1;
                            SysFreeString(fname);
                            ITypeInfo_ReleaseFuncDesc(e, fd);
                        }
                    }
                }
                ITypeInfo_ReleaseTypeAttr(e, attr);
            }
            ITypeInfo_Release(e);
        }
        CHECK(found, "events dispinterface with onx");
    }
    ti = NULL;
    hr = ITypeLib_GetTypeInfoOfGuid(lib, &clsid, &ti);
    if (ti)
    {
        INT flags = 0;
        ITypeInfo_GetTypeAttr(ti, &attr);
        CHECK(attr->typekind == TKIND_COCLASS && attr->cImplTypes == 2, "coclass kind %d impl %u", attr->typekind, attr->cImplTypes);
        CHECK(IsEqualGUID(&attr->guid, &clsid), "coclass guid");
        ITypeInfo_ReleaseTypeAttr(ti, attr);
        /* the component's <event> is a source interface */
        hr = ITypeInfo_GetImplTypeFlags(ti, 1, &flags);
        CHECK(hr == S_OK && (flags & IMPLTYPEFLAG_FSOURCE), "event interface is a source: %lx flags %x", hr, flags);
        ITypeInfo_Release(ti);
    }
    ITypeLib_Release(lib);

    /* the instance's connection points and connections are enumerable */
    {
        IUnknown *obj = NULL;
        IConnectionPointContainer *cpc = NULL;
        IEnumConnectionPoints *ecp = NULL, *ecp2 = NULL;
        IEnumConnections *ec = NULL;
        IConnectionPoint *cp = NULL, *found = NULL;
        CONNECTDATA cd;
        ULONG got;
        IID iid;
        DWORD cookie = 0;
        static IDispatch sink;      /* a sink that only needs to be a distinct, callable IDispatch */

        hr = CoCreateInstance(&clsid, NULL, CLSCTX_INPROC_SERVER, &IID_IUnknown, (void **)&obj);
        CHECK(hr == S_OK && obj, "create the component %08lx", hr);
        if (obj) hr = IUnknown_QueryInterface(obj, &IID_IConnectionPointContainer, (void **)&cpc);
        CHECK(cpc != NULL, "IConnectionPointContainer");
        if (cpc)
        {
            hr = IConnectionPointContainer_EnumConnectionPoints(cpc, &ecp);
            CHECK(hr == S_OK && ecp, "EnumConnectionPoints %08lx", hr);
            if (ecp)
            {
                got = 0; hr = IEnumConnectionPoints_Next(ecp, 1, &cp, &got);
                CHECK(hr == S_OK && got == 1 && cp, "Next gives the connection point %08lx", hr);
                if (cp)
                {
                    hr = IConnectionPoint_GetConnectionInterface(cp, &iid);
                    CHECK(hr == S_OK, "GetConnectionInterface");
                    hr = IConnectionPointContainer_FindConnectionPoint(cpc, &iid, &found);
                    CHECK(hr == S_OK && found == cp, "it is the one FindConnectionPoint gives");
                    if (found) IConnectionPoint_Release(found);
                }
                got = 7; hr = IEnumConnectionPoints_Next(ecp, 1, &found, &got);
                CHECK(hr == S_FALSE && got == 0, "only one connection point %08lx", hr);
                IEnumConnectionPoints_Reset(ecp);
                hr = IEnumConnectionPoints_Clone(ecp, &ecp2);
                CHECK(hr == S_OK && ecp2, "Clone");
                if (ecp2) IEnumConnectionPoints_Release(ecp2);
                IEnumConnectionPoints_Release(ecp);
            }
            if (cp)
            {
                hr = IConnectionPoint_EnumConnections(cp, &ec);
                CHECK(hr == S_OK && ec, "EnumConnections with no sink %08lx", hr);
                if (ec)
                {
                    got = 5; hr = IEnumConnections_Next(ec, 1, &cd, &got);
                    CHECK(hr == S_FALSE && got == 0, "no connection yet");
                    IEnumConnections_Release(ec);
                }
                {
                    /* a sink: an object that is an IDispatch for the connection point's QueryInterface */
                }
                {
                    IDispatch *disp = NULL;
                    hr = CoCreateInstance(&clsid, NULL, CLSCTX_INPROC_SERVER, &IID_IDispatch, (void **)&disp);
                    if (disp)
                    {
                        hr = IConnectionPoint_Advise(cp, (IUnknown *)disp, &cookie);
                        CHECK(hr == S_OK && cookie, "Advise %08lx", hr);
                        hr = IConnectionPoint_EnumConnections(cp, &ec);
                        got = 0; cd.pUnk = NULL;
                        hr = ec ? IEnumConnections_Next(ec, 1, &cd, &got) : E_FAIL;
                        CHECK(hr == S_OK && got == 1 && cd.pUnk && cd.dwCookie == cookie, "the sink is enumerated with its cookie %08lx", hr);
                        if (cd.pUnk) IUnknown_Release(cd.pUnk);
                        if (ec) IEnumConnections_Release(ec);
                        IConnectionPoint_Unadvise(cp, cookie);
                        hr = IConnectionPoint_EnumConnections(cp, &ec);
                        got = 3; hr = ec ? IEnumConnections_Next(ec, 1, &cd, &got) : E_FAIL;
                        CHECK(hr == S_FALSE && got == 0, "gone after Unadvise");
                        if (ec) IEnumConnections_Release(ec);
                        IDispatch_Release(disp);
                    }
                }
                IConnectionPoint_Release(cp);
            }
            IConnectionPointContainer_Release(cpc);
        }
        if (obj) IUnknown_Release(obj);
        (void)sink;
    }
    printf("RESULT: %s\n", fails ? "FAIL" : "PASS");
    return fails;
}
