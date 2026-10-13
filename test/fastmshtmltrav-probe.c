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
L"<div id=a><p id=p1>x<b id=b1>y</b></p><p id=p2>z</p><!--c--></div>"
L"</body></html>";

static const WCHAR script1[] =
L"var r=[]; function t(n,v){r.push(n+'='+v);} function $(i){return document.getElementById(i);}"
L"function T(n,f){try{f();}catch(e){r.push(n+'=ERR '+(e.number&0xffff)+' '+e.message);}}"
L"function ids(it){var o=[],n; while((n=it.nextNode())){o.push(n.id||n.nodeName);} return o.join(',');}"
L"T('IT0',function(){var it=document.createNodeIterator($('a'),1,null,false); t('IT0',typeof it+' '+it.root.id+' '+it.whatToShow+' '+(it.filter===null)+' '+ids(it));});"
L"T('IT1',function(){var it=document.createNodeIterator($('a'),0xFFFFFFFF); t('IT1',ids(it));});"
L"T('IT2',function(){var seen=[]; var it=document.createNodeIterator($('a'),1,function(n){seen.push(n.id); return n.tagName=='P'?3:1;},false); t('IT2',ids(it)+' seen='+seen.join(',')+' '+(typeof it.filter));});"
L"T('IT3',function(){var it=document.createNodeIterator($('a'),1,{acceptNode:function(n){return n.id=='b1'?2:1;}},false); t('IT3',ids(it));});"
L"T('IT4',function(){var it=document.createNodeIterator($('a'),1); var f=it.nextNode(); var s=it.nextNode(); var back=it.previousNode(); t('IT4',f.id+' '+s.id+' '+back.id); it.detach();});"
L"T('IT5',function(){var it=document.createNodeIterator($('a'),1); it.nextNode(); it.nextNode(); var p1=$('p1'); var par=p1.parentNode, nx=p1.nextSibling; par.removeChild(p1); t('IT5',it.nextNode().id); par.insertBefore(p1,nx);});"
L"T('TW0',function(){var tw=document.createTreeWalker($('a'),1,null,false); t('TW0',tw.root.id+' '+tw.currentNode.id+' '+tw.firstChild().id+' '+tw.nextSibling().id+' '+tw.nextSibling()+' '+tw.parentNode().id+' '+tw.lastChild().id);});"
L"T('TW1',function(){var tw=document.createTreeWalker($('a'),1); var o=[],n; while((n=tw.nextNode())) o.push(n.id); t('TW1',o.join(',')+' '+tw.currentNode.id); tw.currentNode=$('p1'); t('TW1b',tw.currentNode.id+' '+tw.firstChild().id+' '+tw.previousNode().id+' '+tw.previousSibling());});"
L"T('TW2',function(){var tw=document.createTreeWalker($('a'),1,function(n){return n.tagName=='B'?2:1;},false); var o=[],n; while((n=tw.nextNode())) o.push(n.id); t('TW2',o.join(','));});"
L"T('TW3',function(){var tw=document.createTreeWalker(document.body,4,null,false); var o=[],n; while((n=tw.nextNode())) o.push(n.nodeValue); t('TW3',o.join(','));});"
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
