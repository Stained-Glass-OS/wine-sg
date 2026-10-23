/* wuianim-probe: Windows.UI.Composition property sets, easing functions,
 * key frame animations and batches (patches/sg/6048).
 *
 *  - property sets keep scalars, vectors and booleans by name, telling a
 *    missing name from one of another type; an object's Properties is one
 *    set, the same each time it is asked for;
 *  - cubic bezier easing functions keep their control points;
 *  - key frame animations keep their key frames, duration and behaviours;
 *    started on a visual's Offset they move it over their duration, and the
 *    window shows the sprite where it ended; a scoped batch holding the
 *    animation tells its Completed handlers once the batch has ended and the
 *    animation finished, not before; an easing function shapes the values;
 *    "Offset.Y" animates one component, "Color" a brush's colour;
 *  - StopAnimation leaves the initial or the final value, as the animation's
 *    stop behaviour says;
 *  - an empty scoped batch, and the commit batch, complete; a suspended
 *    batch leaves out what is started meanwhile;
 *  - expression animations (6050) follow what they reference -- another
 *    visual's Offset, a property set entry, scalar parameters -- with
 *    arithmetic, functions and ?:, until stopped; an expression key frame is
 *    evaluated with its animation's parameters.
 *
 * The interfaces are called through their vtable slots; their IIDs are the
 * public ones. Prints name=value lines; see test/wuianim-gate.sh.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <roapi.h>
#include <winstring.h>
#include <math.h>
#include <stdio.h>

#define SLOT(obj, n, type) ((type)((*(void ***)(obj))[n]))

DEFINE_GUID(probe_IID_ICompositor, 0xb403ca50, 0x7f8c, 0x4e83, 0x98, 0x5f, 0xcc, 0x45, 0x06, 0x00, 0x36, 0xd8);
DEFINE_GUID(probe_IID_IVisual, 0x117e202d, 0xa859, 0x4c89, 0x87, 0x3b, 0xc2, 0xaa, 0x56, 0x67, 0x88, 0xe3);
DEFINE_GUID(probe_IID_IContainerVisual, 0x02f6bc74, 0xed20, 0x4773, 0xaf, 0xe6, 0xd4, 0x9b, 0x4a, 0x93, 0xdb, 0x32);
DEFINE_GUID(probe_IID_ICompositionTarget, 0xa1bea8ba, 0xd726, 0x4663, 0x81, 0x29, 0x6b, 0x5e, 0x79, 0x27, 0xff, 0xa6);
DEFINE_GUID(probe_IID_ICompositorDesktopInterop, 0x29e691fa, 0x4567, 0x4dca, 0xb3, 0x19, 0xd0, 0xf2, 0x07, 0xeb, 0x68, 0x07);
DEFINE_GUID(probe_IID_ICompositionBrush, 0xab0d7608, 0x30c0, 0x40e9, 0xb5, 0x68, 0xb6, 0x0a, 0x6b, 0xd1, 0xfb, 0x46);
DEFINE_GUID(probe_IID_ICompositionObject, 0xbcb4ad45, 0x7609, 0x4550, 0x93, 0x4f, 0x16, 0x00, 0x2a, 0x68, 0xfd, 0xed);
DEFINE_GUID(probe_IID_ICompositionPropertySet2, 0xde80731e, 0xa211, 0x4455, 0x88, 0x80, 0x7d, 0x0f, 0x3f, 0x6a, 0x44, 0xfd);
DEFINE_GUID(probe_IID_IKeyFrameAnimation, 0x126e7f22, 0x3ae9, 0x4540, 0x9a, 0x8a, 0xde, 0xae, 0x8a, 0x4a, 0x4a, 0x84);
DEFINE_GUID(probe_IID_ICompositionAnimation, 0x464c4c2c, 0x1caa, 0x4061, 0x9b, 0x40, 0xe1, 0x3f, 0xde, 0x15, 0x03, 0xca);
DEFINE_GUID(probe_IID_ICompositionEasingFunction, 0x5145e356, 0xbf79, 0x4ea8, 0x8c, 0xc2, 0x6b, 0x5b, 0x47, 0x2e, 0x6c, 0x9a);

/* vtable slots, counted from the IDL */
#define COMPOSITOR_CREATE_COLOR_KEYFRAME        6
#define COMPOSITOR_CREATE_COLOR_BRUSH_WITH_COLOR 8
#define COMPOSITOR_CREATE_CONTAINER_VISUAL      9
#define COMPOSITOR_CREATE_CUBIC_BEZIER          10
#define COMPOSITOR_CREATE_EXPRESSION_WITH       14
#define COMPOSITOR_CREATE_LINEAR_EASING         17
#define COMPOSITOR_CREATE_PROPERTY_SET          18
#define COMPOSITOR_CREATE_SCALAR_KEYFRAME       20
#define COMPOSITOR_CREATE_SCOPED_BATCH          21
#define COMPOSITOR_CREATE_SPRITE_VISUAL         22
#define COMPOSITOR_CREATE_VECTOR3_KEYFRAME      27
#define COMPOSITOR_GET_COMMIT_BATCH             29
#define OBJECT_GET_PROPERTIES                   8
#define OBJECT_START_ANIMATION                  9
#define OBJECT_STOP_ANIMATION                   10
#define SET_INSERT_SCALAR                       10
#define SET_INSERT_VECTOR3                      12
#define SET_TRYGET_SCALAR                       18
#define SET_TRYGET_VECTOR3                      20
#define SET2_INSERT_BOOLEAN                     6
#define SET2_TRYGET_BOOLEAN                     7
#define KF_PUT_DURATION                         9
#define KF_GET_DURATION                         8
#define KF_PUT_ITERATION_COUNT                  13
#define KF_GET_KEYFRAME_COUNT                   14
#define KF_GET_STOP_BEHAVIOR                    15
#define KF_PUT_STOP_BEHAVIOR                    16
#define KF_INSERT_EXPRESSION                    17
#define TYPED_INSERT                            6
#define TYPED_INSERT_EASED                      7
#define COLOR_INSERT                            8
#define BATCH_GET_IS_ACTIVE                     6
#define BATCH_GET_IS_ENDED                      7
#define BATCH_END                               8
#define BATCH_RESUME                            9
#define BATCH_SUSPEND                           10
#define BATCH_ADD_COMPLETED                     11
#define COMMIT_ADD_COMPLETED                    8
#define CUBIC_GET_CP1                           6
#define CUBIC_GET_CP2                           7
#define EXPRESSION_GET                          6
#define ANIMATION_SET_SCALAR_PARAMETER          13
#define ANIMATION_SET_REFERENCE_PARAMETER       12
#define VISUAL_GET_OFFSET                       20
#define VISUAL_PUT_OFFSET                       21
#define VISUAL_GET_OPACITY                      22
#define VISUAL_PUT_OPACITY                      23
#define VISUAL_PUT_SIZE                         36
#define CONTAINER_GET_CHILDREN                  6
#define COLLECTION_INSERT_AT_TOP                9
#define SPRITE_PUT_BRUSH                        7
#define TARGET_PUT_ROOT                         7
#define DESKTOP_INTEROP_CREATE_TARGET           3

