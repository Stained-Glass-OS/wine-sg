/* msscript.ocx: events through connection points, OLE control interfaces, persistence */
#define COBJMACROS
#include <windows.h>
#include <ole2.h>
#include <ocidl.h>
#include <stdio.h>

static int fails;
#ifndef CONNECT_E_NOCONNECTION
#define CONNECT_E_NOCONNECTION ((HRESULT)0x80040200)
#endif
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static const CLSID CLSID_ScriptControl_ = {0x0e59f1d5, 0x1fbe, 0x11d0, {0x8f, 0xf2, 0x00, 0xa0, 0xd1, 0x00, 0x38, 0xbc}};
static const IID DIID_DScriptControlSource_ = {0x8b167d60, 0x8605, 0x11d0, {0xab, 0xcb, 0x00, 0xa0, 0xc9, 0x0f, 0xff, 0xc0}};

static HRESULT dcall(IDispatch *d, const WCHAR *name, WORD flags, VARIANT *arg, VARIANT *res)
{
    DISPID id, put = DISPID_PROPERTYPUT;
    DISPPARAMS dp = { arg, NULL, arg ? 1 : 0, 0 };
    HRESULT hr;

    if (FAILED(hr = IDispatch_GetIDsOfNames(d, &IID_NULL, (WCHAR **)&name, 1, 0, &id))) return hr;
    if (flags == DISPATCH_PROPERTYPUT) { dp.cNamedArgs = 1; dp.rgdispidNamedArgs = &put; }
    return IDispatch_Invoke(d, id, &IID_NULL, 0, flags, &dp, res, NULL, NULL);
}

/* the events sink */
struct sink { IDispatch IDispatch_iface; LONG ref; int events; int clear; IDispatch *control; };
static HRESULT WINAPI s_qi(IDispatch *iface, REFIID riid, void **obj)
{
    if (IsEqualGUID(riid, &IID_IUnknown) || IsEqualGUID(riid, &IID_IDispatch) || IsEqualGUID(riid, &DIID_DScriptControlSource_))
    { *obj = iface; IDispatch_AddRef(iface); return S_OK; }
    *obj = NULL; return E_NOINTERFACE;
}
static ULONG WINAPI s_addref(IDispatch *iface) { return 2; }
static ULONG WINAPI s_release(IDispatch *iface) { return 1; }
static HRESULT WINAPI s_count(IDispatch *iface, UINT *c) { *c = 0; return S_OK; }
static HRESULT WINAPI s_ti(IDispatch *iface, UINT i, LCID l, ITypeInfo **t) { return E_NOTIMPL; }
static HRESULT WINAPI s_ids(IDispatch *iface, REFIID r, WCHAR **n, UINT c, LCID l, DISPID *d) { return E_NOTIMPL; }
static HRESULT WINAPI s_invoke(IDispatch *iface, DISPID id, REFIID r, LCID l, WORD f, DISPPARAMS *p, VARIANT *v, EXCEPINFO *e, UINT *a)
{
    struct sink *s = (struct sink *)iface;
    if (id != 0xbb8) return DISP_E_MEMBERNOTFOUND;
    s->events++;
    if (s->clear)
    {
        VARIANT err;
        VariantInit(&err);
        if (SUCCEEDED(dcall(s->control, L"Error", DISPATCH_PROPERTYGET, NULL, &err)) && V_VT(&err) == VT_DISPATCH)
        {
            dcall(V_DISPATCH(&err), L"Clear", DISPATCH_METHOD, NULL, NULL);
            IDispatch_Release(V_DISPATCH(&err));
        }
    }
    return S_OK;
}
static IDispatchVtbl sink_vtbl = { s_qi, s_addref, s_release, s_count, s_ti, s_ids, s_invoke };

