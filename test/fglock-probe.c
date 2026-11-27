/* fglock-probe: windows and commands for test/fglock-gate.sh (wine-sg 9930-9931, the foreground lock).
 *   fglock-probe win NAME [X Y [DELAY]] a top-level window (made after DELAY ms) titled NAME with an edit box; runs until closed.
 *                              Commands come as WM_APP messages (fglock-probe cmd); each one's result
 *                              is the window property "fgres" (the value + 1).
 *   fglock-probe bridge NAME X Y        a Unix sh starts "win NAME X Y" again (Setup's bridge), this one gone
 *   fglock-probe launch NAME X Y DELAY   "win NAME X Y DELAY" in a process of its own, this one gone at once
 *   fglock-probe cmd NAME CMD [ARG]
 *        setfg       SetForegroundWindow on its own window
 *        allow PID   AllowSetForegroundWindow(PID); "allow any" is ASFW_ANY; "allow NAME2" NAME2's process
 *        lock|unlock LockSetForegroundWindow
 *        spawn NAME2 CreateProcess "fglock-probe win NAME2" (its child); "bridge:NAME2": through a Unix sh;
 *                    "stub:NAME2": through a launcher that is gone before the window shows
 *        timeout MS  SPI_SETFOREGROUNDLOCKTIMEOUT (written to the registry)
 *        menu        open its window menu (TrackPopupMenu) until "cmd NAME endmenu"
 *        endmenu     close it
 *   fglock-probe result NAME   the last command's result
 *   fglock-probe query NAME... "fg=<title> active=<NAME:its thread's active window is it>" and for each
 *                              NAME "NAME text=<edit text> over=<names it is in front of>"
 *   fglock-probe button NAME   the screen rectangle of NAME's taskbar button: "x y w h" (NAME: a probe
 *                              window, else any top-level window titled NAME)
 *   fglock-probe pid NAME      its process id
 *   fglock-probe xwin NAME     its X window (winex11's __wine_x11_whole_window)
 *   fglock-probe find TEXT     "1" when a visible top-level window's title has TEXT, and "fg=1" when it is in front
 *   fglock-probe zorder        the visible top-level windows with a title, front first
 *   fglock-probe startpanel    a stand-in for sg-start: a hidden SgStartPanel window; explorer's WM_USER+10
 *                              (the Windows key, the Start button) shows the window FgStart and calls
 *                              SetForegroundWindow on it, as sg-start does
 */
#include <windows.h>
#include <stdio.h>
#include <string.h>

#define WM_FG_CMD (WM_APP + 1)
enum { C_SETFG = 1, C_ALLOW, C_LOCK, C_UNLOCK, C_SPAWN, C_TIMEOUT, C_MENU, C_ENDMENU };

static HWND main_hwnd, edit_hwnd;
static char spawn_name[64];

static HWND find( const char *name )
{
    return FindWindowA( "FgLockProbe", name );
}

static void result( HWND hwnd, LONG_PTR value )
{
    SetPropA( hwnd, "fgres", (HANDLE)(value + 1) );
}

