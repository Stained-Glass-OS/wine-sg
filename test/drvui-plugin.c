/* A printer maker's UI plug-in of our own (IPrintOemUI2), for the drvui
 * gate (patches/sg/1027), built as makers build theirs from the documented
 * interfaces: it keeps a "stamp" in its part of the devmode, adds an
 * option ("SG Stamp") to the driver's options (CommonUIProp), a page of its
 * own to the document's sheet ("SG Page") and to the printer's ("SG
 * Device"), and asks the core about the features (IPrintCoreUI2).  What it
 * sees goes to the file $SG_DRVUI_LOG, one fact a line.  Our own code. */
#define COBJMACROS
#include <stdio.h>
#include <string.h>
#include <windows.h>
#include <commctrl.h>
#include <prsht.h>
#include <objbase.h>
#include <compstui.h>

static const GUID iid_oem_ui = { 0xc6a7a9d0, 0x774c, 0x11d1, { 0x94, 0x7f, 0x00, 0xa0, 0xc9, 0x06, 0x40, 0xb8 } };
static const GUID iid_oem_ui2 = { 0x292515f9, 0xb54b, 0x489b, { 0x92, 0x75, 0xba, 0xb5, 0x68, 0x21, 0x39, 0x5e } };
static const GUID iid_core_ui2 = { 0x085ccfca, 0x3adf, 0x4c9e, { 0xb4, 0x91, 0xd8, 0x51, 0xa6, 0xed, 0xc9, 0x97 } };

#define SIG 0x49554753  /* "SGUI" */

typedef struct { DWORD size; DWORD signature; DWORD version; DWORD stamp; } sg_dm;

typedef struct
{
    DWORD cbSize; void *pdriverobj; HANDLE hPrinter; HANDLE hModule;
    DEVMODEW *pPublicDMIn, *pPublicDMOut; void *pOEMDMIn, *pOEMDMOut; DWORD cbBufSize;
} OEMDMPARAM;

typedef struct _OEMCUIPPARAM OEMCUIPPARAM;
struct _OEMCUIPPARAM
{
    DWORD cbSize; void *poemuiobj; HANDLE hPrinter; WCHAR *pPrinterName; HANDLE hModule; HANDLE hOEMHeap;
    DEVMODEW *pPublicDM; void *pOEMDM; DWORD dwFlags; OPTITEM *pDrvOptItems; DWORD cDrvOptItems;
    OPTITEM *pOEMOptItems; DWORD cOEMOptItems; void *pOEMUserData;
    LONG (CALLBACK *OEMCUIPCallback)( CPSUICBPARAM *, OEMCUIPPARAM * );
};

typedef struct core core;
typedef struct
{
    HRESULT (STDMETHODCALLTYPE *QueryInterface)( core *, REFIID, void ** );
    ULONG (STDMETHODCALLTYPE *AddRef)( core * );
    ULONG (STDMETHODCALLTYPE *Release)( core * );
    HRESULT (STDMETHODCALLTYPE *DrvGetDriverSetting)( core *, void *, const char *, void *, DWORD, DWORD *, DWORD * );
    HRESULT (STDMETHODCALLTYPE *DrvUpgradeRegistrySetting)( core *, HANDLE, const char *, const char * );
    HRESULT (STDMETHODCALLTYPE *DrvUpdateUISetting)( core *, void *, void *, DWORD, DWORD );
    HRESULT (STDMETHODCALLTYPE *GetOptions)( core *, void *, DWORD, const char *, DWORD, char *, DWORD, DWORD * );
    HRESULT (STDMETHODCALLTYPE *SetOptions)( core *, void *, DWORD, const char *, DWORD, DWORD * );
    void *EnumConstrainedOptions, *WhyConstrained, *GetGlobalAttribute, *GetFeatureAttribute, *GetOptionAttribute;
    HRESULT (STDMETHODCALLTYPE *EnumFeatures)( core *, void *, DWORD, char *, DWORD, DWORD * );
} core_vtbl;
struct core { const core_vtbl *lpVtbl; };

static core *the_core;
static OPTPARAM stamps[3];
static OPTTYPE stamp_type;
static BYTE page_template[256];

