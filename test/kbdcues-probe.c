/* kbdcues-probe: keyboard cues (patches/sg/6541). A new profile hides access-key underlines and
 * focus rectangles until the keyboard is used, as Windows does by default: a dialog's button and
 * label draw no underline, Alt shows them (WM_CHANGEUISTATE / WM_UPDATEUISTATE to the controls),
 * Tab shows the focus rectangle, a menu bar shows its underlines once told; with "Underline access
 * keys" (SPI_SETKEYBOARDCUES) on, a new window shows them from the start.
 * Prints name=value lines; see test/kbdcues-gate.sh.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include <windows.h>
#include <stdio.h>

#define IDC_SAVE 100
#define IDC_NAME 101
#define IDC_EDIT 102

static int updates;

static LRESULT CALLBACK count_proc( HWND hwnd, UINT msg, WPARAM wp, LPARAM lp )
{
    WNDPROC old = (WNDPROC)GetPropA( hwnd, "old" );
    if (msg == WM_UPDATEUISTATE) updates++;
    return CallWindowProcW( old, hwnd, msg, wp, lp );
}

/* dark pixels of a window's client area drawn with WM_PRINTCLIENT-like PrintWindow */
static int dark_pixels( HWND hwnd, BOOL whole )
{
    RECT rc;
    HDC screen = GetDC( 0 ), dc = CreateCompatibleDC( screen );
    HBITMAP bmp;
    int x, y, n = 0;

    GetWindowRect( hwnd, &rc );
    OffsetRect( &rc, -rc.left, -rc.top );
    bmp = CreateCompatibleBitmap( screen, rc.right, rc.bottom );
    SelectObject( dc, bmp );
    PatBlt( dc, 0, 0, rc.right, rc.bottom, WHITENESS );
    if (whole)
    {
        /* the menu bar is non-client: from the screen */
        HDC wdc = GetWindowDC( hwnd );
        BitBlt( dc, 0, 0, rc.right, rc.bottom, wdc, 0, 0, SRCCOPY );
        ReleaseDC( hwnd, wdc );
    }
    else PrintWindow( hwnd, dc, PW_CLIENTONLY );
    for (y = 0; y < rc.bottom; y++)
        for (x = 0; x < rc.right; x++)
        {
            COLORREF c = GetPixel( dc, x, y );
            if (GetRValue( c ) + GetGValue( c ) + GetBValue( c ) < 200) n++;
        }
    DeleteDC( dc );
    DeleteObject( bmp );
    ReleaseDC( 0, screen );
    return n;
}

static void pump( HWND dlg, DWORD ms )
{
    DWORD end = GetTickCount() + ms;
    MSG msg;
    while ((int)(end - GetTickCount()) > 0)
    {
        while (PeekMessageW( &msg, 0, 0, 0, PM_REMOVE ))
        {
            if (dlg && IsDialogMessageW( dlg, &msg )) continue;
            TranslateMessage( &msg );
            DispatchMessageW( &msg );
        }
        Sleep( 10 );
    }
}

static void press( WORD vk )
{
    INPUT in[2] = { 0 };
    in[0].type = in[1].type = INPUT_KEYBOARD;
    in[0].ki.wVk = in[1].ki.wVk = vk;
    in[1].ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput( 2, in, sizeof(INPUT) );
}

static INT_PTR CALLBACK dlg_proc( HWND hwnd, UINT msg, WPARAM wp, LPARAM lp )
{
    return msg == WM_INITDIALOG;
}

