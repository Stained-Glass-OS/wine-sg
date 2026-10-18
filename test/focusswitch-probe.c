/* focusswitch-probe: "place" puts Notepad at 50,50 and the Registry Editor at
 * 300,150 (600x400 each, overlapping); otherwise prints the foreground window's
 * title, whether Notepad is above the Registry Editor in Wine's order, and how
 * many characters Notepad's editor holds. test/focusswitch-gate.sh
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#include <windows.h>
#include <stdio.h>

int main( int argc, char **argv )
{
    HWND n = FindWindowW( L"Notepad", NULL ), r = FindWindowW( L"RegEdit_RegEdit", NULL ), w, ed;
    WCHAR t[64] = L"";
    int i = 0, npos = -1, rpos = -1;
    if (argc > 1)
    {
        SetWindowPos( n, NULL, 50, 50, 600, 400, SWP_NOZORDER );
        SetWindowPos( r, NULL, 300, 150, 600, 400, SWP_NOZORDER );
        return 0;
    }
    GetWindowTextW( GetForegroundWindow(), t, 64 );
    for (w = GetTopWindow( NULL ); w && i < 500; w = GetWindow( w, GW_HWNDNEXT ), i++)
    {
        if (w == n && npos < 0) npos = i;
        if (w == r && rpos < 0) rpos = i;
    }
    ed = FindWindowExW( n, NULL, L"SgNotepadEditor", NULL );
    printf( "fg=%ls notepad_above=%d chars=%ld\n", t, npos >= 0 && npos < rpos,
            ed ? (long)SendMessageW( ed, WM_GETTEXTLENGTH, 0, 0 ) : -1L );
    return 0;
}
