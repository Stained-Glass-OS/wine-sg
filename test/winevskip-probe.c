/* WinEvent hooks set with WINEVENT_SKIPOWNPROCESS / WINEVENT_SKIPOWNTHREAD
 * (patches/sg/0861). "Own" is the process or thread that set the hook; a
 * system-wide hook (no process or thread to watch) with SKIPOWNPROCESS got
 * its own process's events anyway. AnyDesk watches EVENT_SYSTEM_FOREGROUND
 * that way and closed its own address box the moment that box took the focus.
 *
 *   probe.exe          the test; prints "name value" lines
 *   probe.exe child    fires one event from another process and exits */
#include <windows.h>
#include <stdio.h>

#define EV EVENT_OBJECT_NAMECHANGE
#define OBJ 0x5347   /* object id marking our own events */

static LONG count_all, count_skipproc, count_skipthread;

static void CALLBACK cb_all( HWINEVENTHOOK h, DWORD ev, HWND hwnd, LONG obj, LONG child, DWORD tid, DWORD time )
{ if (obj == OBJ) InterlockedIncrement( &count_all ); }
static void CALLBACK cb_skipproc( HWINEVENTHOOK h, DWORD ev, HWND hwnd, LONG obj, LONG child, DWORD tid, DWORD time )
{ if (obj == OBJ) InterlockedIncrement( &count_skipproc ); }
static void CALLBACK cb_skipthread( HWINEVENTHOOK h, DWORD ev, HWND hwnd, LONG obj, LONG child, DWORD tid, DWORD time )
{ if (obj == OBJ) InterlockedIncrement( &count_skipthread ); }

static void pump( DWORD ms )
{
    DWORD end = GetTickCount() + ms;
    MSG msg;
    while ((LONG)(end - GetTickCount()) > 0)
    {
        MsgWaitForMultipleObjects( 0, NULL, FALSE, 50, QS_ALLINPUT );
        while (PeekMessageW( &msg, 0, 0, 0, PM_REMOVE )) DispatchMessageW( &msg );
    }
}

static HWND make_window( void )
{
    return CreateWindowExW( 0, L"static", L"winevskip", WS_POPUP, 0, 0, 10, 10, 0, 0, 0, NULL );
}

static DWORD WINAPI other_thread( void *arg )
{
    HWND hwnd = make_window();
    NotifyWinEvent( EV, hwnd, OBJ, CHILDID_SELF );
    DestroyWindow( hwnd );
    return 0;
}

static void reset( void ) { count_all = count_skipproc = count_skipthread = 0; }

int main( int argc, char **argv )
{
    HWINEVENTHOOK all, skipproc, skipthread;
    WCHAR cmd[MAX_PATH + 16];
    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    HANDLE thread;
    HWND hwnd;

    if (argc > 1 && !strcmp( argv[1], "child" ))
    {
        hwnd = make_window();
        NotifyWinEvent( EV, hwnd, OBJ, CHILDID_SELF );
        Sleep( 200 );
        DestroyWindow( hwnd );
        return 0;
    }

    all = SetWinEventHook( EV, EV, NULL, cb_all, 0, 0, WINEVENT_OUTOFCONTEXT );
    skipproc = SetWinEventHook( EV, EV, NULL, cb_skipproc, 0, 0, WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS );
    skipthread = SetWinEventHook( EV, EV, NULL, cb_skipthread, 0, 0, WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNTHREAD );
    printf( "hooks %d %d %d\n", all != NULL, skipproc != NULL, skipthread != NULL );

    /* an event from this thread */
    reset();
    hwnd = make_window();
    NotifyWinEvent( EV, hwnd, OBJ, CHILDID_SELF );
    pump( 500 );
    printf( "own-thread %ld %ld %ld\n", count_all, count_skipproc, count_skipthread );

    /* an event from another thread of this process */
    reset();
    thread = CreateThread( NULL, 0, other_thread, NULL, 0, NULL );
    while (MsgWaitForMultipleObjects( 1, &thread, FALSE, INFINITE, QS_ALLINPUT ) != WAIT_OBJECT_0) pump( 10 );
    CloseHandle( thread );
    pump( 500 );
    printf( "other-thread %ld %ld %ld\n", count_all, count_skipproc, count_skipthread );

    /* an event from another process */
    reset();
    GetModuleFileNameW( NULL, cmd + 1, MAX_PATH );
    cmd[0] = '"';
    wcscat( cmd, L"\" child" );
    if (CreateProcessW( NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi ))
    {
        while (MsgWaitForMultipleObjects( 1, &pi.hProcess, FALSE, INFINITE, QS_ALLINPUT ) != WAIT_OBJECT_0) pump( 10 );
        CloseHandle( pi.hProcess );
        CloseHandle( pi.hThread );
        pump( 500 );
        printf( "other-process %ld %ld %ld\n", count_all, count_skipproc, count_skipthread );
    }
    else printf( "other-process failed %lu\n", GetLastError() );

    DestroyWindow( hwnd );
    UnhookWinEvent( all );
    UnhookWinEvent( skipproc );
    UnhookWinEvent( skipthread );
    return 0;
}