struct ui_color { BYTE a, r, g, b; };
struct vector2 { float x, y; };
struct vector3 { float x, y, z; };
struct timespan { INT64 duration; };

typedef HRESULT (WINAPI *get_fn)( void *, void ** );
typedef HRESULT (WINAPI *put_fn)( void *, void * );
typedef HRESULT (WINAPI *call_fn)( void * );

static void *compositor;

static void pump( DWORD ms )
{
    DWORD end = GetTickCount() + ms;
    MSG msg;

    do
    {
        while (PeekMessageW( &msg, NULL, 0, 0, PM_REMOVE ))
        {
            TranslateMessage( &msg );
            DispatchMessageW( &msg );
        }
        Sleep( 5 );
    } while ((int)(end - GetTickCount()) > 0);
}

static double now_ms( void )
{
    LARGE_INTEGER c, f;
    QueryPerformanceCounter( &c );
    QueryPerformanceFrequency( &f );
    return (double)c.QuadPart * 1000.0 / (double)f.QuadPart;
}

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

static HSTRING hs( const WCHAR *s )
{
    HSTRING ret = NULL;
    WindowsCreateString( s, (UINT32)wcslen( s ), &ret );
    return ret;
}

/* a Completed handler */
struct handler
{
    void *vtbl;
    LONG ref;
    int calls;
    double when;
    void *sender;
    BOOL args_object;
};

static HRESULT WINAPI handler_qi( struct handler *h, REFIID iid, void **out )
{
    *out = h;
    InterlockedIncrement( &h->ref );
    return S_OK;
}
static ULONG WINAPI handler_addref( struct handler *h ) { return InterlockedIncrement( &h->ref ); }
static ULONG WINAPI handler_release( struct handler *h ) { return InterlockedDecrement( &h->ref ); }
static HRESULT WINAPI handler_invoke( struct handler *h, IInspectable *sender, IInspectable *args )
{
    void *obj;
    if (!h->calls++) h->when = now_ms();
    if (h->sender) IUnknown_Release( (IUnknown *)h->sender );
    h->sender = sender;
    if (sender) IUnknown_AddRef( (IUnknown *)sender );
    if ((obj = qi( args, &probe_IID_ICompositionObject )))
    {
        h->args_object = TRUE;
        IUnknown_Release( (IUnknown *)obj );
    }
    return S_OK;
}
static void *handler_vtbl[] = { handler_qi, handler_addref, handler_release, handler_invoke };

static void handler_init( struct handler *h )
{
    memset( h, 0, sizeof(*h) );
    h->vtbl = handler_vtbl;
    h->ref = 1;
}

static BOOL wait_for( struct handler *h, DWORD ms )
{
    DWORD end = GetTickCount() + ms;
    while (!h->calls && (int)(end - GetTickCount()) > 0) pump( 10 );
    return h->calls > 0;
}

static void *scoped_batch( void )
{
    void *batch = NULL;
    SLOT( compositor, COMPOSITOR_CREATE_SCOPED_BATCH, HRESULT (WINAPI *)( void *, int, void ** ) )(
            compositor, 1 /* Animation */, &batch );
    return batch;
}

