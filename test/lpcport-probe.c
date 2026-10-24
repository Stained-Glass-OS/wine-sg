/* lpcport-probe.c: LPC ports (NtCreatePort ... NtReplyWaitReceivePort) for test/lpcport-gate.sh.
 *   lpcport-probe.exe              the server (and in-process clients); starts the child clients
 *   lpcport-probe.exe child PORT   a client in another process (also built 32-bit: the 64-bit message layout)
 */
#include <windows.h>
#include <winternl.h>
#include <stdio.h>
#include <string.h>

#ifndef STATUS_PORT_CONNECTION_REFUSED
#define STATUS_PORT_CONNECTION_REFUSED ((NTSTATUS)0xC0000041)
#endif
#define STATUS_PORT_MESSAGE_TOO_LONG_ ((NTSTATUS)0xC000002F)
#define STATUS_PORT_DISCONNECTED_     ((NTSTATUS)0xC0000037)
#define STATUS_REPLY_MESSAGE_MISMATCH_ ((NTSTATUS)0xC000021F)
#define STATUS_SERVER_SID_MISMATCH_   ((NTSTATUS)0xC00002A0)
#define STATUS_OBJECT_NAME_COLLISION_ ((NTSTATUS)0xC0000035)
#define STATUS_OBJECT_NAME_NOT_FOUND_ ((NTSTATUS)0xC0000034)
#define STATUS_INVALID_PARAMETER_     ((NTSTATUS)0xC000000D)
#define STATUS_TIMEOUT_               ((NTSTATUS)0x00000102)

#define LPC_REQUEST             1
#define LPC_REPLY               2
#define LPC_DATAGRAM            3
#define LPC_PORT_CLOSED         5
#define LPC_CLIENT_DIED         6
#define LPC_CONNECTION_REQUEST  10

/* the 64-bit layout, which 32-bit callers use as well */
typedef struct
{
    USHORT DataSize;
    USHORT MessageSize;
    USHORT MessageType;
    USHORT VirtualRangesOffset;
    ULONGLONG ClientId[2];
    ULONGLONG MessageId;
    ULONGLONG SectionSize;
    UCHAR Data[304];
} MSG64;
#define HDR 40

typedef NTSTATUS (WINAPI *fnCreatePort)(HANDLE *, OBJECT_ATTRIBUTES *, ULONG, ULONG, ULONG *);
typedef NTSTATUS (WINAPI *fnConnectPort)(HANDLE *, UNICODE_STRING *, SECURITY_QUALITY_OF_SERVICE *, void *, void *,
                                         ULONG *, void *, ULONG *);
typedef NTSTATUS (WINAPI *fnSecureConnectPort)(HANDLE *, UNICODE_STRING *, SECURITY_QUALITY_OF_SERVICE *, void *,
                                               PSID, void *, ULONG *, void *, ULONG *);
typedef NTSTATUS (WINAPI *fnAcceptConnectPort)(HANDLE *, ULONG, void *, BOOLEAN, void *, void *);
typedef NTSTATUS (WINAPI *fnHandle)(HANDLE);
typedef NTSTATUS (WINAPI *fnMsg)(HANDLE, void *);
typedef NTSTATUS (WINAPI *fnMsg2)(HANDLE, void *, void *);
typedef NTSTATUS (WINAPI *fnReplyWaitReceive)(HANDLE, ULONG *, void *, void *);
typedef NTSTATUS (WINAPI *fnReplyWaitReceiveEx)(HANDLE, void **, void *, void *, LARGE_INTEGER *);

static fnCreatePort pNtCreatePort;
static fnConnectPort pNtConnectPort;
static fnSecureConnectPort pNtSecureConnectPort;
static fnAcceptConnectPort pNtAcceptConnectPort;
static fnHandle pNtCompleteConnectPort, pNtRegisterThreadTerminatePort, pNtClose;
static fnMsg pNtRequestPort, pNtReplyPort, pNtListenPort;
static fnMsg2 pNtRequestWaitReplyPort;
static fnReplyWaitReceive pNtReplyWaitReceivePort;
static fnReplyWaitReceiveEx pNtReplyWaitReceivePortEx;

