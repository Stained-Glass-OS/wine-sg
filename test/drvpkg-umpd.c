/* A printer driver of our own built the way makers build theirs, for the
 * drvpkg gate (patches/sg/1020-1022).  Unlike printdrv-umpd.c it:
 *   - draws on a surface of its own (EngCreateDeviceSurface) and hooks
 *     DrvCopyBits, so the page reaches it as a picture, which it hands to
 *     the engine (EngCopyBits) to put on a one-bit bitmap of its own, with
 *     the colour translation it was given;
 *   - writes to its printer handle with WritePrinter, not EngWritePrinter;
 *   - reads its data file with EngGetPrinterDataFileName, EngLoadModule and
 *     EngMapModule, and asks the time (EngQueryLocalTime);
 *   - is its own configuration DLL and, like makers' ones, reads the printer
 *     through the handle DocumentProperties gives it;
 *   - hears GDI's document events (DrvDocumentEvent) and notes them;
 *   - keeps a setting of its own (a density) behind the public settings,
 *     refuses a DEVMODE without it, and looks its job up (GetJob) when the
 *     document starts, as makers' do.
 * Its report, written to the printer:
 *   SGM START <document> data=<first line of the data file> time=<ok|bad>
 *             density=<its own setting> job=<ok|missing>
 *             glyphs=<ok|bad|none: EngComputeGlyphSet's answer for 1252>
 *   PAGE <n> <width>x<height> dark=<pixels> box=<l>,<t>-<r>,<b>
 *   SGM END
 * Our own code. */
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
typedef struct { ULONG iUniq; RECTL rclBounds; BYTE iDComplexity, iFComplexity, iMode, fjOptions; } CLIPOBJ;
typedef struct { ULONG iUniq; ULONG flXlate; USHORT iSrcType, iDstType; ULONG cEntries; ULONG *pulXlate; } XLATEOBJ;
typedef struct { USHORT y, mo, d, h, mi, s, ms, wd; } ENG_TIME_FIELDS;
typedef struct { WCHAR wcLow; USHORT cGlyphs; ULONG *phg; } WCRUN;
typedef struct { ULONG cjThis, flAccel, cGlyphsSupported, cRuns; WCRUN awcrun[1]; } FD_GLYPHSET;

#define BMF_1BPP 1
#define BMF_TOPDOWN 1
#define PAL_INDEXED 1
#define HOOK_BITBLT 0x0001
#define HOOK_COPYBITS 0x0400

void *WINAPI EngCreateBitmap( SIZEL, LONG, ULONG, ULONG, void * );
void *WINAPI EngCreateDeviceSurface( void *, SIZEL, ULONG );
BOOL WINAPI EngAssociateSurface( void *, void *, ULONG );
BOOL WINAPI EngDeleteSurface( void * );
SURFOBJ *WINAPI EngLockSurface( void * );
void WINAPI EngUnlockSurface( SURFOBJ * );
BOOL WINAPI EngCopyBits( SURFOBJ *, SURFOBJ *, CLIPOBJ *, XLATEOBJ *, RECTL *, POINTL * );
HPALETTE WINAPI EngCreatePalette( ULONG, ULONG, ULONG *, ULONG, ULONG, ULONG );
BOOL WINAPI EngDeletePalette( HPALETTE );
WCHAR *WINAPI EngGetPrinterDataFileName( void * );
HANDLE WINAPI EngLoadModule( WCHAR * );
void *WINAPI EngMapModule( HANDLE, ULONG * );
void WINAPI EngFreeModule( HANDLE );
void WINAPI EngQueryLocalTime( ENG_TIME_FIELDS * );
FD_GLYPHSET *WINAPI EngComputeGlyphSet( INT, INT, INT );
HANDLE WINAPI EngCreateSemaphore( void );
void WINAPI EngAcquireSemaphore( HANDLE );
void WINAPI EngReleaseSemaphore( HANDLE );
void WINAPI EngDeleteSemaphore( HANDLE );

struct pdev
{
    HANDLE printer;
    void *hdev;
    void *surface;    /* the device surface GDI sees */
    void *bitmap;     /* our own one-bit bitmap behind it */
    HPALETTE palette;
    HANDLE sem;
    char data[64];
    int page;
    int density;
    const char *glyphs;
};

/* its device font's characters, from a code page, as Zebra's driver asks:
 * the euro sign is 0x80 and 'A' 0x41 in code page 1252 */
