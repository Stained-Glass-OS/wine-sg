/* t2mtx-probe: TTLoadEmbeddedFont of MicroType Express compressed EOT fonts (Stained Glass OS wine-sg 8231;
 * test/t2mtx-gate.sh). It was E_ERRORDECOMPRESSINGFONTDATA. argv: the compressed EOT, the same one
 * XOR-encrypted, a corrupt one, and where to write the loaded font's data (GetFontData of the whole font),
 * which the gate compares with the font the EOT was made from.
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#include <windows.h>
#include <t2embapi.h>
#include <stdio.h>

#ifndef E_ERRORDECOMPRESSINGFONTDATA
#define E_ERRORDECOMPRESSINGFONTDATA 0x0111
#endif

static int failures;
static void check( int ok, const char *what, long got )
{
    printf( "%s  %s (%#lx)\n", ok ? "PASS" : "FAIL", what, got );
    if (!ok) failures++;
}

struct mem { BYTE *data; ULONG size, pos; };

static ULONG __cdecl read_proc( void *stream, void *buf, ULONG len )
{
    struct mem *m = stream;
    if (len > m->size - m->pos) len = m->size - m->pos;
    memcpy( buf, m->data + m->pos, len );
    m->pos += len;
    return len;
}

static BOOL load_file( const char *path, struct mem *m )
{
    HANDLE f = CreateFileA( path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL );
    DWORD got;
    if (f == INVALID_HANDLE_VALUE) return FALSE;
    m->size = GetFileSize( f, NULL );
    m->data = HeapAlloc( GetProcessHeap(), 0, m->size );
    m->pos = 0;
    ReadFile( f, m->data, m->size, &got, NULL );
    CloseHandle( f );
    return got == m->size;
}

static LONG load( struct mem *m, HANDLE *ref )
{
    ULONG priv = 0, status = 0;
    m->pos = 0;
    *ref = NULL;
    return TTLoadEmbeddedFont( ref, 0, &priv, LICENSE_PREVIEWPRINT, &status, read_proc, m, NULL, NULL, NULL );
}

int main( int argc, char **argv )
{
    struct mem eot, xored, corrupt;
    HANDLE ref = NULL, ref2 = NULL, ref3 = NULL;
    ULONG status;
    LONG ret;

    if (argc < 5 || !load_file( argv[1], &eot ) || !load_file( argv[2], &xored ) || !load_file( argv[3], &corrupt ))
    {
        printf( "FAIL  the test files\nRESULT: FAIL\n" );
        return 1;
    }
    ret = load( &eot, &ref );
    check( ret == E_NONE && ref, "TTLoadEmbeddedFont of an MTX compressed font", ret );
    if (ref)
    {
        HDC hdc = CreateCompatibleDC( NULL );
        HFONT font = CreateFontA( -40, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, 0, 0, "SG MTX Test" );
        char face[64] = "";
        DWORD size;
        GLYPHMETRICS gm;
        MAT2 mat = { {0, 1}, {0, 0}, {0, 0}, {0, 1} };

        SelectObject( hdc, font );
        GetTextFaceA( hdc, sizeof(face), face );
        check( !strcmp( face, "SG MTX Test" ), "the decompressed font can be selected", 0 );
        size = GetFontData( hdc, 0, 0, NULL, 0 );
        check( size != GDI_ERROR && size > 0, "GetFontData of the whole font", size );
        if (size != GDI_ERROR && size)
        {
            BYTE *buf = HeapAlloc( GetProcessHeap(), 0, size );
            HANDLE f = CreateFileA( argv[4], GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL );
            DWORD written;
            GetFontData( hdc, 0, 0, buf, size );
            WriteFile( f, buf, size, &written, NULL );
            CloseHandle( f );
        }
        ret = GetGlyphOutlineA( hdc, 'A', GGO_METRICS, &gm, 0, NULL, &mat );
        check( ret != GDI_ERROR && gm.gmBlackBoxX > 10, "a glyph of it has an outline", gm.gmBlackBoxX );
        DeleteDC( hdc );
        DeleteObject( font );
    }
    ret = load( &xored, &ref2 );
    check( ret == E_NONE && ref2, "an XOR-encrypted MTX font (decrypted before it is decompressed)", ret );
    ret = load( &corrupt, &ref3 );
    check( ret == E_ERRORDECOMPRESSINGFONTDATA && !ref3, "corrupt MTX data: E_ERRORDECOMPRESSINGFONTDATA", ret );
    if (ref) TTDeleteEmbeddedFont( ref, 0, &status );
    if (ref2) TTDeleteEmbeddedFont( ref2, 0, &status );
    printf( "RESULT: %s\n", failures ? "FAIL" : "PASS" );
    return failures != 0;
}
