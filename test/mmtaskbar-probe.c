/* mmtaskbar-probe: the taskbars on several displays (mmtaskbar-gate.sh)
 *
 *   mmtaskbar-probe.exe window TITLE X Y   a window there, until it is closed
 *   mmtaskbar-probe.exe apply              WM_SETTINGCHANGE "TraySettings", as Settings sends it
 *   mmtaskbar-probe.exe state              prints:
 *     main L T R B
 *     second L T R B                       each other display's bar (Shell_SecondaryTrayWnd)
 *     work L T R B                         each display's work area, from left to right
 *     mainbutton TITLE                     the main bar's visible buttons for our windows
 */
#include <windows.h>
#include <stdio.h>
#include <string.h>

static BOOL CALLBACK work( HMONITOR m, HDC dc, LPRECT r, LPARAM p )
{
    MONITORINFO mi = { sizeof(mi) };
    GetMonitorInfoW( m, &mi );
    printf( "work %ld %ld %ld %ld\n", mi.rcWork.left, mi.rcWork.top, mi.rcWork.right, mi.rcWork.bottom );
    return TRUE;
}

static BOOL CALLBACK button( HWND child, LPARAM p )
{
    HWND win = (HWND)GetWindowLongPtrW( child, GWLP_ID );
    char title[64];
    if (IsWindowVisible( child ) && IsWindow( win ) && GetWindowTextA( win, title, sizeof(title) ) &&
        !strncmp( title, "mmwin", 5 ))
        printf( "mainbutton %s\n", title );
    return TRUE;
}

int main( int argc, char **argv )
{
    if (argc >= 5 && !strcmp( argv[1], "window" ))
    {
        WNDCLASSA wc = { .lpfnWndProc = DefWindowProcA, .lpszClassName = "mmwin", .hInstance = GetModuleHandleA( NULL ),
                         .hbrBackground = (HBRUSH)(COLOR_WINDOW + 1) };
        MSG msg;
        HWND w;
        RegisterClassA( &wc );
        w = CreateWindowA( "mmwin", argv[2], WS_OVERLAPPEDWINDOW | WS_VISIBLE, atoi( argv[3] ), atoi( argv[4] ), 300, 200,
                           NULL, NULL, wc.hInstance, NULL );
        if (!w) return 1;
        SetTimer( w, 1, 120000, NULL );
        while (GetMessageA( &msg, NULL, 0, 0 ) && msg.message != WM_TIMER) DispatchMessageA( &msg );
        return 0;
    }
    if (argc >= 2 && !strcmp( argv[1], "apply" ))
    {
        SendMessageTimeoutW( HWND_BROADCAST, WM_SETTINGCHANGE, 0, (LPARAM)L"TraySettings", SMTO_ABORTIFHUNG, 3000, NULL );
        return 0;
    }
    if (argc >= 2 && !strcmp( argv[1], "state" ))
    {
        HWND tray = FindWindowW( L"Shell_TrayWnd", NULL ), bar = NULL;
        RECT r;
        if (tray && GetWindowRect( tray, &r )) printf( "main %ld %ld %ld %ld\n", r.left, r.top, r.right, r.bottom );
        while ((bar = FindWindowExW( NULL, bar, L"Shell_SecondaryTrayWnd", NULL )))
            if (IsWindowVisible( bar ) && GetWindowRect( bar, &r ))
                printf( "second %ld %ld %ld %ld\n", r.left, r.top, r.right, r.bottom );
        EnumDisplayMonitors( NULL, NULL, work, 0 );
        if (tray) EnumChildWindows( tray, button, 0 );
        return 0;
    }
    return 2;
}
