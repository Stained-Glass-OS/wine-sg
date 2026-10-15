#define COBJMACROS
#include <windows.h>
#include <ole2.h>
#include <ocidl.h>
#include <exdisp.h>
#include <exdispid.h>
#include <mshtml.h>
#include <mshtmhst.h>
#include <tlogstg.h>
#include <urlmon.h>
#include <docobj.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <shdeprecated.h>
#include <docobjectservice.h>
#include <stdio.h>
#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a) (sizeof(a)/sizeof((a)[0]))
#endif
#ifndef INTERNET_MAX_URL_LENGTH
#define INTERNET_MAX_URL_LENGTH 2084
#endif
#include <string.h>

/* the import libraries lack these two */
const GUID IID_IBrowserService = {0x02ba3b52,0x0547,0x11d1,{0xb8,0x33,0x00,0xc0,0x4f,0xc9,0xb3,0x1f}};
const GUID IID_IDocObjectService = {0x3050f801,0x98b5,0x11cf,{0xbb,0x82,0x00,0xaa,0x00,0xbd,0xce,0x0b}};
const GUID IID_ITravelLogClient = {0x241c033e,0xe659,0x43da,{0xaa,0x4d,0x40,0x86,0xdb,0xc4,0x75,0x8d}};
static int bad, total;
#define EXPECT(c) do { total++; if (!(c)) { bad++; printf("MISMATCH line %d: %s\n", __LINE__, #c); } } while (0)
#define EXPECT_HR(e, c) do { HRESULT h_ = (c); total++; if (h_ != (e)) { bad++; printf("MISMATCH line %d: %s = %lx, expected %lx\n", __LINE__, #c, h_, (HRESULT)(e)); } } while (0)

static HWND hwnd;
static int ev_title, ev_dlbegin, ev_dlcomplete, ev_doccomplete;
static WCHAR ev_titletext[200];

typedef struct { const void *vtbl; } obj;
static HRESULT WINAPI site_QI(IOleClientSite *i, REFIID riid, void **ppv);
static ULONG WINAPI ukn_AddRef(IUnknown *i) { return 2; }
static ULONG WINAPI ukn_Release(IUnknown *i) { return 1; }
static HRESULT WINAPI cs_SaveObject(IOleClientSite *i) { return E_NOTIMPL; }
static HRESULT WINAPI cs_GetMoniker(IOleClientSite *i, DWORD a, DWORD b, IMoniker **m) { return E_NOTIMPL; }
static HRESULT WINAPI cs_GetContainer(IOleClientSite *i, IOleContainer **c) { *c = NULL; return E_NOINTERFACE; }
static HRESULT WINAPI cs_ShowObject(IOleClientSite *i) { return S_OK; }
static HRESULT WINAPI cs_OnShowWindow(IOleClientSite *i, BOOL f) { return S_OK; }
static HRESULT WINAPI cs_RequestNewObjectLayout(IOleClientSite *i) { return E_NOTIMPL; }
static IOleClientSiteVtbl csvtbl = { site_QI, (void *)ukn_AddRef, (void *)ukn_Release, cs_SaveObject, cs_GetMoniker,
    cs_GetContainer, cs_ShowObject, cs_OnShowWindow, cs_RequestNewObjectLayout };
static IOleClientSite clientsite = { &csvtbl };

