/* msgboxpanel-probe.c: the layout of a message box's white panel and button strip (wine-sg 7784).
 * probe.exe prints, for a box without an icon (one line and three lines) and one with an icon:
 *   NAME icon=<visible> textbottom<=strip:<0|1> textmargin=<px above the text> strip=<0|1 the row above the buttons is all face colour>
 */
#include <windows.h>
#include <stdio.h>

static const WCHAR *texts[] = {
    L"PawnIO is not installed, do you want to install it?",
    L"First line of the message.\nSecond line, a little longer than the first.\nThird line.",
    L"PawnIO is not installed, do you want to install it?",
};
static const UINT styles[] = { MB_OKCANCEL, MB_OKCANCEL, MB_OKCANCEL | MB_ICONINFORMATION };
static const char *names[] = { "noicon1", "noicon3", "icon" };
static int cur;

static DWORD WINAPI box_thread( void *arg )
{
    MessageBoxW( NULL, texts[cur], L"msgboxpanel probe", styles[cur] );
    return 0;
}

static void measure( HWND dlg )
{
    static const int ids[] = { IDOK, IDCANCEL };
    RECT client, text, b;
    HWND icon = GetDlgItem( dlg, 0x440 ), txt = GetDlgItem( dlg, 0xffff );
    int i, top = -1, strip, ok = 1, x, y;
    COLORREF face = GetSysColor( COLOR_BTNFACE );
    HDC screen = GetDC( NULL ), mem = CreateCompatibleDC( screen );
    HBITMAP bmp;

    GetClientRect( dlg, &client );
    for (i = 0; i < 2; i++)
    {
        GetWindowRect( GetDlgItem( dlg, ids[i] ), &b );
        MapWindowPoints( NULL, dlg, (POINT *)&b, 2 );
        if (top < 0 || b.top < top) top = b.top;
    }
    strip = top - MulDiv( 11, GetDpiForWindow( dlg ), 96 );
    GetWindowRect( txt, &text );
    MapWindowPoints( NULL, dlg, (POINT *)&text, 2 );

    bmp = CreateCompatibleBitmap( screen, client.right, client.bottom );
    SelectObject( mem, bmp );
    PrintWindow( dlg, mem, PW_CLIENTONLY );
    y = top - 4;
    for (x = 4; x < client.right - 4; x++)
        if (GetPixel( mem, x, y ) != face) { ok = 0; break; }
    printf( "%s icon=%d textbottom<=strip:%d textmargin=%ld strip=%d\n", names[cur],
            icon && IsWindowVisible( icon ) ? 1 : 0, text.bottom <= strip ? 1 : 0, text.top, ok );
    if (!ok) printf( "%s firstbad=%d,%d colour=%06lx\n", names[cur], x, y, GetPixel( mem, x, y ) );
    DeleteDC( mem ); DeleteObject( bmp ); ReleaseDC( NULL, screen );
}

int main( void )
{
    for (cur = 0; cur < 3; cur++)
    {
        HANDLE th = CreateThread( NULL, 0, box_thread, NULL, 0, NULL );
        HWND dlg = NULL;
        int i;
        for (i = 0; i < 100 && !dlg; i++) { Sleep( 100 ); dlg = FindWindowW( L"#32770", L"msgboxpanel probe" ); }
        if (!dlg) { printf( "%s none\n", names[cur] ); return 1; }
        Sleep( 800 );
        measure( dlg );
        PostMessageW( dlg, WM_COMMAND, IDCANCEL, 0 );
        WaitForSingleObject( th, 5000 );
        CloseHandle( th );
    }
    return 0;
}
