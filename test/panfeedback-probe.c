/* panfeedback-probe: uxtheme BeginPanningFeedback / UpdatePanningFeedback / EndPanningFeedback, which were not
 * exported (Stained Glass OS wine-sg 8255; test/panfeedback-gate.sh): Firefox and Thunderbird import them.
 * The top-level window moves by the overpan (at most 30 pixels) and goes back at the end.
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#include <windows.h>
#include <stdio.h>

static int failures;
static void check( int ok, const char *what, RECT *r )
{
    printf( "%s  %s (%ld,%ld)\n", ok ? "PASS" : "FAIL", what, r ? r->left : 0, r ? r->top : 0 );
    if (!ok) failures++;
}

int main( void )
{
    HMODULE ux = LoadLibraryA( "uxtheme.dll" );
    BOOL (WINAPI *pBegin)(HWND) = (void *)GetProcAddress( ux, "BeginPanningFeedback" );
    BOOL (WINAPI *pUpdate)(HWND, LONG, LONG, BOOL) = (void *)GetProcAddress( ux, "UpdatePanningFeedback" );
    BOOL (WINAPI *pEnd)(HWND, BOOL) = (void *)GetProcAddress( ux, "EndPanningFeedback" );
    HWND top, child;
    RECT r;
    BOOL ret;

    if (!pBegin || !pUpdate || !pEnd) { printf( "FAIL  uxtheme exports the three functions\nRESULT: FAIL\n" ); return 1; }
    printf( "PASS  uxtheme exports the three functions\n" );
    top = CreateWindowExA( 0, "STATIC", "pan", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 100, 100, 200, 150, NULL, NULL, NULL, NULL );
    child = CreateWindowExA( 0, "STATIC", "content", WS_CHILD | WS_VISIBLE, 0, 0, 100, 50, top, NULL, NULL, NULL );

    check( !pUpdate( child, 5, 5, FALSE ), "Update before Begin fails", NULL );
    check( !pBegin( (HWND)0xdead0 ), "Begin of no window fails", NULL );
    ret = pBegin( child );
    ret = ret && pUpdate( child, 10, 20, FALSE );
    GetWindowRect( top, &r );
    check( ret && r.left == 110 && r.top == 120, "Update(10, 20) of the child moves its top-level window", &r );
    pUpdate( child, 50, -50, TRUE );
    GetWindowRect( top, &r );
    check( r.left == 130 && r.top == 70, "  overpan is limited to 30 pixels each way (totals, not increments)", &r );
    ret = pEnd( child, TRUE );
    GetWindowRect( top, &r );
    check( ret && r.left == 100 && r.top == 100, "End puts it back", &r );
    check( !pEnd( child, FALSE ), "End again fails (nothing begun)", NULL );
    DestroyWindow( top );
    printf( "RESULT: %s\n", failures ? "FAIL" : "PASS" );
    return failures != 0;
}
