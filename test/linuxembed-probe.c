/* For test/linuxembed-gate.sh: windows by class (and title, "" for any).
 *   activate CLASS TITLE      SetForegroundWindow (restored first)
 *   move CLASS TITLE X Y W H  SetWindowPos
 *   wmclose CLASS TITLE       SendMessage WM_CLOSE (End task, the close button)
 *   exists CLASS TITLE        exists=0|1
 *   rect CLASS TITLE          left top right bottom
 *   foreground CLASS TITLE    foreground=0|1 */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main( int argc, char **argv )
{
    HWND hwnd;
    if (argc < 4) return 2;
    hwnd = FindWindowA( argv[2], argv[3][0] ? argv[3] : NULL );
    if (!strcmp( argv[1], "exists" )) { printf( "exists=%d\n", hwnd != NULL ); return 0; }
    if (!strcmp( argv[1], "foreground" ))   /* is CLASS TITLE the foreground window */
    {
        printf( "foreground=%d\n", hwnd && GetForegroundWindow() == hwnd );
        return 0;
    }
    if (!strcmp( argv[1], "zabove" ) && argc == 6)   /* is CLASS TITLE above CLASS2 TITLE2 in Wine's Z order */
    {
        HWND other = FindWindowA( argv[4], argv[5][0] ? argv[5] : NULL ), w;
        int above = 0;
        if (hwnd && other) for (w = GetWindow( hwnd, GW_HWNDNEXT ); w; w = GetWindow( w, GW_HWNDNEXT )) if (w == other) above = 1;
        printf( "above=%d\n", above );
        return 0;
    }
    if (!strcmp( argv[1], "zoomed" )) { printf( "zoomed=%d\n", hwnd && IsZoomed( hwnd ) ); return 0; }
    if (!hwnd) { printf( "none\n" ); return 1; }
    if (!strcmp( argv[1], "activate" ))
    {
        if (IsIconic( hwnd )) ShowWindow( hwnd, SW_RESTORE );
        SetForegroundWindow( hwnd );
    }
    else if (!strcmp( argv[1], "move" ) && argc == 8)
        SetWindowPos( hwnd, 0, atoi( argv[4] ), atoi( argv[5] ), atoi( argv[6] ), atoi( argv[7] ), SWP_NOZORDER | SWP_NOACTIVATE );
    else if (!strcmp( argv[1], "maximize" )) ShowWindow( hwnd, SW_MAXIMIZE );
    else if (!strcmp( argv[1], "restore" )) ShowWindow( hwnd, SW_RESTORE );
    else if (!strcmp( argv[1], "wmclose" )) SendMessageW( hwnd, WM_CLOSE, 0, 0 );
    else if (!strcmp( argv[1], "rect" ))
    {
        RECT r;
        GetWindowRect( hwnd, &r );
        printf( "%ld %ld %ld %ld\n", r.left, r.top, r.right, r.bottom );
        return 0;
    }
    else return 2;
    printf( "done\n" );
    return 0;
}
