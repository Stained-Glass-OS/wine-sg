/* test/hookcount-gate.sh's probe (patches/sg/9655): a global hook removed while
 * its chain is in use (it is being called), then removed once more by Wine's
 * own bookkeeping:
 *   A  UnhookWindowsHookEx twice from inside the hook;
 *   B  the hook's owner thread unhooks and ends while the chain is in use;
 *   C  inside the hook, after it was unhooked, another thread with a message
 *      queue moves to another desktop.
 * On Windows the second UnhookWindowsHookEx fails with
 * ERROR_INVALID_HOOK_HANDLE and nothing is taken twice. The server's count of a
 * thread's hooks must stay balanced (test/hookcount-gate.sh reads the server's
 * stderr). At the end, D: a new global hook is still called.
 * Usage: hookcount-probe.exe A|B|C|D. Prints KEY VALUE lines. */
#include <windows.h>
#include <stdio.h>

static HMODULE dll;
static void (*set_cb)( void (*)(void) );
static HOOKPROC msgproc;
static HHOOK hook;
static HANDLE ev_ready, ev_go;
static HANDLE other;
static DWORD main_tid;

static void pump_once( void )
{
    MSG msg;
    PostThreadMessageA( main_tid, WM_APP + 1, 0, 0 );
    while (GetMessageA( &msg, 0, 0, 0 )) if (msg.message == WM_APP + 1) break;
}

/* A */
static void cb_a( void )
{
    BOOL r1, r2; DWORD err;
    r1 = UnhookWindowsHookEx( hook );
    SetLastError( 0xdeadbeef );
    r2 = UnhookWindowsHookEx( hook );
    err = GetLastError();
    printf( "A_FIRST %d\nA_SECOND %d %lu\n", r1, r2, err );
}

/* B */
static DWORD WINAPI owner_thread( void *arg )
{
    MSG msg;
    PeekMessageA( &msg, 0, 0, 0, PM_NOREMOVE );
    hook = SetWindowsHookExA( WH_GETMESSAGE, msgproc, dll, 0 );
    SetEvent( ev_ready );
    WaitForSingleObject( ev_go, 10000 );
    printf( "B_UNHOOK %d\n", UnhookWindowsHookEx( hook ) );
    return 0;   /* the thread ends: Wine removes its global hooks */
}

static void cb_b( void )
{
    SetEvent( ev_go );
    printf( "B_ENDED %d\n", WaitForSingleObject( other, 10000 ) == WAIT_OBJECT_0 );
}

/* C */
static DWORD WINAPI mover_thread( void *arg )
{
    MSG msg;
    HDESK desk;
    PeekMessageA( &msg, 0, 0, 0, PM_NOREMOVE );   /* a message queue, no windows */
    SetEvent( ev_ready );
    WaitForSingleObject( ev_go, 10000 );
    desk = CreateDesktopA( "hookcount-other", NULL, NULL, 0, GENERIC_ALL, NULL );
    printf( "C_MOVED %d\n", desk && SetThreadDesktop( desk ) );
    return 0;
}

static void cb_c( void )
{
    printf( "C_UNHOOK %d\n", UnhookWindowsHookEx( hook ) );
    SetEvent( ev_go );
    printf( "C_ENDED %d\n", WaitForSingleObject( other, 10000 ) == WAIT_OBJECT_0 );
}

/* D */
static int d_called;
static void cb_d( void ) { d_called = 1; }

int main( int argc, char **argv )
{
    char which = argc > 1 ? argv[1][0] : 'A';
    MSG msg;

    PeekMessageA( &msg, 0, 0, 0, PM_NOREMOVE );
    main_tid = GetCurrentThreadId();
    ev_ready = CreateEventA( NULL, FALSE, FALSE, NULL );
    ev_go = CreateEventA( NULL, FALSE, FALSE, NULL );
    if (!(dll = LoadLibraryA( "hookcount-dll.dll" ))) { printf( "DLL 0\n" ); return 1; }
    set_cb = (void *)GetProcAddress( dll, "hc_set_callback" );
    msgproc = (HOOKPROC)GetProcAddress( dll, "hc_msgproc" );
    if (!set_cb || !msgproc) { printf( "DLL 0\n" ); return 1; }

    switch (which)
    {
    case 'A':
        hook = SetWindowsHookExA( WH_GETMESSAGE, msgproc, dll, 0 );
        printf( "HOOK %d\n", hook != NULL );
        set_cb( cb_a );
        pump_once();
        break;
    case 'B':
        other = CreateThread( NULL, 0, owner_thread, NULL, 0, NULL );
        WaitForSingleObject( ev_ready, 10000 );
        printf( "HOOK %d\n", hook != NULL );
        set_cb( cb_b );
        pump_once();
        break;
    case 'C':
        other = CreateThread( NULL, 0, mover_thread, NULL, 0, NULL );
        WaitForSingleObject( ev_ready, 10000 );
        hook = SetWindowsHookExA( WH_GETMESSAGE, msgproc, dll, 0 );
        printf( "HOOK %d\n", hook != NULL );
        set_cb( cb_c );
        pump_once();
        break;
    case 'D':
        break;
    }
    /* D (after every scenario): a new global hook is called */
    hook = SetWindowsHookExA( WH_GETMESSAGE, msgproc, dll, 0 );
    set_cb( cb_d );
    pump_once();
    printf( "D_CALLED %d\n", d_called );
    UnhookWindowsHookEx( hook );
    fflush( stdout );
    return 0;
}
