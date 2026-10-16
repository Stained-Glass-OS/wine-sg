#define COBJMACROS
#include <windows.h>
#include <ole2.h>
#include <mshtml.h>
#include <docobj.h>
#include <stdio.h>
static const IID IID_IHTMLElement7_ = {0x305107aa,0x98b5,0x11cf,{0xbb,0x82,0x00,0xaa,0x00,0xbd,0xce,0x0b}};
#define IID_IHTMLElement7 IID_IHTMLElement7_
static const IID DIID_HTMLElementEvents2_ = {0x3050f60f,0x98b5,0x11cf,{0xbb,0x82,0x00,0xaa,0x00,0xbd,0xce,0x0b}};
#define DIID_HTMLElementEvents2 DIID_HTMLElementEvents2_
static const IID IID_IHTMLDocument5_ = {0x3050f80c,0x98b5,0x11cf,{0xbb,0x82,0x00,0xaa,0x00,0xbd,0xce,0x0b}};
#define IID_IHTMLDocument5 IID_IHTMLDocument5_
#define P(...) printf(__VA_ARGS__)
static const CLSID CLSID_HTMLDocument_ = {0x25336920,0x03f9,0x11cf,{0x8f,0xd0,0x00,0xaa,0x00,0x68,0x6f,0x13}};

static const WCHAR page[] =
L"<!DOCTYPE html>\n<html><head><meta http-equiv=\"x-ua-compatible\" content=\"IE=11\"></head><body>"
L"<div id=a title=t1><i>q</i></div><div id=b><p id=p1>x</p></div><div id=c></div>"
L"<svg id=sv width=10 height=10><circle id=ci r=2></circle></svg></body></html>";

