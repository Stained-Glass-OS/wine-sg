/* coreinputview-probe: Windows.UI.ViewManagement.Core.CoreInputView (patches/sg/6210).
 *
 * The class did not exist: React Native for Windows (OneDrive) asks for it when it makes a text
 * input, and the class_not_registered it got stopped its views being shown. One view per thread,
 * held weakly too; no input panel occludes it and none is shown or hidden; events accepted.
 * Interface identity per windows-rs 0.30.0 (MIT).
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <winstring.h>
#include <roapi.h>
#include <inspectable.h>
#include <stdio.h>

DEFINE_GUID(IID_Statics, 0x7d9b97cd, 0xedbe, 0x49cf, 0xa5, 0x4f, 0x33, 0x7d, 0xe0, 0x52, 0x90, 0x7f);
DEFINE_GUID(IID_Statics2, 0x7ebc0862, 0xd049, 0x4e52, 0x87, 0xb0, 0x1e, 0x90, 0xe9, 0x8c, 0x49, 0xed);
DEFINE_GUID(IID_View, 0xc770cd7a, 0x7001, 0x4c32, 0xbf, 0x94, 0x25, 0xc1, 0xf5, 0x54, 0xcb, 0xf1);
DEFINE_GUID(IID_View3, 0xbc941653, 0x3ab9, 0x4849, 0x8f, 0x58, 0x46, 0xe7, 0xf0, 0x35, 0x3c, 0xfc);
DEFINE_GUID(IID_WeakSource, 0x00000038, 0x0000, 0x0000, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46);

typedef struct { void **vtbl; } obj;
#define CALL(o, n, type) ((type)((o)->vtbl[n]))

static obj *as( void *o, REFIID iid ) { obj *r = NULL; if (o) CALL((obj *)o, 0, HRESULT (WINAPI *)(obj *, REFIID, void **))( o, iid, (void **)&r ); return r; }
static obj *statics;
static obj *other;

static DWORD WINAPI other_thread( void *arg )
{
    CALL(statics, 6, HRESULT (WINAPI *)(obj *, obj **))( statics, &other );
    return 0;
}

/* an event handler: never called here */
static HRESULT WINAPI h_qi( void *h, REFIID iid, void **out ) { *out = h; return S_OK; }
static ULONG WINAPI h_ref( void *h ) { return 2; }
static void *h_vtbl[] = { h_qi, h_ref, h_ref, h_qi };
static struct { void **vtbl; } handler = { h_vtbl };

int main( void )
{
    obj *view = NULL, *again = NULL, *by_context = NULL, *list = NULL, *v3, *source, *weak = NULL, *back = NULL, *s2;
    boolean shown = TRUE, hidden = TRUE, shown3 = TRUE;
    UINT32 size = 99;
    INT64 token = 0;
    HANDLE thread;
    HSTRING name;
    HRESULT hr;

    RoInitialize( RO_INIT_MULTITHREADED );
    WindowsCreateString( L"Windows.UI.ViewManagement.Core.CoreInputView", 44, &name );
    hr = RoGetActivationFactory( name, &IID_Statics, (void **)&statics );
    printf( "statics=%08lx\n", hr );
    if (!statics) return 1;
    hr = CALL(statics, 6, HRESULT (WINAPI *)(obj *, obj **))( statics, &view );   /* GetForCurrentView */
    printf( "view=%08lx\n", hr );
    if (!view) return 1;
    CALL(statics, 6, HRESULT (WINAPI *)(obj *, obj **))( statics, &again );
    thread = CreateThread( NULL, 0, other_thread, NULL, 0, NULL );
    WaitForSingleObject( thread, 5000 );
    if ((s2 = as( statics, &IID_Statics2 ))) CALL(s2, 6, HRESULT (WINAPI *)(obj *, void *, obj **))( s2, NULL, &by_context );
    printf( "same=%d other=%d context=%d\n", again == view, other && other != view, by_context == view );

    CALL(view, 8, HRESULT (WINAPI *)(obj *, obj **))( view, &list );   /* GetCoreInputViewOcclusions */
    if (list) CALL(list, 7, HRESULT (WINAPI *)(obj *, UINT32 *))( list, &size );
    printf( "occlusions=%d,%u\n", list != NULL, size );
    CALL(view, 9, HRESULT (WINAPI *)(obj *, boolean *))( view, &shown );
    CALL(view, 10, HRESULT (WINAPI *)(obj *, boolean *))( view, &hidden );
    if ((v3 = as( view, &IID_View3 ))) CALL(v3, 6, HRESULT (WINAPI *)(obj *, boolean *))( v3, &shown3 );
    printf( "show=%d hide=%d show3=%d\n", shown, hidden, shown3 );
    hr = CALL(view, 6, HRESULT (WINAPI *)(obj *, void *, INT64 *))( view, &handler, &token );   /* OcclusionsChanged */
    printf( "event=%08lx,%d\n", hr, token != 0 );

    if ((source = as( view, &IID_WeakSource ))) CALL(source, 3, HRESULT (WINAPI *)(obj *, obj **))( source, &weak );
    if (weak) CALL(weak, 3, HRESULT (WINAPI *)(obj *, REFIID, obj **))( weak, &IID_View, &back );
    printf( "weak=%d,%d\n", weak != NULL, back == view );
    printf( "done=1\n" );
    return 0;
}
