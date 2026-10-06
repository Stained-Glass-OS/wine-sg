/*
 * sgprint: drives a printer driver package through the Windows printing
 * API, for the printer-driver corpus (tools/printer-corpus/run.sh).  Our own
 * code; it uses only the documented winspool/GDI/SetupAPI calls.
 *
 *   sgprint models INF                    "model NAME" per model for this platform
 *   sgprint install MODEL [INF]           InstallPrinterDriverFromPackage: "install 0x0"
 *   sgprint add NAME DRIVER PORT          AddPrinter: "add ok"
 *   sgprint info NAME                     the driver's files, processor, monitor, datatype
 *   sgprint caps NAME                     the DC's measurements
 *   sgprint papers NAME                   "paper ID WxH NAME"
 *   sgprint features NAME                 the driver's DeviceCapabilities answers
 *   sgprint print NAME [FIELD=VALUE...]   the test page (and a second text page)
 *        fields: paper=ID orient=1|2 color=1|2 duplex=1|2|3 copies=N collate=0|1 bin=ID
 *   sgprint ref OUT.bmp WIDTH HEIGHT DPI  the test page drawn on a sheet of WIDTHxHEIGHT
 *                                         (tenths of a millimetre) at DPI, for comparing
 *   sgprint status NAME                   the printer's status and its jobs
 *   sgprint props NAME                    shows the driver's printing preferences
 *   sgprint enum                          "enum NAME|DRIVER|PORT|PROCESSOR" per printer
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <winspool.h>
#include <setupapi.h>
#include <commctrl.h>
#include <prsht.h>

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#endif

HRESULT WINAPI InstallPrinterDriverFromPackageW( LPCWSTR, LPCWSTR, LPCWSTR, LPCWSTR, DWORD );

/* ---- the test page, laid out in thousandths of an inch on the sheet ---- */

struct page_dc
{
    HDC hdc;
    double sx, sy;      /* device units per thousandth of an inch */
    int ox, oy;         /* the printable area's offset on the sheet, device units */
};

static int X( struct page_dc *p, int mils ) { return (int)(mils * p->sx + 0.5) - p->ox; }
static int Y( struct page_dc *p, int mils ) { return (int)(mils * p->sy + 0.5) - p->oy; }

static HFONT font( struct page_dc *p, const WCHAR *face, int points, int weight, BOOL italic )
{
    return CreateFontW( -(int)(points * 1000 / 72 * p->sy + 0.5), 0, 0, 0, weight, italic, FALSE, FALSE,
                        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
                        DEFAULT_PITCH, face );
}

static void text( struct page_dc *p, int x, int y, const WCHAR *face, int points, int weight, const WCHAR *str )
{
    HFONT f = font( p, face, points, weight, FALSE ), old = SelectObject( p->hdc, f );
    SetBkMode( p->hdc, TRANSPARENT );
    SetTextColor( p->hdc, 0 );
    TextOutW( p->hdc, X( p, x ), Y( p, y ), str, wcslen( str ) );
    SelectObject( p->hdc, old );
    DeleteObject( f );
}

static void box( struct page_dc *p, int l, int t, int r, int b, COLORREF color )
{
    RECT rc = { X( p, l ), Y( p, t ), X( p, r ), Y( p, b ) };
    HBRUSH br = CreateSolidBrush( color );
    FillRect( p->hdc, &rc, br );
    DeleteObject( br );
}

/* the page: 8 x 10 inches of content from (0.5, 0.5) inch, so it fits
 * Letter and A4; a label printer gets what fits its label */
