/* Monitor handles are user handles (patches/sg/2228): only the low 32 bits of
 * a 64-bit process's handle count (so a sign extension or other high bits
 * are fine), and a 32-bit process may give the 16-bit index alone or with
 * the 0xffff extension; a 64-bit process may not give the index alone, and
 * wrong middle bits are ERROR_INVALID_MONITOR_HANDLE for both. */
#include <windows.h>
#include <stdio.h>

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

static int tried;

static BOOL try_handle(HMONITOR m, const char *what, BOOL want)
{
    MONITORINFO mi = { sizeof(mi) };
    char name[128];
    BOOL ret;
    SetLastError( 0xdeadbeef );
    ret = GetMonitorInfoW( m, &mi );
    sprintf( name, "%s (%p): %s", what, m, want ? "accepted" : "ERROR_INVALID_MONITOR_HANDLE" );
    check( want ? ret : (!ret && GetLastError() == ERROR_INVALID_MONITOR_HANDLE), name );
    return TRUE;
}

static BOOL CALLBACK proc(HMONITOR full, HDC hdc, LPRECT rect, LPARAM lparam)
{
    ULONG_PTR h = (ULONG_PTR)full;
    if (tried++) return TRUE;
    try_handle( full, "the handle as given", TRUE );
    check( (h & 0xffffffff) > 0xffff, "the handle has bits above the index" );
#ifdef _WIN64
    try_handle( (HMONITOR)((h & 0xffffffff) | ((ULONG_PTR)0x1234 << 32)), "other high bits", TRUE );
    try_handle( (HMONITOR)((h & 0xffffffff) | ((ULONG_PTR)~0u << 32)), "sign extension", TRUE );
    try_handle( (HMONITOR)(h & 0xffff), "the index alone", FALSE );
    try_handle( (HMONITOR)((h & 0xffff) | ((ULONG_PTR)0x9876 << 16)), "wrong middle bits", FALSE );
    try_handle( (HMONITOR)((h & 0xffff) | ((ULONG_PTR)0x12345678 << 16)), "wrong bits above", FALSE );
#else
    try_handle( (HMONITOR)(h & 0xffff), "the index alone", TRUE );
    try_handle( (HMONITOR)((h & 0xffff) | 0xffff0000), "the index with 0xffff", TRUE );
    try_handle( (HMONITOR)((h & 0xffff) | (0x1234 << 16)), "wrong middle bits", FALSE );
#endif
    return TRUE;
}

int main(void)
{
    BOOL ret = EnumDisplayMonitors( NULL, NULL, proc, 0 );
    check( ret && tried, "enumerate" );
    printf( failures ? "RESULT: FAIL\n" : "RESULT: PASS\n" );
    return failures != 0;
}
