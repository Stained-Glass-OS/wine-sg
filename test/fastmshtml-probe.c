#define COBJMACROS
#include <windows.h>
#include <ole2.h>
#include <mshtml.h>
#include <stdio.h>
#define P(...) printf(__VA_ARGS__)
static const CLSID CLSID_HTMLDocument_ = {0x25336920,0x03f9,0x11cf,{0x8f,0xd0,0x00,0xaa,0x00,0x68,0x6f,0x13}};

static BSTR bs(const WCHAR *s) { return SysAllocString(s); }
static const WCHAR *tagname(IUnknown *u)
{
    static WCHAR buf[32];
    IHTMLElement *e; BSTR t = NULL;
    buf[0] = 0;
    if (u && SUCCEEDED(IUnknown_QueryInterface(u, &IID_IHTMLElement, (void**)&e))) {
        IHTMLElement_get_tagName(e, &t);
        if (t) lstrcpynW(buf, t, 32);
        SysFreeString(t); IHTMLElement_Release(e);
    }
    return buf;
}
static LONG rows(IHTMLTableSection *s)
{
    IHTMLElementCollection *c = NULL; LONG n = -1;
    if (SUCCEEDED(IHTMLTableSection_get_rows(s, &c)) && c) { IHTMLElementCollection_get_length(c, &n); IHTMLElementCollection_Release(c); }
    return n;
}
static WCHAR *rtext(IHTMLTxtRange *r)
{
    static WCHAR buf[128]; BSTR t = NULL;
    buf[0] = 0;
    IHTMLTxtRange_get_text(r, &t);
    if (t) lstrcpynW(buf, t, 128);
    SysFreeString(t);
    return buf;
}
static int find(IHTMLTxtRange *r, const WCHAR *s, LONG count, LONG flags)
{
    VARIANT_BOOL b = 0; BSTR str = bs(s);
    HRESULT hr = IHTMLTxtRange_findText(r, str, count, flags, &b);
    SysFreeString(str);
    return FAILED(hr) ? -1 : b != 0;
}
static void report(const char *name, IHTMLTxtRange *r, const WCHAR *text, LONG count, LONG flags)
{
    int ok = find(r, text, count, flags);
    P("%s %d %ls\n", name, ok, rtext(r));
}
static void collapse(IHTMLTxtRange *r, BOOL start) { IHTMLTxtRange_collapse(r, start ? VARIANT_TRUE : VARIANT_FALSE); }