static void draw_test_page( struct page_dc *p, int sheet_w, int sheet_h )
{
    static const COLORREF bars[] = { RGB(255,0,0), RGB(0,255,0), RGB(0,0,255), RGB(0,255,255),
                                     RGB(255,0,255), RGB(255,255,0), RGB(0,0,0) };
    HPEN pen, old_pen;
    HBRUSH old_brush;
    int i, w = min( 7500, sheet_w - 1000 ), h = min( 10000, sheet_h - 1000 );
    BITMAPINFO bmi = { { sizeof(BITMAPINFOHEADER), 64, 64, 1, 24, BI_RGB } };
    BYTE *bits = malloc( 64 * 64 * 3 );

    /* a frame round the content */
    pen = CreatePen( PS_SOLID, max( 1, (int)(20 * p->sx) ), 0 );
    old_pen = SelectObject( p->hdc, pen );
    old_brush = SelectObject( p->hdc, GetStockObject( NULL_BRUSH ) );
    Rectangle( p->hdc, X( p, 500 ), Y( p, 500 ), X( p, 500 + w ), Y( p, 500 + h ) );
    if (h < 2000 || w < 2000)
    {
        /* a label: a heading and a box */
        text( p, 600, 550, L"Arial", min( 18, h / 60 ), FW_BOLD, L"SG print test" );
        box( p, 600, 500 + h / 2, 600 + w / 3, 500 + h - 100, 0 );
        SelectObject( p->hdc, old_brush );
        SelectObject( p->hdc, old_pen );
        DeleteObject( pen );
        free( bits );
        return;
    }
    text( p, 800, 700, L"Arial", 24, FW_BOLD, L"Stained Glass printer test" );
    text( p, 800, 1200, L"Times New Roman", 12, FW_NORMAL, L"The quick brown fox jumps over the lazy dog. 0123456789" );
    text( p, 800, 1450, L"Courier New", 12, FW_NORMAL, L"Courier: ABCDEFGHIJKLMNOPQRSTUVWXYZ abcdefghijklmnopqrstuvwxyz" );
    text( p, 800, 1700, L"Arial", 10, FW_NORMAL, L"Arial 10 pt: \x00e4\x00f6\x00fc \x00df \x20ac \x00a9 \x2013 \x201c quotes \x201d" );

    /* a solid box and colour bars */
    box( p, 800, 2100, 2800, 3100, 0 );
    for (i = 0; i < ARRAYSIZE(bars); i++) box( p, 3100 + i * 600, 2100, 3600 + i * 600, 3100, bars[i] );
    /* a grey ramp */
    for (i = 0; i < 16; i++)
    {
        int v = i * 255 / 15;
        box( p, 800 + i * 400, 3400, 1200 + i * 400, 3900, RGB( v, v, v ) );
    }
    /* lines and an ellipse */
    for (i = 0; i < 10; i++)
    {
        MoveToEx( p->hdc, X( p, 800 ), Y( p, 4200 + i * 100 ), NULL );
        LineTo( p->hdc, X( p, 4000 ), Y( p, 4200 + i * 250 ) );
    }
    Ellipse( p->hdc, X( p, 4500 ), Y( p, 4200 ), X( p, 7000 ), Y( p, 6700 ) );
    /* a picture: a 64x64 checkerboard with a colour gradient */
    for (i = 0; i < 64 * 64; i++)
    {
        int x = i % 64, y = i / 64;
        BOOL dark = ((x / 8) + (y / 8)) & 1;
        bits[i * 3] = dark ? x * 4 : 255;
        bits[i * 3 + 1] = dark ? 0 : 255 - y * 2;
        bits[i * 3 + 2] = dark ? y * 4 : 255;
    }
    StretchDIBits( p->hdc, X( p, 800 ), Y( p, 7000 ), X( p, 2800 ) - X( p, 800 ), Y( p, 9000 ) - Y( p, 7000 ),
                   0, 0, 64, 64, bits, &bmi, DIB_RGB_COLORS, SRCCOPY );
    text( p, 3200, 7000, L"Arial", 14, FW_NORMAL, L"Page 1 of 2" );
    text( p, 3200, 7400, L"Times New Roman", 14, FW_BOLD, L"Bold Times" );
    SelectObject( p->hdc, old_brush );
    SelectObject( p->hdc, old_pen );
    DeleteObject( pen );
    free( bits );
}

static void draw_second_page( struct page_dc *p, int sheet_w, int sheet_h )
{
    int y;
    if (sheet_h < 3000) return;
    for (y = 800; y < min( 10000, sheet_h - 1000 ); y += 400)
        text( p, 800, y, L"Courier New", 10, FW_NORMAL, L"Page 2: a page of text lines, to check page breaks." );
}

/* ---- commands ---- */