static WCHAR port_name[64];
static UNICODE_STRING port_us;

static void set_msg( MSG64 *m, const char *text, int size )
{
    memset( m, 0, sizeof(*m) );
    m->DataSize = size >= 0 ? size : (USHORT)(strlen( text ) + 1);
    m->MessageSize = HDR + m->DataSize;
    if (text) memcpy( m->Data, text, strlen( text ) + 1 );
}

static NTSTATUS lpc_connect( HANDLE *h, const char *info, char *out, ULONG *max_len )
{
    SECURITY_QUALITY_OF_SERVICE qos = { sizeof(qos), SecurityImpersonation, SECURITY_STATIC_TRACKING, TRUE };
    char buffer[100];
    ULONG len = info ? strlen( info ) + 1 : 0;
    NTSTATUS status;

    memset( buffer, 0, sizeof(buffer) );
    if (info) strcpy( buffer, info );
    if (info) len = 100;   /* room for the server's answer; the request carries all of it */
    status = pNtConnectPort( h, &port_us, &qos, NULL, NULL, max_len, info ? buffer : NULL, info ? &len : NULL );
    if (out) { memcpy( out, buffer, sizeof(buffer) ); out[99] = 0; }
    return status;
}

/* ---- the child: a client in another process ---- */
static int child( const char *name )
{
    MSG64 in, out;
    HANDLE h = 0;
    char answer[100];
    ULONG max_len = 0;
    NTSTATUS status;

    MultiByteToWideChar( CP_ACP, 0, name, -1, port_name, ARRAYSIZE(port_name) );
    RtlInitUnicodeString( &port_us, port_name );
    status = lpc_connect( &h, "child", answer, &max_len );
    printf( "ChildConnect=0x%08lx %s %lu\n", status, answer, max_len );
    if (status) return 1;
    pNtRegisterThreadTerminatePort( h );
    set_msg( &in, sizeof(void *) == 4 ? "ping32" : "ping64", -1 );
    status = pNtRequestWaitReplyPort( h, &in, &out );
    printf( "ChildRequest=0x%08lx %d %s\n", status, out.MessageType, (char *)out.Data );
    /* exits with the port open: the server hears LPC_CLIENT_DIED and LPC_PORT_CLOSED */
    return 0;
}

/* ---- the server ---- */
static HANDLE server_port, accepted[16];
static DWORD main_tid;
static int accepted_count;
static volatile LONG datagrams, client_died, port_closed, refused, bad_context, drop_reply_next;
static DWORD died_tid;
static char last_datagram[64], ids_seen[200];

