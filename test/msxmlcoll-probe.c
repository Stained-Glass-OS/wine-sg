/* msxml3 batch (patches/sg/2031), run by test/msxmlcoll-gate.sh. Families
 * (from Wine's todo_wine blocks, which record Windows): the version 6
 * document properties NormalizeAttributeValues and MaxElementDepth kept
 * between set and get, and the IDispatchEx default item of node lists and
 * attribute maps (parameter count, empty result on every path).
 *
 *   msxmlcoll-probe.exe */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <ocidl.h>
#include <msxml6.h>
#include <dispex.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

static const GUID SG_CLSID_DOMDocument60 = { 0x88d96a05, 0xf192, 0x11d4, { 0xa6, 0x5f, 0x00, 0x40, 0x96, 0x32, 0x51, 0xe5 } };
static const GUID SG_CLSID_DOMDocument30 = { 0xf5078f32, 0xc551, 0x11d3, { 0x89, 0xb9, 0x00, 0x00, 0xf8, 0x1f, 0xe2, 0x21 } };
#define DISPID_DOM_NODELIST_LENGTH 0x0000004a
static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

static void test_properties(void)
{
    IXMLDOMDocument2 *doc = NULL;
    VARIANT v;
    HRESULT hr;
    BSTR name;

    CoCreateInstance(&SG_CLSID_DOMDocument60, NULL, CLSCTX_INPROC_SERVER, &IID_IXMLDOMDocument2, (void **)&doc);
    if (!doc) { check(0, "DOMDocument60 created"); return; }

    name = SysAllocString(L"NormalizeAttributeValues");
    V_VT(&v) = VT_I2; V_I2(&v) = 10;
    hr = IXMLDOMDocument2_getProperty(doc, name, &v);
    check(hr == S_OK && V_VT(&v) == VT_BOOL && V_BOOL(&v) == VARIANT_FALSE, "NormalizeAttributeValues defaults to false");
    V_VT(&v) = VT_BOOL; V_BOOL(&v) = VARIANT_TRUE;
    check(IXMLDOMDocument2_setProperty(doc, name, v) == S_OK, "set NormalizeAttributeValues");
    V_VT(&v) = VT_I2; V_I2(&v) = 10;
    hr = IXMLDOMDocument2_getProperty(doc, name, &v);
    check(hr == S_OK && V_VT(&v) == VT_BOOL && V_BOOL(&v) == VARIANT_TRUE, "NormalizeAttributeValues reads back true");
    SysFreeString(name);

    name = SysAllocString(L"MaxElementDepth");
    V_VT(&v) = VT_UI4; V_UI4(&v) = 0xdeadbeef;
    hr = IXMLDOMDocument2_getProperty(doc, name, &v);
    check(hr == S_OK && V_VT(&v) == VT_I4 && V_I4(&v) == 256, "MaxElementDepth defaults to 256");
    V_VT(&v) = VT_I4; V_I4(&v) = 32;
    check(IXMLDOMDocument2_setProperty(doc, name, v) == S_OK, "set MaxElementDepth");
    V_VT(&v) = VT_UI4; V_UI4(&v) = 0xdeadbeef;
    hr = IXMLDOMDocument2_getProperty(doc, name, &v);
    check(hr == S_OK && V_VT(&v) == VT_I4 && V_I4(&v) == 32, "MaxElementDepth reads back 32");
    V_VT(&v) = VT_I2; V_I2(&v) = 7;
    IXMLDOMDocument2_setProperty(doc, name, v);
    V_VT(&v) = VT_EMPTY;
    IXMLDOMDocument2_getProperty(doc, name, &v);
    check(V_VT(&v) == VT_I4 && V_I4(&v) == 7, "MaxElementDepth takes a short");
    SysFreeString(name);
    IXMLDOMDocument2_Release(doc);
}

static HRESULT invoke(IDispatchEx *dispex, DISPID id, WORD flags, int cargs, VARIANT *ret)
{
    VARIANT arg;
    DISPPARAMS dp;
    V_VT(&arg) = VT_I4; V_I4(&arg) = 0;
    dp.cArgs = cargs; dp.cNamedArgs = 0; dp.rgdispidNamedArgs = NULL; dp.rgvarg = cargs ? &arg : NULL;
    V_VT(ret) = VT_EMPTY;
    V_DISPATCH(ret) = (void *)0x1;
    return IDispatchEx_Invoke(dispex, id, &IID_NULL, 0, flags, &dp, ret, NULL, NULL);
}

