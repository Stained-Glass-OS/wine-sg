/* appview3-probe: Windows.ApplicationModel.Core.CoreApplicationView 3 and 5, its title bar, and
 * Windows.ApplicationModel.DesignMode (patches/sg/6196).
 *
 *  - CoreApplication.GetCurrentView() answers ICoreApplicationView3: not a component; its
 *    TitleBar (the same object each time) has no system title bar to measure (Height and insets
 *    0, not visible), keeps ExtendViewIntoTitleBar, takes LayoutMetricsChanged/IsVisibleChanged
 *    handlers and gives a weak reference that resolves to it (event revokers take one);
 *  - it answers ICoreApplicationView5: its Properties is a property set, the same each time, not
 *    CoreApplication.Properties;
 *  - DesignMode: DesignModeEnabled and DesignMode2Enabled are FALSE (a program runs for real).
 *
 * The interfaces are called through their vtable slots; their IIDs are the public ones.
 * Prints name=value lines; see test/appview3-gate.sh.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <roapi.h>
#include <winstring.h>
#include <stdio.h>

#define SLOT(obj, n, type) ((type)((*(void ***)(obj))[n]))

DEFINE_GUID(probe_IID_ICoreApplication, 0x0aacf7a4, 0x5e1d, 0x49df, 0x80, 0x34, 0xfb, 0x6a, 0x68, 0xbc, 0x5e, 0xd1);
DEFINE_GUID(probe_IID_ICoreApplicationView3, 0x07ebe1b3, 0xa4cf, 0x4550, 0xab, 0x70, 0xb0, 0x7e, 0x85, 0x33, 0x0b, 0xc8);
DEFINE_GUID(probe_IID_ICoreApplicationView5, 0x2bc095a8, 0x8ef0, 0x446d, 0x9e, 0x60, 0x3a, 0x3e, 0x04, 0x28, 0xc6, 0x71);
DEFINE_GUID(probe_IID_ICoreApplicationViewTitleBar, 0x006d35e3, 0xe1f1, 0x431b, 0x95, 0x08, 0x29, 0xb9, 0x69, 0x26, 0xac, 0x53);
DEFINE_GUID(probe_IID_IDesignModeStatics, 0x2c3893cc, 0xf81a, 0x4e7a, 0xb8, 0x57, 0x76, 0xa8, 0x08, 0x87, 0xe1, 0x85);
DEFINE_GUID(probe_IID_IDesignModeStatics2, 0x80cf8137, 0xb064, 0x4858, 0xbe, 0xc8, 0x3e, 0xba, 0x22, 0x35, 0x75, 0x35);
DEFINE_GUID(probe_IID_IWeakReferenceSource, 0x00000038, 0x0000, 0x0000, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46);

#define APP_GET_PROPERTIES      11
#define APP_GET_CURRENT_VIEW    12
#define VIEW3_IS_COMPONENT      6
#define VIEW3_TITLE_BAR         7
#define VIEW3_ADD_CLOSING       8
#define VIEW5_PROPERTIES        6
#define BAR_PUT_EXTEND          6
#define BAR_GET_EXTEND          7
#define BAR_LEFT_INSET          8
#define BAR_RIGHT_INSET         9
#define BAR_HEIGHT              10
#define BAR_ADD_METRICS         11
#define BAR_IS_VISIBLE          13
#define WEAK_SOURCE_GET         3
#define WEAK_RESOLVE            3

typedef HRESULT (WINAPI *get_fn)( void *, void ** );

static void *qi( void *obj, const GUID *iid )
{
    void *out = NULL;
    if (obj) IUnknown_QueryInterface( (IUnknown *)obj, iid, &out );
    return out;
}

static BOOL same_object( void *a, void *b )
{
    IUnknown *ua = qi( a, &IID_IUnknown ), *ub = qi( b, &IID_IUnknown );
    BOOL ret = ua && ua == ub;
    if (ua) IUnknown_Release( ua );
    if (ub) IUnknown_Release( ub );
    return ret;
}

static void *statics( const WCHAR *name, const GUID *iid )
{
    HSTRING s = NULL;
    void *out = NULL;
    HRESULT hr;

    WindowsCreateString( name, (UINT32)wcslen( name ), &s );
    hr = RoGetActivationFactory( s, iid, &out );
    WindowsDeleteString( s );
    if (FAILED(hr)) printf( "factory %ls=%#lx\n", name, hr );
    return out;
}

/* a TypedEventHandler that is never called */
static HRESULT WINAPI handler_qi( void *h, REFIID iid, void **out ) { *out = h; return S_OK; }
static ULONG WINAPI handler_addref( void *h ) { return 2; }
static ULONG WINAPI handler_release( void *h ) { return 1; }
static HRESULT WINAPI handler_invoke( void *h, void *sender, void *args ) { return S_OK; }
static void *handler_vtbl[] = { handler_qi, handler_addref, handler_release, handler_invoke };
static struct { void *vtbl; } handler = { handler_vtbl };

