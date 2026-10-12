/* msxml3 batch (patches/sg/3000), run by test/fastmsxml-gate.sh. Families: XPath selections
 * (expr, context, peekNode, matches, removeNext, removeAll, clone), namespace managers
 * (declared prefixes, prefixes of a uri, node contexts, getURIFromNode, reset), parse errors
 * (code, line, column, reason, source text), importNode, nodeFromID, transformNodeToObject,
 * SAX attributes (setAttribute, setAttributes, addAttributeFromIndex, types), reader base
 * URLs and ServerXMLHTTP options. Everything goes through IDispatch, as a script would.
 *
 *   fastmsxml-probe.exe */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <ocidl.h>
#include <msxml6.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include <oleauto.h>
#include <stdlib.h>

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}
#define CHECK(e) check((e), #e)

static VARIANT vbstr(const WCHAR *s) { VARIANT v; V_VT(&v) = VT_BSTR; V_BSTR(&v) = SysAllocString(s); return v; }
static VARIANT vi4(LONG l) { VARIANT v; V_VT(&v) = VT_I4; V_I4(&v) = l; return v; }
static VARIANT vbool(int b) { VARIANT v; V_VT(&v) = VT_BOOL; V_BOOL(&v) = b ? VARIANT_TRUE : VARIANT_FALSE; return v; }
static VARIANT vdisp(IUnknown *u) { VARIANT v; V_VT(&v) = VT_DISPATCH; V_DISPATCH(&v) = (IDispatch *)u; return v; }

/* invoke by name; args are given in call order */
static int raw_dispatch;

static HRESULT dcall(IUnknown *obj, const WCHAR *name, WORD flags, VARIANT *res, int n, ...)
{
    IDispatch *d;
    DISPPARAMS dp;
    VARIANT args[8];
    DISPID id, put = DISPID_PROPERTYPUT;
    WCHAR *names[1];
    HRESULT hr;
    va_list ap;
    int i;

    if (res) VariantInit(res);
    if (raw_dispatch) { d = (IDispatch *)obj; IDispatch_AddRef(d); }
    else if (FAILED(IUnknown_QueryInterface(obj, &IID_IDispatch, (void **)&d))) return E_NOINTERFACE;
    names[0] = (WCHAR *)name;
    if (FAILED(hr = IDispatch_GetIDsOfNames(d, &IID_NULL, names, 1, 0, &id))) { IDispatch_Release(d); return hr; }

    va_start(ap, n);
    for (i = 0; i < n; i++) args[n - 1 - i] = va_arg(ap, VARIANT);
    va_end(ap);
    dp.rgvarg = args;
    dp.cArgs = n;
    dp.cNamedArgs = (flags & (DISPATCH_PROPERTYPUT | DISPATCH_PROPERTYPUTREF)) ? 1 : 0;
    dp.rgdispidNamedArgs = dp.cNamedArgs ? &put : NULL;
    hr = IDispatch_Invoke(d, id, &IID_NULL, 0, flags, &dp, res, NULL, NULL);
    IDispatch_Release(d);
    return hr;
}
#define GET(obj, name, res, n, ...) dcall((IUnknown *)(obj), name, DISPATCH_METHOD | DISPATCH_PROPERTYGET, res, n, ##__VA_ARGS__)
#define PUT(obj, name, val) dcall((IUnknown *)(obj), name, DISPATCH_PROPERTYPUT, NULL, 1, val)