static const char *check_glyphs( void )
{
    FD_GLYPHSET *set = EngComputeGlyphSet( 1252, 32, 224 );
    ULONG i, j, total = 0, euro = 0, a = 0;

    if (!set) return "none";
    for (i = 0; i < set->cRuns; i++)
    {
        for (j = 0; j < set->awcrun[i].cGlyphs; j++)
        {
            WCHAR wc = set->awcrun[i].wcLow + j;
            if (wc == 0x20ac) euro = set->awcrun[i].phg[j];
            if (wc == 'A') a = set->awcrun[i].phg[j];
        }
        total += set->awcrun[i].cGlyphs;
    }
    return total == set->cGlyphsSupported && total > 200 && euro == 0x80 && a == 0x41 ? "ok" : "bad";
}

/* the driver's own settings, after the public ones */
struct sgpriv
{
    DWORD magic;
    DWORD density;
};
#define SGPRIV_MAGIC 0x4b4d4753 /* SGMK */

static const struct sgpriv *private_settings( const DEVMODEW *dm )
{
    const struct sgpriv *priv;

    if (!dm || dm->dmDriverExtra < sizeof(*priv)) return NULL;
    priv = (const struct sgpriv *)((const BYTE *)dm + dm->dmSize);
    return priv->magic == SGPRIV_MAGIC ? priv : NULL;
}

/* what happens, in order, in HKCU\Software\SG Test Maker, value Events:
 * GDI's document events (DrvDocumentEvent, their numbers) and R when the
 * job is rendered */
static void note_event( const WCHAR *what )
{
    WCHAR events[512] = L"";
    DWORD size = sizeof(events) - 64;
    HKEY key;

    if (RegCreateKeyExW( HKEY_CURRENT_USER, L"Software\\SG Test Maker", 0, NULL, 0, KEY_ALL_ACCESS, NULL, &key,
                         NULL )) return;
    if (RegQueryValueExW( key, L"Events", NULL, NULL, (BYTE *)events, &size )) events[0] = 0;
    if (events[0]) lstrcatW( events, L"," );
    lstrcatW( events, what );
    RegSetValueExW( key, L"Events", 0, REG_SZ, (BYTE *)events, (lstrlenW( events ) + 1) * sizeof(WCHAR) );
    RegCloseKey( key );
}

static void out( struct pdev *p, const char *s )
{
    DWORD w;
    WritePrinter( p->printer, (void *)s, strlen( s ), &w );
}

static void *WINAPI enable_pdev( DEVMODEW *dm, WCHAR *addr, ULONG npat, void **pats, ULONG cjcaps, GDIINFO *gi,
                                 ULONG cjdev, DEVINFO *di, void *hdev, WCHAR *name, HANDLE driver )
{
    struct pdev *p;
    ULONG colors[2] = { 0xffffff, 0 };  /* 0 paper, 1 ink */
    ULONG i;

    /* GDI asks for the six hatch brushes; makers' drivers fill the array in
     * without looking (OKI's) */
    if (npat < 6 || !pats) return NULL;
    /* like Brother's label printers' driver, it cannot work without its own
     * settings: GDI hands it a DEVMODE the driver has merged them into */
    if (!private_settings( dm )) return NULL;
    for (i = 0; i < npat; i++) pats[i] = NULL;
    p = calloc( 1, sizeof(*p) );

    p->printer = driver;
    p->hdev = hdev;
    p->density = private_settings( dm )->density;
    p->glyphs = check_glyphs();
    memset( gi, 0, cjcaps );
    gi->ulVersion = 0x5000;
    gi->ulTechnology = DT_RASPRINTER;
    gi->ulHorzSize = 51; gi->ulVertSize = 25;
    gi->ulHorzRes = 200; gi->ulVertRes = 100;
    gi->cBitsPixel = 1; gi->cPlanes = 1; gi->ulNumColors = 2;
    gi->flRaster = RC_BITBLT | RC_STRETCHDIB | RC_DIBTODEV;
    gi->ulLogPixelsX = gi->ulLogPixelsY = 100;
    gi->ulAspectX = gi->ulAspectY = 100; gi->ulAspectXY = 141;
    gi->szlPhysSize.cx = 210; gi->szlPhysSize.cy = 110;
    gi->ptlPhysOffset.x = 5; gi->ptlPhysOffset.y = 5;
    memset( di, 0, cjdev );
    di->iDitherFormat = BMF_1BPP;
    p->palette = EngCreatePalette( PAL_INDEXED, 2, colors, 0, 0, 0 );
    di->hpal = p->palette;
    p->sem = EngCreateSemaphore();
    return p;
}

