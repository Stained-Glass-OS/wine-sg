/* pscff-probe: the PostScript driver (wineps) with CFF fonts and exclusive clipping (Stained
 * Glass OS wine-sg 6507). Installs a PPD on wineps as a printer whose output is a file, then
 * prints: page 1 "Hello" in an OpenType CFF font (the first of the candidates the system has,
 * written in FONT.txt), page 2 a full black square with a hole cut by BEGIN_PATH/END_PATH and
 * CLIP_TO_PATH(CLIP_EXCLUSIVE). The gate renders C:\out.ps with ghostscript.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include <windows.h>
#include <winspool.h>
#include <stdio.h>

#define BEGIN_PATH_ 4096
#define CLIP_TO_PATH_ 4097
#define END_PATH_ 4098

static const char ppd[] =
    "*PPD-Adobe: \"4.3\"\n*FormatVersion: \"4.3\"\n*FileVersion: \"1.0\"\n*LanguageVersion: English\n"
    "*LanguageEncoding: ISOLatin1\n*PCFileName: \"SGCFF.PPD\"\n*Manufacturer: \"SG\"\n*ModelName: \"SG CFF\"\n"
    "*NickName: \"SG CFF\"\n*ShortNickName: \"SG CFF\"\n*PSVersion: \"(3010.000) 0\"\n*LanguageLevel: \"3\"\n"
    "*ColorDevice: True\n*TTRasterizer: Type42\n*OpenUI *PageSize/Page Size: PickOne\n*DefaultPageSize: Letter\n"
    "*PageSize Letter/Letter: \"<</PageSize[612 792]>>setpagedevice\"\n*CloseUI: *PageSize\n"
    "*DefaultImageableArea: Letter\n*ImageableArea Letter/Letter: \"0 0 612 792\"\n*DefaultPaperDimension: Letter\n"
    "*PaperDimension Letter/Letter: \"612 792\"\n*DefaultResolution: 300dpi\n";

static int CALLBACK has_font( const LOGFONTW *lf, const TEXTMETRICW *tm, DWORD type, LPARAM lparam )
{
    *(int *)lparam = 1;
    return 0;
}

int wmain( void )
{
    static const WCHAR *candidates[] = { L"Inter", L"Nimbus Sans", L"Cantarell", L"C059", L"Nimbus Roman", L"URW Gothic" };
    PRINTER_INFO_2W pi = {0};
    DRIVER_INFO_3W d = {0};
    DOCINFOW di = { sizeof(di) };
    LOGFONTW lf = {0};
    const WCHAR *face = NULL;
    HDC dc, screen;
    HANDLE h;
    FILE *f;
    int i;

    /* a CFF font of the system's */
    screen = GetDC( 0 );
    for (i = 0; i < ARRAYSIZE(candidates) && !face; i++)
    {
        int found = 0;
        LOGFONTW q = {0};
        lstrcpyW( q.lfFaceName, candidates[i] );
        EnumFontFamiliesExW( screen, &q, has_font, (LPARAM)&found, 0 );
        if (found)
        {
            HFONT font;
            q.lfHeight = -20;
            font = SelectObject( screen, CreateFontIndirectW( &q ) );
            if (GetFontData( screen, 0x20464643 /* 'CFF ' */, 0, NULL, 0 ) != GDI_ERROR) face = candidates[i];
            DeleteObject( SelectObject( screen, font ) );
        }
    }
    ReleaseDC( 0, screen );
    if (!face) { printf( "no_cff_font=1\n" ); return 0; }
    printf( "font %ls\n", face );

    f = _wfopen( L"C:\\sgcff.ppd", L"w" ); fputs( ppd, f ); fclose( f );
    AddPrintProcessorW( NULL, NULL, (WCHAR *)L"wineps.drv", (WCHAR *)L"wineps" );
    d.cVersion = 3; d.pName = (WCHAR *)L"SG CFF PS"; d.pDriverPath = (WCHAR *)L"wineps.drv";
    d.pDataFile = (WCHAR *)L"C:\\sgcff.ppd"; d.pConfigFile = (WCHAR *)L"wineps.drv"; d.pDefaultDataType = (WCHAR *)L"RAW";
    if (!AddPrinterDriverExW( NULL, 3, (BYTE *)&d, APD_COPY_NEW_FILES | APD_COPY_FROM_DIRECTORY ))
        { printf( "no_driver=1 %lu\n", GetLastError() ); return 1; }
    pi.pPrinterName = (WCHAR *)L"SG CFF Printer"; pi.pPortName = (WCHAR *)L"LPT1:"; pi.pDriverName = (WCHAR *)L"SG CFF PS";
    pi.pPrintProcessor = (WCHAR *)L"wineps"; pi.pDatatype = (WCHAR *)L"RAW";
    if (!(h = AddPrinterW( NULL, 2, (BYTE *)&pi ))) { printf( "no_printer=1 %lu\n", GetLastError() ); return 1; }
    ClosePrinter( h );
    if (!(dc = CreateDCW( L"WINSPOOL", L"SG CFF Printer", NULL, NULL ))) { printf( "no_dc=1\n" ); return 1; }

    di.lpszDocName = L"cff"; di.lpszOutput = L"C:\\out.ps";
    StartDocW( dc, &di );
    StartPage( dc );
    lf.lfHeight = -200;
    lstrcpyW( lf.lfFaceName, face );
    SelectObject( dc, CreateFontIndirectW( &lf ) );
    TextOutW( dc, 300, 300, L"Hello", 5 );
    EndPage( dc );

    StartPage( dc );
    {
        struct { DWORD render; DWORD fill; DWORD bk; } info = { 0 /* RENDERMODE_NO_DISPLAY */, ALTERNATE, OPAQUE };
        WORD mode;
        RECT all = { 300, 300, 2250, 2250 };
        mode = 0; /* CLIP_SAVE */
        ExtEscape( dc, CLIP_TO_PATH_, sizeof(mode), (char *)&mode, 0, NULL );
        ExtEscape( dc, BEGIN_PATH_, 0, NULL, 0, NULL );
        Rectangle( dc, 900, 900, 1650, 1650 );
        ExtEscape( dc, END_PATH_, sizeof(info), (char *)&info, 0, NULL );
        mode = 3; /* CLIP_EXCLUSIVE */
        printf( "clip_exclusive_ret=%d\n", ExtEscape( dc, CLIP_TO_PATH_, sizeof(mode), (char *)&mode, 0, NULL ) );
        FillRect( dc, &all, GetStockObject( BLACK_BRUSH ) );
        mode = 1; /* CLIP_RESTORE */
        ExtEscape( dc, CLIP_TO_PATH_, sizeof(mode), (char *)&mode, 0, NULL );
    }
    EndPage( dc );
    printf( "enddoc=%d\n", EndDoc( dc ) );
    DeleteDC( dc );
    printf( "done=1\n" );
    return 0;
}
