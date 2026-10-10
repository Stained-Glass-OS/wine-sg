/* EvaluateProximityToRect/Polygon, Get/SetDisplayAutoRotationPreferences and
 * AnimateWindow (patches/sg/2225).  AnimateWindow is sampled from a second
 * thread while the (synchronous) call runs: alpha for AW_BLEND, the window
 * region's size for the roll/centre variants. */
#include <windows.h>
#include <stdio.h>

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

static volatile LONG sampling, nsamples;
static int samples[4096];
static HWND g_hwnd;
static int g_mode; /* 0 alpha, 1 region width, 2 region height */

static DWORD WINAPI sampler(void *arg)
{
    while (sampling)
    {
        int v = -1;
        if (g_mode == 0)
        {
            BYTE a; DWORD f;
            if (GetLayeredWindowAttributes( g_hwnd, NULL, &a, &f )) v = a;
        }
        else
        {
            RECT r;
            int t = GetWindowRgnBox( g_hwnd, &r );
            if (t != ERROR) v = g_mode == 1 ? r.right - r.left : r.bottom - r.top;
        }
        if (v >= 0 && nsamples < 4096) samples[nsamples++] = v;
        Sleep( 4 );
    }
    return 0;
}

/* run an animation, return elapsed ms; samples are left in samples[] */
static DWORD animate(HWND hwnd, DWORD time, DWORD flags, int mode, BOOL *ret)
{
    HANDLE th;
    DWORD t0;
    g_hwnd = hwnd; g_mode = mode; nsamples = 0; sampling = 1;
    th = CreateThread( NULL, 0, sampler, NULL, 0, NULL );
    t0 = GetTickCount();
    *ret = AnimateWindow( hwnd, time, flags );
    t0 = GetTickCount() - t0;
    sampling = 0;
    WaitForSingleObject( th, 2000 );
    CloseHandle( th );
    return t0;
}

/* count distinct values; dir>0 expects non-decreasing, dir<0 non-increasing */
static int analyse(int dir, int *distinct)
{
    int i, mono = 1, d = 0;
    for (i = 0; i < nsamples; i++)
    {
        if (i && samples[i] != samples[i - 1]) d++;
        if (i && dir * (samples[i] - samples[i - 1]) < 0) mono = 0;
    }
    *distinct = d + (nsamples > 0);
    return mono;
}

static HWND make_window(DWORD style, HWND parent)
{
    return CreateWindowExA( 0, "static", "anim", style, 100, 100, 300, 200, parent, NULL, NULL, NULL );
}

