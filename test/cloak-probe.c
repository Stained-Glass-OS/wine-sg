/* cloak-probe: DWM cloaking (patches/sg/0067), for test/cloak-gate.sh.
 *
 *   cloak-probe blue        a blue window, another program's, to be beneath
 *   cloak-probe run DIR     a red window over it; then cloak the red one,
 *                           move it while cloaked, and uncloak it -- step by
 *                           step, each step written to
 *                           DIR\step.txt and held until DIR\ack.txt names it,
 *                           so the gate can look at the screen in between
 *   cloak-probe query HWND  DwmGetWindowAttribute(DWMWA_CLOAKED) of another
 *                           process's window
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include <windows.h>
#include <dwmapi.h>
#include <stdio.h>

static WCHAR dir[MAX_PATH];
static LONG shown_msgs, pos_msgs;
static HWND red;

static LRESULT CALLBACK proc( HWND hwnd, UINT msg, WPARAM wp, LPARAM lp )
{
    if (hwnd == red && msg == WM_SHOWWINDOW) shown_msgs++;
    if (hwnd == red && (msg == WM_WINDOWPOSCHANGING || msg == WM_WINDOWPOSCHANGED)) pos_msgs++;
    if (msg == WM_PAINT)
    {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint( hwnd, &ps );
        HBRUSH brush = CreateSolidBrush( red && hwnd == red ? RGB(255, 0, 0) : RGB(0, 0, 255) );
        FillRect( dc, &ps.rcPaint, brush );
        DeleteObject( brush );
        EndPaint( hwnd, &ps );
        return 0;
    }
    return DefWindowProcW( hwnd, msg, wp, lp );
}

static void pump( DWORD ms )
{
    DWORD end = GetTickCount() + ms;
    MSG msg;
    while ((LONG)(end - GetTickCount()) > 0)
    {
        while (PeekMessageW( &msg, NULL, 0, 0, PM_REMOVE )) DispatchMessageW( &msg );
        Sleep( 10 );
    }
}

/* publish a step and wait until the gate has looked at it */
static void step( const char *name )
{
    WCHAR path[MAX_PATH];
    char buf[64] = "";
    DWORD start = GetTickCount(), got;
    HANDLE f;

    pump( 700 );  /* let painting and the X server settle */
    swprintf( path, MAX_PATH, L"%ls\\step.txt", dir );
    f = CreateFileW( path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL );
    WriteFile( f, name, strlen( name ), &got, NULL );
    CloseHandle( f );
    swprintf( path, MAX_PATH, L"%ls\\ack.txt", dir );
    while (GetTickCount() - start < 30000)
    {
        f = CreateFileW( path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, 0, NULL );
        if (f != INVALID_HANDLE_VALUE)
        {
            memset( buf, 0, sizeof(buf) );
            ReadFile( f, buf, sizeof(buf) - 1, &got, NULL );
            CloseHandle( f );
            if (!strcmp( buf, name )) return;
        }
        pump( 50 );
    }
}

static const char *under( POINT pt )
{
    WCHAR text[16] = L"";
    HWND hwnd = WindowFromPoint( pt );

    if (hwnd == red) return "red";
    GetWindowTextW( hwnd, text, ARRAYSIZE(text) );
    return !lstrcmpW( text, L"blue" ) ? "blue" : "other";
}

/* DWMWA_CLOAKED of the red window, as another process reads it */
static void query_from_another_process( void )
{
    WCHAR cmd[MAX_PATH + 64], exe[MAX_PATH];
    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    SECURITY_ATTRIBUTES sa = { sizeof(sa), NULL, TRUE };
    HANDLE rd, wr;
    char buf[128] = "";
    DWORD got = 0;

    CreatePipe( &rd, &wr, &sa, 0 );
    SetHandleInformation( rd, HANDLE_FLAG_INHERIT, 0 );
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = si.hStdError = wr;
    GetModuleFileNameW( NULL, exe, MAX_PATH );
    swprintf( cmd, ARRAYSIZE(cmd), L"\"%ls\" query %#lx", exe, (ULONG)(ULONG_PTR)red );
    DWORD code = 0xdead, err = 0;
    if (CreateProcessW( NULL, cmd, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi ))
    {
        CloseHandle( wr );
        while (WaitForSingleObject( pi.hProcess, 50 ) == WAIT_TIMEOUT) pump( 0 );
        ReadFile( rd, buf, sizeof(buf) - 1, &got, NULL );
        GetExitCodeProcess( pi.hProcess, &code );
        CloseHandle( pi.hProcess ); CloseHandle( pi.hThread );
    }
    else { err = GetLastError(); CloseHandle( wr ); }
    CloseHandle( rd );
    if (buf[0]) printf( "%s", buf );
    else printf( "query none err=%lu exit=%#lx cmd=%ls\n", err, code, cmd );
}