static int models( const WCHAR *inf_path )
{
    HINF inf = SetupOpenInfFileW( inf_path, NULL, INF_STYLE_WIN4, NULL );
    INFCONTEXT mfg, line;
    WCHAR section[256], deco[64], models[512], name[512];
    DWORD i, n;

    if (inf == INVALID_HANDLE_VALUE) { printf( "models: cannot open %lu\n", GetLastError() ); return 1; }
    if (!SetupFindFirstLineW( inf, L"Manufacturer", NULL, &mfg )) return 1;
    do
    {
        BOOL found = FALSE;
        if (!SetupGetStringFieldW( &mfg, 1, section, 256, NULL )) continue;
        n = SetupGetFieldCount( &mfg );
        for (i = 2; i <= n; i++)
        {
            if (!SetupGetStringFieldW( &mfg, i, deco, 64, NULL )) continue;
            if (!_wcsnicmp( deco, L"NTamd64", 7 ))
            {
                swprintf( models, 512, L"%ls.%ls", section, deco );
                found = SetupFindFirstLineW( inf, models, NULL, &line );
                break;
            }
        }
        if (!found)
        {
            wcscpy( models, section );
            if (!SetupFindFirstLineW( inf, models, NULL, &line )) continue;
        }
        do
        {
            if (SetupGetStringFieldW( &line, 0, name, 512, NULL )) printf( "model %ls\n", name );
        } while (SetupFindNextLine( &line, &line ));
    } while (SetupFindNextLine( &mfg, &mfg ));
    SetupCloseInfFile( inf );
    return 0;
}

static void *get_printer( HANDLE h, DWORD level )
{
    DWORD needed = 0;
    void *buf;
    GetPrinterW( h, level, NULL, 0, &needed );
    if (!needed || !(buf = malloc( needed ))) return NULL;
    if (!GetPrinterW( h, level, buf, needed, &needed )) { free( buf ); return NULL; }
    return buf;
}

static int info( const WCHAR *name )
{
    HANDLE h;
    DRIVER_INFO_6W *di;
    PRINTER_INFO_2W *pi;
    DWORD needed = 0;
    WCHAR *p;

    if (!OpenPrinterW( (WCHAR *)name, &h, NULL )) { printf( "info: open %lu\n", GetLastError() ); return 1; }
    GetPrinterDriverW( h, NULL, 6, NULL, 0, &needed );
    di = malloc( needed ? needed : 1 );
    if (GetPrinterDriverW( h, NULL, 6, (BYTE *)di, needed, &needed ))
    {
        printf( "driver %ls\ndriverfile %ls\ndatafile %ls\nconfigfile %ls\nmonitor %ls\ndatatype %ls\n",
                di->pName, di->pDriverPath, di->pDataFile, di->pConfigFile,
                di->pMonitorName ? di->pMonitorName : L"", di->pDefaultDataType ? di->pDefaultDataType : L"" );
        for (p = di->pDependentFiles; p && *p; p += wcslen( p ) + 1) printf( "dependent %ls\n", p );
    }
    else printf( "driver: %lu\n", GetLastError() );
    if ((pi = get_printer( h, 2 )))
    {
        printf( "port %ls\nprocessor %ls\nprinterdatatype %ls\nstatus %#lx\n", pi->pPortName, pi->pPrintProcessor,
                pi->pDatatype, pi->Status );
        free( pi );
    }
    ClosePrinter( h );
    return 0;
}

static int caps( const WCHAR *name )
{
    HDC hdc = CreateDCW( NULL, name, NULL, NULL );
    if (!hdc) { printf( "caps: no DC %lu\n", GetLastError() ); return 1; }
    printf( "caps technology=%d res=%dx%d dpi=%dx%d phys=%dx%d offset=%d,%d bpp=%d colors=%d textcaps=%#x raster=%#x\n",
            GetDeviceCaps( hdc, TECHNOLOGY ), GetDeviceCaps( hdc, HORZRES ), GetDeviceCaps( hdc, VERTRES ),
            GetDeviceCaps( hdc, LOGPIXELSX ), GetDeviceCaps( hdc, LOGPIXELSY ), GetDeviceCaps( hdc, PHYSICALWIDTH ),
            GetDeviceCaps( hdc, PHYSICALHEIGHT ), GetDeviceCaps( hdc, PHYSICALOFFSETX ),
            GetDeviceCaps( hdc, PHYSICALOFFSETY ), GetDeviceCaps( hdc, BITSPIXEL ), GetDeviceCaps( hdc, NUMCOLORS ),
            GetDeviceCaps( hdc, TEXTCAPS ), GetDeviceCaps( hdc, RASTERCAPS ) );
    DeleteDC( hdc );
    return 0;
}

