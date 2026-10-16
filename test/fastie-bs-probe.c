#define COBJMACROS
#include <windows.h>
#include <ole2.h>
#include <ocidl.h>
#include <exdisp.h>
#include <exdispid.h>
#include <mshtml.h>
#include <mshtmhst.h>
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
    IOleObject *oleobj, *docole;
    IDispatch *disp = NULL;
    IHTMLDocument2 *doc = NULL;
    IHTMLWindow2 *win = NULL;
    IOleClientSite *docsite = NULL;
    IServiceProvider *sp = NULL;
    IBrowserService *bs = NULL;
    IDocObjectService *dos = NULL;
    IShellBrowser *shb = NULL;
    IConnectionPointContainer *cpc;
    IConnectionPoint *cp;
    DWORD cookie;
    HRESULT hr;
    BSTR b;
    WCHAR buf[INTERNET_MAX_URL_LENGTH];
    PIDLIST_ABSOLUTE pidl = NULL, pidl2 = NULL;
    BOOL bval;
    DWORD flags;
    int i;

    setvbuf(stdout, NULL, _IONBF, 0);
    CreateThread(NULL, 0, watchdog, NULL, 0, NULL);
    CoInitialize(NULL);
    hwnd = CreateWindowExA(0, "static", "host", WS_POPUP, 0, 0, 400, 300, NULL, NULL, NULL, NULL);

    write_file(L"C:\\sgp1.html", "<html><head><title>one</title></head><body>page one</body></html>");
    write_file(L"C:\\sgp2.html", "<html><head><title>two</title></head><body>page two with a needle in it</body></html>");
    write_file(L"C:\\sgp3.html", "<html><head><title>three</title></head><body>page three</body></html>");

    hr = CoCreateInstance(&CLSID_WebBrowser, NULL, CLSCTX_INPROC_SERVER, &IID_IWebBrowser2, (void **)&wb);
    EXPECT(hr == S_OK);
    if (hr != S_OK) { printf("RESULT: FAIL\n"); return 1; }
    IWebBrowser2_QueryInterface(wb, &IID_IOleObject, (void **)&oleobj);
    IOleObject_SetClientSite(oleobj, &clientsite);
    { RECT rc = {0, 0, 400, 300}; IOleObject_DoVerb(oleobj, OLEIVERB_INPLACEACTIVATE, NULL, &clientsite, 0, hwnd, &rc); }
    IWebBrowser2_QueryInterface(wb, &IID_IConnectionPointContainer, (void **)&cpc);
    IConnectionPointContainer_FindConnectionPoint(cpc, &DIID_DWebBrowserEvents2, &cp);
    IConnectionPoint_Advise(cp, (IUnknown *)&sink, &cookie);

    navigate(L"about:blank");

    IWebBrowser2_get_Document(wb, &disp);
    EXPECT(disp != NULL);
    if (!disp) { printf("RESULT: FAIL\n"); return 1; }
    IDispatch_QueryInterface(disp, &IID_IHTMLDocument2, (void **)&doc);
    IDispatch_Release(disp);
    IHTMLDocument2_QueryInterface(doc, &IID_IOleObject, (void **)&docole);
    EXPECT_HR(S_OK, IOleObject_GetClientSite(docole, &docsite));
    IOleClientSite_QueryInterface(docsite, &IID_IServiceProvider, (void **)&sp);
    EXPECT(sp != NULL);
    EXPECT_HR(S_OK, IServiceProvider_QueryService(sp, &IID_IShellBrowser, &IID_IBrowserService, (void **)&bs));
    EXPECT_HR(S_OK, IServiceProvider_QueryService(sp, &IID_IShellBrowser, &IID_IDocObjectService, (void **)&dos));
    EXPECT_HR(S_OK, IServiceProvider_QueryService(sp, &IID_IShellBrowser, &IID_IShellBrowser, (void **)&shb));
    if (!bs || !dos) { printf("RESULT: FAIL\n"); return 1; }

    /* ---- flags (family: flags) ---- */
    EXPECT_HR(E_POINTER, IBrowserService_GetFlags(bs, NULL));
    EXPECT_HR(S_OK, IBrowserService_GetFlags(bs, &flags));
    EXPECT((flags & BSF_TOPBROWSER) && !(flags & BSF_THEATERMODE));
    EXPECT_HR(S_OK, IBrowserService_SetFlags(bs, BSF_NOLOCALFILEWARNING | BSF_RESIZABLE, BSF_NOLOCALFILEWARNING | BSF_RESIZABLE));
    EXPECT_HR(S_OK, IBrowserService_GetFlags(bs, &flags));
    EXPECT((flags & (BSF_NOLOCALFILEWARNING | BSF_RESIZABLE)) == (BSF_NOLOCALFILEWARNING | BSF_RESIZABLE));
    EXPECT_HR(S_OK, IBrowserService_SetFlags(bs, 0, BSF_RESIZABLE));              /* only the masked bit changes */
    EXPECT_HR(S_OK, IBrowserService_GetFlags(bs, &flags));
    EXPECT((flags & BSF_NOLOCALFILEWARNING) && !(flags & BSF_RESIZABLE) && (flags & BSF_TOPBROWSER));
    EXPECT_HR(S_OK, IBrowserService_SetFlags(bs, BSF_THEATERMODE, BSF_THEATERMODE));
    { VARIANT_BOOL tm = 0; IWebBrowser2_get_TheaterMode(wb, &tm); EXPECT(tm == VARIANT_TRUE); }
    EXPECT_HR(S_OK, IBrowserService_GetFlags(bs, &flags));
    EXPECT(flags & BSF_THEATERMODE);
    IWebBrowser2_put_TheaterMode(wb, VARIANT_FALSE);
    EXPECT_HR(S_OK, IBrowserService_GetFlags(bs, &flags));
    EXPECT(!(flags & BSF_THEATERMODE));
    EXPECT_HR(S_OK, IBrowserService_SetFlags(bs, BSF_REGISTERASDROPTARGET, BSF_REGISTERASDROPTARGET));
    { VARIANT_BOOL dt = 0; IWebBrowser2_get_RegisterAsDropTarget(wb, &dt); EXPECT(dt == VARIANT_TRUE); }

    /* ---- navigate state, can navigate, code page (family: state) ---- */
    { BNSTATE st = 99;
      EXPECT_HR(E_POINTER, IBrowserService_GetNavigateState(bs, NULL));
      EXPECT_HR(S_OK, IBrowserService_GetNavigateState(bs, &st)); EXPECT(st == BNS_NORMAL);
      EXPECT_HR(S_OK, IBrowserService_CanNavigateNow(bs));
      EXPECT_HR(S_OK, IBrowserService_SetNavigateState(bs, BNS_BEGIN_NAVIGATE));
      EXPECT_HR(S_OK, IBrowserService_GetNavigateState(bs, &st)); EXPECT(st == BNS_BEGIN_NAVIGATE);
      EXPECT_HR(S_FALSE, IBrowserService_CanNavigateNow(bs));
      EXPECT_HR(S_OK, IBrowserService_SetNavigateState(bs, BNS_NAVIGATE));
      EXPECT_HR(S_OK, IBrowserService_CanNavigateNow(bs));
      EXPECT_HR(E_INVALIDARG, IBrowserService_SetNavigateState(bs, 7));
      EXPECT_HR(S_OK, IBrowserService_SetNavigateState(bs, BNS_NORMAL)); }
    { VARIANT in, out;
      VariantInit(&in); VariantInit(&out);
      EXPECT_HR(S_OK, IBrowserService_GetSetCodePage(bs, NULL, &out));
      EXPECT(V_VT(&out) == VT_I4 && V_I4(&out) == (LONG)GetACP());
      V_VT(&in) = VT_I4; V_I4(&in) = 932;
      EXPECT_HR(S_OK, IBrowserService_GetSetCodePage(bs, &in, &out));
      EXPECT(V_I4(&out) == (LONG)GetACP());
      EXPECT_HR(S_OK, IBrowserService_GetSetCodePage(bs, NULL, &out));
      EXPECT(V_VT(&out) == VT_I4 && V_I4(&out) == 932);
      EXPECT_HR(S_OK, IBrowserService_GetSetCodePage(bs, &in, NULL));
      V_VT(&in) = VT_BSTR; V_BSTR(&in) = NULL;
      EXPECT_HR(E_INVALIDARG, IBrowserService_GetSetCodePage(bs, &in, &out)); }

    /* ---- title (family: title) ---- */
    ev_title = 0;
    EXPECT_HR(S_OK, IBrowserService_SetTitle(bs, NULL, L"My Title"));
    EXPECT(ev_title == 1 && !wcscmp(ev_titletext, L"My Title"));
    EXPECT_HR(S_OK, IBrowserService_GetTitle(bs, NULL, buf, ARRAY_SIZE(buf)));
    EXPECT(!wcscmp(buf, L"My Title"));
    EXPECT_HR(S_OK, IBrowserService_GetTitle(bs, NULL, buf, 5));
    EXPECT(!wcscmp(buf, L"My T"));
    EXPECT_HR(E_INVALIDARG, IBrowserService_GetTitle(bs, NULL, buf, 0));
    EXPECT_HR(E_INVALIDARG, IBrowserService_SetTitle(bs, NULL, NULL));

    /* ---- control windows (family: controls) ---- */
    { BOOL shown = 5; VARIANT_BOOL vb = 0;
      EXPECT_HR(S_OK, IBrowserService_ShowControlWindow(bs, FCW_STATUS, TRUE));
      IWebBrowser2_get_StatusBar(wb, &vb); EXPECT(vb == VARIANT_TRUE);
      EXPECT_HR(S_OK, IBrowserService_IsControlWindowShown(bs, FCW_STATUS, &shown)); EXPECT(shown == TRUE);
      EXPECT_HR(S_OK, IBrowserService_ShowControlWindow(bs, FCW_STATUS, FALSE));
      IWebBrowser2_get_StatusBar(wb, &vb); EXPECT(vb == VARIANT_FALSE);
      EXPECT_HR(S_OK, IBrowserService_IsControlWindowShown(bs, FCW_STATUS, &shown)); EXPECT(shown == FALSE);
      EXPECT_HR(S_OK, IBrowserService_ShowControlWindow(bs, FCW_TOOLBAR, TRUE));
      EXPECT_HR(S_OK, IBrowserService_IsControlWindowShown(bs, FCW_TOOLBAR, &shown)); EXPECT(shown == TRUE);
      EXPECT_HR(S_OK, IBrowserService_ShowControlWindow(bs, FCW_TOOLBAR, FALSE));
      EXPECT_HR(S_OK, IBrowserService_IsControlWindowShown(bs, FCW_TOOLBAR, &shown)); EXPECT(shown == FALSE);
      EXPECT_HR(S_OK, IBrowserService_IsControlWindowShown(bs, FCW_TREE, &shown)); EXPECT(shown == FALSE);
      EXPECT_HR(E_NOTIMPL, IBrowserService_ShowControlWindow(bs, FCW_TREE, TRUE));
      EXPECT_HR(S_OK, IBrowserService_ShowControlWindow(bs, FCW_TREE, FALSE));
      EXPECT_HR(E_INVALIDARG, IBrowserService_ShowControlWindow(bs, 77, TRUE));
      EXPECT_HR(E_INVALIDARG, IBrowserService_IsControlWindowShown(bs, 77, &shown));
      EXPECT_HR(E_POINTER, IBrowserService_IsControlWindowShown(bs, FCW_STATUS, NULL)); }

    /* ---- pidls (family: pidl) ---- */
    EXPECT_HR(S_OK, IBrowserService_IEParseDisplayName(bs, CP_ACP, L"  http://example.com/a?b=1#frag ", &pidl));
    EXPECT(pidl != NULL);
    EXPECT_HR(S_OK, IBrowserService_IEGetDisplayName(bs, pidl, buf, 0));
    EXPECT(!wcscmp(buf, L"http://example.com/a?b=1#frag"));
    EXPECT_HR(S_OK, IBrowserService_IEGetDisplayName(bs, pidl, buf, SHGDN_FORPARSING));
    EXPECT(!wcscmp(buf, L"http://example.com/a?b=1#frag"));
    ILFree(pidl); pidl = NULL;
    EXPECT_HR(S_OK, IBrowserService_IEParseDisplayName(bs, CP_ACP, L"C:\\windows", &pidl));
    EXPECT(pidl != NULL);
    EXPECT_HR(S_OK, IBrowserService_IEGetDisplayName(bs, pidl, buf, SHGDN_FORPARSING));
    EXPECT(!wcsicmp(buf, L"C:\\windows"));
    EXPECT_HR(S_OK, IBrowserService_IEGetDisplayName(bs, pidl, buf, 0));
    EXPECT(!wcsnicmp(buf, L"file:///C:/windows", 18));
    ILFree(pidl); pidl = NULL;
    EXPECT_HR(S_OK, IBrowserService_IEParseDisplayName(bs, CP_ACP, L"file:///C:/windows", &pidl));
    EXPECT(pidl != NULL);
    EXPECT_HR(S_OK, IBrowserService_IEGetDisplayName(bs, pidl, buf, SHGDN_FORPARSING));
    EXPECT(!wcsicmp(buf, L"C:\\windows"));
    ILFree(pidl); pidl = NULL;
    EXPECT_HR(E_INVALIDARG, IBrowserService_IEParseDisplayName(bs, CP_ACP, NULL, &pidl));
    EXPECT_HR(E_INVALIDARG, IBrowserService_IEParseDisplayName(bs, CP_ACP, L"   ", &pidl));
    EXPECT_HR(E_INVALIDARG, IBrowserService_IEGetDisplayName(bs, NULL, buf, 0));
    EXPECT_HR(S_OK, IBrowserService_GetPidl(bs, &pidl));
    EXPECT(pidl != NULL);
    EXPECT_HR(S_OK, IBrowserService_IEGetDisplayName(bs, pidl, buf, 0));
    EXPECT(!wcscmp(buf, L"about:blank"));
    ILFree(pidl); pidl = NULL;
    EXPECT_HR(S_OK, IBrowserService_SetReferrer(bs, NULL));

    /* ---- display of parse errors (family: parseerror) ---- */
    show_calls = 0;
    EXPECT_HR(S_OK, IBrowserService_DisplayParseError(bs, S_OK, L"nothing.invalid"));
    EXPECT_HR(S_OK, IBrowserService_DisplayParseError(bs, E_OUTOFMEMORY, L"nothing.invalid"));
    EXPECT_HR(S_OK, IBrowserService_DisplayParseError(bs, HRESULT_FROM_WIN32(ERROR_CANCELLED), L"nothing.invalid"));
    EXPECT(show_calls == 0);
    EXPECT_HR(S_OK, IBrowserService_DisplayParseError(bs, E_FAIL, L"nothing.invalid"));
    EXPECT(show_calls == 1 && wcsstr(last_text, L"nothing.invalid") && last_caption[0]);

    /* ---- misc (family: misc) ---- */
    { HPALETTE pal = (HPALETTE)5; IUnknown *unk = NULL, *unk2 = NULL; IOleObject *ole = NULL; IOleInPlaceSite *ips = NULL;
      EXPECT_HR(E_FAIL, IBrowserService_GetPalette(bs, &pal)); EXPECT(pal == NULL);
      EXPECT((int)IBrowserService_GetBrowserIndex(bs) == -1);
      EXPECT_HR(S_OK, IBrowserService_GetBrowserByIndex(bs, ~0u, &unk));
      EXPECT(unk != NULL);
      IUnknown_QueryInterface(unk, &IID_IShellBrowser, (void **)&unk2);
      EXPECT(unk2 != NULL);
      if (unk2) IUnknown_Release(unk2);
      if (unk) IUnknown_Release(unk);
      EXPECT_HR(E_FAIL, IBrowserService_GetBrowserByIndex(bs, 3, &unk));
      EXPECT_HR(S_OK, IBrowserService_GetOleObject(bs, &ole));
      EXPECT(ole != NULL);
      if (ole) IOleObject_Release(ole);
      EXPECT_HR(S_OK, IBrowserService_GetParentSite(bs, &ips));
      EXPECT(ips == &inplace);
      EXPECT_HR(S_OK, IBrowserService_UpdateBackForwardState(bs)); }

    /* ---- history object (family: history) ---- */
    { IOleObject *ole = NULL, *got = NULL; IStream *stm = NULL; IBindCtx *bc = NULL;
      EXPECT_HR(E_FAIL, IBrowserService_GetHistoryObject(bs, &got, NULL, NULL));
      IWebBrowser2_QueryInterface(wb, &IID_IOleObject, (void **)&ole);
      EXPECT_HR(S_OK, IBrowserService_SetHistoryObject(bs, ole, FALSE));
      EXPECT_HR(E_FAIL, IBrowserService_SetHistoryObject(bs, ole, TRUE));
      EXPECT_HR(S_OK, IBrowserService_GetHistoryObject(bs, &got, &stm, &bc));
      EXPECT(got == ole && stm != NULL && bc != NULL);
      if (got) IOleObject_Release(got);
      if (stm) IStream_Release(stm);
      if (bc) IBindCtx_Release(bc);
      EXPECT_HR(S_OK, IBrowserService_GetHistoryObject(bs, &got, NULL, NULL));
      if (got) IOleObject_Release(got);
      IOleObject_Release(ole); }

    /* ---- http-equiv reaches the container (family: httpequiv) ---- */
    cmd_calls = 0;
    EXPECT_HR(S_OK, IBrowserService_OnHttpEquiv(bs, NULL, FALSE, NULL, NULL));
    EXPECT(cmd_calls == 1 && last_cmd == OLECMDID_HTTPEQUIV);
    EXPECT_HR(S_OK, IBrowserService_OnHttpEquiv(bs, NULL, TRUE, NULL, NULL));
    EXPECT(cmd_calls == 2 && last_cmd == OLECMDID_HTTPEQUIV_DONE);

    /* ---- document object service: download events, pending url (family: docobj) ---- */
    ev_dlbegin = ev_dlcomplete = 0;
    EXPECT_HR(S_OK, IDocObjectService_FireDownloadBegin(dos));
    EXPECT(ev_dlbegin == 1);
    { VARIANT_BOOL busy = 0; IWebBrowser2_get_Busy(wb, &busy); EXPECT(busy == VARIANT_TRUE); }
    EXPECT_HR(S_OK, IDocObjectService_FireDownloadComplete(dos));
    EXPECT(ev_dlcomplete == 1);
    { VARIANT_BOOL busy = 5; IWebBrowser2_get_Busy(wb, &busy); EXPECT(busy == VARIANT_FALSE); }
    b = NULL;
    EXPECT_HR(S_OK, IDocObjectService_GetPendingUrl(dos, &b));
    EXPECT(b && !wcscmp(b, L"about:blank"));
    SysFreeString(b);
    EXPECT_HR(E_POINTER, IDocObjectService_GetPendingUrl(dos, NULL));
    EXPECT_HR(S_OK, IDocObjectService_ActiveElementChanged(dos, NULL));
    EXPECT_HR(E_NOTIMPL, IDocObjectService_UpdateDesktopComponent(dos, NULL));
    b = (BSTR)1;
    EXPECT_HR(S_OK, IDocObjectService_GetUrlSearchComponent(dos, &b));
    EXPECT(b && !*b);
    SysFreeString(b);

    /* ---- travel log (family: travel) ---- */
    {
        ITravelLog *tl = NULL, *tl2 = NULL; ITravelEntry *te = NULL;
        BSTR u;
        HMENU menu;
        WCHAR text[200];
        int n;

        navigate(L"file:///C:/sgp1.html");
        navigate(L"file:///C:/sgp2.html?x=1#top");
        navigate(L"file:///C:/sgp3.html");
        EXPECT_HR(S_OK, IBrowserService_GetTravelLog(bs, &tl));
        EXPECT(tl != NULL);
        if (tl) {
            EXPECT(ITravelLog_CountEntries(tl, (IUnknown *)bs) == 3);
            EXPECT_HR(E_FAIL, ITravelLog_GetTravelEntry(tl, (IUnknown *)bs, 1, &te));
            EXPECT_HR(E_FAIL, ITravelLog_GetTravelEntry(tl, (IUnknown *)bs, -3, &te));
            EXPECT_HR(S_OK, ITravelLog_GetTravelEntry(tl, (IUnknown *)bs, -1, &te));
            EXPECT(te != NULL);
            if (te) {
                EXPECT_HR(S_OK, ITravelEntry_GetPidl(te, &pidl));
                EXPECT_HR(S_OK, IBrowserService_IEGetDisplayName(bs, pidl, buf, 0));
                EXPECT(wcsstr(buf, L"sgp2.html"));
                EXPECT_HR(S_OK, ITravelLog_FindTravelEntry(tl, (IUnknown *)bs, pidl, &tl2 ? (ITravelEntry **)&tl2 : NULL));
                if (tl2) { ITravelEntry_Release((ITravelEntry *)tl2); tl2 = NULL; }
                ILFree(pidl); pidl = NULL;
                ITravelEntry_Release(te);
            }
            EXPECT_HR(S_OK, ITravelLog_GetTravelEntry(tl, (IUnknown *)bs, 0, NULL));  /* the entry is optional */
            EXPECT_HR(S_OK, ITravelLog_GetToolTipText(tl, (IUnknown *)bs, -2, 0, text, ARRAY_SIZE(text)));
            EXPECT(wcsstr(text, L"sgp1.html"));
            EXPECT_HR(E_INVALIDARG, ITravelLog_GetToolTipText(tl, (IUnknown *)bs, -2, 0, text, 0));

            menu = CreatePopupMenu();
            EXPECT_HR(S_OK, ITravelLog_InsertMenuEntries(tl, (IUnknown *)bs, menu, 0, 100, 110, TLMENUF_BACK));
            EXPECT(GetMenuItemCount(menu) == 2);
            EXPECT(GetMenuItemID(menu, 0) == 100 && GetMenuItemID(menu, 1) == 101);
            GetMenuStringW(menu, 0, text, ARRAY_SIZE(text), MF_BYPOSITION);
            EXPECT(wcsstr(text, L"sgp2.html"));      /* nearest first */
            DestroyMenu(menu);
            menu = CreatePopupMenu();
            EXPECT_HR(S_OK, ITravelLog_InsertMenuEntries(tl, (IUnknown *)bs, menu, 0, 200, 201, TLMENUF_BACKANDFORTH));
            EXPECT(GetMenuItemCount(menu) == 2);   /* only room for two ids */
            DestroyMenu(menu);
            EXPECT_HR(E_INVALIDARG, ITravelLog_InsertMenuEntries(tl, (IUnknown *)bs, NULL, 0, 100, 110, TLMENUF_BACK));

            /* travel back and forth */
            n = ev_doccomplete;
            EXPECT_HR(S_OK, ITravelLog_Travel(tl, (IUnknown *)bs, -1));
            for (i = 0; i < 300 && ev_doccomplete == n; i++) pump(50);
            u = current_url();
            EXPECT(u && wcsstr(u, L"sgp2.html")); SysFreeString(u);
            EXPECT(ITravelLog_CountEntries(tl, (IUnknown *)bs) == 3);
            EXPECT_HR(S_OK, ITravelLog_GetTravelEntry(tl, (IUnknown *)bs, 1, &te));
            if (te) {
                n = ev_doccomplete;
                EXPECT_HR(S_OK, ITravelEntry_Invoke(te, (IUnknown *)bs));
                for (i = 0; i < 300 && ev_doccomplete == n; i++) pump(50);
                u = current_url();
                EXPECT(u && wcsstr(u, L"sgp3.html")); SysFreeString(u);
                ITravelEntry_Release(te);
            }
            EXPECT_HR(E_FAIL, ITravelLog_Travel(tl, (IUnknown *)bs, 1));
            EXPECT_HR(E_FAIL, ITravelLog_Travel(tl, (IUnknown *)bs, -9));
            EXPECT_HR(S_OK, ITravelLog_Travel(tl, (IUnknown *)bs, 0));
            EXPECT_HR(S_OK, ITravelLog_Clone(tl, &tl2));
            EXPECT(tl2 && ITravelLog_CountEntries(tl2, (IUnknown *)bs) == 3);
            if (tl2) ITravelLog_Release(tl2);
            EXPECT_HR(S_OK, ITravelLog_AddEntry(tl, (IUnknown *)bs, FALSE));
            EXPECT_HR(S_OK, ITravelLog_UpdateEntry(tl, (IUnknown *)bs, FALSE));
            EXPECT_HR(S_OK, ITravelLog_Revert(tl));
            ITravelLog_Release(tl);
        }

        /* the search component of the page shown, its pending url while loading */
        navigate(L"file:///C:/sgp2.html?x=1#top");
        b = NULL;
        EXPECT_HR(S_OK, IDocObjectService_GetUrlSearchComponent(dos, &b));
        EXPECT(b && !wcscmp(b, L"?x=1"));
        SysFreeString(b);
        b = NULL;
        EXPECT_HR(S_OK, IDocObjectService_GetPendingUrl(dos, &b));
        EXPECT(b && wcsstr(b, L"sgp2.html"));
        SysFreeString(b);
    }

    /* ---- navigate by pidl, redirect (family: navpidl) ---- */
    {
        BOOL did = 7; BSTR u; int n;
        EXPECT_HR(S_OK, IBrowserService_IEParseDisplayName(bs, CP_ACP, L"file:///C:/sgp1.html", &pidl));
        n = ev_doccomplete;
        EXPECT_HR(S_OK, IBrowserService_NavigateToPidl(bs, pidl, 0));
        for (i = 0; i < 300 && ev_doccomplete == n; i++) pump(50);
        u = current_url(); EXPECT(u && wcsstr(u, L"sgp1.html")); SysFreeString(u);
        EXPECT_HR(S_OK, IBrowserService_NotifyRedirect(bs, NULL, pidl, &did));
        EXPECT(did == FALSE);                              /* it is there already */
        ILFree(pidl); pidl = NULL;
        IBrowserService_IEParseDisplayName(bs, CP_ACP, L"file:///C:/sgp3.html", &pidl);
        n = ev_doccomplete;
        EXPECT_HR(S_OK, IBrowserService_NotifyRedirect(bs, NULL, pidl, &did));
        EXPECT(did == TRUE);
        for (i = 0; i < 300 && ev_doccomplete == n; i++) pump(50);
        u = current_url(); EXPECT(u && wcsstr(u, L"sgp3.html")); SysFreeString(u);
        ILFree(pidl); pidl = NULL;
        /* back by flag: the pidl is not where it goes */
        IBrowserService_IEParseDisplayName(bs, CP_ACP, L"file:///C:/sgp3.html", &pidl);
        n = ev_doccomplete;
        EXPECT_HR(S_OK, IBrowserService_NavigateToPidl(bs, pidl, HLNF_NAVIGATINGBACK));
        for (i = 0; i < 300 && ev_doccomplete == n; i++) pump(50);
        u = current_url(); EXPECT(u && wcsstr(u, L"sgp1.html")); SysFreeString(u);
        ILFree(pidl); pidl = NULL;
    }

    /* ---- window list (family: winlist) ---- */
    {
        IShellWindows *sw = NULL; LONG found = 0; IDispatch *d = NULL; VARIANT loc, root; SAFEARRAY *arr;
        PIDLIST_ABSOLUTE p = NULL; UINT size;
        hr = CoCreateInstance(&CLSID_ShellWindows, NULL, CLSCTX_ALL, &IID_IShellWindows, (void **)&sw);
        EXPECT(hr == S_OK && sw);
        IBrowserService_IEParseDisplayName(bs, CP_ACP, L"file:///C:/sgp1.html", &p);
        size = ILGetSize(p);
        arr = SafeArrayCreateVector(VT_UI1, 0, size);
        memcpy(arr->pvData, p, size);
        V_VT(&loc) = VT_ARRAY | VT_UI1; V_ARRAY(&loc) = arr;
        VariantInit(&root);
        if (sw) {
            EXPECT_HR(S_FALSE, IShellWindows_FindWindowSW(sw, &loc, &root, SWC_BROWSER, &found, 0, &d));
            EXPECT_HR(S_OK, IBrowserService_UpdateWindowList(bs));          /* not registered: nothing to do */
            EXPECT_HR(S_FALSE, IShellWindows_FindWindowSW(sw, &loc, &root, SWC_BROWSER, &found, 0, &d));
            EXPECT_HR(S_OK, IBrowserService_RegisterWindow(bs, FALSE, SWC_BROWSER));
            EXPECT_HR(S_OK, IBrowserService_UpdateWindowList(bs));
            EXPECT_HR(S_OK, IShellWindows_FindWindowSW(sw, &loc, &root, SWC_BROWSER, &found, 0, &d));
            EXPECT(found && IsWindow(LongToHandle(found)));
            EXPECT_HR(S_FALSE, IShellWindows_FindWindowSW(sw, &loc, &root, SWC_3RDPARTY, &found, 0, &d));
            /* registered once: asking again changes nothing, forcing starts over */
            EXPECT_HR(S_OK, IBrowserService_RegisterWindow(bs, FALSE, SWC_BROWSER));
            EXPECT_HR(S_OK, IShellWindows_FindWindowSW(sw, &loc, &root, SWC_BROWSER, &found, 0, &d));
            EXPECT_HR(S_OK, IBrowserService_RegisterWindow(bs, TRUE, SWC_BROWSER));
            EXPECT_HR(S_OK, IShellWindows_FindWindowSW(sw, &loc, &root, SWC_BROWSER, &found, 0, &d));
            IShellWindows_Release(sw);
        }
        SafeArrayDestroy(arr);
        ILFree(p);
    }

    /* ---- the script helper (family: helper) ---- */
    IHTMLDocument2_get_parentWindow(doc, &win);
    EXPECT(win != NULL);
    if (win) {
        IDispatch *ext = NULL;
        EXPECT_HR(S_OK, IHTMLWindow2_get_external(win, &ext));
        EXPECT(ext != NULL);
        if (ext) {
            DISPID id;
            DISPPARAMS dp;
            VARIANT args[6], res;
            EXCEPINFO ei;
            UINT err;
            OLECHAR *names;

            #define CALL(name, nargs, hrexp) do { \
                names = (OLECHAR *)name; \
                hr = IDispatch_GetIDsOfNames(ext, &IID_NULL, &names, 1, 0, &id); \
                EXPECT_HR(S_OK, hr); \
                dp.rgvarg = args; dp.cArgs = nargs; dp.cNamedArgs = 0; dp.rgdispidNamedArgs = NULL; \
                VariantInit(&res); memset(&ei, 0, sizeof(ei)); \
                hr = IDispatch_Invoke(ext, id, &IID_NULL, 0, DISPATCH_METHOD, &dp, &res, &ei, &err); \
                total++; if (hr != (hrexp)) { bad++; printf("MISMATCH line %d: %ls = %lx, expected %lx\n", __LINE__, name, hr, (HRESULT)(hrexp)); } \
            } while (0)

            /* AddDesktopComponent(URL, Type, Left, Top, Width, Height): the arguments are reversed in args[] */
            for (i = 0; i < 6; i++) VariantInit(&args[i]);
            V_VT(&args[5]) = VT_BSTR; V_BSTR(&args[5]) = SysAllocString(L"http://example.com/");
            V_VT(&args[4]) = VT_BSTR; V_BSTR(&args[4]) = SysAllocString(L"website");
            CALL(L"AddDesktopComponent", 6, S_OK);
            V_BSTR(&args[4]) = SysAllocString(L"bogus");
            CALL(L"AddDesktopComponent", 6, DISP_E_EXCEPTION);
            total++; if (ei.scode != E_INVALIDARG && hr == DISP_E_EXCEPTION) { bad++; printf("MISMATCH line %d: exception scode %lx\n", __LINE__, (long)ei.scode); }
            V_BSTR(&args[4]) = SysAllocString(L"IMAGE");
            CALL(L"AddDesktopComponent", 6, S_OK);

            /* AutoScan(search, failure URL, frame): documented as not implemented */
            V_VT(&args[2]) = VT_BSTR; V_BSTR(&args[2]) = SysAllocString(L"word");
            V_VT(&args[1]) = VT_BSTR; V_BSTR(&args[1]) = SysAllocString(L"http://example.com/");
            V_VT(&args[0]) = VT_ERROR; V_ERROR(&args[0]) = DISP_E_PARAMNOTFOUND;
            CALL(L"AutoScan", 3, DISP_E_EXCEPTION);
            total++; if (ei.scode != E_NOTIMPL) { bad++; printf("MISMATCH line %d: AutoScan scode %lx\n", __LINE__, (long)ei.scode); }

            /* ShowBrowserUI with a name that is none of the four */
            V_VT(&args[1]) = VT_BSTR; V_BSTR(&args[1]) = SysAllocString(L"NoSuchDialog");
            V_VT(&args[0]) = VT_ERROR; V_ERROR(&args[0]) = DISP_E_PARAMNOTFOUND;
            CALL(L"ShowBrowserUI", 2, DISP_E_EXCEPTION);
            total++; if (ei.scode != E_INVALIDARG) { bad++; printf("MISMATCH line %d: ShowBrowserUI scode %lx\n", __LINE__, (long)ei.scode); }

            /* NavigateAndFind(URL, query, frame): the text is selected once the page is loaded */
            {
                int n = ev_doccomplete;
                V_VT(&args[2]) = VT_BSTR; V_BSTR(&args[2]) = SysAllocString(L"file:///C:/sgp2.html");
                V_VT(&args[1]) = VT_BSTR; V_BSTR(&args[1]) = SysAllocString(L"needle");
                V_VT(&args[0]) = VT_ERROR; V_ERROR(&args[0]) = DISP_E_PARAMNOTFOUND;
                CALL(L"NavigateAndFind", 3, S_OK);
                for (i = 0; i < 300 && ev_doccomplete == n; i++) pump(50);
                pump(300);
                {
                    IDispatch *d2 = NULL; IHTMLDocument2 *doc2 = NULL; IHTMLSelectionObject *sel = NULL;
                    IDispatch *rng = NULL; IHTMLTxtRange *tr = NULL; BSTR t = NULL;
                    IWebBrowser2_get_Document(wb, &d2);
                    if (d2) IDispatch_QueryInterface(d2, &IID_IHTMLDocument2, (void **)&doc2);
                    if (doc2) IHTMLDocument2_get_selection(doc2, &sel);
                    if (sel) IHTMLSelectionObject_createRange(sel, &rng);
                    if (rng) IDispatch_QueryInterface(rng, &IID_IHTMLTxtRange, (void **)&tr);
                    if (tr) IHTMLTxtRange_get_text(tr, &t);
                    EXPECT(t && !wcscmp(t, L"needle"));
                    SysFreeString(t);
                    if (tr) IHTMLTxtRange_Release(tr);
                    if (rng) IDispatch_Release(rng);
                    if (sel) IHTMLSelectionObject_Release(sel);
                    if (doc2) IHTMLDocument2_Release(doc2);
                    if (d2) IDispatch_Release(d2);
                }
            }
            IDispatch_Release(ext);
        }
        IHTMLWindow2_Release(win);
    }

    IBrowserService_Release(bs);
    IDocObjectService_Release(dos);
    if (shb) IShellBrowser_Release(shb);
    IServiceProvider_Release(sp);
    IOleClientSite_Release(docsite);
    IOleObject_Release(docole);
    IHTMLDocument2_Release(doc);
    IConnectionPoint_Unadvise(cp, cookie);
    IOleObject_SetClientSite(oleobj, NULL);
    IOleObject_Release(oleobj);
    IWebBrowser2_Release(wb);
    DeleteFileW(L"C:\\sgp1.html"); DeleteFileW(L"C:\\sgp2.html"); DeleteFileW(L"C:\\sgp3.html");
    printf("checks %d, mismatches %d\n", total, bad);
    printf("RESULT: %s\n", bad ? "FAIL" : "PASS");
    return bad != 0;
}
