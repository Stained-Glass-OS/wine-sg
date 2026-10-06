/* A PostScript render plug-in of our own (IPrintOemPS2), for the psplugin
 * gate (patches/sg/1024), built as makers build theirs: it adds a line
 *   %SG<point> <ColorModel option> <PPD features> <bytes seen> <PageSize option>
 * at the job's injection points (the stream's start through the old
 * DRVPROCS table, the prolog, the setup, each page's setup, the end of
 * file, the stream's end), asks the core for the chosen ColorModel
 * (IPrintOemDriverPS) and for the PPD's features and the chosen PageSize
 * (IPrintCorePS2), and sees
 * the whole job through its WritePrinter, passing it on to the printer
 * handle it was given.  Our own code. */
#define COBJMACROS
#include <stdio.h>
#include <string.h>
#include <windows.h>
#include <winspool.h>
#include <objbase.h>

static const GUID iid_oem_ps2 = { 0xbecf7f34, 0x51b3, 0x46c9, { 0x8a, 0x1c, 0x18, 0x67, 0x9b, 0xd2, 0x1f, 0x36 } };
static const GUID iid_driver_ps = { 0xd90060c7, 0x8e1a, 0x11d1, { 0x88, 0x1f, 0x00, 0xc0, 0x4f, 0xb9, 0x61, 0xec } };
static const GUID iid_core_ps2 = { 0xcdbb0b0b, 0xa917, 0x40d7, { 0x9f, 0xbf, 0x48, 0x3b, 0x3b, 0xe7, 0xef, 0x22 } };

typedef LONG_PTR (WINAPI *PFN)(void);
typedef struct { ULONG iFunc; PFN pfn; } DRVFN;
typedef struct { ULONG iDriverVersion; ULONG c; DRVFN *pdrvfn; } DRVENABLEDATA;
typedef struct { DWORD (WINAPI *write)( void *, void *, DWORD ); } DRVPROCS;
typedef struct { DWORD size; void *oem_pdev; HANDLE engine, printer, module; DEVMODEW *pub; void *oem_dm; DRVPROCS *procs; } DEVOBJ;
typedef struct { DWORD size; void *drvobj; HANDLE printer, module; DEVMODEW *in, *out; void *oem_in, *oem_out; DWORD out_size; } OEMDMPARAM;

typedef struct helper helper;
typedef struct
{
    HRESULT (STDMETHODCALLTYPE *QueryInterface)( helper *, REFIID, void ** );
    ULONG (STDMETHODCALLTYPE *AddRef)( helper * );
    ULONG (STDMETHODCALLTYPE *Release)( helper * );
    HRESULT (STDMETHODCALLTYPE *DrvGetDriverSetting)( helper *, void *, const char *, void *, DWORD, DWORD *, DWORD * );
    HRESULT (STDMETHODCALLTYPE *DrvWriteSpoolBuf)( helper *, DEVOBJ *, void *, DWORD, DWORD * );
} helper_vtbl;
struct helper { const helper_vtbl *lpVtbl; };

typedef struct core2 core2;
typedef struct
{
    HRESULT (STDMETHODCALLTYPE *QueryInterface)( core2 *, REFIID, void ** );
    ULONG (STDMETHODCALLTYPE *AddRef)( core2 * );
    ULONG (STDMETHODCALLTYPE *Release)( core2 * );
    HRESULT (STDMETHODCALLTYPE *DrvWriteSpoolBuf)( core2 *, DEVOBJ *, void *, DWORD, DWORD * );
    HRESULT (STDMETHODCALLTYPE *GetOptions)( core2 *, DEVOBJ *, DWORD, const char *, DWORD, char *, DWORD, DWORD * );
    HRESULT (STDMETHODCALLTYPE *GetGlobalAttribute)( core2 *, DEVOBJ *, DWORD, const char *, DWORD *, BYTE *, DWORD, DWORD * );
    HRESULT (STDMETHODCALLTYPE *GetFeatureAttribute)( core2 *, DEVOBJ *, DWORD, const char *, const char *, DWORD *,
                                                      BYTE *, DWORD, DWORD * );
    HRESULT (STDMETHODCALLTYPE *GetOptionAttribute)( core2 *, DEVOBJ *, DWORD, const char *, const char *,
                                                     const char *, DWORD *, BYTE *, DWORD, DWORD * );
    HRESULT (STDMETHODCALLTYPE *EnumFeatures)( core2 *, DEVOBJ *, DWORD, char *, DWORD, DWORD * );
    HRESULT (STDMETHODCALLTYPE *EnumOptions)( core2 *, DEVOBJ *, DWORD, const char *, char *, DWORD, DWORD * );
} core2_vtbl;
struct core2 { const core2_vtbl *lpVtbl; };

static helper *drv;
static core2 *core;
static DWORD seen;

typedef struct ps ps;
struct ps { const void *lpVtbl; LONG ref; };