static int papers( const WCHAR *name )
{
    int n = DeviceCapabilitiesW( name, NULL, DC_PAPERS, NULL, NULL ), i;
    WORD *ids;
    POINT *sizes;
    WCHAR *names;

    if (n <= 0) { printf( "papers %d\n", n ); return 1; }
    ids = calloc( n, sizeof(*ids) );
    sizes = calloc( n, sizeof(*sizes) );
    names = calloc( n, 64 * sizeof(WCHAR) );
    DeviceCapabilitiesW( name, NULL, DC_PAPERS, (WCHAR *)ids, NULL );
    DeviceCapabilitiesW( name, NULL, DC_PAPERSIZE, (WCHAR *)sizes, NULL );
    DeviceCapabilitiesW( name, NULL, DC_PAPERNAMES, names, NULL );
    for (i = 0; i < n; i++) printf( "paper %u %ldx%ld %ls\n", ids[i], sizes[i].x, sizes[i].y, names + i * 64 );
    return 0;
}

static int features( const WCHAR *name )
{
    int n, i;
    WCHAR *bins;
    WORD *binids;

    printf( "duplex %d\ncolor %d\ncollate %d\ncopies %d\nstaple %d\nnup %d\n",
            DeviceCapabilitiesW( name, NULL, DC_DUPLEX, NULL, NULL ),
            DeviceCapabilitiesW( name, NULL, DC_COLORDEVICE, NULL, NULL ),
            DeviceCapabilitiesW( name, NULL, DC_COLLATE, NULL, NULL ),
            DeviceCapabilitiesW( name, NULL, DC_COPIES, NULL, NULL ),
            DeviceCapabilitiesW( name, NULL, DC_STAPLE, NULL, NULL ),
            DeviceCapabilitiesW( name, NULL, DC_NUP, NULL, NULL ) );
    n = DeviceCapabilitiesW( name, NULL, DC_BINS, NULL, NULL );
    if (n > 0)
    {
        binids = calloc( n, sizeof(WORD) );
        bins = calloc( n, 24 * sizeof(WCHAR) );
        DeviceCapabilitiesW( name, NULL, DC_BINS, (WCHAR *)binids, NULL );
        DeviceCapabilitiesW( name, NULL, DC_BINNAMES, bins, NULL );
        for (i = 0; i < n; i++) printf( "bin %u %.24ls\n", binids[i], bins + i * 24 );
    }
    n = DeviceCapabilitiesW( name, NULL, DC_ENUMRESOLUTIONS, NULL, NULL );
    if (n > 0)
    {
        LONG *res = calloc( n, 2 * sizeof(LONG) );
        DeviceCapabilitiesW( name, NULL, DC_ENUMRESOLUTIONS, (WCHAR *)res, NULL );
        for (i = 0; i < n; i++) printf( "resolution %ldx%ld\n", res[i * 2], res[i * 2 + 1] );
    }
    n = DeviceCapabilitiesW( name, NULL, DC_MEDIATYPES, NULL, NULL );
    printf( "mediatypes %d\n", n );
    return 0;
}

static DEVMODEW *make_devmode( const WCHAR *name, int argc, WCHAR **argv )
{
    LONG size = DocumentPropertiesW( NULL, NULL, (WCHAR *)name, NULL, NULL, 0 );
    DEVMODEW *dm;
    int i;

    if (size <= 0) { printf( "devmode: DocumentProperties %ld (%lu)\n", size, GetLastError() ); return NULL; }
    dm = calloc( 1, size );
    if (DocumentPropertiesW( NULL, NULL, (WCHAR *)name, dm, NULL, DM_OUT_BUFFER ) < 0) return NULL;
    for (i = 0; i < argc; i++)
    {
        WCHAR *eq = wcschr( argv[i], '=' );
        int v;
        if (!eq) continue;
        v = _wtoi( eq + 1 );
        if (!wcsncmp( argv[i], L"paper=", 6 )) { dm->dmFields = (dm->dmFields & ~DM_FORMNAME) | DM_PAPERSIZE; dm->dmPaperSize = v; }
        else if (!wcsncmp( argv[i], L"orient=", 7 )) { dm->dmFields |= DM_ORIENTATION; dm->dmOrientation = v; }
        else if (!wcsncmp( argv[i], L"color=", 6 )) { dm->dmFields |= DM_COLOR; dm->dmColor = v; }
        else if (!wcsncmp( argv[i], L"duplex=", 7 )) { dm->dmFields |= DM_DUPLEX; dm->dmDuplex = v; }
        else if (!wcsncmp( argv[i], L"copies=", 7 )) { dm->dmFields |= DM_COPIES; dm->dmCopies = v; }
        else if (!wcsncmp( argv[i], L"collate=", 8 )) { dm->dmFields |= DM_COLLATE; dm->dmCollate = v; }
        else if (!wcsncmp( argv[i], L"bin=", 4 )) { dm->dmFields |= DM_DEFAULTSOURCE; dm->dmDefaultSource = v; }
    }
    if (DocumentPropertiesW( NULL, NULL, (WCHAR *)name, dm, dm, DM_IN_BUFFER | DM_OUT_BUFFER ) < 0)
        printf( "devmode: merge failed %lu\n", GetLastError() );
    return dm;
}