static DWORD WINAPI server_thread( void *arg )
{
    MSG64 msg, *reply = NULL, reply_buf;
    ULONG context;
    NTSTATUS status;

    for (;;)
    {
        memset( &msg, 0xcc, sizeof(msg) );
        status = pNtReplyWaitReceivePort( server_port, &context, reply, &msg );
        reply = NULL;
        if (status)
        {
            printf( "ServerLoop=0x%08lx\n", status );
            return 1;
        }
        switch (msg.MessageType)
        {
        case LPC_CONNECTION_REQUEST:
        {
            BOOL accept = strcmp( (char *)msg.Data, "deny" ) != 0;
            HANDLE h = 0;

            if (!accept) InterlockedIncrement( &refused );
            strcpy( (char *)msg.Data, "welcome" );
            msg.DataSize = 8;
            msg.MessageSize = HDR + 8;
            status = pNtAcceptConnectPort( &h, 0x1234 + accepted_count, &msg, accept, NULL, NULL );
            if (accept && !status)
            {
                accepted[accepted_count++] = h;
                pNtCompleteConnectPort( h );
            }
            break;
        }
        case LPC_DATAGRAM:
            if (context < 0x1234) InterlockedIncrement( &bad_context );
            lstrcpynA( last_datagram, (char *)msg.Data, sizeof(last_datagram) );
            InterlockedIncrement( &datagrams );
            break;
        case LPC_REQUEST:
            if (context < 0x1234) InterlockedIncrement( &bad_context );
            if (!strcmp( (char *)msg.Data, "quit" ))
            {
                reply_buf = msg;
                set_msg( &msg, "bye", -1 );
                msg.MessageId = reply_buf.MessageId;
                msg.ClientId[0] = reply_buf.ClientId[0];
                msg.ClientId[1] = reply_buf.ClientId[1];
                pNtReplyPort( server_port, &msg );
                return 0;
            }
            if (!strcmp( (char *)msg.Data, "hang" ))
            {
                /* no reply: the client's server port is closed under it */
                int i;
                for (i = 0; i < accepted_count; i++)
                    if (accepted[i] && 0x1234 + i == context) { pNtClose( accepted[i] ); accepted[i] = 0; }
                break;
            }
            if (!strcmp( (char *)msg.Data, "twice" ))
            {
                /* a reply to a message id nobody sent */
                MSG64 bogus;
                set_msg( &bogus, "x", -1 );
                bogus.MessageId = msg.MessageId + 1000;
                sprintf( ids_seen, "0x%08lx", pNtReplyPort( server_port, &bogus ) );
            }
            reply_buf = msg;
            {
                char text[300];
                sprintf( text, "reply:%s:%lu:%d", (char *)msg.Data, context,
                         msg.ClientId[1] == (ULONGLONG)main_tid &&
                         msg.ClientId[0] == (ULONGLONG)GetCurrentProcessId() );
                set_msg( &reply_buf, text, -1 );
            }
            reply_buf.MessageId = msg.MessageId;
            reply_buf.ClientId[0] = msg.ClientId[0];
            reply_buf.ClientId[1] = msg.ClientId[1];
            reply = &reply_buf;
            break;
        case LPC_CLIENT_DIED:
            died_tid = (DWORD)msg.ClientId[1];
            InterlockedIncrement( &client_died );
            break;
        case LPC_PORT_CLOSED:
            InterlockedIncrement( &port_closed );
            break;
        default:
            printf( "Unexpected=%d\n", msg.MessageType );
            break;
        }
    }
}

static volatile DWORD client_tid;

static DWORD WINAPI dying_client( void *arg )
{
    HANDLE h = 0;
    client_tid = GetCurrentThreadId();
    if (!lpc_connect( &h, "dier", NULL, NULL )) pNtRegisterThreadTerminatePort( h );
    return 0;   /* the handle stays open */
}

static DWORD WINAPI hanging_client( void *arg )
{
    MSG64 in, out;
    HANDLE h = 0;
    NTSTATUS status = lpc_connect( &h, "hanger", NULL, NULL );

    if (status) return status;
    set_msg( &in, "hang", -1 );
    status = pNtRequestWaitReplyPort( h, &in, &out );
    pNtClose( h );
    return status;
}

static int wait_for( volatile LONG *value, LONG want, DWORD ms )
{
    DWORD start = GetTickCount();
    while (*value < want && GetTickCount() - start < ms) Sleep( 20 );
    return *value >= want;
}

