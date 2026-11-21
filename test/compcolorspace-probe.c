/* compcolorspace-probe: Windows.UI.Composition ColorKeyFrameAnimation.InterpolationColorSpace -- a colour
 * animation red to blue held half way by its AnimationController, in each colour space (Stained Glass OS
 * wine-sg 8235; test/compcolorspace-gate.sh). Colours were interpolated in RGB whatever was asked for.
 * Slots counted from the IDL; IIDs are the public ones.
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <roapi.h>
#include <winstring.h>
#include <stdio.h>
#include <stdlib.h>

#define SLOT(obj, n, type) ((type)((*(void ***)(obj))[n]))

DEFINE_GUID(IID_Compositor_, 0xb403ca50, 0x7f8c, 0x4e83, 0x98, 0x5f, 0xcc, 0x45, 0x06, 0x00, 0x36, 0xd8);
DEFINE_GUID(IID_CompObject_, 0xbcb4ad45, 0x7609, 0x4550, 0x93, 0x4f, 0x16, 0x00, 0x2a, 0x68, 0xfd, 0xed);
DEFINE_GUID(IID_CompObject4_, 0x0bb3784c, 0x346b, 0x4a7c, 0x96, 0x6b, 0x73, 0x10, 0x96, 0x65, 0x53, 0xd5);
DEFINE_GUID(IID_KeyFrame_, 0x126e7f22, 0x3ae9, 0x4540, 0x9a, 0x8a, 0xde, 0xae, 0x8a, 0x4a, 0x4a, 0x84);
DEFINE_GUID(IID_Animation_, 0x464c4c2c, 0x1caa, 0x4061, 0x9b, 0x40, 0xe1, 0x3f, 0xde, 0x15, 0x03, 0xca);
DEFINE_GUID(IID_ColorKeyFrame_, 0x93adb5e9, 0x8e05, 0x4593, 0x84, 0xa3, 0xdc, 0xa1, 0x52, 0x78, 0x1e, 0x56);
DEFINE_GUID(IID_EasingFunction_, 0x5145e356, 0xbf79, 0x4ea8, 0x8c, 0xc2, 0x6b, 0x5b, 0x47, 0x2e, 0x6c, 0x9a);

struct ui_color { BYTE a, r, g, b; };
struct timespan { INT64 duration; };

static void *qi( void *obj, const GUID *iid )
{
    void *out = NULL;
    if (obj) IUnknown_QueryInterface( (IUnknown *)obj, iid, &out );
    return out;
}

static HSTRING hs( const WCHAR *s )
{
    HSTRING h = NULL;
    WindowsCreateString( s, lstrlenW( s ), &h );
    return h;
}

static void pump( DWORD ms )
{
    DWORD end = GetTickCount() + ms;
    MSG msg;
    do
    {
        while (PeekMessageW( &msg, NULL, 0, 0, PM_REMOVE )) DispatchMessageW( &msg );
        Sleep( 5 );
    } while ((int)(end - GetTickCount()) > 0);
}

static int failures;
static void check( int ok, const char *what, struct ui_color c )
{
    printf( "%s  %s (%u %u %u %u)\n", ok ? "PASS" : "FAIL", what, c.a, c.r, c.g, c.b );
    if (!ok) failures++;
}

static int close_color( struct ui_color c, int a, int r, int g, int b )
{
    return abs( c.a - a ) <= 3 && abs( c.r - r ) <= 3 && abs( c.g - g ) <= 3 && abs( c.b - b ) <= 3;
}

int main( void )
{
    static const WCHAR *names[] = { L"Rgb", L"Hsl", L"RgbLinear", L"HslLinear", L"Auto" };
    static const int spaces[] = { 2, 1, 4, 3, 0 };
    struct ui_color red = { 255, 255, 0, 0 }, blue = { 255, 0, 0, 255 }, got[5];
    void *inspectable = NULL, *compositor, *set = NULL, *linear = NULL, *controllers[5] = { 0 };
    HRESULT hr;
    int i;

    setvbuf( stdout, NULL, _IONBF, 0 );
    RoInitialize( RO_INIT_SINGLETHREADED );
    hr = RoActivateInstance( hs( L"Windows.UI.Composition.Compositor" ), (IInspectable **)&inspectable );
    if (!(compositor = qi( inspectable, &IID_Compositor_ ))) { printf( "FAIL  no compositor (%#lx)\nRESULT: FAIL\n", hr ); return 1; }
    SLOT( compositor, 18, HRESULT (WINAPI *)( void *, void ** ) )( compositor, &set );          /* CreatePropertySet */
    SLOT( compositor, 17, HRESULT (WINAPI *)( void *, void ** ) )( compositor, &linear );       /* CreateLinearEasingFunction */
    if (!set || !linear) { printf( "FAIL  property set / easing\nRESULT: FAIL\n" ); return 1; }

    for (i = 0; i < 5; i++)
    {
        void *anim = NULL, *color, *obj4;
        struct timespan ten = { 100000000 };   /* ten seconds: it stays where its controller holds it */
        SLOT( set, 6, HRESULT (WINAPI *)( void *, HSTRING, struct ui_color ) )( set, hs( names[i] ), red );   /* InsertColor */
        SLOT( compositor, 6, HRESULT (WINAPI *)( void *, void ** ) )( compositor, &anim );     /* CreateColorKeyFrameAnimation */
        color = qi( anim, &IID_ColorKeyFrame_ );
        hr = SLOT( color, 7, HRESULT (WINAPI *)( void *, int ) )( color, spaces[i] );          /* put_InterpolationColorSpace */
        if (hr != S_OK) printf( "      put_InterpolationColorSpace(%d) %#lx\n", spaces[i], hr );
        SLOT( color, 9, HRESULT (WINAPI *)( void *, float, struct ui_color, void * ) )( color, 0.0f, red, qi( linear, &IID_EasingFunction_ ) );
        SLOT( color, 9, HRESULT (WINAPI *)( void *, float, struct ui_color, void * ) )( color, 1.0f, blue, qi( linear, &IID_EasingFunction_ ) );
        SLOT( qi( anim, &IID_KeyFrame_ ), 9, HRESULT (WINAPI *)( void *, struct timespan ) )( qi( anim, &IID_KeyFrame_ ), ten );
        SLOT( qi( set, &IID_CompObject_ ), 9, HRESULT (WINAPI *)( void *, HSTRING, void * ) )( qi( set, &IID_CompObject_ ),
                hs( names[i] ), qi( anim, &IID_Animation_ ) );                                   /* StartAnimation */
        if ((obj4 = qi( set, &IID_CompObject4_ )))
            SLOT( obj4, 6, HRESULT (WINAPI *)( void *, HSTRING, void ** ) )( obj4, hs( names[i] ), &controllers[i] );
        if (controllers[i])
        {
            SLOT( controllers[i], 12, HRESULT (WINAPI *)( void * ) )( controllers[i] );                 /* Pause */
            SLOT( controllers[i], 9, HRESULT (WINAPI *)( void *, float ) )( controllers[i], 0.5f );     /* Progress */
        }
        else printf( "      no controller for %ls\n", names[i] );
    }
    pump( 600 );
    for (i = 0; i < 5; i++)
    {
        int status = -1;
        memset( &got[i], 0, sizeof(got[i]) );
        SLOT( set, 14, HRESULT (WINAPI *)( void *, HSTRING, struct ui_color *, int * ) )( set, hs( names[i] ), &got[i], &status );   /* TryGetColor */
    }
    check( close_color( got[0], 255, 128, 0, 128 ), "Rgb: half way in sRGB values", got[0] );
    check( close_color( got[1], 255, 255, 0, 255 ), "Hsl: the hue the short way, red to blue through magenta", got[1] );
    check( close_color( got[2], 255, 188, 0, 188 ), "RgbLinear: half way in linear light", got[2] );
    check( close_color( got[3], 255, 255, 0, 255 ), "HslLinear: hue the short way on linear values", got[3] );
    check( close_color( got[4], 255, 128, 0, 128 ), "Auto: as Rgb", got[4] );
    printf( "RESULT: %s\n", failures ? "FAIL" : "PASS" );
    return failures != 0;
}
