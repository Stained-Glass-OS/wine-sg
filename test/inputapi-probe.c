/* GetPointerType and GetMouseMovePointsEx (patches/sg/2231): an unknown
 * pointer id is ERROR_INVALID_PARAMETER (the mouse is pointer 1); the cursor
 * history of a desktop that does not receive input is
 * ERROR_ACCESS_DENIED. */
#include <windows.h>
#include <stdio.h>

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

int main(void)
{
    BOOL (WINAPI *pGetPointerType)(UINT32, POINTER_INPUT_TYPE *);
    HMODULE u = GetModuleHandleA( "user32.dll" );
    POINTER_INPUT_TYPE type = 0;
    MOUSEMOVEPOINT in = { 0 }, out[4];
    HWINSTA ws0, ws1;
    HDESK d0, d1;
    POINT pt;
    int n;

    pGetPointerType = (void *)GetProcAddress( u, "GetPointerType" );
    if (pGetPointerType)
    {
        SetLastError( 0 );
        check( !pGetPointerType( 0xdead, &type ) && GetLastError() == ERROR_INVALID_PARAMETER, "GetPointerType(0xdead): ERROR_INVALID_PARAMETER" );
        SetLastError( 0 );
        check( !pGetPointerType( 0, &type ) && GetLastError() == ERROR_INVALID_PARAMETER, "GetPointerType(0): ERROR_INVALID_PARAMETER" );
        type = 0;
        check( pGetPointerType( 1, &type ) && type == PT_MOUSE, "GetPointerType(1) is the mouse" );
    }

    SetCursorPos( 300, 300 );
    SetCursorPos( 301, 301 );
    GetCursorPos( &pt );
    in.x = pt.x; in.y = pt.y;
    n = GetMouseMovePointsEx( sizeof(in), &in, out, 4, GMMP_USE_DISPLAY_POINTS );
    check( n >= 1, "cursor history of the input desktop works" );

    ws0 = GetProcessWindowStation();
    d0 = GetThreadDesktop( GetCurrentThreadId() );
    ws1 = CreateWindowStationA( "sg_probe_station", 0, GENERIC_ALL, NULL );
    if (!ws1) { printf( "SKIP  cannot create a window station\n" ); goto done; }
    SetProcessWindowStation( ws1 );
    d1 = CreateDesktopA( "sg_probe_desktop", NULL, NULL, 0, GENERIC_ALL, NULL );
    SetThreadDesktop( d1 );
    SetLastError( 0 );
    n = GetMouseMovePointsEx( sizeof(in), &in, out, 4, GMMP_USE_DISPLAY_POINTS );
    check( n == -1 && GetLastError() == ERROR_ACCESS_DENIED, "cursor history on a desktop without input: ERROR_ACCESS_DENIED" );
    SetProcessWindowStation( ws0 );
    SetThreadDesktop( d0 );
    CloseDesktop( d1 );
    CloseWindowStation( ws1 );
    n = GetMouseMovePointsEx( sizeof(in), &in, out, 4, GMMP_USE_DISPLAY_POINTS );
    check( n >= 1, "and works again on the input desktop" );
done:
    printf( failures ? "RESULT: FAIL\n" : "RESULT: PASS\n" );
    return failures != 0;
}