int main( void )
{
    void *app, *view = NULL, *view3, *view5, *bar = NULL, *bar2 = NULL, *weak_source, *weak = NULL, *resolved = NULL;
    void *props1 = NULL, *props2 = NULL, *app_props = NULL, *design, *design2;
    double height = -1, left = -1, right = -1;
    boolean component = 2, visible = 2, extend = 2, enabled = 2;
    INT64 token = 0;
    HRESULT hr;

    RoInitialize( RO_INIT_MULTITHREADED );
    app = statics( L"Windows.ApplicationModel.Core.CoreApplication", &probe_IID_ICoreApplication );
    if (app) SLOT( app, APP_GET_CURRENT_VIEW, get_fn )( app, &view );
    view3 = qi( view, &probe_IID_ICoreApplicationView3 );
    printf( "view3=%d\n", view3 != NULL );
    if (view3)
    {
        SLOT( view3, VIEW3_IS_COMPONENT, HRESULT (WINAPI *)( void *, boolean * ) )( view3, &component );
        SLOT( view3, VIEW3_TITLE_BAR, get_fn )( view3, &bar );
        SLOT( view3, VIEW3_TITLE_BAR, get_fn )( view3, &bar2 );
        hr = SLOT( view3, VIEW3_ADD_CLOSING, HRESULT (WINAPI *)( void *, void *, INT64 * ) )( view3, &handler, &token );
        printf( "component=%d title_bar=%d same=%d closing_handler=%d\n", component,
                qi( bar, &probe_IID_ICoreApplicationViewTitleBar ) != NULL, same_object( bar, bar2 ), SUCCEEDED(hr) );
    }
    if (bar)
    {
        SLOT( bar, BAR_HEIGHT, HRESULT (WINAPI *)( void *, double * ) )( bar, &height );
        SLOT( bar, BAR_LEFT_INSET, HRESULT (WINAPI *)( void *, double * ) )( bar, &left );
        SLOT( bar, BAR_RIGHT_INSET, HRESULT (WINAPI *)( void *, double * ) )( bar, &right );
        SLOT( bar, BAR_IS_VISIBLE, HRESULT (WINAPI *)( void *, boolean * ) )( bar, &visible );
        printf( "bar height=%.0f insets=%.0f,%.0f visible=%d\n", height, left, right, visible );
        SLOT( bar, BAR_PUT_EXTEND, HRESULT (WINAPI *)( void *, boolean ) )( bar, TRUE );
        SLOT( bar, BAR_GET_EXTEND, HRESULT (WINAPI *)( void *, boolean * ) )( bar, &extend );
        SLOT( bar, BAR_PUT_EXTEND, HRESULT (WINAPI *)( void *, boolean ) )( bar, FALSE );
        token = 0;
        hr = SLOT( bar, BAR_ADD_METRICS, HRESULT (WINAPI *)( void *, void *, INT64 * ) )( bar, &handler, &token );
        printf( "extend=%d metrics_handler=%d\n", extend, SUCCEEDED(hr) && token );
        weak_source = qi( bar, &probe_IID_IWeakReferenceSource );
        if (weak_source) SLOT( weak_source, WEAK_SOURCE_GET, get_fn )( weak_source, &weak );
        if (weak) SLOT( weak, WEAK_RESOLVE, HRESULT (WINAPI *)( void *, REFIID, void ** ) )(
                weak, &probe_IID_ICoreApplicationViewTitleBar, &resolved );
        printf( "weak=%d\n", same_object( resolved, bar ) );
    }
    view5 = qi( view, &probe_IID_ICoreApplicationView5 );
    printf( "view5=%d\n", view5 != NULL );
    if (view5)
    {
        SLOT( view5, VIEW5_PROPERTIES, get_fn )( view5, &props1 );
        SLOT( view5, VIEW5_PROPERTIES, get_fn )( view5, &props2 );
        if (app) SLOT( app, APP_GET_PROPERTIES, get_fn )( app, &app_props );
        printf( "view_properties=%d same=%d not_app=%d\n", props1 != NULL, same_object( props1, props2 ),
                !same_object( props1, app_props ) );
    }
    design = statics( L"Windows.ApplicationModel.DesignMode", &probe_IID_IDesignModeStatics );
    design2 = statics( L"Windows.ApplicationModel.DesignMode", &probe_IID_IDesignModeStatics2 );
    if (design) SLOT( design, 6, HRESULT (WINAPI *)( void *, boolean * ) )( design, &enabled );
    printf( "design_mode=%d", design ? enabled : -1 );
    enabled = 2;
    if (design2) SLOT( design2, 6, HRESULT (WINAPI *)( void *, boolean * ) )( design2, &enabled );
    printf( " design_mode2=%d\n", design2 ? enabled : -1 );
    printf( "done=1\n" );
    fflush( stdout );
    return 0;
}