static int print( const WCHAR *name, int argc, WCHAR **argv )
{
    DOCINFOW doc = { sizeof(doc), L"SG printer test" };
    DEVMODEW *dm = make_devmode( name, argc, argv );
    struct page_dc p;
    int sheet_w, sheet_h;
    HDC hdc;

    if (!(hdc = CreateDCW( NULL, name, NULL, dm ))) { printf( "print: no DC %lu\n", GetLastError() ); return 1; }
    p.hdc = hdc;
    p.sx = GetDeviceCaps( hdc, LOGPIXELSX ) / 1000.0;
    p.sy = GetDeviceCaps( hdc, LOGPIXELSY ) / 1000.0;
    p.ox = GetDeviceCaps( hdc, PHYSICALOFFSETX );
    p.oy = GetDeviceCaps( hdc, PHYSICALOFFSETY );
    sheet_w = (int)(GetDeviceCaps( hdc, PHYSICALWIDTH ) / p.sx);
    sheet_h = (int)(GetDeviceCaps( hdc, PHYSICALHEIGHT ) / p.sy);
    if (StartDocW( hdc, &doc ) <= 0) { printf( "print: StartDoc %lu\n", GetLastError() ); return 1; }
    StartPage( hdc );
    draw_test_page( &p, sheet_w, sheet_h );
    if (EndPage( hdc ) <= 0) printf( "print: EndPage %lu\n", GetLastError() );
    StartPage( hdc );
    draw_second_page( &p, sheet_w, sheet_h );
    if (EndPage( hdc ) <= 0) printf( "print: EndPage %lu\n", GetLastError() );
    if (EndDoc( hdc ) <= 0) { printf( "print: EndDoc %lu\n", GetLastError() ); return 1; }
    DeleteDC( hdc );
    printf( "print ok sheet %dx%d mils\n", sheet_w, sheet_h );
    return 0;
}

static int ref( const WCHAR *out, int w, int h, int dpi )
{
    struct page_dc p;
    int pw = MulDiv( w, dpi, 254 ), ph = MulDiv( h, dpi, 254 ), stride = (pw * 3 + 3) & ~3;
    BITMAPINFO bmi = { { sizeof(BITMAPINFOHEADER), pw, -ph, 1, 24, BI_RGB } };
    BITMAPFILEHEADER bf = { 0x4d42 };
    RECT rc = { 0, 0, pw, ph };
    HBITMAP dib;
    void *bits;
    FILE *f;

    p.hdc = CreateCompatibleDC( NULL );
    dib = CreateDIBSection( p.hdc, &bmi, DIB_RGB_COLORS, &bits, NULL, 0 );
    SelectObject( p.hdc, dib );
    FillRect( p.hdc, &rc, GetStockObject( WHITE_BRUSH ) );
    p.sx = p.sy = dpi / 1000.0;
    p.ox = p.oy = 0;
    draw_test_page( &p, w * 1000 / 254, h * 1000 / 254 );
    GdiFlush();
    if (!(f = _wfopen( out, L"wb" ))) return 1;
    bf.bfOffBits = sizeof(bf) + sizeof(BITMAPINFOHEADER);
    bf.bfSize = bf.bfOffBits + stride * ph;
    fwrite( &bf, sizeof(bf), 1, f );
    fwrite( &bmi.bmiHeader, sizeof(BITMAPINFOHEADER), 1, f );
    fwrite( bits, stride, ph, f );
    fclose( f );
    printf( "ref %dx%d\n", pw, ph );
    return 0;
}

