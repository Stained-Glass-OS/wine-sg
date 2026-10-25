/* cffoutline-probe: GetGlyphOutline of a PostScript-flavoured OpenType font (CFF outlines: cubic
 * curves), as cairo (RawTherapee, GIMP's win32 builds) and GDI+ path text read it (patches/sg/6537).
 * GGO_NATIVE must give the cubic curves as TT_PRIM_CSPLINE, GGO_BEZIER the same; the glyph drawn
 * from either outline must cover what TextOut draws.
 * "probe FACE": the face to test (default Inter). Prints name=value lines.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

static const MAT2 identity = { {0,1}, {0,0}, {0,0}, {0,1} };

/* draws the outline into dc as a path, at (x, y) baseline */
static int draw_outline( HDC dc, const BYTE *buf, DWORD size, int x, int y, int *csplines, int *qsplines )
{
    const BYTE *p = buf;
    BeginPath( dc );
    while (p < buf + size)
    {
        const TTPOLYGONHEADER *ph = (const TTPOLYGONHEADER *)p;
        const BYTE *c = p + sizeof(*ph);
        POINT cur, pts[256];
        cur.x = x + ph->pfxStart.x.value; cur.y = y - ph->pfxStart.y.value;
        MoveToEx( dc, cur.x, cur.y, NULL );
        while (c < p + ph->cb)
        {
            const TTPOLYCURVE *pc = (const TTPOLYCURVE *)c;
            int i, n = pc->cpfx;
            for (i = 0; i < n && i < 256; i++)
            {
                pts[i].x = x + pc->apfx[i].x.value;
                pts[i].y = y - pc->apfx[i].y.value;
            }
            if (pc->wType == TT_PRIM_LINE) PolylineTo( dc, pts, n );
            else if (pc->wType == TT_PRIM_CSPLINE) { (*csplines)++; PolyBezierTo( dc, pts, n - n % 3 ); }
            else if (pc->wType == TT_PRIM_QSPLINE)
            {
                /* quadratic B-spline: implied on-points between controls */
                POINT q0 = cur;
                (*qsplines)++;
                for (i = 0; i < n - 1; i++)
                {
                    POINT ctl = pts[i], end = pts[i + 1], bz[3];
                    if (i < n - 2) { end.x = (pts[i].x + pts[i + 1].x) / 2; end.y = (pts[i].y + pts[i + 1].y) / 2; }
                    bz[0].x = q0.x + 2 * (ctl.x - q0.x) / 3; bz[0].y = q0.y + 2 * (ctl.y - q0.y) / 3;
                    bz[1].x = end.x + 2 * (ctl.x - end.x) / 3; bz[1].y = end.y + 2 * (ctl.y - end.y) / 3;
                    bz[2] = end;
                    PolyBezierTo( dc, bz, 3 );
                    q0 = end;
                }
            }
            if (n) cur = pts[n - 1];
            c += sizeof(*pc) + (n - 1) * sizeof(POINTFX);
        }
        CloseFigure( dc );
        p += ph->cb;
    }
    EndPath( dc );
    SetPolyFillMode( dc, WINDING );
    return FillPath( dc );
}

static int count_black( HDC dc, int w, int h )
{
    int x, y, n = 0;
    for (y = 0; y < h; y++) for (x = 0; x < w; x++) if (GetPixel( dc, x, y ) == 0) n++;
    return n;
}

int main( int argc, char **argv )
{
    const char *face = argc > 1 ? argv[1] : "Inter";
    HDC dc = CreateCompatibleDC( 0 );
    HBITMAP bmp = CreateCompatibleBitmap( GetDC( 0 ), 400, 200 ), bmp2;
    HFONT font = CreateFontA( -150, 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET, OUT_OUTLINE_PRECIS, 0, NONANTIALIASED_QUALITY, 0, face );
    GLYPHMETRICS gm;
    BYTE *buf;
    DWORD size;
    char name[64];
    int cs = 0, qs = 0, text_px, both, native_px, bezier_px, x, y;
    HDC dc2 = CreateCompatibleDC( 0 );

    SelectObject( dc, bmp );
    SelectObject( dc, font );
    GetTextFaceA( dc, sizeof(name), name );
    printf( "face=%s\n", name );

    /* what TextOut draws */
    PatBlt( dc, 0, 0, 400, 200, WHITENESS );
    SetBkMode( dc, TRANSPARENT );
    TextOutA( dc, 20, 20, "N", 1 );
    text_px = count_black( dc, 400, 200 );

    /* GGO_NATIVE */
    size = GetGlyphOutlineA( dc, 'N', GGO_NATIVE, &gm, 0, NULL, &identity );
    buf = malloc( size );
    GetGlyphOutlineA( dc, 'N', GGO_NATIVE, &gm, size, buf, &identity );
    bmp2 = CreateCompatibleBitmap( GetDC( 0 ), 400, 200 );
    SelectObject( dc2, bmp2 );
    PatBlt( dc2, 0, 0, 400, 200, WHITENESS );
    SelectObject( dc2, GetStockObject( BLACK_BRUSH ) );
    SelectObject( dc2, GetStockObject( NULL_PEN ) );
    {
        TEXTMETRICA tm;
        GetTextMetricsA( dc, &tm );
        draw_outline( dc2, buf, size, 20, 20 + tm.tmAscent, &cs, &qs );
    }
    native_px = count_black( dc2, 400, 200 );
    both = 0;
    for (y = 0; y < 200; y++) for (x = 0; x < 400; x++)
        if (GetPixel( dc, x, y ) == 0 && GetPixel( dc2, x, y ) == 0) both++;
    printf( "native csplines=%d qsplines=%d\n", cs, qs );
    printf( "text_px=%d native_px=%d overlap=%d\n", text_px, native_px, both );
    printf( "native_matches=%d\n", text_px > 500 && both * 10 >= text_px * 9 && both * 10 >= native_px * 9 );
    free( buf );

    /* GGO_BEZIER */
    cs = qs = 0;
    size = GetGlyphOutlineA( dc, 'N', GGO_BEZIER, &gm, 0, NULL, &identity );
    buf = malloc( size );
    GetGlyphOutlineA( dc, 'N', GGO_BEZIER, &gm, size, buf, &identity );
    PatBlt( dc2, 0, 0, 400, 200, WHITENESS );
    {
        TEXTMETRICA tm;
        GetTextMetricsA( dc, &tm );
        draw_outline( dc2, buf, size, 20, 20 + tm.tmAscent, &cs, &qs );
    }
    bezier_px = count_black( dc2, 400, 200 );
    both = 0;
    for (y = 0; y < 200; y++) for (x = 0; x < 400; x++)
        if (GetPixel( dc, x, y ) == 0 && GetPixel( dc2, x, y ) == 0) both++;
    printf( "bezier_px=%d overlap=%d\n", bezier_px, both );
    printf( "bezier_matches=%d\n", text_px > 500 && both * 10 >= text_px * 9 && both * 10 >= bezier_px * 9 );
    printf( "done=1\n" );
    return 0;
}
