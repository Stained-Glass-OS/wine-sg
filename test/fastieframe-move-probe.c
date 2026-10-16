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

static void run_script(IWebBrowser2 *wb, const WCHAR *script)
{
    IServiceProvider *sp = NULL; IHTMLWindow2 *w = NULL; BSTR b, lang = SysAllocString(L"JScript"); VARIANT r;
    IWebBrowser2_QueryInterface(wb, &IID_IServiceProvider, (void**)&sp);
    IServiceProvider_QueryService(sp, &IID_IHTMLWindow2, &IID_IHTMLWindow2, (void**)&w);
    b = SysAllocString(script); VariantInit(&r);
    IHTMLWindow2_execScript(w, b, lang, &r);
    SysFreeString(b); SysFreeString(lang);
    pump(300);
    IHTMLWindow2_Release(w); IServiceProvider_Release(sp);
}

static void geom(IWebBrowser2 *wb, const char *tag)
{
    LONG l = -1, t = -1, w = -1, h = -1;
    IWebBrowser2_get_Left(wb, &l); IWebBrowser2_get_Top(wb, &t); IWebBrowser2_get_Width(wb, &w); IWebBrowser2_get_Height(wb, &h);
    P("%s %ld %ld %ld %ld\n", tag, l, t, w, h);
}

int main(void)
{
    IWebBrowser2 *wb; WCHAR path[MAX_PATH], url[MAX_PATH + 16]; HANDLE f; int i;
    setvbuf(stdout, NULL, _IONBF, 0);
    CoInitialize(NULL);
    GetTempPathW(MAX_PATH, path);
    lstrcatW(path, L"sgmv_a.html");
    f = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    { static const char html[] = "<html><head><title>start</title></head><body>A</body></html>"; DWORD n; WriteFile(f, html, sizeof(html) - 1, &n, NULL); CloseHandle(f); }
    lstrcpyW(url, L"file:///"); lstrcatW(url, path);
    for (i = 0; url[i]; i++) if (url[i] == '\\') url[i] = '/';
    wb = new_browser();
    P("CREATE %d\n", wb != NULL);
    navigate(wb, url);
    IWebBrowser2_put_Left(wb, 10); IWebBrowser2_put_Top(wb, 20); IWebBrowser2_put_Width(wb, 800); IWebBrowser2_put_Height(wb, 600);
    geom(wb, "START");
    run_script(wb, L"window.moveTo(150, 170);");
    geom(wb, "MOVETO");
    run_script(wb, L"window.moveBy(5, -10);");
    geom(wb, "MOVEBY");
    run_script(wb, L"window.resizeTo(500, 420);");
    geom(wb, "RESIZETO");
    run_script(wb, L"window.resizeBy(-100, 30);");
    geom(wb, "RESIZEBY");
    IWebBrowser2_Quit(wb);
    DeleteFileW(path);
    P("DONE\n");
    return 0;
}