static int status( const WCHAR *name )
{
    HANDLE h;
    PRINTER_INFO_2W *pi;
    JOB_INFO_2W *jobs;
    DWORD needed = 0, count = 0, i;

    if (!OpenPrinterW( (WCHAR *)name, &h, NULL )) { printf( "status: open %lu\n", GetLastError() ); return 1; }
    if ((pi = get_printer( h, 2 )))
    {
        printf( "printer status %#lx jobs %lu\n", pi->Status, pi->cJobs );
        free( pi );
    }
    EnumJobsW( h, 0, 100, 2, NULL, 0, &needed, &count );
    jobs = malloc( needed ? needed : 1 );
    if (EnumJobsW( h, 0, 100, 2, (BYTE *)jobs, needed, &needed, &count ))
        for (i = 0; i < count; i++)
            printf( "job %lu status %#lx '%ls' %ls\n", jobs[i].JobId, jobs[i].Status,
                    jobs[i].pStatus ? jobs[i].pStatus : L"", jobs[i].pDocument );
    ClosePrinter( h );
    return 0;
}

/* SG_PROPS_WAIT=N: N seconds after the sheet shows, its tabs are listed
 * and OK is pressed (a screenshot can be taken meanwhile) */
static BOOL CALLBACK find_sheet( HWND hwnd, LPARAM lparam )
{
    DWORD pid;
    WCHAR cls[32];
    GetWindowThreadProcessId( hwnd, &pid );
    GetClassNameW( hwnd, cls, ARRAY_SIZE(cls) );
    RECT rect, best;
    HWND *found = (HWND *)lparam;

    if (pid != GetCurrentProcessId() || wcscmp( cls, L"#32770" ) || !IsWindowVisible( hwnd )) return TRUE;
    /* the biggest: some drivers show a dialog of their own over the sheet */
    GetWindowRect( hwnd, &rect );
    if (*found)
    {
        GetWindowRect( *found, &best );
        if ((rect.right - rect.left) * (rect.bottom - rect.top) <= (best.right - best.left) * (best.bottom - best.top))
            return TRUE;
    }
    *found = hwnd;
    return TRUE;
}

static DWORD WINAPI press_ok( void *arg )
{
    WCHAR wait[16], text[128], title[256];
    HWND sheet = NULL, tab;
    TCITEMW item;
    int i, count;

    if (!GetEnvironmentVariableW( L"SG_PROPS_WAIT", wait, ARRAY_SIZE(wait) )) return 0;
    for (i = 0; i < 600 && !sheet; i++)
    {
        Sleep( 100 );
        EnumWindows( find_sheet, (LPARAM)&sheet );
    }
    if (!sheet) { printf( "sheet none\n" ); return 0; }
    if (GetEnvironmentVariableW( L"SG_PROPS_PAGE", text, ARRAY_SIZE(text) ))
        PostMessageW( sheet, PSM_SETCURSEL, _wtoi( text ), 0 );
    Sleep( _wtoi( wait ) * 1000 );
    sheet = NULL;
    EnumWindows( find_sheet, (LPARAM)&sheet );
    GetWindowTextW( sheet, title, ARRAY_SIZE(title) );
    tab = (HWND)SendMessageW( sheet, PSM_GETTABCONTROL, 0, 0 );
    count = tab ? SendMessageW( tab, TCM_GETITEMCOUNT, 0, 0 ) : 0;
    printf( "sheet %ls tabs %d:", title, count );
    for (i = 0; i < count; i++)
    {
        item.mask = TCIF_TEXT;
        item.pszText = text;
        item.cchTextMax = ARRAY_SIZE(text);
        SendMessageW( tab, TCM_GETITEMW, i, (LPARAM)&item );
        printf( " [%ls]", text );
    }
    printf( "\n" );
    if (GetDlgItem( sheet, IDOK )) PostMessageW( GetDlgItem( sheet, IDOK ), BM_CLICK, 0, 0 );
    else PostMessageW( sheet, PSM_PRESSBUTTON, PSBTN_OK, 0 );
    return 0;
}