static HRESULT add_completed( void *batch, int slot, struct handler *h )
{
    INT64 token = 0;
    return SLOT( batch, slot, HRESULT (WINAPI *)( void *, void *, INT64 * ) )( batch, h, &token );
}

static HRESULT start( void *obj, const WCHAR *name, void *animation )
{
    void *object = qi( obj, &probe_IID_ICompositionObject ), *anim = qi( animation, &probe_IID_ICompositionAnimation );
    HSTRING s = hs( name );
    HRESULT hr = SLOT( object, OBJECT_START_ANIMATION, HRESULT (WINAPI *)( void *, HSTRING, void * ) )( object, s, anim );
    WindowsDeleteString( s );
    return hr;
}

static HRESULT stop( void *obj, const WCHAR *name )
{
    void *object = qi( obj, &probe_IID_ICompositionObject );
    HSTRING s = hs( name );
    HRESULT hr = SLOT( object, OBJECT_STOP_ANIMATION, HRESULT (WINAPI *)( void *, HSTRING ) )( object, s );
    WindowsDeleteString( s );
    return hr;
}

static void set_duration( void *animation, double ms )
{
    void *kf = qi( animation, &probe_IID_IKeyFrameAnimation );
    struct timespan t = { (INT64)(ms * 10000) };
    SLOT( kf, KF_PUT_DURATION, HRESULT (WINAPI *)( void *, struct timespan ) )( kf, t );
}

static void *scalar_animation( float to, double ms, void *easing )
{
    void *anim = NULL;
    SLOT( compositor, COMPOSITOR_CREATE_SCALAR_KEYFRAME, get_fn )( compositor, &anim );
    if (easing)
        SLOT( anim, TYPED_INSERT_EASED, HRESULT (WINAPI *)( void *, float, float, void * ) )( anim, 1.0f, to, easing );
    else
        SLOT( anim, TYPED_INSERT, HRESULT (WINAPI *)( void *, float, float ) )( anim, 1.0f, to );
    set_duration( anim, ms );
    return anim;
}

static struct vector3 get_offset( void *visual )
{
    struct vector3 v = { -1, -1, -1 };
    SLOT( visual, VISUAL_GET_OFFSET, HRESULT (WINAPI *)( void *, struct vector3 * ) )( visual, &v );
    return v;
}

static float get_opacity( void *visual )
{
    float v = -1;
    SLOT( visual, VISUAL_GET_OPACITY, HRESULT (WINAPI *)( void *, float * ) )( visual, &v );
    return v;
}

static void check_property_sets( void *visual )
{
    void *set = NULL, *set2, *props1 = NULL, *props2 = NULL, *object;
    HSTRING foo = hs( L"Foo" ), bar = hs( L"Bar" ), flag = hs( L"Flag" );
    struct vector3 v3 = { 1, 2, 3 }, got3 = { 0 };
    int status = -1, status2 = -1, status3 = -1;
    float f = 0;
    boolean b = 0;
    HRESULT hr;

    hr = SLOT( compositor, COMPOSITOR_CREATE_PROPERTY_SET, get_fn )( compositor, &set );
    printf( "propset_created=%d (%#lx)\n", SUCCEEDED(hr) && set, hr );
    if (!set) return;
    SLOT( set, SET_INSERT_SCALAR, HRESULT (WINAPI *)( void *, HSTRING, float ) )( set, foo, 2.5f );
    hr = SLOT( set, SET_TRYGET_SCALAR, HRESULT (WINAPI *)( void *, HSTRING, float *, int * ) )( set, foo, &f, &status );
    printf( "propset_scalar=%d (%#lx %d %f)\n", hr == S_OK && status == 0 && f == 2.5f, hr, status, f );
    hr = SLOT( set, SET_TRYGET_VECTOR3, HRESULT (WINAPI *)( void *, HSTRING, struct vector3 *, int * ) )(
            set, foo, &got3, &status );
    printf( "propset_mismatch=%d (%#lx %d)\n", hr == S_OK && status == 1, hr, status );
    hr = SLOT( set, SET_TRYGET_SCALAR, HRESULT (WINAPI *)( void *, HSTRING, float *, int * ) )( set, bar, &f, &status );
    printf( "propset_notfound=%d (%#lx %d)\n", hr == S_OK && status == 2, hr, status );
    /* inserting again replaces the value and its type */
    SLOT( set, SET_INSERT_VECTOR3, HRESULT (WINAPI *)( void *, HSTRING, struct vector3 ) )( set, foo, v3 );
    SLOT( set, SET_TRYGET_VECTOR3, HRESULT (WINAPI *)( void *, HSTRING, struct vector3 *, int * ) )(
            set, foo, &got3, &status );
    SLOT( set, SET_TRYGET_SCALAR, HRESULT (WINAPI *)( void *, HSTRING, float *, int * ) )( set, foo, &f, &status2 );
    printf( "propset_replace=%d (%d %d %f)\n", status == 0 && got3.z == 3.0f && status2 == 1, status, status2, got3.z );
    set2 = qi( set, &probe_IID_ICompositionPropertySet2 );
    if (set2)
    {
        SLOT( set2, SET2_INSERT_BOOLEAN, HRESULT (WINAPI *)( void *, HSTRING, boolean ) )( set2, flag, 1 );
        SLOT( set2, SET2_TRYGET_BOOLEAN, HRESULT (WINAPI *)( void *, HSTRING, boolean *, int * ) )( set2, flag, &b, &status3 );
    }
    printf( "propset_boolean=%d (%d %d)\n", set2 && status3 == 0 && b == 1, status3, b );

    /* an object's property set */
    object = qi( visual, &probe_IID_ICompositionObject );
    SLOT( object, OBJECT_GET_PROPERTIES, get_fn )( object, &props1 );
    SLOT( object, OBJECT_GET_PROPERTIES, get_fn )( object, &props2 );
    status = -1;
    if (props1 && props2)
    {
        SLOT( props1, SET_INSERT_SCALAR, HRESULT (WINAPI *)( void *, HSTRING, float ) )( props1, bar, 4.0f );
        SLOT( props2, SET_TRYGET_SCALAR, HRESULT (WINAPI *)( void *, HSTRING, float *, int * ) )( props2, bar, &f, &status );
    }
    printf( "object_properties=%d (%p %p %d)\n", props1 && props1 == props2 && status == 0 && f == 4.0f,
            props1, props2, status );
    WindowsDeleteString( foo );
    WindowsDeleteString( bar );
    WindowsDeleteString( flag );
}

