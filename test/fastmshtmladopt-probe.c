#define COBJMACROS
#include <windows.h>
#include <ole2.h>
#include <mshtml.h>
#include <stdio.h>
static const IID IID_IHTMLElement3_ = {0x3050f673,0x98b5,0x11cf,{0xbb,0x82,0x00,0xaa,0x00,0xbd,0xce,0x0b}};
#define IID_IHTMLElement3 IID_IHTMLElement3_
#define P(...) printf(__VA_ARGS__)
static const CLSID CLSID_HTMLDocument_ = {0x25336920,0x03f9,0x11cf,{0x8f,0xd0,0x00,0xaa,0x00,0x68,0x6f,0x13}};

static const WCHAR page[] =
L"<!DOCTYPE html>\n<html><head><meta http-equiv=\"x-ua-compatible\" content=\"IE=11\"></head><body>"
L"<div id=a title=t1><span id=s1>one</span></div><div id=b><p id=p1>x</p></div><div id=c></div><div id=d></div>"
L"</body></html>";

static const WCHAR script1[] =
L"var r=[]; function t(n,v){r.push(n+'='+v);}"
L"function T(n,f){try{f();}catch(e){r.push(n+'=ERR '+(e.number&0xffff)+' '+e.message);}}"
L"T('PI0',function(){var p=document.createProcessingInstruction('xml-stylesheet','href=\\'a.css\\''); t('PI0',[p.nodeType,p.target,p.data,p.nodeName,p.nodeValue,typeof ProcessingInstruction].join(';'));});"
L"T('PI1',function(){var p=document.createProcessingInstruction('t','one'); p.data='two'; document.getElementById('c').appendChild(p); t('PI1',[p.data,p.nodeValue,p.parentNode.id,document.getElementById('c').childNodes.length,p.cloneNode(false).target].join(';'));});"
L"T('PI2',function(){try{document.createProcessingInstruction('a b','x');t('PI2','no error');}catch(e){t('PI2','error');}});"
L"T('AD0',function(){var d2=document.implementation.createHTMLDocument('t'); var e=document.getElementById('a'); var s=document.getElementById('s1'); var x=e.firstChild; var q=d2.adoptNode(e);"
L" t('AD0',[q.ownerDocument.body===d2.body,e.ownerDocument.body===d2.body,q===e,e.parentNode===null,document.getElementById('a')===null,s.ownerDocument.body===d2.body,q.firstChild.id,q.getAttribute('title')].join(';'));"
L" d2.body.appendChild(q); t('AD1',[d2.getElementById('s1')===s,q.parentNode===d2.body,d2.body.innerHTML.length>0,s.parentNode===q].join(';'));});"
L"T('AD2',function(){var e=document.getElementById('b'); var q=document.adoptNode(e); t('AD2',[q===e,e.parentNode===null,q.ownerDocument===document].join(';'));});"
L"T('AD3',function(){var d2=document.implementation.createHTMLDocument('t'); var tn=document.createTextNode('hi'); var q=d2.adoptNode(tn); t('AD3',[q.ownerDocument.body===d2.body,q.nodeValue].join(';'));});"
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
    IHTMLDocument2 *doc; IHTMLDocument3 *doc3; IHTMLWindow2 *win; HRESULT hr;
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
    pump(1500);
    IHTMLDocument2_QueryInterface(doc, &IID_IHTMLDocument3, (void**)&doc3);
    IHTMLDocument2_get_parentWindow(doc, &win);
    b = SysAllocString(script1); lang = SysAllocString(L"jscript");
    VariantInit(&res);
    hr = IHTMLWindow2_execScript(win, b, lang, &res);
    P("SCRIPT %08lx\n", hr);
    SysFreeString(b);
    pump(100);
    SysFreeString(lang);
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
