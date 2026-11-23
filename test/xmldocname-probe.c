/* IXMLDocument::createElement (the IE4/IE5 XML object model in msxml3): the optional
 * second argument is the new element's tag name (or a processing instruction's
 * target), and IXMLElement::addChild puts the child at the index it is given (-1: at the end).
 * wine-sg 9600: the name was ignored, so every created element had an empty name and a saved
 * document lost the element's tags; any index but 0 added the element in the wrong place. */
#define COBJMACROS
#include <windows.h>
#include <ole2.h>
#include <ocidl.h>
#include <msxml2.h>
#include <stdio.h>

static const CLSID clsid_xmldocument = {0xcfc399af,0xd876,0x11d0,{0x9c,0x10,0x00,0xc0,0x4f,0xc9,0x9c,0x8e}};
static int failures;
#define CHECK(c, ...) do { if (c) printf("ok    " __VA_ARGS__); else { failures++; printf("FAIL  " __VA_ARGS__); } } while (0)

static IXMLElement *create(IXMLDocument *doc, LONG type, const WCHAR *name)
{
    VARIANT vt, vn;
    IXMLElement *el = NULL;
    HRESULT hr;
    V_VT(&vt) = VT_I4; V_I4(&vt) = type;
    if (name) { V_VT(&vn) = VT_BSTR; V_BSTR(&vn) = SysAllocString(name); }
    else V_VT(&vn) = VT_NULL;
    hr = IXMLDocument_createElement(doc, vt, vn, &el);
    VariantClear(&vn);
    if (FAILED(hr)) { printf("createElement(%ld) failed %#lx\n", type, hr); return NULL; }
    return el;
}

int main(void)
{
    IXMLDocument *doc;
    IXMLElement *root, *el, *pi, *noname;
    IPersistStreamInit *psi;
    IStream *stm;
    HGLOBAL hg;
    LARGE_INTEGER zero = {{0}};
    BSTR tag;
    HRESULT hr;
    char *text;
    static const char src[] = "<?xml version=\"1.0\"?><list></list>";

    CoInitialize(NULL);
    hr = CoCreateInstance(&clsid_xmldocument, NULL, CLSCTX_INPROC_SERVER, &IID_IXMLDocument, (void **)&doc);
    if (FAILED(hr)) { printf("no XMLDocument %#lx\nRESULT: FAIL\n", hr); return 1; }

    el = create(doc, XMLELEMTYPE_ELEMENT, L"item");
    tag = NULL;
    hr = el ? IXMLElement_get_tagName(el, &tag) : E_FAIL;
    CHECK(hr == S_OK && tag && !lstrcmpW(tag, L"ITEM"), "createElement(ELEMENT, \"item\") tagName %ls\n", tag ? tag : L"(null)");
    SysFreeString(tag);

    pi = create(doc, XMLELEMTYPE_PI, L"app-data");
    tag = NULL;
    hr = pi ? IXMLElement_get_tagName(pi, &tag) : E_FAIL;
    CHECK(hr == S_OK && tag && !lstrcmpW(tag, L"APP-DATA"), "createElement(PI, \"app-data\") tagName %ls\n", tag ? tag : L"(null)");
    SysFreeString(tag);

    noname = create(doc, XMLELEMTYPE_ELEMENT, NULL);
    tag = (BSTR)1;
    hr = noname ? IXMLElement_get_tagName(noname, &tag) : E_FAIL;
    CHECK(hr == S_OK && tag == NULL, "createElement(ELEMENT, VT_NULL) has no tag name\n");
    if (noname) IXMLElement_Release(noname);

    /* the created element, added to a loaded document, is saved with its tags */
    IXMLDocument_QueryInterface(doc, &IID_IPersistStreamInit, (void **)&psi);
    hg = GlobalAlloc(GMEM_MOVEABLE, sizeof(src) - 1);
    memcpy(GlobalLock(hg), src, sizeof(src) - 1); GlobalUnlock(hg);
    CreateStreamOnHGlobal(hg, TRUE, &stm);
    hr = IPersistStreamInit_Load(psi, stm);
    IStream_Release(stm);
    CHECK(hr == S_OK, "loaded a document %#lx\n", hr);
    root = NULL;
    IXMLDocument_get_root(doc, &root);
    if (root && el)
    {
        VARIANT v;
        V_VT(&v) = VT_BSTR; V_BSTR(&v) = SysAllocString(L"1");
        IXMLElement_setAttribute(el, SysAllocString(L"id"), v);
        IXMLElement *first = create(doc, XMLELEMTYPE_ELEMENT, L"first");
        hr = IXMLElement_addChild(root, el, -1, -1);
        CHECK(hr == S_OK, "addChild(-1) appends %#lx\n", hr);
        hr = first ? IXMLElement_addChild(root, first, 0, -1) : E_FAIL;
        CHECK(hr == S_OK, "addChild(0) %#lx\n", hr);
    }
    CreateStreamOnHGlobal(NULL, TRUE, &stm);
    hr = IPersistStreamInit_Save(psi, stm, FALSE);
    IStream_Write(stm, "", 1, NULL);
    GetHGlobalFromStream(stm, &hg);
    text = GlobalLock(hg);
    printf("saved: %s\n", text);
    CHECK(hr == S_OK && strstr(text, "<list><first/><item id=\"1\"/></list>") != NULL,
          "saved document has the children in place: <list><first/><item id=\"1\"/></list>\n");
    GlobalUnlock(hg);
    IStream_Release(stm);

    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
