/* The DHTML Editing Control's table commands (TriEdit's, wine-sg 9607): DECMD_INSERTTABLE with a
 * DEInsertTableParam, the row, column and cell commands on the selection's cell, and
 * DEGetBlockFmtNamesParam receiving DECMD_GETBLOCKFMTNAMES. They were not supported and the two
 * parameter classes did not exist. */
#define COBJMACROS
#include <windows.h>
#include <ole2.h>
#include <ocidl.h>
#include <mshtml.h>
#include <initguid.h>
#include <dhtmled.h>
#include <dhtmliid.h>
#include <dhtmldid.h>
#include <stdio.h>

static int failures;
#define CHECK(c, ...) do { if (c) printf("ok    " __VA_ARGS__); else { failures++; printf("FAIL  " __VA_ARGS__); } } while (0)

static HWND host_hwnd;
static LONG doc_complete, display_changed, context_shown;

/* the container: client site + in-place site + frame */
typedef struct { IOleClientSite cs; IOleInPlaceSite ips; IOleInPlaceFrame frame; } site_t;
static site_t site;

static HRESULT WINAPI cs_QI(IOleClientSite *i, REFIID riid, void **ppv)
{
    if (IsEqualGUID(riid, &IID_IUnknown) || IsEqualGUID(riid, &IID_IOleClientSite)) *ppv = &site.cs;
    else if (IsEqualGUID(riid, &IID_IOleWindow) || IsEqualGUID(riid, &IID_IOleInPlaceSite)) *ppv = &site.ips;
    else { *ppv = NULL; return E_NOINTERFACE; }
    return S_OK;
}
static ULONG WINAPI one(void *i) { return 1; }
static HRESULT WINAPI cs_notimpl(void) { return E_NOTIMPL; }
static HRESULT WINAPI cs_GetMoniker(IOleClientSite *i, DWORD a, DWORD w, IMoniker **m) { *m = NULL; return E_NOTIMPL; }
static HRESULT WINAPI cs_GetContainer(IOleClientSite *i, IOleContainer **c) { *c = NULL; return E_NOINTERFACE; }
static HRESULT WINAPI cs_ok(void) { return S_OK; }
static HRESULT WINAPI cs_OnShowWindow(IOleClientSite *i, BOOL s) { return S_OK; }
static IOleClientSiteVtbl cs_vtbl = { cs_QI, (void *)one, (void *)one, (void *)cs_notimpl, cs_GetMoniker,
    cs_GetContainer, (void *)cs_ok, cs_OnShowWindow, (void *)cs_notimpl };

static HRESULT WINAPI ips_QI(IOleInPlaceSite *i, REFIID riid, void **ppv) { return cs_QI(&site.cs, riid, ppv); }
static HRESULT WINAPI ips_GetWindow(IOleInPlaceSite *i, HWND *h) { *h = host_hwnd; return S_OK; }
static HRESULT WINAPI ips_CSH(IOleInPlaceSite *i, BOOL b) { return E_NOTIMPL; }
static HRESULT WINAPI ips_GetWindowContext(IOleInPlaceSite *i, IOleInPlaceFrame **f, IOleInPlaceUIWindow **d,
        RECT *pos, RECT *clip, OLEINPLACEFRAMEINFO *info)
{
    *f = &site.frame; *d = NULL;
    SetRect(pos, 10, 10, 610, 410); *clip = *pos;
    info->fMDIApp = FALSE; info->hwndFrame = host_hwnd; info->haccel = NULL; info->cAccelEntries = 0;
    return S_OK;
}
static HRESULT WINAPI ips_Scroll(IOleInPlaceSite *i, SIZE s) { return E_NOTIMPL; }
static HRESULT WINAPI ips_OnUIDeactivate(IOleInPlaceSite *i, BOOL b) { return S_OK; }
static HRESULT WINAPI ips_OnPosRectChange(IOleInPlaceSite *i, const RECT *r) { return S_OK; }
static IOleInPlaceSiteVtbl ips_vtbl = { ips_QI, (void *)one, (void *)one, ips_GetWindow, ips_CSH,
    (void *)cs_ok, (void *)cs_ok, (void *)cs_ok, ips_GetWindowContext, ips_Scroll, ips_OnUIDeactivate,
    (void *)cs_ok, (void *)cs_ok, (void *)cs_ok, ips_OnPosRectChange };

