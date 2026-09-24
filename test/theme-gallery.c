/* theme-gallery: one window with the common controls in their usual states,
 * for test/theme-gate.sh -- the canvas the Stained Glass visual style and
 * fonts are judged on.
 *
 *   theme-gallery              the window
 *   theme-gallery --dump       the active theme and font settings
 *   theme-gallery --render F   text in the message font, and the themed scroll
 *                              bar's parts, drawn into a 32 bpp DIB saved as F:
 *                              pixels, independent of any window placement
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include <windows.h>
#include <commctrl.h>
#include <uxtheme.h>
#include <vssym32.h>
#include <stdio.h>

static HFONT g_font;

static HWND add( HWND parent, const WCHAR *cls, const WCHAR *text, DWORD style, int x, int y, int w, int h, int id )
{
    HWND hwnd = CreateWindowExW( !lstrcmpW( cls, L"EDIT" ) || !lstrcmpW( cls, WC_LISTVIEWW ) ? WS_EX_CLIENTEDGE : 0,
                                 cls, text, WS_CHILD | WS_VISIBLE | style, x, y, w, h, parent,
                                 (HMENU)(INT_PTR)id, NULL, NULL );
    SendMessageW( hwnd, WM_SETFONT, (WPARAM)g_font, TRUE );
    return hwnd;
}

static void build( HWND hwnd )
{
    static const WCHAR text[] =
        L"The quick brown fox jumps over the lazy dog. 0123456789\r\n"
        L"Stained Glass OS renders Windows programs with anti-aliased text,\r\n"
        L"flat Windows 10 scroll bars and its own visual style.\r\n";
    WCHAR buf[8192] = L"";
    LVCOLUMNW col = { LVCF_TEXT | LVCF_WIDTH, 0, 120 };
    LVITEMW item = { LVIF_TEXT };
    HWND w;
    int i;

    add( hwnd, L"BUTTON", L"Default button", BS_DEFPUSHBUTTON | WS_TABSTOP, 16, 16, 120, 28, 1 );
    add( hwnd, L"BUTTON", L"Button", BS_PUSHBUTTON | WS_TABSTOP, 144, 16, 100, 28, 2 );
    add( hwnd, L"BUTTON", L"Disabled", BS_PUSHBUTTON | WS_DISABLED, 252, 16, 100, 28, 3 );
    w = add( hwnd, L"BUTTON", L"Check box, checked", BS_AUTOCHECKBOX | WS_TABSTOP, 16, 56, 170, 22, 4 );
    SendMessageW( w, BM_SETCHECK, BST_CHECKED, 0 );
    add( hwnd, L"BUTTON", L"Check box", BS_AUTOCHECKBOX | WS_TABSTOP, 190, 56, 150, 22, 5 );
    w = add( hwnd, L"BUTTON", L"Option one", BS_AUTORADIOBUTTON | WS_GROUP, 16, 82, 150, 22, 6 );
    SendMessageW( w, BM_SETCHECK, BST_CHECKED, 0 );
    add( hwnd, L"BUTTON", L"Option two", BS_AUTORADIOBUTTON, 190, 82, 150, 22, 7 );
    add( hwnd, L"BUTTON", L"Group box", BS_GROUPBOX, 360, 10, 250, 100, 8 );
    add( hwnd, L"STATIC", L"Static text in a group box, 9 pt.", 0, 376, 34, 220, 20, 9 );
    w = add( hwnd, L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP, 376, 60, 220, 200, 10 );
    SendMessageW( w, CB_ADDSTRING, 0, (LPARAM)L"Combo box item" );
    SendMessageW( w, CB_SETCURSEL, 0, 0 );

    for (i = 0; i < 30; i++) lstrcatW( buf, text );
    add( hwnd, L"EDIT", buf, ES_MULTILINE | WS_VSCROLL | WS_HSCROLL | ES_AUTOHSCROLL | WS_TABSTOP, 16, 120, 330, 200, 11 );

    w = add( hwnd, WC_LISTVIEWW, L"", LVS_REPORT | WS_TABSTOP, 360, 120, 250, 200, 12 );
    col.pszText = (WCHAR *)L"Name"; SendMessageW( w, LVM_INSERTCOLUMNW, 0, (LPARAM)&col );
    col.pszText = (WCHAR *)L"Size"; col.cx = 90; SendMessageW( w, LVM_INSERTCOLUMNW, 1, (LPARAM)&col );
    SendMessageW( w, LVM_SETEXTENDEDLISTVIEWSTYLE, 0, LVS_EX_FULLROWSELECT );
    SetWindowTheme( w, L"Explorer", NULL );
    for (i = 0; i < 30; i++)
    {
        WCHAR name[32];
        swprintf( name, 32, L"Document %02d.txt", i );
        item.iItem = i; item.iSubItem = 0; item.pszText = name;
        SendMessageW( w, LVM_INSERTITEMW, 0, (LPARAM)&item );
    }
    item.mask = LVIF_STATE; item.iItem = 1; item.state = item.stateMask = LVIS_SELECTED;
    SendMessageW( w, LVM_SETITEMSTATE, 1, (LPARAM)&item );

    w = add( hwnd, PROGRESS_CLASSW, L"", 0, 16, 332, 330, 18, 13 );
    SendMessageW( w, PBM_SETPOS, 60, 0 );
    w = add( hwnd, TRACKBAR_CLASSW, L"", TBS_HORZ, 360, 328, 250, 30, 14 );
    SendMessageW( w, TBM_SETPOS, TRUE, 40 );
    add( hwnd, L"EDIT", L"Single-line edit", ES_AUTOHSCROLL | WS_TABSTOP | WS_BORDER, 16, 362, 330, 24, 15 );
    w = add( hwnd, WC_TABCONTROLW, L"", 0, 360, 364, 250, 40, 16 );
    {
        TCITEMW tab = { TCIF_TEXT };
        tab.pszText = (WCHAR *)L"General"; SendMessageW( w, TCM_INSERTITEMW, 0, (LPARAM)&tab );
        tab.pszText = (WCHAR *)L"Sharing"; SendMessageW( w, TCM_INSERTITEMW, 1, (LPARAM)&tab );
        tab.pszText = (WCHAR *)L"Security"; SendMessageW( w, TCM_INSERTITEMW, 2, (LPARAM)&tab );
    }
}

static LRESULT CALLBACK proc( HWND hwnd, UINT msg, WPARAM wp, LPARAM lp )
{
    switch (msg)
    {
    case WM_CREATE: build( hwnd ); return 0;
    case WM_CTLCOLORSTATIC: SetBkMode( (HDC)wp, TRANSPARENT ); return (LRESULT)GetSysColorBrush( COLOR_WINDOW );
    case WM_DESTROY: PostQuitMessage( 0 ); return 0;
    }
    return DefWindowProcW( hwnd, msg, wp, lp );
}

static void dump( void )
{
    NONCLIENTMETRICSW ncm = { sizeof(ncm) };
    WCHAR file[MAX_PATH] = L"", color[64] = L"", size[64] = L"";
    UINT smoothing = 0, type = 0;
    LOGFONTW lf;

    SystemParametersInfoW( SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0 );
    SystemParametersInfoW( SPI_GETFONTSMOOTHING, 0, &smoothing, 0 );
    SystemParametersInfoW( SPI_GETFONTSMOOTHINGTYPE, 0, &type, 0 );
    GetCurrentThemeName( file, MAX_PATH, color, 64, size, 64 );
    GetObjectW( GetStockObject( DEFAULT_GUI_FONT ), sizeof(lf), &lf );
    printf( "ThemeActive=%d\nTheme=%ls\nColor=%ls\nSize=%ls\n", IsThemeActive(), file, color, size );
    printf( "FontSmoothing=%u\nFontSmoothingType=%u\n", smoothing, type );
    printf( "CaptionFont=%ls %ld\nMenuFont=%ls %ld\nMessageFont=%ls %ld\nStatusFont=%ls %ld\nGuiFont=%ls\n",
            ncm.lfCaptionFont.lfFaceName, ncm.lfCaptionFont.lfHeight, ncm.lfMenuFont.lfFaceName, ncm.lfMenuFont.lfHeight,
            ncm.lfMessageFont.lfFaceName, ncm.lfMessageFont.lfHeight, ncm.lfStatusFont.lfFaceName, ncm.lfStatusFont.lfHeight,
            lf.lfFaceName );
    printf( "ScrollWidth=%ld\nCaptionHeight=%ld\n", ncm.iScrollWidth, ncm.iCaptionHeight );
}

/* Layout of --render's image, which test/theme-gate.sh reads. */
#define R_W 480
#define R_H 120
static const WCHAR render_text[] = L"The quick brown fox jumps over the lazy dog 0123456789";

