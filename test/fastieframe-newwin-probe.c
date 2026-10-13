#define COBJMACROS
#include <windows.h>
#include <ole2.h>
#include <exdisp.h>
#include <exdispid.h>
#include <mshtml.h>
#include <servprov.h>
#include <stdio.h>
#define P(...) printf(__VA_ARGS__)
static const CLSID CLSID_IE_ = {0x0002df01,0x0000,0x0000,{0xc0,0x00,0x00,0x00,0x00,0x00,0x00,0x46}};

/* sink modes */
enum { M_CANCEL3, M_CANCEL2_ONLY, M_HANDOVER3, M_PROCEED };
static int mode;
static IWebBrowser2 *handover;
static int n3, n2;
static WCHAR got_url[512], got_ctx[512];
static LONG got_flags;

static void pump(DWORD ms)
{
    DWORD end = GetTickCount() + ms;
    MSG msg;
    while (GetTickCount() < end) {
        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
        Sleep(10);
    }
}

static HRESULT WINAPI sink_QI(IDispatch *This, REFIID riid, void **ppv)
{
    if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &IID_IDispatch) || IsEqualIID(riid, &DIID_DWebBrowserEvents2)) { *ppv = This; return S_OK; }
    *ppv = NULL; return E_NOINTERFACE;
}
static ULONG WINAPI sink_AddRef(IDispatch *This) { return 2; }
static ULONG WINAPI sink_Release(IDispatch *This) { return 1; }
static HRESULT WINAPI sink_GetTypeInfoCount(IDispatch *This, UINT *n) { *n = 0; return S_OK; }
static HRESULT WINAPI sink_GetTypeInfo(IDispatch *This, UINT i, LCID l, ITypeInfo **t) { return E_NOTIMPL; }
static HRESULT WINAPI sink_GetIDsOfNames(IDispatch *This, REFIID r, LPOLESTR *n, UINT c, LCID l, DISPID *d) { return E_NOTIMPL; }
static HRESULT WINAPI sink_Invoke(IDispatch *This, DISPID id, REFIID riid, LCID lcid, WORD flags, DISPPARAMS *dp,
                                  VARIANT *res, EXCEPINFO *ei, UINT *err)
{
    if (id == DISPID_NEWWINDOW3) {
        VARIANT_BOOL *cancel = V_BOOLREF(&dp->rgvarg[3]);
        IDispatch **ppdisp = V_DISPATCHREF(&dp->rgvarg[4]);
        n3++;
        lstrcpynW(got_url, V_BSTR(&dp->rgvarg[0]), 512);
        lstrcpynW(got_ctx, V_BSTR(&dp->rgvarg[1]), 512);
        got_flags = V_I4(&dp->rgvarg[2]);
        if (mode == M_CANCEL2_ONLY) return DISP_E_MEMBERNOTFOUND;
        if (mode == M_CANCEL3) *cancel = VARIANT_TRUE;
        if (mode == M_HANDOVER3) { IWebBrowser2_QueryInterface(handover, &IID_IDispatch, (void**)ppdisp); }
        return S_OK;
    }
    if (id == DISPID_NEWWINDOW2) {
        VARIANT_BOOL *cancel = V_BOOLREF(&dp->rgvarg[0]);
        n2++;
        if (mode == M_CANCEL2_ONLY) *cancel = VARIANT_TRUE;
        return S_OK;
    }
    return DISP_E_MEMBERNOTFOUND;
}
static IDispatchVtbl sink_vtbl = { sink_QI, sink_AddRef, sink_Release, sink_GetTypeInfoCount, sink_GetTypeInfo, sink_GetIDsOfNames, sink_Invoke };
static IDispatch sink = { &sink_vtbl };

static void wait_ready(IWebBrowser2 *wb)
{
    READYSTATE rs = 0; int i;
    for (i = 0; i < 400; i++) {
        pump(50);
        IWebBrowser2_get_ReadyState(wb, &rs);
        if (rs == READYSTATE_COMPLETE) { IDispatch *d = NULL; if (SUCCEEDED(IWebBrowser2_get_Document(wb, &d)) && d) { IDispatch_Release(d); break; } }
    }
}

static void navigate(IWebBrowser2 *wb, const WCHAR *url)
{
    BSTR b = SysAllocString(url); VARIANT v; VariantInit(&v);
    IWebBrowser2_Navigate(wb, b, &v, &v, &v, &v); SysFreeString(b);
    wait_ready(wb);
}

static IWebBrowser2 *new_browser(void)
{
    IWebBrowser2 *wb = NULL;
    CoCreateInstance(&CLSID_IE_, NULL, CLSCTX_LOCAL_SERVER, &IID_IWebBrowser2, (void**)&wb);
    if (wb) IWebBrowser2_put_Visible(wb, VARIANT_FALSE);
    return wb;
}

