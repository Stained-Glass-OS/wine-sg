#define COBJMACROS
#include <windows.h>
#include <ole2.h>
#include <mshtml.h>
#include <mshtmcid.h>
#include <docobj.h>
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
static const GUID CGID_MSHTML_ = {0xde4ba900,0x59ca,0x11cf,{0x95,0x92,0x44,0x45,0x53,0x54,0x00,0x00}};
#define CGID_MSHTML CGID_MSHTML_
#define P(...) printf(__VA_ARGS__)
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
L"<div id=d1>Hello <i>big</i> World</div><div id=d2>second line</div>"
L"</body></html>";

static IHTMLDocument2 *doc;
static IHTMLDocument3 *doc3;

static IHTMLElement *byid(const WCHAR *id)
{
    IHTMLElement *e = NULL; BSTR b = SysAllocString(id);
    IHTMLDocument3_getElementById(doc3, b, &e); SysFreeString(b);
    return e;
}

static void inner(const WCHAR *id, const char *tag)
{
    IHTMLElement *e = byid(id); BSTR b = NULL;
    if (e) IHTMLElement_get_innerHTML(e, &b);
    P("%s %ls\n", tag, b ? b : L"(null)");
    SysFreeString(b);
    if (e) IHTMLElement_Release(e);
}

static IHTMLTxtRange *range_of(const WCHAR *id, const WCHAR *find)
{
    IHTMLBodyElement *body = NULL; IHTMLElement *be = NULL; IHTMLTxtRange *rng = NULL; IHTMLElement *e = byid(id); BSTR f = SysAllocString(find); VARIANT_BOOL ok = 0;
    IHTMLDocument2_get_body(doc, &be); IHTMLElement_QueryInterface(be, &IID_IHTMLBodyElement, (void**)&body);
    IHTMLBodyElement_createTextRange(body, &rng);
    IHTMLTxtRange_moveToElementText(rng, e);
    if (find) IHTMLTxtRange_findText(rng, f, 1000000, 0, &ok);
    SysFreeString(f); IHTMLElement_Release(e); IHTMLBodyElement_Release(body); IHTMLElement_Release(be);
    return rng;
}

static void rtext(IHTMLTxtRange *r, const char *tag)
{
    BSTR t = NULL; IHTMLTxtRange_get_text(r, &t); P("%s [%ls]\n", tag, t ? t : L"(null)"); SysFreeString(t);
}

