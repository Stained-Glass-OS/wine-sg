/* A Unidrv render plug-in of our own built as Lexmark's universal driver is
 * (patches/sg/1023): its GPD's personality is HP-GL/2, it hooks only
 * DrvBitBlt, notes "GLBLT" once a page and passes the blit back to the
 * core, which then sends the page as raster.  Our own code. */
#define COBJMACROS
#include <stdio.h>
#include <string.h>
#include <windows.h>
#include <objbase.h>

static const GUID iid_uni = { 0xd67ebbf0, 0x78bf, 0x11d1, { 0x94, 0x80, 0x00, 0xa0, 0xc9, 0x06, 0x40, 0xb8 } };
static const GUID iid_core = { 0xd67ebbf1, 0x78bf, 0x11d1, { 0x94, 0x80, 0x00, 0xa0, 0xc9, 0x06, 0x40, 0xb8 } };

typedef LONG_PTR (WINAPI *PFN)(void);
typedef struct { ULONG iFunc; PFN pfn; } DRVFN;
typedef struct { ULONG iDriverVersion; ULONG c; DRVFN *pdrvfn; } DRVENABLEDATA;
typedef struct { DWORD size; void *oem_pdev; HANDLE engine, printer, module; DEVMODEW *pub; void *oem_dm; void *procs; } DEVOBJ;
typedef struct
{
    void *dhsurf; void *hsurf; void *dhpdev; void *hdev; SIZEL sizlBitmap; ULONG cjBits; void *pvBits;
    void *pvScan0; LONG lDelta; ULONG iUniq; ULONG iBitmapFormat; USHORT iType; USHORT fjBitmap;
} SURFOBJ;
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

static void say( DEVOBJ *dev, const char *s )
{
    DWORD w;
    if (helper) helper->lpVtbl->DrvWriteSpoolBuf( helper, dev, (void *)s, strlen( s ), &w );
}

static BOOL (WINAPI *core_bit_blt)( SURFOBJ *, SURFOBJ *, SURFOBJ *, void *, void *, RECTL *, POINTL *, POINTL *,
                                    void *, POINTL *, ULONG );
static int noted;

static BOOL WINAPI hook_bit_blt( SURFOBJ *dst, SURFOBJ *src, SURFOBJ *mask, void *clip, void *xlate, RECTL *rect,
                                 POINTL *pt, POINTL *mpt, void *brush, POINTL *bpt, ULONG rop )
{
    if (!noted++) say( dst->dhpdev, "GLBLT\n" );
    return core_bit_blt( dst, src, mask, clip, xlate, rect, pt, mpt, brush, bpt, rop );
}

static DRVFN hooks[] = { { 18, (PFN)hook_bit_blt } };

typedef struct uni uni;
struct uni { const void *lpVtbl; LONG ref; };

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
    if (mode == 1) { *(DWORD *)buf = 0x4c474753; return S_OK; }
    if (mode == 3) { *(DWORD *)buf = 1; return S_OK; }
    return E_NOTIMPL;
}
static HRESULT STDMETHODCALLTYPE dev_mode( uni *u, DWORD mode, OEMDMPARAM *p )
{
    if (mode == 1) { p->out_size = 16; return S_OK; }
    if (mode == 2 && p->oem_out && p->out_size >= 16)
    {
        DWORD *d = p->oem_out;
        d[0] = 16; d[1] = 0x4c474753; d[2] = 1; d[3] = 7;
    }
    return S_OK;
}
static HRESULT STDMETHODCALLTYPE enable_driver( uni *u, DWORD ver, DWORD size, DRVENABLEDATA *ded )
{
    ded->iDriverVersion = 0x20000;
    ded->c = sizeof(hooks) / sizeof(hooks[0]);
    ded->pdrvfn = hooks;
    return S_OK;
}
static HRESULT STDMETHODCALLTYPE disable_driver( uni *u ) { return S_OK; }
static HRESULT STDMETHODCALLTYPE enable_pdev( uni *u, DEVOBJ *dev, WCHAR *name, ULONG n, void *pats, ULONG cg,
                                              void *gi, ULONG cd, void *di, DRVENABLEDATA *ded, void **oem )
{
    ULONG i;
    for (i = 0; i < ded->c; i++)
        if (ded->pdrvfn[i].iFunc == 18) core_bit_blt = (void *)ded->pdrvfn[i].pfn;
    *oem = u;
    return core_bit_blt ? S_OK : E_FAIL;
}
static HRESULT STDMETHODCALLTYPE disable_pdev( uni *u, DEVOBJ *dev ) { return S_OK; }
static HRESULT STDMETHODCALLTYPE reset_pdev( uni *u, DEVOBJ *a, DEVOBJ *b ) { return S_OK; }
static HRESULT STDMETHODCALLTYPE publish( uni *u, IUnknown *unk )
{
    return IUnknown_QueryInterface( unk, &iid_core, (void **)&helper );
}
static HRESULT STDMETHODCALLTYPE implemented( uni *u, char *name ) { return S_FALSE; }
static HRESULT STDMETHODCALLTYPE notimpl( void ) { return E_NOTIMPL; }

static const void *vtbl[] =
{
    qi, addref, release, get_info, dev_mode, enable_driver, disable_driver, enable_pdev, disable_pdev, reset_pdev,
    publish, implemented, notimpl, notimpl /* CommandCallback */, notimpl, notimpl, notimpl, notimpl, notimpl,
    notimpl, notimpl, notimpl, notimpl, notimpl, notimpl, notimpl, notimpl, notimpl, notimpl, notimpl,
};
static uni the_uni = { vtbl, 1 };

typedef struct { const void *lpVtbl; } factory;
static HRESULT STDMETHODCALLTYPE f_qi( factory *f, REFIID riid, void **out ) { *out = f; return S_OK; }
static ULONG STDMETHODCALLTYPE f_addref( factory *f ) { return 2; }
static ULONG STDMETHODCALLTYPE f_release( factory *f ) { return 1; }
static HRESULT STDMETHODCALLTYPE f_create( factory *f, IUnknown *outer, REFIID riid, void **out )
{
    return qi( &the_uni, riid, out );
}
static HRESULT STDMETHODCALLTYPE f_lock( factory *f, BOOL lock ) { return S_OK; }
static const void *fvtbl[] = { f_qi, f_addref, f_release, f_create, f_lock };
static factory the_factory = { fvtbl };

HRESULT WINAPI DllGetClassObject( REFCLSID clsid, REFIID riid, void **out )
{
    *out = &the_factory;
    return S_OK;
}
