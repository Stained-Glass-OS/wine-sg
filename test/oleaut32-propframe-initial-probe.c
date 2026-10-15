/* Probe for patches/sg/5123: OleCreatePropertyFrameIndirect with
 * dispidInitialProperty (docs, olectl/nf-olectl-olecreatepropertyframeindirect):
 * MapPropertyToPage on the first object's IPerPropertyBrowsing names the page
 * to show first, that page is activated, then asked through IPropertyPage2 to
 * EditProperty(dispid).  DISPID_UNKNOWN means no initial property.  Table of
 * (dispid, what the map returns, page types) against the exact event log. */
#define COBJMACROS
#include <windows.h>
#include <objbase.h>
#include <ocidl.h>
#include <olectl.h>
#include <prsht.h>
#include <stdio.h>
#include <string.h>

static int failures;
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

static char log_buf[512];
static void logf_(const char *fmt, ...)
{
    va_list ap; size_t n = strlen(log_buf);
    if (n) log_buf[n++] = ' ';
    va_start(ap, fmt); vsnprintf(log_buf + n, sizeof(log_buf) - n, fmt, ap); va_end(ap);
}

static const CLSID clsids[3] = {
    {0x5b0a0001, 0x1111, 0x4222, {0x83, 0x33, 0x44, 0x44, 0x55, 0x55, 0x66, 0x01}},
    {0x5b0a0001, 0x1111, 0x4222, {0x83, 0x33, 0x44, 0x44, 0x55, 0x55, 0x66, 0x02}},
    {0x5b0a0001, 0x1111, 0x4222, {0x83, 0x33, 0x44, 0x44, 0x55, 0x55, 0x66, 0x03}},
};
static BOOL page2_support[3];

/* ---- the object: IUnknown + IPerPropertyBrowsing ---- */
static HRESULT map_result;   /* what MapPropertyToPage returns */
static int map_clsid;        /* index into clsids when S_OK */
static IPerPropertyBrowsing obj_ppb;
static HRESULT WINAPI obj_qi(IPerPropertyBrowsing *i, REFIID r, void **o)
{
    if (IsEqualIID(r, &IID_IUnknown) || IsEqualIID(r, &IID_IPerPropertyBrowsing)) { *o = i; return S_OK; }
    *o = NULL; return E_NOINTERFACE;
}
static ULONG WINAPI obj_addref(IPerPropertyBrowsing *i) { return 2; }
static ULONG WINAPI obj_release(IPerPropertyBrowsing *i) { return 1; }
static HRESULT WINAPI obj_display(IPerPropertyBrowsing *i, DISPID d, BSTR *s) { return E_NOTIMPL; }
static HRESULT WINAPI obj_map(IPerPropertyBrowsing *i, DISPID d, CLSID *c)
{
    logf_("Map(%ld)", (long)d);
    if (map_result == S_OK) *c = clsids[map_clsid];
    return map_result;
}
static HRESULT WINAPI obj_pred(IPerPropertyBrowsing *i, DISPID d, CALPOLESTR *a, CADWORD *b) { return E_NOTIMPL; }
static HRESULT WINAPI obj_pval(IPerPropertyBrowsing *i, DISPID d, DWORD c, VARIANT *v) { return E_NOTIMPL; }
static const IPerPropertyBrowsingVtbl objvtbl = { obj_qi, obj_addref, obj_release, obj_display, obj_map, obj_pred, obj_pval };