static LRESULT CALLBACK proc( HWND hwnd, UINT msg, WPARAM wp, LPARAM lp )
{
    switch (msg)
    {
    case WM_CREATE:
        edit_hwnd = CreateWindowA( "EDIT", "", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
                                   10, 10, 300, 24, hwnd, (HMENU)1, NULL, NULL );
        return 0;
    case WM_SETFOCUS:
        SetFocus( edit_hwnd );
        return 0;
    case WM_FG_CMD:
        switch (wp)
        {
        case C_SETFG:
            SetLastError( 0 );
            result( hwnd, SetForegroundWindow( hwnd ) );
            break;
        case C_ALLOW:
            result( hwnd, AllowSetForegroundWindow( (DWORD)lp ) );
            break;
        case C_LOCK:
            result( hwnd, LockSetForegroundWindow( LSFW_LOCK ) );
            break;
        case C_UNLOCK:
            result( hwnd, LockSetForegroundWindow( LSFW_UNLOCK ) );
            break;
        case C_SPAWN:
        {
            char cmd[MAX_PATH + 80], self[MAX_PATH];
            STARTUPINFOA si = { sizeof(si) };
            PROCESS_INFORMATION pi;
            GetModuleFileNameA( NULL, self, sizeof(self) );
            if (!strncmp( spawn_name, "bridge:", 7 ))
                snprintf( cmd, sizeof(cmd), "\"%s\" bridge %s 640 440", self, spawn_name + 7 );
            else if (!strncmp( spawn_name, "stub:", 5 ))
                snprintf( cmd, sizeof(cmd), "\"%s\" launch %s 640 440 2000", self, spawn_name + 5 );
            else snprintf( cmd, sizeof(cmd), "\"%s\" win %s", self, spawn_name );
            if (CreateProcessA( NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi ))
            {
                CloseHandle( pi.hThread );
                CloseHandle( pi.hProcess );
                result( hwnd, 1 );
            }
            else result( hwnd, 0 );
            break;
        }
        case C_TIMEOUT:
            result( hwnd, SystemParametersInfoA( SPI_SETFOREGROUNDLOCKTIMEOUT, 0, (void *)lp,
                                                 SPIF_UPDATEINIFILE | SPIF_SENDCHANGE ) );
            break;
        case C_MENU:
        {
            HMENU menu = CreatePopupMenu();
            RECT r;
            AppendMenuA( menu, MF_STRING, 1, "One" );
            AppendMenuA( menu, MF_STRING, 2, "Two" );
            GetWindowRect( hwnd, &r );
            result( hwnd, 1 );
            TrackPopupMenu( menu, TPM_RETURNCMD, r.left + 40, r.top + 60, 0, hwnd, NULL );
            DestroyMenu( menu );
            break;
        }
        case C_ENDMENU:
            EndMenu();
            result( hwnd, 1 );
            break;
        }
        return 0;
    case WM_COPYDATA:
    {
        const COPYDATASTRUCT *cds = (const COPYDATASTRUCT *)lp;
        if (cds->cbData < sizeof(spawn_name))
        {
            memcpy( spawn_name, cds->lpData, cds->cbData );
            spawn_name[cds->cbData] = 0;
        }
        return TRUE;
    }
    case WM_DESTROY:
        PostQuitMessage( 0 );
        return 0;
    }
    return DefWindowProcA( hwnd, msg, wp, lp );
}

static LRESULT CALLBACK start_proc( HWND hwnd, UINT msg, WPARAM wp, LPARAM lp )
{
    if (msg == WM_USER + 10 && main_hwnd)
    {
        if (IsWindowVisible( main_hwnd ) && GetForegroundWindow() == main_hwnd) ShowWindow( main_hwnd, SW_HIDE );
        else
        {
            ShowWindow( main_hwnd, SW_SHOWNORMAL );
            result( main_hwnd, SetForegroundWindow( main_hwnd ) );
        }
        return 0;
    }
    return DefWindowProcA( hwnd, msg, wp, lp );
}

static int run_start_panel(void)
{
    WNDCLASSA wc = { 0 };
    MSG msg;

    wc.lpfnWndProc = proc;
    wc.hInstance = GetModuleHandleA( NULL );
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = "FgLockProbe";
    RegisterClassA( &wc );
    wc.lpfnWndProc = start_proc;
    wc.lpszClassName = "SgStartPanel";
    RegisterClassA( &wc );
    main_hwnd = CreateWindowExA( WS_EX_TOOLWINDOW, "FgLockProbe", "FgStart", WS_POPUP | WS_BORDER, 0, 380, 360, 380,
                                 NULL, NULL, wc.hInstance, NULL );
    CreateWindowA( "SgStartPanel", "", WS_POPUP, 0, 0, 0, 0, NULL, NULL, wc.hInstance, NULL );
    while (GetMessageA( &msg, NULL, 0, 0 ) > 0)
    {
        TranslateMessage( &msg );
        DispatchMessageA( &msg );
    }
    return 0;
}

struct find_data { const char *text; HWND found; };

static BOOL CALLBACK find_proc( HWND hwnd, LPARAM lp )
{
    struct find_data *data = (struct find_data *)lp;
    char t[256];
    if (!IsWindowVisible( hwnd ) || !GetWindowTextA( hwnd, t, sizeof(t) ) || !strstr( t, data->text )) return TRUE;
    data->found = hwnd;
    return FALSE;
}

