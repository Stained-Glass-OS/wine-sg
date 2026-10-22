/* uspfeat-probe: Uniscribe ScriptShapeOpenType / ScriptPlaceOpenType honour the
 * range properties (OpenType features turned on or off) (Stained Glass OS
 * wine-sg 6008).   uspfeat-probe.exe FONT.ttf   (from uspfeat-font.py) */
#include <windows.h>
#include <usp10.h>
#include <stdio.h>

#define MS_MAKE_TAG(a, b, c, d) ((DWORD)(a) | ((DWORD)(b) << 8) | ((DWORD)(c) << 16) | ((DWORD)(d) << 24))

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

static int shape( const WCHAR *text, TEXTRANGE_PROPERTIES *range, WORD *glyphs )
{
    SCRIPT_CHARPROP cp[16];
    SCRIPT_GLYPHPROP gp[16];
    WORD clust[16];
    int n = 0, count = 1, len = lstrlenW( text );
    TEXTRANGE_PROPERTIES *ranges[1] = { range };
    int range_chars[1] = { len };
    HRESULT hr;

    hr = ScriptShapeOpenType( hdc, &sc, &items[0].a, script_tag, 0, range ? range_chars : NULL, range ? ranges : NULL,
            range ? count : 0, text, len, 16, clust, cp, glyphs, gp, &n );
    return hr == S_OK ? n : -1;
}

static int place_width( const WCHAR *text, TEXTRANGE_PROPERTIES *range )
{
    SCRIPT_CHARPROP cp[16];
    SCRIPT_GLYPHPROP gp[16];
    WORD clust[16], glyphs[16];
    int adv[16], n = 0, len = lstrlenW( text ), w = 0, i;
    GOFFSET off[16];
    ABC abc;
    TEXTRANGE_PROPERTIES *ranges[1] = { range };
    int range_chars[1] = { len };

    ScriptShapeOpenType( hdc, &sc, &items[0].a, script_tag, 0, NULL, NULL, 0, text, len, 16, clust, cp, glyphs, gp, &n );
    if (ScriptPlaceOpenType( hdc, &sc, &items[0].a, script_tag, 0, range ? range_chars : NULL, range ? ranges : NULL,
            range ? 1 : 0, text, clust, cp, len, glyphs, gp, n, adv, off, &abc ) != S_OK) return -1;
    for (i = 0; i < n; i++) w += adv[i];
    return w;
}

int wmain( int argc, WCHAR **argv )
{
    OPENTYPE_FEATURE_RECORD no_liga[] = { { MS_MAKE_TAG('l','i','g','a'), 0 } };
    OPENTYPE_FEATURE_RECORD smcp[] = { { MS_MAKE_TAG('s','m','c','p'), 1 } };
    OPENTYPE_FEATURE_RECORD no_kern[] = { { MS_MAKE_TAG('k','e','r','n'), 0 } };
    TEXTRANGE_PROPERTIES r_no_liga = { no_liga, 1 }, r_smcp = { smcp, 1 }, r_no_kern = { no_kern, 1 };
    SCRIPT_CONTROL control = {0};
    SCRIPT_STATE state = {0};
    WORD g1[16], g2[16];
    HFONT font;
    int n, n2, w1, w2, nitems;

    if (argc < 2) return 2;
    check( AddFontResourceExW( argv[1], FR_PRIVATE, 0 ) > 0, "the test font", 0 );
    hdc = CreateCompatibleDC( NULL );
    font = CreateFontW( -100, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, 0, 0, L"SG Feature Test" );
    SelectObject( hdc, font );
    ScriptItemizeOpenType( L"AB", 2, 4, &control, &state, items, &script_tag, &nitems );

    /* liga: on by default, off by a range */
    n = shape( L"AB", NULL, g1 );
    check( n == 1, "AB: the default ligature (one glyph)", n );
    n2 = shape( L"AB", &r_no_liga, g2 );
    check( n2 == 2, "AB with liga 0: two glyphs", n2 );
    check( n == 1 && n2 == 2 && g1[0] != g2[0], "the ligature glyph is another glyph", g1[0] );

    /* smcp: off by default, on by a range */
    n = shape( L"a", NULL, g1 );
    n2 = shape( L"a", &r_smcp, g2 );
    check( n == 1 && n2 == 1 && g1[0] != g2[0], "a with smcp 1: the small capital", g2[0] );

    /* kern: a default position feature, off by a range */
    w1 = place_width( L"AD", NULL );
    w2 = place_width( L"AD", &r_no_kern );
    check( w1 > 0 && w2 > w1, "AD kerned by default, not with kern 0", w2 - w1 );

    printf( "RESULT: %s\n", failures ? "FAIL" : "PASS" );
    return failures != 0;
}
