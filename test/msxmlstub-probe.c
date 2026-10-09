/* msxml3 stub batch (patches/sg/2008), run by test/msxmlstub-gate.sh.
 * Families: IXSLTemplate::stylesheet, IXSLProcessor input / ownerTemplate /
 * stylesheet / startMode / startModeURI / readyState / reset, and
 * IXMLDOMNode::specified / parsed on every node type including the document
 * type node, whose other read-only members are checked too.
 *
 *   msxmlstub-probe.exe */
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
static void checkhr(HRESULT hr, HRESULT want, const char *what)
{
    char buf[200];
    snprintf(buf, sizeof(buf), "%s (hr %08lx, want %08lx)", what, (unsigned long)hr, (unsigned long)want);
    check(hr == want, buf);
}

static const WCHAR xsl[] =
    L"<xsl:stylesheet version=\"1.0\" xmlns:xsl=\"http://www.w3.org/1999/XSL/Transform\" xmlns:a=\"urn:x\">"
    L"<xsl:output method=\"text\"/>"
    L"<xsl:param name=\"p\"/>"
    L"<xsl:template match=\"/\">default<xsl:value-of select=\"$p\"/></xsl:template>"
    L"<xsl:template match=\"/\" mode=\"m1\">mode1<xsl:value-of select=\"$p\"/></xsl:template>"
    L"<xsl:template match=\"/\" mode=\"m2\">mode2</xsl:template>"
    L"<xsl:template match=\"/\" mode=\"a:m3\">mode3</xsl:template>"
    L"</xsl:stylesheet>";

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
    if (ok != VARIANT_TRUE) { check(0, "document loads"); return NULL; }
    return doc;
}

/* object identity: compare IUnknown pointers */
static int same(IUnknown *a, IUnknown *b)
{
    IUnknown *ua = NULL, *ub = NULL;
    int r;
    if (!a || !b) return a == b;
    IUnknown_QueryInterface(a, &IID_IUnknown, (void **)&ua);
    IUnknown_QueryInterface(b, &IID_IUnknown, (void **)&ub);
    r = ua == ub;
    IUnknown_Release(ua);
    IUnknown_Release(ub);
    return r;
}

static ULONG refs(IUnknown *u)
{
    IUnknown_AddRef(u);
    return IUnknown_Release(u);
}

/* run the processor and return the text output, "" on failure */
static WCHAR outbuf[200];
static const WCHAR *run(IXSLProcessor *proc)
{
    VARIANT_BOOL ok = VARIANT_FALSE;
    VARIANT v;
    HRESULT hr = IXSLProcessor_transform(proc, &ok);
    outbuf[0] = 0;
    if (hr != S_OK || ok != VARIANT_TRUE) { wcscpy(outbuf, L"<failed>"); return outbuf; }
    VariantInit(&v);
    IXSLProcessor_get_output(proc, &v);
    if (V_VT(&v) == VT_BSTR && V_BSTR(&v)) wcsncpy(outbuf, V_BSTR(&v), 199);
    VariantClear(&v);
    return outbuf;
}

static int bstr_is(BSTR s, const WCHAR *want)
{
    int r = s && !wcscmp(s, want);
    SysFreeString(s);
    return r;
}