static int render( const WCHAR *path )
{
    NONCLIENTMETRICSW ncm = { sizeof(ncm) };
    BITMAPINFO bmi = { { sizeof(bmi.bmiHeader), R_W, -R_H, 1, 32, BI_RGB } };
    BITMAPFILEHEADER bfh = { 0x4d42 };
    RECT rc = { 0, 0, R_W, R_H };
    HTHEME theme;
    HBITMAP dib;
    HFONT font;
    void *bits;
    HANDLE file;
    DWORD written;
    HDC dc = CreateCompatibleDC( NULL );

    SystemParametersInfoW( SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0 );
    dib = CreateDIBSection( dc, &bmi, DIB_RGB_COLORS, &bits, NULL, 0 );
    SelectObject( dc, dib );
    FillRect( dc, &rc, GetStockObject( WHITE_BRUSH ) );

    /* rows 4..40: text, as a control draws it */
    font = CreateFontIndirectW( &ncm.lfMessageFont );
    SelectObject( dc, font );
    SetTextColor( dc, RGB(0, 0, 0) );
    SetBkMode( dc, TRANSPARENT );
    TextOutW( dc, 4, 4, render_text, lstrlenW( render_text ) );
    TextOutW( dc, 4, 22, render_text, lstrlenW( render_text ) );

    /* columns 400..: a vertical scroll bar's parts, top to bottom: up arrow
     * (normal), upper track, thumb (normal), thumb (hot), lower track */
    if ((theme = OpenThemeData( NULL, L"SCROLLBAR" )))
    {
        RECT r;
        SetRect( &r, 400, 0, 417, 17 );   DrawThemeBackground( theme, dc, SBP_ARROWBTN, ABS_UPNORMAL, &r, NULL );
        SetRect( &r, 400, 17, 417, 37 );  DrawThemeBackground( theme, dc, SBP_UPPERTRACKVERT, SCRBS_NORMAL, &r, NULL );
        SetRect( &r, 400, 37, 417, 67 );  DrawThemeBackground( theme, dc, SBP_THUMBBTNVERT, SCRBS_NORMAL, &r, NULL );
        SetRect( &r, 420, 37, 437, 67 );  DrawThemeBackground( theme, dc, SBP_THUMBBTNVERT, SCRBS_HOT, &r, NULL );
        SetRect( &r, 400, 67, 417, 100 ); DrawThemeBackground( theme, dc, SBP_LOWERTRACKVERT, SCRBS_NORMAL, &r, NULL );
        CloseThemeData( theme );
    }
    GdiFlush();

    bfh.bfOffBits = sizeof(bfh) + sizeof(bmi.bmiHeader);
    bfh.bfSize = bfh.bfOffBits + R_W * R_H * 4;
    file = CreateFileW( path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL );
    if (file == INVALID_HANDLE_VALUE) return 1;
    WriteFile( file, &bfh, sizeof(bfh), &written, NULL );
    WriteFile( file, &bmi.bmiHeader, sizeof(bmi.bmiHeader), &written, NULL );
    WriteFile( file, bits, R_W * R_H * 4, &written, NULL );
    CloseHandle( file );
    return 0;
}