static LONG get_long(IUnknown *obj, const WCHAR *name, int n, ...)
{
    VARIANT r;
    LONG ret = -12345;
    HRESULT hr;
    if (n == 0 && SUCCEEDED(hr = GET(obj, name, &r, 0)) && SUCCEEDED(hr = VariantChangeType(&r, &r, 0, VT_I4))) ret = V_I4(&r);
    else fprintf(stderr, "get_long(%ls): hr %08lx vt %d\n", name, (unsigned long)hr, V_VT(&r));
    if (getenv("PROBE_DEBUG")) fprintf(stderr, "  %ls = %ld\n", name, ret);
    return ret;
}
static int streq(IUnknown *obj, const WCHAR *name, const WCHAR *expect)
{
    VARIANT r;
    int ok = 0;
    if (SUCCEEDED(GET(obj, name, &r, 0)) && V_VT(&r) == VT_BSTR && V_BSTR(&r)) ok = !wcscmp(V_BSTR(&r), expect);
    else if (SUCCEEDED(GET(obj, name, &r, 0)) && V_VT(&r) == VT_BSTR && !expect[0]) ok = 1;
    if (!ok && getenv("PROBE_DEBUG")) fprintf(stderr, "  streq(%ls): got vt %d '%ls', want '%ls'\n", name, V_VT(&r), V_VT(&r) == VT_BSTR && V_BSTR(&r) ? V_BSTR(&r) : L"(none)", expect);
    VariantClear(&r);
    return ok;
}
/* the xml of a node ignoring the white space the serializer adds */
static int xml_is(IUnknown *obj, const WCHAR *expect)
{
    VARIANT r;
    WCHAR got[512];
    int i, n = 0, ok = 0;

    if (SUCCEEDED(GET(obj, L"xml", &r, 0)) && V_VT(&r) == VT_BSTR && V_BSTR(&r))
    {
        for (i = 0; V_BSTR(&r)[i] && n < 511; i++)
            if (V_BSTR(&r)[i] != '\r' && V_BSTR(&r)[i] != '\n' && V_BSTR(&r)[i] != '\t') got[n++] = V_BSTR(&r)[i];
        got[n] = 0;
        ok = !wcscmp(got, expect);
        if (!ok && getenv("PROBE_DEBUG")) fprintf(stderr, "  xml: got '%ls' want '%ls'\n", got, expect);
    }
    VariantClear(&r);
    return ok;
}

static IUnknown *get_obj(IUnknown *obj, const WCHAR *name, int n, ...)
{
    VARIANT r;
    IUnknown *ret = NULL;
    HRESULT hr = GET(obj, name, &r, 0);
    if (n) return NULL;
    if (SUCCEEDED(hr) && (V_VT(&r) == VT_DISPATCH || V_VT(&r) == VT_UNKNOWN)) ret = V_UNKNOWN(&r);
    return ret;
}

static IUnknown *create(const WCHAR *progid)
{
    CLSID clsid;
    IUnknown *obj = NULL;
    if (FAILED(CLSIDFromProgID(progid, &clsid))) return NULL;
    if (FAILED(CoCreateInstance(&clsid, NULL, CLSCTX_INPROC_SERVER, &IID_IUnknown, (void **)&obj))) return NULL;
    return obj;
}

static IUnknown *newdoc(const WCHAR *xml)
{
    IUnknown *d = create(L"Msxml2.DOMDocument.6.0");
    VARIANT r;
    if (!d) return NULL;
    PUT(d, L"async", vbool(0));
    dcall(d, L"setProperty", DISPATCH_METHOD, NULL, 2, vbstr(L"ProhibitDTD"), vbool(0));
    GET(d, L"loadXML", &r, 1, vbstr(xml));
    return d;
}

#define OBJ(o, n) get_obj((IUnknown *)(o), n, 0)

