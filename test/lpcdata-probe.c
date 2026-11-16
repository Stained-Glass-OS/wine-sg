/* lpcdata-probe.c: NtReadRequestData / NtWriteRequestData (wine-sg 7809) for test/lpcdata-gate.sh.
 *   lpcdata-probe.exe              the server: starts the clients (64-bit, then 32-bit) in other processes
 *   lpcdata-probe.exe child PORT   a client: sends a request whose data information describes two buffers
 */
#include <windows.h>
#include <winternl.h>
#include <stdio.h>
#include <string.h>

#define LPC_REQUEST             1
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

/* the data information, 64-bit layout: a count, then { base, size } entries */
typedef struct { ULONGLONG Base; ULONG Size; ULONG pad; } ENTRY64;
typedef struct { ULONG Count; ULONG pad; ENTRY64 Entries[2]; } INFO64;

typedef NTSTATUS (WINAPI *fnCreatePort)(HANDLE *, OBJECT_ATTRIBUTES *, ULONG, ULONG, ULONG *);
typedef NTSTATUS (WINAPI *fnConnectPort)(HANDLE *, UNICODE_STRING *, SECURITY_QUALITY_OF_SERVICE *, void *, void *,
                                         ULONG *, void *, ULONG *);
typedef NTSTATUS (WINAPI *fnAcceptConnectPort)(HANDLE *, ULONG, void *, BOOLEAN, void *, void *);
typedef NTSTATUS (WINAPI *fnHandle)(HANDLE);
typedef NTSTATUS (WINAPI *fnMsg)(HANDLE, void *);
typedef NTSTATUS (WINAPI *fnMsg2)(HANDLE, void *, void *);
typedef NTSTATUS (WINAPI *fnReplyWaitReceive)(HANDLE, ULONG *, void *, void *);
typedef NTSTATUS (WINAPI *fnRequestData)(HANDLE, void *, ULONG, void *, ULONG, ULONG *);

static fnCreatePort pNtCreatePort;
static fnConnectPort pNtConnectPort;
static fnAcceptConnectPort pNtAcceptConnectPort;
static fnHandle pNtCompleteConnectPort;
static fnMsg pNtReplyPort;
static fnMsg2 pNtRequestWaitReplyPort;
static fnReplyWaitReceive pNtReplyWaitReceivePort;
static fnRequestData pNtReadRequestData, pNtWriteRequestData;

/* buffer A, then what lies after it in memory: a read past A's size must not reach it */
static struct { char a[24]; char secret[40]; } client_mem = { "client-buffer-A", "SECRET-past-the-entry" };
static char client_b[32];

static int child( const char *name )
{
    SECURITY_QUALITY_OF_SERVICE qos = { sizeof(qos), SecurityImpersonation, SECURITY_STATIC_TRACKING, TRUE };
    WCHAR wname[64];
    UNICODE_STRING us;
    MSG64 in, out;
    INFO64 info;
    HANDLE h = 0;
    NTSTATUS status;

    MultiByteToWideChar( CP_ACP, 0, name, -1, wname, ARRAYSIZE(wname) );
    RtlInitUnicodeString( &us, wname );
    status = pNtConnectPort( &h, &us, &qos, NULL, NULL, NULL, NULL, NULL );
    if (status) return 1;

    memset( &in, 0, sizeof(in) );
    strcpy( (char *)in.Data, "req" );
    memset( &info, 0, sizeof(info) );
    info.Count = 2;
    info.Entries[0].Base = (ULONG_PTR)client_mem.a;
    info.Entries[0].Size = sizeof(client_mem.a);
    info.Entries[1].Base = (ULONG_PTR)client_b;
    info.Entries[1].Size = sizeof(client_b);
    memcpy( in.Data + 16, &info, sizeof(info) );
    in.DataSize = 16 + sizeof(info);
    in.MessageSize = HDR + in.DataSize;
    in.VirtualRangesOffset = HDR + 16;
    status = pNtRequestWaitReplyPort( h, &in, &out );
    /* the exit code tells the server: 0 the reply came and buffer B holds what it wrote */
    if (status) return 2;
    if (strcmp( (char *)out.Data, "done" )) return 3;
    if (strcmp( client_b, "written-by-server" )) return 4;
    return 0;
}

static NTSTATUS rd( HANDLE port, MSG64 *msg, ULONG index, char *buf, ULONG len, ULONG *got )
{
    *got = 0xdead;
    memset( buf, 0, 64 );
    return pNtReadRequestData( port, msg, index, buf, len, got );
}

