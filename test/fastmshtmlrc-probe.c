/* mshtml ngt-recheck round (patch 3282): IDispatchEx::GetMemberProperties, pointer capture, createRangeCollection,
 * XMLHttpRequest.send bodies, IMarkupServices::ParseString(PARSE_ABSOLUTIFYIE40URLS), navigator.userProfile,
 * ITravelLogClient::LoadHistoryPosition, script onerror. */
#define COBJMACROS
#include <windows.h>
#include <ole2.h>
#include <mshtml.h>
#include <dispex.h>
#include <shlwapi.h>
#include <urlmon.h>
#include <stdio.h>
#include <stdlib.h>
#define INTERNET_MAX_URL_LENGTH 2084
#define ARRAY_SIZE(a) (sizeof(a)/sizeof((a)[0]))

static const CLSID CLSID_HTMLDocument_ = {0x25336920,0x03f9,0x11cf,{0x8f,0xd0,0x00,0xaa,0x00,0x68,0x6f,0x13}};
static const CLSID CLSID_DOMDocument_ = {0x2933bf90,0x7b36,0x11d2,{0xb2,0x0e,0x00,0xc0,0x4f,0x98,0x3e,0x60}};
static const IID IID_IMarkupServices_ = {0x3050f4a0,0x98b5,0x11cf,{0xbb,0x82,0x00,0xaa,0x00,0xbd,0xce,0x0b}};
static const IID IID_IHTMLSelectionObject2_ = {0x3050f7ec,0x98b5,0x11cf,{0xbb,0x82,0x00,0xaa,0x00,0xbd,0xce,0x0b}};
#define IID_IHTMLSelectionObject2 IID_IHTMLSelectionObject2_
static const IID IID_ITravelLogClient_ = {0x241c033e,0xe659,0x43da,{0xaa,0x4d,0x40,0x86,0xdb,0xc4,0x75,0x8d}};
static const IID IID_IDispatchEx_ = {0xa6ef9860,0xc720,0x11d0,{0x93,0x37,0x00,0xa0,0xc9,0x0d,0xca,0xa9}};
/* the mingw headers lack these; vtables written from the documented method order */
typedef struct ITLC ITLC;
typedef struct ITLCVtbl {
    HRESULT (WINAPI *QueryInterface)(ITLC*, REFIID, void**);
    ULONG (WINAPI *AddRef)(ITLC*);
    ULONG (WINAPI *Release)(ITLC*);
    HRESULT (WINAPI *FindWindowByIndex)(ITLC*, DWORD, IUnknown**);
    HRESULT (WINAPI *GetWindowData)(ITLC*, IStream*, void*);
    HRESULT (WINAPI *LoadHistoryPosition)(ITLC*, LPWSTR, DWORD);
} ITLCVtbl;
struct ITLC { const ITLCVtbl *lpVtbl; };
typedef struct IMS IMS;
typedef struct IMSVtbl {
    HRESULT (WINAPI *QueryInterface)(IMS*, REFIID, void**);
    ULONG (WINAPI *AddRef)(IMS*);
    ULONG (WINAPI *Release)(IMS*);
    HRESULT (WINAPI *CreateMarkupPointer)(IMS*);
    HRESULT (WINAPI *CreateMarkupContainer)(IMS*);
    HRESULT (WINAPI *CreateElement)(IMS*);
    HRESULT (WINAPI *CloneElement)(IMS*);
    HRESULT (WINAPI *InsertElement)(IMS*);
    HRESULT (WINAPI *RemoveElement)(IMS*);
    HRESULT (WINAPI *Remove)(IMS*);
    HRESULT (WINAPI *Copy)(IMS*);
    HRESULT (WINAPI *Move)(IMS*);
    HRESULT (WINAPI *InsertText)(IMS*);
    HRESULT (WINAPI *ParseString)(IMS*, OLECHAR*, DWORD, IUnknown**, void*, void*);
} IMSVtbl;
struct IMS { const IMSVtbl *lpVtbl; };

