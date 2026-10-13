#define COBJMACROS
#include <windows.h>
#include <ole2.h>
#include <msxml2.h>
#include <stdio.h>
static const CLSID CLSID_DOM_ = {0xf6d90f11,0x9c73,0x11d3,{0xb3,0x2e,0x00,0xc0,0x4f,0x99,0x0b,0xb4}};
int main(void)
{
    IXMLDOMDocument *doc; IXMLDOMElement *root; IXMLDOMText *t; IXMLDOMNode *n, *out; IXMLDOMNodeList *kids; VARIANT_BOOL ok; LONG len = -1; HRESULT hr; BSTR b;
    setvbuf(stdout, NULL, _IONBF, 0);
    CoInitialize(NULL);
    CoCreateInstance(&CLSID_DOM_, NULL, CLSCTX_INPROC_SERVER, &IID_IXMLDOMDocument, (void**)&doc);
    b = SysAllocString(L"<r>a<e/>b</r>"); IXMLDOMDocument_loadXML(doc, b, &ok); SysFreeString(b);
    IXMLDOMDocument_get_documentElement(doc, &root);
    IXMLDOMElement_get_firstChild(root, &n); IXMLDOMNode_Release(n);
    { IXMLDOMNode *e; IXMLDOMElement_get_childNodes(root, &kids); IXMLDOMNodeList_get_item(kids, 1, &e); IXMLDOMNodeList_Release(kids);
      IXMLDOMElement_removeChild(root, e, &out); IXMLDOMNode_Release(out); IXMLDOMNode_Release(e); }
    IXMLDOMElement_get_childNodes(root, &kids); IXMLDOMNodeList_get_length(kids, &len); printf("BEFORE %ld\n", len); IXMLDOMNodeList_Release(kids);
    hr = IXMLDOMElement_normalize(root);
    printf("NORMALIZE %08lx\n", hr);
    IXMLDOMElement_get_childNodes(root, &kids); IXMLDOMNodeList_get_length(kids, &len); printf("AFTER %ld\n", len); IXMLDOMNodeList_Release(kids);
    b = NULL; IXMLDOMElement_get_xml(root, &b); printf("XML %ls\n", b); SysFreeString(b);
    printf("%s\n", len == 1 ? "RESULT: PASS" : "RESULT: FAIL");
    printf("DONE\n");
    return 0;
}
