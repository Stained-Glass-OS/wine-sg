/* msxml3: SAX reader getFeature for the kept features; put_nodeTypedValue on attribute, comment, CDATA and PI nodes. */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <ocidl.h>
#include <msxml6.h>
#include <stdio.h>
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

static const WCHAR *const features[] = {
    L"http://xml.org/sax/features/external-general-entities",
    L"http://xml.org/sax/features/external-parameter-entities",
    L"http://xml.org/sax/features/lexical-handler/parameter-entities",
    L"prohibit-dtd",
};

static int text_is(IXMLDOMNode *n, const WCHAR *want)
{
    BSTR s = NULL;
    int r;
    IXMLDOMNode_get_text(n, &s);
    r = s && !wcscmp(s, want);
    SysFreeString(s);
    return r;
}

int main(void)
{
    ISAXXMLReader *reader;
    IXMLDOMDocument *doc;
    IXMLDOMAttribute *attr;
    IXMLDOMComment *comment;
    IXMLDOMCDATASection *cdata;
    IXMLDOMProcessingInstruction *pi;
    VARIANT_BOOL value;
    VARIANT v;
    HRESULT hr;
    BSTR b, b2;
    int i;

    CoInitialize(NULL);
    reader = create(L"Msxml2.SAXXMLReader.6.0", &IID_ISAXXMLReader);
    if (!reader) { printf("FAIL create reader\nRESULT: FAIL\n"); return 1; }
    for (i = 0; i < 4; i++)
    {
        char what[100];
        value = 0x55;
        hr = ISAXXMLReader_getFeature(reader, features[i], &value);
        sprintf(what, "getFeature %d readable (hr %lx)", i, hr);
        check(hr == S_OK, what);
        hr = ISAXXMLReader_putFeature(reader, features[i], VARIANT_TRUE);
        value = VARIANT_FALSE;
        hr = ISAXXMLReader_getFeature(reader, features[i], &value);
        sprintf(what, "getFeature %d reads back true", i);
        check(hr == S_OK && value == VARIANT_TRUE, what);
        ISAXXMLReader_putFeature(reader, features[i], VARIANT_FALSE);
        value = VARIANT_TRUE;
        hr = ISAXXMLReader_getFeature(reader, features[i], &value);
        sprintf(what, "getFeature %d reads back false", i);
        check(hr == S_OK && value == VARIANT_FALSE, what);
    }
    ISAXXMLReader_Release(reader);

    doc = create(L"Msxml2.DOMDocument.6.0", &IID_IXMLDOMDocument);
    b = SysAllocString(L"a"); IXMLDOMDocument_createAttribute(doc, b, &attr); SysFreeString(b);
    V_VT(&v) = VT_I4; V_I4(&v) = 5;
    hr = IXMLDOMAttribute_put_nodeTypedValue(attr, v);
    check(hr == S_OK && text_is((IXMLDOMNode *)attr, L"5"), "attribute typed value");
    b = SysAllocString(L"c"); IXMLDOMDocument_createComment(doc, b, &comment); SysFreeString(b);
    V_VT(&v) = VT_BSTR; V_BSTR(&v) = SysAllocString(L"new text");
    hr = IXMLDOMComment_put_nodeTypedValue(comment, v);
    check(hr == S_OK && text_is((IXMLDOMNode *)comment, L"new text"), "comment typed value");
    b = SysAllocString(L"d"); IXMLDOMDocument_createCDATASection(doc, b, &cdata); SysFreeString(b);
    hr = IXMLDOMCDATASection_put_nodeTypedValue(cdata, v);
    check(hr == S_OK && text_is((IXMLDOMNode *)cdata, L"new text"), "cdata typed value");
    b = SysAllocString(L"t"); b2 = SysAllocString(L"x");
    IXMLDOMDocument_createProcessingInstruction(doc, b, b2, &pi); SysFreeString(b); SysFreeString(b2);
    hr = IXMLDOMProcessingInstruction_put_nodeTypedValue(pi, v);
    check(hr == S_OK && text_is((IXMLDOMNode *)pi, L"new text"), "pi typed value");
    VariantClear(&v);

    printf("%s\nDONE\n", failures ? "RESULT: FAIL" : "RESULT: PASS");
    return failures != 0;
}
