/* msxml-removeattr-gate.sh's probe (0812): IXMLDOMElement::removeAttributeNode,
 * through MSXML 4 as Meedio's theme loader calls it. Prints "NAME VALUE". */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <msxml2.h>
#include <stdio.h>

DEFINE_GUID(probe_CLSID_DOMDocument40, 0x88d969c0, 0xf192, 0x11d4, 0xa6, 0x5f, 0x00, 0x40, 0x96, 0x32, 0x51, 0xe5);

int wmain(void)
{
    IXMLDOMDocument *doc;
    IXMLDOMElement *root, *other;
    IXMLDOMAttribute *x, *out = NULL;
    IXMLDOMNamedNodeMap *attrs;
    VARIANT_BOOL ok;
    VARIANT v;
    BSTR xml;
    long n = -1;
    HRESULT hr;

    CoInitialize(NULL);
    if (FAILED(CoCreateInstance(&probe_CLSID_DOMDocument40, NULL, CLSCTX_INPROC_SERVER, &IID_IXMLDOMDocument, (void **)&doc)))
    {
        printf("nodoc 1\n");
        return 1;
    }
    IXMLDOMDocument_loadXML(doc, SysAllocString(L"<white a=\"255\" r=\"1\"/>"), &ok);
    IXMLDOMDocument_get_documentElement(doc, &root);
    IXMLDOMElement_getAttributeNode(root, SysAllocString(L"a"), &x);
    hr = IXMLDOMElement_removeAttributeNode(root, x, &out);
    printf("remove %08lx\n", hr);
    printf("returned-same %d\n", out == x);
    IXMLDOMElement_get_attributes(root, &attrs);
    IXMLDOMNamedNodeMap_get_length(attrs, &n);
    printf("left %ld\n", n);
    VariantInit(&v);
    printf("get-removed %08lx\n", IXMLDOMElement_getAttribute(root, SysAllocString(L"a"), &v));
    IXMLDOMElement_get_xml(root, &xml);
    printf("xml %ls\n", xml);
    VariantInit(&v);
    IXMLDOMAttribute_get_value(x, &v);
    printf("value-kept %ls\n", V_VT(&v) == VT_BSTR ? V_BSTR(&v) : L"?");
    printf("again %08lx\n", IXMLDOMElement_removeAttributeNode(root, x, NULL));
    IXMLDOMDocument_createElement(doc, SysAllocString(L"other"), &other);
    IXMLDOMElement_getAttributeNode(root, SysAllocString(L"r"), &x);
    printf("other %08lx\n", IXMLDOMElement_removeAttributeNode(other, x, NULL));
    printf("null %08lx\n", IXMLDOMElement_removeAttributeNode(root, NULL, NULL));
    return 0;
}