static void WINAPI complete_pdev( struct pdev *p, void *hdev )
{
    WCHAR *data;
    HANDLE module;
    ULONG size;
    char *bytes;

    p->hdev = hdev;
    strcpy( p->data, "none" );
    /* the data file, as makers' drivers read theirs */
    if ((data = EngGetPrinterDataFileName( hdev )) && (module = EngLoadModule( data )))
    {
        if ((bytes = EngMapModule( module, &size )) && size)
        {
            int n = 0;
            while (n < size && n < sizeof(p->data) - 1 && bytes[n] != '\r' && bytes[n] != '\n') n++;
            memcpy( p->data, bytes, n );
            p->data[n] = 0;
        }
        EngFreeModule( module );
    }
}

static void WINAPI disable_pdev( struct pdev *p )
{
    EngDeletePalette( p->palette );
    EngDeleteSemaphore( p->sem );
    free( p );
}

static void *WINAPI enable_surface( struct pdev *p )
{
    SIZEL size = { 200, 100 };
    p->bitmap = EngCreateBitmap( size, 32, BMF_1BPP, BMF_TOPDOWN, NULL );
    p->surface = EngCreateDeviceSurface( p, size, BMF_1BPP );
    EngAssociateSurface( p->surface, p->hdev, HOOK_COPYBITS | HOOK_BITBLT );
    return p->surface;
}

static void WINAPI disable_surface( struct pdev *p )
{
    EngDeleteSurface( p->surface );
    EngDeleteSurface( p->bitmap );
}

/* the page arrives as a picture: the engine puts it on our bitmap */
static BOOL WINAPI copy_bits( SURFOBJ *dst, SURFOBJ *src, CLIPOBJ *clip, XLATEOBJ *xlate, RECTL *rect, POINTL *pt )
{
    struct pdev *p = dst->dhpdev ? dst->dhpdev : dst->dhsurf;
    SURFOBJ *mine = EngLockSurface( p->bitmap );
    BOOL ret;

    EngAcquireSemaphore( p->sem );
    ret = EngCopyBits( mine, src, clip, xlate, rect, pt );
    EngReleaseSemaphore( p->sem );
    EngUnlockSurface( mine );
    return ret;
}

static BOOL WINAPI bit_blt( SURFOBJ *dst, SURFOBJ *src, SURFOBJ *mask, CLIPOBJ *clip, XLATEOBJ *xlate, RECTL *rect,
                            POINTL *pt, POINTL *mpt, void *brush, POINTL *bpt, ULONG rop )
{
    return copy_bits( dst, src, clip, xlate, rect, pt );
}

static BOOL WINAPI start_doc( SURFOBJ *so, WCHAR *doc, DWORD job )
{
    struct pdev *p = so->dhpdev;
    ENG_TIME_FIELDS t;
    char buf[300], density[16];
    DWORD needed = 0;

    note_event( L"R" );
    EngQueryLocalTime( &t );
    snprintf( density, sizeof(density), "%d", p->density );
    GetJobW( p->printer, job, 1, NULL, 0, &needed );
    snprintf( buf, sizeof(buf), "SGM START %ls data=%s time=%s density=%s job=%s glyphs=%s\n", doc ? doc : L"",
              p->data, t.y >= 2020 && t.mo >= 1 && t.mo <= 12 ? "ok" : "bad", density, needed ? "ok" : "missing",
              p->glyphs );
    out( p, buf );
    return TRUE;
}

static BOOL WINAPI start_page( SURFOBJ *so )
{
    struct pdev *p = so->dhpdev;
    SURFOBJ *mine = EngLockSurface( p->bitmap );
    memset( mine->pvBits, 0, mine->cjBits );  /* paper */
    EngUnlockSurface( mine );
    p->page++;
    return TRUE;
}

static BOOL WINAPI send_page( SURFOBJ *so )
{
    struct pdev *p = so->dhpdev;
    SURFOBJ *mine = EngLockSurface( p->bitmap );
    int x, y, dark = 0, l = 9999, t = 9999, r = -1, b = -1;
    char buf[200];

    for (y = 0; y < mine->sizlBitmap.cy; y++)
    {
        BYTE *row = (BYTE *)mine->pvScan0 + y * mine->lDelta;
        for (x = 0; x < mine->sizlBitmap.cx; x++)
        {
            if (!(row[x / 8] & (0x80 >> (x % 8)))) continue;
            dark++;
            if (x < l) l = x;
            if (y < t) t = y;
            if (x > r) r = x;
            if (y > b) b = y;
        }
    }
    EngUnlockSurface( mine );
    snprintf( buf, sizeof(buf), "PAGE %d %ldx%ld dark=%d box=%d,%d-%d,%d\n", p->page, so->sizlBitmap.cx,
              so->sizlBitmap.cy, dark, l, t, r, b );
    out( p, buf );
    return TRUE;
}

static BOOL WINAPI end_doc( SURFOBJ *so, ULONG flags )
{
    out( so->dhpdev, "SGM END\n" );
    return TRUE;
}

