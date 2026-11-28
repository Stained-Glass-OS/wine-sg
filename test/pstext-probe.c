/* pstext-probe: text printed through the PostScript driver (wineps) stays text (Stained Glass OS
 * wine-sg 9759). Installs two printers on wineps whose output is a file -- one whose PPD asks for
 * TrueType as Type 42 fonts, one that has no Type 42 rasterizer (fonts are sent as Type 1) -- and
 * prints the same lines on each: a TrueType font, an OpenType CFF font, text given as glyph indices
 * (ETO_GLYPH_INDEX, as Uniscribe-based programs print) and characters outside ASCII. The gate turns
 * each C:\outN.ps into a PDF with ghostscript and reads the text back with pdftotext.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include <windows.h>
#include <winspool.h>
#include <stdio.h>

static const char ppd_head[] =
    "*PPD-Adobe: \"4.3\"\n*FormatVersion: \"4.3\"\n*FileVersion: \"1.0\"\n*LanguageVersion: English\n"
    "*LanguageEncoding: ISOLatin1\n*PCFileName: \"SGTXT.PPD\"\n*Manufacturer: \"SG\"\n*ModelName: \"SG Text\"\n"
    "*NickName: \"SG Text\"\n*ShortNickName: \"SG Text\"\n*PSVersion: \"(3010.000) 0\"\n*LanguageLevel: \"3\"\n"
    "*ColorDevice: True\n*OpenUI *PageSize/Page Size: PickOne\n*DefaultPageSize: Letter\n"
    "*PageSize Letter/Letter: \"<</PageSize[612 792]>>setpagedevice\"\n*CloseUI: *PageSize\n"
    "*DefaultImageableArea: Letter\n*ImageableArea Letter/Letter: \"0 0 612 792\"\n*DefaultPaperDimension: Letter\n"
    "*PaperDimension Letter/Letter: \"612 792\"\n*DefaultResolution: 300dpi\n";

static int CALLBACK has_font( const LOGFONTW *lf, const TEXTMETRICW *tm, DWORD type, LPARAM lparam )
{
    *(int *)lparam = 1;
    return 0;
}

/* the first of the candidates the system has; cff: 1 = must be OpenType CFF, 0 = must be TrueType */
static const WCHAR *pick_font( const WCHAR **candidates, int n, int cff )
{
    HDC screen = GetDC( 0 );
    const WCHAR *face = NULL;
    int i;

    for (i = 0; i < n && !face; i++)
    {
        int found = 0;
        LOGFONTW q = {0};
        lstrcpyW( q.lfFaceName, candidates[i] );
        EnumFontFamiliesExW( screen, &q, has_font, (LPARAM)&found, 0 );
        if (found)
        {
            HFONT font;
            int is_cff;
            q.lfHeight = -20;
            font = SelectObject( screen, CreateFontIndirectW( &q ) );
            is_cff = GetFontData( screen, 0x20464643 /* 'CFF ' */, 0, NULL, 0 ) != GDI_ERROR;
            if (is_cff == cff) face = candidates[i];
            DeleteObject( SelectObject( screen, font ) );
        }
    }
    ReleaseDC( 0, screen );
    return face;
}

static BOOL add_printer( const WCHAR *name, const WCHAR *ppdpath, const char *rasterizer )
{
    PRINTER_INFO_2W pi = {0};
    DRIVER_INFO_3W d = {0};
    WCHAR driver[64];
    HANDLE h;
    FILE *f;

    f = _wfopen( ppdpath, L"w" );
    fputs( ppd_head, f );
    fprintf( f, "*TTRasterizer: %s\n", rasterizer );
    fclose( f );
    swprintf( driver, 64, L"%ls Driver", name );
    d.cVersion = 3; d.pName = driver; d.pDriverPath = (WCHAR *)L"wineps.drv";
    d.pDataFile = (WCHAR *)ppdpath; d.pConfigFile = (WCHAR *)L"wineps.drv"; d.pDefaultDataType = (WCHAR *)L"RAW";
    if (!AddPrinterDriverExW( NULL, 3, (BYTE *)&d, APD_COPY_NEW_FILES | APD_COPY_FROM_DIRECTORY ))
        { printf( "no_driver=1 %lu\n", GetLastError() ); return FALSE; }
    pi.pPrinterName = (WCHAR *)name; pi.pPortName = (WCHAR *)L"LPT1:"; pi.pDriverName = driver;
    pi.pPrintProcessor = (WCHAR *)L"wineps"; pi.pDatatype = (WCHAR *)L"RAW";
    if (!(h = AddPrinterW( NULL, 2, (BYTE *)&pi ))) { printf( "no_printer=1 %lu\n", GetLastError() ); return FALSE; }
    ClosePrinter( h );
    return TRUE;
}

static void line( HDC dc, int y, const WCHAR *face, const WCHAR *text, BOOL glyph_indices )
{
    LOGFONTW lf = {0};
    HFONT old;

    lf.lfHeight = -60;
    lstrcpyW( lf.lfFaceName, face );
    old = SelectObject( dc, CreateFontIndirectW( &lf ) );
    if (glyph_indices)
    {
        WORD glyphs[64];
        int n = lstrlenW( text );
        GetGlyphIndicesW( dc, text, n, glyphs, 0 );
        ExtTextOutW( dc, 150, y, ETO_GLYPH_INDEX, NULL, (WCHAR *)glyphs, n, NULL );
    }
    else TextOutW( dc, 150, y, text, lstrlenW( text ) );
    DeleteObject( SelectObject( dc, old ) );
}