/* ---- the pages ---- */
struct page { IPropertyPage2 iface; int idx; LONG ref; };
static struct page pages[3];
static struct page *impl(IPropertyPage2 *i) { return (struct page *)i; }
static HRESULT WINAPI pg_qi(IPropertyPage2 *i, REFIID r, void **o)
{
    if (IsEqualIID(r, &IID_IUnknown) || IsEqualIID(r, &IID_IPropertyPage) ||
        (IsEqualIID(r, &IID_IPropertyPage2) && page2_support[impl(i)->idx])) { *o = i; IPropertyPage2_AddRef(i); return S_OK; }
    *o = NULL; return E_NOINTERFACE;
}
static ULONG WINAPI pg_addref(IPropertyPage2 *i) { return InterlockedIncrement(&impl(i)->ref); }
static ULONG WINAPI pg_release(IPropertyPage2 *i) { return InterlockedDecrement(&impl(i)->ref); }
static HRESULT WINAPI pg_site(IPropertyPage2 *i, IPropertyPageSite *s) { return S_OK; }
static HRESULT WINAPI pg_activate(IPropertyPage2 *i, HWND parent, LPCRECT r, BOOL modal)
{
    logf_("Activate(%c)", 'A' + impl(i)->idx);
    /* close the sheet once it is shown */
    PostMessageW(GetParent(parent), PSM_PRESSBUTTON, PSBTN_CANCEL, 0);
    return S_OK;
}
static HRESULT WINAPI pg_deactivate(IPropertyPage2 *i) { return S_OK; }
static HRESULT WINAPI pg_info(IPropertyPage2 *i, PROPPAGEINFO *p)
{
    memset(p, 0, sizeof(*p)); p->cb = sizeof(*p); p->size.cx = 100; p->size.cy = 60;
    p->pszTitle = CoTaskMemAlloc(8 * sizeof(WCHAR)); lstrcpyW(p->pszTitle, L"page");
    return S_OK;
}
static HRESULT WINAPI pg_objects(IPropertyPage2 *i, ULONG c, IUnknown **u) { return S_OK; }
static HRESULT WINAPI pg_show(IPropertyPage2 *i, UINT s) { return S_OK; }
static HRESULT WINAPI pg_move(IPropertyPage2 *i, LPCRECT r) { return S_OK; }
static HRESULT WINAPI pg_dirty(IPropertyPage2 *i) { return S_FALSE; }
static HRESULT WINAPI pg_apply(IPropertyPage2 *i) { return S_OK; }
static HRESULT WINAPI pg_help(IPropertyPage2 *i, LPCOLESTR d) { return E_NOTIMPL; }
static HRESULT WINAPI pg_accel(IPropertyPage2 *i, MSG *m) { return E_NOTIMPL; }
static HRESULT WINAPI pg_edit(IPropertyPage2 *i, DISPID d) { logf_("Edit(%c,%ld)", 'A' + impl(i)->idx, (long)d); return S_OK; }
static const IPropertyPage2Vtbl pgvtbl = { pg_qi, pg_addref, pg_release, pg_site, pg_activate, pg_deactivate, pg_info,
    pg_objects, pg_show, pg_move, pg_dirty, pg_apply, pg_help, pg_accel, pg_edit };

/* ---- class factory registered in-process ---- */
struct factory { IClassFactory iface; int idx; };
static HRESULT WINAPI cf_qi(IClassFactory *i, REFIID r, void **o)
{
    if (IsEqualIID(r, &IID_IUnknown) || IsEqualIID(r, &IID_IClassFactory)) { *o = i; return S_OK; }
    *o = NULL; return E_NOINTERFACE;
}
static ULONG WINAPI cf_addref(IClassFactory *i) { return 2; }
static ULONG WINAPI cf_release(IClassFactory *i) { return 1; }
static HRESULT WINAPI cf_create(IClassFactory *i, IUnknown *outer, REFIID r, void **o)
{
    struct page *p = &pages[((struct factory *)i)->idx];
    return IPropertyPage2_QueryInterface(&p->iface, r, o);
}
static HRESULT WINAPI cf_lock(IClassFactory *i, BOOL l) { return S_OK; }
static const IClassFactoryVtbl cfvtbl = { cf_qi, cf_addref, cf_release, cf_create, cf_lock };
static struct factory factories[3];