int main(void)
{
    TOUCH_HIT_TESTING_INPUT in = { 0 };
    TOUCH_HIT_TESTING_PROXIMITY_EVALUATION ev;
    RECT box = { 100, 100, 200, 200 };
    POINT tri[3] = { {0,0}, {100,0}, {0,100} };
    ORIENTATION_PREFERENCE pref;
    BOOL ret;
    HWND w, child;
    DWORD el;
    int distinct, mono, ex;
    RECT r;
    BYTE alpha; DWORD fl;

    /* proximity to a rectangle */
    in.point.x = 150; in.point.y = 150;
    memset( &ev, 0xcc, sizeof(ev) );
    ret = EvaluateProximityToRect( &box, &in, &ev );
    check( ret && ev.score == 0 && ev.adjustedPoint.x == 150 && ev.adjustedPoint.y == 150, "point inside the box: score 0, point unchanged" );
    in.point.x = 70; in.point.y = 150;
    ret = EvaluateProximityToRect( &box, &in, &ev );
    check( ret && ev.score == 30 && ev.adjustedPoint.x == 100 && ev.adjustedPoint.y == 150, "point 30 left of the box: score 30, adjusted to the edge" );
    in.point.x = 5000; in.point.y = 5000;
    ret = EvaluateProximityToRect( &box, &in, &ev );
    check( ret && ev.score == TOUCH_HIT_TESTING_PROXIMITY_FARTHEST && ev.adjustedPoint.x == 199 && ev.adjustedPoint.y == 199, "far point: score capped at 0xfff, adjusted to the nearest corner pixel" );
    SetLastError( 0 );
    ret = EvaluateProximityToRect( NULL, &in, &ev );
    check( !ret && GetLastError() == ERROR_INVALID_PARAMETER, "NULL box fails with ERROR_INVALID_PARAMETER" );
    SetLastError( 0 );
    ret = EvaluateProximityToRect( &box, NULL, &ev );
    check( !ret && GetLastError() == ERROR_INVALID_PARAMETER, "NULL input fails with ERROR_INVALID_PARAMETER" );
    SetLastError( 0 );
    ret = EvaluateProximityToRect( &box, &in, NULL );
    check( !ret && GetLastError() == ERROR_INVALID_PARAMETER, "NULL result fails with ERROR_INVALID_PARAMETER" );

    /* polygon */
    in.point.x = 10; in.point.y = 10;
    ret = EvaluateProximityToPolygon( 3, tri, &in, &ev );
    check( ret && ev.score == 0 && ev.adjustedPoint.x == 10 && ev.adjustedPoint.y == 10, "point inside the triangle: score 0" );
    in.point.x = 100; in.point.y = 100;
    ret = EvaluateProximityToPolygon( 3, tri, &in, &ev );
    check( ret && ev.score == 71 && ev.adjustedPoint.x == 50 && ev.adjustedPoint.y == 50, "point beyond the hypotenuse: nearest edge point (50,50), score 71" );
    SetLastError( 0 );
    ret = EvaluateProximityToPolygon( 0, tri, &in, &ev );
    check( !ret && GetLastError() == ERROR_INVALID_PARAMETER, "no vertices fails with ERROR_INVALID_PARAMETER" );
    SetLastError( 0 );
    ret = EvaluateProximityToPolygon( 3, NULL, &in, &ev );
    check( !ret && GetLastError() == ERROR_INVALID_PARAMETER, "NULL vertices fails with ERROR_INVALID_PARAMETER" );

    /* auto-rotation preferences */
    pref = 99;
    ret = GetDisplayAutoRotationPreferences( &pref );
    check( ret && pref == ORIENTATION_PREFERENCE_NONE, "default preference is NONE" );
    ret = SetDisplayAutoRotationPreferences( ORIENTATION_PREFERENCE_LANDSCAPE | ORIENTATION_PREFERENCE_PORTRAIT_FLIPPED );
    pref = 99;
    GetDisplayAutoRotationPreferences( &pref );
    check( ret && pref == (ORIENTATION_PREFERENCE_LANDSCAPE | ORIENTATION_PREFERENCE_PORTRAIT_FLIPPED), "preference reads back what was set" );
    SetLastError( 0 );
    ret = SetDisplayAutoRotationPreferences( 0x10 | ORIENTATION_PREFERENCE_PORTRAIT );
    GetDisplayAutoRotationPreferences( &pref );
    check( !ret && GetLastError() == ERROR_INVALID_PARAMETER && pref == (ORIENTATION_PREFERENCE_LANDSCAPE | ORIENTATION_PREFERENCE_PORTRAIT_FLIPPED), "unknown bits rejected, value kept" );
    ret = SetDisplayAutoRotationPreferences( ORIENTATION_PREFERENCE_NONE );
    GetDisplayAutoRotationPreferences( &pref );
    check( ret && pref == ORIENTATION_PREFERENCE_NONE, "NONE clears the preference" );
    SetLastError( 0 );
    ret = GetDisplayAutoRotationPreferences( NULL );
    check( !ret && GetLastError() == ERROR_INVALID_PARAMETER, "NULL result fails" );

    /* AnimateWindow */
    w = make_window( WS_POPUP, NULL );
    SetLastError( 0 );
    ret = AnimateWindow( w, 100, AW_HIDE );
    check( !ret && GetLastError() == ERROR_INVALID_PARAMETER, "hiding a hidden window fails" );
    ret = AnimateWindow( NULL, 100, AW_BLEND );
    check( !ret && GetLastError() == ERROR_INVALID_PARAMETER, "invalid window fails" );

    ex = GetWindowLongW( w, GWL_EXSTYLE );
    el = animate( w, 400, AW_BLEND, 0, &ret );
    mono = analyse( 1, &distinct );
    check( ret && IsWindowVisible( w ), "AW_BLEND show succeeds and the window is visible" );
    check( el >= 380 && el < 1500, "AW_BLEND show takes about the given time" );
    check( distinct >= 4 && mono, "alpha rises through several values while it runs" );
    check( GetWindowLongW( w, GWL_EXSTYLE ) == ex, "the window is not layered afterwards" );

    el = animate( w, 400, AW_BLEND | AW_HIDE, 0, &ret );
    mono = analyse( -1, &distinct );
    check( ret && !IsWindowVisible( w ) && el >= 380, "AW_BLEND hide hides after the time" );
    check( distinct >= 4 && mono, "alpha falls through several values while hiding" );
    check( GetWindowLongW( w, GWL_EXSTYLE ) == ex, "still not layered afterwards" );

    /* an already layered window keeps its alpha */
    SetWindowLongW( w, GWL_EXSTYLE, ex | WS_EX_LAYERED );
    SetLayeredWindowAttributes( w, 0, 128, LWA_ALPHA );
    animate( w, 200, AW_BLEND, 0, &ret );
    alpha = 0; fl = 0;
    GetLayeredWindowAttributes( w, NULL, &alpha, &fl );
    check( ret && alpha == 128 && (fl & LWA_ALPHA), "a layered window's alpha is restored" );
    check( (GetWindowLongW( w, GWL_EXSTYLE ) & WS_EX_LAYERED) != 0, "it stays layered" );
    ShowWindow( w, SW_HIDE );
    SetWindowLongW( w, GWL_EXSTYLE, ex );

    /* roll, horizontal */
    el = animate( w, 400, AW_HOR_POSITIVE, 1, &ret );
    mono = analyse( 1, &distinct );
    check( ret && IsWindowVisible( w ) && el >= 380, "roll show succeeds, takes the time" );
    check( distinct >= 4 && mono, "region width grows through several values" );
    check( GetWindowRgnBox( w, &r ) == ERROR, "no region left afterwards" );
    el = animate( w, 400, AW_HOR_NEGATIVE | AW_HIDE, 1, &ret );
    mono = analyse( -1, &distinct );
    check( ret && !IsWindowVisible( w ), "roll hide hides" );
    check( distinct >= 4 && mono, "region width shrinks through several values" );
    check( GetWindowRgnBox( w, &r ) == ERROR, "no region left after hiding" );

    /* centre, vertical */
    el = animate( w, 300, AW_CENTER | AW_SLIDE, 2, &ret );
    mono = analyse( 1, &distinct );
    check( ret && IsWindowVisible( w ) && distinct >= 3 && mono, "centre reveal grows the region height" );
    ShowWindow( w, SW_HIDE );

    /* a window with its own region is not animated through the region */
    {
        HRGN rgn = CreateRectRgn( 0, 0, 50, 50 );
        SetWindowRgn( w, rgn, FALSE );
        ret = AnimateWindow( w, 200, AW_HOR_POSITIVE );
        GetWindowRgnBox( w, &r );
        check( ret && IsWindowVisible( w ) && r.right == 50 && r.bottom == 50, "the window's own region is kept" );
        ShowWindow( w, SW_HIDE );
        SetWindowRgn( w, NULL, FALSE );
    }
    DestroyWindow( w );

    /* AW_BLEND is not allowed on a child */
    w = make_window( WS_POPUP | WS_VISIBLE, NULL );
    child = make_window( WS_CHILD, w );
    SetLastError( 0 );
    ret = AnimateWindow( child, 100, AW_BLEND );
    check( !ret && GetLastError() == ERROR_INVALID_PARAMETER, "AW_BLEND on a child window fails" );
    DestroyWindow( w );

    printf( failures ? "RESULT: FAIL\n" : "RESULT: PASS\n" );
    return failures != 0;
}
