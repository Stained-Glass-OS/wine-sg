/* For test/embedthumb-gate.sh: the taskbar's hover preview.
 *   button CLASS TITLE  "L T R B" of the taskbar button for that window
 *                       (a button of the tray whose control id is the window)
 *   thumb               "visible=0|1 L T R B" of the preview popup
 *   menu                "menu=0|1": a menu (#32768) is shown */
#include <windows.h>
#include <stdio.h>
#include <string.h>

static HWND target, found;

static BOOL CALLBACK find_button( HWND hwnd, LPARAM lp )
{
    char class[32];
    if (GetClassNameA( hwnd, class, sizeof(class) ) && !_stricmp( class, "Button" ) &&
        (HWND)GetWindowLongPtrW( hwnd, GWLP_ID ) == target && IsWindowVisible( hwnd ))
    {
        found = hwnd;
        return FALSE;
    }
    return TRUE;
}

int main( int argc, char **argv )
{
    RECT rc;
    HWND hwnd;

    if (argc < 2) return 2;
    if (!strcmp( argv[1], "button" ) && argc == 4)
    {
        HWND tray = FindWindowA( "Shell_TrayWnd", NULL );
        if (!(target = FindWindowA( argv[2], argv[3][0] ? argv[3] : NULL )) || !tray) { printf( "none\n" ); return 1; }
        EnumChildWindows( tray, find_button, 0 );
        if (!found || !GetWindowRect( found, &rc )) { printf( "none\n" ); return 1; }
        printf( "%ld %ld %ld %ld\n", rc.left, rc.top, rc.right, rc.bottom );
        return 0;
    }
    if (!strcmp( argv[1], "thumb" ))
    {
        hwnd = FindWindowA( "SGTaskbarThumbnails", NULL );
        if (hwnd && IsWindowVisible( hwnd ) && GetWindowRect( hwnd, &rc ))
            printf( "visible=1 %ld %ld %ld %ld\n", rc.left, rc.top, rc.right, rc.bottom );
        else printf( "visible=0\n" );
        return 0;
    }
    if (!strcmp( argv[1], "menu" ))
    {
        hwnd = FindWindowA( "#32768", NULL );
        printf( "menu=%d\n", hwnd && IsWindowVisible( hwnd ) );
        return 0;
    }
    return 2;
}