static HRESULT WINAPI ips_GetWindow(IOleInPlaceSite *i, HWND *w) { *w = hwnd; return S_OK; }
static HRESULT WINAPI ips_CSH(IOleInPlaceSite *i, BOOL f) { return E_NOTIMPL; }
static HRESULT WINAPI ips_Can(IOleInPlaceSite *i) { return S_OK; }
static HRESULT WINAPI ips_OnIPA(IOleInPlaceSite *i) { return S_OK; }
static HRESULT WINAPI ips_OnUIA(IOleInPlaceSite *i) { return S_OK; }
static HRESULT WINAPI ips_GWC(IOleInPlaceSite *i, IOleInPlaceFrame **f, IOleInPlaceUIWindow **d, RECT *pos, RECT *clip, OLEINPLACEFRAMEINFO *info)
{
    *f = NULL; *d = NULL;
    GetClientRect(hwnd, pos); *clip = *pos;
    info->fMDIApp = FALSE; info->hwndFrame = hwnd; info->haccel = NULL; info->cAccelEntries = 0;
    return E_NOTIMPL;
}
static HRESULT WINAPI ips_Scroll(IOleInPlaceSite *i, SIZE s) { return E_NOTIMPL; }
static HRESULT WINAPI ips_OnUIDeact(IOleInPlaceSite *i, BOOL u) { return S_OK; }
static HRESULT WINAPI ips_OnIPDeact(IOleInPlaceSite *i) { return S_OK; }
static HRESULT WINAPI ips_Discard(IOleInPlaceSite *i) { return E_NOTIMPL; }
static HRESULT WINAPI ips_DeactUndo(IOleInPlaceSite *i) { return E_NOTIMPL; }
static HRESULT WINAPI ips_OnPos(IOleInPlaceSite *i, const RECT *r) { return S_OK; }
static IOleInPlaceSiteVtbl ipsvtbl = { (void *)site_QI, (void *)ukn_AddRef, (void *)ukn_Release, ips_GetWindow, ips_CSH, ips_Can, ips_OnIPA,
    ips_OnUIA, ips_GWC, ips_Scroll, ips_OnUIDeact, ips_OnIPDeact, ips_Discard, ips_DeactUndo, ips_OnPos };
static IOleInPlaceSite inplace = { &ipsvtbl };

/* the container's own command target sees what the page asks for with http-equiv */
static int cmd_calls;
static DWORD last_cmd;
static HRESULT WINAPI ct_QS(IOleCommandTarget *i, const GUID *g, ULONG c, OLECMD *cmds, OLECMDTEXT *t) { return OLECMDERR_E_NOTSUPPORTED; }
static HRESULT WINAPI ct_Exec(IOleCommandTarget *i, const GUID *g, DWORD id, DWORD opt, VARIANT *in, VARIANT *out)
{
    if (id == OLECMDID_HTTPEQUIV || id == OLECMDID_HTTPEQUIV_DONE) { cmd_calls++; last_cmd = id; return S_OK; }
    return OLECMDERR_E_NOTSUPPORTED;
}
static IOleCommandTargetVtbl ctvtbl = { (void *)site_QI, (void *)ukn_AddRef, (void *)ukn_Release, ct_QS, ct_Exec };
static IOleCommandTarget cmdtarget = { &ctvtbl };

static int show_calls;
static WCHAR last_text[300], last_caption[100];
static HRESULT WINAPI su_ShowMessage(IDocHostShowUI *i, HWND w, LPOLESTR text, LPOLESTR caption, DWORD type, LPOLESTR hf, DWORD hc, LRESULT *r)
{
    show_calls++;
    lstrcpynW(last_text, text ? text : L"", 300);
    lstrcpynW(last_caption, caption ? caption : L"", 100);
    *r = IDOK;
    return S_OK;
}
static HRESULT WINAPI su_ShowHelp(IDocHostShowUI *i, HWND w, LPOLESTR f, UINT c, DWORD d, POINT p, IDispatch *o) { return S_FALSE; }
static const GUID IID_IDocHostShowUI_ = {0xc4d244b0,0xd43e,0x11cf,{0x89,0x3b,0x00,0xaa,0x00,0xbd,0xce,0x1a}};
static IDocHostShowUIVtbl suvtbl = { (void *)site_QI, (void *)ukn_AddRef, (void *)ukn_Release, su_ShowMessage, su_ShowHelp };
static IDocHostShowUI showui = { &suvtbl };

static HRESULT WINAPI site_QI(IOleClientSite *i, REFIID riid, void **ppv)
{
    if (IsEqualGUID(riid, &IID_IUnknown) || IsEqualGUID(riid, &IID_IOleClientSite)) *ppv = &clientsite;
    else if (IsEqualGUID(riid, &IID_IOleInPlaceSite) || IsEqualGUID(riid, &IID_IOleWindow)) *ppv = &inplace;
    else if (IsEqualGUID(riid, &IID_IOleCommandTarget)) *ppv = &cmdtarget;
    else if (IsEqualGUID(riid, &IID_IDocHostShowUI_)) *ppv = &showui;
    else { *ppv = NULL; return E_NOINTERFACE; }
    return S_OK;
}

