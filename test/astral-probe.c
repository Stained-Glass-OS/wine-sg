/* astral-probe: characters beyond the BMP (a UTF-16 surrogate pair) in GDI
 * text -- patches/sg/0171.
 *
 * Draws into a 32-bpp DIB section with ExtTextOutW (no glyph indices) and
 * compares the pixels: two different emoji must differ from each other and
 * from two missing-glyph boxes (glyph 0 twice, drawn by index), the pair must
 * be one advance wide with nothing for its second half, a font without
 * emoji must fall back to one that has them (the same glyph: its ink, as
 * it sits on the base font's baseline), a path gets the outline, and
 * Uniscribe's ScriptString functions (what edit controls draw with) fall
 * back too.
 *
 *   astral-probe FACE        e.g. Symbola (a font with the characters)
 *   prints key=value lines; see test/astral-gate.sh
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include <windows.h>
#include <usp10.h>
#include <stdio.h>

#define W 160
#define H 140

static HDC dc;
static DWORD *bits;

static HFONT make_font( const WCHAR *face )
{
    LOGFONTW lf = { 0 };
    lf.lfHeight = -48;
    lf.lfQuality = ANTIALIASED_QUALITY;
    lf.lfCharSet = DEFAULT_CHARSET;
    lstrcpynW( lf.lfFaceName, face, LF_FACESIZE );
    return CreateFontIndirectW( &lf );
}

/* draw and return a hash of the ink, and how many pixels are inked */
static unsigned int draw( HFONT font, UINT flags, const WCHAR *str, UINT count, int *ink )
{
    RECT rc = { 0, 0, W, H };
    unsigned int hash = 2166136261u;
    int i;
    HGDIOBJ old = SelectObject( dc, font );
    FillRect( dc, &rc, GetStockObject( WHITE_BRUSH ) );
    SetTextColor( dc, RGB(0, 0, 0) );
    SetBkMode( dc, TRANSPARENT );
    ExtTextOutW( dc, 10, 30, flags, NULL, str, count, NULL );
    GdiFlush();
    *ink = 0;
    for (i = 0; i < W * H; i++)
    {
        DWORD p = bits[i] & 0xffffff;
        if (p != 0xffffff) (*ink)++;
        hash = (hash ^ p) * 16777619u;
    }
    SelectObject( dc, old );
    return hash;
}

