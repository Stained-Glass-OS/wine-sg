#define COBJMACROS
#include <windows.h>
#include <ole2.h>
#include <ocidl.h>
#include <exdisp.h>
#include <mshtml.h>
#include <mshtmhst.h>
#include <stdio.h>
#include <string.h>
static int bad, total;
#define EXPECT(c) do { total++; if (!(c)) { bad++; printf("MISMATCH line %d: %s\n", __LINE__, #c); } } while (0)
static const GUID IID_IDocHostShowUI_ = {0xc4d244b0,0xd43e,0x11cf,{0x89,0x3b,0x00,0xaa,0x00,0xbd,0xce,0x1a}};
#define IID_IDocHostShowUI IID_IDocHostShowUI_
#define STUBS(name) \
    static ULONG WINAPI name##_AddRef(void *i) { return 2; } \
    static ULONG WINAPI name##_Release(void *i) { return 1; }

static HWND hwnd;
static int show_calls;
static WCHAR last_text[200], last_caption[100];
static DWORD last_type;
static LRESULT next_result = IDOK;
static HRESULT next_hr = S_OK;
static int help_calls;

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
    return E_NOTIMPL; /* no frame: ieframe supplies its own */
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

static HRESULT WINAPI su_ShowMessage(IDocHostShowUI *i, HWND w, LPOLESTR text, LPOLESTR caption, DWORD type, LPOLESTR hf, DWORD hc, LRESULT *r)
{
    show_calls++;
    lstrcpynW(last_text, text ? text : L"", 200);
    lstrcpynW(last_caption, caption ? caption : L"", 100);
    last_type = type;
    *r = next_result;
    return next_hr;
}
static HRESULT WINAPI su_ShowHelp(IDocHostShowUI *i, HWND w, LPOLESTR f, UINT c, DWORD d, POINT p, IDispatch *o) { help_calls++; return S_OK; }
static IDocHostShowUIVtbl suvtbl = { (void *)site_QI, (void *)ukn_AddRef, (void *)ukn_Release, su_ShowMessage, su_ShowHelp };
static IDocHostShowUI showui = { &suvtbl };

static HRESULT WINAPI site_QI(IOleClientSite *i, REFIID riid, void **ppv)
{
    if (IsEqualGUID(riid, &IID_IUnknown) || IsEqualGUID(riid, &IID_IOleClientSite)) *ppv = &clientsite;
    else if (IsEqualGUID(riid, &IID_IOleInPlaceSite) || IsEqualGUID(riid, &IID_IOleWindow)) *ppv = &inplace;
    else if (IsEqualGUID(riid, &IID_IDocHostShowUI)) *ppv = &showui;
    else { *ppv = NULL; return E_NOINTERFACE; }
    return S_OK;
}

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
    Sleep(90000);
    printf("watchdog: stuck, probably a message box\nRESULT: FAIL\n");
    fflush(stdout);
    ExitProcess(2);
}

int main(void)
{
    IWebBrowser2 *wb;
    IOleObject *oleobj;
    IDispatch *disp = NULL;
    IHTMLDocument2 *doc = NULL;
    IHTMLWindow2 *win = NULL;
    VARIANT url, v;
    VARIANT_BOOL confirmed;
    READYSTATE rs = 0;
    HRESULT hr;
    BSTR b;
    int i;

    setvbuf(stdout, NULL, _IONBF, 0);
    CreateThread(NULL, 0, watchdog, NULL, 0, NULL);
    CoInitialize(NULL);
    hwnd = CreateWindowExA(0, "static", "host", WS_POPUP, 0, 0, 300, 300, NULL, NULL, NULL, NULL);

    hr = CoCreateInstance(&CLSID_WebBrowser, NULL, CLSCTX_INPROC_SERVER, &IID_IWebBrowser2, (void **)&wb);
    EXPECT(hr == S_OK);
    if (hr != S_OK) { printf("RESULT: FAIL\n"); return 1; }
    IWebBrowser2_QueryInterface(wb, &IID_IOleObject, (void **)&oleobj);
    hr = IOleObject_SetClientSite(oleobj, &clientsite);
    EXPECT(hr == S_OK);
    { RECT rc = {0, 0, 300, 300}; IOleObject_DoVerb(oleobj, OLEIVERB_INPLACEACTIVATE, NULL, &clientsite, 0, hwnd, &rc); }

    VariantInit(&url);
    V_VT(&url) = VT_BSTR;
    V_BSTR(&url) = SysAllocString(L"about:blank");
    hr = IWebBrowser2_Navigate2(wb, &url, NULL, NULL, NULL, NULL);
    EXPECT(hr == S_OK);
    VariantClear(&url);
    for (i = 0; i < 200; i++) {
        pump(50);
        IWebBrowser2_get_ReadyState(wb, &rs);
        if (rs == READYSTATE_COMPLETE) break;
    }
    EXPECT(rs == READYSTATE_COMPLETE);
    hr = IWebBrowser2_get_Document(wb, &disp);
    EXPECT(hr == S_OK && disp);
    if (disp) {
        IDispatch_QueryInterface(disp, &IID_IHTMLDocument2, (void **)&doc);
        IDispatch_Release(disp);
    }
    if (doc) IHTMLDocument2_get_parentWindow(doc, &win);
    EXPECT(win != NULL);
    if (!win) { printf("RESULT: FAIL\n"); return 1; }

    b = SysAllocString(L"hello alert");
    hr = IHTMLWindow2_alert(win, b);
    SysFreeString(b);
    EXPECT(hr == S_OK);
    EXPECT(show_calls == 1);
    EXPECT(!wcscmp(last_text, L"hello alert"));
    EXPECT(last_caption[0] != 0);
    EXPECT((last_type & MB_TYPEMASK) == MB_OK);

    b = SysAllocString(L"really?");
    next_result = IDCANCEL;
    confirmed = VARIANT_TRUE;
    hr = IHTMLWindow2_confirm(win, b, &confirmed);
    EXPECT(hr == S_OK && confirmed == VARIANT_FALSE);
    EXPECT(show_calls == 2);
    EXPECT(!wcscmp(last_text, L"really?"));
    EXPECT((last_type & MB_TYPEMASK) == MB_OKCANCEL);
    next_result = IDOK;
    confirmed = VARIANT_FALSE;
    hr = IHTMLWindow2_confirm(win, b, &confirmed);
    EXPECT(hr == S_OK && confirmed == VARIANT_TRUE);
    EXPECT(show_calls == 3);
    SysFreeString(b);

    /* from script */
    b = SysAllocString(L"alert('from script')");
    VariantInit(&v);
    {
        BSTR lang = SysAllocString(L"JScript");
        hr = IHTMLWindow2_execScript(win, b, lang, &v);
        SysFreeString(lang);
    }
    SysFreeString(b);
    EXPECT(hr == S_OK);
    EXPECT(show_calls == 4 && !wcscmp(last_text, L"from script"));

    IHTMLWindow2_Release(win);
    IHTMLDocument2_Release(doc);
    IOleObject_SetClientSite(oleobj, NULL);
    IOleObject_Release(oleobj);
    IWebBrowser2_Release(wb);
    printf("checks %d, mismatches %d, show calls %d\n", total, bad, show_calls);
    printf("RESULT: %s\n", bad ? "FAIL" : "PASS");
    return bad != 0;
}
