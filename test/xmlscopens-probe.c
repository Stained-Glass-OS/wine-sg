/* xmlscopens-gate.sh's probe (7820): an element's xml declares the namespaces
 * it uses that are declared above it. Prints "NAME VALUE" lines. */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <msxml6.h>
#include <stdio.h>

static IXMLDOMDocument2 *newdoc(void)
{
    IXMLDOMDocument2 *d;
    VARIANT v;

    CoCreateInstance(&CLSID_DOMDocument60, NULL, CLSCTX_INPROC_SERVER, &IID_IXMLDOMDocument2, (void **)&d);
    V_VT(&v) = VT_BSTR;
    V_BSTR(&v) = SysAllocString(L"xmlns:a='http://schemas.microsoft.com/appx/2010/manifest' "
                                L"xmlns:appv='http://schemas.microsoft.com/appv/2010/manifest'");
    IXMLDOMDocument2_setProperty(d, SysAllocString(L"SelectionNamespaces"), v);
    return d;
}

static void show(const char *name, IXMLDOMDocument2 *d, const WCHAR *query)
{
    IXMLDOMNode *n = NULL;
    BSTR xml = NULL;

    IXMLDOMDocument2_selectSingleNode(d, SysAllocString(query), &n);
    if (n) IXMLDOMNode_get_xml(n, &xml);
    if (xml)
    {
        /* one line: msxml's formatting is not what is checked */
        WCHAR *r, *w;
        for (r = w = xml; *r; r++) if (*r != '\r' && *r != '\n' && *r != '\t') *w++ = *r;
        *w = 0;
    }
    printf("%s %ls\n", name, xml ? xml : L"(none)");
}

static long count(IXMLDOMDocument2 *d, const WCHAR *query)
{
    IXMLDOMNodeList *l = NULL;
    long n = -1;

    if (IXMLDOMDocument2_selectNodes(d, SysAllocString(query), &l) == S_OK)
        IXMLDOMNodeList_get_length(l, &n);
    return n;
}

int wmain(void)
{
    IXMLDOMDocument2 *d, *d2;
    IXMLDOMNode *ext = NULL;
    VARIANT_BOOL ok = VARIANT_FALSE;
    BSTR xml = NULL, before = NULL, after = NULL;

    CoInitialize(NULL);
    d = newdoc();
    /* the shape of an Office Click-to-Run package manifest */
    IXMLDOMDocument2_loadXML(d, SysAllocString(
        L"<Package xmlns='http://schemas.microsoft.com/appx/2010/manifest' "
        L"xmlns:appv='http://schemas.microsoft.com/appv/2010/manifest' xmlns:unused='urn:unused'>"
        L"<Properties><appv:Desc>x</appv:Desc></Properties>"
        L"<appv:Extensions><appv:Extension Category='AppV.Shortcut'><appv:Shortcut>"
        L"<appv:File>[{Common Programs}]\\Word.lnk</appv:File></appv:Shortcut></appv:Extension>"
        L"<appv:Extension Category='AppV.COM'/></appv:Extensions>"
        L"<Applications xmlns='http://schemas.microsoft.com/appv/2010/manifest'><Application Id='Word'/></Applications>"
        L"<Empty xmlns=''/></Package>"), &ok);
    printf("loaded %d\n", ok == VARIANT_TRUE);

    IXMLDOMDocument2_get_xml(d, &before);
    show("extensions", d, L"/a:Package/appv:Extensions");
    show("properties", d, L"/a:Package/a:Properties");
    show("applications", d, L"/a:Package/appv:Applications");
    show("application", d, L"/a:Package/appv:Applications/appv:Application");
    show("shortcutfile", d, L"//appv:File");
    show("empty", d, L"/a:Package/Empty");
    IXMLDOMDocument2_get_xml(d, &after);
    printf("doc_unchanged %d\n", before && after && !wcscmp(before, after));

    /* what Click-to-Run does with it: the extensions' xml into a document of their own */
    IXMLDOMDocument2_selectSingleNode(d, SysAllocString(L"/a:Package/appv:Extensions"), &ext);
    if (ext) IXMLDOMNode_get_xml(ext, &xml);
    d2 = newdoc();
    ok = VARIANT_FALSE;
    if (xml) IXMLDOMDocument2_loadXML(d2, xml, &ok);
    printf("reloaded %d\n", ok == VARIANT_TRUE);
    printf("reloaded_extensions %ld\n", count(d2, L"/appv:Extensions/appv:Extension"));
    printf("reloaded_shortcuts %ld\n", count(d2, L"//appv:Extension[@Category='AppV.Shortcut']/appv:Shortcut/appv:File"));
    return 0;
}
