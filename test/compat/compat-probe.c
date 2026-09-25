/* compat-probe: start an application and find its main window, for
 * test/compat/run.sh.
 *
 *   compat-probe launch SECONDS EXE [ARGS]   start it; wait for a new visible
 *                                           top-level window (any process: some
 *                                           applications are launchers)
 *                                           window=0x.. class=.. title=.. WxH
 *                                           or window=none alive=0|1
 *   compat-probe close HWND                 WM_CLOSE, then whether it went
 *   compat-probe alive EXE-NAME             whether a process of that name runs
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
    /* the largest new window: the main one, not a dialog or splash in front */
    if (f->hwnd && GetWindowRect( f->hwnd, &old ) && IsWindowVisible( f->hwnd ) &&
        (old.right - old.left) * (old.bottom - old.top) >= (rc.right - rc.left) * (rc.bottom - rc.top))
        return TRUE;
    f->hwnd = hwnd;
    return TRUE;
}

static DWORD close_pid;

/* the process's other visible, enabled top-level windows (its dialogs) */
static BOOL CALLBACK close_front( HWND hwnd, LPARAM main )
{
    DWORD pid;
    if (hwnd == (HWND)main || !IsWindowVisible( hwnd ) || !IsWindowEnabled( hwnd )) return TRUE;
    GetWindowThreadProcessId( hwnd, &pid );
    if (pid == close_pid) PostMessageW( hwnd, WM_CLOSE, 0, 0 );
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

        EnumWindows( snapshot, 0 );
        WCHAR exe[MAX_PATH];

        ExpandEnvironmentStringsW( argv[3], exe, MAX_PATH );   /* %LOCALAPPDATA% and the like */
        swprintf( cmd, 4096, L"\"%ls\"", exe );
        for (i = 4; i < argc; i++) { wcscat( cmd, L" " ); wcscat( cmd, argv[i] ); }
        if (!CreateProcessW( exe, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi ))
        {
            printf( "launch=failed error=%lu\n", GetLastError() );
            return 1;
        }
        for (i = 0; i < secs * 4 && !f.hwnd; i++)
        {
            Sleep( 250 );
            EnumWindows( find_new, (LPARAM)&f );
        }
        /* a splash screen comes first: give the main window time, then take
         * the largest new one */
        if (f.hwnd)
        {
            Sleep( 5000 );
            f.hwnd = 0;
            EnumWindows( find_new, (LPARAM)&f );
        }
        GetExitCodeProcess( pi.hProcess, &code );
        if (f.hwnd)
        {
            WCHAR cls[128] = L"", title[256] = L"";
            RECT rc;
            Sleep( 2000 );  /* let it paint */
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
        for (i = 0; i < 60 && IsWindow( hwnd ) && IsWindowVisible( hwnd ); i++)
        {
            if (i % 8 == 0) EnumWindows( close_front, (LPARAM)hwnd );
            if (i % 8 == 0 && IsWindowEnabled( hwnd )) PostMessageW( hwnd, WM_CLOSE, 0, 0 );
            Sleep( 250 );
        }
        printf( "closed=%d\n", !IsWindow( hwnd ) || !IsWindowVisible( hwnd ) );
        return 0;
    }
    if (argc == 3 && !lstrcmpW( argv[1], L"alive" ))
    {
        printf( "alive=%d\n", process_running( argv[2] ) );
        return 0;
    }
    return 2;
}