static int run_window( const char *name, int x, int y )
{
    WNDCLASSA wc = { 0 };
    MSG msg;

    wc.lpfnWndProc = proc;
    wc.hInstance = GetModuleHandleA( NULL );
    wc.hCursor = LoadCursorA( NULL, (LPCSTR)IDC_ARROW );
    /* FgB is black inside: the gate tells from a pixel which window X shows where they overlap */
    wc.hbrBackground = !strcmp( name, "FgB" ) ? GetStockObject( BLACK_BRUSH ) : (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = "FgLockProbe";
    RegisterClassA( &wc );
    main_hwnd = CreateWindowA( "FgLockProbe", name, WS_OVERLAPPEDWINDOW, x, y, 420, 260,
                               NULL, NULL, wc.hInstance, NULL );
    SetFocus( main_hwnd );   /* before it is shown, as Firefox and many dialogs do */
    ShowWindow( main_hwnd, SW_SHOWNORMAL );
    UpdateWindow( main_hwnd );
    while (GetMessageA( &msg, NULL, 0, 0 ) > 0)
    {
        TranslateMessage( &msg );
        DispatchMessageA( &msg );
    }
    return 0;
}

static void title_of( HWND hwnd, char *buf, int len )
{
    buf[0] = 0;
    if (hwnd) GetWindowTextA( hwnd, buf, len );
    if (!buf[0]) snprintf( buf, len, "%p", hwnd );
}

/* whether a is in front of b (z-order of top-level windows) */
static BOOL in_front( HWND a, HWND b )
{
    HWND w;
    for (w = GetWindow( GetDesktopWindow(), GW_CHILD ); w; w = GetWindow( w, GW_HWNDNEXT ))
    {
        if (w == a) return TRUE;
        if (w == b) return FALSE;
    }
    return FALSE;
}

int main( int argc, char **argv )
{
    if (argc >= 3 && !strcmp( argv[1], "win" ))
    {
        if (argc >= 6) Sleep( atoi( argv[5] ) );   /* its window only after DELAY ms */
        return run_window( argv[2], argc >= 4 ? atoi( argv[3] ) : 100, argc >= 5 ? atoi( argv[4] ) : 80 );
    }

    if (argc >= 5 && !strcmp( argv[1], "bridge" ))
    {
        /* as Setup does: a Unix program (sh, here) starts the real window's program again, and this one
         * is gone at once */
        char cmd[1024];
        STARTUPINFOA si = { sizeof(si) };
        PROCESS_INFORMATION pi;
        snprintf( cmd, sizeof(cmd), "\\\\?\\unix\\bin\\sh -c \"sleep 2; exec \\\"$WINE\\\" "
                  "'C:\\fglock-probe.exe' win %s %s %s\"", argv[2], argv[3], argv[4] );
        if (!CreateProcessA( NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi )) return 1;
        CloseHandle( pi.hThread );
        CloseHandle( pi.hProcess );
        return 0;
    }

    if (argc >= 6 && !strcmp( argv[1], "launch" ))
    {
        /* starts "win NAME X Y DELAY" and is gone at once: the window's program outlives its parent */
        char cmd[MAX_PATH + 120], self[MAX_PATH];
        STARTUPINFOA si = { sizeof(si) };
        PROCESS_INFORMATION pi;
        GetModuleFileNameA( NULL, self, sizeof(self) );
        snprintf( cmd, sizeof(cmd), "\"%s\" win %s %s %s %s", self, argv[2], argv[3], argv[4], argv[5] );
        if (!CreateProcessA( NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi )) return 1;
        CloseHandle( pi.hThread );
        CloseHandle( pi.hProcess );
        return 0;
    }

    if (argc >= 4 && !strcmp( argv[1], "cmd" ))
    {
        HWND hwnd = find( argv[2] );
        const char *c = argv[3];
        WPARAM cmd = 0;
        LPARAM arg = 0;

        if (!hwnd) { printf( "no window %s\n", argv[2] ); return 1; }
        RemovePropA( hwnd, "fgres" );   /* a property of another process's window: ours to clear */
        if (!strcmp( c, "setfg" )) cmd = C_SETFG;
        else if (!strcmp( c, "allow" ) && argc >= 5)
        {
            cmd = C_ALLOW;
            if (!strcmp( argv[4], "any" )) arg = (LPARAM)ASFW_ANY;
            else if (find( argv[4] ))
            {
                DWORD pid;
                GetWindowThreadProcessId( find( argv[4] ), &pid );
                arg = pid;
            }
            else arg = atoi( argv[4] );
        }
        else if (!strcmp( c, "lock" )) cmd = C_LOCK;
        else if (!strcmp( c, "unlock" )) cmd = C_UNLOCK;
        else if (!strcmp( c, "spawn" ) && argc >= 5)
        {
            COPYDATASTRUCT cds = { 0, (DWORD)strlen( argv[4] ), argv[4] };
            SendMessageA( hwnd, WM_COPYDATA, 0, (LPARAM)&cds );
            cmd = C_SPAWN;
        }
        else if (!strcmp( c, "timeout" ) && argc >= 5) { cmd = C_TIMEOUT; arg = atoi( argv[4] ); }
        else if (!strcmp( c, "menu" )) cmd = C_MENU;
        else if (!strcmp( c, "endmenu" )) cmd = C_ENDMENU;
        else { printf( "bad command %s\n", c ); return 1; }
        PostMessageA( hwnd, WM_FG_CMD, cmd, arg );
        return 0;
    }

    if (argc >= 3 && !strcmp( argv[1], "result" ))
    {
        HWND hwnd = find( argv[2] );
        LONG_PTR v = hwnd ? (LONG_PTR)GetPropA( hwnd, "fgres" ) : 0;
        if (!v) printf( "none\n" ); else printf( "%ld\n", (long)(v - 1) );
        return 0;
    }

    if (argc >= 2 && !strcmp( argv[1], "query" ))
    {
        char t[256];
        HWND fg = GetForegroundWindow();
        int i, j;

        title_of( fg, t, sizeof(t) );
        printf( "fg=%s", t );
        for (i = 2; i < argc; i++)
        {
            HWND hwnd = find( argv[i] ), edit;
            GUITHREADINFO gui = { sizeof(gui) };
            if (!hwnd) { printf( " | %s gone", argv[i] ); continue; }
            edit = GetDlgItem( hwnd, 1 );
            t[0] = 0;
            if (edit) SendMessageA( edit, WM_GETTEXT, sizeof(t), (LPARAM)t );
            GetGUIThreadInfo( GetWindowThreadProcessId( hwnd, NULL ), &gui );
            printf( " | %s text=%s active=%d over=", argv[i], t, gui.hwndActive == hwnd );
            for (j = 2; j < argc; j++)
                if (j != i && find( argv[j] ) && in_front( hwnd, find( argv[j] ) )) printf( "%s,", argv[j] );
        }
        printf( "\n" );
        return 0;
    }

    if (argc >= 3 && !strcmp( argv[1], "button" ))
    {
        HWND hwnd = find( argv[2] ), tray = FindWindowA( "Shell_TrayWnd", NULL ), b;
        if (!hwnd) hwnd = FindWindowA( NULL, argv[2] );   /* any window by its title (other gates) */
        RECT r;
        if (!hwnd || !tray) { printf( "none\n" ); return 1; }
        for (b = GetWindow( tray, GW_CHILD ); b; b = GetWindow( b, GW_HWNDNEXT ))
        {
            if ((HWND)(ULONG_PTR)GetDlgCtrlID( b ) != hwnd || !IsWindowVisible( b )) continue;
            GetWindowRect( b, &r );
            printf( "%ld %ld %ld %ld\n", r.left, r.top, r.right - r.left, r.bottom - r.top );
            return 0;
        }
        printf( "none\n" );
        return 1;
    }

    if (argc >= 2 && !strcmp( argv[1], "startpanel" )) return run_start_panel();

    if (argc >= 2 && !strcmp( argv[1], "gui" ))
    {
        GUITHREADINFO gui = { sizeof(gui) };
        HWND fg = GetForegroundWindow();
        GetGUIThreadInfo( GetWindowThreadProcessId( fg, NULL ), &gui );
        printf( "fg=%p flags=%lx menuowner=%p capture=%p\n", fg, gui.flags, gui.hwndMenuOwner, gui.hwndCapture );
        return 0;
    }

    if (argc >= 2 && !strcmp( argv[1], "zorder" ))
    {
        HWND w;
        char t[256];
        for (w = GetWindow( GetDesktopWindow(), GW_CHILD ); w; w = GetWindow( w, GW_HWNDNEXT ))
            if (IsWindowVisible( w ) && GetWindowTextA( w, t, sizeof(t) )) printf( "%p %s%s\n", w, t,
                    (GetWindowLongA( w, GWL_EXSTYLE ) & WS_EX_TOPMOST) ? " (topmost)" : "" );
        return 0;
    }

    if (argc >= 3 && !strcmp( argv[1], "find" ))
    {
        struct find_data data = { argv[2], NULL };
        EnumWindows( find_proc, (LPARAM)&data );
        printf( "%d fg=%d\n", data.found != NULL, data.found && GetAncestor( GetForegroundWindow(), GA_ROOTOWNER ) == data.found );
        return 0;
    }

    if (argc >= 3 && !strcmp( argv[1], "xwin" ))
    {
        HWND hwnd = find( argv[2] );
        printf( "%lu\n", hwnd ? (unsigned long)(ULONG_PTR)GetPropA( hwnd, "__wine_x11_whole_window" ) : 0ul );
        return 0;
    }

    if (argc >= 3 && !strcmp( argv[1], "pid" ))
    {
        DWORD pid = 0;
        HWND hwnd = find( argv[2] );
        if (hwnd) GetWindowThreadProcessId( hwnd, &pid );
        printf( "%lu\n", pid );
        return 0;
    }
    printf( "usage: see the source\n" );
    return 1;
}