static void test_selection(void)
{
    IUnknown *d = newdoc(L"<r><a id='1'>x</a><a id='2'>y</a><b/></r>");
    IUnknown *sel, *pk, *cl, *m, *ctx, *docel, *removed, *sel2;
    VARIANT r, args;
    HRESULT hr;

    hr = GET(d, L"selectNodes", &r, 1, vbstr(L"/r/a"));
    sel = SUCCEEDED(hr) ? V_UNKNOWN(&r) : NULL;
    check(sel != NULL, "selectNodes");
    if (!sel) return;

    CHECK(get_long(sel, L"length", 0) == 2);
    CHECK(streq(sel, L"expr", L"/r/a"));
    pk = get_obj(sel, L"peekNode", 0);
    check(pk != NULL, "peekNode returns the next node");
    if (pk) { GET(pk, L"getAttribute", &r, 1, vbstr(L"id")); CHECK(V_VT(&r) == VT_BSTR && !wcscmp(V_BSTR(&r), L"1")); }
    pk = get_obj(sel, L"peekNode", 0);
    if (pk) { GET(pk, L"getAttribute", &r, 1, vbstr(L"id")); CHECK(V_VT(&r) == VT_BSTR && !wcscmp(V_BSTR(&r), L"1")); }

    docel = OBJ(d, L"documentElement");
    {
        VARIANT first;
        GET(docel, L"firstChild", &first, 0);
        hr = GET(sel, L"matches", &r, 1, first);
        check(SUCCEEDED(hr) && V_VT(&r) == VT_DISPATCH && V_DISPATCH(&r) != NULL, "matches finds a member");
    }

    hr = GET(sel, L"clone", &r, 0);
    cl = SUCCEEDED(hr) ? V_UNKNOWN(&r) : NULL;
    check(cl != NULL, "clone");
    if (cl) { CHECK(get_long(cl, L"length", 0) == 2); CHECK(streq(cl, L"expr", L"/r/a")); }

    hr = PUT(sel, L"expr", vbstr(L"/r/b"));
    check(SUCCEEDED(hr), "put expr");
    CHECK(get_long(sel, L"length", 0) == 1);
    if (cl) CHECK(get_long(cl, L"length", 0) == 2);
    ctx = get_obj(sel, L"context", 0);
    check(ctx != NULL && streq(ctx, L"nodeName", L"#document"), "context is the document");

    hr = GET(sel, L"getProperty", &r, 1, vbstr(L"SelectionLanguage"));
    check(SUCCEEDED(hr) && V_VT(&r) == VT_BSTR, "getProperty SelectionLanguage");

    GET(d, L"selectNodes", &r, 1, vbstr(L"/r/a"));
    sel2 = V_UNKNOWN(&r);
    hr = GET(sel2, L"removeNext", &r, 0);
    removed = SUCCEEDED(hr) ? V_UNKNOWN(&r) : NULL;
    check(removed != NULL && streq(removed, L"nodeName", L"a"), "removeNext returns the node");
    GET(d, L"selectNodes", &r, 1, vbstr(L"/r/a"));
    CHECK(get_long(V_UNKNOWN(&r), L"length", 0) == 1);
    hr = GET(sel2, L"removeAll", &r, 0);
    check(SUCCEEDED(hr), "removeAll");
    GET(d, L"selectNodes", &r, 1, vbstr(L"/r/a"));
    CHECK(get_long(V_UNKNOWN(&r), L"length", 0) == 0);
    CHECK(get_long(sel2, L"length", 0) == 0);
    (void)args;
}

