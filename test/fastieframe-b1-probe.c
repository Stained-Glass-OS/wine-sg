#define COBJMACROS
#include <windows.h>
#include <ole2.h>
#include <ocidl.h>
#include <exdisp.h>
#include <exdispid.h>
#include <hlink.h>
#include <htiface.h>
#include <olectl.h>
#include <urlhist.h>
#include <stdio.h>
#define P(...) printf(__VA_ARGS__)
static int fails, checks;
#define CHK(c) do { checks++; if (!(c)) { fails++; P("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static const CLSID CLSID_WB = {0x8856f961,0x340a,0x11d0,{0xa9,0x6b,0x00,0xc0,0x4f,0xd7,0x05,0xa2}};
static const CLSID CLSID_CUrlHistory_ = {0x3c374a40,0xbae4,0x11cf,{0xbf,0x7d,0x00,0xaa,0x00,0x69,0x46,0xee}};
static const GUID IID_ITF2 = {0x86d52e11,0x94a8,0x11d0,{0x82,0xaf,0x00,0xc0,0x4f,0xd5,0xae,0x38}};
static const GUID IID_ITF = {0xd5f78c80,0x5252,0x11cf,{0x90,0xfa,0x00,0xaa,0x00,0x42,0x10,0x6e}};
static const GUID IID_IHF = {0x79eac9c5,0xbaf9,0x11ce,{0x8c,0x82,0x00,0xaa,0x00,0x4b,0xa9,0x0b}};

/* sink counting events */
static int fired;
static HRESULT WINAPI sink_QI(IDispatch *i, REFIID r, void **p) { if (IsEqualGUID(r,&IID_IUnknown)||IsEqualGUID(r,&IID_IDispatch)||IsEqualGUID(r,&DIID_DWebBrowserEvents2)) { *p = i; return S_OK; } *p = NULL; return E_NOINTERFACE; }
static ULONG WINAPI sink_AR(IDispatch *i) { return 2; }
static HRESULT WINAPI sink_GTC(IDispatch *i, UINT *c) { *c = 0; return S_OK; }
static HRESULT WINAPI sink_GTI(IDispatch *i, UINT n, LCID l, ITypeInfo **t) { return E_NOTIMPL; }
static HRESULT WINAPI sink_GID(IDispatch *i, REFIID r, LPOLESTR *n, UINT c, LCID l, DISPID *d) { return E_NOTIMPL; }
static HRESULT WINAPI sink_Inv(IDispatch *i, DISPID d, REFIID r, LCID l, WORD f, DISPPARAMS *p, VARIANT *v, EXCEPINFO *e, UINT *a)
{ if (d == DISPID_STATUSTEXTCHANGE || d == DISPID_ONVISIBLE || d == DISPID_ONSTATUSBAR) fired++; return S_OK; }
static IDispatchVtbl sink_vtbl = { sink_QI, sink_AR, sink_AR, sink_GTC, sink_GTI, sink_GID, sink_Inv };
static IDispatch sink = { &sink_vtbl };