int main( int argc, char **argv )
{
    WCHAR face[LF_FACESIZE];
    BITMAPINFO bmi = { { sizeof(bmi.bmiHeader), W, -H, 1, 32, BI_RGB } };
    static const WCHAR grin[] = { 0xd83d, 0xde00 };   /* U+1F600 */
    static const WCHAR heart[] = { 0xd83d, 0xde0d };  /* U+1F60D */
    static const WCHAR clef[] = { 0xd835, 0xdc00 };   /* U+1D400, a bold A */
    static const WCHAR aclef[] = { 'A', 0xd835, 0xdc00, 'B' };
    static const WORD notdef[2] = { 0, 0 };   /* what a pair drew: two boxes */
    HFONT font, plain;
    unsigned int h_grin, h_heart, h_box, h_clef, h_fb;
    int ink, ink_grin, ink_box, ink_fb, dx[4];
    SIZE sz, sz_a, sz_b;
    TEXTMETRICW tm;
    WCHAR got[LF_FACESIZE];

    MultiByteToWideChar( CP_ACP, 0, argc > 1 ? argv[1] : "Symbola", -1, face, LF_FACESIZE );
    dc = CreateCompatibleDC( 0 );
    SelectObject( dc, CreateDIBSection( dc, &bmi, DIB_RGB_COLORS, (void **)&bits, NULL, 0 ) );

    font = make_font( face );
    SelectObject( dc, font );
    GetTextFaceW( dc, LF_FACESIZE, got );
    GetTextMetricsW( dc, &tm );
    printf( "face=%ls\n", got );

    h_grin = draw( font, 0, grin, 2, &ink_grin );
    h_heart = draw( font, 0, heart, 2, &ink );
    h_box = draw( font, ETO_GLYPH_INDEX, notdef, 2, &ink_box );
    h_clef = draw( font, 0, clef, 2, &ink );
    printf( "inked=%d\n", ink_grin > 50 );
    printf( "emoji_differ=%d\n", h_grin != h_heart && h_grin != h_clef && h_heart != h_clef );
    printf( "not_box=%d\n", h_grin != h_box && h_heart != h_box && h_clef != h_box );

    /* one advance: the pair's second half adds nothing */
    SelectObject( dc, font );
    GetTextExtentExPointW( dc, clef, 2, 0, NULL, dx, &sz );
    printf( "extent_pair=%d\n", sz.cx > 0 && dx[0] == dx[1] && dx[1] == sz.cx );
    GetTextExtentPoint32W( dc, L"A", 1, &sz_a );
    GetTextExtentPoint32W( dc, L"B", 1, &sz_b );
    GetTextExtentExPointW( dc, aclef, 4, 0, NULL, dx, &sz );
    printf( "extent_mixed=%d\n", dx[0] == sz_a.cx && dx[1] == dx[2] && dx[1] > dx[0] &&
            dx[3] == dx[2] + sz_b.cx && sz.cx == dx[3] );

    /* a font without them falls back to one that has them */
    plain = make_font( L"Liberation Sans" );
    h_fb = draw( plain, 0, grin, 2, &ink_fb );
    {
        int ink2; unsigned int plain_box = draw( plain, ETO_GLYPH_INDEX, notdef, 2, &ink2 );
        printf( "fallback=%d\n", h_fb != plain_box && ink_fb > 50 );
        /* the same glyph (on the base font's baseline, so compare the ink) */
        printf( "fallback_same=%d\n", ink_fb == ink_grin );
    }

    /* a path gets the character's outline, not a box's */
    {
        int n1, n2;
        SelectObject( dc, font );
        BeginPath( dc ); ExtTextOutW( dc, 10, 10, 0, NULL, grin, 2, NULL ); EndPath( dc );
        n1 = GetPath( dc, NULL, NULL, 0 );
        BeginPath( dc ); ExtTextOutW( dc, 10, 10, ETO_GLYPH_INDEX, NULL, notdef, 2, NULL ); EndPath( dc );
        n2 = GetPath( dc, NULL, NULL, 0 );
        printf( "path=%d\n", n1 > 0 && n1 != n2 );
    }

    /* Uniscribe, as an edit control uses it: the run of emoji falls back */
    {
        SCRIPT_STRING_ANALYSIS ssa;
        RECT rc = { 0, 0, W, H };
        int i, ink_ss = 0;
        HRESULT hr;

        SelectObject( dc, plain );
        FillRect( dc, &rc, GetStockObject( WHITE_BRUSH ) );
        hr = ScriptStringAnalyse( dc, grin, 2, 16, -1, SSA_LINK | SSA_FALLBACK | SSA_GLYPHS, -1,
                                  NULL, NULL, NULL, NULL, NULL, &ssa );
        if (SUCCEEDED( hr ))
        {
            ScriptStringOut( ssa, 10, 30, 0, NULL, 0, 0, FALSE );
            ScriptStringFree( &ssa );
        }
        GdiFlush();
        for (i = 0; i < W * H; i++) if ((bits[i] & 0xffffff) != 0xffffff) ink_ss++;
        printf( "uniscribe=%d\n", SUCCEEDED( hr ) && ink_ss == ink_grin );
    }

    /* CJK Extension B (U+20000) from a font without it: the installed font
     * that has it (HanaMinB), when there is one */
    {
        static const WCHAR extb[] = { 0xd840, 0xdc00 };
        HFONT hana = make_font( L"HanaMinB" );
        WCHAR hface[LF_FACESIZE];
        int ink_hana, ink_fbb, ink_box2;
        unsigned int h_hana, h_fbb, h_box2;

        SelectObject( dc, hana );
        GetTextFaceW( dc, LF_FACESIZE, hface );
        if (!lstrcmpiW( hface, L"HanaMinB" ))
        {
            h_hana = draw( hana, 0, extb, 2, &ink_hana );
            h_fbb = draw( plain, 0, extb, 2, &ink_fbb );
            h_box2 = draw( plain, ETO_GLYPH_INDEX, notdef, 2, &ink_box2 );
            printf( "cjkb_fallback=%d\n", h_fbb != h_box2 && ink_fbb == ink_hana && ink_hana > 50 );
        }
        else printf( "cjkb_fallback=no-font\n" );
    }

    /* the API that takes one UINT keeps Windows' behaviour: the high word is ignored */
    {
        GLYPHMETRICS gm1, gm2;
        static const MAT2 id = { {0,1}, {0,0}, {0,0}, {0,1} };
        SelectObject( dc, font );
        GetGlyphOutlineW( dc, 'A', GGO_METRICS, &gm1, 0, NULL, &id );
        GetGlyphOutlineW( dc, 0x10000 + 'A', GGO_METRICS, &gm2, 0, NULL, &id );
        printf( "ggo_highword=%d\n", !memcmp( &gm1, &gm2, sizeof(gm1) ) );
    }
    return 0;
}