static const struct
{
    const char *name;
    DISPID dispid;
    HRESULT map_hr;
    int map_idx;
    BOOL p2[3];            /* which pages implement IPropertyPage2 */
    const char *expect;
} tests[] = {
    { "DISPID_UNKNOWN: no lookup, first page, no edit", DISPID_UNKNOWN, S_OK, 2, {1,1,1}, "Activate(A)" },
    { "dispid 5 maps to page C: C first, then Edit(C,5)", 5, S_OK, 2, {1,1,1}, "Map(5) Activate(C) Edit(C,5)" },
    { "dispid 5 maps to page B: B first, then Edit(B,5)", 5, S_OK, 1, {1,1,1}, "Map(5) Activate(B) Edit(B,5)" },
    { "dispid 0 (DISPID_VALUE) is a property too", 0, S_OK, 1, {1,1,1}, "Map(0) Activate(B) Edit(B,0)" },
    { "the page has no IPropertyPage2: shown, no edit", 7, S_OK, 2, {1,1,0}, "Map(7) Activate(C)" },
    { "the object cannot map the property: first page, no edit", 5, E_NOTIMPL, 0, {1,1,1}, "Map(5) Activate(A)" },
    { "the mapped page is not in the frame's list: first page, no edit", 5, S_OK, -1, {1,1,1}, "Map(5) Activate(A)" },
};

int main(void)
{
    unsigned i;
    int k;
    DWORD cookies[3];
    HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    check(hr == S_OK, "CoInitializeEx %#lx", hr);

    obj_ppb.lpVtbl = (IPerPropertyBrowsingVtbl *)&objvtbl;
    for (k = 0; k < 3; k++)
    {
        pages[k].iface.lpVtbl = (IPropertyPage2Vtbl *)&pgvtbl; pages[k].idx = k; pages[k].ref = 1;
        factories[k].iface.lpVtbl = (IClassFactoryVtbl *)&cfvtbl; factories[k].idx = k;
        hr = CoRegisterClassObject(&clsids[k], (IUnknown *)&factories[k].iface, CLSCTX_INPROC_SERVER, REGCLS_MULTIPLEUSE, &cookies[k]);
        check(hr == S_OK, "register page class %d: %#lx", k, hr);
    }

    for (i = 0; i < sizeof(tests) / sizeof(tests[0]); i++)
    {
        IUnknown *objs[1] = { (IUnknown *)&obj_ppb };
        CLSID list[3];
        OCPFIPARAMS p = { sizeof(p) };

        for (k = 0; k < 3; k++) { page2_support[k] = tests[i].p2[k]; list[k] = clsids[k]; }
        if (tests[i].map_idx < 0) list[2] = clsids[0]; /* page C's class is not in the list: use two distinct + a duplicate */
        map_result = tests[i].map_hr; map_clsid = tests[i].map_idx < 0 ? 2 : tests[i].map_idx;
        log_buf[0] = 0;
        p.hWndOwner = NULL; p.lpszCaption = L"frame"; p.cObjects = 1; p.lplpUnk = objs;
        p.cPages = tests[i].map_idx < 0 ? 2 : 3; p.lpPages = list; p.lcid = 0x409;
        p.dispidInitialProperty = tests[i].dispid;
        hr = OleCreatePropertyFrameIndirect(&p);
        check(hr == S_OK && !strcmp(log_buf, tests[i].expect), "%s: hr %#lx, log '%s' (expected '%s')",
              tests[i].name, hr, log_buf, tests[i].expect);
    }

    /* OleCreatePropertyFrame has no initial property: it never maps */
    {
        IUnknown *objs[1] = { (IUnknown *)&obj_ppb };
        CLSID list[3] = { clsids[0], clsids[1], clsids[2] };
        for (k = 0; k < 3; k++) page2_support[k] = 1;
        map_result = S_OK; map_clsid = 2; log_buf[0] = 0;
        hr = OleCreatePropertyFrame(NULL, 0, 0, L"frame", 1, objs, 3, list, 0x409, 0, NULL);
        check(hr == S_OK && !strcmp(log_buf, "Activate(A)"), "OleCreatePropertyFrame: hr %#lx, log '%s'", hr, log_buf);
    }

    for (k = 0; k < 3; k++) CoRevokeClassObject(cookies[k]);
    CoUninitialize();
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
