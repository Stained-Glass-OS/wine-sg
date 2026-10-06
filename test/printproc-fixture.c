/* A print processor of our own, for the printproc gate (patches/sg/1030),
 * built as printer makers build theirs (Canon's, HP's): it takes EMF jobs
 * and prints their pages itself through GDI's print processor functions
 * (GdiGetSpoolFileHandle, GdiGetDC, GdiGetPageCount, GdiPlayPageEMF...),
 * drawing a mark of its own on each page; what it does goes to the file
 * $SG_PRINTPROC_LOG.  Our own code. */
#include <stdio.h>
#include <string.h>
#include <windows.h>
#include <winspool.h>
#include <winsplp.h>

HANDLE WINAPI GdiGetSpoolFileHandle( WCHAR *, DEVMODEW *, WCHAR * );
BOOL WINAPI GdiDeleteSpoolFileHandle( HANDLE );
DWORD WINAPI GdiGetPageCount( HANDLE );
HDC WINAPI GdiGetDC( HANDLE );
HANDLE WINAPI GdiGetPageHandle( HANDLE, DWORD, DWORD * );
BOOL WINAPI GdiStartDocEMF( HANDLE, DOCINFOW * );
BOOL WINAPI GdiStartPageEMF( HANDLE );
BOOL WINAPI GdiPlayPageEMF( HANDLE, HANDLE, RECT *, RECT *, RECT * );
BOOL WINAPI GdiEndPageEMF( HANDLE, DWORD );
BOOL WINAPI GdiEndDocEMF( HANDLE );

struct pp
{
    WCHAR printer[MAX_PATH];
    WCHAR document[MAX_PATH];
    DEVMODEW *devmode;
};

static void say( const char *fmt, ... )
{
    char buf[256], path[MAX_PATH];
    va_list args;
    FILE *f;

    if (!GetEnvironmentVariableA( "SG_PRINTPROC_LOG", path, sizeof(path) )) return;
    va_start( args, fmt );
    vsnprintf( buf, sizeof(buf), fmt, args );
    va_end( args );
    if ((f = fopen( path, "a" ))) { fputs( buf, f ); fclose( f ); }
}

__declspec(dllexport) BOOL WINAPI EnumPrintProcessorDatatypesW( WCHAR *server, WCHAR *name, DWORD level, BYTE *buf,
                                                                DWORD size, DWORD *needed, DWORD *returned )
{
    static const WCHAR emf[] = L"NT EMF 1.008";
    DATATYPES_INFO_1W *info = (DATATYPES_INFO_1W *)buf;

    *needed = sizeof(*info) + sizeof(emf);
    *returned = 0;
    if (level != 1 || !buf || size < *needed)
    {
        SetLastError( ERROR_INSUFFICIENT_BUFFER );
        return FALSE;
    }
    info->pName = (WCHAR *)(info + 1);
    memcpy( info->pName, emf, sizeof(emf) );
    *returned = 1;
    return TRUE;
}

__declspec(dllexport) HANDLE WINAPI OpenPrintProcessor( WCHAR *port, PRINTPROCESSOROPENDATA *data )
{
    struct pp *pp = calloc( 1, sizeof(*pp) );

    if (!pp) return NULL;
    /* as makers' processors do: the name the spooler opened it with */
    lstrcpynW( pp->printer, port, MAX_PATH );
    if (data->pDocumentName) lstrcpynW( pp->document, data->pDocumentName, MAX_PATH );
    if (data->pDevMode && (pp->devmode = malloc( data->pDevMode->dmSize + data->pDevMode->dmDriverExtra )))
        memcpy( pp->devmode, data->pDevMode, data->pDevMode->dmSize + data->pDevMode->dmDriverExtra );
    say( "open %ls\n", data->pDatatype );
    return pp;
}

__declspec(dllexport) BOOL WINAPI PrintDocumentOnPrintProcessor( HANDLE handle, WCHAR *doc_name )
{
    struct pp *pp = handle;
    HANDLE spool;
    HDC hdc;
    DOCINFOW di = { sizeof(di) };
    DWORD count, i, type;
    BOOL ret = TRUE;

    /* SG_PRINTPROC_QUIT: gives up without reading the job, as Samsung's does here */
    if (GetEnvironmentVariableW( L"SG_PRINTPROC_QUIT", NULL, 0 )) { say( "quit\n" ); return FALSE; }
    if (!(spool = GdiGetSpoolFileHandle( pp->printer, pp->devmode, doc_name ))) { say( "no spool handle\n" ); return FALSE; }
    hdc = GdiGetDC( spool );
    count = GdiGetPageCount( spool );
    say( "pages %lu dc %d\n", count, hdc != NULL );
    di.lpszDocName = pp->document;
    if (!GdiStartDocEMF( spool, &di )) { say( "startdoc failed\n" ); ret = FALSE; }
    for (i = 1; ret && i <= count; i++)
    {
        HANDLE page = GdiGetPageHandle( spool, i, &type );
        HBRUSH brush = CreateSolidBrush( RGB( 255, 0, 0 ) );
        RECT mark = { 10, 10, 60, 60 };

        if (!GdiStartPageEMF( spool )) { ret = FALSE; break; }
        /* as HP's processor plays a page: into a sheet's size from the
         * printable area's origin; SG_PRINTPROC_NORECT: no rectangle */
        {
            RECT sheet;
            SetRect( &sheet, 0, 0, GetDeviceCaps( hdc, PHYSICALWIDTH ), GetDeviceCaps( hdc, PHYSICALHEIGHT ) );
            RECT empty = { 0, 0, -1, -1 };  /* Lexmark's processor passes these */
            if (GetEnvironmentVariableW( L"SG_PRINTPROC_EMPTYRECT", NULL, 0 ))
                say( "page %lu played %d\n", i, GdiPlayPageEMF( spool, page, &empty, &empty, &empty ) );
            else
                say( "page %lu played %d\n", i, GdiPlayPageEMF( spool, page,
                     GetEnvironmentVariableW( L"SG_PRINTPROC_NORECT", NULL, 0 ) ? NULL : &sheet, NULL, NULL ) );
        }
        FillRect( hdc, &mark, brush );    /* the processor's own mark */
        DeleteObject( brush );
        if (!GdiEndPageEMF( spool, 0 )) ret = FALSE;
    }
    if (!GdiEndDocEMF( spool )) ret = FALSE;
    GdiDeleteSpoolFileHandle( spool );
    say( "done %d\n", ret );
    return ret;
}

__declspec(dllexport) BOOL WINAPI ClosePrintProcessor( HANDLE handle )
{
    struct pp *pp = handle;
    free( pp->devmode );
    free( pp );
    return TRUE;
}

__declspec(dllexport) BOOL WINAPI ControlPrintProcessor( HANDLE handle, DWORD command )
{
    return TRUE;
}
