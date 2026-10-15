#define COBJMACROS
#include <windows.h>
#include <ole2.h>
#include <msxml2.h>
#include <stdio.h>
#define P(...) printf(__VA_ARGS__)
static int fails, checks;
#define CHK(c) do { checks++; if (!(c)) { fails++; P("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static const CLSID CLSID_DOM30 = {0xf5078f32,0xc551,0x11d3,{0x89,0xb9,0x00,0x00,0xf8,0x1f,0xe2,0x21}};

#define DEFN(iface, node) do { IXMLDOMNode *d = (IXMLDOMNode*)1; HRESULT h = IXMLDOMNode_get_definition(node, &d); \
    CHK(h == S_FALSE && d == NULL); h = IXMLDOMNode_get_definition(node, NULL); CHK(h == E_INVALIDARG); } while (0)

int main(void)
{
    IXMLDOMDocument *doc; IXMLDOMDocumentType *dt; IXMLDOMNode *dtn, *node = NULL, *n2 = NULL; VARIANT_BOOL ok;
    IXMLDOMElement *root; IXMLDOMNodeList *list; IXMLDOMComment *com; IXMLDOMDocumentFragment *frag;
    IXMLDOMText *txt; IXMLDOMProcessingInstruction *pi; IXMLDOMCDATASection *cd; IXMLDOMAttribute *at;
    HRESULT hr; VARIANT v, v2; BSTR b; LONG len;
    setvbuf(stdout, NULL, _IONBF, 0);
    CoInitialize(NULL);
    hr = CoCreateInstance(&CLSID_DOM30, NULL, CLSCTX_INPROC_SERVER, &IID_IXMLDOMDocument, (void**)&doc);
    if (FAILED(hr)) { P("FAIL: no DOMDocument %lx\nRESULT: FAIL\n", hr); return 1; }
    b = SysAllocString(L"<?xml version=\"1.0\"?><!DOCTYPE r [<!ELEMENT r (#PCDATA)>]><r a=\"1\"><!--c-->text<![CDATA[x]]><?p q?></r>");
    hr = IXMLDOMDocument_loadXML(doc, b, &ok); SysFreeString(b);
    CHK(hr == S_OK && ok == VARIANT_TRUE);
    hr = IXMLDOMDocument_get_doctype(doc, &dt); CHK(hr == S_OK && dt);
    IXMLDOMDocumentType_QueryInterface(dt, &IID_IXMLDOMNode, (void**)&dtn);

    /* definition on every node kind: no DTD definition is found */
    DEFN(0, dtn);
    IXMLDOMDocument_get_documentElement(doc, &root);
    IXMLDOMElement_QueryInterface(root, &IID_IXMLDOMNode, (void**)&node); DEFN(0, node); IXMLDOMNode_Release(node);
    IXMLDOMDocument_QueryInterface(doc, &IID_IXMLDOMNode, (void**)&node); DEFN(0, node); IXMLDOMNode_Release(node);
    IXMLDOMElement_get_childNodes(root, &list);
    IXMLDOMNodeList_get_length(list, &len); CHK(len == 4);
    { int i; for (i = 0; i < len; i++) { IXMLDOMNodeList_get_item(list, i, &node); DEFN(0, node); IXMLDOMNode_Release(node); } }
    IXMLDOMNodeList_Release(list);
    b = SysAllocString(L"a"); IXMLDOMElement_getAttributeNode(root, b, &at); SysFreeString(b);
    IXMLDOMAttribute_QueryInterface(at, &IID_IXMLDOMNode, (void**)&node); DEFN(0, node); IXMLDOMNode_Release(node);
    IXMLDOMDocument_createDocumentFragment(doc, &frag);
    IXMLDOMDocumentFragment_QueryInterface(frag, &IID_IXMLDOMNode, (void**)&node); DEFN(0, node); IXMLDOMNode_Release(node);

    /* doctype as a node */
    VariantInit(&v);
    hr = IXMLDOMNode_get_childNodes(dtn, &list); CHK(hr == S_OK && list);
    if (list) { len = -1; IXMLDOMNodeList_get_length(list, &len); CHK(len == 0); IXMLDOMNodeList_Release(list); }
    hr = IXMLDOMNode_get_dataType(dtn, &v); CHK(hr == S_FALSE && V_VT(&v) == VT_NULL);
    b = SysAllocString(L"string");
    CHK(IXMLDOMNode_put_dataType(dtn, b) == E_FAIL); SysFreeString(b);
    CHK(IXMLDOMNode_put_dataType(dtn, NULL) == E_INVALIDARG);
    b = SysAllocString(L"*"); hr = IXMLDOMNode_selectNodes(dtn, b, &list); CHK(hr == S_OK && list);
    if (list) IXMLDOMNodeList_Release(list);
    hr = IXMLDOMNode_selectSingleNode(dtn, b, &node); CHK(hr != E_NOTIMPL); if (node) IXMLDOMNode_Release(node); SysFreeString(b);
    hr = IXMLDOMNode_cloneNode(dtn, VARIANT_TRUE, &n2); CHK(hr != E_NOTIMPL); if (n2) IXMLDOMNode_Release(n2);
    IXMLDOMDocument_createComment(doc, L"zz" ? (b = SysAllocString(L"zz")) : NULL, &com); SysFreeString(b);
    IXMLDOMComment_QueryInterface(com, &IID_IXMLDOMNode, (void**)&node);
    n2 = NULL; hr = IXMLDOMNode_appendChild(dtn, node, &n2); CHK(hr != E_NOTIMPL);
    n2 = NULL; VariantInit(&v2); hr = IXMLDOMNode_insertBefore(dtn, node, v2, &n2); CHK(hr != E_NOTIMPL);
    n2 = NULL; hr = IXMLDOMNode_replaceChild(dtn, node, node, &n2); CHK(hr != E_NOTIMPL);
    n2 = NULL; hr = IXMLDOMNode_removeChild(dtn, node, &n2); CHK(hr != E_NOTIMPL);
    b = SysAllocString(L"t"); hr = IXMLDOMNode_put_text(dtn, b); CHK(hr != E_NOTIMPL); SysFreeString(b);
    V_VT(&v) = VT_BSTR; V_BSTR(&v) = SysAllocString(L"t");
    hr = IXMLDOMNode_put_nodeValue(dtn, v); CHK(hr != E_NOTIMPL);
    hr = IXMLDOMNode_put_nodeTypedValue(dtn, v); CHK(hr != E_NOTIMPL);
    VariantClear(&v);
    hr = IXMLDOMNode_transformNode(dtn, dtn, &b); CHK(hr != E_NOTIMPL); if (SUCCEEDED(hr)) SysFreeString(b);
    IXMLDOMNode_Release(node); IXMLDOMComment_Release(com);

    /* class factory */
    {
        IClassFactory *cf;
        hr = CoGetClassObject(&CLSID_DOM30, CLSCTX_INPROC_SERVER, NULL, &IID_IClassFactory, (void**)&cf);
        CHK(hr == S_OK && cf);
        if (cf) { CHK(IClassFactory_LockServer(cf, TRUE) == S_OK); CHK(IClassFactory_LockServer(cf, FALSE) == S_OK); IClassFactory_Release(cf); }
    }
    IXMLDOMDocumentFragment_Release(frag); IXMLDOMAttribute_Release(at);
    IXMLDOMNode_Release(dtn); IXMLDOMDocumentType_Release(dt); IXMLDOMElement_Release(root); IXMLDOMDocument_Release(doc);
    P("checks %d fails %d\nRESULT: %s\n", checks, fails, fails ? "FAIL" : "PASS");
    return fails != 0;
}
