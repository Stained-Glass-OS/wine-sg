#define COBJMACROS
#include <windows.h>
#include <ole2.h>
#include <mshtml.h>
#include <stdio.h>
#define P(...) printf(__VA_ARGS__)
static const CLSID CLSID_HTMLDocument_ = {0x25336920,0x03f9,0x11cf,{0x8f,0xd0,0x00,0xaa,0x00,0x68,0x6f,0x13}};
static const WCHAR page[] =
L"<!DOCTYPE html>\n<html><head><meta http-equiv=\"x-ua-compatible\" content=\"IE=11\"></head><body>"
L"<div id=e2>x</div><div id=rt>aa bb cc dd</div></body></html>";
static const WCHAR script[] =
L"var r=[]; function t(n,v){r.push(n+'='+v);} function $(i){return document.getElementById(i);}"
L"try{var s=$('e2').style; s.columnCount='3'; s.columnGap='20px'; t('COL0',s.columnCount+' '+s.columnGap);}catch(e){t('COL0','ERR');}"
L"document.title=r.join('|');";
static void pump(DWORD ms)
{
    DWORD end = GetTickCount() + ms; MSG msg;
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
static const IID IID_IHTMLDocument3_ = {0x3050f485,0x98b5,0x11cf,{0xbb,0x82,0x00,0xaa,0x00,0xbd,0xce,0x0b}};
int main(void)
{
    IHTMLDocument2 *doc; IHTMLDocument3 *doc3; IHTMLWindow2 *win; HRESULT hr; VARIANT res; BSTR b, lang;
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
    IHTMLDocument2_QueryInterface(doc, &IID_IHTMLDocument3_, (void**)&doc3);
    IHTMLDocument2_get_parentWindow(doc, &win);
    b = SysAllocString(script); lang = SysAllocString(L"jscript"); VariantInit(&res);
    hr = IHTMLWindow2_execScript(win, b, lang, &res); P("SCRIPT %08lx\n", hr);
    SysFreeString(b); SysFreeString(lang);
    b = NULL; IHTMLDocument2_get_title(doc, &b);
    if (b) { WCHAR *p = b, *q; while (p && *p) { q = wcschr(p, '|'); if (q) *q = 0; P("%ls\n", p); p = q ? q + 1 : NULL; } }
    SysFreeString(b);
    {
        IHTMLElement *body = NULL, *rt; IHTMLBodyElement *be; IHTMLTxtRange *r; LONG moved = -9; BSTR t;
        IHTMLDocument2_get_body(doc, &body);
        IHTMLElement_QueryInterface(body, &IID_IHTMLBodyElement, (void**)&be);
        IHTMLBodyElement_createTextRange(be, &r);
        rt = byid(doc3, L"rt");
        IHTMLTxtRange_moveToElementText(r, rt);
        t = SysAllocString(L"word"); hr = IHTMLTxtRange_moveStart(r, t, 1, &moved); P("MOVESTART %08lx %ld", hr, moved);
        { BSTR txt = NULL; IHTMLTxtRange_get_text(r, &txt); P(" [%ls]\n", txt ? txt : L""); SysFreeString(txt); }
        hr = IHTMLTxtRange_moveEnd(r, t, -1, &moved); P("MOVEEND %08lx %ld", hr, moved);
        { BSTR txt = NULL; IHTMLTxtRange_get_text(r, &txt); P(" [%ls]\n", txt ? txt : L""); SysFreeString(txt); }
        SysFreeString(t);
        {
            IHTMLSelectionObject *sel = NULL; BSTR st = NULL, rhtml = NULL;
            IHTMLTxtRange_moveToElementText(r, rt);
            hr = IHTMLTxtRange_select(r); P("SELECT %08lx\n", hr);
            IHTMLDocument2_get_selection(doc, &sel);
            hr = IHTMLSelectionObject_clear(sel); P("SELCLEAR %08lx\n", hr);
            IHTMLElement_get_innerHTML(rt, &rhtml); P("AFTERCLEAR [%ls]\n", rhtml ? rhtml : L""); SysFreeString(rhtml);
            IHTMLTxtRange_moveToElementText(r, body);
            IHTMLTxtRange_select(r);
            hr = IHTMLSelectionObject_empty(sel); P("SELEMPTY %08lx\n", hr);
            st = NULL; IHTMLSelectionObject_get_type(sel, &st); P("SELTYPE %ls\n", st ? st : L""); SysFreeString(st);
            IHTMLSelectionObject_Release(sel);
        }
        IHTMLTxtRange_Release(r); IHTMLBodyElement_Release(be); IHTMLElement_Release(rt); IHTMLElement_Release(body);
    }
    {
        IOleObject *oo; SIZEL sz;
        IHTMLDocument2_QueryInterface(doc, &IID_IOleObject, (void**)&oo);
        sz.cx = sz.cy = 0xdeadbeef;
        hr = IOleObject_SetExtent(oo, 0, &sz); P("EXT_BADBOTH %08lx\n", hr);
        sz.cx = 800; sz.cy = 700;
        hr = IOleObject_SetExtent(oo, 0, &sz); P("EXT_BADASPECT %08lx\n", hr);
        sz.cx = sz.cy = 0xdeadbeef;
        hr = IOleObject_SetExtent(oo, DVASPECT_CONTENT, &sz); P("EXT_NEG %08lx\n", hr);
        sz.cx = 800; sz.cy = 700;
        hr = IOleObject_SetExtent(oo, DVASPECT_CONTENT, &sz); P("EXT_OK %08lx\n", hr);
        IOleObject_Release(oo);
    }
    P("DONE\n");
    return 0;
}