static BOOL print_to( const WCHAR *printer, const WCHAR *output, const WCHAR *tt, const WCHAR *cff )
{
    DOCINFOW di = { sizeof(di) };
    HDC dc;

    if (!(dc = CreateDCW( L"WINSPOOL", printer, NULL, NULL ))) { printf( "no_dc=1\n" ); return FALSE; }
    di.lpszDocName = L"text"; di.lpszOutput = output;
    StartDocW( dc, &di );
    StartPage( dc );
    line( dc, 150, tt, L"TrueTypeLine searchable words", FALSE );
    line( dc, 300, tt, L"GlyphIndexLine shaped words", TRUE );
    line( dc, 450, tt, L"Gr\x00fc\x00df" L"e caf\x00e9 \x20ac" L"5", FALSE );
    if (cff) line( dc, 600, cff, L"CffFontLine outline words", FALSE );
    EndPage( dc );
    printf( "enddoc=%d\n", EndDoc( dc ) );
    DeleteDC( dc );
    return TRUE;
}

/* an enhanced metafile (what a print job is spooled as) records the advance of each glyph of
 * ETO_GLYPH_INDEX text given without widths: the glyphs' own advances, not those of the
 * characters whose codes equal the glyph indices */
static int CALLBACK emf_record( HDC dc, HANDLETABLE *table, const ENHMETARECORD *rec, int n, LPARAM lparam )
{
    if (rec->iType == EMR_EXTTEXTOUTW)
    {
        const EMREXTTEXTOUTW *t = (const EMREXTTEXTOUTW *)rec;
        const INT *dx = (const INT *)((const BYTE *)rec + t->emrtext.offDx);
        INT *want = (INT *)lparam;
        UINT i;
        for (i = 0; i < t->emrtext.nChars && i < 8; i++)
            if (dx[i] != want[i]) { printf( "emf_dx %u: %d, glyph advance %d\n", i, dx[i], want[i] ); want[8] = 0; }
        want[9]++;
    }
    return 1;
}

static void check_emf( const WCHAR *face )
{
    static const WCHAR text[] = L"lWiM";
    LOGFONTW lf = {0};
    WORD glyphs[4];
    INT want[10] = {0};
    HDC screen = GetDC( 0 ), emf;
    HENHMETAFILE mf;
    HFONT font;
    int i;

    lf.lfHeight = -60;
    lstrcpyW( lf.lfFaceName, face );
    font = CreateFontIndirectW( &lf );
    emf = CreateEnhMetaFileW( screen, NULL, NULL, NULL );
    SelectObject( emf, font );
    GetGlyphIndicesW( emf, text, 4, glyphs, 0 );
    for (i = 0; i < 4; i++)
    {
        SIZE sz;
        GetTextExtentPointI( emf, glyphs + i, 1, &sz );
        want[i] = sz.cx;
    }
    ExtTextOutW( emf, 10, 10, ETO_GLYPH_INDEX, NULL, (const WCHAR *)glyphs, 4, NULL );
    mf = CloseEnhMetaFile( emf );
    want[8] = 1;
    EnumEnhMetaFile( NULL, mf, emf_record, want, NULL );
    printf( "emf_dx_ok=%d records=%d\n", want[8] && want[9] == 1, want[9] );
    DeleteEnhMetaFile( mf );
    DeleteObject( font );
    ReleaseDC( 0, screen );
}

int wmain( void )
{
    static const WCHAR *tts[] = { L"DejaVu Sans", L"Liberation Sans", L"Noto Sans", L"FreeSans" };
    static const WCHAR *cffs[] = { L"Nimbus Sans", L"C059", L"Nimbus Roman", L"URW Gothic", L"Cantarell", L"Inter" };
    const WCHAR *tt = pick_font( tts, ARRAYSIZE(tts), 0 ), *cff = pick_font( cffs, ARRAYSIZE(cffs), 1 );

    if (!tt) { printf( "no_tt_font=1\n" ); return 0; }
    printf( "truetype %ls\ncff %ls\n", tt, cff ? cff : L"(none)" );
    if (!cff) printf( "no_cff_font=1\n" );
    AddPrintProcessorW( NULL, NULL, (WCHAR *)L"wineps.drv", (WCHAR *)L"wineps" );
    if (!add_printer( L"SG Type42 Printer", L"C:\\sgt42.ppd", "Type42" )) return 1;
    if (!add_printer( L"SG Type1 Printer", L"C:\\sgt1.ppd", "None" )) return 1;
    if (!print_to( L"SG Type42 Printer", L"C:\\out42.ps", tt, cff )) return 1;
    if (!print_to( L"SG Type1 Printer", L"C:\\out1.ps", tt, cff )) return 1;
    check_emf( tt );
    printf( "done=1\n" );
    return 0;
}