int WINAPI wWinMain( HINSTANCE inst, HINSTANCE prev, WCHAR *cmd, int show )
{
    INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_WIN95_CLASSES | ICC_STANDARD_CLASSES };
    NONCLIENTMETRICSW ncm = { sizeof(ncm) };
    WNDCLASSW wc = { 0 };
    HMENU menu = CreateMenu(), file = CreatePopupMenu();
    MSG msg;

    if (wcsstr( cmd, L"--dump" )) { dump(); return 0; }
    if (!wcsncmp( cmd, L"--render ", 9 )) return render( cmd + 9 );
    InitCommonControlsEx( &icc );
    SystemParametersInfoW( SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0 );
    g_font = CreateFontIndirectW( &ncm.lfMessageFont );

    AppendMenuW( file, MF_STRING, 100, L"&Open...\tCtrl+O" );
    AppendMenuW( file, MF_SEPARATOR, 0, NULL );
    AppendMenuW( file, MF_STRING, 101, L"E&xit" );
    AppendMenuW( menu, MF_POPUP, (UINT_PTR)file, L"&File" );
    AppendMenuW( menu, MF_STRING, 102, L"&Edit" );
    AppendMenuW( menu, MF_STRING, 103, L"&View" );
    AppendMenuW( menu, MF_STRING, 104, L"&Help" );

    wc.lpfnWndProc = proc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursorW( NULL, (const WCHAR *)IDC_ARROW );
    wc.hbrBackground = GetSysColorBrush( COLOR_WINDOW );
    wc.lpszClassName = L"SgThemeGallery";
    RegisterClassW( &wc );
    CreateWindowW( wc.lpszClassName, L"Theme gallery - Stained Glass", WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                   20, 20, 640, 480, NULL, menu, inst, NULL );
    while (GetMessageW( &msg, NULL, 0, 0 ))
    {
        TranslateMessage( &msg );
        DispatchMessageW( &msg );
    }
    return 0;
}