int main( int argc, char **argv )
{
    HMODULE ntdll = GetModuleHandleA( "ntdll.dll" );
    OBJECT_ATTRIBUTES attr;
    HANDLE h, h2, thread, srv;
    MSG64 in, out;
    char answer[100], exe[MAX_PATH], cmd[MAX_PATH * 2];
    ULONG max_len = 0;
    NTSTATUS status, status2;
    DWORD code;
    int i;

#define LOAD(name) p##name = (void *)GetProcAddress( ntdll, #name )
    LOAD(NtCreatePort); LOAD(NtConnectPort); LOAD(NtSecureConnectPort); LOAD(NtAcceptConnectPort);
    LOAD(NtCompleteConnectPort); LOAD(NtRegisterThreadTerminatePort); LOAD(NtClose); LOAD(NtRequestPort);
    LOAD(NtReplyPort); LOAD(NtListenPort); LOAD(NtRequestWaitReplyPort); LOAD(NtReplyWaitReceivePort);
    LOAD(NtReplyWaitReceivePortEx);
    setvbuf( stdout, NULL, _IONBF, 0 );
    if (!pNtCreatePort || !pNtRequestPort || !pNtReplyPort || !pNtReplyWaitReceivePortEx)
    {
        printf( "Exports=0\n" );
        return 1;
    }
    printf( "Exports=1\n" );

    if (argc > 2 && !strcmp( argv[1], "child" )) return child( argv[2] );
    main_tid = GetCurrentThreadId();

    swprintf( port_name, ARRAYSIZE(port_name), L"\\SgLpcProbe%lu", GetCurrentProcessId() );
    RtlInitUnicodeString( &port_us, port_name );
    InitializeObjectAttributes( &attr, &port_us, 0, NULL, NULL );

    status = pNtCreatePort( &server_port, &attr, 100, 0x148, NULL );
    status2 = pNtCreatePort( &h, &attr, 100, 0x148, NULL );
    printf( "Create=0x%08lx 0x%08lx\n", status, status2 );
    if (status) return 1;

    /* nothing waiting: the Ex form times out */
    {
        LARGE_INTEGER timeout;
        void *ctx = NULL;
        timeout.QuadPart = -2000000;   /* 200 ms */
        status = pNtReplyWaitReceivePortEx( server_port, &ctx, NULL, &in, &timeout );
        printf( "Timeout=0x%08lx\n", status );
    }

    srv = CreateThread( NULL, 0, server_thread, NULL, 0, NULL );

    /* an in-process client */
    status = lpc_connect( &h, "hello-info", answer, &max_len );
    printf( "Connect=0x%08lx %s %lu\n", status, answer, max_len );
    status = lpc_connect( &h2, "deny", NULL, NULL );
    printf( "Refused=0x%08lx %ld\n", status, refused );
    {
        UNICODE_STRING missing;
        SECURITY_QUALITY_OF_SERVICE qos = { sizeof(qos), SecurityImpersonation, SECURITY_STATIC_TRACKING, TRUE };
        RtlInitUnicodeString( &missing, L"\\SgLpcNobodyHere" );
        status = pNtConnectPort( &h2, &missing, &qos, NULL, NULL, NULL, NULL, NULL );
        printf( "Missing=0x%08lx\n", status );
    }

    set_msg( &in, "dg1", -1 );
    status = pNtRequestPort( h, &in );
    wait_for( &datagrams, 1, 3000 );
    printf( "Datagram=0x%08lx %d %ld %s %ld\n", status, in.MessageType, datagrams, last_datagram, bad_context );

    set_msg( &in, "ping", -1 );
    memset( &out, 0, sizeof(out) );
    status = pNtRequestWaitReplyPort( h, &in, &out );
    printf( "Request=0x%08lx %d %u %u %s\n", status, out.MessageType, out.DataSize,
            out.MessageSize == HDR + out.DataSize, (char *)out.Data );

    set_msg( &in, "twice", -1 );
    status = pNtRequestWaitReplyPort( h, &in, &out );
    printf( "Mismatch=0x%08lx %s %s\n", status, ids_seen, (char *)out.Data );

    set_msg( &in, NULL, 400 );
    status = pNtRequestPort( h, &in );
    set_msg( &in, "abc", -1 );
    in.MessageSize = HDR + 1;
    status2 = pNtRequestPort( h, &in );
    printf( "BadMessages=0x%08lx 0x%08lx\n", status, status2 );

    /* NtSecureConnectPort: the server's user must be the given one */
    {
        SECURITY_QUALITY_OF_SERVICE qos = { sizeof(qos), SecurityImpersonation, SECURITY_STATIC_TRACKING, TRUE };
        char user_buf[256];
        TOKEN_USER *user = (TOKEN_USER *)user_buf;
        SID_IDENTIFIER_AUTHORITY nt = { SECURITY_NT_AUTHORITY };
        PSID system = NULL;
        HANDLE token, h3 = 0;
        DWORD len;

        OpenProcessToken( GetCurrentProcess(), TOKEN_QUERY, &token );
        GetTokenInformation( token, TokenUser, user, sizeof(user_buf), &len );
        CloseHandle( token );
        AllocateAndInitializeSid( &nt, 1, SECURITY_LOCAL_SYSTEM_RID, 0, 0, 0, 0, 0, 0, 0, &system );
        status = pNtSecureConnectPort( &h3, &port_us, &qos, NULL, user->User.Sid, NULL, NULL, NULL, NULL );
        if (!status) pNtClose( h3 );
        status2 = pNtSecureConnectPort( &h3, &port_us, &qos, NULL, system, NULL, NULL, NULL, NULL );
        printf( "SecureConnect=0x%08lx 0x%08lx\n", status, status2 );
        FreeSid( system );
    }

    /* a client thread that registered its port ends */
    i = client_died;
    CloseHandle( CreateThread( NULL, 0, dying_client, NULL, 0, NULL ) );
    wait_for( &client_died, i + 1, 5000 );
    printf( "ClientDied=%ld %d\n", client_died - i, died_tid == client_tid );

    /* the server closes the client's server port while it waits for a reply */
    thread = CreateThread( NULL, 0, hanging_client, NULL, 0, NULL );
    code = WaitForSingleObject( thread, 5000 ) ? 1 : 0;
    GetExitCodeThread( thread, &code );
    CloseHandle( thread );
    printf( "Disconnected=0x%08lx\n", code );

    /* clients in other processes, 64-bit and 32-bit */
    for (i = 0; i < 2; i++)
    {
        STARTUPINFOA si = { sizeof(si) };
        PROCESS_INFORMATION pi;
        LONG died = client_died, closed = port_closed;
        char name[64];

        GetModuleFileNameA( NULL, exe, sizeof(exe) );
        if (i == 1)
        {
            char *p = strrchr( exe, '\\' );
            strcpy( p + 1, "lpcport-probe32.exe" );
        }
        WideCharToMultiByte( CP_ACP, 0, port_name, -1, name, sizeof(name), NULL, NULL );
        sprintf( cmd, "\"%s\" child %s", exe, name );
        if (!CreateProcessA( exe, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi ))
        {
            printf( "Child%d=none\n", i );
            continue;
        }
        WaitForSingleObject( pi.hProcess, 20000 );
        GetExitCodeProcess( pi.hProcess, &code );
        CloseHandle( pi.hProcess );
        CloseHandle( pi.hThread );
        wait_for( &client_died, died + 1, 5000 );
        wait_for( &port_closed, closed + 1, 5000 );
        printf( "Child%d=%lu %ld %ld\n", i, code, client_died - died, port_closed - closed );
    }

    /* a port that is closed can no longer be connected to */
    set_msg( &in, "quit", -1 );
    status = pNtRequestWaitReplyPort( h, &in, &out );
    printf( "Quit=0x%08lx %s\n", status, (char *)out.Data );
    printf( "ServerEnd=%d\n", WaitForSingleObject( srv, 5000 ) == WAIT_OBJECT_0 );
    pNtClose( server_port );
    status = lpc_connect( &h2, "late", NULL, NULL );
    printf( "AfterClose=0x%08lx\n", status );
    printf( "done=1\n" );
    return 0;
}