static HRESULT STDMETHODCALLTYPE qi( ps *p, REFIID riid, void **out )
{
    if (IsEqualGUID( riid, &IID_IUnknown ) || IsEqualGUID( riid, &iid_oem_ps2 )) { *out = p; p->ref++; return S_OK; }
    *out = NULL;
    return E_NOINTERFACE;
}
static ULONG STDMETHODCALLTYPE addref( ps *p ) { return ++p->ref; }
static ULONG STDMETHODCALLTYPE release( ps *p ) { return --p->ref; }
static HRESULT STDMETHODCALLTYPE get_info( ps *p, DWORD mode, void *buf, DWORD size, DWORD *needed )
{
    if (needed) *needed = 4;
    if (!buf || size < 4) return E_FAIL;
    if (mode == 1) { *(DWORD *)buf = 0x53505347; return S_OK; }
    if (mode == 3) { *(DWORD *)buf = 1; return S_OK; }
    return E_NOTIMPL;
}
static HRESULT STDMETHODCALLTYPE dev_mode( ps *p, DWORD mode, OEMDMPARAM *dm )
{
    if (mode == 1) { dm->out_size = 16; return S_OK; }
    if (mode == 2 && dm->oem_out && dm->out_size >= 16)
    {
        DWORD *d = dm->oem_out;
        d[0] = 16; d[1] = 0x53505347; d[2] = 1; d[3] = 0;
    }
    return S_OK;
}
static HRESULT STDMETHODCALLTYPE enable_driver( ps *p, DWORD ver, DWORD size, DRVENABLEDATA *ded )
{
    ded->iDriverVersion = 0x20000;
    ded->c = 0;
    ded->pdrvfn = NULL;
    return S_OK;
}
static HRESULT STDMETHODCALLTYPE disable_driver( ps *p ) { return S_OK; }
static HRESULT STDMETHODCALLTYPE enable_pdev( ps *p, DEVOBJ *dev, WCHAR *name, ULONG n, void *pats, ULONG cg,
                                              void *gi, ULONG cd, void *di, DRVENABLEDATA *ded, void **oem )
{
    *oem = p;
    return dev->oem_dm && ((DWORD *)dev->oem_dm)[1] == 0x53505347 ? S_OK : E_FAIL;
}
static HRESULT STDMETHODCALLTYPE disable_pdev( ps *p, DEVOBJ *dev ) { return S_OK; }
static HRESULT STDMETHODCALLTYPE reset_pdev( ps *p, DEVOBJ *a, DEVOBJ *b ) { return S_OK; }
static HRESULT STDMETHODCALLTYPE publish( ps *p, IUnknown *unk )
{
    if (!drv) IUnknown_QueryInterface( unk, &iid_driver_ps, (void **)&drv );
    if (!core) IUnknown_QueryInterface( unk, &iid_core_ps2, (void **)&core );
    return drv ? S_OK : E_FAIL;
}
static HRESULT STDMETHODCALLTYPE command( ps *p, DEVOBJ *dev, DWORD index, void *data, DWORD size, DWORD *result )
{
    char color[64] = "?", page[64] = "?", features[1024], line[200];
    DWORD needed = 0, count = 0, n = 0, w;
    const char *f;

    if (index != 1 && index != 14 && index != 16 && index != 101 && index != 19 && index != 20) return S_OK;
    if (drv) drv->lpVtbl->DrvGetDriverSetting( drv, dev, "ColorModel", color, sizeof(color), &needed, &count );
    if (core) core->lpVtbl->GetOptions( core, dev, 0, "PageSize\0", 10, page, sizeof(page), &needed );
    if (core && SUCCEEDED(core->lpVtbl->EnumFeatures( core, dev, 0, features, sizeof(features), &needed )))
        for (f = features; *f; f += strlen( f ) + 1) n++;
    /* GetOptions answers "PageSize\0Letter\0\0" */
    snprintf( line, sizeof(line), "%%SG%lu %s %lu %lu %s\n", index, color, n, seen, page + strlen( page ) + 1 );
    if (index == 1) dev->procs->write( dev, line, strlen( line ) );  /* the old table */
    else drv->lpVtbl->DrvWriteSpoolBuf( drv, dev, line, strlen( line ), &w );
    *result = 0;
    return S_OK;
}
/* the whole job, on its way out */
static HRESULT STDMETHODCALLTYPE write_printer( ps *p, DEVOBJ *dev, void *buf, DWORD size, DWORD *written )
{
    if (!buf && !size) { *written = 0; return S_OK; }   /* yes, we do this */
    seen += size;
    return WritePrinter( dev->printer, buf, size, written ) ? S_OK : E_FAIL;
}
static HRESULT STDMETHODCALLTYPE get_adjustment( ps *p, DEVOBJ *dev, DWORD a, void *b, DWORD c, BOOL *d ) { return E_NOTIMPL; }

static const void *vtbl[] =
{
    qi, addref, release, get_info, dev_mode, enable_driver, disable_driver, enable_pdev, disable_pdev, reset_pdev,
    publish, command, write_printer, get_adjustment,
};
static ps the_ps = { vtbl, 1 };

typedef struct { const void *lpVtbl; } factory;
static HRESULT STDMETHODCALLTYPE f_qi( factory *f, REFIID riid, void **out ) { *out = f; return S_OK; }
static ULONG STDMETHODCALLTYPE f_addref( factory *f ) { return 2; }
static ULONG STDMETHODCALLTYPE f_release( factory *f ) { return 1; }
static HRESULT STDMETHODCALLTYPE f_create( factory *f, IUnknown *outer, REFIID riid, void **out ) { return qi( &the_ps, riid, out ); }
static HRESULT STDMETHODCALLTYPE f_lock( factory *f, BOOL lock ) { return S_OK; }
static const void *fvtbl[] = { f_qi, f_addref, f_release, f_create, f_lock };
static factory the_factory = { fvtbl };

HRESULT WINAPI DllGetClassObject( REFCLSID clsid, REFIID riid, void **out )
{
    *out = &the_factory;
    return S_OK;
}