int main(void)
{
    IHTMLDocument2 *doc; IHTMLDocument3 *doc3; IHTMLElement *body, *el; IHTMLTable *table; HRESULT hr;
    IHTMLTableSection *sec, *sec2; IDispatch *d1, *d2; IHTMLElementCollection *col;
    IHTMLTableCaption *cap; BSTR b; VARIANT v, nil; IHTMLTxtRange *r; IHTMLBodyElement *be;
    CoInitialize(NULL);
    hr = CoCreateInstance(&CLSID_HTMLDocument_,NULL,CLSCTX_INPROC_SERVER,&IID_IHTMLDocument2,(void**)&doc);
    P("DOC %08lx\n", hr);
    IHTMLDocument2_get_body(doc, &body);
    b = bs(L"<table id=t><tbody><tr><td>a</td></tr><tr><td>b</td></tr></tbody></table>");
    IHTMLElement_put_innerHTML(body, b); SysFreeString(b);
    IHTMLDocument2_QueryInterface(doc, &IID_IHTMLDocument3, (void**)&doc3);
    b = bs(L"t"); IHTMLDocument3_getElementById(doc3, b, &el); SysFreeString(b);
    hr = IHTMLElement_QueryInterface(el, &IID_IHTMLTable, (void**)&table); P("TABLE %08lx\n", hr);

    /* the body of the table */
    IHTMLTable_get_tBodies(table, &col);
    V_VT(&v) = VT_I4; V_I4(&v) = 0; V_VT(&nil) = VT_EMPTY;
    IHTMLElementCollection_item(col, v, nil, &d1); IHTMLElementCollection_Release(col);
    hr = IDispatch_QueryInterface(d1, &IID_IHTMLTableSection, (void**)&sec);
    P("TBODY %08lx %ls rows %ld\n", hr, tagname((IUnknown*)d1), rows(sec));
    hr = IHTMLTableSection_insertRow(sec, 1, &d2);
    P("INSERT %08lx %ls rows %ld\n", hr, tagname((IUnknown*)d2), rows(sec)); IDispatch_Release(d2);
    hr = IHTMLTableSection_insertRow(sec, 9, &d2); P("INSERT_BAD %08lx\n", hr);
    hr = IHTMLTableSection_deleteRow(sec, -1); P("DELETE %08lx rows %ld\n", hr, rows(sec));
    hr = IHTMLTableSection_deleteRow(sec, 7); P("DELETE_BAD %08lx\n", hr);
    b = bs(L"center"); IHTMLTableSection_put_align(sec, b); SysFreeString(b);
    IHTMLTableSection_get_align(sec, &b); P("ALIGN %ls\n", b); SysFreeString(b);
    IHTMLTableSection_Release(sec); IDispatch_Release(d1);

    /* head and foot */
    sec = (void*)1; hr = IHTMLTable_get_tHead(table, &sec); P("HEAD0 %08lx %d\n", hr, sec != NULL);
    hr = IHTMLTable_createTHead(table, &d1); P("HEAD_CREATE %08lx %ls\n", hr, tagname((IUnknown*)d1));
    hr = IHTMLTable_get_tHead(table, &sec); P("HEAD1 %08lx %d\n", hr, sec != NULL);
    IHTMLTableSection_Release(sec); IDispatch_Release(d1);
    hr = IHTMLTable_deleteTHead(table);
    sec = (void*)1; IHTMLTable_get_tHead(table, &sec); P("HEAD2 %08lx %d\n", hr, sec != NULL);
    hr = IHTMLTable_createTFoot(table, &d1); P("FOOT_CREATE %08lx %ls\n", hr, tagname((IUnknown*)d1)); IDispatch_Release(d1);
    hr = IHTMLTable_get_tFoot(table, &sec2); P("FOOT1 %08lx %d\n", hr, sec2 != NULL); IHTMLTableSection_Release(sec2);
    hr = IHTMLTable_deleteTFoot(table);
    sec2 = (void*)1; IHTMLTable_get_tFoot(table, &sec2); P("FOOT2 %08lx %d\n", hr, sec2 != NULL);

    /* the caption */
    hr = IHTMLTable_createCaption(table, &cap); P("CAPTION %08lx %ls\n", hr, tagname((IUnknown*)cap));
    b = bs(L"bottom"); IHTMLTableCaption_put_vAlign(cap, b); SysFreeString(b);
    IHTMLTableCaption_get_vAlign(cap, &b); P("CAPVALIGN %ls\n", b); SysFreeString(b);
    IHTMLTableCaption_Release(cap);
    hr = IHTMLTable_deleteCaption(table);
    cap = (void*)1; IHTMLTable_get_caption(table, &cap); P("CAPTION2 %08lx %d\n", hr, cap != NULL);
    IHTMLTable_Release(table); IHTMLElement_Release(el);

    /* findText */
    b = bs(L"Abc def <b>abc</b> xyz<br />abc2 Abc"); IHTMLElement_put_innerHTML(body, b); SysFreeString(b);
    IHTMLElement_QueryInterface(body, &IID_IHTMLBodyElement, (void**)&be);
    IHTMLBodyElement_createTextRange(be, &r);
    P("TEXT %ls\n", rtext(r));
    report("F1", r, L"abc", 1000000, 0);
    collapse(r, FALSE); report("F2", r, L"abc", 1000000, 0);
    collapse(r, FALSE); report("F3", r, L"abc", 1000000, 0);
    collapse(r, FALSE); report("F4", r, L"abc", 1000000, 0);
    collapse(r, FALSE); { int ok = find(r, L"abc", 1000000, 0); P("F5 %d [%ls]\n", ok, rtext(r)); }
    IHTMLTxtRange_Release(r);
    IHTMLBodyElement_createTextRange(be, &r);
    { int a = find(r, L"abc", 1000000, 4); WCHAR t[64]; lstrcpyW(t, rtext(r)); P("CASE %d %ls | %d\n", a, t, find(r, L"ABC", 1000000, 4)); }
    IHTMLTxtRange_Release(r);
    IHTMLBodyElement_createTextRange(be, &r);
    { int a = find(r, L"abc", 1000000, 2); P("WORD %d %ls", a, rtext(r)); }
    collapse(r, FALSE); { int a = find(r, L"abc", 1000000, 2); P(" %d %ls", a, rtext(r)); }
    collapse(r, FALSE); { int a = find(r, L"abc", 1000000, 2); P(" %d %ls\n", a, rtext(r)); }
    IHTMLTxtRange_Release(r);
    IHTMLBodyElement_createTextRange(be, &r);
    collapse(r, TRUE);
    { int a = find(r, L"xyz", 14, 0); P("COUNT %d", a); }
    { int a = find(r, L"xyz", 15, 0); P(" %d %ls\n", a, rtext(r)); }
    IHTMLTxtRange_Release(r);
    IHTMLBodyElement_createTextRange(be, &r);
    collapse(r, FALSE);
    { int a = find(r, L"abc", -1000000, 0); P("BACK %d %ls", a, rtext(r)); }
    collapse(r, TRUE); { int a = find(r, L"abc", -1000000, 0); P(" %d %ls\n", a, rtext(r)); }
    IHTMLTxtRange_Release(r);
    P("DONE\n");
    return 0;
}