static void check_objects( void )
{
    struct vector2 p1 = { 0.25f, 0.1f }, p2 = { 0.75f, 0.9f }, bad = { 2.0f, 0 }, g1 = { 0 }, g2 = { 0 };
    void *cubic = NULL, *badcubic = NULL, *anim = NULL, *kf, *expr = NULL, *animation;
    struct timespan t = { 0 };
    INT32 count = -1;
    int behavior = -1;
    HSTRING s, text = NULL;
    HRESULT hr, hr_key, hr_dur;
    UINT32 len = 0;

    SLOT( compositor, COMPOSITOR_CREATE_CUBIC_BEZIER, HRESULT (WINAPI *)( void *, struct vector2, struct vector2, void ** ) )(
            compositor, p1, p2, &cubic );
    if (cubic)
    {
        SLOT( cubic, CUBIC_GET_CP1, HRESULT (WINAPI *)( void *, struct vector2 * ) )( cubic, &g1 );
        SLOT( cubic, CUBIC_GET_CP2, HRESULT (WINAPI *)( void *, struct vector2 * ) )( cubic, &g2 );
    }
    hr = SLOT( compositor, COMPOSITOR_CREATE_CUBIC_BEZIER, HRESULT (WINAPI *)( void *, struct vector2, struct vector2, void ** ) )(
            compositor, bad, p2, &badcubic );
    printf( "easing_cubic=%d (%f %f %#lx)\n", cubic && g1.x == 0.25f && g1.y == 0.1f && g2.x == 0.75f && g2.y == 0.9f
            && hr == E_INVALIDARG, g1.x, g2.y, hr );

    SLOT( compositor, COMPOSITOR_CREATE_SCALAR_KEYFRAME, get_fn )( compositor, &anim );
    kf = qi( anim, &probe_IID_IKeyFrameAnimation );
    if (kf)
    {
        SLOT( anim, TYPED_INSERT, HRESULT (WINAPI *)( void *, float, float ) )( anim, 0.0f, 1.0f );
        SLOT( anim, TYPED_INSERT, HRESULT (WINAPI *)( void *, float, float ) )( anim, 1.0f, 2.0f );
        SLOT( anim, TYPED_INSERT, HRESULT (WINAPI *)( void *, float, float ) )( anim, 1.0f, 3.0f );
        hr_key = SLOT( anim, TYPED_INSERT, HRESULT (WINAPI *)( void *, float, float ) )( anim, 1.5f, 3.0f );
        SLOT( kf, KF_GET_KEYFRAME_COUNT, HRESULT (WINAPI *)( void *, INT32 * ) )( kf, &count );
        set_duration( anim, 300 );
        hr_dur = SLOT( kf, KF_PUT_DURATION, HRESULT (WINAPI *)( void *, struct timespan ) )( kf, t );
        SLOT( kf, KF_GET_DURATION, HRESULT (WINAPI *)( void *, struct timespan * ) )( kf, &t );
        SLOT( kf, KF_GET_STOP_BEHAVIOR, HRESULT (WINAPI *)( void *, int * ) )( kf, &behavior );
        printf( "keyframe_object=%d (count %d, key %#lx, dur %#lx %lld, stop %d)\n", count == 2
                && hr_key == E_INVALIDARG && hr_dur == E_INVALIDARG && t.duration == 3000000 && behavior == 2,
                count, hr_key, hr_dur, t.duration, behavior );
    }
    else printf( "keyframe_object=0 (no IKeyFrameAnimation)\n" );

    s = hs( L"Foo * 2" );
    hr = SLOT( compositor, COMPOSITOR_CREATE_EXPRESSION_WITH, HRESULT (WINAPI *)( void *, HSTRING, void ** ) )(
            compositor, s, &expr );
    if (expr) SLOT( expr, EXPRESSION_GET, HRESULT (WINAPI *)( void *, HSTRING * ) )( expr, &text );
    animation = qi( expr, &probe_IID_ICompositionAnimation );
    if (animation)
    {
        HSTRING foo = hs( L"Foo" );
        hr = SLOT( animation, ANIMATION_SET_SCALAR_PARAMETER, HRESULT (WINAPI *)( void *, HSTRING, float ) )(
                animation, foo, 1.0f );
        WindowsDeleteString( foo );
    }
    printf( "expression_object=%d (%#lx %ls)\n", expr && animation && hr == S_OK
            && !wcscmp( WindowsGetStringRawBuffer( text, &len ), L"Foo * 2" ), hr,
            WindowsGetStringRawBuffer( text, &len ) );
    WindowsDeleteString( s );
}

