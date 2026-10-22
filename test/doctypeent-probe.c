/* doctypeent-probe: MSXML3 IXMLDOMDocumentType's entities and notations (Stained Glass OS
 * wine-sg 6018): IXMLDOMEntity (publicId, systemId, notationName), IXMLDOMNotation, the
 * read-only named node maps.   doctypeent-probe.exe */
#define COBJMACROS
#include <windows.h>
#include <msxml2.h>
#include <stdio.h>

static const GUID CLSID_DOMDocument30_ = {0xf5078f32,0xc551,0x11d3,{0x89,0xb9,0x00,0x00,0xf8,0x1f,0xe2,0x21}};

static int failures;

static void check( int ok, const char *what, long got )
{
    if (ok) printf( "ok    %s\n", what );
    else { printf( "FAIL  %s (got %#lx)\n", what, got ); failures++; }
}

static const WCHAR xml[] =
    L"<?xml version=\"1.0\"?>\n"
    L"<!DOCTYPE root [\n"
    L"<!NOTATION gif PUBLIC \"-//SG//gif\" \"viewer.exe\">\n"
    L"<!NOTATION png SYSTEM \"png.exe\">\n"
    L"<!ENTITY copy \"(c) SG\">\n"
    L"<!ENTITY ext SYSTEM \"ext.xml\">\n"
    L"<!ENTITY pic SYSTEM \"pic.gif\" NDATA gif>\n"
    L"<!ENTITY % param \"x\">\n"
    L"<!ELEMENT root ANY>\n"
    L"]>\n"
    L"<root>&copy;</root>\n";

static BOOL str_is( BSTR s, const WCHAR *expect )
{
    BOOL ret = s && !lstrcmpW( s, expect );
    SysFreeString( s );
    return ret;
}

static BOOL var_is( VARIANT *v, const WCHAR *expect )
{
    BOOL ret = expect ? V_VT(v) == VT_BSTR && !lstrcmpW( V_BSTR(v), expect ) : V_VT(v) == VT_NULL;
    VariantClear( v );
    return ret;
}

static IXMLDOMNode *named( IXMLDOMNamedNodeMap *map, const WCHAR *name )
{
    IXMLDOMNode *node = NULL;
    BSTR b = SysAllocString( name );
    IXMLDOMNamedNodeMap_getNamedItem( map, b, &node );
    SysFreeString( b );
    return node;
}

