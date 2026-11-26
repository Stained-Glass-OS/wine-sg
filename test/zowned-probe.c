/* zowned-probe: an owner window and the popup it owns, over it. Prints the
 * owner's X window (winex11's __wine_x11_whole_window), then the Z order of
 * the two each time the gate asks (a line on stdin), as Windows keeps it: an
 * owned window is always above its owner. */
#include <windows.h>
#include <stdio.h>

static HWND owner, popup;

static LRESULT CALLBACK proc( HWND hwnd, UINT msg, WPARAM wp, LPARAM lp )
{
    if (msg == WM_PAINT)
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint( hwnd, &ps );
        FillRect( hdc, &ps.rcPaint, GetStockObject( hwnd == popup ? GRAY_BRUSH : WHITE_BRUSH ) );
        EndPaint( hwnd, &ps );
        return 0;
    }
    return DefWindowProcW( hwnd, msg, wp, lp );
}

static const char *order(void)
{
    HWND h;
    for (h = GetTopWindow( NULL ); h; h = GetWindow( h, GW_HWNDNEXT ))
    {
        if (h == popup) return "owned-above";
        if (h == owner) return "owner-above";
    }
    return "none";
}

static void pump( DWORD ms )
{
    DWORD end = GetTickCount() + ms;
    MSG msg;
    while ((int)(end - GetTickCount()) > 0)
    {
        while (PeekMessageW( &msg, 0, 0, 0, PM_REMOVE )) { TranslateMessage( &msg ); DispatchMessageW( &msg ); }
        MsgWaitForMultipleObjects( 0, NULL, FALSE, 20, QS_ALLINPUT );
    }
}

int wmain( int argc, WCHAR **argv )
{
    WNDCLASSW wc = { 0 };
    FILE *log = _wfopen( argv[1], L"w" );
    WCHAR go[MAX_PATH];
    int i;

    wc.lpfnWndProc = proc;
    wc.hInstance = GetModuleHandleW( NULL );
    wc.hCursor = LoadCursorW( NULL, (LPCWSTR)IDC_ARROW );
    wc.lpszClassName = L"zowned";
    RegisterClassW( &wc );
    owner = CreateWindowExW( 0, L"zowned", L"zowned owner", WS_OVERLAPPEDWINDOW, 100, 100, 500, 400, 0, 0, 0, 0 );
    popup = CreateWindowExW( WS_EX_DLGMODALFRAME, L"zowned", L"", WS_POPUP | WS_CAPTION | WS_SYSMENU,
                             200, 180, 260, 180, owner, 0, 0, 0 );
    /* the browser's order: the dialog first, then its owner, disabled behind it */
    ShowWindow( popup, SW_SHOWNOACTIVATE );
    ShowWindow( owner, SW_SHOWNOACTIVATE );
    EnableWindow( owner, FALSE );
    SetActiveWindow( popup );
    pump( 1500 );
    fprintf( log, "XWIN %lu\n", (unsigned long)(ULONG_PTR)GetPropW( owner, L"__wine_x11_whole_window" ) );
    fprintf( log, "SHOWN %s\n", order() );
    /* put right for the next check, if the showing got it wrong */
    SetWindowPos( popup, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE );
    pump( 1000 );
    fprintf( log, "BEFORE %s\n", order() );
    fflush( log );
    /* the gate exposes the owner's X window, then makes C:\go */
    wcscpy( go, argv[1] );
    wcscat( go, L".go" );
    for (i = 0; i < 300 && GetFileAttributesW( go ) == INVALID_FILE_ATTRIBUTES; i++) pump( 100 );
    pump( 1500 );
    fprintf( log, "AFTER %s\n", order() );
    fclose( log );
    return 0;
}