static void *expression( const WCHAR *text )
{
    void *expr = NULL;
    HSTRING s = hs( text );
    SLOT( compositor, COMPOSITOR_CREATE_EXPRESSION_WITH, HRESULT (WINAPI *)( void *, HSTRING, void ** ) )(
            compositor, s, &expr );
    WindowsDeleteString( s );
    return expr;
}

static void set_scalar_parameter( void *anim, const WCHAR *name, float value )
{
    void *animation = qi( anim, &probe_IID_ICompositionAnimation );
    HSTRING s = hs( name );
    SLOT( animation, ANIMATION_SET_SCALAR_PARAMETER, HRESULT (WINAPI *)( void *, HSTRING, float ) )( animation, s, value );
    WindowsDeleteString( s );
}

static void set_reference_parameter( void *anim, const WCHAR *name, void *object )
{
    void *animation = qi( anim, &probe_IID_ICompositionAnimation );
    HSTRING s = hs( name );
    SLOT( animation, ANIMATION_SET_REFERENCE_PARAMETER, HRESULT (WINAPI *)( void *, HSTRING, void * ) )(
            animation, s, qi( object, &probe_IID_ICompositionObject ) );
    WindowsDeleteString( s );
}

static void put_offset( void *visual, float x, float y )
{
    struct vector3 v = { x, y, 0 };
    SLOT( visual, VISUAL_PUT_OFFSET, HRESULT (WINAPI *)( void *, struct vector3 ) )( visual, v );
}

static void check_expressions( void *children, HWND hwnd )
{
    struct ui_color red = { 255, 255, 0, 0 };
    struct vector2 size = { 50, 50 };
    void *source = NULL, *sprite = NULL, *visual, *brush = NULL, *expr, *set = NULL, *anim, *kf;
    struct vector3 v1, v2, v3;
    float opacity, opacity2, opacity3;
    COLORREF pixel;
    HSTRING s;
    HRESULT hr;
    HDC dc;

    /* a sprite following another visual's offset */
    SLOT( compositor, COMPOSITOR_CREATE_CONTAINER_VISUAL, get_fn )( compositor, &source );
    source = qi( source, &probe_IID_IVisual );
    put_offset( source, 20, 0 );
    SLOT( compositor, COMPOSITOR_CREATE_COLOR_BRUSH_WITH_COLOR, HRESULT (WINAPI *)( void *, struct ui_color, void ** ) )(
            compositor, red, &brush );
    SLOT( compositor, COMPOSITOR_CREATE_SPRITE_VISUAL, get_fn )( compositor, &sprite );
    SLOT( sprite, SPRITE_PUT_BRUSH, put_fn )( sprite, qi( brush, &probe_IID_ICompositionBrush ) );
    visual = qi( sprite, &probe_IID_IVisual );
    SLOT( visual, VISUAL_PUT_SIZE, HRESULT (WINAPI *)( void *, struct vector2 ) )( visual, size );
    SLOT( children, COLLECTION_INSERT_AT_TOP, put_fn )( children, visual );

    expr = expression( L"Vector3(src.Offset.X + dx, Max(5, src.Offset.Y), 0)" );
    set_reference_parameter( expr, L"src", source );
    set_scalar_parameter( expr, L"dx", 10 );
    hr = start( visual, L"Offset", expr );
    pump( 200 );
    v1 = get_offset( visual );
    put_offset( source, 40, 0 );
    pump( 300 );
    v2 = get_offset( visual );
    printf( "expr_reference=%d (%#lx %f %f)\n", hr == S_OK && v1.x == 30.0f && v1.y == 5.0f, hr, v1.x, v1.y );
    printf( "expr_follows=%d (%f %f)\n", v2.x == 50.0f && v2.y == 5.0f, v2.x, v2.y );
    dc = GetDC( hwnd );
    pixel = GetPixel( dc, 75, 30 );
    ReleaseDC( hwnd, dc );
    printf( "expr_drawn=%d (%06lx)\n", pixel == RGB(255, 0, 0), pixel );
    stop( visual, L"Offset" );
    put_offset( source, 60, 0 );
    pump( 200 );
    v3 = get_offset( visual );
    printf( "expr_stopped=%d (%f)\n", v3.x == 50.0f, v3.x );

    /* scalar parameters, functions, a property set entry, ?: */
    expr = expression( L"Clamp(p * 2, 0, 1) - (props.Foo > 2 ? 0.25 : 0)" );
    set_scalar_parameter( expr, L"p", 0.3f );
    SLOT( compositor, COMPOSITOR_CREATE_PROPERTY_SET, get_fn )( compositor, &set );
    s = hs( L"Foo" );
    SLOT( set, SET_INSERT_SCALAR, HRESULT (WINAPI *)( void *, HSTRING, float ) )( set, s, 3.0f );
    set_reference_parameter( expr, L"props", set );
    start( visual, L"Opacity", expr );
    pump( 200 );
    opacity = get_opacity( visual );
    SLOT( set, SET_INSERT_SCALAR, HRESULT (WINAPI *)( void *, HSTRING, float ) )( set, s, 1.0f );
    pump( 300 );
    opacity2 = get_opacity( visual );
    WindowsDeleteString( s );
    printf( "expr_scalar=%d (%f %f)\n", fabsf( opacity - 0.35f ) < 0.0001f && fabsf( opacity2 - 0.6f ) < 0.0001f,
            opacity, opacity2 );
    stop( visual, L"Opacity" );

    /* an expression key frame, with its animation's parameter */
    SLOT( compositor, COMPOSITOR_CREATE_SCALAR_KEYFRAME, get_fn )( compositor, &anim );
    kf = qi( anim, &probe_IID_IKeyFrameAnimation );
    s = hs( L"Lerp(this.StartingValue, q, 0.5) + 0.1" );
    hr = SLOT( kf, KF_INSERT_EXPRESSION, HRESULT (WINAPI *)( void *, float, HSTRING ) )( kf, 1.0f, s );
    WindowsDeleteString( s );
    set_scalar_parameter( anim, L"q", 0.4f );
    set_duration( anim, 100 );
    SLOT( visual, VISUAL_PUT_OPACITY, HRESULT (WINAPI *)( void *, float ) )( visual, 0.2f );
    start( visual, L"Opacity", anim );
    pump( 600 );
    opacity3 = get_opacity( visual );
    printf( "expr_keyframe=%d (%#lx %f)\n", hr == S_OK && fabsf( opacity3 - 0.4f ) < 0.0001f, hr, opacity3 );

    /* a malformed expression key frame is refused */
    s = hs( L"1 + * 2" );
    hr = SLOT( kf, KF_INSERT_EXPRESSION, HRESULT (WINAPI *)( void *, float, HSTRING ) )( kf, 0.5f, s );
    WindowsDeleteString( s );
    printf( "expr_malformed=%d (%#lx)\n", hr == E_INVALIDARG, hr );
}