int main( int argc, char **argv )
{
    struct { DLGTEMPLATE t; WORD menu, cls, title; WORD pt; WCHAR font[9]; } *tmpl;
    BYTE buffer[1024] = { 0 }, *p;
    HWND dlg, save, name, edit, main;
    HMENU bar, file;
    BOOL cues = TRUE;
    int before, after, sbefore, safter, mbefore, mafter;
    UINT state;

    setvbuf( stdout, NULL, _IONBF, 0 );
    SystemParametersInfoW( SPI_GETKEYBOARDCUES, 0, &cues, 0 );
    printf( "cues_default_off=%d\n", !cues );

    if (argc > 1 && !strcmp( argv[1], "cueson" ))
    {
        /* "Underline access keys" on: a new window shows them from the start */
        HWND w;
        SystemParametersInfoW( SPI_SETKEYBOARDCUES, 0, (void *)TRUE, 0 );
        w = CreateWindowW( L"BUTTON", L"&Save", WS_POPUP | WS_VISIBLE, 10, 10, 120, 40, 0, 0, 0, 0 );
        printf( "cueson_state=%u\n", (UINT)SendMessageW( w, WM_QUERYUISTATE, 0, 0 ) );
        printf( "cueson_shown=%d\n", !(SendMessageW( w, WM_QUERYUISTATE, 0, 0 ) & UISF_HIDEACCEL) );
        SystemParametersInfoW( SPI_SETKEYBOARDCUES, 0, (void *)FALSE, 0 );
        printf( "done=1\n" );
        return 0;
    }

    if (argc > 1 && !strcmp( argv[1], "v6" ))
    {
        /* the same with common controls 6 (the controls of programs with a manifest) */
        static const char manifest[] =
            "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
            "<assembly xmlns=\"urn:schemas-microsoft-com:asm.v1\" manifestVersion=\"1.0\">"
            "<dependency><dependentAssembly><assemblyIdentity type=\"win32\" name=\"Microsoft.Windows.Common-Controls\""
            " version=\"6.0.0.0\" processorArchitecture=\"*\" publicKeyToken=\"6595b64144ccf1df\" language=\"*\"/>"
            "</dependentAssembly></dependency></assembly>";
        ACTCTXW ctx = { sizeof(ctx) };
        ULONG_PTR cookie;
        HANDLE file, actctx;
        DWORD written;

        file = CreateFileW( L"C:\\v6.manifest", GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL );
        WriteFile( file, manifest, sizeof(manifest) - 1, &written, NULL );
        CloseHandle( file );
        ctx.lpSource = L"C:\\v6.manifest";
        actctx = CreateActCtxW( &ctx );
        printf( "v6_context=%d\n", actctx != INVALID_HANDLE_VALUE && ActivateActCtx( actctx, &cookie ) );
        LoadLibraryW( L"comctl32.dll" );
    }

    /* a dialog: an "&Name:" label, an edit box and a "&Save" button */
    tmpl = (void *)buffer;
    tmpl->t.style = WS_POPUP | WS_VISIBLE | WS_CAPTION | DS_SETFONT;
    tmpl->t.cdit = 0;
    tmpl->t.cx = 160; tmpl->t.cy = 80;
    tmpl->pt = 12;
    lstrcpyW( tmpl->font, L"Tahoma" );
    dlg = CreateDialogIndirectW( NULL, &tmpl->t, NULL, dlg_proc );
    (void)p;
    name = CreateWindowW( L"STATIC", L"&Name:", WS_CHILD | WS_VISIBLE, 10, 12, 100, 24, dlg, (HMENU)IDC_NAME, 0, 0 );
    edit = CreateWindowW( L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | WS_TABSTOP, 10, 40, 140, 24, dlg, (HMENU)IDC_EDIT, 0, 0 );
    save = CreateWindowW( L"BUTTON", L"&Save", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON, 160, 40, 120, 40, dlg, (HMENU)IDC_SAVE, 0, 0 );
    SendMessageW( name, WM_SETFONT, (WPARAM)GetStockObject( DEFAULT_GUI_FONT ), 0 );
    SendMessageW( save, WM_SETFONT, (WPARAM)GetStockObject( DEFAULT_GUI_FONT ), 0 );
    SetWindowPos( dlg, 0, 50, 50, 320, 140, SWP_NOZORDER );
    SetPropA( save, "old", (HANDLE)SetWindowLongPtrW( save, GWLP_WNDPROC, (LONG_PTR)count_proc ) );
    /* a menu bar: no underline until its window is told the keyboard is in use */
    bar = CreateMenu();
    file = CreatePopupMenu();
    AppendMenuW( file, MF_STRING, 1, L"&Open" );
    AppendMenuW( bar, MF_POPUP, (UINT_PTR)file, L"&File" );
    AppendMenuW( bar, MF_STRING, 2, L"&Help" );
    main = CreateWindowW( L"STATIC", L"main", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 400, 50, 300, 150, 0, bar, 0, 0 );
    pump( NULL, 500 );
    mbefore = dark_pixels( main, TRUE );
    SetForegroundWindow( dlg );
    SetFocus( edit );
    pump( dlg, 500 );

    state = SendMessageW( save, WM_QUERYUISTATE, 0, 0 );
    printf( "initial_state=%u\n", state );
    printf( "initial_hidden=%d\n", (state & (UISF_HIDEACCEL | UISF_HIDEFOCUS)) == (UISF_HIDEACCEL | UISF_HIDEFOCUS) );
    before = dark_pixels( save, FALSE );
    sbefore = dark_pixels( name, FALSE );

    /* Alt: the access keys */
    press( VK_MENU );
    pump( dlg, 800 );
    state = SendMessageW( save, WM_QUERYUISTATE, 0, 0 );
    after = dark_pixels( save, FALSE );
    safter = dark_pixels( name, FALSE );
    printf( "after_alt_state=%u updates=%d\n", state, updates );
    printf( "alt_reveals=%d\n", !(state & UISF_HIDEACCEL) && updates > 0 );
    printf( "button px %d -> %d, label px %d -> %d\n", before, after, sbefore, safter );
    printf( "button_underline=%d\n", after > before );
    printf( "label_underline=%d\n", safter > sbefore );

    /* Tab: the focus rectangle */
    press( VK_TAB );
    pump( dlg, 800 );
    state = SendMessageW( save, WM_QUERYUISTATE, 0, 0 );
    printf( "after_tab_state=%u focus=%d\n", state, GetFocus() == save );
    printf( "tab_shows_focus=%d\n", !(state & UISF_HIDEFOCUS) );

    SendMessageW( main, WM_CHANGEUISTATE, MAKEWPARAM( UIS_CLEAR, UISF_HIDEACCEL ), 0 );
    pump( NULL, 500 );
    mafter = dark_pixels( main, TRUE );
    printf( "menu px %d -> %d\n", mbefore, mafter );
    printf( "menu_underline=%d\n", mafter > mbefore );
    {
        /* a window opened after keyboard use shows its cues from the start */
        HWND late = CreateWindowW( L"BUTTON", L"&Late", WS_POPUP | WS_VISIBLE, 10, 300, 120, 40, 0, 0, 0, 0 );
        printf( "keyboard_opened_shown=%d\n", !(SendMessageW( late, WM_QUERYUISTATE, 0, 0 ) & UISF_HIDEACCEL) );
    }
    printf( "done=1\n" );
    return 0;
}
