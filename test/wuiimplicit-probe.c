/* wuiimplicit-probe: Windows.UI.Composition ICompositor2 implicit animations and step easing
 * (patches/sg/6195).
 *
 *  - the compositor answers ICompositor2; CreateImplicitAnimationCollection makes a map from
 *    property names to animations (Insert tells a replacement, Lookup, HasKey, Remove, a view
 *    and an iterator of its pairs);
 *  - a visual answers ICompositionObject2: its ImplicitAnimations collection is kept; when its
 *    Offset is then set, the collection's "Offset" animation (key frame "this.FinalValue", Target
 *    "Offset" through ICompositionAnimation2) runs from the old value to the new one: halfway
 *    it is between them, at the end it is the new value;
 *  - without a collection, setting Offset takes effect at once;
 *  - CreateStepEasingFunction makes a step function (StepCount 1, FinalStep 1); with 4 steps and
 *    single-frame first and last steps, late in an animation it holds the third step (0.75), not
 *    the last.
 *
 * The interfaces are called through their vtable slots; their IIDs are the public ones (windows-rs
 * identity data). Prints name=value lines; see test/wuiimplicit-gate.sh.
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
DEFINE_GUID(probe_IID_ICompositor2, 0x735081dc, 0x5e24, 0x45da, 0xa3, 0x8f, 0xe3, 0x2c, 0xc3, 0x49, 0xa9, 0xa0);
DEFINE_GUID(probe_IID_IVisual, 0x117e202d, 0xa859, 0x4c89, 0x87, 0x3b, 0xc2, 0xaa, 0x56, 0x67, 0x88, 0xe3);
DEFINE_GUID(probe_IID_IContainerVisual, 0x02f6bc74, 0xed20, 0x4773, 0xaf, 0xe6, 0xd4, 0x9b, 0x4a, 0x93, 0xdb, 0x32);
DEFINE_GUID(probe_IID_ICompositionTarget, 0xa1bea8ba, 0xd726, 0x4663, 0x81, 0x29, 0x6b, 0x5e, 0x79, 0x27, 0xff, 0xa6);
DEFINE_GUID(probe_IID_ICompositorDesktopInterop, 0x29e691fa, 0x4567, 0x4dca, 0xb3, 0x19, 0xd0, 0xf2, 0x07, 0xeb, 0x68, 0x07);
DEFINE_GUID(probe_IID_ICompositionObject, 0xbcb4ad45, 0x7609, 0x4550, 0x93, 0x4f, 0x16, 0x00, 0x2a, 0x68, 0xfd, 0xed);
DEFINE_GUID(probe_IID_ICompositionObject2, 0xef874ea1, 0x5cff, 0x4b68, 0x9e, 0x30, 0xa1, 0x51, 0x9d, 0x08, 0xba, 0x03);
DEFINE_GUID(probe_IID_IKeyFrameAnimation, 0x126e7f22, 0x3ae9, 0x4540, 0x9a, 0x8a, 0xde, 0xae, 0x8a, 0x4a, 0x4a, 0x84);
DEFINE_GUID(probe_IID_ICompositionAnimation, 0x464c4c2c, 0x1caa, 0x4061, 0x9b, 0x40, 0xe1, 0x3f, 0xde, 0x15, 0x03, 0xca);
DEFINE_GUID(probe_IID_ICompositionAnimation2, 0x369b603e, 0xa80f, 0x4948, 0x93, 0xe3, 0xed, 0x23, 0xfb, 0x38, 0xc6, 0xcb);
DEFINE_GUID(probe_IID_ICompositionAnimationBase, 0x1c2c2999, 0xe818, 0x48d3, 0xa6, 0xdd, 0xd7, 0x8c, 0x82, 0xf8, 0xac, 0xe9);
DEFINE_GUID(probe_IID_ICompositionEasingFunction, 0x5145e356, 0xbf79, 0x4ea8, 0x8c, 0xc2, 0x6b, 0x5b, 0x47, 0x2e, 0x6c, 0x9a);
DEFINE_GUID(probe_IID_IStepEasingFunction, 0xd0caa74b, 0x560c, 0x4a0b, 0xa5, 0xf6, 0x20, 0x6c, 0xa8, 0xc3, 0xec, 0xd6);
DEFINE_GUID(probe_IID_IImplicitAnimationCollection, 0x0598a3ff, 0x0a92, 0x4c9d, 0xa4, 0x27, 0xb2, 0x55, 0x19, 0x25, 0x0d, 0xbf);
/* IMap<String, ICompositionAnimationBase>, IIterable<IKeyValuePair<String, ICompositionAnimationBase>> */
DEFINE_GUID(probe_IID_IMap, 0x4ba9aa1d, 0x57ac, 0x54de, 0xa8, 0x07, 0xb5, 0x2e, 0x68, 0x9b, 0xfc, 0x04);
DEFINE_GUID(probe_IID_IIterable, 0xc739c420, 0x64b2, 0x53f2, 0x89, 0xb9, 0x05, 0x90, 0x6a, 0xba, 0x7c, 0xb0);

