/* Windows.Data.Xml.Dom gives back the node a call made (patches/sg/6545).
 * windows.ui wrapped the MSXML node an AppendChild (InsertBefore, CloneNode,
 * parent/child/sibling getters, prefix/namespace getters, SetNamedItem...)
 * returned with the call in the wrapper's own argument list, so the node was
 * read before the call filled it: AppendChild succeeded and returned no node.
 * SnoreToast (KDE's notification helper: digiKam, Kdenlive...) appends <audio>
 * to a toast template and uses the node at once: it crashed. The probe does
 * what SnoreToast does, then the other calls of that kind. */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <winstring.h>
#include <roapi.h>
#include <stdio.h>
#define WIDL_using_Windows_Foundation
#include "windows.foundation.h"
#define WIDL_using_Windows_Data_Xml_Dom
#include "windows.data.xml.dom.h"
#define WIDL_using_Windows_UI_Notifications
#include "windows.ui.notifications.h"

static HSTRING hs( const WCHAR *s ) { HSTRING r = NULL; WindowsCreateString( s, lstrlenW( s ), &r ); return r; }

int main( void )
{
    IToastNotificationManagerStatics *manager = NULL;
    IXmlNode *root = NULL, *tmp = NULL, *appended = NULL, *node = NULL, *clone = NULL;
    IXmlNamedNodeMap *attributes = NULL;
    IXmlElement *audio = NULL;
    IXmlNodeList *list = NULL;
    IXmlDocument *xml = NULL;
    IInspectable *name_value = NULL;
    HSTRING s;
    HRESULT hr;
    int ok_parent = 0, ok_child = 0, ok_clone = 0, ok_insert = 0;

    RoInitialize( RO_INIT_MULTITHREADED );
    s = hs( L"Windows.UI.Notifications.ToastNotificationManager" );
    hr = RoGetActivationFactory( s, &IID_IToastNotificationManagerStatics, (void **)&manager );
    WindowsDeleteString( s );
    if (FAILED(hr)) { printf( "no toast manager %#lx\n", hr ); return 1; }
    hr = IToastNotificationManagerStatics_GetTemplateContent( manager, ToastTemplateType_ToastText02, &xml );
    printf( "template=%d\n", SUCCEEDED(hr) && xml );
    if (!xml) return 1;
    s = hs( L"toast" );
    IXmlDocument_GetElementsByTagName( xml, s, &list );
    WindowsDeleteString( s );
    if (list) IXmlNodeList_Item( list, 0, &root );
    if (!root) { printf( "no toast element\n" ); return 1; }

    /* SnoreToast */
    s = hs( L"audio" );
    IXmlDocument_CreateElement( xml, s, &audio );
    WindowsDeleteString( s );
    IXmlElement_QueryInterface( audio, &IID_IXmlNode, (void **)&tmp );
    hr = IXmlNode_AppendChild( root, tmp, &appended );
    printf( "append_returns_node=%d (hr %#lx)\n", SUCCEEDED(hr) && appended, hr );
    if (appended)
    {
        hr = IXmlNode_get_Attributes( appended, &attributes );
        printf( "appended_usable=%d\n", SUCCEEDED(hr) && attributes );
        if (attributes) IXmlNamedNodeMap_Release( attributes );

        /* the others of that kind */
        if (SUCCEEDED(IXmlNode_get_ParentNode( appended, &node )) && node) { ok_parent = 1; IXmlNode_Release( node ); node = NULL; }
        if (SUCCEEDED(IXmlNode_get_LastChild( root, &node )) && node) { ok_child = 1; IXmlNode_Release( node ); node = NULL; }
        if (SUCCEEDED(IXmlNode_CloneNode( appended, TRUE, &clone )) && clone)
        {
            ok_clone = 1;
            if (SUCCEEDED(IXmlNode_InsertBefore( root, clone, appended, &node )) && node) { ok_insert = 1; IXmlNode_Release( node ); }
            IXmlNode_Release( clone );
        }
        if (SUCCEEDED(IXmlNode_get_NodeName( appended, &s ))) WindowsDeleteString( s );
        IXmlNode_get_LocalName( appended, &name_value );
        printf( "parent=%d last_child=%d clone=%d insert=%d local_name=%d\n", ok_parent, ok_child, ok_clone, ok_insert,
                name_value != NULL );
        if (name_value) IInspectable_Release( name_value );
        IXmlNode_Release( appended );
    }
    printf( "done=1\n" );
    return 0;
}