int main( void )
{
    IXMLDOMDocument *doc;
    IXMLDOMDocumentType *dt = NULL;
    IXMLDOMNamedNodeMap *ents = NULL, *notas = NULL;
    IXMLDOMNode *node, *parent;
    IXMLDOMEntity *ent;
    IXMLDOMNotation *nota;
    VARIANT_BOOL ok;
    DOMNodeType type;
    VARIANT v;
    BSTR b;
    LONG len = -1, n;
    HRESULT hr;

    CoInitialize( NULL );
    if (CoCreateInstance( &CLSID_DOMDocument30_, NULL, CLSCTX_INPROC_SERVER, &IID_IXMLDOMDocument, (void **)&doc ) != S_OK)
    {
        printf( "FAIL  no DOMDocument30\nRESULT: FAIL\n" );
        return 1;
    }
    IXMLDOMDocument_put_resolveExternals( doc, VARIANT_FALSE );
    IXMLDOMDocument_put_validateOnParse( doc, VARIANT_FALSE );
    b = SysAllocString( xml );
    hr = IXMLDOMDocument_loadXML( doc, b, &ok );
    SysFreeString( b );
    check( hr == S_OK && ok, "the document", hr );
    hr = IXMLDOMDocument_get_doctype( doc, &dt );
    check( hr == S_OK && dt, "its document type", hr );
    if (!dt) { printf( "RESULT: FAIL\n" ); return 1; }

    /* entities */
    hr = IXMLDOMDocumentType_get_entities( dt, &ents );
    check( hr == S_OK && ents, "get_entities", hr );
    if (ents)
    {
        IXMLDOMNamedNodeMap_get_length( ents, &len );
        check( len == 3, "three general entities (not the parameter entity)", len );
        node = NULL;
        IXMLDOMNamedNodeMap_get_item( ents, 0, &node );
        check( node && IXMLDOMNode_get_nodeName( node, &b ) == S_OK && str_is( b, L"copy" ),
               "the first is copy, in declaration order", 0 );
        if (node)
        {
            IXMLDOMNode_get_nodeType( node, &type );
            check( type == NODE_ENTITY, "NODE_ENTITY", type );
            IXMLDOMNode_get_nodeTypeString( node, &b );
            check( str_is( b, L"entity" ), "nodeTypeString entity", 0 );
            hr = IXMLDOMNode_QueryInterface( node, &IID_IXMLDOMEntity, (void **)&ent );
            check( hr == S_OK, "IXMLDOMEntity", hr );
            if (hr == S_OK)
            {
                VariantInit( &v );
                IXMLDOMEntity_get_publicId( ent, &v );
                check( var_is( &v, NULL ), "an internal entity has no publicId", 0 );
                IXMLDOMEntity_get_systemId( ent, &v );
                check( var_is( &v, NULL ), "nor a systemId", 0 );
                IXMLDOMEntity_Release( ent );
            }
            parent = (IXMLDOMNode *)1;
            hr = IXMLDOMNode_get_parentNode( node, &parent );
            check( hr == S_FALSE && !parent, "no parent node", hr );
            IXMLDOMNode_Release( node );
        }
        node = named( ents, L"ext" );
        check( node != NULL, "getNamedItem ext", 0 );
        if (node && IXMLDOMNode_QueryInterface( node, &IID_IXMLDOMEntity, (void **)&ent ) == S_OK)
        {
            VariantInit( &v );
            IXMLDOMEntity_get_systemId( ent, &v );
            check( var_is( &v, L"ext.xml" ), "its systemId", 0 );
            b = (BSTR)1;
            IXMLDOMEntity_get_notationName( ent, &b );
            check( !b, "a parsed entity has no notation", 0 );
            IXMLDOMEntity_Release( ent );
        }
        if (node) IXMLDOMNode_Release( node );
        node = named( ents, L"pic" );
        if (node && IXMLDOMNode_QueryInterface( node, &IID_IXMLDOMEntity, (void **)&ent ) == S_OK)
        {
            IXMLDOMEntity_get_notationName( ent, &b );
            check( str_is( b, L"gif" ), "an unparsed entity's notationName (NDATA)", 0 );
            IXMLDOMEntity_Release( ent );
        }
        else check( 0, "getNamedItem pic", 0 );
        if (node) IXMLDOMNode_Release( node );
        check( named( ents, L"param" ) == NULL, "the parameter entity is not there", 0 );
        b = SysAllocString( L"copy" );
        node = NULL;
        hr = IXMLDOMNamedNodeMap_removeNamedItem( ents, b, &node );
        SysFreeString( b );
        check( FAILED(hr), "the map is read-only", hr );
        n = 0;
        IXMLDOMNamedNodeMap_reset( ents );
        while (IXMLDOMNamedNodeMap_nextNode( ents, &node ) == S_OK && node && n < 10)
        {
            n++;
            IXMLDOMNode_Release( node );
        }
        check( n == 3, "nextNode goes over the three", n );
        IXMLDOMNamedNodeMap_Release( ents );
    }

    /* notations */
    hr = IXMLDOMDocumentType_get_notations( dt, &notas );
    check( hr == S_OK && notas, "get_notations", hr );
    if (notas)
    {
        IXMLDOMNamedNodeMap_get_length( notas, &len );
        check( len == 2, "two notations", len );
        node = named( notas, L"gif" );
        check( node != NULL, "getNamedItem gif", 0 );
        if (node)
        {
            IXMLDOMNode_get_nodeType( node, &type );
            check( type == NODE_NOTATION, "NODE_NOTATION", type );
            hr = IXMLDOMNode_QueryInterface( node, &IID_IXMLDOMNotation, (void **)&nota );
            check( hr == S_OK, "IXMLDOMNotation", hr );
            if (hr == S_OK)
            {
                VariantInit( &v );
                IXMLDOMNotation_get_publicId( nota, &v );
                check( var_is( &v, L"-//SG//gif" ), "its publicId", 0 );
                IXMLDOMNotation_get_systemId( nota, &v );
                check( var_is( &v, L"viewer.exe" ), "its systemId", 0 );
                IXMLDOMNotation_Release( nota );
            }
            IXMLDOMNode_get_xml( node, &b );
            check( str_is( b, L"<!NOTATION gif PUBLIC \"-//SG//gif\" \"viewer.exe\">" ), "its xml: the declaration", 0 );
            IXMLDOMNode_Release( node );
        }
        node = named( notas, L"png" );
        if (node && IXMLDOMNode_QueryInterface( node, &IID_IXMLDOMNotation, (void **)&nota ) == S_OK)
        {
            VariantInit( &v );
            IXMLDOMNotation_get_publicId( nota, &v );
            check( var_is( &v, NULL ), "png: no publicId", 0 );
            IXMLDOMNotation_get_systemId( nota, &v );
            check( var_is( &v, L"png.exe" ), "png: its systemId", 0 );
            IXMLDOMNotation_Release( nota );
        }
        else check( 0, "getNamedItem png", 0 );
        if (node) IXMLDOMNode_Release( node );
        node = NULL;
        IXMLDOMNamedNodeMap_get_item( notas, 5, &node );
        check( !node, "an item past the end is NULL", 0 );
        IXMLDOMNamedNodeMap_Release( notas );
    }

    IXMLDOMDocumentType_Release( dt );
    IXMLDOMDocument_Release( doc );
    printf( "RESULT: %s\n", failures ? "FAIL" : "PASS" );
    return failures != 0;
}
