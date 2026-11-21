/* anipad-probe: user32 reads every frame of an animated cursor whose frame chunk has an odd size (Stained Glass
 * OS wine-sg 8287; test/anipad-gate.sh).  RIFF pads such a chunk to an even size; the frame loop did not, so
 * every frame after it failed ("Error creating animated cursor, only using first frame!", GnuCash, darktable).
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#include <windows.h>
#include <stdio.h>
#include <string.h>

static BYTE buf[40000];
static DWORD pos;

static void put( const void *p, DWORD n ) { memcpy( buf + pos, p, n ); pos += n; }
static void put32( DWORD v ) { put( &v, 4 ); }
static void put16( WORD v ) { put( &v, 2 ); }

/* a 32x32 cursor of one colour, plus 'extra' bytes after it */
static DWORD frame( DWORD colour, DWORD extra )
{
    BITMAPINFOHEADER bih = { sizeof(bih), 32, 64, 1, 32 };
    DWORD start = pos, i;

    put16( 0 ); put16( 2 ); put16( 1 );                              /* CURSORICONFILEDIR */
    buf[pos++] = 32; buf[pos++] = 32; buf[pos++] = 0; buf[pos++] = 0; /* width, height, colours, reserved */
    put16( 1 ); put16( 1 );                                          /* hotspot */
    put32( sizeof(bih) + 32 * 32 * 4 + 32 * 4 ); put32( 22 );
    put( &bih, sizeof(bih) );
    for (i = 0; i < 32 * 32; i++) put32( colour | 0xff000000 );
    for (i = 0; i < 32; i++) put32( 0 );                              /* AND mask: opaque */
    for (i = 0; i < extra; i++) buf[pos++] = 0;
    return pos - start;
}

static DWORD pixel( HCURSOR cursor, UINT step )
{
    BITMAPINFO bmi = { { sizeof(bmi.bmiHeader), 32, -32, 1, 32 } };
    DWORD *bits, ret;
    HDC dc = CreateCompatibleDC( 0 );
    HBITMAP bmp = CreateDIBSection( dc, &bmi, DIB_RGB_COLORS, (void **)&bits, NULL, 0 );

    SelectObject( dc, bmp );
    memset( bits, 0, 32 * 32 * 4 );
    DrawIconEx( dc, 0, 0, cursor, 32, 32, step, NULL, DI_NORMAL );
    ret = bits[16 * 32 + 16] & 0xffffff;
    DeleteDC( dc ); DeleteObject( bmp );
    return ret;
}

int main( void )
{
    DWORD riff_size, list_size, n, size_at, fails = 0;
    HCURSOR cursor;
    DWORD p0, p1;

    put( "RIFF", 4 ); riff_size = pos; put32( 0 ); put( "ACON", 4 );
    put( "anih", 4 ); put32( 36 );
    put32( 36 ); put32( 2 ); put32( 2 ); put32( 0 ); put32( 0 ); put32( 0 ); put32( 0 ); put32( 10 ); put32( 1 );
    put( "LIST", 4 ); list_size = pos; put32( 0 ); put( "fram", 4 );
    put( "icon", 4 ); size_at = pos; put32( 0 ); n = frame( 0xff0000, 1 ); memcpy( buf + size_at, &n, 4 ); buf[pos++] = 0; /* odd: pad */
    put( "icon", 4 ); size_at = pos; put32( 0 ); n = frame( 0x0000ff, 0 ); memcpy( buf + size_at, &n, 4 );
    n = pos - list_size - 4; memcpy( buf + list_size, &n, 4 );
    n = pos - riff_size - 4; memcpy( buf + riff_size, &n, 4 );

    cursor = CreateIconFromResourceEx( buf, pos, FALSE, 0x30000, 32, 32, 0 );
    printf( "%s  the animated cursor is made (%p)\n", cursor ? "PASS" : "FAIL", cursor );
    if (!cursor) { printf( "RESULT: FAIL\n" ); return 1; }
    p0 = pixel( cursor, 0 ); p1 = pixel( cursor, 1 );
    printf( "%s  step 0 is the first frame (red): %06lx\n", p0 == 0xff0000 ? "PASS" : "FAIL", p0 );
    if (p0 != 0xff0000) fails++;
    printf( "%s  step 1 is the frame after the odd-sized one (blue): %06lx\n", p1 == 0x0000ff ? "PASS" : "FAIL", p1 );
    if (p1 != 0x0000ff) fails++;
    printf( "RESULT: %s\n", fails ? "FAIL" : "PASS" );
    return fails != 0;
}
