/* msxml3: XSLProcessor::addParameter value types (strings, numbers, booleans). */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <ocidl.h>
#include <msxml6.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

static void *create(const WCHAR *progid, REFIID riid)
{
    CLSID clsid;
    void *obj = NULL;
    if (FAILED(CLSIDFromProgID(progid, &clsid))) return NULL;
    if (FAILED(CoCreateInstance(&clsid, NULL, CLSCTX_INPROC_SERVER, riid, &obj))) return NULL;
    return obj;
}

static IXMLDOMDocument *load(const WCHAR *progid, const WCHAR *text)
{
    IXMLDOMDocument *doc = create(progid, &IID_IXMLDOMDocument);
    VARIANT_BOOL ok = VARIANT_FALSE;
    BSTR b = SysAllocString(text);
    if (doc) IXMLDOMDocument_loadXML(doc, b, &ok);
    SysFreeString(b);
    return ok == VARIANT_TRUE ? doc : NULL;
}

static WCHAR outbuf[300];
static const WCHAR *run(IXSLProcessor *proc)
{
    VARIANT_BOOL ok = VARIANT_FALSE;
    VARIANT v;
    HRESULT hr = IXSLProcessor_transform(proc, &ok);
    outbuf[0] = 0;
    if (hr != S_OK || ok != VARIANT_TRUE) { wcscpy(outbuf, L"<failed>"); return outbuf; }
    VariantInit(&v);
    IXSLProcessor_get_output(proc, &v);
    if (V_VT(&v) == VT_BSTR && V_BSTR(&v)) wcsncpy(outbuf, V_BSTR(&v), 299);
    VariantClear(&v);
    return outbuf;
}

static HRESULT param(IXSLProcessor *proc, const WCHAR *name, VARIANT v)
{
    BSTR n = SysAllocString(name);
    HRESULT hr = IXSLProcessor_addParameter(proc, n, v, NULL);
    SysFreeString(n);
    return hr;
}

static const WCHAR xsl[] =
    L"<xsl:stylesheet version=\"1.0\" xmlns:xsl=\"http://www.w3.org/1999/XSL/Transform\">"
    L"<xsl:output method=\"text\"/>"
    L"<xsl:param name=\"n\"/><xsl:param name=\"b\"/><xsl:param name=\"s\"/>"
    L"<xsl:template match=\"/\"><xsl:value-of select=\"$n * 2\"/>|<xsl:if test=\"$b\">yes</xsl:if>"
    L"<xsl:if test=\"not($b)\">no</xsl:if>|<xsl:value-of select=\"$s\"/>|<xsl:value-of select=\"string($n)\"/>|<xsl:if test=\"$n &gt; 1000000\">big</xsl:if>"
    L"</xsl:template></xsl:stylesheet>";