static HRESULT WINAPI fr_QI(IOleInPlaceFrame *i, REFIID riid, void **ppv) { *ppv = i; return S_OK; }
static HRESULT WINAPI fr_GetWindow(IOleInPlaceFrame *i, HWND *h) { *h = host_hwnd; return S_OK; }
static HRESULT WINAPI fr_b(IOleInPlaceFrame *i, BOOL b) { return S_OK; }
static HRESULT WINAPI fr_rect(IOleInPlaceFrame *i, void *r) { return INPLACE_E_NOTOOLSPACE; }
static HRESULT WINAPI fr_SetActiveObject(IOleInPlaceFrame *i, IOleInPlaceActiveObject *o, LPCOLESTR n) { return S_OK; }
static HRESULT WINAPI fr_InsertMenus(IOleInPlaceFrame *i, HMENU m, OLEMENUGROUPWIDTHS *w) { return E_NOTIMPL; }
static HRESULT WINAPI fr_SetMenu(IOleInPlaceFrame *i, HMENU m, HOLEMENU o, HWND h) { return S_OK; }
static HRESULT WINAPI fr_RemoveMenus(IOleInPlaceFrame *i, HMENU m) { return E_NOTIMPL; }
static HRESULT WINAPI fr_SetStatusText(IOleInPlaceFrame *i, LPCOLESTR t) { return S_OK; }
static HRESULT WINAPI fr_TA(IOleInPlaceFrame *i, MSG *m, WORD w) { return S_FALSE; }
static IOleInPlaceFrameVtbl fr_vtbl = { fr_QI, (void *)one, (void *)one, fr_GetWindow, fr_b, (void *)fr_rect,
    (void *)fr_rect, (void *)fr_rect, fr_SetActiveObject, fr_InsertMenus, fr_SetMenu, fr_RemoveMenus,
    fr_SetStatusText, fr_b, fr_TA };

/* the events */
static IDHTMLEdit *edit;
static HRESULT WINAPI ev_QI(IDispatch *i, REFIID riid, void **ppv)
{
    if (IsEqualGUID(riid, &IID_IUnknown) || IsEqualGUID(riid, &IID_IDispatch) || IsEqualGUID(riid, &DIID__DHTMLEditEvents))
    { *ppv = i; return S_OK; }
    *ppv = NULL; return E_NOINTERFACE;
}
static HRESULT WINAPI ev_GTIC(IDispatch *i, UINT *c) { *c = 0; return S_OK; }
static HRESULT WINAPI ev_GTI(IDispatch *i, UINT n, LCID l, ITypeInfo **t) { return E_NOTIMPL; }
static HRESULT WINAPI ev_GIDON(IDispatch *i, REFIID r, LPOLESTR *n, UINT c, LCID l, DISPID *d) { return E_NOTIMPL; }
static HRESULT WINAPI ev_Invoke(IDispatch *i, DISPID id, REFIID r, LCID l, WORD f, DISPPARAMS *dp, VARIANT *res,
        EXCEPINFO *ei, UINT *ae)
{
    if (id == DISPID_DOCUMENTCOMPLETE) doc_complete++;
    else if (id == DISPID_DISPLAYCHANGED) display_changed++;
    else if (id == DISPID_SHOWCONTEXTMENU) context_shown++;
    return S_OK;
}
static IDispatchVtbl ev_vtbl = { ev_QI, (void *)one, (void *)one, ev_GTIC, ev_GTI, ev_GIDON, ev_Invoke };
static IDispatch events = { &ev_vtbl };