int main(void)
{
    HRESULT hr; VARIANT_BOOL vb, vb2; VARIANT v;
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
    { BSTR m = SysAllocString(L"On"); hr = IHTMLDocument2_put_designMode(doc, m); SysFreeString(m); P("DESIGN %08lx\n", hr); }
    pump(1500);
    /* the design mode starts the page again from an empty one: the content goes in afterwards */
    {
        IHTMLElement *be = NULL; BSTR h = SysAllocString(L"<div id=d1>Hello <i>big</i> World</div><div id=d2>second line</div>");
        IHTMLDocument2_get_body(doc, &be); hr = IHTMLElement_put_innerHTML(be, h); P("CONTENT %08lx\n", hr);
        SysFreeString(h); IHTMLElement_Release(be);
    }

    {
        IHTMLTxtRange *world = range_of(L"d1", L"World"), *hello = range_of(L"d1", L"Hello");
        IHTMLTxtRange *sel_range = range_of(L"d2", L"second"); IHTMLSelectionObject *sel = NULL; IDispatch *cur = NULL; IHTMLTxtRange *cr = NULL;
        BSTR c;
        rtext(world, "WORLD"); rtext(hello, "HELLO");
        IHTMLTxtRange_select(sel_range);

        c = SysAllocString(L"bold");
        vb = 9; hr = IHTMLTxtRange_queryCommandSupported(world, c, &vb); P("SUPP %08lx %d\n", hr, vb);
        vb = 9; hr = IHTMLTxtRange_queryCommandEnabled(world, c, &vb); P("ENAB %08lx %d\n", hr, vb);
        vb = 9; vb2 = 9; IHTMLTxtRange_queryCommandState(world, c, &vb); IHTMLTxtRange_queryCommandState(hello, c, &vb2); P("STATE0 %d %d\n", vb, vb2);
        VariantInit(&v);
        vb = 9; hr = IHTMLTxtRange_execCommand(world, c, VARIANT_FALSE, v, &vb); P("EXEC %08lx %d\n", hr, vb);
        inner(L"d1", "D1A");
        vb = 9; vb2 = 9; IHTMLTxtRange_queryCommandState(world, c, &vb); IHTMLTxtRange_queryCommandState(hello, c, &vb2); P("STATE1 %d %d\n", vb, vb2);
        rtext(world, "WORLD2");
        /* the selection is as it was */
        IHTMLDocument2_get_selection(doc, &sel); IHTMLSelectionObject_createRange(sel, &cur); IDispatch_QueryInterface(cur, &IID_IHTMLTxtRange, (void**)&cr);
        rtext(cr, "SEL");
        IHTMLTxtRange_Release(cr); IDispatch_Release(cur); IHTMLSelectionObject_Release(sel);
        SysFreeString(c);
        c = SysAllocString(L"nosuchcommand");
        vb = 9; hr = IHTMLTxtRange_queryCommandSupported(world, c, &vb); P("SUPPNO %08lx %d\n", hr, vb);
        vb = 9; hr = IHTMLTxtRange_execCommand(world, c, VARIANT_FALSE, v, &vb); P("EXECNO %08lx\n", hr);
        SysFreeString(c);
        /* a second command, and the command target of the range */
        c = SysAllocString(L"italic");
        vb = 9; hr = IHTMLTxtRange_execCommand(hello, c, VARIANT_FALSE, v, &vb); P("EXEC2 %08lx %d\n", hr, vb); SysFreeString(c);
        inner(L"d1", "D1B");
        {
            IOleCommandTarget *ct = NULL; OLECMD cmd; cmd.cmdID = IDM_UNDERLINE; cmd.cmdf = 0;
            IHTMLTxtRange_QueryInterface(hello, &IID_IOleCommandTarget, (void**)&ct);
            hr = IOleCommandTarget_QueryStatus(ct, &CGID_MSHTML, 1, &cmd, NULL); P("QS %08lx %x\n", hr, cmd.cmdf);
            hr = IOleCommandTarget_Exec(ct, &CGID_MSHTML, IDM_UNDERLINE, 0, NULL, NULL); P("CTEXEC %08lx\n", hr);
            IOleCommandTarget_Release(ct);
        }
        inner(L"d1", "D1C");
        c = SysAllocString(L"fontname"); VariantInit(&v); V_VT(&v) = VT_BSTR; V_BSTR(&v) = SysAllocString(L"Arial");
        hr = IHTMLTxtRange_execCommand(world, c, VARIANT_FALSE, v, &vb); P("FONT %08lx %d\n", hr, vb); VariantClear(&v);
        VariantInit(&v); hr = IHTMLTxtRange_queryCommandValue(world, c, &v); P("FONTVAL %08lx %d %ls\n", hr, V_VT(&v), V_VT(&v) == VT_BSTR && V_BSTR(&v) ? V_BSTR(&v) : L"-"); VariantClear(&v);
        SysFreeString(c);
        c = SysAllocString(L"bold"); VariantInit(&v); hr = IHTMLTxtRange_queryCommandValue(world, c, &v); P("BOLDVAL %08lx %d %d\n", hr, V_VT(&v), V_VT(&v) == VT_BOOL ? V_BOOL(&v) : 0); SysFreeString(c);
        IHTMLTxtRange_Release(world); IHTMLTxtRange_Release(hello); IHTMLTxtRange_Release(sel_range);
    }
    P("DONE\n");
    return 0;
}
