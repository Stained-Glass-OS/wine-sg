/* A popup alpha-layered at half (premultiplied red, alpha 128), shown on the
 * desktop as Chrome shows its menus: UpdateLayeredWindow, then shown. */
#include <windows.h>

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
    hwnd = CreateWindowExW( WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW, L"SgLayeredBlend", L"SGLAYERED",
                            WS_POPUP, 300, 200, 200, 150, 0, 0, inst, 0 );
    dib = CreateDIBSection( screen, &bi, DIB_RGB_COLORS, (void **)&bits, NULL, 0 );
    for (i = 0; i < 200 * 150; i++) bits[i] = 0x80800000;
    SelectObject( mem, dib );
    UpdateLayeredWindow( hwnd, screen, &pt, &size, mem, &zero, 0, &bf, ULW_ALPHA );
    ShowWindow( hwnd, SW_SHOWNOACTIVATE );
    while (GetMessageW( &msg, 0, 0, 0 )) DispatchMessageW( &msg );
    return 0;
}