/* a small automation object: add(a,b), greet(s), big(n), nothing() */
typedef struct { IDispatch iface; LONG ref; int calls; } ext_obj;
static HRESULT WINAPI e_qi(IDispatch *i, REFIID r, void **o)
{
    if (IsEqualIID(r, &IID_IUnknown) || IsEqualIID(r, &IID_IDispatch)) { *o = i; IDispatch_AddRef(i); return S_OK; }
    *o = NULL; return E_NOINTERFACE;
}
static ULONG WINAPI e_addref(IDispatch *i) { return InterlockedIncrement(&((ext_obj *)i)->ref); }
static ULONG WINAPI e_release(IDispatch *i) { return InterlockedDecrement(&((ext_obj *)i)->ref); }
static HRESULT WINAPI e_tic(IDispatch *i, UINT *n) { *n = 0; return S_OK; }
static HRESULT WINAPI e_ti(IDispatch *i, UINT n, LCID l, ITypeInfo **t) { return E_NOTIMPL; }
static HRESULT WINAPI e_ids(IDispatch *i, REFIID r, LPOLESTR *names, UINT c, LCID l, DISPID *ids)
{
    if (!wcscmp(names[0], L"add")) ids[0] = 1;
    else if (!wcscmp(names[0], L"greet")) ids[0] = 2;
    else if (!wcscmp(names[0], L"big")) ids[0] = 3;
    else if (!wcscmp(names[0], L"nothing")) ids[0] = 4;
    else return DISP_E_UNKNOWNNAME;
    return S_OK;
}
static HRESULT WINAPI e_invoke(IDispatch *i, DISPID id, REFIID r, LCID l, WORD f, DISPPARAMS *p, VARIANT *res, EXCEPINFO *x, UINT *e)
{
    VARIANT t;
    ((ext_obj *)i)->calls++;
    switch (id)
    {
    case 1: /* arguments arrive last first */
        VariantInit(&t); VariantChangeType(&t, &p->rgvarg[1], 0, VT_R8);
        V_VT(res) = VT_R8; V_R8(res) = V_R8(&t);
        VariantChangeType(&t, &p->rgvarg[0], 0, VT_R8); V_R8(res) += V_R8(&t);
        return S_OK;
    case 2:
    {
        WCHAR buf[80]; swprintf(buf, 80, L"hello %ls", V_VT(&p->rgvarg[0]) == VT_BSTR ? V_BSTR(&p->rgvarg[0]) : L"?");
        V_VT(res) = VT_BSTR; V_BSTR(res) = SysAllocString(buf); return S_OK;
    }
    case 3:
        VariantInit(&t); VariantChangeType(&t, &p->rgvarg[0], 0, VT_R8);
        V_VT(res) = VT_BOOL; V_BOOL(res) = V_R8(&t) > 100 ? VARIANT_TRUE : VARIANT_FALSE; return S_OK;
    case 4: return S_OK;
    }
    return DISP_E_MEMBERNOTFOUND;
}
static IDispatchVtbl ext_vtbl = { e_qi, e_addref, e_release, e_tic, e_ti, e_ids, e_invoke };

static const WCHAR xsl2[] =
    L"<xsl:stylesheet version=\"1.0\" xmlns:xsl=\"http://www.w3.org/1999/XSL/Transform\" xmlns:x=\"urn:ext\">"
    L"<xsl:output method=\"text\"/>"
    L"<xsl:template match=\"/\"><xsl:value-of select=\"x:add(2, 3)\"/>|<xsl:value-of select=\"x:greet('w')\"/>|"
    L"<xsl:if test=\"x:big(500)\">big</xsl:if><xsl:if test=\"not(x:big(5))\">small</xsl:if>|[<xsl:value-of select=\"x:nothing()\"/>]"
    L"</xsl:template></xsl:stylesheet>";
static const WCHAR xsl3[] =
    L"<xsl:stylesheet version=\"1.0\" xmlns:xsl=\"http://www.w3.org/1999/XSL/Transform\" xmlns:x=\"urn:ext\">"
    L"<xsl:output method=\"text\"/>"
    L"<xsl:template match=\"/\">a<xsl:value-of select=\"x:missing()\"/>b</xsl:template></xsl:stylesheet>";

