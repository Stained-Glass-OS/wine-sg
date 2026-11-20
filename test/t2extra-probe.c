/* t2extra-probe: the t2embed functions that were spec stubs (Stained Glass OS wine-sg 8225): TTEmbedFontEx,
 * TTGetNewFontName, TTCharToUnicode, TTRunValidationTests(Ex). Prints ok/FAIL lines and RESULT.
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#include <windows.h>
#include <t2embapi.h>
#include <stdio.h>

#ifndef E_CHARCODECOUNTINVALID
#define E_CHARCODECOUNTINVALID 0x0002
#define E_FONTREFERENCEINVALID 0x0008
#define E_INPUTPARAMINVALID 0x0019
#endif
#ifndef TTEMBED_RAW
#define TTEMBED_RAW 0
#endif

typedef struct { ULONG ulStructSize; LONG lTestFromSize, lTestToSize; ULONG ulCharSet; USHORT usReserved1, usCharCodeCount; USHORT *pusCharCodeSet; } VPARAMS;
typedef struct { ULONG ulStructSize; LONG lTestFromSize, lTestToSize; ULONG ulCharSet; USHORT usReserved1, usCharCodeCount; ULONG *pulCharCodeSet; } VPARAMSEX;

static int failures;
static void check( int ok, const char *what, long got )
{
    if (ok) printf( "ok    %s\n", what );
    else { printf( "FAIL  %s (got %#lx)\n", what, got ); failures++; }
}

struct mem { BYTE data[1 << 20]; ULONG size, pos; };
static struct mem a, b;

static ULONG __cdecl write_proc( void *stream, void *buf, ULONG len )
{
    struct mem *m = stream;
    if (m->size + len > sizeof(m->data)) return 0;
    memcpy( m->data + m->size, buf, len );
    m->size += len;
    return len;
}

static ULONG __cdecl read_proc( void *stream, void *buf, ULONG len )
{
    struct mem *m = stream;
    if (len > m->size - m->pos) len = m->size - m->pos;
    memcpy( buf, m->data + m->pos, len );
    m->pos += len;
    return len;
}

static HDC dc_with( const WCHAR *face, BYTE charset )
{
    HDC hdc = CreateCompatibleDC( NULL );
    SelectObject( hdc, CreateFontW( -20, 0, 0, 0, FW_NORMAL, 0, 0, 0, charset, 0, 0, 0, 0, face ) );
    return hdc;
}

int main( void )
{
    HMODULE t2 = LoadLibraryA( "t2embed.dll" );
    LONG (WINAPI *pEmbedEx)(HDC,ULONG,ULONG,ULONG*,ULONG*,WRITEEMBEDPROC,LPVOID,ULONG*,USHORT,USHORT,TTEMBEDINFO*) = (void *)GetProcAddress( t2, "TTEmbedFontEx" );
    LONG (WINAPI *pEmbed)(HDC,ULONG,ULONG,ULONG*,ULONG*,WRITEEMBEDPROC,LPVOID,USHORT*,USHORT,USHORT,TTEMBEDINFO*) = (void *)GetProcAddress( t2, "TTEmbedFont" );
    LONG (WINAPI *pLoad)(HANDLE*,ULONG,ULONG*,ULONG,ULONG*,READEMBEDPROC,LPVOID,LPWSTR,LPSTR,TTLOADINFO*) = (void *)GetProcAddress( t2, "TTLoadEmbeddedFont" );
    LONG (WINAPI *pDelete)(HANDLE,ULONG,ULONG*) = (void *)GetProcAddress( t2, "TTDeleteEmbeddedFont" );
    LONG (WINAPI *pNewName)(HANDLE*,LPWSTR,LONG,LPSTR,LONG) = (void *)GetProcAddress( t2, "TTGetNewFontName" );
    LONG (WINAPI *pCharToUnicode)(HDC,UCHAR*,ULONG,USHORT*,ULONG,ULONG) = (void *)GetProcAddress( t2, "TTCharToUnicode" );
    LONG (WINAPI *pValidate)(HDC,VPARAMS*) = (void *)GetProcAddress( t2, "TTRunValidationTests" );
    LONG (WINAPI *pValidateEx)(HDC,VPARAMSEX*) = (void *)GetProcAddress( t2, "TTRunValidationTestsEx" );
    HDC tahoma = dc_with( L"Tahoma", ANSI_CHARSET ), symbol = dc_with( L"Wingdings", SYMBOL_CHARSET );
    USHORT chars16[] = { 'A', 'B' }, missing16[] = { 0xe123 }, out[4];
    ULONG chars32[] = { 'A', 'B' }, priv = 0, status = 0;
    UCHAR bytes[] = { 0x41, 0x80 };
    VPARAMS vp = { sizeof(vp), 8, 12, 0, 0, 2, chars16 };
    VPARAMSEX vpx = { sizeof(vpx), 8, 12, 0, 0, 2, chars32 };
    HANDLE ref = NULL, ref2 = NULL;
    WCHAR name[64];
    LONG ret;

    check( pEmbedEx && pNewName && pCharToUnicode && pValidate && pValidateEx, "exports", 0 );
    if (!pEmbedEx || !pNewName || !pCharToUnicode || !pValidate || !pValidateEx) goto done;

    ret = pValidate( tahoma, &vp );
    check( ret == E_NONE, "TTRunValidationTests: Tahoma's A and B at 8-12", ret );
    vp.pusCharCodeSet = missing16; vp.usCharCodeCount = 1;
    ret = pValidate( tahoma, &vp );
    check( ret != E_NONE, "TTRunValidationTests: a character the font lacks fails", ret );
    ret = pValidate( NULL, &vp );
    check( ret == E_HDCINVALID, "TTRunValidationTests: no DC", ret );
    ret = pValidateEx( tahoma, &vpx );
    check( ret == E_NONE, "TTRunValidationTestsEx: UCS-4 codes", ret );

    memset( out, 0, sizeof(out) );
    ret = pCharToUnicode( tahoma, bytes, 2, out, 4, 0 );
    check( ret == E_NONE && out[0] == 0x41 && out[1] == 0x20ac, "TTCharToUnicode: an ANSI font through code page 1252", out[1] );
    ret = pCharToUnicode( symbol, bytes, 1, out, 4, 0 );
    check( ret == E_NONE && out[0] == 0xf041, "TTCharToUnicode: a symbol font in U+F0xx", out[0] );
    ret = pCharToUnicode( tahoma, bytes, 2, out, 1, 0 );
    check( ret == E_CHARCODECOUNTINVALID, "TTCharToUnicode: too small an output", ret );

    ret = pEmbedEx( tahoma, TTEMBED_RAW, CHARSET_UNICODE, &priv, &status, write_proc, &a, chars32, 2, 0, NULL );
    check( ret == E_NONE && a.size > 1000, "TTEmbedFontEx embeds the font", ret );
    ret = pEmbed( tahoma, TTEMBED_RAW, CHARSET_UNICODE, &priv, &status, write_proc, &b, chars16, 2, 0, NULL );
    check( ret == E_NONE && a.size == b.size, "the same stream as TTEmbedFont", (long)a.size - (long)b.size );

    a.pos = 0;
    ret = pLoad( &ref, 0, &priv, LICENSE_PREVIEWPRINT, &status, read_proc, &a, (WCHAR *)L"SG Renamed Face", NULL, NULL );
    check( ret == E_NONE && ref, "TTLoadEmbeddedFont with a new name", ret );
    memset( name, 0, sizeof(name) );
    ret = pNewName( &ref, name, 64, NULL, 0 );
    check( ret == E_NONE && !lstrcmpW( name, L"SG Renamed Face" ), "TTGetNewFontName gives the new name", ret );
    {
        /* the renamed font is there to select (Tahoma has embedded bitmaps: FreeType needs its style name kept) */
        HDC dc = dc_with( L"SG Renamed Face", DEFAULT_CHARSET );
        WCHAR face[64] = {0};
        GetTextFaceW( dc, 64, face );
        check( !lstrcmpW( face, L"SG Renamed Face" ), "the renamed font can be selected", 0 );
        DeleteDC( dc );
    }
    b.pos = 0;
    ret = pLoad( &ref2, 0, &priv, LICENSE_PREVIEWPRINT, &status, read_proc, &b, NULL, NULL, NULL );
    memset( name, 0, sizeof(name) );
    ret = pNewName( &ref2, name, 64, NULL, 0 );
    printf( "      unrenamed: %ls\n", name );
    check( ret == E_NONE && !lstrcmpW( name, L"Tahoma" ), "TTGetNewFontName without a rename: the font's own family", ret );
    ret = pNewName( &ref, name, 4, NULL, 0 );
    check( ret == E_INPUTPARAMINVALID, "TTGetNewFontName: too small a buffer", ret );
    pDelete( ref, 0, &status );
    ret = pNewName( &ref, name, 64, NULL, 0 );
    check( ret == E_FONTREFERENCEINVALID, "TTGetNewFontName after TTDeleteEmbeddedFont", ret );
    pDelete( ref2, 0, &status );

done:
    printf( "RESULT: %s\n", failures ? "FAIL" : "PASS" );
    return failures != 0;
}
