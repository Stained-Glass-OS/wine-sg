/* ShutdownBlockReasonCreate/Destroy/Query (patches/sg/2202): Create and
 * Destroy were "stub" ERROR_CALL_NOT_IMPLEMENTED and Query was not even
 * exported -- a window never had a reason.  The reason lives server-side
 * (window properties), so it is readable from ANOTHER process -- the logoff
 * UI is one -- and it goes away with the window.
 *
 *   probe.exe [OTHER.exe ...]   run the checks, then start each OTHER.exe (and
 *                               this one) as "--child HWND" to query the
 *                               window cross-process, 32-bit and 64-bit.
 *   probe.exe --child HWND      query/deny from a second process. */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

/* a long reason with characters that exercise both halves of a packed value */
static void make_long( WCHAR *s, unsigned int n )
{
    unsigned int i;
    for (i = 0; i < n; i++) s[i] = (i % 7 == 0) ? 0xfffe : (i % 5 == 0) ? 0x8001 : 'a' + i % 26;
    s[n] = 0;
}

static int child( HWND hwnd )
{
    WCHAR want[256], buf[256];
    DWORD size = ARRAYSIZE(buf);
    BOOL ret;

    make_long( want, 249 );
    ret = ShutdownBlockReasonQuery( hwnd, buf, &size );
    check( ret && size == 250 && !wcscmp( buf, want ), "[other process] the long reason reads back from a window it does not own" );

    ret = ShutdownBlockReasonCreate( hwnd, L"hijack" );
    check( !ret && GetLastError() == ERROR_ACCESS_DENIED, "[other process] Create on another process's window is ERROR_ACCESS_DENIED" );
    ret = ShutdownBlockReasonDestroy( hwnd );
    check( !ret && GetLastError() == ERROR_ACCESS_DENIED, "[other process] Destroy on another process's window is ERROR_ACCESS_DENIED" );

    size = ARRAYSIZE(buf);
    ret = ShutdownBlockReasonQuery( hwnd, buf, &size );
    check( ret && !wcscmp( buf, want ), "[other process] the reason is untouched afterwards" );
    return failures ? 1 : 0;
}

static void run_child( const char *exe, HWND hwnd )
{
    char cmd[MAX_PATH + 64];
    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    DWORD code = 99;

    snprintf( cmd, sizeof(cmd), "\"%s\" --child %lx", exe, (unsigned long)(ULONG_PTR)hwnd );
    printf( "-- second process: %s\n", exe );
    fflush( stdout );
    if (!CreateProcessA( NULL, cmd, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi ))
    {
        check( 0, "second process started" );
        return;
    }
    WaitForSingleObject( pi.hProcess, 60000 );
    GetExitCodeProcess( pi.hProcess, &code );
    CloseHandle( pi.hProcess );
    CloseHandle( pi.hThread );
    check( code == 0, "second process saw every case pass" );
}

