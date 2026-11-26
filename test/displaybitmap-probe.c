/* The screen DC's bitmap (patches/sg/9627), run by test/displaybitmap-gate.sh:
 * what TightVNC's desktop server asks before it captures (Screen::getBMI):
 * GetCurrentObject(GetDC(0), OBJ_BITMAP), GetDIBits for its format -- then
 * DeleteObject of that bitmap and DeleteDC, and the same again. Prints
 * "FIRST <lines> <bpp>", "SECOND <lines> <bpp>", "BLT <ok>". */
#include <windows.h>
#include <stdio.h>

static int ask( const char *what )
{
    struct { BITMAPINFOHEADER h; RGBQUAD c[256]; } bmi = { { sizeof(BITMAPINFOHEADER) } };
    HDC dc = GetDC( NULL );
    HBITMAP bm = GetCurrentObject( dc, OBJ_BITMAP );
    int r = GetDIBits( dc, bm, 0, 0, NULL, (BITMAPINFO *)&bmi, DIB_RGB_COLORS );
    printf( "%s %d %d\n", what, r, bmi.h.biBitCount );
    DeleteObject( bm );
    DeleteDC( dc );
    return r;
}

int main( void )
{
    HDC screen, mem;
    HBITMAP dib;
    void *bits;
    BITMAPINFO bi = { { sizeof(bi.bmiHeader), 64, -64, 1, 32, BI_RGB } };

    ask( "FIRST" );
    ask( "SECOND" );
    screen = GetDC( NULL );
    mem = CreateCompatibleDC( screen );
    dib = CreateDIBSection( mem, &bi, DIB_RGB_COLORS, &bits, NULL, 0 );
    SelectObject( mem, dib );
    printf( "BLT %d\n", BitBlt( mem, 0, 0, 64, 64, screen, 0, 0, SRCCOPY ) );
    return 0;
}