static void report( const char *when )
{
    POINT pt;
    RECT rc;
    DWORD cloaked = 0xdead;
    HRESULT hr = DwmGetWindowAttribute( red, DWMWA_CLOAKED, &cloaked, sizeof(cloaked) );

    GetWindowRect( red, &rc );
    pt.x = (rc.left + rc.right) / 2; pt.y = (rc.top + rc.bottom) / 2;
    printf( "%s visible=%d style_visible=%d cloaked=%#lx hr=%#lx from_point=%s center=%ld,%ld showmsgs=%ld posmsgs=%ld\n",
            when, IsWindowVisible( red ), !!(GetWindowLongW( red, GWL_STYLE ) & WS_VISIBLE), cloaked, hr,
            under( pt ),
            pt.x, pt.y, shown_msgs, pos_msgs );
    fflush( stdout );
}

int main( int argc, char **argv )
{
    WNDCLASSW wc = { 0 };
    BOOL on;

    if (argc == 3 && !strcmp( argv[1], "query" ))
    {
        DWORD cloaked = 0xdead;
        HRESULT hr = DwmGetWindowAttribute( (HWND)(ULONG_PTR)strtoul( argv[2], NULL, 0 ), DWMWA_CLOAKED, &cloaked, sizeof(cloaked) );
        printf( "query cloaked=%#lx hr=%#lx\n", cloaked, hr );
        return 0;
    }
    wc.lpfnWndProc = proc;
    wc.hCursor = LoadCursorW( NULL, (const WCHAR *)IDC_ARROW );
    wc.lpszClassName = L"SgCloakProbe";
    RegisterClassW( &wc );
    if (argc == 2 && !strcmp( argv[1], "blue" ))
    {
        CreateWindowExW( 0, wc.lpszClassName, L"blue", WS_POPUP | WS_VISIBLE, 50, 50, 500, 400, NULL, NULL, NULL, NULL );
        pump( 120000 );
        return 0;
    }
    if (argc != 3 || strcmp( argv[1], "run" )) return 2;
    MultiByteToWideChar( CP_ACP, 0, argv[2], -1, dir, MAX_PATH );

    red = CreateWindowExW( 0, wc.lpszClassName, L"red", WS_POPUP | WS_VISIBLE, 150, 120, 300, 250, NULL, NULL, NULL, NULL );
    SetForegroundWindow( red );
    printf( "hwnd red=%#lx\n", (ULONG)(ULONG_PTR)red );
    report( "shown" );
    step( "shown" );

    shown_msgs = pos_msgs = 0;
    on = TRUE;
    printf( "set_cloak hr=%#lx\n", DwmSetWindowAttribute( red, DWMWA_CLOAK, &on, sizeof(on) ) );
    pump( 300 );
    report( "cloaked" );
    query_from_another_process();
    step( "cloaked" );

    /* the program moves its cloaked window: it moves, and stays unseen */
    SetWindowPos( red, NULL, 100, 100, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE );
    pump( 300 );
    report( "moved" );
    step( "moved" );

    shown_msgs = pos_msgs = 0;
    on = FALSE;
    printf( "set_uncloak hr=%#lx\n", DwmSetWindowAttribute( red, DWMWA_CLOAK, &on, sizeof(on) ) );
    pump( 300 );
    report( "uncloaked" );
    step( "uncloaked" );
    return 0;
}