#define COMPOSITOR_CREATE_CONTAINER_VISUAL      9
#define COMPOSITOR_CREATE_SCALAR_KEYFRAME       20
#define COMPOSITOR_CREATE_SPRITE_VISUAL         22
#define COMPOSITOR_CREATE_VECTOR3_KEYFRAME      27
#define COMPOSITOR2_CREATE_IMPLICIT             11
#define COMPOSITOR2_CREATE_STEP                 17
#define COMPOSITOR2_CREATE_STEP_COUNT           18
#define MAP_LOOKUP                              6
#define MAP_GET_SIZE                            7
#define MAP_HAS_KEY                             8
#define MAP_GET_VIEW                            9
#define MAP_INSERT                              10
#define MAP_REMOVE                              11
#define ITERABLE_FIRST                          6
#define ITERATOR_GET_CURRENT                    6
#define ITERATOR_GET_HAS_CURRENT                7
#define ITERATOR_MOVE_NEXT                      8
#define PAIR_GET_KEY                            6
#define OBJECT2_GET_IMPLICIT                    8
#define OBJECT2_PUT_IMPLICIT                    9
#define ANIMATION2_GET_TARGET                   7
#define ANIMATION2_PUT_TARGET                   8
#define STEP_GET_FINAL                          6
#define STEP_PUT_IS_FINAL_SINGLE                11
#define STEP_PUT_IS_INITIAL_SINGLE              13
#define STEP_GET_COUNT                          14
#define STEP_PUT_COUNT                          15
#define OBJECT_START_ANIMATION                  9
#define KF_PUT_DURATION                         9
#define KF_INSERT_EXPRESSION                    17
#define TYPED_INSERT                            6
#define TYPED_INSERT_EASED                      7
#define VISUAL_GET_OFFSET                       20
#define VISUAL_PUT_OFFSET                       21
#define VISUAL_GET_OPACITY                      22
#define VISUAL_PUT_OPACITY                      23
#define VISUAL_PUT_SIZE                         36
#define CONTAINER_GET_CHILDREN                  6
#define COLLECTION_INSERT_AT_TOP                9
#define TARGET_PUT_ROOT                         7
#define DESKTOP_INTEROP_CREATE_TARGET           3

struct vector2 { float x, y; };
struct vector3 { float x, y, z; };
struct timespan { INT64 duration; };

typedef HRESULT (WINAPI *get_fn)( void *, void ** );
typedef HRESULT (WINAPI *put_fn)( void *, void * );

static void *compositor, *compositor2;

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