/* serves one client: checks what the request lets it read and write */
static void serve( HANDLE server_port, const char *tag )
{
    MSG64 msg, bad;
    ULONG context, got;
    char buf[64];
    HANDLE h = 0;
    NTSTATUS status;

    /* (the previous client's end comes first: LPC_CLIENT_DIED, LPC_PORT_CLOSED) */
    do
    {
        memset( &msg, 0, sizeof(msg) );
        status = pNtReplyWaitReceivePort( server_port, &context, NULL, &msg );
    } while (!status && (msg.MessageType == 5 || msg.MessageType == 6));
    if (status || msg.MessageType != LPC_CONNECTION_REQUEST)
    {
        printf( "%s.Connect=0x%08lx %d\n", tag, status, msg.MessageType );
        return;
    }
    msg.DataSize = 0;
    msg.MessageSize = HDR;
    pNtAcceptConnectPort( &h, 1, &msg, TRUE, NULL, NULL );
    pNtCompleteConnectPort( h );

    memset( &msg, 0, sizeof(msg) );
    status = pNtReplyWaitReceivePort( server_port, &context, NULL, &msg );
    printf( "%s.Request=0x%08lx %d %d\n", tag, status, msg.MessageType, msg.VirtualRangesOffset );
    if (status || msg.MessageType != LPC_REQUEST) return;

    status = rd( server_port, &msg, 0, buf, 24, &got );
    printf( "%s.Read=0x%08lx %lu %s\n", tag, status, got, buf );
    status = rd( server_port, &msg, 0, buf, 64, &got );
    printf( "%s.Over=0x%08lx %d\n", tag, status, strstr( buf + 24, "SECRET" ) != NULL );
    status = rd( server_port, &msg, 2, buf, 4, &got );
    printf( "%s.Index=0x%08lx\n", tag, status );
    got = 0;
    status = pNtWriteRequestData( server_port, &msg, 1, (void *)"written-by-server", 18, &got );
    printf( "%s.Write=0x%08lx %lu\n", tag, status, got );
    status = pNtWriteRequestData( server_port, &msg, 1, buf, 33, &got );
    printf( "%s.WriteOver=0x%08lx\n", tag, status );
    bad = msg;
    bad.MessageId += 1000;
    printf( "%s.OtherId=0x%08lx\n", tag, rd( server_port, &bad, 0, buf, 4, &got ) );
    bad = msg;
    bad.ClientId[1] += 4;
    printf( "%s.OtherClient=0x%08lx\n", tag, rd( server_port, &bad, 0, buf, 4, &got ) );

    {
        MSG64 reply = msg;
        memset( reply.Data, 0, sizeof(reply.Data) );
        strcpy( (char *)reply.Data, "done" );
        reply.DataSize = 5;
        reply.MessageSize = HDR + 5;
        reply.VirtualRangesOffset = 0;
        status = pNtReplyPort( server_port, &reply );
    }
    printf( "%s.After=0x%08lx\n", tag, rd( server_port, &msg, 0, buf, 4, &got ) );
}

int main( int argc, char **argv )
{
    HMODULE ntdll = GetModuleHandleA( "ntdll.dll" );
    OBJECT_ATTRIBUTES attr;
    UNICODE_STRING us;
    HANDLE server_port;
    char exe[MAX_PATH], cmd[MAX_PATH + 64];
    NTSTATUS status;
    int i;

#define LOAD(name) p##name = (void *)GetProcAddress( ntdll, #name )
    LOAD(NtCreatePort); LOAD(NtConnectPort); LOAD(NtAcceptConnectPort); LOAD(NtCompleteConnectPort);
    LOAD(NtReplyPort); LOAD(NtRequestWaitReplyPort); LOAD(NtReplyWaitReceivePort);
    LOAD(NtReadRequestData); LOAD(NtWriteRequestData);
    setvbuf( stdout, NULL, _IONBF, 0 );
    if (!pNtReadRequestData || !pNtWriteRequestData) { printf( "Exports=0\n" ); return 1; }
    if (argc > 2 && !strcmp( argv[1], "child" )) return child( argv[2] );
    printf( "Exports=1\n" );

    RtlInitUnicodeString( &us, L"\\SgLpcData" );
    InitializeObjectAttributes( &attr, &us, 0, NULL, NULL );
    if ((status = pNtCreatePort( &server_port, &attr, 0, 0x148, NULL )))
    {
        printf( "Create=0x%08lx\n", status );
        return 1;
    }
    for (i = 0; i < 2; i++)
    {
        STARTUPINFOA si = { sizeof(si) };
        PROCESS_INFORMATION pi;

        GetModuleFileNameA( NULL, exe, sizeof(exe) );
        if (i) strcpy( strrchr( exe, '\\' ) + 1, "lpcdata-probe32.exe" );
        sprintf( cmd, "\"%s\" child \\SgLpcData", exe );
        if (!CreateProcessA( exe, cmd, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi ))
        {
            printf( "Spawn%d=none\n", i );
            continue;
        }
        serve( server_port, i ? "W32" : "W64" );
        {
            DWORD code = 99;
            WaitForSingleObject( pi.hProcess, 20000 );
            GetExitCodeProcess( pi.hProcess, &code );
            printf( "%s.Child=%lu\n", i ? "W32" : "W64", code );
        }
        CloseHandle( pi.hProcess );
        CloseHandle( pi.hThread );
    }
    printf( "Done=1\n" );
    return 0;
}
