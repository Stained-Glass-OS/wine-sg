/* lpcview-probe.c: LPC ports' shared sections (wine-sg 7810) for test/lpcview-gate.sh.
 *   lpcview-probe.exe              the server: starts the clients (64-bit, then 32-bit) in other processes
 *   lpcview-probe.exe child PORT   a client: connects with a section of its own
 */
#include <windows.h>
#include <winternl.h>
#include <stdio.h>
#include <string.h>

#define LPC_CONNECTION_REQUEST  10

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

/* the caller's own layout (the 32-bit one is converted by WoW64) */
typedef struct { ULONG Length; HANDLE SectionHandle; ULONG SectionOffset; ULONG ViewSize; void *ViewBase;
                 void *TargetViewBase; } LPC_SECTION_WRITE;
typedef struct { ULONG Length; ULONG ViewSize; void *ViewBase; } LPC_SECTION_READ;

typedef NTSTATUS (WINAPI *fnCreatePort)(HANDLE *, OBJECT_ATTRIBUTES *, ULONG, ULONG, ULONG *);
typedef NTSTATUS (WINAPI *fnConnectPort)(HANDLE *, UNICODE_STRING *, SECURITY_QUALITY_OF_SERVICE *,
                                         LPC_SECTION_WRITE *, LPC_SECTION_READ *, ULONG *, void *, ULONG *);
typedef NTSTATUS (WINAPI *fnAcceptConnectPort)(HANDLE *, ULONG, void *, BOOLEAN, LPC_SECTION_WRITE *,
                                               LPC_SECTION_READ *);
typedef NTSTATUS (WINAPI *fnHandle)(HANDLE);
typedef NTSTATUS (WINAPI *fnReplyWaitReceiveEx)(HANDLE, void **, void *, void *, LARGE_INTEGER *);

static fnCreatePort pNtCreatePort;
static fnConnectPort pNtConnectPort;
static fnAcceptConnectPort pNtAcceptConnectPort;
static fnHandle pNtCompleteConnectPort;
static fnReplyWaitReceiveEx pNtReplyWaitReceivePortEx;

#define VIEW 0x2000

static int child( const char *name )
{
    SECURITY_QUALITY_OF_SERVICE qos = { sizeof(qos), SecurityImpersonation, SECURITY_STATIC_TRACKING, TRUE };
    LPC_SECTION_WRITE write;
    LPC_SECTION_READ read;
    WCHAR wname[64];
    UNICODE_STRING us;
    char info[100];
    ULONG info_len = sizeof(info);
    unsigned long long remote = 0;
    HANDLE h = 0, section;
    NTSTATUS status;
    char *view;

    MultiByteToWideChar( CP_ACP, 0, name, -1, wname, ARRAYSIZE(wname) );
    RtlInitUnicodeString( &us, wname );
    section = CreateFileMappingA( INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, 0x20000, NULL );
    if (!section) return 10;
    {
        /* filled before connecting: the server finds it in its view of our range */
        char *pre = MapViewOfFile( section, FILE_MAP_WRITE, 0, 0x10000, VIEW );
        if (!pre) return 11;
        strcpy( pre, "client-view" );
        UnmapViewOfFile( pre );
    }
    memset( &write, 0, sizeof(write) );
    write.Length = sizeof(write);
    write.SectionHandle = section;
    write.SectionOffset = 0x10000;
    write.ViewSize = VIEW;
    memset( &read, 0, sizeof(read) );
    read.Length = sizeof(read);
    memset( info, 0, sizeof(info) );
    strcpy( info, "hello" );
    status = pNtConnectPort( &h, &us, &qos, &write, &read, NULL, info, &info_len );
    if (status) return 1;
    view = write.ViewBase;
    if (!view || write.ViewSize < VIEW) return 5;
    /* the server wrote into our view before it let us go, and where it mapped it */
    sscanf( view + 200, "%llx", &remote );
    /* (a 32-bit client holds the low half of a 64-bit server's address, its structure being 32-bit) */
    if (!remote || (ULONG_PTR)write.TargetViewBase != (ULONG_PTR)remote) return 2;
    if (strcmp( view + 100, "srv-in-client-view" )) return 3;
    if (!read.ViewBase || read.ViewSize < VIEW || strcmp( read.ViewBase, "server-wrote" )) return 4;
    return 0;
}