static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static void pump(DWORD ms)
{
    DWORD end = GetTickCount() + ms; MSG msg;
    while (GetTickCount() < end) { while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) { TranslateMessage(&msg); DispatchMessageW(&msg); } Sleep(10); }
}
static void write_file(const WCHAR *name, const char *text)
{
    DWORD w;
    HANDLE f = CreateFileW(name, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    WriteFile(f, text, strlen(text), &w, NULL);
    CloseHandle(f);
}
static HRESULT dcall(IDispatch *d, const WCHAR *name, WORD flags, VARIANT *args, int n, VARIANT *ret)
{
    DISPID id, put = DISPID_PROPERTYPUT;
    DISPPARAMS dp = { args, NULL, n, 0 };
    OLECHAR *nm = (OLECHAR *)name;
    HRESULT hr = IDispatch_GetIDsOfNames(d, &IID_NULL, &nm, 1, LOCALE_USER_DEFAULT, &id);
    if (FAILED(hr)) return hr;
    if (flags & DISPATCH_PROPERTYPUT) { dp.rgdispidNamedArgs = &put; dp.cNamedArgs = 1; }
    if (ret) V_VT(ret) = VT_EMPTY;
    return IDispatch_Invoke(d, id, &IID_NULL, LOCALE_USER_DEFAULT, flags, &dp, ret, NULL, NULL);
}
static HRESULT pget(IDispatch *d, const WCHAR *name, VARIANT *ret) { return dcall(d, name, DISPATCH_PROPERTYGET, NULL, 0, ret); }
static VARIANT vbstr(const WCHAR *s) { VARIANT v; V_VT(&v) = VT_BSTR; V_BSTR(&v) = SysAllocString(s); return v; }
static VARIANT vi4(int i) { VARIANT v; V_VT(&v) = VT_I4; V_I4(&v) = i; return v; }
static char sbuf[8][1024]; static int sidx;
static const char *vs(VARIANT *v)
{
    VARIANT s; char *b = sbuf[sidx++ & 7];
    V_VT(&s) = VT_EMPTY;
    if (V_VT(v) == VT_EMPTY) return "(empty)";
    if (FAILED(VariantChangeType(&s, v, 0, VT_BSTR)) || !V_BSTR(&s)) return "(?)";
    WideCharToMultiByte(CP_UTF8, 0, V_BSTR(&s), -1, b, 1023, NULL, NULL);
    VariantClear(&s);
    return b;
}
static const char *pstr(IDispatch *d, const WCHAR *name)
{
    VARIANT v; HRESULT hr = pget(d, name, &v); const char *s;
    if (FAILED(hr)) { char *b = sbuf[sidx++ & 7]; sprintf(b, "ERR%08lx", hr); return b; }
    s = vs(&v); VariantClear(&v); return s;
}
static IDispatch *by_id(IHTMLDocument3 *doc3, const WCHAR *id)
{
    IHTMLElement *e = NULL; IDispatch *d = NULL; BSTR b = SysAllocString(id);
    IHTMLDocument3_getElementById(doc3, b, &e);
    SysFreeString(b);
    if (e) { IHTMLElement_QueryInterface(e, &IID_IDispatch, (void **)&d); IHTMLElement_Release(e); }
    return d;
}
static void run_script(IHTMLDocument2 *doc, const WCHAR *code)
{
    IHTMLWindow2 *win = NULL; VARIANT r; BSTR c = SysAllocString(code), l = SysAllocString(L"JScript");
    IHTMLDocument2_get_parentWindow(doc, &win);
    if (win) { HRESULT hr = IHTMLWindow2_execScript(win, c, l, &r); CHECK(hr == S_OK, "execScript %08lx for %ls", hr, code); IHTMLWindow2_Release(win); }
    SysFreeString(c); SysFreeString(l);
}
static IDispatch *window_disp(IHTMLDocument2 *doc)
{
    IHTMLWindow2 *win = NULL; IDispatch *d = NULL;
    IHTMLDocument2_get_parentWindow(doc, &win);
    if (win) { IHTMLWindow2_QueryInterface(win, &IID_IDispatch, (void **)&d); IHTMLWindow2_Release(win); }
    return d;
}

static void load(const WCHAR *file, IHTMLDocument2 **pdoc, IHTMLDocument3 **pdoc3, ULONG waitms)
{
    IPersistMoniker *pm; WCHAR path[MAX_PATH], url[INTERNET_MAX_URL_LENGTH]; DWORD size = INTERNET_MAX_URL_LENGTH; IMoniker *mk = NULL; HRESULT hr;
    GetFullPathNameW(file, MAX_PATH, path, NULL);
    UrlCreateFromPathW(path, url, &size, 0);
    CoCreateInstance(&CLSID_HTMLDocument_, NULL, CLSCTX_INPROC_SERVER, &IID_IHTMLDocument2, (void **)pdoc);
    IHTMLDocument2_QueryInterface(*pdoc, &IID_IHTMLDocument3, (void **)pdoc3);
    IHTMLDocument2_QueryInterface(*pdoc, &IID_IPersistMoniker, (void **)&pm);
    CreateURLMonikerEx(NULL, url, &mk, 0);
    hr = IPersistMoniker_Load(pm, FALSE, mk, NULL, STGM_READ);
    CHECK(hr == S_OK, "load %ls %08lx", file, hr);
    IMoniker_Release(mk); IPersistMoniker_Release(pm);
    pump(waitms);
}

static const char page[] =
    "<html><head><meta http-equiv=\"x-ua-compatible\" content=\"IE=edge\"><title>rc</title></head><body>"
    "<div id=d style=\"width:10px\">div</div><a name=anc></a><p id=pp>some text to select</p>"
    "<div style=\"height:3000px\"></div></body></html>";

static void test_member_properties(IHTMLDocument2 *doc, IHTMLDocument3 *doc3)
{
    IDispatch *e = by_id(doc3, L"d"); IDispatchEx *dex = NULL; HRESULT hr; DISPID id; DWORD flags;
    BSTR n;
    hr = IDispatch_QueryInterface(e, &IID_IDispatchEx_, (void **)&dex);
    CHECK(hr == S_OK && dex, "IDispatchEx %08lx", hr);
    if (!dex) return;
    /* a read/write property */
    n = SysAllocString(L"title"); hr = IDispatchEx_GetDispID(dex, n, fdexNameCaseSensitive, &id); SysFreeString(n);
    CHECK(hr == S_OK, "dispid title %08lx", hr);
    flags = 0xdeadbeef;
    hr = IDispatchEx_GetMemberProperties(dex, id, grfdexPropAll, &flags);
    CHECK(hr == S_OK, "props title %08lx", hr);
    CHECK((flags & fdexPropCanGet) && (flags & fdexPropCanPut) && (flags & fdexPropCannotCall) && !(flags & fdexPropCannotGet),
          "title flags %lx", flags);
    /* fetch selects: only the asked-for pair is answered */
    flags = 0xdeadbeef; hr = IDispatchEx_GetMemberProperties(dex, id, fdexPropCanGet, &flags);
    CHECK(hr == S_OK && flags == fdexPropCanGet, "title fetch CanGet %08lx %lx", hr, flags);
    flags = 0xdeadbeef; hr = IDispatchEx_GetMemberProperties(dex, id, 0, &flags);
    CHECK(hr == S_OK && flags == 0, "title fetch 0 %08lx %lx", hr, flags);
    /* a read-only property */
    n = SysAllocString(L"offsetWidth"); hr = IDispatchEx_GetDispID(dex, n, fdexNameCaseSensitive, &id); SysFreeString(n);
    CHECK(hr == S_OK, "dispid offsetWidth %08lx", hr);
    flags = 0; hr = IDispatchEx_GetMemberProperties(dex, id, grfdexPropAll, &flags);
    CHECK(hr == S_OK && (flags & fdexPropCanGet) && (flags & fdexPropCannotPut), "offsetWidth flags %08lx %lx", hr, flags);
    /* a method */
    n = SysAllocString(L"appendChild"); hr = IDispatchEx_GetDispID(dex, n, fdexNameCaseSensitive, &id); SysFreeString(n);
    CHECK(hr == S_OK, "dispid appendChild %08lx", hr);
    flags = 0; hr = IDispatchEx_GetMemberProperties(dex, id, grfdexPropAll, &flags);
    CHECK(hr == S_OK && (flags & fdexPropCanCall) && !(flags & fdexPropCannotCall), "appendChild flags %08lx %lx", hr, flags);
    /* an expando */
    n = SysAllocString(L"myExpando"); hr = IDispatchEx_GetDispID(dex, n, fdexNameEnsure | fdexNameCaseSensitive, &id); SysFreeString(n);
    CHECK(hr == S_OK, "dispid expando %08lx", hr);
    flags = 0; hr = IDispatchEx_GetMemberProperties(dex, id, grfdexPropAll, &flags);
    CHECK(hr == S_OK && (flags & fdexPropCanGet) && (flags & fdexPropCanPut) && (flags & fdexPropCannotCall), "expando flags %08lx %lx", hr, flags);
    /* a deleted expando and an unknown id */
    n = SysAllocString(L"myExpando"); IDispatchEx_DeleteMemberByName(dex, n, fdexNameCaseSensitive); SysFreeString(n);
    hr = IDispatchEx_GetMemberProperties(dex, id, grfdexPropCanAll, &flags);
    CHECK(hr == DISP_E_MEMBERNOTFOUND, "deleted expando %08lx", hr);
    hr = IDispatchEx_GetMemberProperties(dex, 0x4fffffff, grfdexPropCanAll, &flags);
    CHECK(hr == DISP_E_MEMBERNOTFOUND, "unknown id %08lx", hr);
    n = SysAllocString(L"title"); IDispatchEx_GetDispID(dex, n, fdexNameCaseSensitive, &id); SysFreeString(n);
    hr = IDispatchEx_GetMemberProperties(dex, id, 0x40000000, &flags);
    CHECK(hr == E_INVALIDARG, "bad fetch bits %08lx", hr);
    IDispatchEx_Release(dex); IDispatch_Release(e);
}

static void test_pointer_capture(IHTMLDocument2 *doc, IHTMLDocument3 *doc3)
{
    IDispatch *e = by_id(doc3, L"d"), *w = window_disp(doc); VARIANT a, r; HRESULT hr;
    run_script(doc, L"window.got = 0; window.lost = 0; var dd = document.getElementById('d');"
                    L"dd.onmsgotpointercapture = function() { window.got++; };"
                    L"dd.onmslostpointercapture = function() { window.lost++; };");
    a = vi4(7);
    hr = dcall(e, L"msSetPointerCapture", DISPATCH_METHOD, &a, 1, NULL);
    CHECK(hr == S_OK, "msSetPointerCapture %08lx", hr);
    pump(100);
    CHECK(!strcmp(pstr(w, L"got"), "1") && !strcmp(pstr(w, L"lost"), "0"), "after set: got %s lost %s", pstr(w, L"got"), pstr(w, L"lost"));
    hr = dcall(e, L"msSetPointerCapture", DISPATCH_METHOD, &a, 1, NULL); pump(50);
    CHECK(hr == S_OK && !strcmp(pstr(w, L"got"), "1"), "setting the same pointer again is silent: got %s", pstr(w, L"got"));
    a = vi4(8);
    hr = dcall(e, L"msReleasePointerCapture", DISPATCH_METHOD, &a, 1, NULL); pump(50);
    CHECK(hr == S_OK && !strcmp(pstr(w, L"lost"), "0"), "release of a pointer not captured: lost %s", pstr(w, L"lost"));
    a = vi4(7);
    hr = dcall(e, L"msReleasePointerCapture", DISPATCH_METHOD, &a, 1, NULL); pump(100);
    CHECK(hr == S_OK && !strcmp(pstr(w, L"lost"), "1"), "release: lost %s", pstr(w, L"lost"));
    (void)r;
    IDispatch_Release(e); IDispatch_Release(w);
}

static void test_range_collection(IHTMLDocument2 *doc, IHTMLDocument3 *doc3)
{
    IHTMLSelectionObject *sel = NULL; IHTMLSelectionObject2 *sel2 = NULL; IDispatch *coll = NULL, *item = NULL; HRESULT hr;
    VARIANT r, a;
    IHTMLDocument2_get_selection(doc, &sel);
    CHECK(sel != NULL, "selection");
    if (!sel) return;
    hr = IHTMLSelectionObject_QueryInterface(sel, &IID_IHTMLSelectionObject2, (void **)&sel2);
    CHECK(hr == S_OK, "IHTMLSelectionObject2 %08lx", hr);
    if (!sel2) return;
    hr = IHTMLSelectionObject2_createRangeCollection(sel2, &coll);
    CHECK(hr == S_OK && coll, "createRangeCollection %08lx", hr);
    if (coll) {
        CHECK(!strcmp(pstr(coll, L"length"), "1"), "length %s", pstr(coll, L"length"));
        a = vi4(0); hr = dcall(coll, L"item", DISPATCH_METHOD, &a, 1, &r);
        CHECK(hr == S_OK && V_VT(&r) == VT_DISPATCH, "item(0) %08lx", hr);
        if (hr == S_OK && V_VT(&r) == VT_DISPATCH) {
            IHTMLTxtRange *tr = NULL;
            hr = IDispatch_QueryInterface(V_DISPATCH(&r), &IID_IHTMLTxtRange, (void **)&tr);
            CHECK(hr == S_OK, "item(0) is a TextRange %08lx", hr);
            if (tr) IHTMLTxtRange_Release(tr);
            VariantClear(&r);
        }
        hr = dcall(coll, L"0", DISPATCH_PROPERTYGET, NULL, 0, &r);
        CHECK(hr == S_OK && V_VT(&r) == VT_DISPATCH, "[0] %08lx", hr);
        VariantClear(&r);
        a = vi4(1); hr = dcall(coll, L"item", DISPATCH_METHOD, &a, 1, &r);
        CHECK(hr == S_OK && (V_VT(&r) != VT_DISPATCH || !V_DISPATCH(&r)), "item(1) is null %08lx", hr);
        (void)item;
        IDispatch_Release(coll);
    }
    IHTMLSelectionObject2_Release(sel2); IHTMLSelectionObject_Release(sel);
}

static void test_ops_profile(IHTMLDocument2 *doc)
{
    IHTMLWindow2 *win = NULL; IOmNavigator *nav = NULL; IDispatch *navd = NULL, *prof = NULL; VARIANT r, a[2]; HRESULT hr;
    IHTMLDocument2_get_parentWindow(doc, &win);
    IHTMLWindow2_get_navigator(win, &nav);
    IOmNavigator_QueryInterface(nav, &IID_IDispatch, (void **)&navd);
    hr = pget(navd, L"userProfile", &r);
    CHECK(hr == S_OK && V_VT(&r) == VT_DISPATCH && V_DISPATCH(&r), "userProfile %08lx", hr);
    if (hr == S_OK && V_VT(&r) == VT_DISPATCH) {
        prof = V_DISPATCH(&r);
        a[0] = vbstr(L"Vcard.Email");
        hr = dcall(prof, L"addRequest", DISPATCH_METHOD, a, 1, &r);
        CHECK(hr == S_OK && V_VT(&r) == VT_BOOL && V_BOOL(&r) == VARIANT_TRUE, "addRequest %08lx", hr);
        hr = dcall(prof, L"clearRequest", DISPATCH_METHOD, NULL, 0, &r);
        CHECK(hr == S_OK, "clearRequest %08lx", hr);
        hr = dcall(prof, L"getAttribute", DISPATCH_METHOD, a, 1, &r);
        CHECK(hr == S_OK, "getAttribute %08lx", hr);
        VariantClear(&r);
        hr = dcall(prof, L"commitChanges", DISPATCH_METHOD, NULL, 0, &r);
        CHECK(hr == S_OK && V_VT(&r) == VT_BOOL && V_BOOL(&r) == VARIANT_FALSE, "commitChanges %08lx", hr);
        VariantClear(&a[0]);
        IDispatch_Release(prof);
    }
    hr = pget(navd, L"opsProfile", &r);
    CHECK(hr == S_OK && V_VT(&r) == VT_DISPATCH && V_DISPATCH(&r), "opsProfile %08lx", hr);
    VariantClear(&r);
    IDispatch_Release(navd); IOmNavigator_Release(nav); IHTMLWindow2_Release(win);
}

static void test_travel_log(IHTMLDocument2 *doc)
{
    IHTMLWindow2 *win = NULL; ITLC *tlc = NULL; IHTMLLocation *loc = NULL; BSTR hash = NULL; HRESULT hr;
    WCHAR anc[] = L"anc";
    IHTMLDocument2_get_parentWindow(doc, &win);
    hr = IHTMLWindow2_QueryInterface(win, &IID_ITravelLogClient_, (void **)&tlc);
    CHECK(hr == S_OK && tlc, "ITravelLogClient %08lx", hr);
    {   /* baseline: the same navigation through location.hash */
        BSTR h = SysAllocString(L"base");
        IHTMLWindow2_get_location(win, &loc);
        hr = IHTMLLocation_put_hash(loc, h); SysFreeString(h);
        pump(300);
        IHTMLLocation_get_hash(loc, &hash);
        printf("note: put_hash baseline %08lx -> %ls\n", hr, hash ? hash : L"(none)");
        SysFreeString(hash); hash = NULL; IHTMLLocation_Release(loc);
    }
    if (tlc) {
        hr = tlc->lpVtbl->LoadHistoryPosition(tlc, anc, 0);
        CHECK(hr == S_OK, "LoadHistoryPosition %08lx", hr);
        pump(300);
        IHTMLWindow2_get_location(win, &loc);
        IHTMLLocation_get_hash(loc, &hash);
        /* a document without a host does not take over a fragment navigation (the put_hash baseline above), so
         * only the call itself can be checked here */
        (void)hash;
        SysFreeString(hash); IHTMLLocation_Release(loc);
        hr = tlc->lpVtbl->LoadHistoryPosition(tlc, NULL, 40);
        CHECK(hr == S_OK, "LoadHistoryPosition(NULL, 40) %08lx", hr);
        tlc->lpVtbl->Release(tlc);
    }
    IHTMLWindow2_Release(win);
}

static void test_parse_string(IHTMLDocument2 *doc)
{
    IMS *ms = NULL; IUnknown *cont = NULL; IHTMLDocument2 *cdoc = NULL; IHTMLElement *body = NULL; BSTR html = NULL; HRESULT hr;
    OLECHAR markup[] = L"<a id=x href=\"foo/bar.htm\">l</a><img id=y src=\"pics/p.gif\">";
    hr = IHTMLDocument2_QueryInterface(doc, &IID_IMarkupServices_, (void **)&ms);
    CHECK(hr == S_OK && ms, "IMarkupServices %08lx", hr);
    if (!ms) return;
    /* without the flag the URLs stay as written */
    hr = ms->lpVtbl->ParseString(ms, markup, 0, &cont, NULL, NULL);
    CHECK(hr == S_OK && cont, "ParseString %08lx", hr);
    if (cont) {
        IUnknown_QueryInterface(cont, &IID_IHTMLDocument2, (void **)&cdoc);
        IHTMLDocument2_get_body(cdoc, &body); IHTMLElement_get_innerHTML(body, &html);
        CHECK(html && wcsstr(html, L"foo/bar.htm") && !wcsstr(html, L"file:///"), "plain: %ls", html ? html : L"(none)");
        SysFreeString(html); IHTMLElement_Release(body); IHTMLDocument2_Release(cdoc); IUnknown_Release(cont);
    }
    /* PARSE_ABSOLUTIFYIE40URLS (1): relative to the parsing document's URL */
    cont = NULL;
    hr = ms->lpVtbl->ParseString(ms, markup, 1, &cont, NULL, NULL);
    CHECK(hr == S_OK && cont, "ParseString absolutify %08lx", hr);
    if (cont) {
        IUnknown_QueryInterface(cont, &IID_IHTMLDocument2, (void **)&cdoc);
        IHTMLDocument2_get_body(cdoc, &body); IHTMLElement_get_innerHTML(body, &html);
        CHECK(html && wcsstr(html, L"href=\"file:///") && wcsstr(html, L"/foo/bar.htm\"") && wcsstr(html, L"src=\"file:///") && wcsstr(html, L"/pics/p.gif\""), "absolutified: %ls", html ? html : L"(none)");
        SysFreeString(html); IHTMLElement_Release(body); IHTMLDocument2_Release(cdoc); IUnknown_Release(cont);
    }
    ms->lpVtbl->Release(ms);
}

static void test_script_error(IHTMLDocument2 *doc)
{
    IDispatch *w = window_disp(doc);
    run_script(doc, L"window.se = 0; var s = document.createElement('script'); s.onerror = function() { window.se++; };"
                    L"s.src = 'file:///C:/does-not-exist-sg.js'; document.body.appendChild(s);");
    pump(1500);
    CHECK(!strcmp(pstr(w, L"se"), "1"), "script onerror fired %s times", pstr(w, L"se"));
    IDispatch_Release(w);
}

static void test_xhr(const char *port)
{
    IHTMLDocument2 *doc = NULL; IPersistMoniker *pm = NULL; IMoniker *mk = NULL; WCHAR page_url[128];
    IDispatch *w, *x = NULL; VARIANT r, a[2]; HRESULT hr; SAFEARRAY *sa; unsigned char bytes[4] = {0, 255, 10, 13};
    WCHAR script[512], url[128];
    swprintf(url, ARRAY_SIZE(url), L"http://127.0.0.1:%hs/echo", port);
    swprintf(page_url, ARRAY_SIZE(page_url), L"http://127.0.0.1:%hs/page.htm", port);
    CoCreateInstance(&CLSID_HTMLDocument_, NULL, CLSCTX_INPROC_SERVER, &IID_IHTMLDocument2, (void **)&doc);
    IHTMLDocument2_QueryInterface(doc, &IID_IPersistMoniker, (void **)&pm);
    CreateURLMonikerEx(NULL, page_url, &mk, 0);
    hr = IPersistMoniker_Load(pm, FALSE, mk, NULL, STGM_READ);
    CHECK(hr == S_OK, "load page over http %08lx", hr);
    IMoniker_Release(mk); IPersistMoniker_Release(pm);
    pump(2500);
    w = window_disp(doc);
    run_script(doc, L"window.x = new XMLHttpRequest();");
    hr = pget(w, L"x", &r);
    CHECK(hr == S_OK && V_VT(&r) == VT_DISPATCH, "XMLHttpRequest %08lx", hr);
    if (hr != S_OK || V_VT(&r) != VT_DISPATCH) return;
    x = V_DISPATCH(&r);
    (void)script;
#define SEND(body, expect, what) do { \
        a[1] = vbstr(L"POST"); a[0] = vbstr(url); \
        VARIANT args[3]; args[2] = a[1]; args[1] = a[0]; args[0].vt = VT_BOOL; V_BOOL(&args[0]) = VARIANT_FALSE; \
        hr = dcall(x, L"open", DISPATCH_METHOD, args, 3, NULL); CHECK(hr == S_OK, "open %08lx", hr); \
        hr = dcall(x, L"send", DISPATCH_METHOD, &(body), 1, NULL); \
        CHECK(hr == S_OK, "send %s %08lx", what, hr); \
        CHECK(!strcmp(pstr(x, L"responseText"), expect), "%s: response '%s' expected '%s'", what, pstr(x, L"responseText"), expect); \
        VariantClear(&args[2]); VariantClear(&args[1]); } while (0)
    { VARIANT b = vbstr(L"hello"); SEND(b, "len=5 sum=532", "BSTR"); VariantClear(&b); }
    { VARIANT b = vi4(12345); SEND(b, "len=5 sum=255", "number"); }
    sa = SafeArrayCreateVector(VT_UI1, 0, 4);
    { void *p; SafeArrayAccessData(sa, &p); memcpy(p, bytes, 4); SafeArrayUnaccessData(sa); }
    { VARIANT b; V_VT(&b) = VT_ARRAY | VT_UI1; V_ARRAY(&b) = sa; SEND(b, "len=4 sum=278", "byte array"); }
    SafeArrayDestroy(sa);
    {
        IDispatch *xml = NULL; VARIANT b, ok; BSTR src = SysAllocString(L"<a>xy</a>"); VARIANT_BOOL success;
        CoCreateInstance(&CLSID_DOMDocument_, NULL, CLSCTX_INPROC_SERVER, &IID_IDispatch, (void **)&xml);
        if (xml) {
            VARIANT s = vbstr(L"<a>xy</a>");
            hr = dcall(xml, L"loadXML", DISPATCH_METHOD, &s, 1, &ok); CHECK(hr == S_OK, "loadXML %08lx", hr);
            VariantClear(&s);
            V_VT(&b) = VT_DISPATCH; V_DISPATCH(&b) = xml;
            SEND(b, "len=11 sum=749", "XML DOM");
            IDispatch_Release(xml);
        } else printf("note: no msxml DOMDocument\n");
        SysFreeString(src); (void)success;
    }
    { VARIANT b = vbstr(L"after"); SEND(b, "len=5 sum=530", "BSTR again"); VariantClear(&b); }
    IDispatch_Release(x); IDispatch_Release(w); IHTMLDocument2_Release(doc);
}

int main(int argc, char **argv)
{
    IHTMLDocument2 *doc; IHTMLDocument3 *doc3;
    setvbuf(stdout, NULL, _IONBF, 0);
    CoInitialize(NULL);
    write_file(L"rc.htm", page);
    load(L"rc.htm", &doc, &doc3, 2500);
    test_member_properties(doc, doc3);
    test_pointer_capture(doc, doc3);
    test_range_collection(doc, doc3);
    test_ops_profile(doc);
    test_travel_log(doc);
    test_parse_string(doc);
    test_script_error(doc);
    if (argc > 1) test_xhr(argv[1]);
    IHTMLDocument3_Release(doc3); IHTMLDocument2_Release(doc);
    pump(200);
    CoUninitialize();
    printf(fails ? "RESULT: FAIL\n" : "RESULT: PASS\n");
    return fails != 0;
}
