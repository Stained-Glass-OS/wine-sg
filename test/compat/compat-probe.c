/* compat-probe: start an application and find its main window, for
 * test/compat/run.sh.
 *
 *   compat-probe launch SECONDS EXE [ARGS]   start it; wait for a new visible
 *                                           top-level window (any process: some
 *                                           applications are launchers); with
 *                                           SG_ACCEPT_DIALOG=1 a dialog that comes
 *                                           first (a game's settings) is accepted
 *                                           with its default button
 *                                           window=0x.. class=.. title=.. WxH
 *                                           or window=none alive=0|1
 *   compat-probe close HWND                 WM_CLOSE, then whether it went
 *                                           (closed=) and its process ended
 *                                           within 30 s (exited=)
 *   compat-probe alive EXE-NAME             whether a process of that name runs
 *   compat-probe list                       every top-level window: pid, class,
 *                                           visible, iconic, rect, title (triage)
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include <windows.h>
#include <tlhelp32.h>
#include <stdio.h>

static HWND before[4096];
static int nbefore;

static BOOL CALLBACK snapshot( HWND hwnd, LPARAM lp )
{
    if (nbefore < ARRAYSIZE(before)) before[nbefore++] = hwnd;
    return TRUE;
}

struct found { HWND hwnd; };

static BOOL CALLBACK find_new( HWND hwnd, LPARAM lp )
{
    struct found *f = (struct found *)lp;
    RECT old;
    WCHAR cls[64];
    RECT rc;
    int i;

    if (!IsWindowVisible( hwnd ) || GetWindow( hwnd, GW_OWNER )) return TRUE;
    for (i = 0; i < nbefore; i++) if (before[i] == hwnd) return TRUE;
    GetClassNameW( hwnd, cls, 64 );
    if (!lstrcmpW( cls, L"tooltips_class32" ) || !lstrcmpW( cls, L"#32768" )) return TRUE;
    GetWindowRect( hwnd, &rc );
    if (rc.right - rc.left < 120 || rc.bottom - rc.top < 80) return TRUE;  /* splash dots, tray helpers */
    /* the main window: one with a caption (a splash screen has none), and of
     * those the largest -- not a dialog in front */
    if (f->hwnd && GetWindowRect( f->hwnd, &old ) && IsWindowVisible( f->hwnd ))
    {
        BOOL cap_old = (GetWindowLongW( f->hwnd, GWL_STYLE ) & WS_CAPTION) == WS_CAPTION;
        BOOL cap_new = (GetWindowLongW( hwnd, GWL_STYLE ) & WS_CAPTION) == WS_CAPTION;
        if (cap_old && !cap_new) return TRUE;
        if (cap_old == cap_new &&
            (old.right - old.left) * (old.bottom - old.top) >= (rc.right - rc.left) * (rc.bottom - rc.top))
            return TRUE;
    }
    f->hwnd = hwnd;
    return TRUE;
}

static DWORD close_pid;

/* the process's other visible, enabled top-level windows (its dialogs) */
static DWORD shell_pid;

/* what is in front of the main window: its own dialogs, and the ones its
 * helpers put up (Notepad++'s updater is another process) -- anything
 * visible that is not the shell's */
static BOOL CALLBACK close_front( HWND hwnd, LPARAM main )
{
    DWORD pid;
    if (hwnd == (HWND)main || !IsWindowVisible( hwnd ) || !IsWindowEnabled( hwnd )) return TRUE;
    GetWindowThreadProcessId( hwnd, &pid );
    if (pid == shell_pid || pid == GetCurrentProcessId()) return TRUE;
    if (pid == close_pid || (GetWindowLongW( hwnd, GWL_STYLE ) & WS_CAPTION) == WS_CAPTION)
        PostMessageW( hwnd, WM_CLOSE, 0, 0 );
    return TRUE;
}

static BOOL CALLBACK list_proc( HWND hwnd, LPARAM lp )
{
    WCHAR cls[64] = L"", title[128] = L"";
    DWORD pid;
    RECT rc;
    GetWindowThreadProcessId( hwnd, &pid );
    GetClassNameW( hwnd, cls, 64 );
    GetWindowTextW( hwnd, title, 128 );
    GetWindowRect( hwnd, &rc );
    printf( "%#lx pid=%04lx vis=%d icon=%d owner=%p style=%08lx rect=%ld,%ld,%ld,%ld class=%ls title=%ls\n",
            (ULONG)(ULONG_PTR)hwnd, pid, IsWindowVisible( hwnd ), IsIconic( hwnd ), GetWindow( hwnd, GW_OWNER ),
            GetWindowLongW( hwnd, GWL_STYLE ), rc.left, rc.top, rc.right, rc.bottom, cls, title );
    return TRUE;
}

static BOOL process_running( const WCHAR *name )
{
    PROCESSENTRY32W pe = { sizeof(pe) };
    HANDLE snap = CreateToolhelp32Snapshot( TH32CS_SNAPPROCESS, 0 );
    BOOL ret = FALSE;
    if (Process32FirstW( snap, &pe ))
        do if (!lstrcmpiW( pe.szExeFile, name )) ret = TRUE; while (!ret && Process32NextW( snap, &pe ));
    CloseHandle( snap );
    return ret;
}