static void test_objects(void)
{
    ext_obj eo = { { &ext_vtbl }, 1, 0 };
    IXSLTemplate *tmpl = create(L"Msxml2.XSLTemplate.3.0", &IID_IXSLTemplate);
    IXSLProcessor *proc;
    IXMLDOMDocument *sheet = load(L"Msxml2.FreeThreadedDOMDocument.3.0", xsl2);
    IXMLDOMDocument *sheet3 = load(L"Msxml2.FreeThreadedDOMDocument.3.0", xsl3);
    IXMLDOMDocument *input = load(L"Msxml2.DOMDocument.3.0", L"<a/>");
    VARIANT v;
    BSTR uri = SysAllocString(L"urn:ext");
    VARIANT_BOOL ok = VARIANT_TRUE;
    HRESULT hr;

    if (!tmpl || !sheet || !sheet3 || !input) { check(0, "objects setup"); return; }
    IXSLTemplate_putref_stylesheet(tmpl, (IXMLDOMNode *)sheet);
    IXSLTemplate_createProcessor(tmpl, &proc);
    V_VT(&v) = VT_UNKNOWN; V_UNKNOWN(&v) = (IUnknown *)input;
    IXSLProcessor_put_input(proc, v);

    check(IXSLProcessor_addObject(proc, NULL, uri) == E_INVALIDARG, "addObject(NULL)");
    check(IXSLProcessor_addObject(proc, &eo.iface, uri) == S_OK, "addObject");
    check(eo.ref == 2, "object referenced once by the processor");
    check(!wcscmp(run(proc), L"5|hello w|big small|[]") || !wcscmp(run(proc), L"5|hello w|bigsmall|[]"), "extension functions of the object");
    check(eo.calls >= 4, "methods were called");

    IXSLProcessor_Release(proc);
    check(eo.ref == 1, "object released with the processor");

    IXSLTemplate_putref_stylesheet(tmpl, (IXMLDOMNode *)sheet3);
    IXSLTemplate_createProcessor(tmpl, &proc);
    IXSLProcessor_put_input(proc, v);
    IXSLProcessor_addObject(proc, &eo.iface, uri);
    hr = IXSLProcessor_transform(proc, &ok);
    check(1, "unknown method does not crash");
    IXSLProcessor_Release(proc);
    SysFreeString(uri);
}

int main(void)
{
    IXSLTemplate *tmpl;
    IXSLProcessor *proc;
    IXMLDOMDocument *sheet, *input;
    VARIANT v;
    HRESULT hr;

    CoInitialize(NULL);
    tmpl = create(L"Msxml2.XSLTemplate.3.0", &IID_IXSLTemplate);
    sheet = load(L"Msxml2.FreeThreadedDOMDocument.3.0", xsl);
    input = load(L"Msxml2.DOMDocument.3.0", L"<a/>");
    if (!tmpl || !sheet || !input) { printf("FAIL setup\nRESULT: FAIL\n"); return 1; }
    IXSLTemplate_putref_stylesheet(tmpl, (IXMLDOMNode *)sheet);
    IXSLTemplate_createProcessor(tmpl, &proc);
    V_VT(&v) = VT_UNKNOWN; V_UNKNOWN(&v) = (IUnknown *)input;
    IXSLProcessor_put_input(proc, v);

    V_VT(&v) = VT_I4; V_I4(&v) = 21;
    check(param(proc, L"n", v) == S_OK, "addParameter VT_I4");
    V_VT(&v) = VT_BOOL; V_BOOL(&v) = VARIANT_TRUE;
    check(param(proc, L"b", v) == S_OK, "addParameter VT_BOOL");
    V_VT(&v) = VT_BSTR; V_BSTR(&v) = SysAllocString(L"str");
    check(param(proc, L"s", v) == S_OK, "addParameter VT_BSTR");
    SysFreeString(V_BSTR(&v));
    check(!wcscmp(run(proc), L"42|yes|str|21|"), "integer arithmetic, boolean test, string");

    V_VT(&v) = VT_R8; V_R8(&v) = 1.5;
    param(proc, L"n", v);
    V_VT(&v) = VT_BOOL; V_BOOL(&v) = VARIANT_FALSE;
    param(proc, L"b", v);
    check(!wcscmp(run(proc), L"3|no|str|1.5|"), "double and false");

    V_VT(&v) = VT_R8; V_R8(&v) = 1e20;
    param(proc, L"n", v);
    check(wcsstr(run(proc), L"|big") != NULL, "large double is a number (no exponent syntax error)");

    V_VT(&v) = VT_UI1; V_UI1(&v) = 7;
    hr = param(proc, L"n", v);
    check(hr == S_OK && !wcscmp(run(proc), L"14|no|str|7|"), "byte");

    test_objects();
    printf("%s\nDONE\n", failures ? "RESULT: FAIL" : "RESULT: PASS");
    return failures != 0;
}
