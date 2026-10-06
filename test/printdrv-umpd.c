/* A tiny user-mode printer graphics driver of our own, for the printdrv gate
 * (patches/sg/0920-0925).  It describes a 2 x 1 inch device at 100 dpi
 * (1 x 2 turned, landscape),
 * draws nothing itself (GDI's engine draws the pages into its surface), and
 * writes a text report of each page to the printer with EngWritePrinter:
 *   SGTD START <document>
 *   PAGE <n> <width>x<height> dark=<pixels> box=<l>,<t>-<r>,<b>
 *   SGTD END
 * It is also its own configuration DLL (DrvDeviceCapabilities and
 * DrvDocumentPropertySheets), so the INF names one file.  Our own code. */
#include <stdio.h>
#include <string.h>
#include <windows.h>
#include <winspool.h>

typedef LONG_PTR (WINAPI *PFN)(void);
typedef struct { ULONG iFunc; PFN pfn; } DRVFN;
typedef struct { ULONG iDriverVersion; ULONG c; DRVFN *pdrvfn; } DRVENABLEDATA;
typedef struct { LONG x, y, Y; } CIECHROMA;
typedef struct { CIECHROMA c[7]; LONG g[9]; } COLORINFO;
typedef struct
{
    ULONG ulVersion, ulTechnology, ulHorzSize, ulVertSize, ulHorzRes, ulVertRes, cBitsPixel, cPlanes,
          ulNumColors, flRaster, ulLogPixelsX, ulLogPixelsY, flTextCaps, ulDACRed, ulDACGreen, ulDACBlue,
          ulAspectX, ulAspectY, ulAspectXY;
    LONG xStyleStep, yStyleStep, denStyleStep;
    POINTL ptlPhysOffset;
    SIZEL szlPhysSize;
    ULONG ulNumPalReg;
    COLORINFO ciDevice;
    ULONG rest[14];
    BYTE *pats[3];
    ULONG tail[3];
} GDIINFO;
typedef struct { ULONG flGraphicsCaps; LOGFONTW f[3]; ULONG cFonts, iDitherFormat; USHORT cx, cy; HPALETTE hpal;
                 ULONG flGraphicsCaps2; } DEVINFO;
typedef struct
{
    void *dhsurf; void *hsurf; void *dhpdev; void *hdev; SIZEL sizlBitmap; ULONG cjBits; void *pvBits;
    void *pvScan0; LONG lDelta; ULONG iUniq; ULONG iBitmapFormat; USHORT iType; USHORT fjBitmap;
} SURFOBJ;

#define BMF_24BPP 5
#define BMF_TOPDOWN 1

void *WINAPI EngCreateBitmap( SIZEL, LONG, ULONG, ULONG, void * );
BOOL WINAPI EngAssociateSurface( void *, void *, ULONG );
BOOL WINAPI EngDeleteSurface( void * );
BOOL WINAPI EngWritePrinter( HANDLE, void *, DWORD, DWORD * );

struct pdev
{
    HANDLE printer;
    void *hdev;
    void *surface;
    int page;
    BOOL turned;
};

static void out( struct pdev *p, const char *s )
{
    DWORD w;
    EngWritePrinter( p->printer, (void *)s, strlen( s ), &w );
}

static void *WINAPI enable_pdev( DEVMODEW *dm, WCHAR *addr, ULONG npat, void **pats, ULONG cjcaps, GDIINFO *gi,
                                 ULONG cjdev, DEVINFO *di, void *hdev, WCHAR *name, HANDLE driver )
{
    struct pdev *p = calloc( 1, sizeof(*p) );
    p->printer = driver;
    p->hdev = hdev;
    memset( gi, 0, cjcaps );
    gi->ulVersion = 0x5000;
    gi->ulTechnology = DT_RASPRINTER;
    p->turned = dm && (dm->dmFields & DM_ORIENTATION) && dm->dmOrientation == DMORIENT_LANDSCAPE;
    gi->ulHorzSize = 51; gi->ulVertSize = 25;
    gi->ulHorzRes = 200; gi->ulVertRes = 100;
    gi->cBitsPixel = 24; gi->cPlanes = 1; gi->ulNumColors = (ULONG)-1;
    gi->flRaster = RC_BITBLT | RC_STRETCHDIB | RC_DIBTODEV;
    gi->ulLogPixelsX = gi->ulLogPixelsY = 100;
    gi->ulAspectX = gi->ulAspectY = 100; gi->ulAspectXY = 141;
    gi->szlPhysSize.cx = 210; gi->szlPhysSize.cy = 110;
    gi->ptlPhysOffset.x = 5; gi->ptlPhysOffset.y = 5;
    if (p->turned)
    {
        gi->ulHorzSize = 25; gi->ulVertSize = 51;
        gi->ulHorzRes = 100; gi->ulVertRes = 200;
        gi->szlPhysSize.cx = 110; gi->szlPhysSize.cy = 210;
    }
    memset( di, 0, cjdev );
    di->iDitherFormat = BMF_24BPP;
    return p;
}

static void WINAPI complete_pdev( struct pdev *p, void *hdev ) { p->hdev = hdev; }
static void WINAPI disable_pdev( struct pdev *p ) { free( p ); }