/* DWebBrowserEvents2 sink */
static HRESULT WINAPI sink_QI(IDispatch *i, REFIID r, void **p)
{
    if (IsEqualGUID(r, &IID_IUnknown) || IsEqualGUID(r, &IID_IDispatch) || IsEqualGUID(r, &DIID_DWebBrowserEvents2)) { *p = i; return S_OK; }
    *p = NULL; return E_NOINTERFACE;
}
static HRESULT WINAPI sink_GTC(IDispatch *i, UINT *c) { *c = 0; return S_OK; }
static HRESULT WINAPI sink_GTI(IDispatch *i, UINT n, LCID l, ITypeInfo **t) { return E_NOTIMPL; }
static HRESULT WINAPI sink_GID(IDispatch *i, REFIID r, LPOLESTR *n, UINT c, LCID l, DISPID *d) { return E_NOTIMPL; }
static HRESULT WINAPI sink_Inv(IDispatch *i, DISPID d, REFIID r, LCID l, WORD f, DISPPARAMS *p, VARIANT *v, EXCEPINFO *e, UINT *a)
{
    if (d == DISPID_TITLECHANGE) { ev_title++; lstrcpynW(ev_titletext, V_BSTR(&p->rgvarg[0]), 200); }
    if (d == DISPID_DOWNLOADBEGIN) ev_dlbegin++;
    if (d == DISPID_DOWNLOADCOMPLETE) ev_dlcomplete++;
    if (d == DISPID_DOCUMENTCOMPLETE) ev_doccomplete++;
    return S_OK;
}
static IDispatchVtbl sink_vtbl = { sink_QI, (void *)ukn_AddRef, (void *)ukn_Release, sink_GTC, sink_GTI, sink_GID, sink_Inv };
static IDispatch sink = { &sink_vtbl };

static void pump(DWORD ms)
{
    DWORD end = GetTickCount() + ms;
    MSG msg;
    while (GetTickCount() < end) {
        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
        Sleep(10);
    }
}

static DWORD WINAPI watchdog(void *arg)
{
    Sleep(150000);
    printf("watchdog: stuck\nRESULT: FAIL\n");
    fflush(stdout);
    ExitProcess(2);
}

static IWebBrowser2 *wb;

static void navigate(const WCHAR *url)
{
    VARIANT v;
    READYSTATE rs = 0;
    int i, before = ev_doccomplete;

    VariantInit(&v);
    V_VT(&v) = VT_BSTR;
    V_BSTR(&v) = SysAllocString(url);
    EXPECT_HR(S_OK, IWebBrowser2_Navigate2(wb, &v, NULL, NULL, NULL, NULL));
    VariantClear(&v);
    for (i = 0; i < 300; i++) {
        pump(50);
        IWebBrowser2_get_ReadyState(wb, &rs);
        if (rs == READYSTATE_COMPLETE && ev_doccomplete > before) break;
    }
    EXPECT(rs == READYSTATE_COMPLETE);
}

static void write_file(const WCHAR *path, const char *text)
{
    HANDLE h = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    DWORD n;
    WriteFile(h, text, strlen(text), &n, NULL);
    CloseHandle(h);
}

static BOOL ends_with(const WCHAR *s, const WCHAR *suffix)
{
    size_t a = wcslen(s), b = wcslen(suffix);
    return a >= b && !wcsicmp(s + a - b, suffix);
}

static BSTR current_url(void)
{
    BSTR u = NULL;
    IWebBrowser2_get_LocationURL(wb, &u);
    return u;
}