static void test_xsl(void)
{
    IXSLTemplate *tmpl = create(L"Msxml2.XSLTemplate.3.0", &IID_IXSLTemplate);
    IXSLProcessor *proc, *proc2;
    IXMLDOMDocument *sheet, *input;
    IXMLDOMNode *node;
    IXSLTemplate *owner;
    VARIANT v, v2;
    BSTR s, s2;
    LONG state;
    ULONG r1, r2;
    HRESULT hr;
    VARIANT_BOOL ok;

    if (!tmpl) { check(0, "XSLTemplate created"); return; }
    sheet = load(L"Msxml2.FreeThreadedDOMDocument.3.0", xsl);
    input = load(L"Msxml2.DOMDocument.3.0", L"<a/>");
    if (!sheet || !input) return;

    /* IXSLTemplate::stylesheet */
    checkhr(IXSLTemplate_get_stylesheet(tmpl, NULL), E_INVALIDARG, "template stylesheet(NULL)");
    node = (void *)0xdeadbeef;
    checkhr(IXSLTemplate_get_stylesheet(tmpl, &node), S_OK, "template stylesheet before putref");
    check(node == NULL, "template stylesheet before putref is NULL");
    IXSLTemplate_putref_stylesheet(tmpl, (IXMLDOMNode *)sheet);
    r1 = refs((IUnknown *)sheet);
    node = NULL;
    checkhr(IXSLTemplate_get_stylesheet(tmpl, &node), S_OK, "template stylesheet after putref");
    check(node && same((IUnknown *)node, (IUnknown *)sheet), "template stylesheet is the node set");
    r2 = refs((IUnknown *)sheet);
    check(r2 == r1 + 1, "template stylesheet adds a reference");
    if (node) IXMLDOMNode_Release(node);

    hr = IXSLTemplate_createProcessor(tmpl, &proc);
    checkhr(hr, S_OK, "createProcessor");
    if (hr != S_OK) return;

    /* ownerTemplate / stylesheet */
    checkhr(IXSLProcessor_get_ownerTemplate(proc, NULL), E_INVALIDARG, "ownerTemplate(NULL)");
    r1 = refs((IUnknown *)tmpl);
    owner = NULL;
    checkhr(IXSLProcessor_get_ownerTemplate(proc, &owner), S_OK, "ownerTemplate");
    check(owner && same((IUnknown *)owner, (IUnknown *)tmpl), "ownerTemplate is the creating template");
    r2 = refs((IUnknown *)tmpl);
    check(r2 == r1 + 1, "ownerTemplate adds a reference");
    if (owner) IXSLTemplate_Release(owner);
    checkhr(IXSLProcessor_get_stylesheet(proc, NULL), E_INVALIDARG, "processor stylesheet(NULL)");
    node = NULL;
    checkhr(IXSLProcessor_get_stylesheet(proc, &node), S_OK, "processor stylesheet");
    check(node && same((IUnknown *)node, (IUnknown *)sheet), "processor stylesheet is the template's");
    if (node) IXMLDOMNode_Release(node);

    /* ready state: no input, input set, transformed, reset */
    checkhr(IXSLProcessor_get_readyState(proc, NULL), E_INVALIDARG, "readyState(NULL)");
    state = -1;
    checkhr(IXSLProcessor_get_readyState(proc, &state), S_OK, "readyState without input");
    check(state == READYSTATE_LOADING, "readyState without input is LOADING");

    /* input */
    checkhr(IXSLProcessor_get_input(proc, NULL), E_INVALIDARG, "input(NULL)");
    V_VT(&v) = VT_BSTR; V_BSTR(&v) = NULL;
    checkhr(IXSLProcessor_get_input(proc, &v), S_OK, "input before put");
    check(V_VT(&v) == VT_EMPTY, "input before put is VT_EMPTY");

    /* transform without input must fail cleanly */
    ok = VARIANT_TRUE;
    hr = IXSLProcessor_transform(proc, &ok);
    check(FAILED(hr) && ok == VARIANT_FALSE, "transform without input fails");

    V_VT(&v) = VT_UNKNOWN; V_UNKNOWN(&v) = (IUnknown *)input;
    checkhr(IXSLProcessor_put_input(proc, v), S_OK, "put_input(document)");
    VariantInit(&v2);
    checkhr(IXSLProcessor_get_input(proc, &v2), S_OK, "input after put");
    check(V_VT(&v2) == VT_UNKNOWN && same(V_UNKNOWN(&v2), (IUnknown *)input), "input is the document put");
    VariantClear(&v2);
    state = -1;
    IXSLProcessor_get_readyState(proc, &state);
    check(state == READYSTATE_LOADED, "readyState with input is LOADED");

    /* input given as a file name: the processor loads it, get_input returns the document */
    {
        HANDLE f = CreateFileW(L"msxmlstub-in.xml", GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
        DWORD written;
        WriteFile(f, "<x>hi</x>", 9, &written, NULL);
        CloseHandle(f);
    }
    V_VT(&v) = VT_BSTR; V_BSTR(&v) = SysAllocString(L"msxmlstub-in.xml");
    checkhr(IXSLProcessor_put_input(proc, v), S_OK, "put_input(file name)");
    SysFreeString(V_BSTR(&v));
    VariantInit(&v2);
    IXSLProcessor_get_input(proc, &v2);
    s = NULL;
    if (V_VT(&v2) == VT_UNKNOWN && V_UNKNOWN(&v2))
    {
        IXMLDOMNode *n = NULL;
        IUnknown_QueryInterface(V_UNKNOWN(&v2), &IID_IXMLDOMNode, (void **)&n);
        if (n) { IXMLDOMNode_get_text(n, &s); IXMLDOMNode_Release(n); }
        check(n != NULL, "input from file is a node");
    }
    else check(0, "input from file is VT_UNKNOWN");
    check(bstr_is(s, L"hi"), "input from file holds the file's document");
    VariantClear(&v2);
    DeleteFileW(L"msxmlstub-in.xml");
    V_VT(&v) = VT_UNKNOWN; V_UNKNOWN(&v) = (IUnknown *)input;
    IXSLProcessor_put_input(proc, v);

    /* start mode */
    checkhr(IXSLProcessor_get_startMode(proc, NULL), E_INVALIDARG, "startMode(NULL)");
    checkhr(IXSLProcessor_get_startModeURI(proc, NULL), E_INVALIDARG, "startModeURI(NULL)");
    s = s2 = NULL;
    checkhr(IXSLProcessor_get_startMode(proc, &s), S_OK, "startMode initially");
    check(bstr_is(s, L""), "startMode initially empty");
    checkhr(IXSLProcessor_get_startModeURI(proc, &s2), S_OK, "startModeURI initially");
    check(bstr_is(s2, L""), "startModeURI initially empty");
    check(!wcscmp(run(proc), L"default"), "default mode transform");
    state = -1;
    IXSLProcessor_get_readyState(proc, &state);
    check(state == READYSTATE_COMPLETE, "readyState after transform is COMPLETE");

    s = SysAllocString(L"m1");
    checkhr(IXSLProcessor_setStartMode(proc, s, NULL), S_OK, "setStartMode(m1)");
    SysFreeString(s);
    s = s2 = NULL;
    IXSLProcessor_get_startMode(proc, &s);
    IXSLProcessor_get_startModeURI(proc, &s2);
    check(bstr_is(s, L"m1"), "startMode reads back m1");
    check(bstr_is(s2, L""), "startModeURI still empty");
    check(!wcscmp(run(proc), L"mode1"), "transform starts in mode m1");

    s = SysAllocString(L"m2");
    IXSLProcessor_setStartMode(proc, s, NULL);
    SysFreeString(s);
    check(!wcscmp(run(proc), L"mode2"), "transform starts in mode m2");

    /* parameters still apply when a start mode is set */
    s = SysAllocString(L"m1");
    IXSLProcessor_setStartMode(proc, s, NULL);
    SysFreeString(s);
    s = SysAllocString(L"p");
    V_VT(&v) = VT_BSTR; V_BSTR(&v) = SysAllocString(L"V");
    checkhr(IXSLProcessor_addParameter(proc, s, v, NULL), S_OK, "addParameter");
    SysFreeString(s);
    SysFreeString(V_BSTR(&v));
    check(!wcscmp(run(proc), L"mode1V"), "start mode and parameter together");

    /* namespaced mode */
    s = SysAllocString(L"m3");
    s2 = SysAllocString(L"urn:x");
    checkhr(IXSLProcessor_setStartMode(proc, s, s2), S_OK, "setStartMode(m3, urn:x)");
    SysFreeString(s);
    SysFreeString(s2);
    s = NULL;
    IXSLProcessor_get_startModeURI(proc, &s);
    check(bstr_is(s, L"urn:x"), "startModeURI reads back");
    check(!wcscmp(run(proc), L"mode3"), "transform starts in namespaced mode");

    /* reset: drops the result, keeps input / mode / parameters */
    state = -1;
    IXSLProcessor_get_readyState(proc, &state);
    check(state == READYSTATE_COMPLETE, "COMPLETE before reset");
    checkhr(IXSLProcessor_reset(proc), S_OK, "reset");
    state = -1;
    IXSLProcessor_get_readyState(proc, &state);
    check(state == READYSTATE_LOADED, "readyState after reset is LOADED");
    VariantInit(&v2);
    IXSLProcessor_get_output(proc, &v2);
    check(V_VT(&v2) == VT_EMPTY, "output cleared by reset");
    VariantClear(&v2);
    VariantInit(&v2);
    IXSLProcessor_get_input(proc, &v2);
    check(V_VT(&v2) == VT_UNKNOWN, "input kept by reset");
    VariantClear(&v2);
    s = NULL;
    IXSLProcessor_get_startMode(proc, &s);
    check(bstr_is(s, L"m3"), "start mode kept by reset");
    check(!wcscmp(run(proc), L"mode3"), "transform works again after reset");

    /* back to the default mode */
    checkhr(IXSLProcessor_setStartMode(proc, NULL, NULL), S_OK, "setStartMode(NULL, NULL)");
    s = s2 = NULL;
    IXSLProcessor_get_startMode(proc, &s);
    IXSLProcessor_get_startModeURI(proc, &s2);
    check(bstr_is(s, L"") && bstr_is(s2, L""), "start mode cleared");
    check(!wcscmp(run(proc), L"defaultV"), "default mode again, parameter kept");

    /* a second processor from the template starts clean */
    IXSLTemplate_createProcessor(tmpl, &proc2);
    s = NULL;
    IXSLProcessor_get_startMode(proc2, &s);
    check(bstr_is(s, L""), "new processor has no start mode");
    IXSLProcessor_Release(proc2);

    IXSLProcessor_Release(proc);
    IXSLTemplate_Release(tmpl);
    IXMLDOMDocument_Release(sheet);
    IXMLDOMDocument_Release(input);
}

/* ---- specified / parsed on all node types ----------------------------- */

static void check_sp(IUnknown *u, const char *name)
{
    IXMLDOMNode *n = NULL;
    VARIANT_BOOL b;
    char buf[100];
    HRESULT hr;

    IUnknown_QueryInterface(u, &IID_IXMLDOMNode, (void **)&n);
    if (!n) { check(0, name); return; }

    snprintf(buf, sizeof(buf), "%s specified(NULL)", name);
    hr = IXMLDOMNode_get_specified(n, NULL);
    checkhr(hr, E_INVALIDARG, buf);
    snprintf(buf, sizeof(buf), "%s parsed(NULL)", name);
    hr = IXMLDOMNode_get_parsed(n, NULL);
    checkhr(hr, E_INVALIDARG, buf);

    b = VARIANT_FALSE;
    hr = IXMLDOMNode_get_specified(n, &b);
    snprintf(buf, sizeof(buf), "%s specified", name);
    check(hr == S_OK && b == VARIANT_TRUE, buf);
    b = VARIANT_FALSE;
    hr = IXMLDOMNode_get_parsed(n, &b);
    snprintf(buf, sizeof(buf), "%s parsed", name);
    check(hr == S_OK && b == VARIANT_TRUE, buf);
    IXMLDOMNode_Release(n);
}

static void test_nodes(void)
{
    static const WCHAR text[] =
        L"<?xml version=\"1.0\"?><!DOCTYPE r [<!ENTITY e \"x\">]>"
        L"<r a=\"1\"><!--c--><![CDATA[d]]>t<?pi x?></r>";
    IXMLDOMDocument *doc = load(L"Msxml2.DOMDocument.3.0", text);
    IXMLDOMElement *root = NULL;
    IXMLDOMNode *n = NULL, *n2 = NULL;
    IXMLDOMNodeList *kids = NULL;
    IXMLDOMAttribute *attr = NULL;
    IXMLDOMEntityReference *er = NULL;
    IXMLDOMDocumentFragment *frag = NULL;
    IXMLDOMDocumentType *dt = NULL;
    VARIANT_BOOL b;
    VARIANT v;
    BSTR s, name;
    DOMNodeType type;
    HRESULT hr;
    long i, len;
    IXMLDOMNamedNodeMap *map;

    if (!doc) return;
    check_sp((IUnknown *)doc, "document");
    IXMLDOMDocument_get_documentElement(doc, &root);
    check_sp((IUnknown *)root, "element");
    IXMLDOMElement_get_childNodes(root, &kids);
    IXMLDOMNodeList_get_length(kids, &len);
    for (i = 0; i < len; i++)
    {
        IXMLDOMNodeList_get_item(kids, i, &n);
        IXMLDOMNode_get_nodeType(n, &type);
        switch (type)
        {
        case NODE_COMMENT: check_sp((IUnknown *)n, "comment"); break;
        case NODE_CDATA_SECTION: check_sp((IUnknown *)n, "cdata"); break;
        case NODE_TEXT: check_sp((IUnknown *)n, "text"); break;
        case NODE_PROCESSING_INSTRUCTION: check_sp((IUnknown *)n, "pi"); break;
        default: break;
        }
        IXMLDOMNode_Release(n);
    }
    IXMLDOMNodeList_Release(kids);

    IXMLDOMElement_get_attributes(root, &map);
    IXMLDOMNamedNodeMap_get_item(map, 0, &n);
    check_sp((IUnknown *)n, "attribute");
    IXMLDOMNode_Release(n);
    IXMLDOMNamedNodeMap_Release(map);

    name = SysAllocString(L"e");
    hr = IXMLDOMDocument_createEntityReference(doc, name, &er);
    SysFreeString(name);
    if (hr == S_OK) { check_sp((IUnknown *)er, "entity reference"); IXMLDOMEntityReference_Release(er); }
    hr = IXMLDOMDocument_createDocumentFragment(doc, &frag);
    if (hr == S_OK) { check_sp((IUnknown *)frag, "document fragment"); IXMLDOMDocumentFragment_Release(frag); }
    (void)attr; (void)b; (void)v;

    /* the document type node */
    hr = IXMLDOMDocument_get_doctype(doc, &dt);
    checkhr(hr, S_OK, "get_doctype");
    if (hr != S_OK) goto done;
    check_sp((IUnknown *)dt, "doctype");

    VariantInit(&v);
    V_VT(&v) = VT_I4;
    hr = IXMLDOMDocumentType_get_nodeValue(dt, &v);
    check(hr == S_FALSE && V_VT(&v) == VT_NULL, "doctype nodeValue is null");
    checkhr(IXMLDOMDocumentType_get_nodeValue(dt, NULL), E_INVALIDARG, "doctype nodeValue(NULL)");

    n = (void *)0xdeadbeef;
    hr = IXMLDOMDocumentType_get_parentNode(dt, &n);
    check(hr == S_OK && n != NULL, "doctype parentNode");
    type = NODE_INVALID;
    if (n) { IXMLDOMNode_get_nodeType(n, &type); IXMLDOMNode_Release(n); }
    check(type == NODE_DOCUMENT, "doctype parent is the document");
    checkhr(IXMLDOMDocumentType_get_parentNode(dt, NULL), E_INVALIDARG, "doctype parentNode(NULL)");

    n = (void *)0xdeadbeef;
    checkhr(IXMLDOMDocumentType_get_firstChild(dt, &n), S_FALSE, "doctype firstChild");
    check(n == NULL, "doctype firstChild is NULL");
    n = (void *)0xdeadbeef;
    checkhr(IXMLDOMDocumentType_get_lastChild(dt, &n), S_FALSE, "doctype lastChild");
    check(n == NULL, "doctype lastChild is NULL");
    map = (void *)0xdeadbeef;
    checkhr(IXMLDOMDocumentType_get_attributes(dt, &map), S_FALSE, "doctype attributes");
    check(map == NULL, "doctype attributes is NULL");
    b = VARIANT_TRUE;
    checkhr(IXMLDOMDocumentType_hasChildNodes(dt, &b), S_FALSE, "doctype hasChildNodes");
    check(b == VARIANT_FALSE, "doctype hasChildNodes is false");

    n = NULL;
    hr = IXMLDOMDocumentType_get_nextSibling(dt, &n);
    name = NULL;
    if (n) { IXMLDOMNode_get_nodeName(n, &name); IXMLDOMNode_Release(n); }
    check(hr == S_OK && bstr_is(name, L"r"), "doctype nextSibling is the root element");
    /* the XML declaration is the document's first child, a processing instruction */
    n = NULL;
    hr = IXMLDOMDocumentType_get_previousSibling(dt, &n);
    name = NULL;
    if (n) { IXMLDOMNode_get_nodeName(n, &name); IXMLDOMNode_Release(n); }
    check(hr == S_OK && bstr_is(name, L"xml"), "doctype previousSibling is the XML declaration");

    {
        IXMLDOMDocument *owner = NULL;
        hr = IXMLDOMDocumentType_get_ownerDocument(dt, &owner);
        check(hr == S_OK && owner != NULL, "doctype ownerDocument");
        if (owner) IXMLDOMDocument_Release(owner);
        checkhr(IXMLDOMDocumentType_get_ownerDocument(dt, NULL), E_INVALIDARG, "doctype ownerDocument(NULL)");
    }

    s = NULL;
    checkhr(IXMLDOMDocumentType_get_nodeTypeString(dt, &s), S_OK, "doctype nodeTypeString");
    check(bstr_is(s, L"documenttype"), "doctype nodeTypeString is documenttype");
    s = NULL;
    checkhr(IXMLDOMDocumentType_get_text(dt, &s), S_OK, "doctype text");
    check(bstr_is(s, L""), "doctype text is empty");
    s = NULL;
    hr = IXMLDOMDocumentType_get_xml(dt, &s);
    check(hr == S_OK && s && wcsstr(s, L"<!DOCTYPE r") != NULL, "doctype xml contains the declaration");
    SysFreeString(s);
    s = (void *)0xdeadbeef;
    checkhr(IXMLDOMDocumentType_get_namespaceURI(dt, &s), S_FALSE, "doctype namespaceURI");
    check(s == NULL, "doctype namespaceURI is NULL");
    s = (void *)0xdeadbeef;
    checkhr(IXMLDOMDocumentType_get_prefix(dt, &s), S_FALSE, "doctype prefix");
    check(s == NULL, "doctype prefix is NULL");
    s = NULL;
    checkhr(IXMLDOMDocumentType_get_baseName(dt, &s), S_OK, "doctype baseName");
    check(bstr_is(s, L"r"), "doctype baseName is the name");
    IXMLDOMDocumentType_Release(dt);
done:
    (void)n2;
    IXMLDOMElement_Release(root);
    IXMLDOMDocument_Release(doc);
}

int main(void)
{
    CoInitialize(NULL);
    test_xsl();
    test_nodes();
    CoUninitialize();
    printf("%d checks failed\n", failures);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