static int devprops( const WCHAR *name )
{
    HANDLE h;
    BOOL ret;

    if (!OpenPrinterW( (WCHAR *)name, &h, NULL )) return 1;
    CloseHandle( CreateThread( NULL, 0, press_ok, NULL, 0, NULL ) );
    ret = PrinterProperties( NULL, h );
    printf( "devprops %d %lu\n", ret, ret ? 0 : GetLastError() );
    ClosePrinter( h );
    return 0;
}

static int props( const WCHAR *name )
{
    HANDLE h;
    LONG size, ret;
    DEVMODEW *dm;

    if (!OpenPrinterW( (WCHAR *)name, &h, NULL )) return 1;
    size = DocumentPropertiesW( NULL, h, (WCHAR *)name, NULL, NULL, 0 );
    dm = calloc( 1, size > 0 ? size : sizeof(DEVMODEW) );
    CloseHandle( CreateThread( NULL, 0, press_ok, NULL, 0, NULL ) );
    ret = DocumentPropertiesW( NULL, h, (WCHAR *)name, dm, NULL, DM_OUT_BUFFER | DM_IN_PROMPT );
    printf( "props %ld\n", ret );
    ClosePrinter( h );
    return 0;
}

int wmain( int argc, WCHAR **argv )
{
    setvbuf( stdout, NULL, _IONBF, 0 );
    if (argc >= 3 && !wcscmp( argv[1], L"models" )) return models( argv[2] );
    if (argc >= 3 && !wcscmp( argv[1], L"install" ))
    {
        HRESULT hr = InstallPrinterDriverFromPackageW( NULL, argc > 3 ? argv[3] : NULL, argv[2], NULL, 0 );
        printf( "install %#lx\n", hr );
        return FAILED(hr);
    }
    if (argc >= 5 && !wcscmp( argv[1], L"add" ))
    {
        PRINTER_INFO_2W pi = { 0 };
        HANDLE h;
        pi.pPrinterName = argv[2];
        pi.pDriverName = argv[3];
        pi.pPortName = argv[4];
        pi.pPrintProcessor = (WCHAR *)L"winprint";
        pi.pDatatype = (WCHAR *)L"RAW";
        if (!(h = AddPrinterW( NULL, 2, (BYTE *)&pi ))) { printf( "add failed %lu\n", GetLastError() ); return 1; }
        ClosePrinter( h );
        printf( "add ok\n" );
        return 0;
    }
    if (argc >= 3 && !wcscmp( argv[1], L"info" )) return info( argv[2] );
    if (argc >= 3 && !wcscmp( argv[1], L"caps" )) return caps( argv[2] );
    if (argc >= 3 && !wcscmp( argv[1], L"papers" )) return papers( argv[2] );
    if (argc >= 3 && !wcscmp( argv[1], L"features" )) return features( argv[2] );
    if (argc >= 3 && !wcscmp( argv[1], L"print" )) return print( argv[2], argc - 3, argv + 3 );
    if (argc >= 6 && !wcscmp( argv[1], L"ref" )) return ref( argv[2], _wtoi( argv[3] ), _wtoi( argv[4] ), _wtoi( argv[5] ) );
    if (argc >= 3 && !wcscmp( argv[1], L"status" )) return status( argv[2] );
    if (argc >= 3 && !wcscmp( argv[1], L"props" )) return props( argv[2] );
    if (argc >= 3 && !wcscmp( argv[1], L"devprops" )) return devprops( argv[2] );
    if (argc >= 2 && !wcscmp( argv[1], L"enum" ))
    {
        DWORD needed = 0, count = 0, i;
        PRINTER_INFO_2W *pi;
        EnumPrintersW( PRINTER_ENUM_LOCAL, NULL, 2, NULL, 0, &needed, &count );
        pi = malloc( needed ? needed : 1 );
        if (!EnumPrintersW( PRINTER_ENUM_LOCAL, NULL, 2, (BYTE *)pi, needed, &needed, &count )) count = 0;
        for (i = 0; i < count; i++)
            printf( "enum %ls|%ls|%ls|%ls\n", pi[i].pPrinterName, pi[i].pDriverName, pi[i].pPortName,
                    pi[i].pPrintProcessor );
        return 0;
    }
    printf( "usage: see the source\n" );
    return 2;
}
