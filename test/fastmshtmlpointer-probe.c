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
static const IID IID_IMarkupPointer2_ = {0x3050f675,0x98b5,0x11cf,{0xbb,0x82,0x00,0xaa,0x00,0xbd,0xce,0x0b}};
#define IID_IMarkupPointer2 IID_IMarkupPointer2_
static const IID IID_IMarkupServices_ = {0x3050f4a0,0x98b5,0x11cf,{0xbb,0x82,0x00,0xaa,0x00,0xbd,0xce,0x0b}};
#define IID_IMarkupServices IID_IMarkupServices_
#define P(...) printf(__VA_ARGS__)
#define MOVEUNIT_NEXTURLBEG MOVEUNIT_NEXTURLBEGIN
#define MOVEUNIT_PREVURLBEG MOVEUNIT_PREVURLBEGIN
#define MOVEUNIT_NEXTWORDBEG MOVEUNIT_NEXTWORDBEGIN
#define MOVEUNIT_PREVWORDBEG MOVEUNIT_PREVWORDBEGIN
static const CLSID CLSID_HTMLDocument_ = {0x25336920,0x03f9,0x11cf,{0x8f,0xd0,0x00,0xaa,0x00,0x68,0x6f,0x13}};



static void pump(DWORD ms)
{
    DWORD end = GetTickCount() + ms;
    MSG msg;
    while (GetTickCount() < end) {
        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
        Sleep(10);
    }
}




static const WCHAR page[] =
L"<!DOCTYPE html>\n<html><head><meta http-equiv=\"x-ua-compatible\" content=\"IE=11\"></head><body>"
L"<div id=u>go to http://a.example/x?y=1, then www.b.org now. Done! Or not? Last one</div>"
L"</body></html>";

static IHTMLDocument2 *doc;
static IHTMLDocument3 *doc3;
static IMarkupServices *ms;

static IHTMLElement *byid(const WCHAR *id)
{
    IHTMLElement *e = NULL; BSTR b = SysAllocString(id);
    IHTMLDocument3_getElementById(doc3, b, &e); SysFreeString(b);
    return e;
}

static IMarkupPointer *mkptr(const WCHAR *id, ELEMENT_ADJACENCY adj)
{
    IMarkupPointer *p = NULL; IHTMLElement *e = byid(id);
    IMarkupServices_CreateMarkupPointer(ms, &p);
    if (e) { IMarkupPointer_MoveAdjacentToElement(p, e, adj); IHTMLElement_Release(e); }
    return p;
}

static void rt(IMarkupPointer *p, const char *tag, HRESULT hr)
{
    MARKUP_CONTEXT_TYPE ctx = 9; LONG cch = 13; OLECHAR buf[14] = {0};
    IMarkupPointer_Right(p, FALSE, &ctx, NULL, &cch, buf);
    if (ctx != CONTEXT_TYPE_Text) cch = 0;
    buf[cch] = 0;
    P("%s %08lx [%ls]\n", tag, hr, buf);
}

#define MU(p, act, tag) do { IMarkupPointer2 *p2_; HRESULT h_; IMarkupPointer_QueryInterface(p, &IID_IMarkupPointer2, (void**)&p2_); \
    h_ = IMarkupPointer2_MoveUnit(p2_, act); IMarkupPointer2_Release(p2_); rt(p, tag, h_); } while (0)

int main(void)
{
    HRESULT hr;
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
    hr = IHTMLDocument2_QueryInterface(doc, &IID_IMarkupServices, (void**)&ms);
    P("MS %08lx\n", hr);

    {
        IMarkupPointer *p = mkptr(L"u", ELEM_ADJ_AfterBegin), *q = mkptr(L"u", ELEM_ADJ_AfterBegin);
        IMarkupPointer2 *p2; BOOL in; HRESULT h; int i;
        MU(p, MOVEUNIT_NEXTURLBEG, "UBEG1");
        MU(p, MOVEUNIT_NEXTURLEND, "UEND1");
        MU(p, MOVEUNIT_NEXTURLBEG, "UBEG2");
        MU(p, MOVEUNIT_NEXTURLEND, "UEND2");
        MU(p, MOVEUNIT_PREVURLBEG, "UPBEG2");
        MU(p, MOVEUNIT_PREVURLEND, "UPEND1");
        MU(p, MOVEUNIT_PREVURLBEG, "UPBEG1");
        MU(p, MOVEUNIT_PREVURLBEG, "UPBEG0");
        IMarkupPointer_Release(p); IMarkupPointer_Release(q);
        p = mkptr(L"u", ELEM_ADJ_AfterBegin); q = mkptr(L"u", ELEM_ADJ_AfterBegin);
        IMarkupPointer_QueryInterface(p, &IID_IMarkupPointer2, (void**)&p2);
        in = 9; h = IMarkupPointer2_IsInsideURL(p2, q, &in); P("INURL0 %08lx %d\n", h, in);
        IMarkupPointer2_MoveUnit(p2, MOVEUNIT_NEXTURLBEG);
        IMarkupPointer_MoveToPointer(q, p);
        for (i = 0; i < 10; i++) IMarkupPointer_MoveUnit(q, MOVEUNIT_NEXTCHAR);
        in = 9; h = IMarkupPointer2_IsInsideURL(p2, q, &in); P("INURL1 %08lx %d\n", h, in);
        in = 9; h = IMarkupPointer2_IsInsideURL(p2, p, &in); P("INURL2 %08lx %d\n", h, in);
        for (i = 0; i < 40; i++) IMarkupPointer_MoveUnit(q, MOVEUNIT_NEXTCHAR);
        in = 9; h = IMarkupPointer2_IsInsideURL(p2, q, &in); P("INURL3 %08lx %d\n", h, in);
        IMarkupPointer2_Release(p2); IMarkupPointer_Release(p); IMarkupPointer_Release(q);
    }
    {
        IMarkupPointer *p = mkptr(L"u", ELEM_ADJ_AfterBegin);
        MU(p, MOVEUNIT_NEXTWORDEND, "WE1");
        MU(p, MOVEUNIT_NEXTWORDEND, "WE2");
        MU(p, MOVEUNIT_PREVWORDEND, "PWE1");
        MU(p, MOVEUNIT_NEXTWORDBEG, "WB1");
        MU(p, MOVEUNIT_NEXTSENTENCE, "S1");
        MU(p, MOVEUNIT_NEXTSENTENCE, "S2");
        MU(p, MOVEUNIT_PREVSENTENCE, "PS1");
        MU(p, MOVEUNIT_PREVSENTENCE, "PS0");
        IMarkupPointer_Release(p);
    }
    P("DONE\n");
    return 0;
}
