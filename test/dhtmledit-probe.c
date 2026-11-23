/* The DHTML Editing Control (dhtmled.ocx) edits a real MSHTML document, the way it does on
 * Windows (wine-sg 9601): sited in a window, it loads the document into MSHTML in design
 * mode, DocumentComplete fires when MSHTML has it, the DECMD_ commands (ExecCommand) and
 * their state (QueryStatus) are MSHTML's editing commands, DOM is the live document,
 * DocumentHTML and IsDirty follow the edits, SetContextMenu takes the program's menu.
 * They were E_NOTIMPL and the control kept only the source text. */
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

int main(void)
{
    IConnectionPointContainer *cpc;
    IConnectionPoint *cp;
    IOleObject *oleobj;
    IOleInPlaceObject *ipobj;
    IPersistStreamInit *psi;
    IHTMLDocument2 *dom = NULL;
    IHTMLElement *body = NULL;
    VARIANT in, out, strs, states;
    VARIANT_BOOL dirty;
    SAFEARRAY *sa;
    BSTR html;
    DWORD cookie;
    HWND hwnd;
    RECT rc;
    HRESULT hr;
    LONG i;

    OleInitialize(NULL);
    host_hwnd = CreateWindowW(L"STATIC", L"dhtmledit host", WS_OVERLAPPEDWINDOW | WS_VISIBLE | WS_CLIPCHILDREN,
                              0, 0, 640, 480, NULL, NULL, NULL, NULL);
    site.cs.lpVtbl = &cs_vtbl; site.ips.lpVtbl = &ips_vtbl; site.frame.lpVtbl = &fr_vtbl;

    hr = CoCreateInstance(&CLSID_DHTMLEdit, NULL, CLSCTX_INPROC_SERVER, &IID_IDHTMLEdit, (void **)&edit);
    if (FAILED(hr)) { printf("no DHTMLEdit %#lx\nRESULT: FAIL\n", hr); return 1; }
    IDHTMLEdit_QueryInterface(edit, &IID_IConnectionPointContainer, (void **)&cpc);
    hr = IConnectionPointContainer_FindConnectionPoint(cpc, &DIID__DHTMLEditEvents, &cp);
    if (SUCCEEDED(hr)) hr = IConnectionPoint_Advise(cp, (IUnknown *)&events, &cookie);
    CHECK(hr == S_OK, "event sink connected %#lx\n", hr);

    IDHTMLEdit_QueryInterface(edit, &IID_IOleObject, (void **)&oleobj);
    IOleObject_SetClientSite(oleobj, &site.cs);
    IDHTMLEdit_QueryInterface(edit, &IID_IPersistStreamInit, (void **)&psi);
    IPersistStreamInit_InitNew(psi);

    SetRect(&rc, 10, 10, 610, 410);
    doc_complete = 0;
    hr = IOleObject_DoVerb(oleobj, OLEIVERB_UIACTIVATE, NULL, &site.cs, 0, host_hwnd, &rc);
    CHECK(hr == S_OK, "DoVerb(UIACTIVATE) %#lx\n", hr);
    CHECK(pump_until(&doc_complete, 1, 60000), "DocumentComplete for the new document once the control is in place\n");
    doc_complete = 0;
    html = SysAllocString(L"<html><body><p id=\"para\">hello world</p></body></html>");
    hr = IDHTMLEdit_put_DocumentHTML(edit, html);
    SysFreeString(html);
    CHECK(hr == S_OK, "put_DocumentHTML %#lx\n", hr);
    CHECK(pump_until(&doc_complete, 1, 60000), "DocumentComplete fired once MSHTML had the document\n");
    pump(500);

    IDHTMLEdit_QueryInterface(edit, &IID_IOleInPlaceObject, (void **)&ipobj);
    hwnd = NULL;
    hr = IOleInPlaceObject_GetWindow(ipobj, &hwnd);
    CHECK(hr == S_OK && hwnd && IsChild(host_hwnd, hwnd), "in place, the editing surface is a window in the container (%p)\n", hwnd);

    /* DOM is the live document */
    hr = IDHTMLEdit_get_DOM(edit, &dom);
    CHECK(hr == S_OK && dom, "DOM %#lx\n", hr);
    if (dom)
    {
        BSTR mode = NULL, text = NULL;
        IHTMLDocument2_get_designMode(dom, &mode);
        CHECK(mode && !lstrcmpiW(mode, L"On"), "the document is in design mode (%ls)\n", mode ? mode : L"null");
        IHTMLDocument2_get_body(dom, &body);
        if (body) IHTMLElement_get_innerText(body, &text);
        CHECK(text && wcsstr(text, L"hello world"), "DOM body text %ls\n", text ? text : L"null");
        SysFreeString(mode); SysFreeString(text);
    }

    CHECK(status(DECMD_BOLD) == DECMDF_ENABLED, "QueryStatus(BOLD) before: %#x\n", status(DECMD_BOLD));
    {
        IStream *stm = NULL;
        CreateStreamOnHGlobal(NULL, TRUE, &stm);
        hr = IPersistStreamInit_Save(psi, stm, TRUE);
        IStream_Release(stm);
        dirty = VARIANT_TRUE;
        IDHTMLEdit_get_IsDirty(edit, &dirty);
        CHECK(hr == S_OK && dirty == VARIANT_FALSE, "saved (clearing dirty), the document is not dirty %#lx\n", hr);
    }

    hr = exec(DECMD_SELECTALL, NULL, NULL);
    CHECK(hr == S_OK, "ExecCommand(SELECTALL) %#lx\n", hr);
    display_changed = 0;
    hr = exec(DECMD_BOLD, NULL, NULL);
    CHECK(hr == S_OK, "ExecCommand(BOLD) %#lx\n", hr);
    CHECK(display_changed > 0, "DisplayChanged after a command\n");
    CHECK(status(DECMD_BOLD) == DECMDF_LATCHED, "QueryStatus(BOLD) after: %#x (LATCHED)\n", status(DECMD_BOLD));
    html = doc_html();
    printf("DocumentHTML: %ls\n", html ? html : L"(null)");
    CHECK(has_tag(html, L"<strong>", L"<b>"), "DocumentHTML has the bold text\n");
    SysFreeString(html);
    IDHTMLEdit_get_IsDirty(edit, &dirty);
    CHECK(dirty == VARIANT_TRUE, "an edited document is dirty\n");
    CHECK(status(DECMD_UNDO) == DECMDF_ENABLED, "QueryStatus(UNDO): %#x\n", status(DECMD_UNDO));

    V_VT(&in) = VT_BSTR; V_BSTR(&in) = SysAllocString(L"Courier New");
    hr = exec(DECMD_SETFONTNAME, &in, NULL);
    VariantClear(&in);
    CHECK(hr == S_OK, "ExecCommand(SETFONTNAME) %#lx\n", hr);
    VariantInit(&out);
    hr = exec(DECMD_GETFONTNAME, NULL, &out);
    CHECK(hr == S_OK && V_VT(&out) == VT_BSTR && !lstrcmpiW(V_BSTR(&out), L"Courier New"),
          "ExecCommand(GETFONTNAME) %#lx %ls\n", hr, V_VT(&out) == VT_BSTR ? V_BSTR(&out) : L"?");
    VariantClear(&out);
    html = doc_html();
    CHECK(has_tag(html, L"courier new", NULL), "DocumentHTML has the font\n");
    SysFreeString(html);

    hr = exec(DECMD_UNDO, NULL, NULL);
    CHECK(hr == S_OK, "ExecCommand(UNDO) %#lx\n", hr);
    html = doc_html();
    CHECK(!has_tag(html, L"courier new", NULL), "UNDO took the font away\n");
    SysFreeString(html);

    CHECK(status(DECMD_GETBLOCKFMTNAMES + 1000) == DECMDF_NOTSUPPORTED, "unknown command NOTSUPPORTED\n");

    /* the program's context menu: strings and states go together */
    sa = SafeArrayCreateVector(VT_VARIANT, 0, 3);
    for (i = 0; i < 3; i++)
    {
        VARIANT v; V_VT(&v) = VT_BSTR; V_BSTR(&v) = SysAllocString(i == 0 ? L"Cut" : i == 1 ? L"" : L"Paste");
        SafeArrayPutElement(sa, &i, &v); VariantClear(&v);
    }
    V_VT(&strs) = VT_ARRAY | VT_VARIANT; V_ARRAY(&strs) = sa;
    sa = SafeArrayCreateVector(VT_VARIANT, 0, 3);
    for (i = 0; i < 3; i++)
    {
        VARIANT v; V_VT(&v) = VT_I4; V_I4(&v) = i == 2 ? DECMDF_DISABLED : DECMDF_ENABLED;
        SafeArrayPutElement(sa, &i, &v);
    }
    V_VT(&states) = VT_ARRAY | VT_VARIANT; V_ARRAY(&states) = sa;
    hr = IDHTMLEdit_SetContextMenu(edit, &strs, &states);
    CHECK(hr == S_OK, "SetContextMenu %#lx\n", hr);
    SafeArrayRedim(V_ARRAY(&states), &(SAFEARRAYBOUND){2, 0});
    hr = IDHTMLEdit_SetContextMenu(edit, &strs, &states);
    CHECK(hr == E_INVALIDARG, "SetContextMenu with lists of different lengths %#lx\n", hr);
    VariantClear(&strs); VariantClear(&states);

    /* a new document replaces the edited one */
    doc_complete = 0;
    html = SysAllocString(L"<p>second</p>");
    IDHTMLEdit_put_DocumentHTML(edit, html);
    SysFreeString(html);
    CHECK(pump_until(&doc_complete, 1, 30000), "DocumentComplete for the new document\n");
    html = doc_html();
    CHECK(has_tag(html, L"second", NULL) && !has_tag(html, L"hello", NULL), "DocumentHTML is the new document\n");
    SysFreeString(html);

    hr = IOleInPlaceObject_InPlaceDeactivate(ipobj);
    CHECK(hr == S_OK, "InPlaceDeactivate %#lx\n", hr);
    hwnd = (HWND)1;
    IOleInPlaceObject_GetWindow(ipobj, &hwnd);
    CHECK(hwnd == NULL, "no window once deactivated\n");

    if (body) IHTMLElement_Release(body);
    if (dom) IHTMLDocument2_Release(dom);
    IOleInPlaceObject_Release(ipobj);
    IConnectionPoint_Unadvise(cp, cookie);
    IOleObject_Close(oleobj, OLECLOSE_NOSAVE);
    IOleObject_SetClientSite(oleobj, NULL);
    IOleObject_Release(oleobj);
    IPersistStreamInit_Release(psi);
    IDHTMLEdit_Release(edit);
    /* DHTMLSafe, the control for pages: the same editor without the file and print members */
    {
        IDispatch *safe = NULL;
        IUnknown *unk = NULL;
        LPOLESTR name;
        DISPID id;

        hr = CoCreateInstance(&CLSID_DHTMLSafe, NULL, CLSCTX_INPROC_SERVER, &IID_IDispatch, (void **)&safe);
        CHECK(hr == S_OK && safe, "DHTMLSafe created %#lx\n", hr);
        if (safe)
        {
            hr = IDispatch_QueryInterface(safe, &IID_IDHTMLEdit, (void **)&unk);
            CHECK(hr == E_NOINTERFACE, "DHTMLSafe has no IDHTMLEdit %#lx\n", hr);
            if (unk) IUnknown_Release(unk);
            name = (LPOLESTR)L"DocumentHTML";
            hr = IDispatch_GetIDsOfNames(safe, &IID_NULL, &name, 1, 0, &id);
            CHECK(hr == S_OK && id == DISPID_DOCUMENTHTML, "DHTMLSafe DocumentHTML %#lx\n", hr);
            name = (LPOLESTR)L"SaveDocument";
            hr = IDispatch_GetIDsOfNames(safe, &IID_NULL, &name, 1, 0, &id);
            CHECK(hr == DISP_E_UNKNOWNNAME, "DHTMLSafe has no SaveDocument %#lx\n", hr);
            IDispatch_Release(safe);
        }
    }

    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    fflush(stdout);
    ExitProcess(failures != 0);
}
