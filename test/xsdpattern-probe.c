/* xsdpattern-gate.sh's probe (0489): XML Schema patterns with MSXML's \uXXXX
 * escapes, added to a schema cache and used to validate.
 *   xsdpattern-probe N TEXT   schema N's pattern; TEXT the element's text
 *                             (BSU: x, a backslash, u0041 -- no shell mangles it)
 * prints "add HR valid HR" */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <msxml6.h>
#include <stdio.h>

static const WCHAR *const patterns[] = {
    L"[a-z]+",                                          /* 0: no escapes */
    L"[^\\u0041-\\u005A]+",                             /* 1: no capitals */
    L"[^\\uFDD0-\\uFDEF\\uFFF9-\\uFFFF\\p{IsPrivateUse}]+", /* 2: App-V's */
    L"[^&quot;&amp;&lt;&gt;\\u0000-\\u0020\\u007F]+",   /* 3: no space or quote */
    L"a\\u002Eb",                                       /* 4: an escaped regex metacharacter: a literal dot */
    L"x\\\\u0041",                                      /* 5: an escaped backslash, then "u0041" */
};

int wmain(int argc, WCHAR **argv)
{
    IXMLDOMSchemaCollection *cache;
    IXMLDOMDocument2 *xsd, *doc;
    IXMLDOMParseError *err;
    VARIANT v;
    VARIANT_BOOL ok;
    HRESULT add, valid = E_FAIL;
    WCHAR buf[2048];
    int n;

    if (argc < 3 || (n = _wtoi(argv[1])) < 0 || n >= ARRAYSIZE(patterns)) return 2;
    CoInitialize(NULL);
    swprintf(buf, ARRAYSIZE(buf), L"<xs:schema xmlns:xs='http://www.w3.org/2001/XMLSchema' targetNamespace='urn:t' "
             L"xmlns='urn:t'><xs:simpleType name='s'><xs:restriction base='xs:string'><xs:pattern value='%ls'/>"
             L"</xs:restriction></xs:simpleType><xs:element name='e' type='s'/></xs:schema>", patterns[n]);
    CoCreateInstance(&CLSID_DOMDocument60, NULL, CLSCTX_INPROC_SERVER, &IID_IXMLDOMDocument2, (void **)&xsd);
    CoCreateInstance(&CLSID_DOMDocument60, NULL, CLSCTX_INPROC_SERVER, &IID_IXMLDOMDocument2, (void **)&doc);
    CoCreateInstance(&CLSID_XMLSchemaCache60, NULL, CLSCTX_INPROC_SERVER, &IID_IXMLDOMSchemaCollection, (void **)&cache);
    IXMLDOMDocument2_loadXML(xsd, SysAllocString(buf), &ok);
    V_VT(&v) = VT_DISPATCH; V_DISPATCH(&v) = (IDispatch *)xsd;
    add = IXMLDOMSchemaCollection_add(cache, SysAllocString(L"urn:t"), v);
    if (SUCCEEDED(add))
    {
        swprintf(buf, ARRAYSIZE(buf), L"<e xmlns='urn:t'>%ls</e>", lstrcmpW(argv[2], L"BSU") ? argv[2] : L"x\\u0041");
        IXMLDOMDocument2_loadXML(doc, SysAllocString(buf), &ok);
        V_VT(&v) = VT_DISPATCH; V_DISPATCH(&v) = (IDispatch *)cache;
        IXMLDOMDocument2_putref_schemas(doc, v);
        valid = IXMLDOMDocument2_validate(doc, &err);
    }
    printf("add %08lx valid %08lx\n", add, valid);
    return 0;
}
