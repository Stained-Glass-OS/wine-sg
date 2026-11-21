/* uspranges-probe: Uniscribe ScriptShapeOpenType / ScriptPlaceOpenType with several ranges of different
 * features, each applied to its own characters, and alternate substitution by the feature record's
 * lParameter (Stained Glass OS wine-sg 8233; before, the first range's features were used for the whole run
 * and an alternate was always the first).   uspranges-probe.exe FONT.ttf  (from uspranges-font.py)
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#include <windows.h>
#include <usp10.h>
#include <stdio.h>

#define TAG(a, b, c, d) ((DWORD)(a) | ((DWORD)(b) << 8) | ((DWORD)(c) << 16) | ((DWORD)(d) << 24))
enum { G_A = 2, G_B = 3, G_C = 4, G_a = 5, G_asc = 6, G_D = 7, G_alt1 = 8 };

static int failures;
static void check( int ok, const char *what, long got )
{
    if (ok) printf( "ok    %s\n", what );
    else { printf( "FAIL  %s (got %ld)\n", what, got ); failures++; }
}

static HDC hdc;
static SCRIPT_CACHE sc;
static SCRIPT_ITEM items[4];
static OPENTYPE_TAG script_tag;

static int shape( const WCHAR *text, int count, int *chars, TEXTRANGE_PROPERTIES **ranges, WORD *glyphs, WORD *clust )
{
    SCRIPT_CHARPROP cp[16];
    SCRIPT_GLYPHPROP gp[16];
    int n = 0;
    HRESULT hr = ScriptShapeOpenType( hdc, &sc, &items[0].a, script_tag, 0, chars, ranges, count, text,
                                      lstrlenW( text ), 16, clust, cp, glyphs, gp, &n );
    return hr == S_OK ? n : -1;
}

static int width( const WCHAR *text, int count, int *chars, TEXTRANGE_PROPERTIES **ranges )
{
    SCRIPT_CHARPROP cp[16];
    SCRIPT_GLYPHPROP gp[16];
    WORD clust[16], glyphs[16];
    int adv[16], n = 0, len = lstrlenW( text ), w = 0, i;
    GOFFSET off[16];
    ABC abc;

    ScriptShapeOpenType( hdc, &sc, &items[0].a, script_tag, 0, NULL, NULL, 0, text, len, 16, clust, cp, glyphs, gp, &n );
    if (ScriptPlaceOpenType( hdc, &sc, &items[0].a, script_tag, 0, chars, ranges, count, text, clust, cp, len,
                             glyphs, gp, n, adv, off, &abc ) != S_OK) return -1;
    for (i = 0; i < n; i++) w += adv[i];
    return w;
}

static int alternate( LONG param )
{
    OPENTYPE_FEATURE_RECORD rec = { TAG('s','a','l','t'), param };
    TEXTRANGE_PROPERTIES r = { &rec, 1 }, *ranges[1] = { &r };
    int chars[1] = { 1 };
    WORD g[4], clust[4];
    return shape( L"a", 1, chars, ranges, g, clust ) == 1 ? g[0] : -1;
}

int wmain( int argc, WCHAR **argv )
{
    OPENTYPE_FEATURE_RECORD no_liga = { TAG('l','i','g','a'), 0 }, smcp = { TAG('s','m','c','p'), 1 };
    OPENTYPE_FEATURE_RECORD no_kern = { TAG('k','e','r','n'), 0 };
    TEXTRANGE_PROPERTIES r_no_liga = { &no_liga, 1 }, r_smcp = { &smcp, 1 }, r_no_kern = { &no_kern, 1 }, r_none = { NULL, 0 };
    SCRIPT_CONTROL control = {0};
    SCRIPT_STATE state = {0};
    WORD g[16], clust[16];
    HFONT font;
    int n, w, nitems;

    if (argc < 2) return 2;
    check( AddFontResourceExW( argv[1], FR_PRIVATE, 0 ) > 0, "the test font", 0 );
    hdc = CreateCompatibleDC( NULL );
    font = CreateFontW( -1000, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, 0, 0, L"SG Range Test" );
    SelectObject( hdc, font );
    ScriptItemizeOpenType( L"AB", 2, 4, &control, &state, items, &script_tag, &nitems );

    {
        /* ABAB: liga off for the first two characters, the defaults for the last two */
        TEXTRANGE_PROPERTIES *ranges[2] = { &r_no_liga, &r_none };
        int chars[2] = { 2, 2 };
        n = shape( L"ABAB", 2, chars, ranges, g, clust );
        printf( "      ABAB: %d glyphs %d %d %d\n", n, g[0], g[1], g[2] );
        check( n == 3 && g[0] == G_A && g[1] == G_B && g[2] == G_C, "ABAB: no ligature in the first range, one in the second", n );
        check( n == 3 && clust[2] == 2 && clust[3] == 2, "the clusters of the ligature", clust[3] );
    }
    {
        /* the other way round */
        TEXTRANGE_PROPERTIES *ranges[2] = { &r_none, &r_no_liga };
        int chars[2] = { 2, 2 };
        n = shape( L"ABAB", 2, chars, ranges, g, clust );
        check( n == 3 && g[0] == G_C && g[1] == G_A && g[2] == G_B, "ABAB: a ligature in the first range only", n );
    }
    {
        /* aa: smcp on in the second range only */
        TEXTRANGE_PROPERTIES *ranges[2] = { &r_none, &r_smcp };
        int chars[2] = { 1, 1 };
        n = shape( L"aa", 2, chars, ranges, g, clust );
        check( n == 2 && g[0] == G_a && g[1] == G_asc, "aa: the small capital in the second range only", g[1] );
    }
    {
        /* ADAD: kern off for the first pair */
        TEXTRANGE_PROPERTIES *ranges[2] = { &r_no_kern, &r_none };
        int chars[2] = { 2, 2 };
        int all = width( L"ADAD", 0, NULL, NULL );
        w = width( L"ADAD", 2, chars, ranges );
        printf( "      ADAD widths: default %d, by range %d\n", all, w );
        /* 600-unit glyphs at a 1000-pixel em: 2400 unkerned, 2200 with both pairs kerned */
        check( abs( all - 2200 ) <= 4 && abs( w - 2300 ) <= 4, "ADAD: the second pair kerned, the first not", w );
    }
    /* salt: lParameter is the 1-based alternate */
    check( alternate( 1 ) == G_alt1, "salt 1: the first alternate", alternate( 1 ) );
    check( alternate( 2 ) == G_alt1 + 1, "salt 2: the second alternate", alternate( 2 ) );
    check( alternate( 3 ) == G_alt1 + 2, "salt 3: the third alternate", alternate( 3 ) );
    check( alternate( 4 ) == G_a, "salt 4 (beyond the set): the glyph stays", alternate( 4 ) );
    check( alternate( 0 ) == G_a, "salt 0: off", alternate( 0 ) );

    printf( "RESULT: %s\n", failures ? "FAIL" : "PASS" );
    return failures != 0;
}
