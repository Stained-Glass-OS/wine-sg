/* eot-probe: t2embed embeds fonts in documents and loads them back (Stained
 * Glass OS wine-sg 6009): TTEmbedFont, TTEmbedFontFromFileA,
 * TTGetEmbeddedFontInfo, TTLoadEmbeddedFont (with a new family name),
 * TTDeleteEmbeddedFont, permissions and XOR encryption.
 *   eot-probe.exe FONT.ttf LOCKED.ttf   (from uspfeat-font.py: "SG Feature Test"
 *   installable, "SG Locked Font" with fsType 2, restricted) */
#include <windows.h>
#include <t2embapi.h>
#include <stdio.h>

#ifndef TTEMBED_RAW
#define TTEMBED_RAW 0
#endif
#ifndef TTEMBED_XORENCRYPTDATA
#define TTEMBED_XORENCRYPTDATA 0x10000000
#endif
#ifndef E_FONTNOTEMBEDDABLE
#define E_FONTNOTEMBEDDABLE 0x0104
#endif
LONG WINAPI TTEmbedFont(HDC,ULONG,ULONG,ULONG*,ULONG*,WRITEEMBEDPROC,LPVOID,USHORT*,USHORT,USHORT,TTEMBEDINFO*);
LONG WINAPI TTEmbedFontFromFileA(HDC,LPCSTR,USHORT,ULONG,ULONG,ULONG*,ULONG*,WRITEEMBEDPROC,LPVOID,USHORT*,USHORT,USHORT,TTEMBEDINFO*);
LONG WINAPI TTGetEmbeddedFontInfo(ULONG,ULONG*,ULONG,ULONG*,READEMBEDPROC,LPVOID,TTLOADINFO*);

static int failures;

static void check( int ok, const char *what, long got )
{
    if (ok) printf( "ok    %s\n", what );
    else { printf( "FAIL  %s (got %#lx)\n", what, got ); failures++; }
}

struct mem { BYTE data[65536]; ULONG size, pos; };

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

static struct mem plain, xored, fromfile;

static HDC dc_with_font( const WCHAR *face )
{
    HDC hdc = CreateCompatibleDC( NULL );
    SelectObject( hdc, CreateFontW( -40, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, 0, 0, face ) );
    return hdc;
}

static BOOL face_is( const WCHAR *face )
{
    WCHAR got[LF_FACESIZE] = {0};
    HDC hdc = dc_with_font( face );
    GetTextFaceW( hdc, LF_FACESIZE, got );
    DeleteDC( hdc );
    return !lstrcmpW( got, face );
}

int wmain( int argc, WCHAR **argv )
{
    ULONG priv = 0, status = 99;
    HANDLE ref = NULL;
    HDC hdc;
    LONG ret;
    char path[MAX_PATH];

    if (argc < 3) return 2;
    AddFontResourceExW( argv[1], FR_PRIVATE, 0 );
    AddFontResourceExW( argv[2], FR_PRIVATE, 0 );

    hdc = dc_with_font( L"SG Feature Test" );
    ret = TTEmbedFont( hdc, TTEMBED_RAW, CHARSET_UNICODE, &priv, &status, write_proc, &plain, NULL, 0, 0, NULL );
    check( ret == E_NONE && plain.size > 100, "TTEmbedFont", ret );
    check( priv == EMBED_INSTALLABLE && status == 0, "an installable font, not subsetted", priv );
    check( plain.size == *(ULONG *)plain.data && *(USHORT *)(plain.data + 34) == 0x504c,
           "the structure: its size and the EOT magic number", *(USHORT *)(plain.data + 34) );
    check( *(ULONG *)(plain.data + 4) < plain.size &&
           !memcmp( plain.data + plain.size - *(ULONG *)(plain.data + 4), "\0\1\0\0", 4 ),
           "the font data at its end (a TrueType font)", *(ULONG *)(plain.data + 4) );
    ret = TTEmbedFont( hdc, TTEMBED_XORENCRYPTDATA, CHARSET_UNICODE, &priv, &status, write_proc, &xored, NULL, 0, 0, NULL );
    check( ret == E_NONE && xored.size == plain.size && (*(ULONG *)(xored.data + 12) & TTEMBED_XORENCRYPTDATA) &&
           xored.data[xored.size - *(ULONG *)(xored.data + 4)] == (0x00 ^ 0x50), "TTEMBED_XORENCRYPTDATA", ret );
    check( TTEmbedFont( hdc, TTEMBED_RAW, CHARSET_UNICODE, NULL, &status, write_proc, &xored, NULL, 0, 0, NULL ) != E_NONE,
           "no privilege status pointer: an error", 0 );
    DeleteDC( hdc );

    hdc = dc_with_font( L"SG Locked Font" );
    {
        struct mem *locked = calloc( 1, sizeof(*locked) );
        ret = TTEmbedFont( hdc, TTEMBED_RAW, CHARSET_UNICODE, &priv, &status, write_proc, locked, NULL, 0, 0, NULL );
        check( ret == E_FONTNOTEMBEDDABLE && priv == EMBED_NOEMBEDDING, "a restricted font is not embedded", ret );
        free( locked );
    }
    DeleteDC( hdc );

    WideCharToMultiByte( CP_ACP, 0, argv[1], -1, path, MAX_PATH, NULL, NULL );
    ret = TTEmbedFontFromFileA( NULL, path, 0, TTEMBED_RAW, CHARSET_UNICODE, &priv, &status, write_proc, &fromfile,
                                NULL, 0, 0, NULL );
    check( ret == E_NONE && fromfile.size == plain.size, "TTEmbedFontFromFileA", ret );

    plain.pos = 0;
    ret = TTGetEmbeddedFontInfo( 0, &priv, EMBED_EDITABLE, &status, read_proc, &plain, NULL );
    check( ret == E_NONE && priv == EMBED_EDITABLE, "TTGetEmbeddedFontInfo (the more restrictive level)", priv );

    /* loaded back, under a name of its own */
    check( !face_is( L"SG Embedded Copy" ), "the new name is not a font yet", 0 );
    xored.pos = 0;
    ret = TTLoadEmbeddedFont( &ref, TTLOAD_PRIVATE, &priv, EMBED_INSTALLABLE, &status, read_proc, &xored,
                              (WCHAR *)L"SG Embedded Copy", NULL, NULL );
    check( ret == E_NONE && ref, "TTLoadEmbeddedFont (encrypted data, renamed)", ret );
    check( priv == EMBED_INSTALLABLE, "its embedding level", priv );
    check( face_is( L"SG Embedded Copy" ), "the loaded font is there under its new name", 0 );
    ret = TTDeleteEmbeddedFont( ref, 0, &status );
    check( ret == E_NONE, "TTDeleteEmbeddedFont", ret );
    check( !face_is( L"SG Embedded Copy" ), "and it is gone", 0 );
    check( TTDeleteEmbeddedFont( ref, 0, &status ) != E_NONE, "deleting it again fails", 0 );

    plain.pos = 0;
    plain.data[34] = 0;
    ret = TTLoadEmbeddedFont( &ref, TTLOAD_PRIVATE, &priv, EMBED_INSTALLABLE, &status, read_proc, &plain, NULL, NULL, NULL );
    check( ret != E_NONE, "a structure without the magic number is refused", ret );

    printf( "RESULT: %s\n", failures ? "FAIL" : "PASS" );
    return failures != 0;
}