int main(void)
{
    IOleObject *oleobj;
    IDispatch *disp = NULL;
    IHTMLDocument2 *doc = NULL;
    IHTMLWindow2 *win = NULL;
    IConnectionPointContainer *cpc;
    IConnectionPoint *cp;
    DWORD cookie;
    HRESULT hr;
    BSTR b;

    setvbuf(stdout, NULL, _IONBF, 0);
    CreateThread(NULL, 0, watchdog, NULL, 0, NULL);
    CoInitialize(NULL);
    hwnd = CreateWindowExA(0, "static", "host", WS_POPUP, 0, 0, 400, 300, NULL, NULL, NULL, NULL);
    write_file(L"C:\\sgm1.html", "<html><head><meta http-equiv=\"x-ua-compatible\" content=\"IE=9\"><title>the title</title></head><body><select id=s><option id=o1>one</option><option id=o2>two</option></select>"
               "<div id=d onclick='x'>text</div></body></html>");

    hr = CoCreateInstance(&CLSID_WebBrowser, NULL, CLSCTX_INPROC_SERVER, &IID_IWebBrowser2, (void **)&wb);
    EXPECT(hr == S_OK);
    if (hr != S_OK) { printf("RESULT: FAIL\n"); return 1; }
    IWebBrowser2_QueryInterface(wb, &IID_IOleObject, (void **)&oleobj);
    IOleObject_SetClientSite(oleobj, &clientsite);
    { RECT rc = {0, 0, 400, 300}; IOleObject_DoVerb(oleobj, OLEIVERB_INPLACEACTIVATE, NULL, &clientsite, 0, hwnd, &rc); }
    IWebBrowser2_QueryInterface(wb, &IID_IConnectionPointContainer, (void **)&cpc);
    IConnectionPointContainer_FindConnectionPoint(cpc, &DIID_DWebBrowserEvents2, &cp);
    IConnectionPoint_Advise(cp, (IUnknown *)&sink, &cookie);

    /* ---- ITravelLogClient::GetWindowData (family: windowdata) ---- */
    navigate(L"about:blank");
    IWebBrowser2_get_Document(wb, &disp);
    IDispatch_QueryInterface(disp, &IID_IHTMLDocument2, (void **)&doc);
    IDispatch_Release(disp);
    IHTMLDocument2_get_parentWindow(doc, &win);
    {
        ITravelLogClient *tlc = NULL;
        WINDOWDATA wd;

        EXPECT_HR(S_OK, IHTMLWindow2_QueryInterface(win, &IID_ITravelLogClient, (void **)&tlc));
        if (tlc) {
            memset(&wd, 0xcc, sizeof(wd));
            EXPECT_HR(INET_E_SECURITY_PROBLEM, ITravelLogClient_GetWindowData(tlc, NULL, &wd));   /* about: */
            EXPECT_HR(E_POINTER, ITravelLogClient_GetWindowData(tlc, NULL, NULL));
            ITravelLogClient_Release(tlc);
        }
        IHTMLWindow2_Release(win); win = NULL;
        IHTMLDocument2_Release(doc); doc = NULL;

        navigate(L"file:///C:/sgm1.html#anchor");
        IWebBrowser2_get_Document(wb, &disp);
        IDispatch_QueryInterface(disp, &IID_IHTMLDocument2, (void **)&doc);
        IDispatch_Release(disp);
        IHTMLDocument2_get_parentWindow(doc, &win);
        IHTMLWindow2_QueryInterface(win, &IID_ITravelLogClient, (void **)&tlc);
        memset(&wd, 0, sizeof(wd));
        EXPECT_HR(S_OK, ITravelLogClient_GetWindowData(tlc, NULL, &wd));
        EXPECT(wd.lpszUrl && !wcsicmp(wd.lpszUrl, L"file:///C:/sgm1.html"));
        EXPECT(wd.lpszUrlLocation && !wcscmp(wd.lpszUrlLocation, L"#anchor"));
        EXPECT(wd.lpszTitle && !wcscmp(wd.lpszTitle, L"the title"));
        CoTaskMemFree(wd.lpszUrl); CoTaskMemFree(wd.lpszUrlLocation); CoTaskMemFree(wd.lpszTitle);
        ITravelLogClient_Release(tlc);
    }

    /* ---- IInternetHostSecurityManager::GetSecurityId (family: secid) ---- */
    {
        IInternetHostSecurityManager *hsm = NULL;
        BYTE id[512]; DWORD cb = sizeof(id);
        { IServiceProvider *sp = NULL; IHTMLDocument2_QueryInterface(doc, &IID_IServiceProvider, (void **)&sp);
          EXPECT(sp != NULL);
          if (sp) { EXPECT_HR(S_OK, IServiceProvider_QueryService(sp, &SID_SInternetHostSecurityManager, &IID_IInternetHostSecurityManager, (void **)&hsm)); IServiceProvider_Release(sp); } }
        if (hsm) {
            EXPECT_HR(S_OK, IInternetHostSecurityManager_GetSecurityId(hsm, id, &cb, 0));
            EXPECT(cb > 6 && !memcmp(id, "file:", 5));
            EXPECT_HR(E_INVALIDARG, IInternetHostSecurityManager_GetSecurityId(hsm, NULL, &cb, 0));
            EXPECT_HR(E_INVALIDARG, IInternetHostSecurityManager_GetSecurityId(hsm, id, NULL, 0));
            IInternetHostSecurityManager_Release(hsm);
        }
    }

    /* ---- option index is accepted and not stored; range command targets (family: dom) ---- */
    {
        IHTMLDocument3 *doc3 = NULL; IHTMLElement *e = NULL; IHTMLOptionElement *opt = NULL; LONG idx = 5;
        IHTMLDocument2_QueryInterface(doc, &IID_IHTMLDocument3, (void **)&doc3);
        b = SysAllocString(L"o2");
        IHTMLDocument3_getElementById(doc3, b, &e);
        SysFreeString(b);
        EXPECT(e != NULL);
        IHTMLElement_QueryInterface(e, &IID_IHTMLOptionElement, (void **)&opt);
        EXPECT_HR(S_OK, IHTMLOptionElement_put_index(opt, 0));
        EXPECT_HR(S_OK, IHTMLOptionElement_get_index(opt, &idx));
        EXPECT(idx == 1);
        IHTMLOptionElement_Release(opt);
        IHTMLElement_Release(e);

        {   /* a string as the handler is ignored in the document modes of the page (default: latest) */
            VARIANT v;
            IHTMLElement *div = NULL;
            b = SysAllocString(L"d");
            IHTMLDocument3_getElementById(doc3, b, &div);
            SysFreeString(b);
            V_VT(&v) = VT_BSTR; V_BSTR(&v) = SysAllocString(L"function();");
            EXPECT_HR(S_OK, IHTMLElement_put_onclick(div, v));
            SysFreeString(V_BSTR(&v));
            VariantInit(&v);
            EXPECT_HR(S_OK, IHTMLElement_get_onclick(div, &v));
            EXPECT(V_VT(&v) == VT_NULL);
            VariantClear(&v);
            IHTMLElement_Release(div);
        }
        {
            IHTMLBodyElement *body = NULL; IHTMLElement *be = NULL; IHTMLTxtRange *r = NULL; IOleCommandTarget *ct = NULL;
            static const GUID unknown_group = {0x12345678,0x1234,0x1234,{1,2,3,4,5,6,7,8}};
            IHTMLDocument2_get_body(doc, &be);
            IHTMLElement_QueryInterface(be, &IID_IHTMLBodyElement, (void **)&body);
            IHTMLBodyElement_createTextRange(body, &r);
            EXPECT_HR(S_OK, IHTMLTxtRange_QueryInterface(r, &IID_IOleCommandTarget, (void **)&ct));
            EXPECT_HR(OLECMDERR_E_UNKNOWNGROUP, IOleCommandTarget_Exec(ct, &unknown_group, 1, 0, NULL, NULL));
            IOleCommandTarget_Release(ct); IHTMLTxtRange_Release(r); IHTMLBodyElement_Release(body); IHTMLElement_Release(be);
        }
        IHTMLDocument3_Release(doc3);
    }

    IHTMLWindow2_Release(win);
    IHTMLDocument2_Release(doc);
    IConnectionPoint_Unadvise(cp, cookie);
    IOleObject_SetClientSite(oleobj, NULL);
    IOleObject_Release(oleobj);
    IWebBrowser2_Release(wb);
    DeleteFileW(L"C:\\sgm1.html");
    printf("checks %d, mismatches %d\n", total, bad);
    printf("RESULT: %s\n", bad ? "FAIL" : "PASS");
    return bad != 0;
}