int main(void)
{
    IDispatch *sc = NULL, *sc2 = NULL;
    IConnectionPointContainer *cpc = NULL;
    IConnectionPoint *cp = NULL;
    IEnumConnectionPoints *ecp = NULL;
    IEnumConnections *ec = NULL;
    IOleObject *oleobj = NULL;
    IViewObjectEx *view = NULL;
    IPersistStreamInit *psi = NULL, *psi2 = NULL;
    IStream *stream = NULL;
    struct sink sink = { { &sink_vtbl }, 1, 0, 0, NULL };
    CONNECTDATA cd;
    VARIANT arg, res;
    DWORD cookie = 0;
    ULONG n;
    CLSID clsid;
    HRESULT hr;

    CoInitialize(NULL);
    hr = CoCreateInstance(&CLSID_ScriptControl_, NULL, CLSCTX_INPROC_SERVER, &IID_IDispatch, (void **)&sc);
    CHECK(hr == S_OK, "create %lx", hr);
    if (!sc) return 1;
    sink.control = sc;
    VariantInit(&arg);
    V_VT(&arg) = VT_BSTR; V_BSTR(&arg) = SysAllocString(L"VBScript");
    hr = dcall(sc, L"Language", DISPATCH_PROPERTYPUT, &arg, NULL);
    CHECK(hr == S_OK, "Language %lx", hr);
    VariantClear(&arg);

    /* connection points */
    hr = IDispatch_QueryInterface(sc, &IID_IConnectionPointContainer, (void **)&cpc);
    CHECK(hr == S_OK, "cpc %lx", hr);
    hr = IConnectionPointContainer_EnumConnectionPoints(cpc, &ecp);
    CHECK(hr == S_OK && ecp, "EnumConnectionPoints %lx", hr);
    n = 0;
    if (ecp) { IConnectionPoint *pts[4]; hr = IEnumConnectionPoints_Next(ecp, 4, pts, &n); CHECK(hr == S_FALSE && n == 2, "points %lx %lu", hr, n); while (n) IConnectionPoint_Release(pts[--n]); IEnumConnectionPoints_Release(ecp); }
    hr = IConnectionPointContainer_FindConnectionPoint(cpc, &DIID_DScriptControlSource_, &cp);
    CHECK(hr == S_OK && cp, "FindConnectionPoint %lx", hr);
    hr = IConnectionPoint_Advise(cp, (IUnknown *)&sink.IDispatch_iface, &cookie);
    CHECK(hr == S_OK && cookie, "Advise %lx cookie %lu", hr, cookie);
    hr = IConnectionPoint_EnumConnections(cp, &ec);
    CHECK(hr == S_OK && ec, "EnumConnections %lx", hr);
    if (ec) { n = 0; hr = IEnumConnections_Next(ec, 1, &cd, &n); CHECK(hr == S_OK && n == 1 && cd.dwCookie == cookie, "connection"); if (n) IUnknown_Release(cd.pUnk); IEnumConnections_Release(ec); }

    /* the Error event: not cleared (the call fails), then cleared (the call succeeds) */
    V_VT(&arg) = VT_BSTR; V_BSTR(&arg) = SysAllocString(L"this is not ( valid");
    hr = dcall(sc, L"ExecuteStatement", DISPATCH_METHOD, &arg, NULL);
    CHECK(FAILED(hr) && sink.events == 1, "bad statement %lx, events %d", hr, sink.events);
    sink.clear = 1;
    hr = dcall(sc, L"ExecuteStatement", DISPATCH_METHOD, &arg, NULL);
    CHECK(hr == S_OK && sink.events == 2, "cleared error %lx, events %d", hr, sink.events);
    VariantClear(&arg);
    V_VT(&arg) = VT_BSTR; V_BSTR(&arg) = SysAllocString(L"1+1");
    VariantInit(&res);
    hr = dcall(sc, L"Eval", DISPATCH_METHOD, &arg, &res);
    CHECK(hr == S_OK && sink.events == 2, "no event on success: %lx events %d", hr, sink.events);
    VariantClear(&arg); VariantClear(&res);
    hr = IConnectionPoint_Unadvise(cp, cookie);
    CHECK(hr == S_OK, "Unadvise %lx", hr);
    hr = IConnectionPoint_Unadvise(cp, cookie);
    CHECK(hr == CONNECT_E_NOCONNECTION, "second Unadvise %lx", hr);
    V_VT(&arg) = VT_BSTR; V_BSTR(&arg) = SysAllocString(L"this is not ( valid");
    dcall(sc, L"ExecuteStatement", DISPATCH_METHOD, &arg, NULL);
    CHECK(sink.events == 2, "event after Unadvise (%d)", sink.events);
    VariantClear(&arg);
    IConnectionPoint_Release(cp);
    IConnectionPointContainer_Release(cpc);

    /* OLE control interfaces */
    hr = IDispatch_QueryInterface(sc, &IID_IOleObject, (void **)&oleobj);
    CHECK(hr == S_OK, "IOleObject %lx", hr);
    if (oleobj)
    {
        SIZEL size = { 1000, 2000 }, got = {0};
        IEnumOLEVERB *verbs = NULL;

        hr = IOleObject_GetUserClassID(oleobj, &clsid);
        CHECK(hr == S_OK && IsEqualGUID(&clsid, &CLSID_ScriptControl_), "GetUserClassID %lx", hr);
        hr = IOleObject_SetHostNames(oleobj, L"app", L"doc");
        CHECK(hr == S_OK, "SetHostNames %lx", hr);
        hr = IOleObject_DoVerb(oleobj, OLEIVERB_SHOW, NULL, NULL, 0, NULL, NULL);
        CHECK(hr == S_OK, "DoVerb(SHOW) %lx", hr);
        hr = IOleObject_Update(oleobj);
        CHECK(hr == S_OK, "Update %lx", hr);
        hr = IOleObject_SetExtent(oleobj, DVASPECT_CONTENT, &size);
        CHECK(hr == S_OK, "SetExtent %lx", hr);
        hr = IOleObject_GetExtent(oleobj, DVASPECT_CONTENT, &got);
        CHECK(hr == S_OK && got.cx == 1000 && got.cy == 2000, "GetExtent %lx %ldx%ld", hr, got.cx, got.cy);
        hr = IOleObject_SetExtent(oleobj, DVASPECT_ICON, &size);
        CHECK(hr == DV_E_DVASPECT, "SetExtent(icon) %lx", hr);
        hr = IOleObject_SetMoniker(oleobj, OLEWHICHMK_CONTAINER, NULL);
        CHECK(hr == E_NOTIMPL, "SetMoniker %lx", hr);
        hr = IOleObject_EnumVerbs(oleobj, &verbs);
        if (verbs) IEnumOLEVERB_Release(verbs);
        IOleObject_Release(oleobj);
    }
    hr = IDispatch_QueryInterface(sc, &IID_IViewObjectEx, (void **)&view);
    CHECK(hr == S_OK, "IViewObjectEx %lx", hr);
    if (view)
    {
        RECT bounds = { 0, 0, 10, 10 };
        POINT in = { 5, 5 }, out = { 50, 50 };
        DWORD hit = 99;

        hr = IViewObjectEx_QueryHitPoint(view, DVASPECT_CONTENT, &bounds, in, 0, &hit);
        CHECK(hr == S_OK && hit == HITRESULT_HIT, "hit inside %lx %lu", hr, hit);
        hr = IViewObjectEx_QueryHitPoint(view, DVASPECT_CONTENT, &bounds, out, 0, &hit);
        CHECK(hr == S_OK && hit == HITRESULT_OUTSIDE, "hit outside %lx %lu", hr, hit);
        hr = IViewObjectEx_Draw(view, DVASPECT_CONTENT, -1, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 0);
        CHECK(hr == S_OK, "Draw %lx", hr);
        IViewObjectEx_Release(view);
    }

    /* persistence */
    V_VT(&arg) = VT_I4; V_I4(&arg) = 4321;
    hr = dcall(sc, L"Timeout", DISPATCH_PROPERTYPUT, &arg, NULL);
    CHECK(hr == S_OK, "put Timeout %lx", hr);
    hr = IDispatch_QueryInterface(sc, &IID_IPersistStreamInit, (void **)&psi);
    CHECK(hr == S_OK, "IPersistStreamInit %lx", hr);
    hr = CreateStreamOnHGlobal(NULL, TRUE, &stream);
    if (psi && stream)
    {
        hr = IPersistStreamInit_IsDirty(psi);
        CHECK(hr == S_OK, "IsDirty %lx", hr);
        hr = IPersistStreamInit_Save(psi, stream, TRUE);
        CHECK(hr == S_OK, "Save %lx", hr);
        hr = IPersistStreamInit_IsDirty(psi);
        CHECK(hr == S_FALSE, "IsDirty after save %lx", hr);
        IStream_Seek(stream, (LARGE_INTEGER){{0}}, STREAM_SEEK_SET, NULL);
        hr = CoCreateInstance(&CLSID_ScriptControl_, NULL, CLSCTX_INPROC_SERVER, &IID_IDispatch, (void **)&sc2);
        IDispatch_QueryInterface(sc2, &IID_IPersistStreamInit, (void **)&psi2);
        hr = IPersistStreamInit_Load(psi2, stream);
        CHECK(hr == S_OK, "Load %lx", hr);
        VariantInit(&res);
        hr = dcall(sc2, L"Timeout", DISPATCH_PROPERTYGET, NULL, &res);
        CHECK(hr == S_OK && V_I4(&res) == 4321, "loaded Timeout %lx %ld", hr, V_I4(&res));
        hr = IPersistStreamInit_GetClassID(psi2, &clsid);
        CHECK(hr == S_OK && IsEqualGUID(&clsid, &CLSID_ScriptControl_), "GetClassID %lx", hr);
        IPersistStreamInit_Release(psi2);
        IDispatch_Release(sc2);
    }
    if (stream) IStream_Release(stream);
    if (psi) IPersistStreamInit_Release(psi);
    IDispatch_Release(sc);
    printf("RESULT: %s\n", fails ? "FAIL" : "PASS");
    return fails;
}
