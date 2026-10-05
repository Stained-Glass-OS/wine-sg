/* For test/linuxembed-gate.sh: windows by class (and title, "" for any).
 *   activate CLASS TITLE      SetForegroundWindow (restored first)
 *   move CLASS TITLE X Y W H  SetWindowPos
 *   wmclose CLASS TITLE       SendMessage WM_CLOSE (End task, the close button)
 *   endtask CLASS TITLE       SendMessage SgLinuxWindowEnd (Task Manager's End
 *                             task when the program did not close)
 *   exists CLASS TITLE        exists=0|1
 *   rect CLASS TITLE          left top right bottom
 *   foreground CLASS TITLE    foreground=0|1
 *   zoomed / iconic CLASS TITLE  zoomed=0|1, iconic=0|1
 *   list CLASS -              the top-level windows of CLASS: title, shown
 *   watch CLASS SECONDS       seen=N: how often (every 20 ms) one was there
 *   fglog SECONDS - -         each window made the foreground window for
 *                             SECONDS: "fg CLASS|TITLE", a line each */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void CALLBACK fg_event( HWINEVENTHOOK hook, DWORD event, HWND hwnd, LONG obj, LONG child, DWORD thread, DWORD time )
{
    char class[64] = "", title[128] = "";
    GetClassNameA( hwnd, class, sizeof(class) );
    GetWindowTextA( hwnd, title, sizeof(title) );
    printf( "fg %s|%s\n", class, title );
    fflush( stdout );
}

static BOOL CALLBACK list_window( HWND hwnd, LPARAM lp )
{
    char class[64] = "", title[128] = "";
    GetClassNameA( hwnd, class, sizeof(class) );
    if (strcmp( class, (const char *)lp )) return TRUE;
    GetWindowTextA( hwnd, title, sizeof(title) );
    printf( "%p '%s'%s%s\n", hwnd, title, IsWindowVisible( hwnd ) ? " visible" : " hidden",
            GetWindowLongA( hwnd, GWL_EXSTYLE ) & WS_EX_TOOLWINDOW ? " tool" : "" );
    return TRUE;
}

int main( int argc, char **argv )
{
    HWND hwnd;
    if (argc < 4) return 2;
    if (!strcmp( argv[1], "list" )) { EnumWindows( list_window, (LPARAM)argv[2] ); return 0; }
    if (!strcmp( argv[1], "watch" ))   /* watch CLASS SECONDS: was a window of CLASS there meanwhile */
    {
        DWORD end = GetTickCount() + atoi( argv[3] ) * 1000;
        int seen = 0;
        printf( "watching\n" );
        fflush( stdout );
        while ((LONG)(end - GetTickCount()) > 0)
        {
            if (FindWindowA( argv[2], NULL )) seen++;
            Sleep( 20 );
        }
        printf( "seen=%d\n", seen );
        return 0;
    }
    if (!strcmp( argv[1], "fglog" ))
    {
        DWORD end = GetTickCount() + atoi( argv[2] ) * 1000;
        MSG msg;
        if (!SetWinEventHook( EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND, NULL, fg_event, 0, 0,
                              WINEVENT_OUTOFCONTEXT )) return 1;
        printf( "logging\n" );
        fflush( stdout );
        while ((LONG)(end - GetTickCount()) > 0)
        {
            MsgWaitForMultipleObjects( 0, NULL, FALSE, 100, QS_ALLINPUT );
            while (PeekMessageA( &msg, 0, 0, 0, PM_REMOVE )) DispatchMessageA( &msg );
        }
        return 0;
    }
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
        int n = 0;
        /* bounded: Z order changing as it is walked once went round for ever */
        if (hwnd && other)
            for (w = GetWindow( hwnd, GW_HWNDNEXT ); w && n < 10000; w = GetWindow( w, GW_HWNDNEXT ), n++)
                if (w == other) { above = 1; break; }
        printf( "above=%d\n", above );
        return 0;
    }
    if (!strcmp( argv[1], "zoomed" )) { printf( "zoomed=%d\n", hwnd && IsZoomed( hwnd ) ); return 0; }
    if (!strcmp( argv[1], "iconic" )) { printf( "iconic=%d\n", hwnd && IsIconic( hwnd ) ); return 0; }
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
    else if (!strcmp( argv[1], "minimize" )) ShowWindow( hwnd, SW_MINIMIZE );
    else if (!strcmp( argv[1], "wmclose" )) SendMessageW( hwnd, WM_CLOSE, 0, 0 );
    else if (!strcmp( argv[1], "endtask" )) SendMessageW( hwnd, RegisterWindowMessageW( L"SgLinuxWindowEnd" ), 0, 0 );
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