/* container for the control: a client site with an IOleContainer-less answer, IDispatch */
typedef struct { IOleClientSite cs; IDispatch disp; } Site;
static HRESULT WINAPI cs_QI(IOleClientSite *i, REFIID r, void **p)
{
    Site *s = (Site*)i;
    if (IsEqualGUID(r,&IID_IUnknown)||IsEqualGUID(r,&IID_IOleClientSite)) { *p = i; return S_OK; }
    if (IsEqualGUID(r,&IID_IDispatch)) { *p = &s->disp; return S_OK; }
    *p = NULL; return E_NOINTERFACE;
}
static ULONG WINAPI cs_AR(IOleClientSite *i) { return 2; }
static HRESULT WINAPI cs_Save(IOleClientSite *i) { return E_NOTIMPL; }
static HRESULT WINAPI cs_Mk(IOleClientSite *i, DWORD a, DWORD b, IMoniker **m) { return E_NOTIMPL; }
static IDispatch *container_disp;
static HRESULT WINAPI cs_Cont(IOleClientSite *i, IOleContainer **c) { *c = NULL; return E_NOINTERFACE; }
static HRESULT WINAPI cs_Show(IOleClientSite *i) { return S_OK; }
static HRESULT WINAPI cs_OnShow(IOleClientSite *i, BOOL b) { return S_OK; }
static HRESULT WINAPI cs_Req(IOleClientSite *i) { return S_OK; }
static IOleClientSiteVtbl cs_vtbl = { cs_QI, cs_AR, cs_AR, cs_Save, cs_Mk, cs_Cont, cs_Show, cs_OnShow, cs_Req };
static HRESULT WINAPI d_QI(IDispatch *i, REFIID r, void **p) { return cs_QI((IOleClientSite*)((char*)i - offsetof(Site, disp)), r, p); }
static ULONG WINAPI d_AR(IDispatch *i) { return 2; }
static HRESULT WINAPI d_Inv(IDispatch *i, DISPID d, REFIID r, LCID l, WORD f, DISPPARAMS *p, VARIANT *v, EXCEPINFO *e, UINT *a) { return DISP_E_MEMBERNOTFOUND; }
static IDispatchVtbl d_vtbl = { d_QI, d_AR, d_AR, sink_GTC, sink_GTI, sink_GID, d_Inv };

