/* Session events for services (patches/sg/9634, i1011), run by
 * test/sessionchange-vmgate.sh in a QA VM:
 *   probe.exe --service LOG   the test service: accepts SERVICE_CONTROL_SESSIONCHANGE
 *                             and appends "EVENT <type> <session>" to LOG for each
 *   probe.exe session         prints "SESSION <its session>"
 *   probe.exe tell EVENT ID   I_ScSendTSMessage(EVENT, ID): prints "TELL <error>" */
#include <windows.h>
#include <stdio.h>

typedef DWORD (WINAPI *send_ts_fn)( DWORD event, DWORD session_id, DWORD size, void *data );

static SERVICE_STATUS_HANDLE status_handle;
static HANDLE stop_event;
static char log_path[MAX_PATH];

static void report( DWORD state )
{
    SERVICE_STATUS st = { SERVICE_WIN32_OWN_PROCESS, state,
                          state == SERVICE_RUNNING ? SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SESSIONCHANGE : 0 };
    SetServiceStatus( status_handle, &st );
}

static DWORD WINAPI handler( DWORD ctl, DWORD type, void *data, void *ctx )
{
    if (ctl == SERVICE_CONTROL_STOP) { report( SERVICE_STOP_PENDING ); SetEvent( stop_event ); }
    if (ctl == SERVICE_CONTROL_SESSIONCHANGE)
    {
        const DWORD *n = data;   /* WTSSESSION_NOTIFICATION: cbSize, dwSessionId */
        char line[64];
        DWORD w;
        HANDLE f = CreateFileA( log_path, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_ALWAYS, 0, NULL );
        snprintf( line, sizeof(line), "EVENT %lu %lu %lu\n", type, n ? n[1] : 99999, n ? n[0] : 0 );
        if (f != INVALID_HANDLE_VALUE) { WriteFile( f, line, strlen( line ), &w, NULL ); CloseHandle( f ); }
    }
    return NO_ERROR;
}

static void WINAPI service_main( DWORD argc, char **argv )
{
    status_handle = RegisterServiceCtrlHandlerExA( argc ? argv[0] : "", handler, NULL );
    stop_event = CreateEventA( NULL, TRUE, FALSE, NULL );
    report( SERVICE_RUNNING );
    WaitForSingleObject( stop_event, INFINITE );
    report( SERVICE_STOPPED );
}

int main( int argc, char **argv )
{
    if (argc > 2 && !strcmp( argv[1], "--service" ))
    {
        SERVICE_TABLE_ENTRYA table[] = { { (char *)"", service_main }, { NULL, NULL } };
        lstrcpynA( log_path, argv[2], sizeof(log_path) );
        StartServiceCtrlDispatcherA( table );
        return 0;
    }
    if (argc > 1 && !strcmp( argv[1], "session" ))
    {
        DWORD s = 0;
        ProcessIdToSessionId( GetCurrentProcessId(), &s );
        printf( "SESSION %lu\n", s );
        return 0;
    }
    if (argc > 3 && !strcmp( argv[1], "tell" ))
    {
        send_ts_fn send = (send_ts_fn)GetProcAddress( LoadLibraryA( "sechost.dll" ), "I_ScSendTSMessage" );
        if (!send) { printf( "TELL none\n" ); return 0; }
        printf( "TELL %lu\n", send( strtoul( argv[2], NULL, 0 ), strtoul( argv[3], NULL, 0 ), 0, NULL ) );
        return 0;
    }
    return 1;
}