static const WCHAR script1[] =
L"var r=[]; function t(n,v){r.push(n+'='+v);} function $(i){return document.getElementById(i);}"
L"function T(n,f){try{f();}catch(e){r.push(n+'=ERR '+(e.number&0xffff)+' '+e.message);}}"
L"T('DM',function(){t('DM0',document.designMode); document.designMode='off'; t('DM1',document.designMode);});"
L"T('CM',function(){var c=document.createComment('x'); c.className='k'; c.lang='fr'; c.tabIndex=3; t('CM',c.className+' '+c.lang+' '+c.tabIndex);});"
L"T('APPIN',function(){var w=document.createElement('b'); $('a').applyElement(w,'inside'); t('APPIN',$('a').innerHTML.toLowerCase());});"
L"T('APPOUT',function(){var w=document.createElement('u'); $('c').applyElement(w,'outside'); t('APPOUT',$('c').parentNode.tagName+' '+w.firstChild.id);});"
L"T('PROTO',function(){t('PROTO',document.protocol.length>0);});"
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

    /* C-side checks, before scripts alter the page */
    {
        IHTMLElement *svg = byid(doc3, L"sv"), *a = byid(doc3, L"a");
        IProvideClassInfo2 *pci; GUID g; IHTMLElement7 *e7; VARIANT v; IHTMLElement2 *e2; IHTMLRect *rect;
        LONG n; BSTR s = NULL; IOleObject *oo; IObjectWithSite *ows; IUnknown *site = NULL; IEnumUnknown *eu, *eu2; ULONG got;
        IOleDocument *od; DWORD st = 0;

        hr = IHTMLElement_QueryInterface(a, &IID_IProvideClassInfo2, (void**)&pci);
        if (SUCCEEDED(hr)) {
            hr = IProvideClassInfo2_GetGUID(pci, GUIDKIND_DEFAULT_SOURCE_DISP_IID, &g); P("PCIG=%08lx\n", g.Data1);
            P("PCI=%08lx %d\n", hr, IsEqualGUID(&g, &DIID_HTMLElementEvents2));
            hr = IProvideClassInfo2_GetGUID(pci, 7, &g);
            P("PCIBAD=%08lx\n", hr);
            IProvideClassInfo2_Release(pci);
        } else P("PCI=noiface\n");

        hr = IHTMLElement_QueryInterface(a, &IID_IHTMLElement7, (void**)&e7);
        if (SUCCEEDED(hr)) {
            V_VT(&v) = VT_DISPATCH; V_DISPATCH(&v) = NULL;
            hr = IHTMLElement7_put_onmstransitionend(e7, v);
            VariantInit(&v);
            hr = IHTMLElement7_get_onmstransitionend(e7, &v);
            P("EV=%08lx\n", hr);
            IHTMLElement7_Release(e7);
        }

        hr = IHTMLElement_QueryInterface(a, &IID_IHTMLElement2, (void**)&e2);
        hr = IHTMLElement2_getBoundingClientRect(e2, &rect);
        hr = IHTMLRect_put_left(rect, 77);
        n = 0; IHTMLRect_get_left(rect, &n);
        P("RECT=%08lx %ld\n", hr, n);
        IHTMLRect_Release(rect);
        s = NULL; hr = IHTMLElement2_componentFromPoint(e2, -5000, -5000, &s);
        P("CFP=%08lx %ls\n", hr, s ? s : L"(null)");
        IHTMLElement2_Release(e2);

        if (svg) {
            s = NULL; hr = IHTMLElement_get_innerHTML(svg, &s);
            P("SVGIN=%08lx %d\n", hr, s && wcsstr(s, L"circle") != NULL);
            hr = IHTMLElement_put_innerHTML(svg, L"<rect id='r1' width='3' height='3'></rect>");
            s = NULL; IHTMLElement_get_innerHTML(svg, &s);
            P("SVGSET=%08lx %d\n", hr, s && wcsstr(s, L"rect") != NULL && !wcsstr(s, L"circle"));
            s = NULL; hr = IHTMLElement_get_lang(svg, &s);
            P("SVGLANG=%08lx\n", hr);
            hr = IHTMLElement_put_lang(svg, L"de"); s = NULL; IHTMLElement_get_lang(svg, &s);
            P("SVGLANG2=%08lx %ls\n", hr, s ? s : L"(null)");
            hr = IHTMLElement_get_offsetWidth(svg, &n);
            P("SVGOFF=%08lx\n", hr);
        }

        {
            IHTMLDocument5 *d5; IHTMLDOMNode *cn; IHTMLElement *ce; BSTR cb = SysAllocString(L"hi");
            IHTMLDocument2_QueryInterface(doc, &IID_IHTMLDocument5, (void**)&d5);
            hr = IHTMLDocument5_createComment(d5, cb, &cn);
            IHTMLDOMNode_QueryInterface(cn, &IID_IHTMLElement, (void**)&ce);
            SysFreeString(cb); cb = SysAllocString(L"kls");
            IHTMLElement_put_className(ce, cb);
            s = NULL; IHTMLElement_get_className(ce, &s);
            P("CMC=%ls\n", s ? s : L"(null)");
            s = NULL; IHTMLElement_get_lang(ce, &s);
            P("CML=[%ls]\n", s ? s : L"(null)");
            IHTMLElement_Release(ce); IHTMLDOMNode_Release(cn); IHTMLDocument5_Release(d5);
        }
        hr = IHTMLDocument2_QueryInterface(doc, &IID_IOleObject, (void**)&oo);
        hr = IOleObject_IsUpToDate(oo); P("UPTODATE=%08lx\n", hr);
        hr = IOleObject_Update(oo); P("UPDATE=%08lx\n", hr);
        hr = IOleObject_DoVerb(oo, 12345, NULL, NULL, 0, NULL, NULL); P("BADVERB=%08lx\n", hr);
        IOleObject_Release(oo);
        hr = IHTMLDocument2_QueryInterface(doc, &IID_IObjectWithSite, (void**)&ows);
        if (SUCCEEDED(hr)) {
            hr = IObjectWithSite_GetSite(ows, &IID_IUnknown, (void**)&site);
            P("SITE0=%08lx\n", hr);
            hr = IObjectWithSite_SetSite(ows, (IUnknown*)doc);
            site = NULL; hr = IObjectWithSite_GetSite(ows, &IID_IUnknown, (void**)&site);
            P("SITE1=%08lx %d\n", hr, site != NULL);
            if (site) IUnknown_Release(site);
            IObjectWithSite_SetSite(ows, NULL);
            IObjectWithSite_Release(ows);
        }
        hr = IHTMLDocument2_QueryInterface(doc, &IID_IOleDocument, (void**)&od);
        hr = IOleDocument_GetDocMiscStatus(od, &st); P("DOCMISC=%08lx %d\n", hr, (int)(st != 0));
        IOleDocument_Release(od);
        {
            IOleContainer *oc;
            hr = IHTMLDocument2_QueryInterface(doc, &IID_IOleContainer, (void**)&oc);
            hr = IOleContainer_EnumObjects(oc, OLECONTF_EMBEDDINGS, &eu);
            hr = IEnumUnknown_Clone(eu, &eu2);
            P("ENUMCLONE=%08lx %08lx %08lx\n", hr, IEnumUnknown_Skip(eu2, 1), IEnumUnknown_Reset(eu2));
            IEnumUnknown_Release(eu2); IEnumUnknown_Release(eu);
            hr = IOleContainer_LockContainer(oc, TRUE); P("LOCK=%08lx\n", hr);
            IOleContainer_Release(oc);
        }
        IHTMLElement_Release(a); if (svg) IHTMLElement_Release(svg);
    }

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
