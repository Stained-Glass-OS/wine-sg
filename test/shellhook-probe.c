/* RegisterShellHookWindow / DeregisterShellHookWindow and the SHELLHOOK
 * messages (patches/sg/2207).  Both functions were stubs that returned FALSE,
 * so a program that wants to follow the top-level windows (task switchers,
 * AutoHotkey scripts, launchers) never heard of one.
 *
 * A registered window is posted RegisterWindowMessage("SHELLHOOK") with the
 * HSHELL_* code in wParam and the window in lParam.  The events come from the
 * processes that own the windows, so most checks run in a SECOND process too.
 *
 *   probe.exe [OTHER.exe ...]   run the checks, then the cross-process ones
 *                               with each OTHER.exe (and this one) as a child
 *   probe.exe --child HOOKWND   create a window, retitle, activate, destroy it
 *   probe.exe --register HOOKWND  register a hook window, tell HOOKWND, wait */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MSG_CHILD_WINDOW (WM_APP + 1)
#define MSG_REGISTERED   (WM_APP + 2)
#define SLOTS 32

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

static UINT shell_msg;
struct entry { WPARAM w; LPARAM l; UINT msg; };
static struct entry log_[256];
static int log_n;

static LRESULT CALLBACK hook_proc( HWND hwnd, UINT msg, WPARAM wp, LPARAM lp )
{
    if ((msg == shell_msg || msg == MSG_CHILD_WINDOW || msg == MSG_REGISTERED) && log_n < 256)
    {
        log_[log_n].w = wp;
        log_[log_n].l = lp;
        log_[log_n].msg = msg;
        log_n++;
        return 0;
    }
    return DefWindowProcW( hwnd, msg, wp, lp );
}

static LRESULT CALLBACK plain_proc( HWND hwnd, UINT msg, WPARAM wp, LPARAM lp )
{
    return DefWindowProcW( hwnd, msg, wp, lp );
}

static void pump( DWORD ms )
{
    DWORD end = GetTickCount() + ms;
    MSG msg;
    do
    {
        while (PeekMessageW( &msg, NULL, 0, 0, PM_REMOVE )) { TranslateMessage( &msg ); DispatchMessageW( &msg ); }
        Sleep( 10 );
    } while ((LONG)(end - GetTickCount()) > 0);
}

static void clear_log( void ) { pump( 50 ); log_n = 0; }

/* index of a SHELLHOOK entry with this code and window, or -1 */
static int find( UINT code, HWND hwnd )
{
    int i;
    for (i = 0; i < log_n; i++)
        if (log_[i].msg == shell_msg && log_[i].w == code && log_[i].l == (LPARAM)hwnd) return i;
    return -1;
}

static int count_shell( void )
{
    int i, n = 0;
    for (i = 0; i < log_n; i++) if (log_[i].msg == shell_msg) n++;
    return n;
}

static HWND make_window( const WCHAR *cls, DWORD style, HWND owner, int x, int y, int w, int h )
{
    return CreateWindowExW( 0, cls, L"shell", style, x, y, w, h, owner, NULL, GetModuleHandleW( NULL ), NULL );
}

static HWND slot_value( int i )
{
    WCHAR name[32];
    swprintf( name, ARRAYSIZE(name), L"SgShellHook%02x", i );
    return GetPropW( GetDesktopWindow(), name );
}

static int stale_slots( void )
{
    int i, n = 0;
    for (i = 0; i < SLOTS; i++)
    {
        HWND h = slot_value( i );
        if (h && !IsWindow( h )) n++;
    }
    return n;
}

static int live_slots( void )
{
    int i, n = 0;
    for (i = 0; i < SLOTS; i++)
    {
        HWND h = slot_value( i );
        if (h && IsWindow( h )) n++;
    }
    return n;
}

static void register_classes( void )
{
    WNDCLASSW wc = {0};
    wc.hInstance = GetModuleHandleW( NULL );
    wc.lpfnWndProc = hook_proc;
    wc.lpszClassName = L"SgShellHookWnd";
    RegisterClassW( &wc );
    wc.lpfnWndProc = plain_proc;
    wc.lpszClassName = L"SgShellPlain";
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    RegisterClassW( &wc );
}

static int child_window( HWND hook )
{
    HWND w;

    register_classes();
    w = make_window( L"SgShellPlain", WS_OVERLAPPEDWINDOW, NULL, 10, 10, 200, 150 );
    PostMessageW( hook, MSG_CHILD_WINDOW, (WPARAM)w, 0 );
    SetWindowTextW( w, L"renamed" );
    ShowWindow( w, SW_SHOWNOACTIVATE );
    SetActiveWindow( w );
    pump( 100 );
    DestroyWindow( w );
    pump( 100 );
    return 0;
}

