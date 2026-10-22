/* procdpi-probe: the DPI awareness of another process (Stained Glass OS wine-sg 6024):
 * GetDpiAwarenessContextForProcess, GetProcessDpiAwareness and GetSystemDpiForProcess of a child
 * that made itself per-monitor aware and of one that did not.
 *   procdpi-probe.exe          the test
 *   procdpi-probe.exe aware    a child: per-monitor aware, waits
 *   procdpi-probe.exe unaware  a child: sets nothing, waits */
#include <windows.h>
#include <stdio.h>

typedef DPI_AWARENESS_CONTEXT (WINAPI *ctx_for_process_fn)(HANDLE);
typedef DPI_AWARENESS (WINAPI *awareness_fn)(DPI_AWARENESS_CONTEXT);
typedef UINT (WINAPI *sysdpi_fn)(HANDLE);
typedef HRESULT (WINAPI *proc_awareness_fn)(HANDLE, int *);
typedef BOOL (WINAPI *set_ctx_fn)(DPI_AWARENESS_CONTEXT);

static int failures;

static void check( int ok, const char *what, long got )
{
    if (ok) printf( "ok    %s\n", what );
    else { printf( "FAIL  %s (got %#lx)\n", what, got ); failures++; }
}

static HANDLE start_child( const char *arg, HANDLE ready )
{
    char cmd[MAX_PATH + 64], self[MAX_PATH];
    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION pi;

    GetModuleFileNameA( NULL, self, MAX_PATH );
    sprintf( cmd, "\"%s\" %s", self, arg );
    if (!CreateProcessA( NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi )) return NULL;
    CloseHandle( pi.hThread );
    WaitForSingleObject( ready, 30000 );
    return pi.hProcess;
}

int main( int argc, char **argv )
{
    HMODULE user32 = GetModuleHandleA( "user32.dll" ), shcore = LoadLibraryA( "shcore.dll" );
    ctx_for_process_fn ctx_for_process = (ctx_for_process_fn)GetProcAddress( user32, "GetDpiAwarenessContextForProcess" );
    awareness_fn awareness = (awareness_fn)GetProcAddress( user32, "GetAwarenessFromDpiAwarenessContext" );
    sysdpi_fn sysdpi = (sysdpi_fn)GetProcAddress( user32, "GetSystemDpiForProcess" );
    proc_awareness_fn proc_awareness = (proc_awareness_fn)GetProcAddress( shcore, "GetProcessDpiAwareness" );
    set_ctx_fn set_ctx = (set_ctx_fn)GetProcAddress( user32, "SetProcessDpiAwarenessContext" );
    HANDLE ready = CreateEventA( NULL, TRUE, FALSE, "sg_procdpi_ready" );
    HANDLE done = CreateEventA( NULL, TRUE, FALSE, "sg_procdpi_done" );
    HANDLE aware, unaware;
    int value = -1;
    UINT dpi;
    HDC hdc;

    if (argc > 1)
    {
        if (!strcmp( argv[1], "aware" )) set_ctx( DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 );
        SetEvent( ready );
        WaitForSingleObject( done, 60000 );
        return 0;
    }
    if (!ctx_for_process || !awareness || !sysdpi || !proc_awareness || !set_ctx)
    {
        printf( "FAIL  a function is missing\nRESULT: FAIL\n" );
        return 1;
    }
    hdc = GetDC( 0 );
    ReleaseDC( 0, hdc );

    aware = start_child( "aware", ready );
    ResetEvent( ready );
    unaware = start_child( "unaware", ready );
    check( aware && unaware, "two children", 0 );
    if (!aware || !unaware) { printf( "RESULT: FAIL\n" ); return 1; }

    check( awareness( ctx_for_process( aware ) ) == DPI_AWARENESS_PER_MONITOR_AWARE,
           "the aware child's context: per-monitor aware", awareness( ctx_for_process( aware ) ) );
    check( awareness( ctx_for_process( unaware ) ) == DPI_AWARENESS_UNAWARE,
           "the other's: unaware", awareness( ctx_for_process( unaware ) ) );
    check( proc_awareness( aware, &value ) == S_OK && value == 2 /* PROCESS_PER_MONITOR_DPI_AWARE */,
           "GetProcessDpiAwareness of the aware child", value );
    value = -1;
    check( proc_awareness( unaware, &value ) == S_OK && value == 0, "and of the other", value );
    dpi = sysdpi( unaware );
    check( dpi == 96, "GetSystemDpiForProcess of an unaware process: 96", dpi );
    dpi = sysdpi( aware );
    check( dpi == sysdpi( GetCurrentProcess() ) && dpi == 144, "of an aware one: the system DPI (144 here)", dpi );

    SetEvent( done );
    WaitForSingleObject( aware, 10000 );
    WaitForSingleObject( unaware, 10000 );
    printf( "RESULT: %s\n", failures ? "FAIL" : "PASS" );
    return failures != 0;
}
