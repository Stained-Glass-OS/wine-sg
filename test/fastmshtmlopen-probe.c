#define COBJMACROS
#include <windows.h>
#include <ole2.h>
#include <mshtml.h>
#include <stdio.h>
static const IID IID_IHTMLEventObj3_ = {0x3050f680,0x98b5,0x11cf,{0xbb,0x82,0x00,0xaa,0x00,0xbd,0xce,0x0b}};
#define IID_IHTMLEventObj3 IID_IHTMLEventObj3_
static const IID IID_IHTMLEventObj4_ = {0x3050f814,0x98b5,0x11cf,{0xbb,0x82,0x00,0xaa,0x00,0xbd,0xce,0x0b}};
#define IID_IHTMLEventObj4 IID_IHTMLEventObj4_
static const IID IID_IHTMLEventObj5_ = {0x30510478,0x98b5,0x11cf,{0xbb,0x82,0x00,0xaa,0x00,0xbd,0xce,0x0b}};
#define IID_IHTMLEventObj5 IID_IHTMLEventObj5_
static const IID IID_IHTMLEventObj6_ = {0x30510734,0x98b5,0x11cf,{0xbb,0x82,0x00,0xaa,0x00,0xbd,0xce,0x0b}};
#define IID_IHTMLEventObj6 IID_IHTMLEventObj6_
static const IID IID_IHTMLDocument4_ = {0x3050f69a,0x98b5,0x11cf,{0xbb,0x82,0x00,0xaa,0x00,0xbd,0xce,0x0b}};
#define IID_IHTMLDocument4 IID_IHTMLDocument4_
#define P(...) printf(__VA_ARGS__)
static const CLSID CLSID_HTMLDocument_ = {0x25336920,0x03f9,0x11cf,{0x8f,0xd0,0x00,0xaa,0x00,0x68,0x6f,0x13}};



static const WCHAR page[] =
L"<!DOCTYPE html>\n<html><head><meta http-equiv=\"x-ua-compatible\" content=\"IE=11\"></head><body>"
L"<iframe name=fr1 src=\"about:blank\"></iframe><iframe name=fr2 src=\"about:blank\"></iframe>"
L"</body></html>";

static const WCHAR script[] =
L"var r=[]; function t(n,v){r.push(n+'='+v);}"
L"function T(n,f){try{f();}catch(e){r.push(n+'=ERR '+(e.number&0xffff)+' '+e.message);}}"
L"window.name='toplevel';"
/* a name finds a frame of the page */
L"T('FRAME',function(){var w=window.open('about:blank#a','fr2'); t('FRAME0',w.name+' '+(w===window.frames['fr2']));});"
/* the top window is found by its name from a frame */
L"T('TOPNAME',function(){var w=frames['fr1'].open('about:blank#b','toplevel'); t('TOPNAME0',w.name);});"
L"T('PARENT',function(){var w=frames['fr1'].open('about:blank#c','_parent'); t('PARENT0',w.name);});"
L"T('TOP',function(){var w=frames['fr1'].open('about:blank#d','_top'); t('TOP0',w.name);});"
L"T('SELF',function(){var w=frames['fr1'].open('about:blank#e','_self'); t('SELF0',w.name);});"
L"t('MODE',document.documentMode);"
L"document.title=r.join('|');";
static void pump(DWORD ms)
{
    DWORD end = GetTickCount() + ms;
    MSG msg;
    while (GetTickCount() < end) {
        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
        Sleep(10);
    }
}

static IHTMLElement *byid(IHTMLDocument3 *d3, const WCHAR *id)
{
    IHTMLElement *e = NULL; BSTR b = SysAllocString(id);
    IHTMLDocument3_getElementById(d3, b, &e); SysFreeString(b);
    return e;
}


int main(void)
{
    IHTMLDocument2 *doc; IHTMLWindow2 *win; HRESULT hr;
    VARIANT res; BSTR b, lang;
    setvbuf(stdout, NULL, _IONBF, 0);
    CoInitialize(NULL);
    hr = CoCreateInstance(&CLSID_HTMLDocument_,NULL,CLSCTX_INPROC_SERVER,&IID_IHTMLDocument2,(void**)&doc);
    P("DOC %08lx\n", hr);
    {
        IPersistStreamInit *psi; IStream *stream; HGLOBAL mem; char *ascii; int len;
        len = WideCharToMultiByte(CP_ACP, 0, page, -1, NULL, 0, NULL, NULL) - 1;
        mem = GlobalAlloc(0, len); ascii = GlobalLock(mem);
        WideCharToMultiByte(CP_ACP, 0, page, -1, ascii, len, NULL, NULL); GlobalUnlock(mem);
        CreateStreamOnHGlobal(mem, TRUE, &stream);
        IHTMLDocument2_QueryInterface(doc, &IID_IPersistStreamInit, (void**)&psi);
        IPersistStreamInit_InitNew(psi);
        IPersistStreamInit_Load(psi, stream);
        IPersistStreamInit_Release(psi); IStream_Release(stream);
    }
    pump(2000);
    IHTMLDocument2_get_parentWindow(doc, &win);
    b = SysAllocString(script); lang = SysAllocString(L"jscript");
    VariantInit(&res);
    hr = IHTMLWindow2_execScript(win, b, lang, &res);
    P("SCRIPT %08lx\n", hr);
    SysFreeString(b); SysFreeString(lang);
    pump(500);
    b = NULL; IHTMLDocument2_get_title(doc, &b);
    if (b) {
        WCHAR *p = b, *q;
        while (p && *p) {
            q = wcschr(p, '|');
            if (q) *q = 0;
            P("%ls\n", p);
            p = q ? q + 1 : NULL;
        }
    }
    SysFreeString(b);
    P("DONE\n");
    return 0;
}