static void test_namespaces(void)
{
    IUnknown *nm = create(L"Msxml2.MXNamespaceManager.6.0"), *pf, *d2, *docel, *child;
    VARIANT r;
    HRESULT hr;

    if (!nm) { check(0, "namespace manager"); return; }
    dcall(nm, L"declarePrefix", DISPATCH_METHOD, NULL, 2, vbstr(L"a"), vbstr(L"urn:a"));
    dcall(nm, L"declarePrefix", DISPATCH_METHOD, NULL, 2, vbstr(L"b"), vbstr(L"urn:a"));
    pf = SUCCEEDED(GET(nm, L"getDeclaredPrefixes", &r, 0)) ? V_UNKNOWN(&r) : NULL;
    check(pf != NULL, "getDeclaredPrefixes");
    if (pf)
    {
        CHECK(get_long(pf, L"length", 0) == 3);
        hr = GET(pf, L"item", &r, 1, vi4(0));
        check(SUCCEEDED(hr) && V_VT(&r) == VT_BSTR && !wcscmp(V_BSTR(&r), L"xml"), "first prefix is xml");
    }
    pf = SUCCEEDED(GET(nm, L"getPrefixes", &r, 1, vbstr(L"urn:a"))) ? V_UNKNOWN(&r) : NULL;
    check(pf != NULL && get_long(pf, L"length", 0) == 2, "two prefixes for one uri");
    hr = GET(nm, L"getURI", &r, 1, vbstr(L"a"));
    check(SUCCEEDED(hr) && V_VT(&r) == VT_BSTR && !wcscmp(V_BSTR(&r), L"urn:a"), "getURI");

    d2 = newdoc(L"<r xmlns:q='urn:q'><c xmlns='urn:d'/></r>");
    docel = OBJ(d2, L"documentElement");
    hr = dcall(nm, L"pushNodeContext", DISPATCH_METHOD, NULL, 2, vdisp(docel), vbool(1));
    check(SUCCEEDED(hr), "pushNodeContext");
    hr = GET(nm, L"getURI", &r, 1, vbstr(L"q"));
    check(SUCCEEDED(hr) && V_VT(&r) == VT_BSTR && !wcscmp(V_BSTR(&r), L"urn:q"), "node context declares the node's namespaces");
    child = OBJ(docel, L"firstChild");
    hr = GET(nm, L"getURIFromNode", &r, 2, vbstr(L""), vdisp(child));
    check(SUCCEEDED(hr) && V_VT(&r) == VT_BSTR && !wcscmp(V_BSTR(&r), L"urn:d"), "getURIFromNode default namespace");
    hr = GET(nm, L"getURIFromNode", &r, 2, vbstr(L"q"), vdisp(child));
    check(SUCCEEDED(hr) && V_VT(&r) == VT_BSTR && !wcscmp(V_BSTR(&r), L"urn:q"), "getURIFromNode inherited prefix");
    hr = GET(nm, L"reset", &r, 0);
    check(SUCCEEDED(hr), "reset");
    pf = SUCCEEDED(GET(nm, L"getDeclaredPrefixes", &r, 0)) ? V_UNKNOWN(&r) : NULL;
    check(pf != NULL && get_long(pf, L"length", 0) == 1, "reset leaves only xml");
}

static void test_parse_errors(void)
{
    IUnknown *bad = newdoc(L"<a>\r\n<b></a>"), *pe, *good, *empty;
    VARIANT r;

    pe = bad ? OBJ(bad, L"parseError") : NULL;
    check(pe != NULL, "parseError");
    if (pe)
    {
        CHECK(get_long(pe, L"errorCode", 0) == (LONG)0xC00CE56D);
        CHECK(get_long(pe, L"line", 0) == 2);
        CHECK(get_long(pe, L"linepos", 0) > 0);
        CHECK(streq(pe, L"srcText", L"<b></a>"));
        CHECK(get_long(pe, L"filepos", 0) > 0);
        GET(pe, L"reason", &r, 0);
        check(V_VT(&r) == VT_BSTR && wcsstr(V_BSTR(&r), L"does not match the start tag"), "reason names the tags");
    }
    empty = newdoc(L"no xml content here");
    pe = empty ? OBJ(empty, L"parseError") : NULL;
    CHECK(pe && get_long(pe, L"errorCode", 0) == (LONG)0xC00CE556);
    good = newdoc(L"<a/>");
    pe = good ? OBJ(good, L"parseError") : NULL;
    CHECK(pe && get_long(pe, L"errorCode", 0) == 0);
}

