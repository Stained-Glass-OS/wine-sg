/* ctrlrange-probe: body.createControlRange() and IHTMLControlRange (Stained Glass OS wine-sg 6047):
 * elements added, read back, removed; the common parent; select(); a command (Delete) on the range in
 * design mode; the same from script (JScript in the page) */
#define COBJMACROS
#include <windows.h>
#include <ole2.h>
#include <mshtml.h>
#include <stdio.h>
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

static IHTMLDocument2 *doc;
static IHTMLDocument3 *doc3;

static IHTMLElement *byid(const WCHAR *id)
{
    IHTMLElement *e = NULL; BSTR b = SysAllocString(id);
    IHTMLDocument3_getElementById(doc3, b, &e); SysFreeString(b);
    return e;
}

static void idof(IHTMLElement *e, const char *tag)
{
    BSTR id = NULL;
    if (e) IHTMLElement_get_id(e, &id);
    P("%s %ls\n", tag, e ? (id && *id ? id : L"(none)") : L"(null)");
    SysFreeString(id);
}

int main(void)
{
    IHTMLElement *body = NULL, *img, *tbl, *e = NULL;
    IHTMLTextContainer *tc = NULL;
    IHTMLControlRange *cr = NULL;
    IHTMLControlElement *ce;
    IDispatch *disp = NULL;
    VARIANT_BOOL vb;
    VARIANT v;
    LONG n;
    HRESULT hr;
    BSTR s;

    setvbuf(stdout, NULL, _IONBF, 0);
    CoInitialize(NULL);
    hr = CoCreateInstance(&CLSID_HTMLDocument_, NULL, CLSCTX_INPROC_SERVER, &IID_IHTMLDocument2, (void**)&doc);
    P("DOC %08lx\n", hr);
    {
        IPersistStreamInit *psi;
        IHTMLDocument2_QueryInterface(doc, &IID_IPersistStreamInit, (void**)&psi);
        IPersistStreamInit_InitNew(psi);
        IPersistStreamInit_Release(psi);
    }
    pump(1000);
    IHTMLDocument2_QueryInterface(doc, &IID_IHTMLDocument3, (void**)&doc3);
    s = SysAllocString(L"On"); hr = IHTMLDocument2_put_designMode(doc, s); SysFreeString(s); P("DESIGN %08lx\n", hr);
    pump(1500);
    IHTMLDocument2_get_body(doc, &body);
    s = SysAllocString(L"<div id=box><img id=i1 src=x.png width=20 height=20><p>text</p><table id=t1><tr><td>cell</td></tr></table></div><div id=other><span id=s1 contenteditable=false>x</span></div>");
    hr = IHTMLElement_put_innerHTML(body, s); SysFreeString(s); P("CONTENT %08lx\n", hr);
    img = byid(L"i1"); tbl = byid(L"t1");

    IHTMLElement_QueryInterface(body, &IID_IHTMLTextContainer, (void**)&tc);
    hr = IHTMLTextContainer_createControlRange(tc, &disp);
    P("CREATE %08lx\n", hr);
    if (disp) IDispatch_QueryInterface(disp, &IID_IHTMLControlRange, (void**)&cr);
    P("IFACE %d\n", cr != NULL);
    if (!cr) { P("DONE\n"); return 0; }
    n = -1; IHTMLControlRange_get_length(cr, &n); P("LEN0 %ld\n", n);
    IHTMLElement_QueryInterface(img, &IID_IHTMLControlElement, (void**)&ce);
    hr = IHTMLControlRange_add(cr, ce); P("ADD %08lx\n", hr);
    hr = IHTMLControlRange_add(cr, ce); IHTMLControlElement_Release(ce);
    n = -1; IHTMLControlRange_get_length(cr, &n); P("LEN1 %ld\n", n);
    IHTMLElement_QueryInterface(tbl, &IID_IHTMLControlElement, (void**)&ce);
    IHTMLControlRange_add(cr, ce); IHTMLControlElement_Release(ce);
    n = -1; IHTMLControlRange_get_length(cr, &n); P("LEN2 %ld\n", n);
    e = NULL; IHTMLControlRange_item(cr, 1, &e); idof(e, "ITEM1"); if (e) IHTMLElement_Release(e);
    e = NULL; hr = IHTMLControlRange_item(cr, 5, &e); P("ITEMBAD %08lx\n", hr);
    e = NULL; IHTMLControlRange_commonParentElement(cr, &e); idof(e, "PARENT"); if (e) IHTMLElement_Release(e);
    /* the image and an element outside its parent: the body holds both */
    {
        IHTMLControlRange *cr2 = NULL; IDispatch *d2 = NULL; IHTMLElement *s1 = byid(L"s1"), *bd = NULL;
        IHTMLTextContainer_createControlRange(tc, &d2);
        if (d2) IDispatch_QueryInterface(d2, &IID_IHTMLControlRange, (void**)&cr2);
        IHTMLElement_QueryInterface(img, &IID_IHTMLControlElement, (void**)&ce); IHTMLControlRange_add(cr2, ce); IHTMLControlElement_Release(ce);
        IHTMLElement_QueryInterface(s1, &IID_IHTMLControlElement, (void**)&ce); IHTMLControlRange_add(cr2, ce); IHTMLControlElement_Release(ce);
        IHTMLControlRange_commonParentElement(cr2, &bd);
        { BSTR tag = NULL; if (bd) IHTMLElement_get_tagName(bd, &tag); P("PARENT2 %ls\n", tag ? tag : L"(null)"); SysFreeString(tag); }
        if (bd) IHTMLElement_Release(bd);
        IHTMLControlRange_Release(cr2); IDispatch_Release(d2); IHTMLElement_Release(s1);
    }
    hr = IHTMLControlRange_remove(cr, 0); P("REMOVE %08lx\n", hr);
    n = -1; IHTMLControlRange_get_length(cr, &n); P("LEN3 %ld\n", n);
    e = NULL; IHTMLControlRange_item(cr, 0, &e); idof(e, "ITEM0"); if (e) IHTMLElement_Release(e);
    hr = IHTMLControlRange_select(cr); P("SELECT %08lx\n", hr);
    {
        IHTMLSelectionObject *sel = NULL; BSTR t = NULL; IDispatch *rd = NULL; IHTMLTxtRange *tr = NULL;
        IHTMLDocument2_get_selection(doc, &sel);
        if (sel) { IHTMLSelectionObject_get_type(sel, &t); IHTMLSelectionObject_createRange(sel, &rd); }
        P("SELTYPE %ls\n", t ? t : L"(null)"); SysFreeString(t); t = NULL;
        if (rd) { IDispatch_QueryInterface(rd, &IID_IHTMLTxtRange, (void**)&tr); if (tr) { IHTMLTxtRange_get_htmlText(tr, &t); IHTMLTxtRange_Release(tr); } IDispatch_Release(rd); }
        P("SELHTML %ls\n", t ? t : L"(null)"); SysFreeString(t);
        if (sel) IHTMLSelectionObject_Release(sel);
    }
    s = SysAllocString(L"Delete");
    vb = 9; hr = IHTMLControlRange_queryCommandSupported(cr, s, &vb); P("SUPP %08lx %d\n", hr, vb);
    VariantInit(&v);
    vb = 9; hr = IHTMLControlRange_execCommand(cr, s, VARIANT_FALSE, v, &vb); P("EXEC %08lx %d\n", hr, vb);
    SysFreeString(s);
    e = byid(L"t1"); P("TABLE_AFTER %d\n", e != NULL); if (e) IHTMLElement_Release(e);
    e = byid(L"i1"); P("IMG_AFTER %d\n", e != NULL); if (e) IHTMLElement_Release(e);
    IHTMLControlRange_Release(cr); IDispatch_Release(disp);

    /* from script */
    {
        IHTMLWindow2 *win = NULL; VARIANT res;
        BSTR code = SysAllocString(L"var st = 0; try { var r = document.body.createControlRange(); st = 1; r.add(document.getElementById('i1')); st = 2; "
                                   L"document.title = 'len=' + r.length + ' id=' + r.item(0).id; } catch(e) { document.title = 'error at ' + st + ': ' + e.message; }");
        BSTR lang = SysAllocString(L"JScript");
        BSTR title = NULL;
        IHTMLDocument2_get_parentWindow(doc, &win);
        VariantInit(&res);
        hr = IHTMLWindow2_execScript(win, code, lang, &res);
        IHTMLDocument2_get_title(doc, &title);
        P("SCRIPT %08lx %ls\n", hr, title ? title : L"(null)");
        SysFreeString(code); SysFreeString(lang); SysFreeString(title);
        IHTMLWindow2_Release(win);
    }
    P("DONE\n");
    return 0;
}
