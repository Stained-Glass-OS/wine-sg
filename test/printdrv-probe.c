/* Printing through Windows printer drivers (patches/sg/0920-0925).
 *   printdrv-probe install MODEL [INF]        InstallPrinterDriverFromPackage: "install 0x0"
 *   printdrv-probe add NAME DRIVER PORT        adds a printer: "add ok"
 *   printdrv-probe enum                        "enum NAME|DRIVER|PORT|PROCESSOR" per printer
 *   printdrv-probe caps NAME                   "caps HORZRES VERTRES LOGPIXELSX PHYSICALWIDTH OFFSETX"
 *   printdrv-probe papers NAME                 "paper ID WxH NAME" per paper
 *   printdrv-probe print NAME [FORM [COLOR]]   one page: a black (or COLOR, hex RGB) box at (20,10)-(120,60),
 *                                              "print ok" */
#include <stdio.h>
#include <windows.h>
#include <winspool.h>

HRESULT WINAPI InstallPrinterDriverFromPackageW( LPCWSTR, LPCWSTR, LPCWSTR, LPCWSTR, DWORD );

int wmain( int argc, WCHAR **argv )
{
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
        printf( "count=%lu\n", count );
        return 0;
    }
    if (argc >= 3 && !wcscmp( argv[1], L"caps" ))
    {
        HDC hdc = CreateDCW( NULL, argv[2], NULL, NULL );
        if (!hdc) { printf( "caps failed %lu\n", GetLastError() ); return 1; }
        printf( "caps %d %d %d %d %d\n", GetDeviceCaps( hdc, HORZRES ), GetDeviceCaps( hdc, VERTRES ),
                GetDeviceCaps( hdc, LOGPIXELSX ), GetDeviceCaps( hdc, PHYSICALWIDTH ),
                GetDeviceCaps( hdc, PHYSICALOFFSETX ) );
        printf( "desktop %d %d\n", GetDeviceCaps( hdc, DESKTOPHORZRES ), GetDeviceCaps( hdc, DESKTOPVERTRES ) );
        DeleteDC( hdc );
        return 0;
    }
    /* shellicons: the shell's printer icons makers' setup programs show
     * (Epson's divides by how many it loaded) */
    if (argc >= 2 && !wcscmp( argv[1], L"shellicons" ))
    {
        static const int ids[] = { 17, 140, 168, 169, 177, 184 };
        HMODULE shell = LoadLibraryW( L"shell32.dll" );
        unsigned int i, n = 0;
        for (i = 0; i < ARRAYSIZE(ids); i++) if (LoadIconW( shell, MAKEINTRESOURCEW(ids[i]) )) n++;
        printf( "shellicons %u/%u\n", n, (unsigned int)ARRAYSIZE(ids) );
        return 0;
    }
    if (argc >= 3 && !wcscmp( argv[1], L"papers" ))
    {
        int n = DeviceCapabilitiesW( argv[2], NULL, DC_PAPERS, NULL, NULL ), i;
        WORD *ids;
        POINT *sizes;
        WCHAR *names;
        if (n <= 0) { printf( "papers %d\n", n ); return 1; }
        ids = calloc( n, sizeof(*ids) );
        sizes = calloc( n, sizeof(*sizes) );
        names = calloc( n, 64 * sizeof(WCHAR) );
        DeviceCapabilitiesW( argv[2], NULL, DC_PAPERS, (WCHAR *)ids, NULL );
        DeviceCapabilitiesW( argv[2], NULL, DC_PAPERSIZE, (WCHAR *)sizes, NULL );
        DeviceCapabilitiesW( argv[2], NULL, DC_PAPERNAMES, names, NULL );
        for (i = 0; i < n; i++)
            printf( "paper %u %ldx%ld %ls\n", ids[i], sizes[i].x, sizes[i].y, names + i * 64 );
        return 0;
    }
    /* printd PRINTER DENSITY: prints with the driver's own setting (the DWORD
     * after its marker) changed; printd PRINTER: with only public settings */
    if (argc >= 3 && !wcscmp( argv[1], L"printd" ))
    {
        DOCINFOW doc = { sizeof(doc), L"printdrv probe" };
        LONG size = DocumentPropertiesW( NULL, NULL, argv[2], NULL, NULL, 0 );
        RECT box = { 20, 10, 120, 60 };
        DEVMODEW *dm;
        HDC hdc;

        if (argc > 3)
        {
            if (size <= (LONG)sizeof(DEVMODEW)) { printf( "printd: devmode size %ld\n", size ); return 1; }
            dm = calloc( 1, size );
            dm->dmSize = size;
            if (DocumentPropertiesW( NULL, NULL, argv[2], dm, NULL, DM_OUT_BUFFER ) != IDOK ||
                dm->dmDriverExtra < 8) { printf( "printd: no settings of the driver's own\n" ); return 1; }
            ((DWORD *)((BYTE *)dm + dm->dmSize))[1] = wcstoul( argv[3], NULL, 10 );
            DocumentPropertiesW( NULL, NULL, argv[2], dm, dm, DM_IN_BUFFER | DM_OUT_BUFFER );
            printf( "devmode %u+%u\n", dm->dmSize, dm->dmDriverExtra );
        }
        else
        {
            dm = calloc( 1, sizeof(*dm) );
            dm->dmSize = sizeof(*dm);
            dm->dmSpecVersion = DM_SPECVERSION;
            lstrcpynW( dm->dmDeviceName, argv[2], CCHDEVICENAME );
            dm->dmFields = DM_COPIES;
            dm->dmCopies = 1;
        }
        if (!(hdc = CreateDCW( NULL, argv[2], NULL, dm ))) { printf( "printd: no DC %lu\n", GetLastError() ); return 1; }
        if (StartDocW( hdc, &doc ) <= 0) { printf( "printd: StartDoc %lu\n", GetLastError() ); return 1; }
        StartPage( hdc );
        FillRect( hdc, &box, GetStockObject( BLACK_BRUSH ) );
        EndPage( hdc );
        if (EndDoc( hdc ) <= 0) { printf( "printd: EndDoc %lu\n", GetLastError() ); return 1; }
        DeleteDC( hdc );
        printf( "printd ok\n" );
        return 0;
    }
    if (argc >= 3 && !wcscmp( argv[1], L"print" ))
    {
        DOCINFOW doc = { sizeof(doc), L"printdrv probe" };
        LONG size = DocumentPropertiesW( NULL, NULL, argv[2], NULL, NULL, 0 );
        DEVMODEW *dm = NULL;
        RECT box = { 20, 10, 120, 60 };
        HDC hdc;

        if (size > 0 && argc > 3 && argv[3][0])
        {
            dm = calloc( 1, size );
            DocumentPropertiesW( NULL, NULL, argv[2], dm, NULL, DM_OUT_BUFFER );
            dm->dmFields = (dm->dmFields & ~DM_PAPERSIZE) | DM_FORMNAME;
            lstrcpynW( dm->dmFormName, argv[3], CCHFORMNAME );
            DocumentPropertiesW( NULL, NULL, argv[2], dm, dm, DM_IN_BUFFER | DM_OUT_BUFFER );
        }
        if (!(hdc = CreateDCW( NULL, argv[2], NULL, dm ))) { printf( "print: no DC %lu\n", GetLastError() ); return 1; }
        if (StartDocW( hdc, &doc ) <= 0) { printf( "print: StartDoc %lu\n", GetLastError() ); return 1; }
        StartPage( hdc );
        if (argc > 4)
        {
            DWORD rgb = wcstoul( argv[4], NULL, 16 );
            HBRUSH brush = CreateSolidBrush( RGB( rgb >> 16, (rgb >> 8) & 0xff, rgb & 0xff ) );
            FillRect( hdc, &box, brush );
            DeleteObject( brush );
        }
        else FillRect( hdc, &box, GetStockObject( BLACK_BRUSH ) );
        EndPage( hdc );
        if (EndDoc( hdc ) <= 0) { printf( "print: EndDoc %lu\n", GetLastError() ); return 1; }
        DeleteDC( hdc );
        printf( "print ok\n" );
        return 0;
    }
    printf( "usage\n" );
    return 2;
}