static void test_import_nodefromid(void)
{
    IUnknown *src = newdoc(L"<s><t k='v'>text</t></s>"), *dst = newdoc(L"<r/>"), *t, *imp, *idd, *byid;
    VARIANT r;
    HRESULT hr;

    t = OBJ(OBJ(src, L"documentElement"), L"firstChild");
    hr = GET(dst, L"importNode", &r, 2, vdisp(t), vbool(1));
    imp = SUCCEEDED(hr) ? V_UNKNOWN(&r) : NULL;
    check(imp != NULL && xml_is(imp, L"<t k=\"v\">text</t>"), "importNode deep copy");
    if (imp)
    {
        hr = GET(OBJ(dst, L"documentElement"), L"appendChild", &r, 1, vdisp(imp));
        check(SUCCEEDED(hr) && xml_is(dst, L"<r><t k=\"v\">text</t></r>"), "imported node can be appended");
    }
    hr = GET(dst, L"importNode", &r, 2, vdisp(t), vbool(0));
    check(SUCCEEDED(hr) && xml_is(V_UNKNOWN(&r), L"<t k=\"v\"/>"), "importNode shallow copy");

    idd = newdoc(L"<!DOCTYPE r [<!ATTLIST a id ID #IMPLIED>]><r><a id='x1'/><a id='x2'/></r>");
    hr = GET(idd, L"nodeFromID", &r, 1, vbstr(L"x2"));
    byid = SUCCEEDED(hr) && V_VT(&r) == VT_DISPATCH ? V_UNKNOWN(&r) : NULL;
    check(byid != NULL, "nodeFromID finds an element");
    if (byid) { GET(byid, L"getAttribute", &r, 1, vbstr(L"id")); CHECK(V_VT(&r) == VT_BSTR && !wcscmp(V_BSTR(&r), L"x2")); }
    hr = GET(idd, L"nodeFromID", &r, 1, vbstr(L"nope"));
    check(SUCCEEDED(hr) && (V_VT(&r) != VT_DISPATCH || V_DISPATCH(&r) == NULL), "nodeFromID without a match");
}

static void test_transform(void)
{
    IUnknown *xsl = newdoc(L"<xsl:stylesheet version='1.0' xmlns:xsl='http://www.w3.org/1999/XSL/Transform'>"
            L"<xsl:output method='xml' omit-xml-declaration='yes'/><xsl:template match='/'><out><xsl:value-of select='/r/@v'/>"
            L"</out></xsl:template></xsl:stylesheet>");
    IUnknown *src = newdoc(L"<r v='42'/>"), *out = create(L"Msxml2.DOMDocument.6.0");
    VARIANT r;
    HRESULT hr;

    hr = dcall(OBJ(src, L"documentElement"), L"transformNodeToObject", DISPATCH_METHOD, &r, 2, vdisp(xsl), vdisp(out));
    check(SUCCEEDED(hr) && xml_is(out, L"<out>42</out>"), "element transformNodeToObject");
}

static IUnknown *vbattrs(IUnknown *mx)
{
    IUnknown *vb = NULL;
    IUnknown_QueryInterface(mx, &IID_IVBSAXAttributes, (void **)&vb);
    return vb;
}

