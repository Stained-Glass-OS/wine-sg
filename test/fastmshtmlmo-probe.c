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
L"function d(rec){return rec.type+':'+rec.target.id+':'+rec.addedNodes.length+':'+rec.removedNodes.length+':'+(rec.attributeName)+':'+(rec.oldValue);}"
L"var calls=0;"
/* childList + attributes on a, no subtree */
L"var o1=new MutationObserver(function(recs,obs){calls++; for(var i=0;i<recs.length;i++) t('O1_'+i,d(recs[i])); t('O1_SELF',obs===o1);});"
L"o1.observe($('a'),{childList:true,attributes:true,attributeOldValue:true});"
L"var e=document.createElement('b'); $('a').appendChild(e);"
L"$('a').setAttribute('title','t2');"
L"$('a').removeChild($('s1'));"
/* subtree: attribute deep in b */
L"var o2=new MutationObserver(function(recs){for(var i=0;i<recs.length;i++) t('O2_'+i,d(recs[i]));});"
L"o2.observe($('b'),{attributes:true,subtree:true,attributeFilter:['class']});"
L"$('p1').setAttribute('id','p1'); $('p1').className='k'; $('p1').setAttribute('title','ignored');"
/* characterData with old value */
L"var o3=new MutationObserver(function(recs){for(var i=0;i<recs.length;i++) t('O3_'+i,d(recs[i])+':'+recs[i].target.nodeValue);});"
L"$('c').appendChild(document.createTextNode('abc'));"
L"o3.observe($('c'),{characterData:true,characterDataOldValue:true,subtree:true});"
L"$('c').firstChild.nodeValue='xyz';"
/* takeRecords and disconnect */
L"var o4=new MutationObserver(function(){t('O4_CALLED','yes');});"
L"o4.observe($('d'),{childList:true}); $('d').appendChild(document.createElement('i'));"
L"var taken=o4.takeRecords(); t('TAKE0',taken.length+' '+taken[0].type+' '+taken[0].addedNodes[0].tagName+' '+(taken[0].previousSibling===null)+' '+o4.takeRecords().length);"
L"o4.observe($('d'),{childList:true}); $('d').appendChild(document.createElement('u')); o4.disconnect();"
/* innerHTML gives one record with several nodes */
L"var o5=new MutationObserver(function(recs){t('O5_N',recs.length); for(var i=0;i<recs.length;i++) t('O5_'+i,d(recs[i]));});"
L"o5.observe($('d'),{childList:true}); $('d').innerHTML='<s></s><s></s>';"
L"t('SYNC',calls);";
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
    pump(800);
    b = SysAllocString(script2);
    hr = IHTMLWindow2_execScript(win, b, lang, &res);
    P("SCRIPT2 %08lx\n", hr);
    SysFreeString(b); SysFreeString(lang);
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
