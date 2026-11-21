/* gdipenum-probe: GDI+ flat API metafile enumeration variants and GdipConvertToEmfPlusToStream that were spec
 * stubs (Stained Glass OS wine-sg 8238; test/gdipenum-gate.sh). An EMF drawn with GDI is enumerated through
 * every variant (the same records each time); the EMF converted to EMF+ into a memory stream.
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#define COBJMACROS
#include <windows.h>
#include <objbase.h>
#include <stdio.h>

typedef struct { float X, Y; } PF;
typedef struct { INT X, Y; } PI;
typedef struct { float X, Y, Width, Height; } RF;
typedef struct { INT X, Y, Width, Height; } RI;
typedef BOOL (CALLBACK *ENUMPROC)(int type, UINT flags, UINT size, const BYTE *data, void *ctx);
typedef struct { UINT32 GdiplusVersion; void *DebugEventCallback; BOOL SuppressBackgroundThread, SuppressExternalCodecs; } STARTUP;

static int failures;
static void check( int ok, const char *what, long got )
{
    printf( "%s  %s (%ld)\n", ok ? "PASS" : "FAIL", what, got );
    if (!ok) failures++;
}

static int records;
static BOOL CALLBACK count_proc( int type, UINT flags, UINT size, const BYTE *data, void *ctx )
{
    records++;
    return TRUE;
}

#define GP(name) name = (void *)GetProcAddress( gp, #name )

int main( void )
{
    HMODULE gp = LoadLibraryA( "gdiplus.dll" );
    int (WINAPI *GdiplusStartup)(ULONG_PTR *, const STARTUP *, void *);
    int (WINAPI *GdipCreateMetafileFromEmf)(HENHMETAFILE, BOOL, void **);
    int (WINAPI *GdipCreateBitmapFromScan0)(INT, INT, INT, INT, BYTE *, void **);
    int (WINAPI *GdipGetImageGraphicsContext)(void *, void **);
    int (WINAPI *GdipEnumerateMetafileDestRect)(void *, void *, const RF *, ENUMPROC, void *, void *);
    int (WINAPI *GdipEnumerateMetafileDestPoints)(void *, void *, const PF *, INT, ENUMPROC, void *, void *);
    int (WINAPI *GdipEnumerateMetafileDestPointsI)(void *, void *, const PI *, INT, ENUMPROC, void *, void *);
    int (WINAPI *GdipEnumerateMetafileSrcRectDestPoint)(void *, void *, const PF *, const RF *, int, ENUMPROC, void *, void *);
    int (WINAPI *GdipEnumerateMetafileSrcRectDestPointI)(void *, void *, const PI *, const RI *, int, ENUMPROC, void *, void *);
    int (WINAPI *GdipEnumerateMetafileSrcRectDestPointsI)(void *, void *, const PI *, INT, const RI *, int, ENUMPROC, void *, void *);
    int (WINAPI *GdipConvertToEmfPlusToStream)(void *, void *, INT *, IStream *, int, const WCHAR *, void **);
    int (WINAPI *GdipGetMetafileHeaderFromMetafile)(void *, BYTE *);
    STARTUP st = { 1 };
    ULONG_PTR token;
    HDC dc;
    HENHMETAFILE emf;
    RECT r = { 0, 0, 1000, 1000 };
    void *metafile = NULL, *bitmap = NULL, *graphics = NULL, *plus = NULL;
    RF dest = { 0, 0, 50, 50 }, srcf = { 0, 0, 20, 20 };
    PF pts[3] = { { 0, 0 }, { 50, 0 }, { 0, 50 } }, pt = { 5, 5 };
    PI ptsi[3] = { { 0, 0 }, { 50, 0 }, { 0, 50 } }, pti = { 5, 5 };
    RI srci = { 0, 0, 20, 20 };
    IStream *stream;
    STATSTG stat;
    INT ok = -1, ref, n, s;
    BYTE header[256];

    GP(GdiplusStartup); GP(GdipCreateMetafileFromEmf); GP(GdipCreateBitmapFromScan0); GP(GdipGetImageGraphicsContext);
    GP(GdipEnumerateMetafileDestRect); GP(GdipEnumerateMetafileDestPoints); GP(GdipEnumerateMetafileDestPointsI);
    GP(GdipEnumerateMetafileSrcRectDestPoint); GP(GdipEnumerateMetafileSrcRectDestPointI);
    GP(GdipEnumerateMetafileSrcRectDestPointsI); GP(GdipConvertToEmfPlusToStream); GP(GdipGetMetafileHeaderFromMetafile);
    CoInitialize( NULL );
    GdiplusStartup( &token, &st, NULL );

    dc = CreateEnhMetaFileW( NULL, NULL, &r, NULL );
    Rectangle( dc, 1, 1, 10, 10 );
    Ellipse( dc, 2, 2, 8, 8 );
    emf = CloseEnhMetaFile( dc );
    GdipCreateMetafileFromEmf( emf, TRUE, &metafile );
    GdipCreateBitmapFromScan0( 64, 64, 0, 0x26200a /* PixelFormat32bppARGB */, NULL, &bitmap );
    GdipGetImageGraphicsContext( bitmap, &graphics );
    check( metafile && graphics, "an EMF and a graphics", 0 );

    records = 0; s = GdipEnumerateMetafileDestRect( graphics, metafile, &dest, count_proc, NULL, NULL ); ref = records;
    check( s == 0 && ref > 2, "GdipEnumerateMetafileDestRect: the reference count of records", ref );
    records = 0; s = GdipEnumerateMetafileDestPoints( graphics, metafile, pts, 3, count_proc, NULL, NULL );
    check( s == 0 && records == ref, "GdipEnumerateMetafileDestPoints", records );
    records = 0; s = GdipEnumerateMetafileDestPointsI( graphics, metafile, ptsi, 3, count_proc, NULL, NULL );
    check( s == 0 && records == ref, "GdipEnumerateMetafileDestPointsI", records );
    records = 0; s = GdipEnumerateMetafileSrcRectDestPoint( graphics, metafile, &pt, &srcf, 2 /* UnitPixel */, count_proc, NULL, NULL );
    check( s == 0 && records == ref, "GdipEnumerateMetafileSrcRectDestPoint", records );
    records = 0; s = GdipEnumerateMetafileSrcRectDestPointI( graphics, metafile, &pti, &srci, 2, count_proc, NULL, NULL );
    check( s == 0 && records == ref, "GdipEnumerateMetafileSrcRectDestPointI", records );
    records = 0; s = GdipEnumerateMetafileSrcRectDestPointsI( graphics, metafile, ptsi, 3, &srci, 2, count_proc, NULL, NULL );
    check( s == 0 && records == ref, "GdipEnumerateMetafileSrcRectDestPointsI", records );
    n = GdipEnumerateMetafileDestPointsI( graphics, metafile, ptsi, 2, count_proc, NULL, NULL );
    check( n == 2 /* InvalidParameter */, "two points: InvalidParameter", n );

    CreateStreamOnHGlobal( NULL, TRUE, &stream );
    s = GdipConvertToEmfPlusToStream( graphics, metafile, &ok, stream, 5 /* EmfTypeEmfPlusDual */, L"SG", &plus );
    IStream_Stat( stream, &stat, STATFLAG_NONAME );
    printf( "      converted: status %d, success %d, %lu bytes\n", s, ok, stat.cbSize.LowPart );
    check( s == 0 && plus && stat.cbSize.LowPart > 100, "GdipConvertToEmfPlusToStream records into the stream", s );
    if (plus)
    {
        memset( header, 0, sizeof(header) );
        GdipGetMetafileHeaderFromMetafile( plus, header );
        check( *(int *)header == 5 /* MetafileTypeEmfPlusDual */, "the result is an EMF+ dual metafile", *(int *)header );
    }
    printf( "RESULT: %s\n", failures ? "FAIL" : "PASS" );
    return failures != 0;
}