static void test_sax(void)
{
    IUnknown *at = create(L"Msxml2.SAXAttributes.6.0"), *at2 = create(L"Msxml2.SAXAttributes.6.0"), *rd, *x;
    IUnknown *vat, *vat2;
    VARIANT r;
    HRESULT hr;

    if (!at || !at2) { check(0, "SAXAttributes"); return; }
    vat = vbattrs(at);
    vat2 = vbattrs(at2);
    check(vat && vat2, "IVBSAXAttributes");
    if (!vat || !vat2) return;

    dcall(at, L"addAttribute", DISPATCH_METHOD, NULL, 5, vbstr(L""), vbstr(L"a"), vbstr(L"a"), vbstr(L"string"), vbstr(L"1"));
    dcall(at, L"addAttribute", DISPATCH_METHOD, NULL, 5, vbstr(L""), vbstr(L"c"), vbstr(L"c"), vbstr(L"CDATA"), vbstr(L"3"));
    hr = dcall(at, L"setAttribute", DISPATCH_METHOD, NULL, 6, vi4(0), vbstr(L""), vbstr(L"b"), vbstr(L"b"), vbstr(L"CDATA"), vbstr(L"2"));
    check(SUCCEEDED(hr), "setAttribute");

    raw_dispatch = 1;
    GET(vat, L"getLocalName", &r, 1, vi4(0));
    CHECK(V_VT(&r) == VT_BSTR && !wcscmp(V_BSTR(&r), L"b"));
    GET(vat, L"getValue", &r, 1, vi4(0));
    CHECK(V_VT(&r) == VT_BSTR && !wcscmp(V_BSTR(&r), L"2"));
    CHECK(get_long(vat, L"length", 0) == 2);
    hr = GET(vat, L"getTypeFromName", &r, 2, vbstr(L""), vbstr(L"b"));
    check(SUCCEEDED(hr) && V_VT(&r) == VT_BSTR && !wcscmp(V_BSTR(&r), L"CDATA"), "getTypeFromName");
    hr = GET(vat, L"getTypeFromQName", &r, 1, vbstr(L"c"));
    check(SUCCEEDED(hr) && V_VT(&r) == VT_BSTR && !wcscmp(V_BSTR(&r), L"CDATA"), "getTypeFromQName");
    raw_dispatch = 0;

    hr = dcall(at2, L"addAttributeFromIndex", DISPATCH_METHOD, NULL, 2, vdisp(vat), vi4(1));
    raw_dispatch = 1;
    check(SUCCEEDED(hr) && get_long(vat2, L"length", 0) == 1, "addAttributeFromIndex");
    GET(vat2, L"getQName", &r, 1, vi4(0));
    CHECK(V_VT(&r) == VT_BSTR && !wcscmp(V_BSTR(&r), L"c"));
    raw_dispatch = 0;
    hr = dcall(at2, L"setAttributes", DISPATCH_METHOD, NULL, 1, vdisp(vat));
    raw_dispatch = 1;
    check(SUCCEEDED(hr) && get_long(vat2, L"length", 0) == 2, "setAttributes copies the list");
    raw_dispatch = 0;
    hr = dcall(at, L"removeAttribute", DISPATCH_METHOD, NULL, 1, vi4(0));
    raw_dispatch = 1;
    check(SUCCEEDED(hr) && get_long(vat, L"length", 0) == 1, "removeAttribute");
    raw_dispatch = 0;

    rd = create(L"Msxml2.SAXXMLReader.6.0");
    if (rd)
    {
        PUT(rd, L"baseURL", vbstr(L"http://example.com/base/"));
        CHECK(streq(rd, L"baseURL", L"http://example.com/base/"));
        PUT(rd, L"secureBaseURL", vbstr(L"https://example.com/"));
        CHECK(streq(rd, L"secureBaseURL", L"https://example.com/"));
    }

    x = create(L"Msxml2.ServerXMLHTTP.6.0");
    if (x)
    {
        hr = GET(x, L"getOption", &r, 1, vi4(0));
        check(SUCCEEDED(hr) && V_I4(&r) == 65001, "default URL code page");
        dcall(x, L"setOption", DISPATCH_METHOD, NULL, 2, vi4(2), vi4(13056));
        hr = GET(x, L"getOption", &r, 1, vi4(2));
        check(SUCCEEDED(hr) && V_I4(&r) == 13056, "ignore certificate flags");
        hr = dcall(x, L"setTimeouts", DISPATCH_METHOD, NULL, 4, vi4(1000), vi4(2000), vi4(3000), vi4(4000));
        check(SUCCEEDED(hr), "setTimeouts");
    }
}

int main(void)
{
    CoInitialize(NULL);
    test_selection();
    test_namespaces();
    test_parse_errors();
    test_import_nodefromid();
    test_transform();
    test_sax();
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures ? 1 : 0;
}