static void test_collection(IDispatchEx *dispex, const char *name)
{
    VARIANT ret;
    HRESULT hr;
    char buf[200];

    hr = invoke(dispex, DISPID_VALUE, DISPATCH_METHOD, 0, &ret);
    snprintf(buf, sizeof(buf), "%s: item() without an index is a bad parameter count", name);
    check(hr == DISP_E_BADPARAMCOUNT && V_VT(&ret) == VT_EMPTY && V_DISPATCH(&ret) == NULL, buf);
    hr = invoke(dispex, DISPID_VALUE, DISPATCH_METHOD, 2, &ret);
    snprintf(buf, sizeof(buf), "%s: item(a, b) is a bad parameter count", name);
    check(hr == DISP_E_BADPARAMCOUNT && V_VT(&ret) == VT_EMPTY && V_DISPATCH(&ret) == NULL, buf);
    hr = invoke(dispex, DISPID_VALUE, DISPATCH_METHOD, 1, &ret);
    snprintf(buf, sizeof(buf), "%s: item(0) of an empty collection is a null object", name);
    check(hr == S_OK && V_VT(&ret) == VT_DISPATCH && V_DISPATCH(&ret) == NULL, buf);
    hr = invoke(dispex, DISPID_VALUE, DISPATCH_PROPERTYGET, 1, &ret);
    snprintf(buf, sizeof(buf), "%s: property get of the default item", name);
    check(hr == S_OK && V_VT(&ret) == VT_DISPATCH && V_DISPATCH(&ret) == NULL, buf);
    hr = invoke(dispex, DISPID_DOM_NODELIST_LENGTH, DISPATCH_METHOD, 0, &ret);
    snprintf(buf, sizeof(buf), "%s: length called as a method is not found and leaves nothing", name);
    check(hr == DISP_E_MEMBERNOTFOUND && V_VT(&ret) == VT_EMPTY && V_I4(&ret) == 0, buf);
    hr = invoke(dispex, DISPID_DOM_NODELIST_LENGTH, DISPATCH_PROPERTYGET, 0, &ret);
    snprintf(buf, sizeof(buf), "%s: length as a property", name);
    check(hr == S_OK && V_VT(&ret) == VT_I4 && V_I4(&ret) == 0, buf);
}

static void test_dispex(void)
{
    IXMLDOMDocument2 *doc = NULL;
    IXMLDOMNodeList *list = NULL;
    IXMLDOMNamedNodeMap *map = NULL;
    IXMLDOMNode *node = NULL;
    IDispatchEx *dispex = NULL;
    VARIANT_BOOL ok;
    BSTR text = SysAllocString(L"<root><b/></root>"), path = SysAllocString(L"root/b");

    CoCreateInstance(&SG_CLSID_DOMDocument30, NULL, CLSCTX_INPROC_SERVER, &IID_IXMLDOMDocument2, (void **)&doc);
    IXMLDOMDocument2_loadXML(doc, text, &ok);

    IXMLDOMDocument2_selectNodes(doc, path, &list);
    IXMLDOMNodeList_get_item(list, 0, &node);
    IXMLDOMNodeList_Release(list);
    IXMLDOMNode_get_attributes(node, &map);
    IXMLDOMNamedNodeMap_QueryInterface(map, &IID_IDispatchEx, (void **)&dispex);
    test_collection(dispex, "attribute map");
    IDispatchEx_Release(dispex);
    IXMLDOMNamedNodeMap_Release(map);
    IXMLDOMNode_Release(node);

    IXMLDOMNode *root = NULL;
    IXMLDOMDocument2_selectNodes(doc, path, &list);
    IXMLDOMNodeList_get_item(list, 0, &root);
    IXMLDOMNodeList_Release(list);
    IXMLDOMNode_get_childNodes(root, &list);
    IXMLDOMNode_Release(root);
    IXMLDOMNodeList_QueryInterface(list, &IID_IDispatchEx, (void **)&dispex);
    test_collection(dispex, "node list");
    IDispatchEx_Release(dispex);
    IXMLDOMNodeList_Release(list);
    IXMLDOMDocument2_Release(doc);
}

int main(void)
{
    CoInitialize(NULL);
    test_properties();
    test_dispex();
    printf("%d checks failed\n", failures);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    CoUninitialize();
    return failures != 0;
}
