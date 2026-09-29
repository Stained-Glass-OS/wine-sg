/* msxml-replace-gate.sh's probe (0490): removeChild of a node replaceChild
 * took out of the same parent. Prints one line per step: "NAME HR". */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <msxml6.h>
#include <stdio.h>

static IXMLDOMDocument2 *doc;

static IXMLDOMNode *elem(const WCHAR *name)
{
    IXMLDOMElement *e;
    IXMLDOMDocument2_createElement(doc, SysAllocString(name), &e);
    return (IXMLDOMNode *)e;
}

int wmain(void)
{
    IXMLDOMNode *p, *q, *a, *b, *x, *out;
    long n;
    IXMLDOMNodeList *kids;

    CoInitialize(NULL);
    CoCreateInstance(&CLSID_DOMDocument60, NULL, CLSCTX_INPROC_SERVER, &IID_IXMLDOMDocument2, (void **)&doc);
    p = elem(L"p"); q = elem(L"q"); a = elem(L"a"); b = elem(L"b"); x = elem(L"x");
    IXMLDOMNode_appendChild(p, a, NULL);
    IXMLDOMNode_appendChild(p, x, NULL);
    printf("replace %08lx\n", IXMLDOMNode_replaceChild(p, b, a, &out));
    printf("remove-replaced %08lx\n", IXMLDOMNode_removeChild(p, a, &out));
    printf("remove-replaced-out %d\n", out == a);
    IXMLDOMNode_get_childNodes(p, &kids); IXMLDOMNodeList_get_length(kids, &n);
    printf("children %ld\n", n);
    printf("remove-from-other %08lx\n", IXMLDOMNode_removeChild(q, a, NULL));
    printf("remove-child %08lx\n", IXMLDOMNode_removeChild(p, x, NULL));
    printf("remove-removed %08lx\n", IXMLDOMNode_removeChild(p, x, NULL));
    IXMLDOMNode_appendChild(q, a, NULL);
    printf("remove-moved %08lx\n", IXMLDOMNode_removeChild(p, a, NULL));
    return 0;
}