static LRESULT CALLBACK wndproc( HWND hwnd, UINT msg, WPARAM wp, LPARAM lp )
{
    if (msg == WM_ERASEBKGND) return 1;
    return DefWindowProcW( hwnd, msg, wp, lp );
}

static void check_animations( void )
{
    static const WCHAR name[] = L"Windows.UI.Composition.Compositor";
    WNDCLASSW wc = { 0, wndproc, 0, 0, NULL, NULL, NULL, NULL, NULL, L"wuianim" };
    struct ui_color red = { 255, 255, 0, 0 }, green = { 255, 0, 255, 0 };
    void *desktop, *target = NULL, *ctarget, *root = NULL, *container, *children = NULL, *brush = NULL, *cbrush;
    void *sprite = NULL, *visual, *anim = NULL, *batch, *cubic = NULL, *color_anim = NULL, *commit = NULL;
    void *set = NULL;
    struct vector3 to = { 150, 0, 0 }, mid, end;
    struct vector2 size = { 50, 50 }, ease_in = { 1.0f, 0.0f };
    struct handler h, h2, h3, h4;
    IInspectable *inspectable = NULL;
    double t0, elapsed;
    boolean ended = 0, active = 1;
    HSTRING s;
    HWND hwnd;
    HDC dc;
    COLORREF there = 0xdeadbeef, gone = 0xdeadbeef, now_green = 0xdeadbeef;
    float opacity, frac;
    HRESULT hr;

    s = hs( name );
    hr = RoActivateInstance( s, &inspectable );
    WindowsDeleteString( s );
    printf( "compositor_activated=%d (%#lx)\n", SUCCEEDED(hr), hr );
    if (FAILED( hr )) return;
    compositor = qi( inspectable, &probe_IID_ICompositor );
    desktop = qi( inspectable, &probe_IID_ICompositorDesktopInterop );
    if (!compositor || !desktop) return;

    wc.hInstance = GetModuleHandleW( NULL );
    RegisterClassW( &wc );
    hwnd = CreateWindowExW( 0, L"wuianim", L"wuianim", WS_POPUP | WS_VISIBLE, 50, 50, 200, 100, NULL, NULL, NULL, NULL );
    pump( 300 );
    SLOT( desktop, DESKTOP_INTEROP_CREATE_TARGET, HRESULT (WINAPI *)( void *, HWND, BOOL, void ** ) )(
            desktop, hwnd, FALSE, &target );
    if (!target) return;
    ctarget = qi( target, &probe_IID_ICompositionTarget );
    SLOT( compositor, COMPOSITOR_CREATE_CONTAINER_VISUAL, get_fn )( compositor, &root );
    container = qi( root, &probe_IID_IContainerVisual );
    SLOT( container, CONTAINER_GET_CHILDREN, get_fn )( container, &children );
    SLOT( compositor, COMPOSITOR_CREATE_COLOR_BRUSH_WITH_COLOR, HRESULT (WINAPI *)( void *, struct ui_color, void ** ) )(
            compositor, red, &brush );
    cbrush = qi( brush, &probe_IID_ICompositionBrush );
    SLOT( compositor, COMPOSITOR_CREATE_SPRITE_VISUAL, get_fn )( compositor, &sprite );
    SLOT( sprite, SPRITE_PUT_BRUSH, put_fn )( sprite, cbrush );
    visual = qi( sprite, &probe_IID_IVisual );
    SLOT( visual, VISUAL_PUT_SIZE, HRESULT (WINAPI *)( void *, struct vector2 ) )( visual, size );
    SLOT( children, COLLECTION_INSERT_AT_TOP, put_fn )( children, visual );
    SLOT( ctarget, TARGET_PUT_ROOT, put_fn )( ctarget, qi( root, &probe_IID_IVisual ) );
    pump( 500 );

    check_property_sets( visual );
    check_objects();

    /* Offset from where it is to x 150 over a second, in a scoped batch */
    SLOT( compositor, COMPOSITOR_CREATE_VECTOR3_KEYFRAME, get_fn )( compositor, &anim );
    s = hs( L"this.StartingValue" );
    SLOT( qi( anim, &probe_IID_IKeyFrameAnimation ), KF_INSERT_EXPRESSION, HRESULT (WINAPI *)( void *, float, HSTRING ) )(
            qi( anim, &probe_IID_IKeyFrameAnimation ), 0.0f, s );
    WindowsDeleteString( s );
    SLOT( anim, TYPED_INSERT, HRESULT (WINAPI *)( void *, float, struct vector3 ) )( anim, 1.0f, to );
    set_duration( anim, 1000 );
    batch = scoped_batch();
    t0 = now_ms();
    hr = start( visual, L"Offset", anim );
    printf( "start_animation=%d (%#lx)\n", hr == S_OK, hr );
    SLOT( batch, BATCH_END, call_fn )( batch );
    /* subscribed after End, as programs do */
    handler_init( &h );
    add_completed( batch, BATCH_ADD_COMPLETED, &h );
    while (now_ms() - t0 < 300) pump( 10 );
    mid = get_offset( visual );
    elapsed = now_ms() - t0;
    printf( "anim_midway=%d (x %f at %.0f ms, calls %d)\n", mid.x > 0.0f && mid.x < 150.0f && !h.calls,
            mid.x, elapsed, h.calls );
    wait_for( &h, 5000 );
    end = get_offset( visual );
    printf( "batch_completed=%d (calls %d)\n", h.calls == 1, h.calls );
    printf( "batch_after_animation=%d (%.0f ms)\n", h.calls && h.when - t0 >= 950, h.when - t0 );
    printf( "batch_sender=%d\n", h.sender && same_object( h.sender, batch ) && h.args_object );
    SLOT( batch, BATCH_GET_IS_ENDED, HRESULT (WINAPI *)( void *, boolean * ) )( batch, &ended );
    SLOT( batch, BATCH_GET_IS_ACTIVE, HRESULT (WINAPI *)( void *, boolean * ) )( batch, &active );
    printf( "batch_state=%d (%d %d)\n", ended && !active, ended, active );
    printf( "anim_final=%d (%f %f)\n", end.x == 150.0f && end.y == 0.0f, end.x, end.y );
    pump( 400 );
    dc = GetDC( hwnd );
    there = GetPixel( dc, 175, 25 );
    gone = GetPixel( dc, 25, 25 );
    ReleaseDC( hwnd, dc );
    printf( "anim_drawn=%d (%06lx %06lx)\n", there == RGB(255, 0, 0) && gone != RGB(255, 0, 0), there, gone );

    /* one component */
    anim = scalar_animation( 40.0f, 100, NULL );
    batch = scoped_batch();
    start( visual, L"Offset.Y", anim );
    SLOT( batch, BATCH_END, call_fn )( batch );
    handler_init( &h );
    add_completed( batch, BATCH_ADD_COMPLETED, &h );
    wait_for( &h, 3000 );
    end = get_offset( visual );
    printf( "anim_component=%d (%f %f)\n", h.calls && end.x == 150.0f && end.y == 40.0f, end.x, end.y );

    /* a brush's colour */
    SLOT( compositor, COMPOSITOR_CREATE_COLOR_KEYFRAME, get_fn )( compositor, &color_anim );
    SLOT( color_anim, COLOR_INSERT, HRESULT (WINAPI *)( void *, float, struct ui_color ) )( color_anim, 1.0f, green );
    set_duration( color_anim, 100 );
    batch = scoped_batch();
    hr = start( brush, L"Color", color_anim );
    SLOT( batch, BATCH_END, call_fn )( batch );
    handler_init( &h );
    add_completed( batch, BATCH_ADD_COMPLETED, &h );
    wait_for( &h, 3000 );
    pump( 400 );
    dc = GetDC( hwnd );
    now_green = GetPixel( dc, 175, 65 );
    ReleaseDC( hwnd, dc );
    printf( "anim_color=%d (%#lx %06lx)\n", hr == S_OK && now_green == RGB(0, 255, 0), hr, now_green );

    /* a mismatched type is refused; a property set entry is animated */
    hr = start( visual, L"Opacity", color_anim );
    SLOT( compositor, COMPOSITOR_CREATE_PROPERTY_SET, get_fn )( compositor, &set );
    s = hs( L"Foo" );
    SLOT( set, SET_INSERT_SCALAR, HRESULT (WINAPI *)( void *, HSTRING, float ) )( set, s, 0.0f );
    WindowsDeleteString( s );
    printf( "anim_typecheck=%d (%#lx)\n", hr == E_INVALIDARG && start( set, L"Foo", color_anim ) == E_INVALIDARG
            && start( set, L"Foo", scalar_animation( 7.0f, 100, NULL ) ) == S_OK, hr );

    /* easing: a strong ease-in lags well behind linear progress */
    SLOT( compositor, COMPOSITOR_CREATE_CUBIC_BEZIER, HRESULT (WINAPI *)( void *, struct vector2, struct vector2, void ** ) )(
            compositor, ease_in, ease_in, &cubic );
    SLOT( visual, VISUAL_PUT_OPACITY, HRESULT (WINAPI *)( void *, float ) )( visual, 0.0f );
    anim = scalar_animation( 1.0f, 1000, qi( cubic, &probe_IID_ICompositionEasingFunction ) );
    t0 = now_ms();
    start( visual, L"Opacity", anim );
    while (now_ms() - t0 < 500) pump( 10 );
    opacity = get_opacity( visual );
    frac = (float)((now_ms() - t0) / 1000.0);
    printf( "anim_eased=%d (%f at %f)\n", opacity >= 0.0f && frac < 0.95f && opacity < frac - 0.2f, opacity, frac );
    stop( visual, L"Opacity" );

    /* StopAnimation: the initial or the final value */
    SLOT( visual, VISUAL_PUT_OPACITY, HRESULT (WINAPI *)( void *, float ) )( visual, 1.0f );
    anim = scalar_animation( 0.2f, 10000, NULL );
    SLOT( qi( anim, &probe_IID_IKeyFrameAnimation ), KF_PUT_STOP_BEHAVIOR, HRESULT (WINAPI *)( void *, int ) )(
            qi( anim, &probe_IID_IKeyFrameAnimation ), 1 /* SetToInitialValue */ );
    start( visual, L"Opacity", anim );
    pump( 200 );
    stop( visual, L"Opacity" );
    opacity = get_opacity( visual );
    printf( "stop_initial=%d (%f)\n", opacity == 1.0f, opacity );
    anim = scalar_animation( 0.2f, 10000, NULL );
    start( visual, L"Opacity", anim );
    pump( 200 );
    stop( visual, L"Opacity" );
    opacity = get_opacity( visual );
    printf( "stop_final=%d (%f)\n", fabsf( opacity - 0.2f ) < 0.0001f, opacity );

    /* an empty batch completes */
    batch = scoped_batch();
    SLOT( batch, BATCH_END, call_fn )( batch );
    handler_init( &h2 );
    add_completed( batch, BATCH_ADD_COMPLETED, &h2 );
    printf( "empty_batch=%d\n", wait_for( &h2, 3000 ) && h2.calls == 1 );

    /* what is started while a batch is suspended is not in it */
    batch = scoped_batch();
    SLOT( batch, BATCH_SUSPEND, call_fn )( batch );
    SLOT( batch, BATCH_GET_IS_ACTIVE, HRESULT (WINAPI *)( void *, boolean * ) )( batch, &active );
    start( visual, L"Opacity", scalar_animation( 0.5f, 3000, NULL ) );
    SLOT( batch, BATCH_RESUME, call_fn )( batch );
    SLOT( batch, BATCH_END, call_fn )( batch );
    handler_init( &h3 );
    add_completed( batch, BATCH_ADD_COMPLETED, &h3 );
    t0 = now_ms();
    wait_for( &h3, 5000 );
    printf( "suspended_batch=%d (active %d, %.0f ms)\n", !active && h3.calls == 1 && h3.when - t0 < 1500, active,
            h3.when - t0 );

    /* the commit batch ends with the next commit */
    hr = SLOT( compositor, COMPOSITOR_GET_COMMIT_BATCH, HRESULT (WINAPI *)( void *, int, void ** ) )(
            compositor, 1, &commit );
    active = 0;
    if (commit) SLOT( commit, BATCH_GET_IS_ACTIVE, HRESULT (WINAPI *)( void *, boolean * ) )( commit, &active );
    handler_init( &h4 );
    if (commit) add_completed( commit, COMMIT_ADD_COMPLETED, &h4 );
    wait_for( &h4, 3000 );
    ended = 0;
    if (commit) SLOT( commit, BATCH_GET_IS_ENDED, HRESULT (WINAPI *)( void *, boolean * ) )( commit, &ended );
    printf( "commit_batch=%d (%#lx active %d calls %d ended %d)\n", commit && active && h4.calls == 1 && ended, hr,
            active, h4.calls, ended );

    check_expressions( children, hwnd );

    DestroyWindow( hwnd );
}

int main( void )
{
    RoInitialize( RO_INIT_SINGLETHREADED );
    check_animations();
    printf( "done=1\n" );
    fflush( stdout );
    return 0;
}
