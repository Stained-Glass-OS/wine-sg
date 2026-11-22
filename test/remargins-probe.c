/* remargins-probe: rich edit EM_SETMARGINS / EM_GETMARGINS (Stained Glass OS wine-sg 8296;
 * test/remargins-gate.sh): the margins inset the formatting rectangle; they were an unsupported message
 * (CodeLite's output panes set them).
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#include <windows.h>
#include <richedit.h>
#include <stdio.h>

static int fails;
static void check( int ok, const char *what, long a, long b ) { printf( "%s  %s (%ld %ld)\n", ok ? "PASS" : "FAIL", what, a, b ); if (!ok) fails++; }

int main( void )
{
    HWND hwnd;
    RECT before, after;
    POINTL pt0, pt1;
    LRESULT r;

    LoadLibraryA( "riched20.dll" );
    hwnd = CreateWindowExA( 0, "RichEdit20A", NULL, WS_POPUP | WS_VISIBLE | ES_MULTILINE, 0, 0, 300, 100, NULL, NULL, NULL, NULL );
    SendMessageA( hwnd, WM_SETTEXT, 0, (LPARAM)"margin text" );
    SendMessageA( hwnd, EM_GETRECT, 0, (LPARAM)&before );
    SendMessageA( hwnd, EM_POSFROMCHAR, (WPARAM)&pt0, 0 );

    SendMessageA( hwnd, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, MAKELONG( 20, 30 ) );
    r = SendMessageA( hwnd, EM_GETMARGINS, 0, 0 );
    check( LOWORD( r ) == 20 && HIWORD( r ) == 30, "EM_GETMARGINS gives what was set", LOWORD( r ), HIWORD( r ) );
    SendMessageA( hwnd, EM_GETRECT, 0, (LPARAM)&after );
    check( after.left == before.left + 20, "the formatting rectangle starts 20 px further in", before.left, after.left );
    check( after.right == before.right - 30, "and ends 30 px earlier", before.right, after.right );
    SendMessageA( hwnd, EM_POSFROMCHAR, (WPARAM)&pt1, 0 );
    check( pt1.x == pt0.x + 20, "the first character moved with it", pt0.x, pt1.x );

    SendMessageA( hwnd, EM_SETMARGINS, EC_RIGHTMARGIN, MAKELONG( 99, 5 ) );
    r = SendMessageA( hwnd, EM_GETMARGINS, 0, 0 );
    check( LOWORD( r ) == 20 && HIWORD( r ) == 5, "EC_RIGHTMARGIN alone leaves the left one", LOWORD( r ), HIWORD( r ) );

    SendMessageA( hwnd, EM_SETMARGINS, EC_USEFONTINFO, 0 );
    SendMessageA( hwnd, EM_GETRECT, 0, (LPARAM)&after );
    check( after.left == before.left && after.right == before.right, "EC_USEFONTINFO: back to the narrow margins", after.left, after.right );
    printf( "RESULT: %s\n", fails ? "FAIL" : "PASS" );
    return fails != 0;
}
