/* gdigamut-probe: gdi32 CheckColorsInGamut and ColorCorrectPalette, which were spec stubs (Stained Glass OS
 * wine-sg 8244; test/gdigamut-gate.sh).
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#include <windows.h>
#include <stdio.h>

static int failures;
static void check( int ok, const char *what, long got )
{
    printf( "%s  %s (%#lx)\n", ok ? "PASS" : "FAIL", what, got );
    if (!ok) failures++;
}

int main( void )
{
    HMODULE gdi = GetModuleHandleA( "gdi32.dll" );
    BOOL (WINAPI *pGamut)(HDC, void *, void *, DWORD) = (void *)GetProcAddress( gdi, "CheckColorsInGamut" );
    BOOL (WINAPI *pCorrect)(HDC, HPALETTE, DWORD, DWORD) = (void *)GetProcAddress( gdi, "ColorCorrectPalette" );
    RGBTRIPLE colors[3] = { { 0, 0, 255 }, { 0, 255, 0 }, { 255, 0, 0 } };
    BYTE result[3] = { 0xcc, 0xcc, 0xcc };
    struct { WORD ver, count; PALETTEENTRY e[4]; } lp = { 0x300, 4 };
    PALETTEENTRY back[4];
    HDC dc = CreateCompatibleDC( NULL );
    HPALETTE pal;
    BOOL ret;
    int i;

    for (i = 0; i < 4; i++) { lp.e[i].peRed = i * 40; lp.e[i].peGreen = 255 - i * 40; lp.e[i].peBlue = 7 * i; }
    pal = CreatePalette( (LOGPALETTE *)&lp );
    ret = pGamut( dc, colors, result, 3 );
    check( ret && !result[0] && !result[1] && !result[2], "CheckColorsInGamut: every colour in gamut", ret );
    SetLastError( 0 );
    check( !pGamut( NULL, colors, result, 3 ) && GetLastError() == ERROR_INVALID_PARAMETER, "no DC: ERROR_INVALID_PARAMETER", GetLastError() );
    ret = pCorrect( dc, pal, 1, 3 );
    GetPaletteEntries( pal, 0, 4, back );
    check( ret && !memcmp( back, lp.e, sizeof(back) ), "ColorCorrectPalette: the entries kept (no transform)", ret );
    check( !pCorrect( dc, pal, 2, 3 ), "a range past the palette fails", 0 );
    check( !pCorrect( dc, (HPALETTE)dc, 0, 1 ), "something that is no palette fails", 0 );
    printf( "RESULT: %s\n", failures ? "FAIL" : "PASS" );
    return failures != 0;
}