static DRVFN fns[] =
{
    { 0, (PFN)enable_pdev }, { 1, (PFN)complete_pdev }, { 2, (PFN)disable_pdev },
    { 3, (PFN)enable_surface }, { 4, (PFN)disable_surface },
    { 18, (PFN)bit_blt }, { 19, (PFN)copy_bits },
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

/* configuration: one paper; like makers' UI DLLs it needs the printer's handle */
static BOOL printer_readable( HANDLE printer )
{
    DWORD needed = 0;
    GetPrinterW( printer, 2, NULL, 0, &needed );
    return needed != 0;
}

DWORD WINAPI DrvDeviceCapabilities( HANDLE printer, WCHAR *name, WORD cap, void *output, DEVMODEW *dm )
{
    if (!printer_readable( printer )) return GDI_ERROR;
    switch (cap)
    {
    case DC_PAPERS: if (output) *(WORD *)output = 257; return 1;
    case DC_PAPERSIZE: if (output) { ((POINT *)output)->x = 533; ((POINT *)output)->y = 279; } return 1;
    case DC_PAPERNAMES: if (output) lstrcpyW( output, L"SG Maker 2x1" ); return 1;
    case DC_SIZE: return sizeof(DEVMODEW);
    case DC_EXTRA: return sizeof(struct sgpriv);
    }
    return 0;
}

typedef struct { WORD cbSize, Reserved; HANDLE hPrinter; LPCWSTR pszPrinterName; DEVMODEW *pdmIn, *pdmOut;
                 DWORD cbOut, fMode; } DOCPROPHDR;

LONG WINAPI DrvDocumentPropertySheets( void *info, LPARAM lparam )
{
    DOCPROPHDR *dph = (DOCPROPHDR *)lparam;
    struct { DEVMODEW dm; struct sgpriv priv; } full;
    const struct sgpriv *in;
#define dm full.dm

    if (info || !dph) return 1;
    if (!printer_readable( dph->hPrinter )) return -1;
    memset( &dm, 0, sizeof(dm) );
    dm.dmSize = sizeof(dm);
    dm.dmSpecVersion = DM_SPECVERSION;
    dm.dmFields = DM_PAPERSIZE | DM_COPIES;
    dm.dmPaperSize = 257;
    dm.dmCopies = 1;
    lstrcpynW( dm.dmDeviceName, dph->pszPrinterName, CCHDEVICENAME );
    dm.dmDriverExtra = sizeof(full.priv);
    full.priv.magic = SGPRIV_MAGIC;
    full.priv.density = 3;
    if (!dph->fMode || !dph->pdmOut)
    {
        dph->cbOut = sizeof(full);
        return sizeof(full);
    }
    if ((dph->fMode & DM_IN_BUFFER) && dph->pdmIn && (dph->pdmIn->dmFields & DM_COPIES))
        dm.dmCopies = dph->pdmIn->dmCopies;
    if ((dph->fMode & DM_IN_BUFFER) && (in = private_settings( dph->pdmIn ))) full.priv.density = in->density;
    memcpy( dph->pdmOut, &full, sizeof(full) );
    return 1;
#undef dm
}

/* a new printer: the driver makes its own registry entry, as makers'
 * drivers do (Epson's); a deleted one: it takes it away */
BOOL WINAPI DrvPrinterEvent( WCHAR *name, INT event, DWORD flags, LPARAM lparam )
{
    HKEY key;

    if (RegCreateKeyExW( HKEY_LOCAL_MACHINE, L"Software\\SG Test Maker\\Printers", 0, NULL, 0, KEY_ALL_ACCESS, NULL,
                         &key, NULL )) return FALSE;
    if (event == 3 /* PRINTER_EVENT_INITIALIZE */)
        RegSetValueExW( key, name, 0, REG_SZ, (BYTE *)L"initialized", sizeof(L"initialized") );
    else if (event == 4 /* PRINTER_EVENT_DELETE */)
        RegDeleteValueW( key, name );
    RegCloseKey( key );
    return TRUE;
}

int WINAPI DrvDocumentEvent( HANDLE printer, HDC hdc, int esc, ULONG cb_in, void *in, ULONG cb_out, void *out )
{
    WCHAR num[16];

    wsprintfW( num, L"%d", esc );
    note_event( num );
    /* like Brother's and HP's, for a printer it has nothing to do for it
     * answers the DC's creation with failure: it hears nothing more */
    if (esc == 1 /* DOCUMENTEVENT_CREATEDCPRE */ && in && wcsstr( ((WCHAR **)in)[1], L"Quiet" ))
        return -1; /* DOCUMENTEVENT_FAILURE */
    return 1; /* DOCUMENTEVENT_SUCCESS */
}