static void serve( HANDLE server_port, HANDLE server_section, const char *tag )
{
    LPC_SECTION_WRITE write;
    LPC_SECTION_READ read;
    MSG64 msg;
    void *context;
    LARGE_INTEGER timeout;
    HANDLE h = 0;
    NTSTATUS status;

    do
    {
        memset( &msg, 0, sizeof(msg) );
        timeout.QuadPart = -100000000;   /* 10 s */
        status = pNtReplyWaitReceivePortEx( server_port, &context, NULL, &msg, &timeout );
    } while (!status && (msg.MessageType == 5 || msg.MessageType == 6));
    if (status || msg.MessageType != LPC_CONNECTION_REQUEST)
    {
        printf( "%s.Connect=0x%08lx %d\n", tag, status, msg.MessageType );
        return;
    }
    memset( &write, 0, sizeof(write) );
    write.Length = sizeof(write);
    write.SectionHandle = server_section;
    write.SectionOffset = 0;
    write.ViewSize = VIEW;
    memset( &read, 0, sizeof(read) );
    read.Length = sizeof(read);
    status = pNtAcceptConnectPort( &h, 1, &msg, TRUE, &write, &read );
    printf( "%s.Accept=0x%08lx %d %d\n", tag, status, write.ViewBase != NULL && write.ViewSize >= VIEW,
            read.ViewBase != NULL && read.ViewSize >= VIEW );
    if (status || !read.ViewBase || !write.ViewBase) return;
    /* the client's view, as it filled it before connecting (its section's offset 0x10000) */
    printf( "%s.ClientView=%d\n", tag, !strcmp( (char *)read.ViewBase, "client-view" ) );
    strcpy( (char *)read.ViewBase + 100, "srv-in-client-view" );
    strcpy( (char *)write.ViewBase, "server-wrote" );
    /* and, in the view itself, where this process mapped it */
    sprintf( (char *)read.ViewBase + 200, "%llx", (unsigned long long)(ULONG_PTR)read.ViewBase );
    printf( "%s.Complete=0x%08lx\n", tag, pNtCompleteConnectPort( h ));
}

int main( int argc, char **argv )
{
    HMODULE ntdll = GetModuleHandleA( "ntdll.dll" );
    OBJECT_ATTRIBUTES attr;
    UNICODE_STRING us;
    HANDLE server_port, server_section;
    char exe[MAX_PATH], cmd[MAX_PATH + 64];
    NTSTATUS status;
    int i;

#define LOAD(name) p##name = (void *)GetProcAddress( ntdll, #name )
    LOAD(NtCreatePort); LOAD(NtConnectPort); LOAD(NtAcceptConnectPort); LOAD(NtCompleteConnectPort);
    LOAD(NtReplyWaitReceivePortEx);
    setvbuf( stdout, NULL, _IONBF, 0 );
    if (argc > 2 && !strcmp( argv[1], "child" )) return child( argv[2] );

    RtlInitUnicodeString( &us, L"\\SgLpcView" );
    InitializeObjectAttributes( &attr, &us, 0, NULL, NULL );
    if ((status = pNtCreatePort( &server_port, &attr, 100, 0x148, NULL )))
    {
        printf( "Create=0x%08lx\n", status );
        return 1;
    }
    for (i = 0; i < 2; i++)
    {
        STARTUPINFOA si = { sizeof(si) };
        PROCESS_INFORMATION pi;
        DWORD code = 99;

        server_section = CreateFileMappingA( INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, 0x10000, NULL );
        GetModuleFileNameA( NULL, exe, sizeof(exe) );
        if (i) strcpy( strrchr( exe, '\\' ) + 1, "lpcview-probe32.exe" );
        sprintf( cmd, "\"%s\" child \\SgLpcView", exe );
        if (!CreateProcessA( exe, cmd, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi ))
        {
            printf( "Spawn%d=none\n", i );
            continue;
        }
        serve( server_port, server_section, i ? "W32" : "W64" );
        WaitForSingleObject( pi.hProcess, 20000 );
        GetExitCodeProcess( pi.hProcess, &code );
        printf( "%s.Child=%lu\n", i ? "W32" : "W64", code );
        CloseHandle( pi.hProcess );
        CloseHandle( pi.hThread );
    }
    printf( "Done=1\n" );
    return 0;
}