int wmain( int argc, WCHAR **argv )
{
    if (argc >= 4 && !lstrcmpW( argv[1], L"launch" ))
    {
        STARTUPINFOW si = { sizeof(si) };
        PROCESS_INFORMATION pi;
        WCHAR cmd[4096];
        int secs = _wtoi( argv[2] ), i;
        struct found f = { 0 };
        DWORD code = STILL_ACTIVE;

        WCHAR exe[MAX_PATH], flag[4] = L"";
        BOOL accept = GetEnvironmentVariableW( L"SG_ACCEPT_DIALOG", flag, ARRAYSIZE(flag) ) && flag[0] == '1';

        EnumWindows( snapshot, 0 );

        ExpandEnvironmentStringsW( argv[3], exe, MAX_PATH );   /* %LOCALAPPDATA% and the like */
        swprintf( cmd, 4096, L"\"%ls\"", exe );
        for (i = 4; i < argc; i++) { wcscat( cmd, L" " ); wcscat( cmd, argv[i] ); }
        if (!CreateProcessW( exe, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi ))
        {
            printf( "launch=failed error=%lu\n", GetLastError() );
            return 1;
        }
        /* poll until a captioned window has been the choice for 4 s (a splash
         * screen comes first, and goes); at the end take what there is */
        {
            HWND chosen = 0;
            int stable = 0;
            for (i = 0; i < secs * 4; i++)
            {
                Sleep( 250 );
                f.hwnd = 0;
                EnumWindows( find_new, (LPARAM)&f );
                if (f.hwnd && f.hwnd == chosen) stable++;
                else { chosen = f.hwnd; stable = 0; }
                if (accept && chosen && stable == 8)
                {
                    WCHAR cls[16] = L"";
                    GetClassNameW( chosen, cls, ARRAYSIZE(cls) );
                    if (!lstrcmpW( cls, L"#32770" ))
                    {
                        DWORD def = SendMessageW( chosen, DM_GETDEFID, 0, 0 );
                        WORD id = HIWORD(def) == DC_HASDEFID ? LOWORD(def) : IDOK;
                        HWND button = GetDlgItem( chosen, id );
                        printf( "accepted=%#x\n", id );
                        PostMessageW( chosen, WM_COMMAND, MAKEWPARAM( id, BN_CLICKED ), (LPARAM)button );
                        accept = FALSE;
                        chosen = 0;
                        stable = 0;
                    }
                }
                /* settle early only on a main-window-sized one: a first-run
                 * dialog ("please wait while we update") comes and goes too */
                if (chosen && stable >= 16 &&
                    (GetWindowLongW( chosen, GWL_STYLE ) & WS_CAPTION) == WS_CAPTION)
                {
                    RECT rc;
                    GetWindowRect( chosen, &rc );
                    if (rc.right - rc.left >= 400 && rc.bottom - rc.top >= 300) break;
                }
            }
            f.hwnd = chosen && IsWindow( chosen ) && IsWindowVisible( chosen ) ? chosen : 0;
        }
        GetExitCodeProcess( pi.hProcess, &code );
        if (f.hwnd)
        {
            WCHAR cls[128] = L"", title[256] = L"";
            RECT rc;
            Sleep( 10000 );  /* let it paint: a first start is slow */
            GetClassNameW( f.hwnd, cls, 128 );
            GetWindowTextW( f.hwnd, title, 256 );
            GetWindowRect( f.hwnd, &rc );
            printf( "window=%#lx class=%ls size=%ldx%ld title=%ls\n", (ULONG)(ULONG_PTR)f.hwnd, cls,
                    rc.right - rc.left, rc.bottom - rc.top, title );
        }
        else printf( "window=none alive=%d exit=%#lx\n", code == STILL_ACTIVE, code );
        return 0;
    }
    if (argc == 3 && !lstrcmpW( argv[1], L"close" ))
    {
        HWND hwnd = (HWND)(ULONG_PTR)wcstoul( argv[2], NULL, 0 );
        int i;

        /* as a person would: dismiss what is in front (a first-run dialog
         * disables the main window), then close the main window */
        GetWindowThreadProcessId( hwnd, &close_pid );
        GetWindowThreadProcessId( FindWindowW( L"Shell_TrayWnd", NULL ), &shell_pid );
        for (i = 0; i < 120 && IsWindow( hwnd ) && IsWindowVisible( hwnd ); i++)
        {
            if (i % 8 == 0) EnumWindows( close_front, (LPARAM)hwnd );
            if (i % 8 == 0 && IsWindowEnabled( hwnd )) PostMessageW( hwnd, WM_CLOSE, 0, 0 );
            Sleep( 250 );
        }
        printf( "closed=%d\n", !IsWindow( hwnd ) || !IsWindowVisible( hwnd ) );
        /* and whether its process then ends (a hung shutdown keeps it) */
        {
            HANDLE process = OpenProcess( SYNCHRONIZE, FALSE, close_pid );
            if (process)
            {
                printf( "exited=%d\n", WaitForSingleObject( process, 30000 ) == WAIT_OBJECT_0 );
                CloseHandle( process );
            }
            else printf( "exited=1\n" );
        }
        return 0;
    }
    if (argc == 2 && !lstrcmpW( argv[1], L"list" ))
    {
        EnumWindows( list_proc, 0 );
        return 0;
    }
    if (argc == 3 && !lstrcmpW( argv[1], L"alive" ))
    {
        printf( "alive=%d\n", process_running( argv[2] ) );
        return 0;
    }
    return 2;
}