static void *WINAPI enable_surface( struct pdev *p )
{
    SIZEL size = { p->turned ? 100 : 200, p->turned ? 200 : 100 };
    p->surface = EngCreateBitmap( size, 600, BMF_24BPP, BMF_TOPDOWN, NULL );
    EngAssociateSurface( p->surface, p->hdev, 0 );
    return p->surface;
}

static void WINAPI disable_surface( struct pdev *p ) { EngDeleteSurface( p->surface ); }

static BOOL WINAPI start_doc( SURFOBJ *so, WCHAR *doc, DWORD job )
{
    char buf[300];
    snprintf( buf, sizeof(buf), "SGTD START %ls\n", doc ? doc : L"" );
    out( so->dhpdev, buf );
    return TRUE;
}

static BOOL WINAPI start_page( SURFOBJ *so )
{
    ((struct pdev *)so->dhpdev)->page++;
    return TRUE;
}

static BOOL WINAPI send_page( SURFOBJ *so )
{
    struct pdev *p = so->dhpdev;
    int x, y, dark = 0, l = 9999, t = 9999, r = -1, b = -1;
    char buf[200];

    for (y = 0; y < so->sizlBitmap.cy; y++)
    {
        BYTE *row = (BYTE *)so->pvScan0 + y * so->lDelta;
        for (x = 0; x < so->sizlBitmap.cx; x++)
        {
            if (row[x * 3] + row[x * 3 + 1] + row[x * 3 + 2] >= 3 * 128) continue;
            dark++;
            if (x < l) l = x;
            if (y < t) t = y;
            if (x > r) r = x;
            if (y > b) b = y;
        }
    }
    snprintf( buf, sizeof(buf), "PAGE %d %ldx%ld dark=%d box=%d,%d-%d,%d\n", p->page, so->sizlBitmap.cx,
              so->sizlBitmap.cy, dark, l, t, r, b );
    out( p, buf );
    return TRUE;
}

static BOOL WINAPI end_doc( SURFOBJ *so, ULONG flags )
{
    out( so->dhpdev, "SGTD END\n" );
    return TRUE;
}

static DRVFN fns[] =
{
    { 0, (PFN)enable_pdev }, { 1, (PFN)complete_pdev }, { 2, (PFN)disable_pdev },
    { 3, (PFN)enable_surface }, { 4, (PFN)disable_surface },
    { 35, (PFN)start_doc }, { 33, (PFN)start_page }, { 32, (PFN)send_page }, { 34, (PFN)end_doc },
};

BOOL WINAPI DrvEnableDriver( ULONG version, ULONG size, DRVENABLEDATA *ded )
{
    ded->iDriverVersion = 0x30000;
    ded->c = sizeof(fns) / sizeof(fns[0]);
    ded->pdrvfn = fns;
    return TRUE;
}

void WINAPI DrvDisableDriver( void ) {}

/* configuration: one paper, "SG Test 2x1" */
DWORD WINAPI DrvDeviceCapabilities( HANDLE printer, WCHAR *name, WORD cap, void *output, DEVMODEW *dm )
{
    switch (cap)
    {
    case DC_PAPERS: if (output) *(WORD *)output = 256; return 1;
    case DC_PAPERSIZE: if (output) { ((POINT *)output)->x = 533; ((POINT *)output)->y = 279; } return 1;
    case DC_PAPERNAMES: if (output) lstrcpyW( output, L"SG Test 2x1" ); return 1;
    case DC_ORIENTATION: return 90;
    case DC_SIZE: return sizeof(DEVMODEW);
    case DC_EXTRA: return 0;
    }
    return 0;
}

typedef struct { WORD cbSize, Reserved; HANDLE hPrinter; LPCWSTR pszPrinterName; DEVMODEW *pdmIn, *pdmOut;
                 DWORD cbOut, fMode; } DOCPROPHDR;

LONG WINAPI DrvDocumentPropertySheets( void *info, LPARAM lparam )
{
    DOCPROPHDR *dph = (DOCPROPHDR *)lparam;
    DEVMODEW dm;

    if (info || !dph) return 1;
    memset( &dm, 0, sizeof(dm) );
    dm.dmSize = sizeof(dm);
    dm.dmSpecVersion = DM_SPECVERSION;
    dm.dmFields = DM_PAPERSIZE | DM_COPIES | DM_ORIENTATION;
    dm.dmPaperSize = 256;
    dm.dmCopies = 1;
    dm.dmOrientation = DMORIENT_PORTRAIT;
    lstrcpynW( dm.dmDeviceName, dph->pszPrinterName, CCHDEVICENAME );
    if (!dph->fMode || !dph->pdmOut)
    {
        dph->cbOut = sizeof(dm);
        return sizeof(dm);
    }
    if (dph->pdmIn && (dph->fMode & DM_IN_BUFFER) && (dph->pdmIn->dmFields & DM_COPIES))
        dm.dmCopies = dph->pdmIn->dmCopies;
    if (dph->pdmIn && (dph->fMode & DM_IN_BUFFER) && (dph->pdmIn->dmFields & DM_ORIENTATION))
        dm.dmOrientation = dph->pdmIn->dmOrientation;
    memcpy( dph->pdmOut, &dm, sizeof(dm) );
    return 1;
}
