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
L"<p id=a>One two. Three four!  Five six?</p><p id=b>Next para.</p>"
L"</body></html>";

static const WCHAR script[] =
L"var r=[]; function t(n,v){r.push(n+'='+v);}"
L"function T(n,f){try{f();}catch(e){r.push(n+'=ERR '+(e.number&0xffff)+' '+e.message);}}"
L"function fresh(){var x=document.body.createTextRange(); x.moveToElementText(document.getElementById('a')); x.collapse(true); return x;}"
/* sentence units */
L"T('MS',function(){var x=fresh(); var n=x.move('sentence',1); t('MS0',n); x.moveEnd('sentence',1); t('MS1','['+x.text+']');"
L" n=x.move('sentence',1); x.moveEnd('sentence',1); t('MS2',n+'['+x.text+']');"
L" n=x.move('sentence',-1); x.moveEnd('sentence',1); t('MS3',n+'['+x.text+']');"
L" n=x.move('sentence',-5); t('MS4',n);});"
L"T('MSS',function(){var x=fresh(); x.moveEnd('sentence',2); t('MSS0','['+x.text+']'); var n=x.moveStart('sentence',1); t('MSS1',n+'['+x.text+']');"
L" n=x.moveEnd('sentence',-1); t('MSS2',n+'['+x.text+']');});"
L"T('EXP',function(){var x=fresh(); x.move('character',9); t('EXP0',x.expand('sentence')+'['+x.text+']'); x.collapse(true); x.move('character',3);"
L" t('EXP1',x.expand('sentence')+'['+x.text+']');});"
/* textedit */
L"T('TE',function(){var x=fresh(); x.move('character',3); var n=x.moveEnd('textedit',1); t('TE0',n+'['+x.text+']'); n=x.moveStart('textedit',-1); t('TE1',n+'['+x.text+']');"
L" n=x.moveEnd('textedit',1); t('TE2',n); x=fresh(); n=x.move('textedit',1); t('TE3',n+'['+x.text+']'); x.expand('textedit'); t('TE4',x.text.length>20);"
L" n=x.move('textedit',-1); t('TE5',n); n=x.move('textedit',-1); t('TE6',n);});"
/* bookmarks */
L"T('BM',function(){var x=fresh(); x.move('character',4); x.moveEnd('word',2); var b=x.getBookmark(); t('BM0',typeof b+' '+b); var txt=x.text;"
L" var y=document.body.createTextRange(); t('BMa',typeof y.moveToBookmark); var ok=y.moveToBookmark(b); t('BM1',ok+' '+(y.text==txt)); t('BMe',typeof y.isEqual); "
L" x.collapse(false); x.moveEnd('character',5); var ok2=x.moveToBookmark(b); t('BM2',ok2+' '+(x.text==txt));"
L" t('BM3',y.moveToBookmark('garbage')+' '+y.moveToBookmark(''));});"
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
    b = SysAllocString(script); lang = SysAllocString(L"jscript");
    VariantInit(&res);
    hr = IHTMLWindow2_execScript(win, b, lang, &res);
    P("SCRIPT %08lx\n", hr);
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