static int child_register( HWND parent_hook )
{
    HWND h;

    register_classes();
    h = make_window( L"SgShellHookWnd", WS_OVERLAPPED, NULL, 0, 0, 10, 10 );
    if (!RegisterShellHookWindow( h )) return 2;
    PostMessageW( parent_hook, MSG_REGISTERED, (WPARAM)h, 0 );
    { int n; for (n = 0; n < 120; n++) pump( 1000 ); }
    return 0;
}

static DWORD run_child( const char *exe, const char *mode, HWND hook, BOOL wait )
{
    char cmd[MAX_PATH + 64];
    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    DWORD code = 99;

    snprintf( cmd, sizeof(cmd), "\"%s\" %s %lx", exe, mode, (unsigned long)(ULONG_PTR)hook );
    printf( "-- second process: %s %s\n", exe, mode );
    if (!CreateProcessA( NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi ))
    {
        check( 0, "the second process started" );
        return code;
    }
    if (!wait) { CloseHandle( pi.hThread ); return (DWORD)(ULONG_PTR)pi.hProcess; }
    WaitForSingleObject( pi.hProcess, 60000 );
    GetExitCodeProcess( pi.hProcess, &code );
    CloseHandle( pi.hProcess );
    CloseHandle( pi.hThread );
    return code;
}

static int hook_codes[16], hook_wins[16], hook_n;
static LRESULT CALLBACK wh_shell( int code, WPARAM wp, LPARAM lp )
{
    if (code >= 0 && hook_n < 16) { hook_codes[hook_n] = code; hook_wins[hook_n] = (int)wp; hook_n++; }
    return CallNextHookEx( NULL, code, wp, lp );
}