/* pumps until MS milliseconds after T0 */
static void pump_until( double t0, double ms )
{
    MSG msg;

    while (now_ms() - t0 < ms)
    {
        while (PeekMessageW( &msg, NULL, 0, 0, PM_REMOVE ))
        {
            TranslateMessage( &msg );
            DispatchMessageW( &msg );
        }
        Sleep( 2 );
    }
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

static void set_duration( void *animation, double ms )
{
    void *kf = qi( animation, &probe_IID_IKeyFrameAnimation );
    struct timespan t = { (INT64)(ms * 10000) };
    SLOT( kf, KF_PUT_DURATION, HRESULT (WINAPI *)( void *, struct timespan ) )( kf, t );
}

static struct vector3 get_offset( void *visual )
{
    struct vector3 v = { -1, -1, -1 };
    SLOT( visual, VISUAL_GET_OFFSET, HRESULT (WINAPI *)( void *, struct vector3 * ) )( visual, &v );
    return v;
}

static void put_offset( void *visual, float x )
{
    struct vector3 v = { x, 0, 0 };
    SLOT( visual, VISUAL_PUT_OFFSET, HRESULT (WINAPI *)( void *, struct vector3 ) )( visual, v );
}

static float get_opacity( void *visual )
{
    float v = -1;
    SLOT( visual, VISUAL_GET_OPACITY, HRESULT (WINAPI *)( void *, float * ) )( visual, &v );
    return v;
}

/* a Vector3 animation to this.FinalValue that animates Offset, as WinUI 2's NavigationView makes */
static void *offset_animation( double ms )
{
    void *anim = NULL, *anim2;
    HSTRING s;
    HRESULT hr;

    SLOT( compositor, COMPOSITOR_CREATE_VECTOR3_KEYFRAME, get_fn )( compositor, &anim );
    if (!anim) return NULL;
    anim2 = qi( anim, &probe_IID_ICompositionAnimation2 );
    printf( "animation2=%d\n", anim2 != NULL );
    if (anim2)
    {
        HSTRING got = NULL;
        s = hs( L"Offset" );
        SLOT( anim2, ANIMATION2_PUT_TARGET, HRESULT (WINAPI *)( void *, HSTRING ) )( anim2, s );
        WindowsDeleteString( s );
        SLOT( anim2, ANIMATION2_GET_TARGET, HRESULT (WINAPI *)( void *, HSTRING * ) )( anim2, &got );
        printf( "target=%ls\n", WindowsGetStringRawBuffer( got, NULL ) );
        WindowsDeleteString( got );
    }
    s = hs( L"this.FinalValue" );
    hr = SLOT( qi( anim, &probe_IID_IKeyFrameAnimation ), KF_INSERT_EXPRESSION, HRESULT (WINAPI *)( void *, float, HSTRING ) )(
            qi( anim, &probe_IID_IKeyFrameAnimation ), 1.0f, s );
    WindowsDeleteString( s );
    if (FAILED(hr)) printf( "insert_expression=%#lx\n", hr );
    set_duration( anim, ms );
    return anim;
}

static void check_collection( void *collection, void *anim )
{
    void *map = qi( collection, &probe_IID_IMap ), *base = qi( anim, &probe_IID_ICompositionAnimationBase );
    void *found = NULL, *iterable, *iterator = NULL, *pair = NULL, *view = NULL;
    boolean replaced = 2, has = 0, has_current = 0, missing = 1;
    unsigned int size = 0, view_size = 0;
    HSTRING key = hs( L"Offset" ), other = hs( L"Opacity" ), got = NULL;
    HRESULT hr, hr_missing, hr_remove;

    if (!map || !base)
    {
        printf( "map=0\n" );
        return;
    }
    hr = SLOT( map, MAP_INSERT, HRESULT (WINAPI *)( void *, HSTRING, void *, boolean * ) )( map, key, base, &replaced );
    printf( "insert=%d replaced=%d\n", SUCCEEDED(hr), replaced );
    SLOT( map, MAP_INSERT, HRESULT (WINAPI *)( void *, HSTRING, void *, boolean * ) )( map, key, base, &replaced );
    SLOT( map, MAP_GET_SIZE, HRESULT (WINAPI *)( void *, unsigned int * ) )( map, &size );
    SLOT( map, MAP_HAS_KEY, HRESULT (WINAPI *)( void *, HSTRING, boolean * ) )( map, key, &has );
    SLOT( map, MAP_HAS_KEY, HRESULT (WINAPI *)( void *, HSTRING, boolean * ) )( map, other, &missing );
    SLOT( map, MAP_LOOKUP, HRESULT (WINAPI *)( void *, HSTRING, void ** ) )( map, key, &found );
    hr_missing = SLOT( map, MAP_LOOKUP, HRESULT (WINAPI *)( void *, HSTRING, void ** ) )( map, other, (void **)&got );
    printf( "again replaced=%d size=%u has=%d missing=%d lookup=%d lookup_missing=%#lx\n", replaced, size, has, missing,
            same_object( found, anim ), hr_missing );
    hr_remove = SLOT( map, MAP_REMOVE, HRESULT (WINAPI *)( void *, HSTRING ) )( map, other );
    SLOT( map, MAP_GET_VIEW, get_fn )( map, &view );
    if (view) SLOT( view, MAP_GET_SIZE, HRESULT (WINAPI *)( void *, unsigned int * ) )( view, &view_size );
    printf( "remove_missing=%#lx view_size=%u\n", hr_remove, view_size );
    if ((iterable = qi( collection, &probe_IID_IIterable )))
    {
        got = NULL;
        SLOT( iterable, ITERABLE_FIRST, get_fn )( iterable, &iterator );
        if (iterator)
        {
            SLOT( iterator, ITERATOR_GET_HAS_CURRENT, HRESULT (WINAPI *)( void *, boolean * ) )( iterator, &has_current );
            SLOT( iterator, ITERATOR_GET_CURRENT, get_fn )( iterator, &pair );
            if (pair) SLOT( pair, PAIR_GET_KEY, HRESULT (WINAPI *)( void *, HSTRING * ) )( pair, &got );
            SLOT( iterator, ITERATOR_MOVE_NEXT, HRESULT (WINAPI *)( void *, boolean * ) )( iterator, &has );
        }
        printf( "iterator first=%d key=%ls next=%d\n", has_current, got ? WindowsGetStringRawBuffer( got, NULL ) : L"-", has );
    }
    WindowsDeleteString( key );
    WindowsDeleteString( other );
}

static void check_step( void *visual )
{
    void *easing = NULL, *step, *anim = NULL;
    INT32 count = -1, final = -1;
    double t0;
    float late;

    SLOT( compositor2, COMPOSITOR2_CREATE_STEP, get_fn )( compositor2, &easing );
    step = qi( easing, &probe_IID_IStepEasingFunction );
    if (!step)
    {
        printf( "step=0\n" );
        return;
    }
    SLOT( step, STEP_GET_COUNT, HRESULT (WINAPI *)( void *, INT32 * ) )( step, &count );
    SLOT( step, STEP_GET_FINAL, HRESULT (WINAPI *)( void *, INT32 * ) )( step, &final );
    printf( "step=1 count=%d final=%d\n", count, final );
    SLOT( step, STEP_PUT_COUNT, HRESULT (WINAPI *)( void *, INT32 ) )( step, 4 );
    SLOT( step, STEP_GET_FINAL, HRESULT (WINAPI *)( void *, INT32 * ) )( step, &final );
    SLOT( step, STEP_PUT_IS_INITIAL_SINGLE, HRESULT (WINAPI *)( void *, boolean ) )( step, TRUE );
    SLOT( step, STEP_PUT_IS_FINAL_SINGLE, HRESULT (WINAPI *)( void *, boolean ) )( step, TRUE );
    printf( "step_final_follows=%d\n", final );

    SLOT( visual, VISUAL_PUT_OPACITY, HRESULT (WINAPI *)( void *, float ) )( visual, 0.0f );
    SLOT( compositor, COMPOSITOR_CREATE_SCALAR_KEYFRAME, get_fn )( compositor, &anim );
    SLOT( anim, TYPED_INSERT_EASED, HRESULT (WINAPI *)( void *, float, float, void * ) )(
            anim, 1.0f, 1.0f, qi( easing, &probe_IID_ICompositionEasingFunction ) );
    set_duration( anim, 1000 );
    {
        void *object = qi( visual, &probe_IID_ICompositionObject );
        HSTRING s = hs( L"Opacity" );
        SLOT( object, OBJECT_START_ANIMATION, HRESULT (WINAPI *)( void *, HSTRING, void * ) )(
                object, s, qi( anim, &probe_IID_ICompositionAnimation ) );
        WindowsDeleteString( s );
    }
    t0 = now_ms();
    pump_until( t0, 880 );
    late = get_opacity( visual );
    pump_until( t0, 1300 );
    printf( "step_late=%.2f step_end=%.2f\n", late, get_opacity( visual ) );
}

static LRESULT CALLBACK wndproc( HWND hwnd, UINT msg, WPARAM wp, LPARAM lp )
{
    if (msg == WM_ERASEBKGND) return 1;
    return DefWindowProcW( hwnd, msg, wp, lp );
}

static void check( void )
{
    WNDCLASSW wc = { 0, wndproc, 0, 0, NULL, NULL, NULL, NULL, NULL, L"wuiimplicit" };
    void *desktop, *target = NULL, *ctarget, *root = NULL, *container, *children = NULL;
    void *sprite = NULL, *visual, *object2, *collection = NULL, *got = NULL, *anim;
    IInspectable *inspectable = NULL;
    struct vector2 size = { 50, 50 };
    struct vector3 mid, end, now;
    HSTRING s;
    HWND hwnd;
    double t0;
    HRESULT hr;

    s = hs( L"Windows.UI.Composition.Compositor" );
    hr = RoActivateInstance( s, &inspectable );
    WindowsDeleteString( s );
    if (FAILED(hr)) { printf( "compositor=%#lx\n", hr ); return; }
    compositor = qi( inspectable, &probe_IID_ICompositor );
    compositor2 = qi( inspectable, &probe_IID_ICompositor2 );
    desktop = qi( inspectable, &probe_IID_ICompositorDesktopInterop );
    printf( "compositor2=%d\n", compositor2 != NULL );
    if (!compositor || !compositor2 || !desktop) return;

    wc.hInstance = GetModuleHandleW( NULL );
    RegisterClassW( &wc );
    hwnd = CreateWindowExW( 0, L"wuiimplicit", L"wuiimplicit", WS_POPUP | WS_VISIBLE, 50, 50, 200, 100, NULL, NULL, NULL, NULL );
    pump( 300 );
    SLOT( desktop, DESKTOP_INTEROP_CREATE_TARGET, HRESULT (WINAPI *)( void *, HWND, BOOL, void ** ) )( desktop, hwnd, FALSE, &target );
    if (!target) return;
    ctarget = qi( target, &probe_IID_ICompositionTarget );
    SLOT( compositor, COMPOSITOR_CREATE_CONTAINER_VISUAL, get_fn )( compositor, &root );
    container = qi( root, &probe_IID_IContainerVisual );
    SLOT( container, CONTAINER_GET_CHILDREN, get_fn )( container, &children );
    SLOT( compositor, COMPOSITOR_CREATE_SPRITE_VISUAL, get_fn )( compositor, &sprite );
    visual = qi( sprite, &probe_IID_IVisual );
    SLOT( visual, VISUAL_PUT_SIZE, HRESULT (WINAPI *)( void *, struct vector2 ) )( visual, size );
    SLOT( children, COLLECTION_INSERT_AT_TOP, put_fn )( children, visual );
    SLOT( ctarget, TARGET_PUT_ROOT, put_fn )( ctarget, qi( root, &probe_IID_IVisual ) );

    /* the collection */
    hr = SLOT( compositor2, COMPOSITOR2_CREATE_IMPLICIT, get_fn )( compositor2, &collection );
    printf( "collection=%d\n", SUCCEEDED(hr) && qi( collection, &probe_IID_IImplicitAnimationCollection ) != NULL );
    if (!collection) return;
    anim = offset_animation( 600 );
    check_collection( collection, anim );

    /* set on a visual: Offset changes animate */
    object2 = qi( visual, &probe_IID_ICompositionObject2 );
    printf( "object2=%d\n", object2 != NULL );
    if (!object2) return;
    SLOT( object2, OBJECT2_PUT_IMPLICIT, put_fn )( object2, collection );
    SLOT( object2, OBJECT2_GET_IMPLICIT, get_fn )( object2, &got );
    printf( "implicit_kept=%d\n", same_object( got, collection ) );
    pump( 100 );
    put_offset( visual, 100 );
    t0 = now_ms();
    pump_until( t0, 300 );
    mid = get_offset( visual );
    pump_until( t0, 900 );
    end = get_offset( visual );
    printf( "implicit_mid=%d (%.1f)\nimplicit_end=%.0f\n", mid.x > 5 && mid.x < 95, mid.x, end.x );

    /* no collection: at once */
    SLOT( object2, OBJECT2_PUT_IMPLICIT, put_fn )( object2, NULL );
    put_offset( visual, 20 );
    pump( 50 );
    now = get_offset( visual );
    printf( "no_implicit=%.0f\n", now.x );

    check_step( visual );
    DestroyWindow( hwnd );
}

int main( void )
{
    RoInitialize( RO_INIT_SINGLETHREADED );
    check();
    printf( "done=1\n" );
    fflush( stdout );
    return 0;
}