int main(int argc, char **argv)
{
    WCHAR buf[300], want[300];
    HWND hwnd, hwnd2;
    DWORD size;
    BOOL ret;
    int i;

    if (argc >= 3 && !strcmp( argv[1], "--child" ))
    {
        int r = child( (HWND)(ULONG_PTR)strtoul( argv[2], NULL, 16 ));
        printf( r ? "CHILD: FAIL\n" : "CHILD: PASS\n" );
        return r;
    }

    hwnd = CreateWindowExW( 0, L"Button", L"probe", WS_OVERLAPPED, 0, 0, 10, 10, NULL, NULL, NULL, NULL );
    check( hwnd != NULL, "test window created" );

    size = ARRAYSIZE(buf);
    ret = ShutdownBlockReasonQuery( hwnd, buf, &size );
    check( ret && buf[0] == 0, "a window with no reason queries as an empty string" );

    ret = ShutdownBlockReasonCreate( hwnd, L"finishing an important save" );
    check( ret, "ShutdownBlockReasonCreate succeeds" );

    size = ARRAYSIZE(buf);
    ret = ShutdownBlockReasonQuery( hwnd, buf, &size );
    check( ret && !wcscmp( buf, L"finishing an important save" ), "the reason reads back" );
    check( size == wcslen( L"finishing an important save" ) + 1, "the reported size matches the string" );

    /* a small buffer fails with the size the caller needs */
    size = 2;
    ret = ShutdownBlockReasonQuery( hwnd, buf, &size );
    check( !ret && GetLastError() == ERROR_INSUFFICIENT_BUFFER, "a too-small buffer fails with ERROR_INSUFFICIENT_BUFFER" );
    check( size == wcslen( L"finishing an important save" ) + 1, "...and still reports the size needed" );

    /* setting a new reason replaces the old one, not leaks beside it */
    ret = ShutdownBlockReasonCreate( hwnd, L"second reason" );
    check( ret, "a second ShutdownBlockReasonCreate succeeds" );
    size = ARRAYSIZE(buf);
    ShutdownBlockReasonQuery( hwnd, buf, &size );
    check( !wcscmp( buf, L"second reason" ) && size == 14, "the replaced reason reads back, not the first one" );

    /* odd and even lengths, a length-0 reason, and the limits */
    ret = ShutdownBlockReasonCreate( hwnd, L"" );
    size = ARRAYSIZE(buf);
    check( ret && ShutdownBlockReasonQuery( hwnd, buf, &size ) && buf[0] == 0 && size == 1, "an empty reason is accepted" );
    ret = ShutdownBlockReasonCreate( hwnd, L"x" );
    size = ARRAYSIZE(buf);
    check( ret && ShutdownBlockReasonQuery( hwnd, buf, &size ) && !wcscmp( buf, L"x" ) && size == 2, "a one-character reason" );
    ret = ShutdownBlockReasonCreate( hwnd, NULL );
    check( !ret && GetLastError() == ERROR_INVALID_PARAMETER, "a NULL reason is ERROR_INVALID_PARAMETER" );
    make_long( want, 256 );
    ret = ShutdownBlockReasonCreate( hwnd, want );
    check( !ret && GetLastError() == ERROR_INVALID_PARAMETER, "a reason of 256 characters (too long) is ERROR_INVALID_PARAMETER" );
    size = ARRAYSIZE(buf);
    ShutdownBlockReasonQuery( hwnd, buf, &size );
    check( !wcscmp( buf, L"x" ), "...and the previous reason is left as it was" );

    /* the long reason, read back by this process and by others */
    make_long( want, 249 );
    ret = ShutdownBlockReasonCreate( hwnd, want );
    size = ARRAYSIZE(buf);
    check( ret && ShutdownBlockReasonQuery( hwnd, buf, &size ) && size == 250 && !wcscmp( buf, want ), "a 249-character reason reads back intact" );
    for (i = 1; i < argc; i++) run_child( argv[i], hwnd );
    run_child( argv[0], hwnd );
    size = ARRAYSIZE(buf);
    check( ShutdownBlockReasonQuery( hwnd, buf, &size ) && !wcscmp( buf, want ), "after the other processes' attempts the reason is intact" );

    /* shorter replaces longer: nothing of the longer one is left behind */
    ShutdownBlockReasonCreate( hwnd, L"ab" );
    size = ARRAYSIZE(buf);
    check( ShutdownBlockReasonQuery( hwnd, buf, &size ) && !wcscmp( buf, L"ab" ) && size == 3, "a shorter reason replaces a longer one" );

    ret = ShutdownBlockReasonDestroy( hwnd );
    check( ret, "ShutdownBlockReasonDestroy succeeds" );

    size = ARRAYSIZE(buf);
    ShutdownBlockReasonQuery( hwnd, buf, &size );
    check( buf[0] == 0, "after Destroy the window queries as having no reason again" );

    /* an invalid window handle fails, not crashes */
    ret = ShutdownBlockReasonCreate( (HWND)0x7fffffff, L"x" );
    check( !ret && GetLastError() == ERROR_INVALID_WINDOW_HANDLE, "Create on a bogus window fails cleanly" );
    size = ARRAYSIZE(buf);
    ret = ShutdownBlockReasonQuery( (HWND)0x7fffffff, buf, &size );
    check( !ret && GetLastError() == ERROR_INVALID_WINDOW_HANDLE, "Query on a bogus window fails cleanly" );

    /* the reason goes with the window: a destroyed window is gone, and a new
     * window (which may reuse the handle) starts with none */
    ShutdownBlockReasonCreate( hwnd, want );
    DestroyWindow( hwnd );
    size = ARRAYSIZE(buf);
    ret = ShutdownBlockReasonQuery( hwnd, buf, &size );
    check( !ret && GetLastError() == ERROR_INVALID_WINDOW_HANDLE, "Query on a destroyed window fails" );
    for (i = 0; i < 300; i++)
    {
        hwnd2 = CreateWindowExW( 0, L"Button", L"probe", WS_OVERLAPPED, 0, 0, 10, 10, NULL, NULL, NULL, NULL );
        size = ARRAYSIZE(buf);
        buf[0] = 1;
        if (!ShutdownBlockReasonQuery( hwnd2, buf, &size ) || buf[0] != 0 || size != 1) break;
        if (!ShutdownBlockReasonCreate( hwnd2, want )) break;
        DestroyWindow( hwnd2 );
    }
    check( i == 300, "300 windows created, given a long reason and destroyed: each starts with none" );

    printf( failures ? "RESULT: FAIL\n" : "RESULT: PASS\n" );
    return failures ? 1 : 0;
}
