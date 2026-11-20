/* usptabs-probe: far-edge tab stops in ScriptStringAnalyse (Stained Glass OS wine-sg 8223). A negative entry of
 * SCRIPT_TABDEF.pTabStops is a far-edge stop: the text after the tab ends at it (Microsoft's SCRIPT_TABDEF
 * documentation). It was treated as a near stop (with a FIXME). Prints name=value lines.
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#include <windows.h>
#include <usp10.h>
#include <stdio.h>

static int end_of( HDC dc, const WCHAR *text, SCRIPT_TABDEF *tabs, int *start_x )
{
    SCRIPT_STRING_ANALYSIS ssa = NULL;
    int len = lstrlenW( text ), x = -1, tab = 0;
    HRESULT hr;

    while (text[tab] && text[tab] != '\t') tab++;
    hr = ScriptStringAnalyse( dc, text, len, len * 2 + 16, -1, SSA_GLYPHS | SSA_TAB, 0, NULL, NULL, NULL, tabs, NULL, &ssa );
    if (FAILED(hr)) { printf( "      analyse %08lx\n", hr ); return -1; }
    ScriptStringCPtoX( ssa, len - 1, TRUE, &x );          /* the trailing edge of the last character */
    ScriptStringCPtoX( ssa, tab + 1, FALSE, start_x );    /* the leading edge of the first after the tab */
    ScriptStringFree( &ssa );
    return x;
}

int main( void )
{
    HDC dc = CreateCompatibleDC( NULL );
    HFONT font = CreateFontW( -16, 0, 0, 0, FW_NORMAL, 0, 0, 0, ANSI_CHARSET, 0, 0, 0, 0, L"Arial" );
    int far_stops[] = { -200, 300 }, near_stops[] = { 100, 300 }, both[] = { 60, -200 };
    SCRIPT_TABDEF far_def = { 2, 4, far_stops, 0 }, near_def = { 2, 4, near_stops, 0 }, mixed_def = { 2, 4, both, 0 };
    int end, start;

    SelectObject( dc, font );
    end = end_of( dc, L"ab\tXYZ", &far_def, &start );
    printf( "far_end=%d (text after the tab ends at %d, starts at %d; want end 200)\n", end == 200, end, start );
    end = end_of( dc, L"ab\tXYZ", &near_def, &start );
    printf( "near_start=%d (starts at %d; want 100)\n", start == 100, start );
    /* the first stop (near, 60) is passed by "abcdefgh", the far one at 200 takes the tab */
    end = end_of( dc, L"abcdefghij\tXY", &mixed_def, &start );
    printf( "mixed_far=%d (end %d start %d; want end 200)\n", end == 200, end, start );
    /* text too wide for the far stop: the default stops after it */
    end = end_of( dc, L"ab\tXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX", &far_def, &start );
    printf( "far_too_wide=%d (start %d end %d)\n", start >= 0 && end > 200, start, end );
    printf( "done=1\n" );
    return 0;
}
