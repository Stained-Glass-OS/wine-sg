/* dumpread-probe: dbgeng dump targets (Stained Glass OS wine-sg 6073), for test/dumpread-gate.sh.
 *   dumpread-probe crash   the child: crashes in crash_here, its vectored handler writes C:\crash.dmp and exits
 *   dumpread-probe         the reader: runs the child, then opens the dump with dbgeng
 * Prints KEY=VALUE lines. */
#define COBJMACROS
#define INITGUID
#include <windows.h>
#include <dbghelp.h>
#include <dbgeng.h>
#include <stdio.h>

__declspec(dllexport) volatile DWORD g_marker;
static DWORD crash_tid;

__declspec(dllexport) __attribute__((noinline)) void crash_here( int *p )
{
    g_marker = 0x5ae1dead;
    *p = 42;
}

static LONG CALLBACK handler( EXCEPTION_POINTERS *ptrs )
{
    MINIDUMP_EXCEPTION_INFORMATION info = { GetCurrentThreadId(), ptrs, FALSE };
    HANDLE file = CreateFileA( "C:\\crash.dmp", GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL );
    FILE *f;

    MiniDumpWriteDump( GetCurrentProcess(), GetCurrentProcessId(), file, MiniDumpWithDataSegs, &info, NULL, NULL );
    CloseHandle( file );
    if ((f = fopen( "C:\\crash-info.txt", "w" )))
    {
        fprintf( f, "%lu %p\n", GetCurrentThreadId(), ptrs->ExceptionRecord->ExceptionAddress );
        fclose( f );
    }
    ExitProcess( 3 );
}

