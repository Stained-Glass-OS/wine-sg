/* msxml3 batch (patches/sg/2032), run by test/mxwdom-gate.sh: an MXXMLWriter
 * whose output is a DOM document builds the document from the SAX events
 * (from Wine's test_mxwriter_domdoc, which records Windows): startDocument
 * empties it, elements appear at startElement, text only at endElement.
 *
 *   mxwdom-probe.exe */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <ocidl.h>
#include <msxml6.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

static const GUID SG_CLSID_MXXMLWriter60 = { 0x88d96a0f, 0xf192, 0x11d4, { 0xa6, 0x5f, 0x00, 0x40, 0x96, 0x32, 0x51, 0xe5 } };
static const GUID SG_CLSID_DOMDocument60 = { 0x88d96a05, 0xf192, 0x11d4, { 0xa6, 0x5f, 0x00, 0x40, 0x96, 0x32, 0x51, 0xe5 } };
static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

static int bstr_is(BSTR b, const WCHAR *w)
{
    int ok = b && !wcscmp(b, w);
    if (!ok) printf("   got [%ls]\n", b ? b : L"(null)");
    SysFreeString(b);
    return ok;
}

int main(void)
{
    IMXWriter *writer = NULL;
    ISAXContentHandler *content = NULL;
    IXMLDOMDocument *doc = NULL;
    IXMLDOMElement *root = NULL, *e2 = NULL;
    IXMLDOMNodeList *list = NULL;
    IXMLDOMNode *node = NULL;
    VARIANT dest, v;
    BSTR str;
    LONG len = 0;
    HRESULT hr;

    CoInitialize(NULL);
    CoCreateInstance(&SG_CLSID_MXXMLWriter60, NULL, CLSCTX_INPROC_SERVER, &IID_IMXWriter, (void **)&writer);
    IMXWriter_QueryInterface(writer, &IID_ISAXContentHandler, (void **)&content);
    CoCreateInstance(&SG_CLSID_DOMDocument60, NULL, CLSCTX_INPROC_SERVER, &IID_IXMLDOMDocument, (void **)&doc);
    check(writer && content && doc, "objects created");

    V_VT(&dest) = VT_DISPATCH;
    V_DISPATCH(&dest) = (IDispatch *)doc;
    check(IMXWriter_put_output(writer, dest) == S_OK, "a document is accepted as output");

    str = SysAllocString(L"Old");
    IXMLDOMDocument_createElement(doc, str, &root);
    SysFreeString(str);
    IXMLDOMDocument_appendChild(doc, (IXMLDOMNode *)root, NULL);
    IXMLDOMElement_Release(root);
    root = NULL;

    check(ISAXContentHandler_startDocument(content) == S_OK, "startDocument");
    hr = IXMLDOMDocument_get_documentElement(doc, &root);
    check(hr == S_FALSE && root == NULL, "startDocument empties the document");

    check(ISAXContentHandler_startElement(content, L"", 0, L"", 0, L"BankAccount", 11, NULL) == S_OK, "startElement");
    hr = IXMLDOMDocument_get_documentElement(doc, &root);
    check(hr == S_OK && root != NULL, "the root element is there at once");
    IXMLDOMElement_get_nodeName(root, &str);
    check(bstr_is(str, L"BankAccount"), "the root's name");

    check(ISAXContentHandler_startElement(content, L"", 0, L"", 0, L"Number", 6, NULL) == S_OK, "startElement child");
    IXMLDOMElement_get_childNodes(root, &list);
    IXMLDOMNodeList_get_length(list, &len);
    check(len == 1, "the child is there at once");
    IXMLDOMNodeList_get_item(list, 0, &node);
    IXMLDOMNode_get_nodeName(node, &str);
    check(bstr_is(str, L"Number"), "the child's name");

    check(ISAXContentHandler_characters(content, L"12345", 5) == S_OK, "characters");
    IXMLDOMNode_get_text(node, &str);
    check(bstr_is(str, L""), "text is not visible before the element ends");
    check(ISAXContentHandler_endElement(content, L"", 0, L"", 0, L"Number", 6) == S_OK, "endElement");
    IXMLDOMNode_get_text(node, &str);
    check(bstr_is(str, L"12345"), "text is visible after the element ends");
    IXMLDOMNode_Release(node);

    check(ISAXContentHandler_startElement(content, L"", 0, L"", 0, L"Name", 4, NULL) == S_OK, "second child");
    ISAXContentHandler_characters(content, L"Captain Ahab", 12);
    ISAXContentHandler_endElement(content, L"", 0, L"", 0, L"Name", 4);
    ISAXContentHandler_processingInstruction(content, L"note", 4, L"hello", 5);
    ISAXContentHandler_endElement(content, L"", 0, L"", 0, L"BankAccount", 11);
    IXMLDOMNodeList_get_length(list, &len);
    check(len == 3, "the node list follows (two elements and an instruction)");
    IXMLDOMNodeList_get_item(list, 1, &node);
    IXMLDOMNode_get_nodeName(node, &str);
    check(bstr_is(str, L"Name"), "second child's name");
    IXMLDOMNode_get_text(node, &str);
    check(bstr_is(str, L"Captain Ahab"), "second child's text");
    IXMLDOMNode_Release(node);
    IXMLDOMNodeList_get_item(list, 2, &node);
    IXMLDOMNode_get_nodeName(node, &str);
    check(bstr_is(str, L"note"), "the instruction's target");
    IXMLDOMNode_Release(node);
    IXMLDOMNodeList_Release(list);
    IXMLDOMElement_Release(root);

    check(ISAXContentHandler_endDocument(content) == S_OK, "endDocument");
    hr = IXMLDOMDocument_get_documentElement(doc, &root);
    check(hr == S_OK && root != NULL, "the root stays after endDocument");
    IXMLDOMElement_Release(root);

    /* a second run replaces the first, with attributes and a namespace */
    ISAXContentHandler_startDocument(content);
    hr = IXMLDOMDocument_get_documentElement(doc, &root);
    check(hr == S_FALSE, "a second startDocument empties the document again");
    {
        IMXAttributes *attrs = NULL;
        ISAXAttributes *sax = NULL;
        CoCreateInstance(&CLSID_SAXAttributes60, NULL, CLSCTX_INPROC_SERVER, &IID_IMXAttributes, (void **)&attrs);
        IMXAttributes_addAttribute(attrs, L"", L"id", L"id", L"CDATA", L"7");
        IMXAttributes_QueryInterface(attrs, &IID_ISAXAttributes, (void **)&sax);
        ISAXContentHandler_startElement(content, L"urn:x", 5, L"item", 4, L"x:item", 6, sax);
        ISAXAttributes_Release(sax);
        IMXAttributes_Release(attrs);
    }
    ISAXContentHandler_endElement(content, L"urn:x", 5, L"item", 4, L"x:item", 6);
    ISAXContentHandler_endDocument(content);
    IXMLDOMDocument_get_documentElement(doc, &root);
    str = NULL;
    if (root) { IXMLDOMElement_get_namespaceURI(root, &str); }
    check(bstr_is(str, L"urn:x"), "the element has its namespace");
    V_VT(&v) = VT_EMPTY;
    str = SysAllocString(L"id");
    if (root) IXMLDOMElement_getAttribute(root, str, &v);
    SysFreeString(str);
    check(V_VT(&v) == VT_BSTR && !wcscmp(V_BSTR(&v), L"7"), "the element has its attribute");
    VariantClear(&v);
    if (root) IXMLDOMElement_Release(root);

    /* detached, the writer writes text again */
    V_VT(&dest) = VT_EMPTY;
    check(IMXWriter_put_output(writer, dest) == S_OK, "output detached");
    (void)e2;

    IXMLDOMDocument_Release(doc);
    ISAXContentHandler_Release(content);
    IMXWriter_Release(writer);
    printf("%d checks failed\n", failures);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    CoUninitialize();
    return failures != 0;
}
