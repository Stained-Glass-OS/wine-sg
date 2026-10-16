#define COBJMACROS
#include <windows.h>
#include <ole2.h>
#include <exdisp.h>
#include <mshtml.h>
#include <servprov.h>
#include <stdio.h>
#define P(...) printf(__VA_ARGS__)
static const CLSID CLSID_IE_ = {0x0002df01,0x0000,0x0000,{0xc0,0x00,0x00,0x00,0x00,0x00,0x00,0x46}};

static void pump(DWORD ms)
{
    DWORD end = GetTickCount() + ms;
    MSG msg;
    while (GetTickCount() < end) {
        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
        Sleep(10);
    }
}

int main(int argc, char **argv)
{
    IWebBrowser2 *wb; IServiceProvider *sp; IHTMLWindow2 *w = NULL; HRESULT hr;
    BSTR b; VARIANT v, e; READYSTATE rs = 0; int i;
    WCHAR path[MAX_PATH], url[MAX_PATH + 16];
    HANDLE f;
    setvbuf(stdout, NULL, _IONBF, 0);
    CoInitialize(NULL);
    GetTempPathW(MAX_PATH, path);
    lstrcatW(path, L"sgie.html");
    f = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    { static const char html[] = "<html><head><title>start</title></head><body><p id=p>hello</p></body></html>"; DWORD n; WriteFile(f, html, sizeof(html) - 1, &n, NULL); CloseHandle(f); }
    lstrcpyW(url, L"file:///"); lstrcatW(url, path);
    for (i = 0; url[i]; i++) if (url[i] == '\\') url[i] = '/';

    hr = CoCreateInstance(&CLSID_IE_, NULL, CLSCTX_LOCAL_SERVER, &IID_IWebBrowser2, (void**)&wb);
    P("CREATE %08lx\n", hr);
    if (FAILED(hr)) return 1;
    IWebBrowser2_put_Visible(wb, VARIANT_FALSE);
    b = SysAllocString(url); VariantInit(&v); VariantInit(&e);
    hr = IWebBrowser2_Navigate(wb, b, &v, &e, &e, &e); SysFreeString(b);
    P("NAVIGATE %08lx\n", hr);
    for (i = 0; i < 400; i++) {
        pump(50);
        IWebBrowser2_get_ReadyState(wb, &rs);
        if (rs == READYSTATE_COMPLETE) { IDispatch *d = NULL; if (SUCCEEDED(IWebBrowser2_get_Document(wb, &d)) && d) { IDispatch_Release(d); break; } }
    }
    P("READY %d\n", rs);
    hr = IWebBrowser2_QueryInterface(wb, &IID_IServiceProvider, (void**)&sp);
    hr = IServiceProvider_QueryService(sp, &IID_IHTMLWindow2, &IID_IHTMLWindow2, (void**)&w);
    P("SERVICE %08lx %d\n", hr, w != NULL);
    if (w) {
        LONG n = -1; IHTMLDocument2 *doc = NULL; IHTMLLocation *loc = NULL; IOmNavigator *nav = NULL; IHTMLScreen *scr = NULL; VARIANT r;
        hr = IHTMLWindow2_get_length(w, &n); P("LENGTH %08lx %ld\n", hr, n);
        b = SysAllocString(L"sgwin"); hr = IHTMLWindow2_put_name(w, b); SysFreeString(b); P("PUTNAME %08lx\n", hr);
        b = NULL; hr = IHTMLWindow2_get_name(w, &b); P("GETNAME %08lx %ls\n", hr, b ? b : L"(null)"); SysFreeString(b);
        hr = IHTMLWindow2_get_location(w, &loc);
        if (loc) { b = NULL; IHTMLLocation_get_protocol(loc, &b); P("LOCATION %08lx %ls\n", hr, b ? b : L"(null)"); SysFreeString(b); IHTMLLocation_Release(loc); }
        hr = IHTMLWindow2_get_navigator(w, &nav);
        if (nav) { b = NULL; IOmNavigator_get_appName(nav, &b); P("NAVIGATOR %08lx %d\n", hr, b != NULL && b[0]); SysFreeString(b); IOmNavigator_Release(nav); }
        hr = IHTMLWindow2_get_screen(w, &scr);
        if (scr) { LONG sw = 0; IHTMLScreen_get_width(scr, &sw); P("SCREEN %08lx %d\n", hr, sw > 0); IHTMLScreen_Release(scr); }
        b = SysAllocString(L"document.title='viaie'"); { BSTR lang = SysAllocString(L"JScript"); VariantInit(&r);
          hr = IHTMLWindow2_execScript(w, b, lang, &r); P("EXEC %08lx\n", hr); SysFreeString(lang); } SysFreeString(b);
        hr = IHTMLWindow2_get_document(w, &doc);
        if (doc) { b = NULL; IHTMLDocument2_get_title(doc, &b); P("TITLE %08lx %ls\n", hr, b ? b : L"(null)"); SysFreeString(b); IHTMLDocument2_Release(doc); }
        hr = IHTMLWindow2_scrollTo(w, 0, 0); P("SCROLL %08lx\n", hr);
        { BSTR s = NULL; hr = IHTMLWindow2_toString(w, &s); P("TOSTRING %08lx %d\n", hr, s != NULL); SysFreeString(s); }
        { BSTR s = SysAllocString(L"stat"); hr = IHTMLWindow2_put_defaultStatus(w, s); SysFreeString(s); P("DEFSTATUS %08lx\n", hr); }
        { VARIANT_BOOL closed = 7; hr = IHTMLWindow2_get_closed(w, &closed); P("CLOSED %08lx %d\n", hr, closed); }
        IHTMLWindow2_Release(w);
    }
    {
        IConnectionPointContainer *cpc = NULL; IEnumConnectionPoints *en = NULL, *en2 = NULL; IConnectionPoint *cp; ULONG got, total = 0, rest = 0;
        hr = IWebBrowser2_QueryInterface(wb, &IID_IConnectionPointContainer, (void**)&cpc);
        P("CPC %08lx\n", hr);
        if (cpc) {
            hr = IConnectionPointContainer_EnumConnectionPoints(cpc, &en);
            P("ENUMCP %08lx\n", hr);
            if (en) {
                while (IEnumConnectionPoints_Next(en, 1, &cp, &got) == S_OK) { total++; IConnectionPoint_Release(cp); }
                P("CPCOUNT %lu\n", total);
                IEnumConnectionPoints_Reset(en);
                hr = IEnumConnectionPoints_Skip(en, 1); P("CPSKIP %08lx\n", hr);
                hr = IEnumConnectionPoints_Clone(en, &en2); P("CPCLONE %08lx\n", hr);
                if (en2) { while (IEnumConnectionPoints_Next(en2, 1, &cp, &got) == S_OK) { rest++; IConnectionPoint_Release(cp); } IEnumConnectionPoints_Release(en2); }
                P("CPREST %lu\n", rest);
                IEnumConnectionPoints_Release(en);
            }
            IConnectionPointContainer_Release(cpc);
        }
    }
    IServiceProvider_Release(sp);
    IWebBrowser2_Quit(wb);
    IWebBrowser2_Release(wb);
    DeleteFileW(path);
    P("DONE\n");
    return 0;
}