int main( int argc, char **argv )
{
    HWND hook, w1, w2, w3, w4, ch, tmp;
    HHOOK hh;
    FLASHWINFO fi;
    int i, n, ok;
    SIZE_T x;

    if (argc >= 3 && !strcmp( argv[1], "--child" ))
        return child_window( (HWND)(ULONG_PTR)strtoul( argv[2], NULL, 16 ));
    if (argc >= 3 && !strcmp( argv[1], "--register" ))
        return child_register( (HWND)(ULONG_PTR)strtoul( argv[2], NULL, 16 ));

    register_classes();
    shell_msg = RegisterWindowMessageW( L"SHELLHOOK" );
    check( shell_msg >= 0xc000, "RegisterWindowMessage(SHELLHOOK) gives a registered message id" );

    SetLastError( 0 );
    check( !RegisterShellHookWindow( (HWND)0x7ff1 ) && GetLastError() == ERROR_INVALID_WINDOW_HANDLE,
           "registering a window that does not exist is ERROR_INVALID_WINDOW_HANDLE" );
    check( !DeregisterShellHookWindow( (HWND)0x7ff1 ), "deregistering one that was never registered fails" );

    hook = make_window( L"SgShellHookWnd", WS_OVERLAPPED, NULL, 0, 0, 10, 10 );
    w1 = make_window( L"SgShellPlain", WS_OVERLAPPEDWINDOW, NULL, 10, 10, 200, 150 );
    clear_log();
    w4 = make_window( L"SgShellPlain", WS_OVERLAPPEDWINDOW, NULL, 10, 10, 200, 150 );
    pump( 100 );
    check( count_shell() == 0, "no SHELLHOOK message before anything is registered" );
    DestroyWindow( w4 );

    check( RegisterShellHookWindow( hook ), "RegisterShellHookWindow succeeds" );
    check( RegisterShellHookWindow( hook ), "registering the same window twice succeeds" );
    check( live_slots() == 1, "...and holds one slot, not two" );

    /* creation */
    clear_log();
    w2 = make_window( L"SgShellPlain", WS_OVERLAPPEDWINDOW, NULL, 20, 20, 200, 150 );
    pump( 100 );
    check( find( HSHELL_WINDOWCREATED, w2 ) >= 0, "a top-level window being created is HSHELL_WINDOWCREATED" );
    clear_log();
    w3 = make_window( L"SgShellPlain", WS_POPUP, w2, 30, 30, 100, 100 );
    ch = make_window( L"SgShellPlain", WS_CHILD, w2, 0, 0, 50, 50 );
    pump( 100 );
    check( count_shell() == 0, "an owned window and a child window are not announced" );

    /* title */
    clear_log();
    SetWindowTextW( w2, L"a new title" );
    SetWindowTextW( w3, L"owned title" );
    pump( 100 );
    check( find( HSHELL_REDRAW, w2 ) >= 0, "a new title for a top-level window is HSHELL_REDRAW" );
    check( find( HSHELL_REDRAW, w3 ) < 0, "...but not for an owned window" );

    /* activation */
    ShowWindow( w2, SW_SHOWNOACTIVATE );
    ShowWindow( w1, SW_SHOWNOACTIVATE );
    clear_log();
    SetActiveWindow( w2 );
    pump( 100 );
    check( find( HSHELL_WINDOWACTIVATED, w2 ) >= 0, "activating a window is HSHELL_WINDOWACTIVATED" );
    clear_log();
    ShowWindow( w3, SW_SHOWNOACTIVATE );
    SetActiveWindow( w3 );
    pump( 100 );
    check( find( HSHELL_WINDOWACTIVATED, w2 ) >= 0 && find( HSHELL_WINDOWACTIVATED, w3 ) < 0,
           "activating an owned window reports the window it belongs to" );
    clear_log();
    SetActiveWindow( w1 );
    pump( 100 );
    check( find( HSHELL_WINDOWACTIVATED, w1 ) >= 0, "moving the activation to another window reports that one" );
    tmp = make_window( L"SgShellPlain", WS_POPUP, NULL, 0, 0, GetSystemMetrics( SM_CXSCREEN ),
                       GetSystemMetrics( SM_CYSCREEN ) );
    ShowWindow( tmp, SW_SHOWNOACTIVATE );
    clear_log();
    SetActiveWindow( tmp );
    pump( 100 );
    check( find( HSHELL_RUDEAPPACTIVATED, tmp ) >= 0 && find( HSHELL_WINDOWACTIVATED, tmp ) < 0,
           "a window that fills the screen is HSHELL_RUDEAPPACTIVATED" );
    DestroyWindow( tmp );

    /* flashing */
    SetForegroundWindow( w1 );
    pump( 100 );
    memset( &fi, 0, sizeof(fi) );
    fi.cbSize = sizeof(fi);
    fi.hwnd = w2;
    fi.dwFlags = FLASHW_ALL;
    fi.uCount = 2;
    clear_log();
    FlashWindowEx( &fi );
    pump( 100 );
    check( GetForegroundWindow() != w2 || find( 0x8006, w2 ) < 0, "(precondition: w2 is not the foreground window)" );
    check( find( HSHELL_FLASH, w2 ) >= 0 || GetForegroundWindow() == w2,
           "flashing a window that is not in front is HSHELL_FLASH" );
    fi.dwFlags = FLASHW_STOP;
    clear_log();
    FlashWindowEx( &fi );
    pump( 100 );
    check( find( HSHELL_REDRAW, w2 ) >= 0, "stopping the flashing redraws the button (HSHELL_REDRAW)" );

    /* app command */
    clear_log();
    SendMessageW( w2, WM_APPCOMMAND, (WPARAM)w2, MAKELPARAM( 0, APPCOMMAND_VOLUME_MUTE ) | 0x8000 );
    pump( 100 );
    ok = 0;
    for (i = 0; i < log_n; i++)
        if (log_[i].msg == shell_msg && log_[i].w == HSHELL_APPCOMMAND &&
            (LPARAM)log_[i].l == (LPARAM)(MAKELPARAM( 0, APPCOMMAND_VOLUME_MUTE ) | 0x8000)) ok = 1;
    check( ok, "an application command nobody handled is HSHELL_APPCOMMAND with its command" );

    /* a WH_SHELL hook sees the same events */
    hook_n = 0;
    hh = SetWindowsHookExW( WH_SHELL, wh_shell, NULL, GetCurrentThreadId() );
    check( hh != NULL, "a WH_SHELL hook installs" );
    tmp = make_window( L"SgShellPlain", WS_OVERLAPPEDWINDOW, NULL, 40, 40, 100, 100 );
    SetWindowTextW( tmp, L"hooked" );
    ShowWindow( tmp, SW_SHOWNOACTIVATE );
    SetActiveWindow( tmp );
    pump( 100 );
    for (i = ok = 0, n = 0; i < hook_n; i++)
    {
        if (hook_codes[i] == HSHELL_WINDOWCREATED && hook_wins[i] == (int)(ULONG_PTR)tmp) ok |= 1;
        if (hook_codes[i] == HSHELL_REDRAW && hook_wins[i] == (int)(ULONG_PTR)tmp) ok |= 2;
        if (hook_codes[i] == HSHELL_WINDOWACTIVATED && hook_wins[i] == (int)(ULONG_PTR)tmp) ok |= 4;
    }
    check( ok == 7, "the WH_SHELL hook saw created, redraw and activated as well" );
    UnhookWindowsHookEx( hh );
    clear_log();
    DestroyWindow( tmp );
    pump( 100 );
    check( find( HSHELL_WINDOWDESTROYED, tmp ) >= 0, "destroying a top-level window is HSHELL_WINDOWDESTROYED" );

    /* destroy: the owned window goes quietly, the owner is announced */
    clear_log();
    DestroyWindow( w2 );
    pump( 100 );
    check( find( HSHELL_WINDOWDESTROYED, w2 ) >= 0, "the owner is announced when it goes" );
    check( find( HSHELL_WINDOWDESTROYED, w3 ) < 0, "...its owned window, destroyed with it, is not" );
    (void)ch;

    /* second process: its windows are announced to this one */
    for (x = 1; x < (SIZE_T)argc + 1; x++)
    {
        const char *exe = x < (SIZE_T)argc ? argv[x] : argv[0];
        HWND cw = NULL;
        int a, b, c, d;

        clear_log();
        check( run_child( exe, "--child", hook, TRUE ) == 0, "the second process ran its window through" );
        pump( 200 );
        for (i = 0; i < log_n; i++) if (log_[i].msg == MSG_CHILD_WINDOW) cw = (HWND)log_[i].w;
        a = find( HSHELL_WINDOWCREATED, cw );
        b = find( HSHELL_REDRAW, cw );
        c = find( HSHELL_WINDOWACTIVATED, cw );
        d = find( HSHELL_WINDOWDESTROYED, cw );
        check( cw && a >= 0, "[other process] its window being created reaches the hook window" );
        check( b > a && c > a, "[other process] ...its new title and its activation too" );
        check( d > c, "[other process] ...and its destruction, last" );
    }

    /* a registered window of a process that is killed leaves a dead slot,
     * which the next event clears */
    {
        HANDLE proc;
        int before, dead_before;

        clear_log();
        before = live_slots();
        proc = (HANDLE)(ULONG_PTR)run_child( argc > 1 ? argv[1] : argv[0], "--register", hook, FALSE );
        for (i = 0; i < 50 && !(log_n && log_[log_n - 1].msg == MSG_REGISTERED); i++) pump( 100 );
        check( live_slots() == before + 1, "[other process] a window it registered holds a slot" );
        TerminateProcess( proc, 0 );
        WaitForSingleObject( proc, 10000 );
        CloseHandle( proc );
        pump( 300 );
        dead_before = stale_slots();
        check( dead_before == 1, "[other process] killed, it leaves a slot whose window is gone" );
        tmp = make_window( L"SgShellPlain", WS_OVERLAPPEDWINDOW, NULL, 40, 40, 100, 100 );
        pump( 100 );
        check( stale_slots() == 0, "...which the next event clears" );
        DestroyWindow( tmp );
    }

    /* slots come back: a registered window that is destroyed gives its slot up */
    for (i = ok = 0; i < 3 * SLOTS; i++)
    {
        tmp = make_window( L"SgShellHookWnd", WS_OVERLAPPED, NULL, 0, 0, 10, 10 );
        if (!RegisterShellHookWindow( tmp )) break;
        DestroyWindow( tmp );
        ok++;
    }
    check( ok == 3 * SLOTS, "registering and destroying windows over and over never runs out of slots" );
    check( stale_slots() == 0 && live_slots() == 1, "...and leaves nothing behind" );

    /* a registration takes up to SLOTS windows, then it is an error */
    {
        HWND many[SLOTS + 2];
        int got = 0;

        for (i = 0; i < SLOTS + 2; i++)
        {
            many[i] = make_window( L"SgShellHookWnd", WS_OVERLAPPED, NULL, 0, 0, 10, 10 );
            if (RegisterShellHookWindow( many[i] )) got++;
        }
        check( got == SLOTS - 1, "the shell hook table holds a bounded number of windows" );
        for (i = 0; i < SLOTS + 2; i++) DestroyWindow( many[i] );
    }

    /* deregistering */
    check( DeregisterShellHookWindow( hook ), "DeregisterShellHookWindow succeeds" );
    check( !DeregisterShellHookWindow( hook ), "...and fails the second time" );
    clear_log();
    w4 = make_window( L"SgShellPlain", WS_OVERLAPPEDWINDOW, NULL, 10, 10, 200, 150 );
    pump( 100 );
    check( count_shell() == 0, "a deregistered window is posted nothing" );
    DestroyWindow( w4 );
    DestroyWindow( w1 );
    DestroyWindow( hook );

    printf( "RESULT: %s\n", failures ? "FAIL" : "PASS" );
    return failures ? 1 : 0;
}