int main( int argc, char **argv )
{
    IDebugClient *client = NULL;
    IDebugClient4 *client4 = NULL;
    IDebugControl *control = NULL;
    IDebugSymbols *symbols = NULL;
    IDebugDataSpaces *data = NULL;
    IDebugSystemObjects *objects = NULL;
    IDebugAdvanced *advanced = NULL;
    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    char cmd[MAX_PATH + 16], name[MAX_PATH];
    ULONG type, pid, tid, used, loaded = 0, unloaded = 0, threads = 0, sysid = 0, cls, qual, count, frames = 0;
    struct { EXCEPTION_RECORD64 record; ULONG first_chance; } exc;
    DEBUG_MODULE_PARAMETERS params;
    DEBUG_STACK_FRAME stack[16];
    ULONG64 base = 0, disp = 0;
    DWORD child_tid = 0, marker = 0;
    void *child_addr = NULL;
    CONTEXT ctx;
    HRESULT hr;
    FILE *f;
    BOOL found = FALSE;
    ULONG i;

    if (argc > 1 && !strcmp( argv[1], "crash" ))
    {
        AddVectoredExceptionHandler( 1, handler );
        crash_here( (int *)0x10 );
        return 1;
    }
    setvbuf( stdout, NULL, _IONBF, 0 );
    snprintf( cmd, sizeof(cmd), "\"%s\" crash", argv[0] );
    DeleteFileA( "C:\\crash.dmp" );
    if (!CreateProcessA( NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi )) return 1;
    WaitForSingleObject( pi.hProcess, 60000 );
    if ((f = fopen( "C:\\crash-info.txt", "r" ))) { fscanf( f, "%lu %p", &child_tid, &child_addr ); fclose( f ); }
    printf( "Child=%d\n", GetFileAttributesA( "C:\\crash.dmp" ) != INVALID_FILE_ATTRIBUTES && child_tid );

    DebugCreate( &IID_IDebugClient, (void **)&client );
    client->lpVtbl->QueryInterface( client, &IID_IDebugControl, (void **)&control );
    client->lpVtbl->QueryInterface( client, &IID_IDebugSymbols, (void **)&symbols );
    client->lpVtbl->QueryInterface( client, &IID_IDebugDataSpaces, (void **)&data );
    client->lpVtbl->QueryInterface( client, &IID_IDebugSystemObjects, (void **)&objects );
    client->lpVtbl->QueryInterface( client, &IID_IDebugAdvanced, (void **)&advanced );
    client->lpVtbl->QueryInterface( client, &IID_IDebugClient4, (void **)&client4 );
    if (!client4) return 1;
    if (!control || !symbols || !data || !objects || !advanced) return 1;

    printf( "OpenMissing=%#lx\n", client->lpVtbl->OpenDumpFile( client, "C:\\nothere.dmp" ) );
    printf( "OpenNotDump=%#lx\n", client->lpVtbl->OpenDumpFile( client, argv[0] ) );
    hr = client->lpVtbl->OpenDumpFile( client, "C:\\crash.dmp" );
    printf( "Open=%#lx\n", hr );
    hr = control->lpVtbl->WaitForEvent( control, 0, INFINITE );
    printf( "Wait=%#lx\n", hr );
    hr = control->lpVtbl->GetLastEventInformation( control, &type, &pid, &tid, &exc, sizeof(exc), &used, NULL, 0, NULL );
    printf( "Event=%#lx %lu %#lx %d %d\n", hr, type, exc.record.ExceptionCode,
            exc.record.ExceptionAddress == (ULONG_PTR)child_addr, exc.record.ExceptionInformation[1] == 0x10 );
    control->lpVtbl->GetDebuggeeType( control, &cls, &qual );
    printf( "Type=%lu %lu\n", cls, qual );
    objects->lpVtbl->GetNumberThreads( objects, &threads );
    objects->lpVtbl->GetCurrentThreadSystemId( objects, &sysid );
    printf( "Threads=%d %d\n", threads >= 1, sysid == child_tid );

    symbols->lpVtbl->GetNumberModules( symbols, &loaded, &unloaded );
    for (i = 0; i < loaded; i++)
    {
        name[0] = 0;
        symbols->lpVtbl->GetModuleNames( symbols, i, 0, name, sizeof(name), NULL, NULL, 0, NULL, NULL, 0, NULL );
        if (strstr( name, "dumpread-probe" ))
        {
            symbols->lpVtbl->GetModuleParameters( symbols, 1, NULL, i, &params );
            found = params.Base == (ULONG_PTR)GetModuleHandleA( NULL ) && params.Size > 0;
        }
    }
    printf( "Modules=%d %d\n", loaded >= 3, found );

    /* the data the child wrote before it crashed, at the same address (the same image, the same base) */
    hr = data->lpVtbl->ReadVirtual( data, (ULONG_PTR)&g_marker, &marker, sizeof(marker), &used );
    printf( "Memory=%#lx %#lx\n", hr, marker );
    hr = data->lpVtbl->ReadVirtual( data, 0x10, &marker, sizeof(marker), &used );
    printf( "MemoryNone=%d\n", FAILED(hr) );

    memset( &ctx, 0, sizeof(ctx) );
    hr = advanced->lpVtbl->GetThreadContext( advanced, &ctx, sizeof(ctx) );
#ifdef _WIN64
    printf( "Context=%#lx %d\n", hr, ctx.Rip == (ULONG_PTR)child_addr );
#else
    printf( "Context=%#lx %d\n", hr, ctx.Eip == (ULONG_PTR)child_addr );
#endif
    printf( "SetContext=%#lx\n", advanced->lpVtbl->SetThreadContext( advanced, &ctx, sizeof(ctx) ) );

    hr = control->lpVtbl->GetStackTrace( control, 0, 0, 0, stack, ARRAYSIZE(stack), &frames );
    name[0] = 0;
    if (frames) symbols->lpVtbl->GetNameByOffset( symbols, stack[0].InstructionOffset, name, sizeof(name), NULL, &disp );
    printf( "Stack=%#lx %d %d %d\n", hr, frames >= 2, frames && stack[0].InstructionOffset == (ULONG_PTR)child_addr,
            strstr( name, "crash_here" ) != NULL );

    client4->lpVtbl->GetNumberDumpFiles( client4, &count );
    name[0] = 0;
    hr = client4->lpVtbl->GetDumpFile( client4, 0, name, sizeof(name), NULL, &base, &type );
    printf( "DumpFile=%lu %#lx %d %#lx\n", count, hr, !_stricmp( name, "C:\\crash.dmp" ), type );
    printf( "WaitAgain=%#lx\n", control->lpVtbl->WaitForEvent( control, 0, 1000 ) );
    printf( "End=%#lx\n", client->lpVtbl->EndSession( client, DEBUG_END_PASSIVE ) );
    count = 5;
    client4->lpVtbl->GetNumberDumpFiles( client4, &count );
    printf( "AfterEnd=%lu\n", count );
    printf( "done=1\n" );
    return 0;
}
