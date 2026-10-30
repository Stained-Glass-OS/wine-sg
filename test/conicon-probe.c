/* conicon-probe: a console program's window shows the program's own icon (patches/sg/6201).
 *
 * A console program asks its console window (conhost's, another process) which program it shows: the
 * window's SgConsoleImage property names the program's image, the file the shell takes its icons from
 * (other processes' icon handles cannot be drawn by the shell). The icons conhost put on the window
 * are the program's own (a red square, test/conicon-gate.sh writes it): read from that file here.
 * Writes name=value lines to the file named on its command line.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include <windows.h>
#include <shellapi.h>
#include <stdio.h>

static int red_middle( HICON icon )
{
    ICONINFO info;
    BITMAP bm;
    HDC dc;
    COLORREF c;
    int red;

    if (!icon || !GetIconInfo( icon, &info )) return -1;
    GetObjectW( info.hbmColor, sizeof(bm), &bm );
    dc = CreateCompatibleDC( NULL );
    SelectObject( dc, info.hbmColor );
    c = GetPixel( dc, bm.bmWidth / 2, bm.bmHeight / 2 );
    red = GetRValue( c ) > 200 && GetGValue( c ) < 60 && GetBValue( c ) < 60;
    DeleteDC( dc );
    DeleteObject( info.hbmColor );
    DeleteObject( info.hbmMask );
    return red;
}

int wmain( int argc, WCHAR **argv )
{
    HWND hwnd = GetConsoleWindow();
    HICON big, small;
    FILE *out;

    if (argc < 2 || !(out = _wfopen( argv[1], L"w" ))) return 1;
    Sleep( 500 );
    {
        WCHAR self[MAX_PATH], named[MAX_PATH] = L"";
        ATOM atom = (ATOM)(ULONG_PTR)GetPropW( hwnd, L"SgConsoleImage" );
        if (atom) GlobalGetAtomNameW( atom, named, MAX_PATH );
        GetModuleFileNameW( NULL, self, MAX_PATH );
        fprintf( out, "window=%d\n", hwnd != NULL );
        fprintf( out, "image=%d\n", named[0] && !lstrcmpiW( named, self ) );
        big = small = NULL;
        if (named[0]) ExtractIconExW( named, 0, &big, &small, 1 );
    }
    fprintf( out, "big=%d\n", red_middle( big ) );
    fprintf( out, "small=%d\n", red_middle( small ) );
    fprintf( out, "done=1\n" );
    fclose( out );
    return 0;
}