static void say( const char *fmt, ... )
{
    char buf[512], path[MAX_PATH];
    va_list args;
    FILE *f;

    if (!GetEnvironmentVariableA( "SG_DRVUI_LOG", path, sizeof(path) )) return;
    va_start( args, fmt );
    vsnprintf( buf, sizeof(buf), fmt, args );
    va_end( args );
    if ((f = fopen( path, "a" ))) { fputs( buf, f ); fclose( f ); }
}

typedef struct ui ui;
struct ui { const void *lpVtbl; };

static HRESULT STDMETHODCALLTYPE qi( ui *u, REFIID riid, void **out )
{
    if (IsEqualGUID( riid, &IID_IUnknown ) || IsEqualGUID( riid, &iid_oem_ui ) || IsEqualGUID( riid, &iid_oem_ui2 ))
    {
        *out = u;
        return S_OK;
    }
    *out = NULL;
    return E_NOINTERFACE;
}
static ULONG STDMETHODCALLTYPE addref( ui *u ) { return 2; }
static ULONG STDMETHODCALLTYPE release( ui *u ) { return 1; }

static HRESULT STDMETHODCALLTYPE get_info( ui *u, DWORD mode, void *buf, DWORD size, DWORD *needed )
{
    if (needed) *needed = 4;
    if (!buf || size < 4) return E_FAIL;
    if (mode == 1) { *(DWORD *)buf = SIG; return S_OK; }
    if (mode == 2) { *(DWORD *)buf = 0x00010000; return S_OK; }
    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE dev_mode( ui *u, DWORD mode, OEMDMPARAM *p )
{
    sg_dm *out = p->pOEMDMOut;
    const sg_dm *in = p->pOEMDMIn;

    switch (mode)
    {
    case 1: p->cbBufSize = sizeof(sg_dm); return S_OK;
    case 2: out->size = sizeof(*out); out->signature = SIG; out->version = 1; out->stamp = 0; return S_OK;
    case 3:
    case 4:
        if (in && in->signature == SIG && in->size == sizeof(*in)) *out = *in;
        return S_OK;
    }
    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE publish( ui *u, IUnknown *unk )
{
    HRESULT hr = IUnknown_QueryInterface( unk, &iid_core_ui2, (void **)&the_core );
    say( "publish core_ui2 %d\n", SUCCEEDED(hr) );
    return S_OK;
}

static LONG CALLBACK stamp_callback( CPSUICBPARAM *p, OEMCUIPPARAM *cuip )
{
    OPTITEM *mine = cuip->pOEMOptItems;
    char buf[256];
    DWORD needed = 0;

    if (p->Reason == CPSUICB_REASON_SEL_CHANGED && p->pCurItem == mine)
        say( "stamp changed %ld\n", mine->Sel );
    if (p->Reason == CPSUICB_REASON_APPLYNOW)
    {
        if (cuip->pOEMDM) ((sg_dm *)cuip->pOEMDM)->stamp = mine->Sel;
        memset( buf, 0, sizeof(buf) );
        if (the_core && SUCCEEDED(the_core->lpVtbl->GetOptions( the_core, cuip->poemuiobj, 0, "PageSize\0", 10, buf,
                                                              sizeof(buf), &needed )))
            say( "apply stamp %ld PageSize %s\n", mine->Sel, buf + strlen( buf ) + 1 );
        else say( "apply stamp %ld\n", mine->Sel );
    }
    return CPSUICB_ACTION_NONE;
}

static HRESULT STDMETHODCALLTYPE common_ui_prop( ui *u, DWORD mode, OEMCUIPPARAM *p )
{
    char features[2048];
    DWORD needed = 0, n = 0;
    const char *f;

    if (!p->pOEMOptItems)
    {
        /* the driver's options are there to look at already */
        say( "commonuiprop %lu first drv items %d\n", mode, p->pDrvOptItems && p->cDrvOptItems > 0 );
        if (the_core && SUCCEEDED(the_core->lpVtbl->EnumFeatures( the_core, p->poemuiobj, 0, features, sizeof(features),
                                                                &needed )))
            for (f = features; *f; f += strlen( f ) + 1) n++;
        say( "features %lu\n", n );
        p->cOEMOptItems = mode == 1 ? 1 : 0;
        return S_OK;
    }
    stamps[0].cbSize = stamps[1].cbSize = stamps[2].cbSize = sizeof(OPTPARAM);
    stamps[0].pData = (WCHAR *)L"Off";
    stamps[1].pData = (WCHAR *)L"Draft";
    stamps[2].pData = (WCHAR *)L"Final";
    stamp_type.cbSize = sizeof(stamp_type);
    stamp_type.Type = TVOT_COMBOBOX;
    stamp_type.Count = 3;
    stamp_type.pOptParam = stamps;
    memset( p->pOEMOptItems, 0, sizeof(OPTITEM) );
    p->pOEMOptItems->cbSize = sizeof(OPTITEM);
    p->pOEMOptItems->Level = 1;
    p->pOEMOptItems->pName = (WCHAR *)L"SG Stamp";
    p->pOEMOptItems->pOptType = &stamp_type;
    p->pOEMOptItems->Flags = OPTIF_CALLBACK;
    p->pOEMOptItems->DMPubID = DMPUB_USER + 1;
    p->pOEMOptItems->Sel = p->pOEMDM ? ((sg_dm *)p->pOEMDM)->stamp : 0;
    p->cOEMOptItems = 1;
    p->OEMCUIPCallback = stamp_callback;
    say( "commonuiprop second stamp %ld\n", p->pOEMOptItems->Sel );
    return S_OK;
}

static INT_PTR CALLBACK page_proc( HWND hwnd, UINT msg, WPARAM wp, LPARAM lp )
{
    return msg == WM_INITDIALOG;
}

static void make_page( void )
{
    DLGTEMPLATE *t = (DLGTEMPLATE *)page_template;
    WORD *w;
    memset( page_template, 0, sizeof(page_template) );
    t->style = WS_CHILD | DS_CONTROL;
    t->cx = 200;
    t->cy = 100;
    w = (WORD *)(t + 1);
    w[0] = w[1] = w[2] = 0;
}

static HRESULT add_page( PROPSHEETUI_INFO *info, const WCHAR *title )
{
    PROPSHEETPAGEW psp;

    if (info->Reason != PROPSHEETUI_REASON_INIT) return S_OK;
    make_page();
    memset( &psp, 0, sizeof(psp) );
    psp.dwSize = sizeof(psp);
    psp.dwFlags = PSP_DLGINDIRECT | PSP_USETITLE;
    psp.pResource = (DLGTEMPLATE *)page_template;
    psp.pszTitle = title;
    psp.pfnDlgProc = page_proc;
    say( "page %ls %d\n", title,
         info->pfnComPropSheet( info->hComPropSheet, CPSFUNC_ADD_PROPSHEETPAGEW, (LPARAM)&psp, 0 ) != 0 );
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE document_sheets( ui *u, PROPSHEETUI_INFO *info, LPARAM lparam )
{
    return add_page( info, L"SG Page" );
}

static HRESULT STDMETHODCALLTYPE device_sheets( ui *u, PROPSHEETUI_INFO *info, LPARAM lparam )
{
    return add_page( info, L"SG Device" );
}

static HRESULT STDMETHODCALLTYPE notimpl( void ) { return E_NOTIMPL; }

static const void *vtbl[] =
{
    qi, addref, release, get_info, dev_mode, publish, common_ui_prop, document_sheets, device_sheets,
    notimpl, notimpl, notimpl, notimpl, notimpl, notimpl, notimpl, notimpl,   /* ... UpdateExternalFonts */
    notimpl, notimpl /* HideStandardUI: no */, notimpl,
};
static ui the_ui = { vtbl };

typedef struct { const void *lpVtbl; } factory;
static HRESULT STDMETHODCALLTYPE f_qi( factory *f, REFIID riid, void **out ) { *out = f; return S_OK; }
static ULONG STDMETHODCALLTYPE f_addref( factory *f ) { return 2; }
static ULONG STDMETHODCALLTYPE f_release( factory *f ) { return 1; }
static HRESULT STDMETHODCALLTYPE f_create( factory *f, IUnknown *outer, REFIID riid, void **out )
{
    return qi( &the_ui, riid, out );
}
static HRESULT STDMETHODCALLTYPE f_lock( factory *f, BOOL lock ) { return S_OK; }
static const void *fvtbl[] = { f_qi, f_addref, f_release, f_create, f_lock };
static factory the_factory = { fvtbl };

HRESULT WINAPI DllGetClassObject( REFCLSID clsid, REFIID riid, void **out )
{
    *out = &the_factory;
    return S_OK;
}