int main(void)
{
    IWebBrowser2 *wb; IOleObject *oo; IUnknown *unk; HRESULT hr; BSTR b; VARIANT v, v2; VARIANT_BOOL vb;
    IDispatch *disp; Site site; IConnectionPointContainer *cpc; IConnectionPoint *cp; DWORD cookie;
    setvbuf(stdout, NULL, _IONBF, 0);
    OleInitialize(NULL);
    hr = CoCreateInstance(&CLSID_WB, NULL, CLSCTX_INPROC_SERVER, &IID_IWebBrowser2, (void**)&wb);
    if (FAILED(hr)) { P("FAIL: no WebBrowser %lx\nRESULT: FAIL\n", hr); return 1; }
    site.cs.lpVtbl = &cs_vtbl; site.disp.lpVtbl = &d_vtbl;
    IWebBrowser2_QueryInterface(wb, &IID_IOleObject, (void**)&oo);
    IOleObject_SetClientSite(oo, &site.cs);

    /* Parent / Container / TopLevelContainer */
    disp = (IDispatch*)1;
    hr = IWebBrowser2_get_Parent(wb, &disp);
    CHK(hr == S_OK && disp != (IDispatch*)1); /* this container has no IOleContainer: NULL */
    if (disp && disp != (IDispatch*)1) IDispatch_Release(disp);
    hr = IWebBrowser2_get_Container(wb, &disp);
    CHK(hr == S_OK);
    hr = IWebBrowser2_get_TopLevelContainer(wb, &vb);
    CHK(hr == S_OK && vb == VARIANT_FALSE);

    /* Type with no document */
    b = NULL; hr = IWebBrowser2_get_Type(wb, &b);
    CHK(hr == S_OK && b != NULL); SysFreeString(b);

    /* properties */
    b = SysAllocString(L"MyProp"); VariantInit(&v); V_VT(&v) = VT_I4; V_I4(&v) = 77;
    hr = IWebBrowser2_PutProperty(wb, b, v); CHK(hr == S_OK);
    VariantInit(&v2); hr = IWebBrowser2_GetProperty(wb, b, &v2);
    CHK(hr == S_OK && V_VT(&v2) == VT_I4 && V_I4(&v2) == 77);
    SysFreeString(b); b = SysAllocString(L"myprop"); VariantInit(&v2);
    hr = IWebBrowser2_GetProperty(wb, b, &v2); CHK(hr == S_OK && V_VT(&v2) == VT_I4); SysFreeString(b);
    b = SysAllocString(L"other"); hr = IWebBrowser2_GetProperty(wb, b, &v2); CHK(hr == S_OK && V_VT(&v2) == VT_EMPTY); SysFreeString(b);

    /* paths */
    b = NULL; hr = IWebBrowser2_get_FullName(wb, &b); CHK(hr == S_OK && b && wcsstr(b, L".exe")); SysFreeString(b);
    b = NULL; hr = IWebBrowser2_get_Path(wb, &b); CHK(hr == S_OK && b && b[wcslen(b)-1] == '\\'); SysFreeString(b);

    /* status text + events */
    IWebBrowser2_QueryInterface(wb, &IID_IConnectionPointContainer, (void**)&cpc);
    IConnectionPointContainer_FindConnectionPoint(cpc, &DIID_DWebBrowserEvents2, &cp);
    IConnectionPoint_Advise(cp, (IUnknown*)&sink, &cookie);
    b = SysAllocString(L"hello"); hr = IWebBrowser2_put_StatusText(wb, b); CHK(hr == S_OK); SysFreeString(b);
    b = NULL; hr = IWebBrowser2_get_StatusText(wb, &b); CHK(hr == S_OK && b && !wcscmp(b, L"hello")); SysFreeString(b);

    /* register flags */
    hr = IWebBrowser2_put_RegisterAsDropTarget(wb, VARIANT_TRUE); CHK(hr == S_OK);
    vb = 0; hr = IWebBrowser2_get_RegisterAsDropTarget(wb, &vb); CHK(hr == S_OK && vb == VARIANT_TRUE);
    CHK(IWebBrowser2_get_RegisterAsDropTarget(wb, NULL) == E_INVALIDARG);

    /* ClientToWindow, ShowBrowserBar */
    { int cx = 10, cy = 20; hr = IWebBrowser2_ClientToWindow(wb, &cx, &cy); CHK(hr == S_OK && cx == 10 && cy == 20); }
    CHK(IWebBrowser2_get_Resizable(wb, &vb) == E_NOTIMPL);

    /* FreezeEvents suppresses events */
    {
        IOleControl *ctl;
        IWebBrowser2_QueryInterface(wb, &IID_IOleControl, (void**)&ctl);
        fired = 0; IWebBrowser2_put_StatusBar(wb, VARIANT_TRUE); CHK(fired == 1);
        CHK(IOleControl_FreezeEvents(ctl, TRUE) == S_OK);
        fired = 0; IWebBrowser2_put_StatusBar(wb, VARIANT_FALSE); CHK(fired == 0);
        CHK(IOleControl_FreezeEvents(ctl, FALSE) == S_OK);
        fired = 0; IWebBrowser2_put_StatusBar(wb, VARIANT_TRUE); CHK(fired == 1);
        CHK(IOleControl_OnAmbientPropertyChange(ctl, (-5513)) == S_OK);
        IOleControl_Release(ctl);
    }

    /* verbs */
    {
        IEnumOLEVERB *e1, *e2; OLEVERB verbs[8]; ULONG got;
        hr = IOleObject_EnumVerbs(oo, &e1); CHK(hr == S_OK);
        got = 0; IEnumOLEVERB_Next(e1, 1, verbs, &got);
        hr = IEnumOLEVERB_Clone(e1, &e2); CHK(hr == S_OK && e2);
        if (e2) { got = 0; hr = IEnumOLEVERB_Next(e2, 1, verbs, &got); CHK(hr == S_OK && verbs[0].lVerb == OLEIVERB_INPLACEACTIVATE); IEnumOLEVERB_Release(e2); }
        IEnumOLEVERB_Release(e1);
        { HRESULT hp = IOleObject_DoVerb(oo, OLEIVERB_PRIMARY, NULL, &site.cs, -1, NULL, NULL);
          HRESULT hu = IOleObject_DoVerb(oo, 12345, NULL, &site.cs, -1, NULL, NULL);
          CHK(hu == (SUCCEEDED(hp) ? OLEOBJ_S_INVALIDVERB : hp)); }
        CHK(IOleObject_DoVerb(oo, -1234, NULL, &site.cs, -1, NULL, NULL) == OLEOBJ_E_INVALIDVERB);
        CHK(IOleObject_Update(oo) == S_OK);
        CHK(IOleObject_IsUpToDate(oo) == S_OK);
        CHK(IOleObject_Close(oo, 7) == E_INVALIDARG);
    }
    {
        IOleInPlaceObject *ip;
        IWebBrowser2_QueryInterface(wb, &IID_IOleInPlaceObject, (void**)&ip);
        CHK(IOleInPlaceObject_ReactivateAndUndo(ip) == INPLACE_E_NOTUNDOABLE);
        IOleInPlaceObject_Release(ip);
    }

    /* persistence */
    {
        IPersistStreamInit *psi; IPersistMemory *pm; ULARGE_INTEGER sz; ULONG msz = 0; BYTE buf[256]; IStream *st;
        IWebBrowser2_QueryInterface(wb, &IID_IPersistStreamInit, (void**)&psi);
        IWebBrowser2_QueryInterface(wb, &IID_IPersistMemory, (void**)&pm);
        CHK(IPersistStreamInit_InitNew(psi) == S_OK);
        CHK(IPersistStreamInit_IsDirty(psi) == S_FALSE);
        CHK(IPersistStreamInit_GetSizeMax(psi, &sz) == S_OK && sz.QuadPart > 0);
        CHK(IPersistMemory_GetSizeMax(pm, &msz) == S_OK && msz > 0);
        IWebBrowser2_put_RegisterAsBrowser(wb, VARIANT_TRUE);
        CHK(IPersistStreamInit_IsDirty(psi) == S_OK);
        memset(buf, 0, sizeof(buf));
        CHK(IPersistMemory_Save(pm, buf, TRUE, msz) == S_OK);
        CHK(IPersistMemory_IsDirty(pm) == S_FALSE);
        IWebBrowser2_put_RegisterAsBrowser(wb, VARIANT_FALSE);
        CHK(IPersistMemory_IsDirty(pm) == S_OK);
        CHK(IPersistMemory_Load(pm, buf, msz) == S_OK);
        vb = 0; IWebBrowser2_get_RegisterAsBrowser(wb, &vb); CHK(vb == VARIANT_TRUE);
        CHK(IPersistMemory_IsDirty(pm) == S_FALSE);
        /* stream round trip */
        CreateStreamOnHGlobal(NULL, TRUE, &st);
        CHK(IPersistStreamInit_Save(psi, st, TRUE) == S_OK);
        IWebBrowser2_put_RegisterAsBrowser(wb, VARIANT_FALSE);
        { LARGE_INTEGER z = {{0}}; IStream_Seek(st, z, STREAM_SEEK_SET, NULL); }
        CHK(IPersistStreamInit_Load(psi, st) == S_OK);
        vb = 0; IWebBrowser2_get_RegisterAsBrowser(wb, &vb); CHK(vb == VARIANT_TRUE);
        IStream_Release(st);
        /* foreign data is accepted */
        memset(buf, 0x55, sizeof(buf));
        CHK(IPersistMemory_Load(pm, buf, 100) == S_OK);
        IPersistStreamInit_Release(psi); IPersistMemory_Release(pm);
    }

    /* target frame / hlink frame */
    {
        ITargetFrame2 *tf; IHlinkFrame *hf; IUnknown *f = NULL; LPWSTR s = NULL; DWORD o = 0, w = 0, h = 0;
        hr = IWebBrowser2_QueryInterface(wb, &IID_ITF2, (void**)&tf); CHK(hr == S_OK);
        if (SUCCEEDED(hr)) {
            hr = ITargetFrame2_FindFrame(tf, L"_self", 0, &f); CHK(hr == S_OK && f); if (f) IUnknown_Release(f);
            f = (IUnknown*)1; hr = ITargetFrame2_FindFrame(tf, L"nosuchframe", 0, &f); CHK(hr == S_OK && f == NULL);
            hr = ITargetFrame2_GetParentFrame(tf, &f); CHK(hr == S_OK && f == NULL);
            CHK(ITargetFrame2_SetFrameOptions(tf, 0x1b) == S_OK);
            CHK(ITargetFrame2_GetFrameOptions(tf, &o) == S_OK && o == 0x1b);
            CHK(ITargetFrame2_SetFrameMargins(tf, 5, 6) == S_OK);
            CHK(ITargetFrame2_GetFrameMargins(tf, &w, &h) == S_OK && w == 5 && h == 6);
            hr = ITargetFrame2_GetFrameSrc(tf, &s); CHK(hr == S_OK && s != NULL); CoTaskMemFree(s);
            ITargetFrame2_Release(tf);
        }
        hr = IWebBrowser2_QueryInterface(wb, &IID_IHF, (void**)&hf); CHK(hr == S_OK);
        if (SUCCEEDED(hr)) {
            IHlinkBrowseContext *bc = NULL, *bc2 = NULL;
            HlinkCreateBrowseContext(NULL, &IID_IHlinkBrowseContext, (void**)&bc);
            CHK(bc != NULL);
            CHK(IHlinkFrame_SetBrowseContext(hf, bc) == S_OK);
            CHK(IHlinkFrame_GetBrowseContext(hf, &bc2) == S_OK && bc2 == bc);
            if (bc2) IHlinkBrowseContext_Release(bc2);
            IHlinkBrowseContext_Release(bc);
            IHlinkFrame_Release(hf);
        }
    }

    IConnectionPoint_Unadvise(cp, cookie);
    IConnectionPoint_Release(cp); IConnectionPointContainer_Release(cpc);
    IOleObject_Close(oo, OLECLOSE_NOSAVE);
    IOleObject_Release(oo);
    IWebBrowser2_Release(wb);

    /* URL history */
    {
        IUrlHistoryStg2 *hist; IEnumSTATURL *en; STATURL st; ULONG got; int found = 0;
        hr = CoCreateInstance(&CLSID_CUrlHistory_, NULL, CLSCTX_INPROC_SERVER, &IID_IUrlHistoryStg2, (void**)&hist);
        CHK(hr == S_OK);
        if (SUCCEEDED(hr)) {
            CHK(IUrlHistoryStg2_ClearHistory(hist) == S_OK);
            CHK(IUrlHistoryStg2_AddUrl(hist, L"http://sg.test/a.html", L"Title A", 0) == S_OK);
            CHK(IUrlHistoryStg2_AddUrl(hist, L"http://sg.test/b.html", NULL, 0) == S_OK);
            memset(&st, 0, sizeof(st));
            hr = IUrlHistoryStg2_QueryUrl(hist, L"http://sg.test/a.html", 0, &st);
            CHK(hr == S_OK && st.pwcsUrl && !wcscmp(st.pwcsUrl, L"http://sg.test/a.html"));
            CHK(st.pwcsTitle && !wcscmp(st.pwcsTitle, L"Title A"));
            CHK(st.ftLastVisited.dwHighDateTime != 0);
            CoTaskMemFree(st.pwcsUrl); CoTaskMemFree(st.pwcsTitle);
            hr = IUrlHistoryStg2_QueryUrl(hist, L"http://sg.test/a.html", STATURL_QUERYFLAG_NOURL | STATURL_QUERYFLAG_NOTITLE, &st);
            CHK(hr == S_OK && st.pwcsUrl == NULL && st.pwcsTitle == NULL);
            CHK(FAILED(IUrlHistoryStg2_QueryUrl(hist, L"http://sg.test/none.html", 0, &st)));
            hr = IUrlHistoryStg2_EnumUrls(hist, &en); CHK(hr == S_OK);
            if (SUCCEEDED(hr)) {
                while (IEnumSTATURL_Next(en, 1, &st, &got) == S_OK) {
                    if (st.pwcsUrl && wcsstr(st.pwcsUrl, L"sg.test")) found++;
                    CoTaskMemFree(st.pwcsUrl); CoTaskMemFree(st.pwcsTitle);
                }
                CHK(found == 2);
                IEnumSTATURL_Release(en);
            }
            CHK(IUrlHistoryStg2_DeleteUrl(hist, L"http://sg.test/a.html", 0) == S_OK);
            CHK(FAILED(IUrlHistoryStg2_QueryUrl(hist, L"http://sg.test/a.html", 0, &st)));
            CHK(IUrlHistoryStg2_ClearHistory(hist) == S_OK);
            CHK(FAILED(IUrlHistoryStg2_QueryUrl(hist, L"http://sg.test/b.html", 0, &st)));
            IUrlHistoryStg2_Release(hist);
        }
    }
    P("checks %d fails %d\nRESULT: %s\n", checks, fails, fails ? "FAIL" : "PASS");
    return fails != 0;
}