/* run window.open in the page of wb; returns "null" or "win" and the new window's name */
static void run_open(IWebBrowser2 *wb, const WCHAR *url_b, const char *tag)
{
    IServiceProvider *sp = NULL; IHTMLWindow2 *w = NULL; IHTMLDocument2 *doc = NULL; BSTR b, lang = SysAllocString(L"JScript"); VARIANT r;
    WCHAR script[1024]; HRESULT hr;
    IWebBrowser2_QueryInterface(wb, &IID_IServiceProvider, (void**)&sp);
    IServiceProvider_QueryService(sp, &IID_IHTMLWindow2, &IID_IHTMLWindow2, (void**)&w);
    wsprintfW(script, L"var r = window.open('%s', 'pop'); document.title = r === null ? 'null' : (r ? 'win' : 'undef');", url_b);
    b = SysAllocString(script); VariantInit(&r);
    hr = IHTMLWindow2_execScript(w, b, lang, &r);
    SysFreeString(b); SysFreeString(lang);
    pump(1500);
    IHTMLWindow2_get_document(w, &doc);
    b = NULL; if (doc) IHTMLDocument2_get_title(doc, &b);
    P("%s %08lx %ls n3=%d n2=%d\n", tag, hr, b ? b : L"(null)", n3, n2);
    SysFreeString(b);
    if (doc) IHTMLDocument2_Release(doc);
    IHTMLWindow2_Release(w); IServiceProvider_Release(sp);
}

static void advise(IWebBrowser2 *wb, DWORD *cookie)
{
    IConnectionPointContainer *cpc = NULL; IConnectionPoint *cp = NULL;
    IWebBrowser2_QueryInterface(wb, &IID_IConnectionPointContainer, (void**)&cpc);
    IConnectionPointContainer_FindConnectionPoint(cpc, &DIID_DWebBrowserEvents2, &cp);
    IConnectionPoint_Advise(cp, (IUnknown*)&sink, cookie);
    IConnectionPoint_Release(cp); IConnectionPointContainer_Release(cpc);
}

int main(void)
{
    IWebBrowser2 *wb, *wb2, *wb3; DWORD cookie; WCHAR path[MAX_PATH], url[MAX_PATH + 16], pathb[MAX_PATH], urlb[MAX_PATH + 16]; HANDLE f; int i;
    setvbuf(stdout, NULL, _IONBF, 0);
    CoInitialize(NULL);
    GetTempPathW(MAX_PATH, path); lstrcpyW(pathb, path);
    lstrcatW(path, L"sgnw_a.html"); lstrcatW(pathb, L"sgnw_b.html");
    f = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    { static const char html[] = "<html><head><title>start</title></head><body>A</body></html>"; DWORD n; WriteFile(f, html, sizeof(html) - 1, &n, NULL); CloseHandle(f); }
    f = CreateFileW(pathb, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    { static const char html[] = "<html><head><title>popped</title></head><body>B</body></html>"; DWORD n; WriteFile(f, html, sizeof(html) - 1, &n, NULL); CloseHandle(f); }
    lstrcpyW(url, L"file:///"); lstrcatW(url, path); lstrcpyW(urlb, L"file:///"); lstrcatW(urlb, pathb);
    for (i = 0; url[i]; i++) if (url[i] == '\\') url[i] = '/';
    for (i = 0; urlb[i]; i++) if (urlb[i] == '\\') urlb[i] = '/';

    /* the browsers are all made first: a new one right after a Quit can reach the dying server */
    wb = new_browser(); wb2 = new_browser(); wb3 = new_browser(); handover = new_browser();
    P("CREATE %d\n", wb && wb2 && wb3 && handover);
    /* 1: the host cancels in NewWindow3 */
    advise(wb, &cookie); navigate(wb, url);
    mode = M_CANCEL3;
    run_open(wb, urlb, "CANCEL3");
    P("ARGS %d %ls\n", got_flags, wcsstr(got_url, L"sgnw_b.html") ? L"url-ok" : got_url);
    P("CTX %ls\n", wcsstr(got_ctx, L"sgnw_a.html") ? L"ctx-ok" : got_ctx);

    /* 2: a sink that only knows NewWindow2 */
    n3 = n2 = 0;
    advise(wb2, &cookie); navigate(wb2, url);
    mode = M_CANCEL2_ONLY;
    run_open(wb2, urlb, "CANCEL2");

    /* 3: the host provides the browser the page goes to */
    n3 = n2 = 0;
    advise(wb3, &cookie); navigate(wb3, url);
    mode = M_HANDOVER3;
    run_open(wb3, urlb, "HANDOVER");
    {
        BSTR loc = NULL; READYSTATE rs = 0; int tries;
        for (tries = 0; tries < 100; tries++) {
            pump(100); IWebBrowser2_get_ReadyState(handover, &rs);
            loc = NULL; IWebBrowser2_get_LocationURL(handover, &loc);
            if (loc && wcsstr(loc, L"sgnw_b.html") && rs == READYSTATE_COMPLETE) break;
            SysFreeString(loc);
        }
        P("HANDOVERLOC %d\n", loc && wcsstr(loc, L"sgnw_b.html") != NULL);
        SysFreeString(loc);
    }
    IWebBrowser2_Quit(handover); IWebBrowser2_Quit(wb3); IWebBrowser2_Quit(wb2); IWebBrowser2_Quit(wb);

    DeleteFileW(path); DeleteFileW(pathb);
    P("DONE\n");
    return 0;
}
