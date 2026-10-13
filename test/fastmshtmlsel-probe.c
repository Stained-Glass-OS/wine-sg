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
L"var r=[]; function t(n,v){r.push(n+'='+v);} function $(i){return document.getElementById(i);}"
L"function T(n,f){try{f();}catch(e){r.push(n+'=ERR '+(e.number&0xffff)+' '+e.message);}}"
L"var sel=window.getSelection();"
L"T('S0',function(){t('S0',typeof sel+' '+sel.rangeCount+' '+sel.isCollapsed+' '+(document.getSelection().rangeCount));});"
L"T('SA',function(){var rg=document.createRange(); t('SA',typeof rg+' '+typeof rg.selectNodeContents+' '+typeof rg.toString+' '+typeof sel.addRange); rg.selectNodeContents($('s1')); t('SB','ok'); sel.addRange(rg); t('SC','ok');});"
L"T('S1',function(){var rg=document.createRange(); rg.selectNodeContents($('s1')); sel.addRange(rg); t('S1',sel.rangeCount+' '+sel.isCollapsed+' ['+sel.toString()+'] '+sel.anchorNode.nodeValue+' '+sel.anchorOffset+' '+sel.focusOffset);});"
L"T('S2',function(){var rg=sel.getRangeAt(0); t('S2',rg.startContainer.nodeValue+' '+rg.toString()+' '+(rg.collapsed));});"
L"T('S3',function(){sel.collapseToEnd(); t('S3',sel.isCollapsed+' '+sel.anchorOffset+' ['+sel.toString()+']'); sel.collapseToStart(); t('S3b',sel.anchorOffset);});"
L"T('S4',function(){sel.removeAllRanges(); t('S4',sel.rangeCount+' '+sel.anchorNode);});"
L"T('S5',function(){sel.selectAllChildren($('p1')); t('S5',sel.rangeCount+' ['+sel.toString()+']'); sel.collapse($('b'),0); t('S5b',sel.isCollapsed+' '+sel.focusNode.id);});"
L"T('S6',function(){sel.selectAllChildren($('s1')); sel.deleteFromDocument(); t('S6',$('s1').innerHTML.length+' '+sel.isCollapsed);});"
L"T('S7',function(){try{sel.getRangeAt(5);t('S7','no error');}catch(e){t('S7','error');}});"
L"T('S8',function(){sel.removeAllRanges(); var rg=document.createRange(); rg.selectNodeContents($('p1')); sel.addRange(rg); t('S8a',sel.rangeCount); sel.removeRange(rg); t('S8',sel.rangeCount);});"
L"T('S9',function(){t('S9',(Object.prototype.toString.call(sel))+' '+(sel instanceof Selection));});"
L"T('P0',function(){var a=performance.now(); var b=performance.now(); t('P0',(typeof a)+' '+(a>0)+' '+(b>=a)+' '+(a<600000)+' '+(a!==Math.floor(a)||b!==Math.floor(b)));});"
L"T('GA',function(){var n=$('a').getAttributeNode('title'); t('GA0',typeof n+' '+(n===null)); t('GA1',n.value+' '+n.name+' '+n.specified+' '+n.nodeValue);});"
L"T('GD',function(){$('a').setAttribute('data-x','5'); var n=$('a').getAttributeNode('data-x'); t('GD',n===null?'null':n.value+' '+n.name); var m=$('a').getAttributeNode('id'); t('GD2',m===null?'null':m.value);});"
L"T('GC',function(){var n=$('a').getAttributeNode('nosuch'); t('GC',n===null);});"
L"document.title=r.join('|');";

static const WCHAR script2[] =
L"t('ASYNC',calls); document.title=r.join('|');";

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
