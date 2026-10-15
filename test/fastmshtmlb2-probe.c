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
static const IID IID_IHTMLDocument4_ = {0x3050f69a,0x98b5,0x11cf,{0xbb,0x82,0x00,0xaa,0x00,0xbd,0xce,0x0b}};
#define IID_IHTMLDocument4 IID_IHTMLDocument4_
#define P(...) printf(__VA_ARGS__)
static const CLSID CLSID_HTMLDocument_ = {0x25336920,0x03f9,0x11cf,{0x8f,0xd0,0x00,0xaa,0x00,0x68,0x6f,0x13}};

static const WCHAR page[] =
L"<!DOCTYPE html>\n<html><head><meta http-equiv=\"x-ua-compatible\" content=\"IE=11\"></head><body>"
L"<div id=a></div><svg id=sv width=10 height=10><circle id=ci r=2 cx=3></circle><text x=0 y=20><tspan id=tx>abc</tspan></text></svg></body></html>";

static const WCHAR script1[] =
L"var r=[]; function t(n,v){r.push(n+'='+v);} function $(i){return document.getElementById(i);}"
L"function T(n,f){try{f();}catch(e){r.push(n+'=ERR '+(e.number&0xffff)+' '+e.message);}}"
L"T('NS',function(){var e=document.createElementNS('http://www.w3.org/2000/svg','tspan'); t('NS',e.tagName);});"
L"var s=$('sv'),ci=$('ci'),tx=$('tx');"
L"T('W',function(){t('W',s.width.baseVal.value+' '+s.width.baseVal.unitType);});"
L"T('WS',function(){s.width.baseVal.valueAsString='2cm'; t('WS',s.getAttribute('width')+' '+Math.round(s.width.baseVal.value)+' '+s.width.baseVal.unitType);});"
L"T('CIR',function(){t('CIR',ci.cx.baseVal.value+' '+ci.r.baseVal.value);});"
L"T('MAT',function(){var m=s.createSVGMatrix().translate(5,7).scale(2); var p=s.createSVGPoint(); p.x=1; p.y=1; var q=p.matrixTransform(m); t('MAT',q.x+','+q.y+' '+m.inverse().a);});"
L"T('ANG',function(){var a=s.createSVGAngle(); a.newValueSpecifiedUnits(2,180); a.convertToSpecifiedUnits(3); t('ANG',a.valueInSpecifiedUnits.toFixed(3)+' '+a.valueAsString);});"
L"T('TR',function(){var x=s.createSVGTransform(); x.setRotate(90,0,0); t('TR',x.type+' '+x.angle+' '+Math.round(x.matrix.b));});"
L"T('GEBI',function(){t('GEBI',s.getElementById('ci').id+' '+s.getElementById('a'));});"
L"T('SCALE',function(){s.currentScale=2; s.pauseAnimations(); t('SCALE',s.currentScale+' '+s.animationsPaused()+' '+s.pixelUnitToMillimeterX.toFixed(3));});"
L"T('XB',function(){ci.xmlbase='http://x/'; t('XB',ci.xmlbase);});"
L"T('TXT',function(){t('TXT',tx.getNumberOfChars()+' '+(tx.getComputedTextLength()>0)+' '+(tx.getSubStringLength(0,1)>0)+' '+tx.lengthAdjust.baseVal);});"
L"T('OPEN',function(){t('OPEN0',typeof window.opener); window.opener=window; t('OPEN',window.opener===window);});"
L"T('OFF',function(){t('OFF0',window.offscreenBuffering); window.offscreenBuffering=true; t('OFF',window.offscreenBuffering);});"
L"T('MCS',function(){t('MCS',window.maxConnectionsPerServer>0);});"
L"T('DT',function(){var d=document.implementation.createDocumentType('x:y','pub','sys'); t('DT',d.name);});"
L"T('CD',function(){var d=document.implementation.createDocument('http://www.w3.org/1999/xhtml','html',document.implementation.createDocumentType('html','','')); t('CD',d.documentElement.tagName);});"
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
    {
        IHTMLDocument4 *d4; IHTMLDocument2 *parent = (IHTMLDocument2*)1; BSTR m = NULL;
        IProvideClassInfo2 *pci; IProvideMultipleClassInfo *pmci; GUID g; ULONG n = 0;
        IHTMLDocument2_QueryInterface(doc, &IID_IHTMLDocument4, (void**)&d4);
        hr = IHTMLDocument4_get_media(d4, &m); P("MEDIA0=%08lx %ls\n", hr, m ? m : L"(null)"); SysFreeString(m);
        b = SysAllocString(L"print"); IHTMLDocument4_put_media(d4, b); SysFreeString(b);
        m = NULL; hr = IHTMLDocument4_get_media(d4, &m); P("MEDIA1=%08lx %ls\n", hr, m ? m : L"(null)"); SysFreeString(m);
        IHTMLDocument4_Release(d4);
        hr = IHTMLDocument3_get_parentDocument(doc3, &parent); P("PARENT=%08lx %d\n", hr, parent == NULL);
        hr = IHTMLDocument2_QueryInterface(doc, &IID_IProvideClassInfo2, (void**)&pci);
        memset(&g, 0, sizeof(g));
        hr = IProvideClassInfo2_GetGUID(pci, GUIDKIND_DEFAULT_SOURCE_DISP_IID, &g);
        P("CI2=%08lx %d %08lx\n", hr, (int)(g.Data1 == 0x3050f260), IProvideClassInfo2_GetGUID(pci, 7, &g));
        IProvideClassInfo2_Release(pci);
        hr = IHTMLDocument2_QueryInterface(doc, &IID_IProvideMultipleClassInfo, (void**)&pmci);
        if (SUCCEEDED(hr)) {
            IID prim, src;
            hr = IProvideMultipleClassInfo_GetInfoOfIndex(pmci, 0, MULTICLASSINFO_GETIIDPRIMARY|MULTICLASSINFO_GETIIDSOURCE, NULL, NULL, NULL, &prim, &src);
            P("MCI=%08lx %d\n", hr, (int)(src.Data1 == 0x3050f260));
            IProvideMultipleClassInfo_Release(pmci);
        }
        (void)n;
    }
    {
        static const IID IID_ISVGElement_ = {0x305104c5,0x98b5,0x11cf,{0xbb,0x82,0x00,0xaa,0x00,0xbd,0xce,0x0b}};
        IHTMLElement *ci = byid(doc3, L"ci"), *sv = byid(doc3, L"sv"), *own = NULL; void *svge = NULL, *svgs = NULL, *o1 = (void*)1, *o2 = (void*)1;
        void **vt; BSTR id = NULL;
        typedef HRESULT (__stdcall *getter)(void*, void**);
        hr = IHTMLElement_QueryInterface(ci, &IID_ISVGElement_, &svge);
        vt = svge ? *(void***)svge : NULL;
        if (vt) { hr = ((getter)vt[10])(svge, &o1); hr = ((getter)vt[12])(svge, &o2); }
        if (o1) { IUnknown_QueryInterface((IUnknown*)o1, &IID_IHTMLElement, (void**)&own); if (own) IHTMLElement_get_id(own, &id); }
        P("OWN=%d %ls %d\n", svge != NULL, id ? id : L"(none)", o2 != NULL);
        if (own) IHTMLElement_Release(own);
        SysFreeString(id);
        if (o1) IUnknown_Release((IUnknown*)o1);
        if (o2) IUnknown_Release((IUnknown*)o2);
        if (svge) IUnknown_Release((IUnknown*)svge);
        hr = IHTMLElement_QueryInterface(sv, &IID_ISVGElement_, &svgs);
        vt = svgs ? *(void***)svgs : NULL; o1 = (void*)1;
        if (vt) hr = ((getter)vt[10])(svgs, &o1);
        P("OWNTOP=%08lx %d\n", hr, o1 == NULL);
        if (svgs) IUnknown_Release((IUnknown*)svgs);
        IHTMLElement_Release(ci); IHTMLElement_Release(sv);
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
