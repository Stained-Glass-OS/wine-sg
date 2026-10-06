/* A Unidrv render plug-in of our own, for the printdrv gate (patches/sg/0921):
 * it hooks DrvStartDoc (writing "HOOK START" before Unidrv's own start),
 * turns each band to one bit per pixel in ImageProcessing (a DIB, a set bit
 * white) and reports each row with ink from FilterGraphics (the device's
 * bits, a set bit ink) as "ROW <y> <ink pixels>".  Our own code. */
#define COBJMACROS
#include <stdio.h>
#include <string.h>
#include <windows.h>
#include <objbase.h>

static const GUID clsid_render = { 0x6d6abf26, 0x9f38, 0x11d1, { 0x88, 0x2a, 0x00, 0xc0, 0x4f, 0xb9, 0x61, 0xec } };
static const GUID iid_uni = { 0xd67ebbf0, 0x78bf, 0x11d1, { 0x94, 0x80, 0x00, 0xa0, 0xc9, 0x06, 0x40, 0xb8 } };
static const GUID iid_core = { 0xd67ebbf1, 0x78bf, 0x11d1, { 0x94, 0x80, 0x00, 0xa0, 0xc9, 0x06, 0x40, 0xb8 } };

typedef LONG_PTR (WINAPI *PFN)(void);
typedef struct { ULONG iFunc; PFN pfn; } DRVFN;
typedef struct { ULONG iDriverVersion; ULONG c; DRVFN *pdrvfn; } DRVENABLEDATA;
typedef struct { DWORD size; void *oem_pdev; HANDLE engine, printer, module; DEVMODEW *pub; void *oem_dm; void *procs; } DEVOBJ;
typedef struct { void *dhsurf, *hsurf, *dhpdev; } SURFOBJ_HEAD;
typedef struct { DWORD size; POINT offset; char *halftone; BOOL banding, blank; } IPPARAMS;
typedef struct { DWORD size; void *drvobj; HANDLE printer, module; DEVMODEW *in, *out; void *oem_in, *oem_out; DWORD out_size; } OEMDMPARAM;

typedef struct core core;
typedef struct
{
    HRESULT (STDMETHODCALLTYPE *QueryInterface)( core *, REFIID, void ** );
    ULONG (STDMETHODCALLTYPE *AddRef)( core * );
    ULONG (STDMETHODCALLTYPE *Release)( core * );
    HRESULT (STDMETHODCALLTYPE *DrvGetDriverSetting)( core *, void *, const char *, void *, DWORD, DWORD *, DWORD * );
    HRESULT (STDMETHODCALLTYPE *DrvWriteSpoolBuf)( core *, DEVOBJ *, void *, DWORD, DWORD * );
} core_vtbl;
struct core { const core_vtbl *lpVtbl; };

static core *helper;
static BOOL (WINAPI *core_start_doc)( SURFOBJ_HEAD *, WCHAR *, DWORD );
static BYTE *band;

static void say( DEVOBJ *dev, const char *s )
{
    DWORD w;
    if (helper) helper->lpVtbl->DrvWriteSpoolBuf( helper, dev, (void *)s, strlen( s ), &w );
}

static BOOL WINAPI hook_start_doc( SURFOBJ_HEAD *so, WCHAR *name, DWORD job )
{
    say( so->dhpdev, "HOOK START\n" );
    return core_start_doc( so, name, job );
}

static DRVFN hooks[] = { { 35, (PFN)hook_start_doc } };

typedef struct uni uni;
struct uni { const void *lpVtbl; LONG ref; };
static int row;

