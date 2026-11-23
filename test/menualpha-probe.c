/* menualpha-probe: a menu item's 32 bpp bitmap with an alpha channel is drawn
 * blended, as on Windows (patches/sg/8702). SWT (TuxGuitar, Eclipse, DBeaver)
 * gives menus premultiplied 32 bpp DIB sections; win32u copied them with
 * BitBlt and every transparent pixel came out black.
 * Item 0: 16x16, an opaque red 8x8 square in a transparent field.
 * Item 1: 16x16 32 bpp, alpha 0 everywhere, blue (no alpha channel: opaque, as before).
 * Prints "black N red N blue N": pixels of the menu window (which has no text). */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

static HMENU menu;
static HWND owner;
static int done;

static HBITMAP make_bitmap( int alpha )
{
    BITMAPINFO bi = {{ sizeof(BITMAPINFOHEADER), 16, -16, 1, 32, BI_RGB }};
    DWORD *bits;
    HBITMAP b = CreateDIBSection( NULL, &bi, DIB_RGB_COLORS, (void **)&bits, NULL, 0 );
    int x, y;
    for (y = 0; y < 16; y++) for (x = 0; x < 16; x++)
    {
        if (alpha) bits[y * 16 + x] = (x >= 4 && x < 12 && y >= 4 && y < 12) ? 0xffff0000 : 0;
        else bits[y * 16 + x] = 0x000000ff;
    }
    return b;
}

static void count( HDC dc, RECT *r, int *black, int *red, int *blue )
{
    int x, y;
    *black = *red = *blue = 0;
    for (y = r->top; y < r->bottom; y++) for (x = r->left; x < r->right; x++)
    {
        COLORREF c = GetPixel( dc, x, y );
        if (c == RGB(0, 0, 0)) (*black)++;
        else if (c == RGB(255, 0, 0)) (*red)++;
        else if (c == RGB(0, 0, 255)) (*blue)++;
    }
}

static void CALLBACK check( HWND hwnd, UINT msg, UINT_PTR id, DWORD t )
{
    HWND popup = FindWindowW( L"#32768", NULL );
    RECT r0, wr;
    int black, red, blue;
    HDC dc;

    KillTimer( hwnd, id );
    if (!popup) { printf( "no menu window\n" ); done = 1; EndMenu(); return; }
    GetWindowRect( popup, &wr );
    /* the whole menu window: its items are the two bitmaps, no text */
    SetRect( &r0, 0, 0, wr.right - wr.left, wr.bottom - wr.top );
    dc = GetWindowDC( popup );
    count( dc, &r0, &black, &red, &blue );
    ReleaseDC( popup, dc );
    printf( "black %d red %d blue %d\n", black, red, blue );
    done = 1;
    EndMenu();
}

int main( void )
{
    MENUITEMINFOW mi = { sizeof(mi) };
    WNDCLASSW wc = { 0 };

    wc.lpfnWndProc = DefWindowProcW; wc.hInstance = GetModuleHandleW( NULL ); wc.lpszClassName = L"MenuAlpha";
    RegisterClassW( &wc );
    owner = CreateWindowW( L"MenuAlpha", L"menu alpha", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 50, 50, 300, 200, NULL, NULL, NULL, NULL );
    menu = CreatePopupMenu();
    mi.fMask = MIIM_BITMAP | MIIM_ID;
    mi.wID = 100; mi.hbmpItem = make_bitmap( 1 );
    InsertMenuItemW( menu, 0, TRUE, &mi );
    mi.wID = 101; mi.hbmpItem = make_bitmap( 0 );
    InsertMenuItemW( menu, 1, TRUE, &mi );
    SetForegroundWindow( owner );
    SetTimer( owner, 1, 1500, check );
    TrackPopupMenu( menu, TPM_LEFTALIGN | TPM_TOPALIGN, 100, 100, 0, owner, NULL );
    if (!done) printf( "menu ended early\n" );
    return 0;
}