static BOOL pump_until(LONG *counter, LONG value, DWORD ms)
{
    DWORD end = GetTickCount() + ms;
    MSG msg;
    while (*counter < value && GetTickCount() < end)
    {
        while (PeekMessageW(&msg, 0, 0, 0, PM_REMOVE)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
        Sleep(10);
    }
    return *counter >= value;
}

static void pump(DWORD ms)
{
    LONG never = 0;
    pump_until(&never, 1, ms);
}

static BSTR doc_html(void)
{
    BSTR html = NULL;
    IDHTMLEdit_get_DocumentHTML(edit, &html);
    return html;
}

static BOOL has_tag(BSTR html, const WCHAR *a, const WCHAR *b)
{
    WCHAR low[4096];
    int i;
    if (!html) return FALSE;
    lstrcpynW(low, html, 4096);
    for (i = 0; low[i]; i++) low[i] = towlower(low[i]);
    return wcsstr(low, a) || (b && wcsstr(low, b));
}

static DHTMLEDITCMDF status(DHTMLEDITCMDID id)
{
    DHTMLEDITCMDF f = 0xdead;
    HRESULT hr = IDHTMLEdit_QueryStatus(edit, id, &f);
    return hr == S_OK ? f : 0xbad;
}

static HRESULT exec(DHTMLEDITCMDID id, VARIANT *in, VARIANT *out)
{
    return IDHTMLEdit_ExecCommand(edit, id, OLECMDEXECOPT_DODEFAULT, in, out);
}


static IHTMLDocument2 *dom;

/* puts the caret in the cell with the id */
static void select_cell(const WCHAR *id)
{
    IHTMLDocument3 *doc3;
    IHTMLElement *elem = NULL;
    IHTMLBodyElement *body = NULL;
    IHTMLElement *b = NULL;
    IHTMLTxtRange *range = NULL;
    BSTR bid = SysAllocString(id);

    IHTMLDocument2_QueryInterface(dom, &IID_IHTMLDocument3, (void **)&doc3);
    IHTMLDocument3_getElementById(doc3, bid, &elem);
    IHTMLDocument2_get_body(dom, &b);
    if (b) IHTMLElement_QueryInterface(b, &IID_IHTMLBodyElement, (void **)&body);
    if (body) IHTMLBodyElement_createTextRange(body, &range);
    if (range && elem)
    {
        IHTMLTxtRange_moveToElementText(range, elem);
        IHTMLTxtRange_collapse(range, VARIANT_TRUE);
        IHTMLTxtRange_select(range);
    }
    if (range) IHTMLTxtRange_Release(range);
    if (body) IHTMLBodyElement_Release(body);
    if (b) IHTMLElement_Release(b);
    if (elem) IHTMLElement_Release(elem);
    IHTMLDocument3_Release(doc3);
    SysFreeString(bid);
}

static LONG count_tag(const WCHAR *tag)
{
    IHTMLDocument3 *doc3;
    IHTMLElementCollection *col = NULL;
    BSTR t = SysAllocString(tag);
    LONG n = -1;

    IHTMLDocument2_QueryInterface(dom, &IID_IHTMLDocument3, (void **)&doc3);
    IHTMLDocument3_getElementsByTagName(doc3, t, &col);
    if (col) { IHTMLElementCollection_get_length(col, &n); IHTMLElementCollection_Release(col); }
    IHTMLDocument3_Release(doc3);
    SysFreeString(t);
    return n;
}

static BOOL set_prop(IDispatch *obj, const WCHAR *name, VARIANT *v)
{
    DISPID put = DISPID_PROPERTYPUT, id;
    DISPPARAMS dp = { v, &put, 1, 1 };
    LPOLESTR n = (LPOLESTR)name;
    return SUCCEEDED(IDispatch_GetIDsOfNames(obj, &IID_NULL, &n, 1, 0, &id)) &&
           SUCCEEDED(IDispatch_Invoke(obj, id, &IID_NULL, 0, DISPATCH_PROPERTYPUT, &dp, NULL, NULL, NULL));
}

int main(void)
{
    IConnectionPointContainer *cpc;
    IConnectionPoint *cp;
    IOleObject *oleobj;
    IPersistStreamInit *psi;
    IDispatch *param = NULL, *names = NULL;
    VARIANT in, v;
    BSTR html;
    DWORD cookie;
    RECT rc;
    HRESULT hr;
    LONG before;

    OleInitialize(NULL);
    host_hwnd = CreateWindowW(L"STATIC", L"dhtmltable host", WS_OVERLAPPEDWINDOW | WS_VISIBLE | WS_CLIPCHILDREN,
                              0, 0, 640, 480, NULL, NULL, NULL, NULL);
    site.cs.lpVtbl = &cs_vtbl; site.ips.lpVtbl = &ips_vtbl; site.frame.lpVtbl = &fr_vtbl;
    hr = CoCreateInstance(&CLSID_DHTMLEdit, NULL, CLSCTX_INPROC_SERVER, &IID_IDHTMLEdit, (void **)&edit);
    if (FAILED(hr)) { printf("no DHTMLEdit %#lx\nRESULT: FAIL\n", hr); return 1; }
    IDHTMLEdit_QueryInterface(edit, &IID_IConnectionPointContainer, (void **)&cpc);
    IConnectionPointContainer_FindConnectionPoint(cpc, &DIID__DHTMLEditEvents, &cp);
    IConnectionPoint_Advise(cp, (IUnknown *)&events, &cookie);
    IDHTMLEdit_QueryInterface(edit, &IID_IOleObject, (void **)&oleobj);
    IOleObject_SetClientSite(oleobj, &site.cs);
    IDHTMLEdit_QueryInterface(edit, &IID_IPersistStreamInit, (void **)&psi);
    IPersistStreamInit_InitNew(psi);
    SetRect(&rc, 10, 10, 610, 410);
    doc_complete = 0;
    IOleObject_DoVerb(oleobj, OLEIVERB_UIACTIVATE, NULL, &site.cs, 0, host_hwnd, &rc);
    pump_until(&doc_complete, 1, 60000);
    doc_complete = 0;
    html = SysAllocString(L"<html><body><p id=\"before\">text</p>"
                          L"<table id=\"t\" border=\"1\"><tr><td id=\"a1\">a1</td><td id=\"b1\">b1</td></tr>"
                          L"<tr><td id=\"a2\">a2</td><td id=\"b2\">b2</td></tr></table></body></html>");
    IDHTMLEdit_put_DocumentHTML(edit, html);
    SysFreeString(html);
    CHECK(pump_until(&doc_complete, 1, 60000), "document loaded\n");
    pump(300);
    IDHTMLEdit_get_DOM(edit, &dom);
    if (!dom) { printf("no DOM\nRESULT: FAIL\n"); return 1; }

    /* the parameter classes */
    hr = CoCreateInstance(&CLSID_DEInsertTableParam, NULL, CLSCTX_INPROC_SERVER, &IID_IDispatch, (void **)&param);
    CHECK(hr == S_OK && param, "DEInsertTableParam created %#lx\n", hr);
    hr = CoCreateInstance(&CLSID_DEGetBlockFmtNamesParam, NULL, CLSCTX_INPROC_SERVER, &IID_IDispatch, (void **)&names);
    CHECK(hr == S_OK && names, "DEGetBlockFmtNamesParam created %#lx\n", hr);

    select_cell(L"before");
    CHECK(status(DECMD_INSERTROW) == DECMDF_DISABLED, "outside a table the row commands are disabled (%#x)\n", status(DECMD_INSERTROW));
    CHECK(status(DECMD_INSERTTABLE) == DECMDF_ENABLED, "INSERTTABLE enabled (%#x)\n", status(DECMD_INSERTTABLE));

    if (param)
    {
        before = count_tag(L"TABLE");
        V_VT(&v) = VT_I4; V_I4(&v) = 3; set_prop(param, L"NumRows", &v);
        V_VT(&v) = VT_I4; V_I4(&v) = 4; set_prop(param, L"NumCols", &v);
        V_VT(&v) = VT_BSTR; V_BSTR(&v) = SysAllocString(L"id=\"made\" border=\"2\""); set_prop(param, L"TableAttrs", &v); VariantClear(&v);
        V_VT(&v) = VT_BSTR; V_BSTR(&v) = SysAllocString(L"Totals"); set_prop(param, L"Caption", &v); VariantClear(&v);
        V_VT(&in) = VT_DISPATCH; V_DISPATCH(&in) = param;
        hr = exec(DECMD_INSERTTABLE, &in, NULL);
        CHECK(hr == S_OK, "ExecCommand(INSERTTABLE) %#lx\n", hr);
        CHECK(count_tag(L"TABLE") == before + 1, "a table was inserted (%ld tables)\n", count_tag(L"TABLE"));
        html = doc_html();
        CHECK(has_tag(html, L"id=\"made\"", NULL) && has_tag(html, L"<caption>totals</caption>", NULL), "with its attributes and caption\n");
        SysFreeString(html);
        CHECK(count_tag(L"TD") == 4 + 12, "3 x 4 cells (%ld cells)\n", count_tag(L"TD"));
    }

    select_cell(L"b1");
    CHECK(status(DECMD_INSERTROW) == DECMDF_ENABLED, "in a cell the row commands are enabled (%#x)\n", status(DECMD_INSERTROW));
    before = count_tag(L"TR");
    hr = exec(DECMD_INSERTROW, NULL, NULL);
    CHECK(hr == S_OK && count_tag(L"TR") == before + 1, "INSERTROW %#lx (%ld rows)\n", hr, count_tag(L"TR"));
    select_cell(L"b1");
    before = count_tag(L"TD");
    hr = exec(DECMD_INSERTCOL, NULL, NULL);
    CHECK(hr == S_OK && count_tag(L"TD") == before + 3, "INSERTCOL adds a cell to each of the 3 rows %#lx (%ld)\n", hr, count_tag(L"TD") - before);
    select_cell(L"a2");
    before = count_tag(L"TD");
    hr = exec(DECMD_DELETECELLS, NULL, NULL);
    CHECK(hr == S_OK && count_tag(L"TD") == before - 1, "DELETECELLS %#lx\n", hr);
    select_cell(L"b2");
    before = count_tag(L"TR");
    hr = exec(DECMD_DELETEROWS, NULL, NULL);
    CHECK(hr == S_OK && count_tag(L"TR") == before - 1, "DELETEROWS %#lx\n", hr);
    select_cell(L"a1");
    before = count_tag(L"TD");
    hr = exec(DECMD_SPLITCELL, NULL, NULL);
    CHECK(hr == S_OK && count_tag(L"TD") == before + 1, "SPLITCELL %#lx\n", hr);
    CHECK(status(DECMD_MERGECELLS) == DECMDF_DISABLED, "MERGECELLS needs more than one cell selected (%#x)\n", status(DECMD_MERGECELLS));

    if (names)
    {
        VARIANT got;
        DISPPARAMS dp = {0};
        LPOLESTR n = (LPOLESTR)L"Names";
        DISPID id;

        V_VT(&in) = VT_DISPATCH; V_DISPATCH(&in) = names;
        hr = exec(DECMD_GETBLOCKFMTNAMES, &in, NULL);
        CHECK(hr == S_OK, "ExecCommand(GETBLOCKFMTNAMES, names param) %#lx\n", hr);
        VariantInit(&got);
        IDispatch_GetIDsOfNames(names, &IID_NULL, &n, 1, 0, &id);
        hr = IDispatch_Invoke(names, id, &IID_NULL, 0, DISPATCH_PROPERTYGET, &dp, &got, NULL, NULL);
        CHECK(hr == S_OK && (V_VT(&got) & VT_ARRAY), "Names is an array (vt %#x)\n", V_VT(&got));
        if (V_VT(&got) == (VT_ARRAY | VT_BSTR))
        {
            LONG idx = 3;
            BSTR name = NULL;
            SafeArrayGetElement(V_ARRAY(&got), &idx, &name);
            CHECK(name && !lstrcmpW(name, L"Heading 1"), "the fourth name is Heading 1 (%ls)\n", name ? name : L"null");
            SysFreeString(name);
        }
        VariantClear(&got);

        /* a block format set by its name reads back by its name */
        select_cell(L"before");
        V_VT(&in) = VT_BSTR; V_BSTR(&in) = SysAllocString(L"Heading 2");
        hr = exec(DECMD_SETBLOCKFMT, &in, NULL);
        VariantClear(&in);
        VariantInit(&got);
        exec(DECMD_GETBLOCKFMT, NULL, &got);
        CHECK(hr == S_OK && V_VT(&got) == VT_BSTR && V_BSTR(&got) && !lstrcmpW(V_BSTR(&got), L"Heading 2"),
              "SETBLOCKFMT(Heading 2) reads back as %ls\n", V_VT(&got) == VT_BSTR && V_BSTR(&got) ? V_BSTR(&got) : L"?");
        VariantClear(&got);
    }

    if (param) IDispatch_Release(param);
    if (names) IDispatch_Release(names);
    IHTMLDocument2_Release(dom);
    IConnectionPoint_Unadvise(cp, cookie);
    IOleObject_Close(oleobj, OLECLOSE_NOSAVE);
    IOleObject_SetClientSite(oleobj, NULL);
    IOleObject_Release(oleobj);
    IPersistStreamInit_Release(psi);
    IDHTMLEdit_Release(edit);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    fflush(stdout);
    ExitProcess(failures != 0);
}