static HRESULT STDMETHODCALLTYPE qi( uni *u, REFIID riid, void **out )
{
    if (IsEqualGUID( riid, &IID_IUnknown ) || IsEqualGUID( riid, &iid_uni )) { *out = u; u->ref++; return S_OK; }
    *out = NULL;
    return E_NOINTERFACE;
}
static ULONG STDMETHODCALLTYPE addref( uni *u ) { return ++u->ref; }
static ULONG STDMETHODCALLTYPE release( uni *u ) { return --u->ref; }
static HRESULT STDMETHODCALLTYPE get_info( uni *u, DWORD mode, void *buf, DWORD size, DWORD *needed )
{
    if (needed) *needed = 4;
    if (!buf || size < 4) return E_FAIL;
    if (mode == 1) { *(DWORD *)buf = 0x50544753; return S_OK; }
    if (mode == 3) { *(DWORD *)buf = 1; return S_OK; }
    return E_NOTIMPL;
}
static HRESULT STDMETHODCALLTYPE dev_mode( uni *u, DWORD mode, OEMDMPARAM *p )
{
    if (mode == 1) { p->out_size = 16; return S_OK; }
    if (mode == 2 && p->oem_out && p->out_size >= 16)
    {
        DWORD *d = p->oem_out;
        d[0] = 16; d[1] = 0x50544753; d[2] = 1; d[3] = 7;
        return S_OK;
    }
    return S_OK;
}
static HRESULT STDMETHODCALLTYPE enable_driver( uni *u, DWORD ver, DWORD size, DRVENABLEDATA *ded )
{
    ded->iDriverVersion = 0x20000;
    ded->c = 1;
    ded->pdrvfn = hooks;
    return S_OK;
}
static HRESULT STDMETHODCALLTYPE disable_driver( uni *u ) { return S_OK; }
static HRESULT STDMETHODCALLTYPE enable_pdev( uni *u, DEVOBJ *dev, WCHAR *name, ULONG n, void *pats, ULONG cg,
                                              void *gi, ULONG cd, void *di, DRVENABLEDATA *ded, void **oem )
{
    ULONG i;
    for (i = 0; i < ded->c; i++) if (ded->pdrvfn[i].iFunc == 35) core_start_doc = (void *)ded->pdrvfn[i].pfn;
    *oem = u;
    return core_start_doc ? S_OK : E_FAIL;
}
static HRESULT STDMETHODCALLTYPE disable_pdev( uni *u, DEVOBJ *dev ) { return S_OK; }
static HRESULT STDMETHODCALLTYPE reset_pdev( uni *u, DEVOBJ *a, DEVOBJ *b ) { return S_OK; }
static HRESULT STDMETHODCALLTYPE publish( uni *u, IUnknown *unk )
{
    return IUnknown_QueryInterface( unk, &iid_core, (void **)&helper );
}
static HRESULT STDMETHODCALLTYPE implemented( uni *u, char *name )
{
    return !strcmp( name, "ImageProcessing" ) || !strcmp( name, "FilterGraphics" ) ? S_OK : S_FALSE;
}
static HRESULT STDMETHODCALLTYPE notimpl( void ) { return E_NOTIMPL; }
static HRESULT STDMETHODCALLTYPE image_processing( uni *u, DEVOBJ *dev, BYTE *bits, BITMAPINFOHEADER *bih,
                                                   BYTE *colors, DWORD id, IPPARAMS *ip, BYTE **result )
{
    int w = bih->biWidth, h = abs( bih->biHeight ), stride = (w * 3 + 3) & ~3, ostride = ((w + 31) / 32) * 4, x, y;
    int dark = 0;
    char buf[64];

    /* the band comes back as a DIB, a set bit white, as a maker's plug-in
     * returns it */
    free( band );
    band = calloc( ostride, h );
    for (y = 0; y < h; y++)
        for (x = 0; x < w; x++)
        {
            BYTE *p = bits + y * stride + x * 3;
            if (p[0] + p[1] + p[2] < 3 * 128) dark++;
            else band[y * ostride + x / 8] |= 0x80 >> (x % 8);
        }
    snprintf( buf, sizeof(buf), "IP %dx%d id %lu dark %d\n", w, h, id, dark );
    say( dev, buf );
    row = 0;
    *result = band;
    return S_OK;
}
static HRESULT STDMETHODCALLTYPE filter_graphics( uni *u, DEVOBJ *dev, BYTE *buf, DWORD len )
{
    int ink = 0;
    DWORD i;
    char line[64];

    for (i = 0; i < len; i++) { BYTE b = buf[i]; while (b) { ink += b & 1; b >>= 1; } }
    if (ink)
    {
        snprintf( line, sizeof(line), "ROW %d %d\n", row, ink );
        say( dev, line );
    }
    row++;
    return S_OK;
}

static const void *vtbl[] =
{
    qi, addref, release, get_info, dev_mode, enable_driver, disable_driver, enable_pdev, disable_pdev, reset_pdev,
    publish, implemented, notimpl /* DriverDMS */, notimpl /* CommandCallback */, image_processing, filter_graphics,
    notimpl, notimpl, notimpl, notimpl, notimpl, notimpl, notimpl, notimpl, notimpl, notimpl,
};
static uni the_uni = { vtbl, 1 };

typedef struct { const void *lpVtbl; } factory;
static HRESULT STDMETHODCALLTYPE f_qi( factory *f, REFIID riid, void **out )
{
    if (IsEqualGUID( riid, &IID_IUnknown ) || IsEqualGUID( riid, &IID_IClassFactory )) { *out = f; return S_OK; }
    *out = NULL;
    return E_NOINTERFACE;
}
static ULONG STDMETHODCALLTYPE f_ref( factory *f ) { return 1; }
static HRESULT STDMETHODCALLTYPE f_create( factory *f, IUnknown *outer, REFIID riid, void **out )
{
    return qi( &the_uni, riid, out );
}
static HRESULT STDMETHODCALLTYPE f_lock( factory *f, BOOL lock ) { return S_OK; }
static const void *f_vtbl[] = { f_qi, f_ref, f_ref, f_create, f_lock };
static factory the_factory = { f_vtbl };

HRESULT WINAPI DllGetClassObject( REFCLSID clsid, REFIID riid, void **out )
{
    if (!IsEqualGUID( clsid, &clsid_render )) return CLASS_E_CLASSNOTAVAILABLE;
    return f_qi( &the_factory, riid, out );
}

HRESULT WINAPI DllCanUnloadNow( void ) { return S_FALSE; }
