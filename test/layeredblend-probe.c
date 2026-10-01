/* A popup alpha-layered at half (premultiplied red, alpha 128), shown on the
 * desktop as Chrome shows its menus: UpdateLayeredWindow, then shown. With
 * "glass", a popup that is a sheet of glass (DwmExtendFrameIntoClientArea -1)
 * with the same pixels, lower down: Chrome's bubbles. */
#include <windows.h>
#include <dwmapi.h>

int WINAPI wWinMain( HINSTANCE inst, HINSTANCE prev, WCHAR *cmd, int show )
{
    WNDCLASSW wc = { 0, DefWindowProcW, 0, 0, inst, 0, 0, 0, 0, L"SgLayeredBlend" };
    BITMAPINFO bi = {{ sizeof(BITMAPINFOHEADER), 200, -150, 1, 32, BI_RGB }};
    BLENDFUNCTION bf = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
    POINT pt = { 300, 200 }, zero = { 0, 0 };
    SIZE size = { 200, 150 };
    HDC screen = GetDC( 0 ), mem = CreateCompatibleDC( screen );
    DWORD *bits;
    HBITMAP dib;
    HWND hwnd;
    MSG msg;
    int i;

    RegisterClassW( &wc );
    dib = CreateDIBSection( screen, &bi, DIB_RGB_COLORS, (void **)&bits, NULL, 0 );
    for (i = 0; i < 200 * 150; i++) bits[i] = 0x80800000;
    SelectObject( mem, dib );
    if (cmd && !wcscmp( cmd, L"glass" ))
    {
        /* a sheet of glass (DwmExtendFrameIntoClientArea, -1): the program's
         * own premultiplied pixels, as a swap chain with alpha shows them */
        MARGINS m = { -1, -1, -1, -1 };
        HDC dc;
        hwnd = CreateWindowExW( WS_EX_TOPMOST | WS_EX_TOOLWINDOW, L"SgLayeredBlend", L"SGGLASS",
                                WS_POPUP, 300, 380, 200, 150, 0, 0, inst, 0 );
        DwmExtendFrameIntoClientArea( hwnd, &m );
        ShowWindow( hwnd, SW_SHOWNOACTIVATE );
        for (i = 0; i < 20; i++)
        {
            dc = GetDC( hwnd );
            BitBlt( dc, 0, 0, 200, 150, mem, 0, 0, SRCCOPY );
            ReleaseDC( hwnd, dc );
            Sleep( 100 );
        }
    }
    else
    {
        hwnd = CreateWindowExW( WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW, L"SgLayeredBlend", L"SGLAYERED",
                                WS_POPUP, 300, 200, 200, 150, 0, 0, inst, 0 );
        UpdateLayeredWindow( hwnd, screen, &pt, &size, mem, &zero, 0, &bf, ULW_ALPHA );
        ShowWindow( hwnd, SW_SHOWNOACTIVATE );
    }
    while (GetMessageW( &msg, 0, 0, 0 )) DispatchMessageW( &msg );
    return 0;
}
