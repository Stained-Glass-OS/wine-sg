/* mshtml batch 5 (patch 3193): connection point enumerators, localStorage space, setTimeout of any value,
 * toStaticHTML, styleMedia, clipboardData, style sheet imports, hasFeature/isSupported, range scroll/help, object form. */
#define COBJMACROS
#include <windows.h>
#include <ole2.h>
#include <mshtml.h>
#include <mshtmhst.h>
#include <shlwapi.h>
#include <stdio.h>
#define INTERNET_MAX_URL_LENGTH 2084
#define ARRAY_SIZE(a) (sizeof(a)/sizeof((a)[0]))

static const CLSID CLSID_HTMLDocument_ = {0x25336920,0x03f9,0x11cf,{0x8f,0xd0,0x00,0xaa,0x00,0x68,0x6f,0x13}};
#define DEFIID(n,a) static const IID n##_ = {0x3050##a,0x98b5,0x11cf,{0xbb,0x82,0x00,0xaa,0x00,0xbd,0xce,0x0b}}
DEFIID(IID_IHTMLCurrentStyle4, f33b); DEFIID(IID_IHTMLCurrentStyle2, f658); DEFIID(IID_IHTMLDOMAttribute2, f810); DEFIID(IID_IHTMLElement4, f80f);
#define IID_IHTMLCurrentStyle4 IID_IHTMLCurrentStyle4_
#define IID_IHTMLCurrentStyle2 IID_IHTMLCurrentStyle2_
#define IID_IHTMLDOMAttribute2 IID_IHTMLDOMAttribute2_
#define IID_IHTMLElement4 IID_IHTMLElement4_
#define DEFIID5(n,a,b) static const IID n##_ = {0x##a,0x98b5,0x11cf,{0xbb,0x82,0x00,0xaa,0x00,0xbd,0xce,0x0b}}
DEFIID5(IID_IMarkupContainer, 3050f5f9, 0); DEFIID5(IID_IHTMLWindow7, 305104b7, 0); DEFIID5(IID_IHTMLWindow6, 30510453, 0);
#define IID_IMarkupContainer IID_IMarkupContainer_
#define IID_IHTMLWindow7 IID_IHTMLWindow7_
#define IID_IHTMLWindow6 IID_IHTMLWindow6_
static HRESULT (WINAPI *pShowHTMLDialog)(HWND, IMoniker *, VARIANT *, WCHAR *, VARIANT *);
static HRESULT (WINAPI *pShowModelessHTMLDialog)(HWND, IMoniker *, VARIANT *, VARIANT *, IHTMLWindow2 **);
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

static IMoniker *moniker_of(const WCHAR *file)
{
    WCHAR full[MAX_PATH], url[INTERNET_MAX_URL_LENGTH];
    DWORD size = INTERNET_MAX_URL_LENGTH;
    IMoniker *mk = NULL;
    GetFullPathNameW(file, MAX_PATH, full, NULL);
    UrlCreateFromPathW(full, url, &size, 0);
    CreateURLMonikerEx(NULL, url, &mk, 0);
    return mk;
}

/* ---- IDispatch helpers ---- */
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
static HRESULT pput(IDispatch *d, const WCHAR *name, VARIANT v) { return dcall(d, name, DISPATCH_PROPERTYPUT, &v, 1, NULL); }
static VARIANT vbstr(const WCHAR *s) { VARIANT v; V_VT(&v) = VT_BSTR; V_BSTR(&v) = SysAllocString(s); return v; }
static VARIANT vi4(int i) { VARIANT v; V_VT(&v) = VT_I4; V_I4(&v) = i; return v; }
static VARIANT vbool(BOOL b) { VARIANT v; V_VT(&v) = VT_BOOL; V_BOOL(&v) = b ? VARIANT_TRUE : VARIANT_FALSE; return v; }

static char sbuf[8][256];
static int sidx;
static const char *vs(VARIANT *v)
{
    VARIANT s; char *b = sbuf[sidx++ & 7];
    V_VT(&s) = VT_EMPTY;
    if (V_VT(v) == VT_EMPTY) return "(empty)";
    if (FAILED(VariantChangeType(&s, v, 0, VT_BSTR))) return "(?)";
    WideCharToMultiByte(CP_UTF8, 0, V_BSTR(&s), -1, b, 255, NULL, NULL);
    VariantClear(&s);
    return b;
}
static const char *pstr(IDispatch *d, const WCHAR *name)
{
    VARIANT v; HRESULT hr = pget(d, name, &v);
    const char *s;
    if (FAILED(hr)) { char *b = sbuf[sidx++ & 7]; sprintf(b, "ERR%08lx", hr); return b; }
    s = vs(&v);
    VariantClear(&v);
    return s;
}
static IDispatch *by_id(IHTMLDocument3 *doc3, const WCHAR *id)
{
    IHTMLElement *e = NULL; IDispatch *d = NULL; BSTR b = SysAllocString(id);
    IHTMLDocument3_getElementById(doc3, b, &e);
    SysFreeString(b);
    if (e) { IHTMLElement_QueryInterface(e, &IID_IDispatch, (void **)&d); IHTMLElement_Release(e); }
    return d;
}
static int coll_len(IDispatch *d)
{
    VARIANT v; int n = -1;
    if (d && SUCCEEDED(pget(d, L"length", &v))) n = V_I4(&v);
    return n;
}


typedef struct { IPropertyNotifySink IPropertyNotifySink_iface; LONG ref; } Sink;
static HRESULT WINAPI Sink_QI(IPropertyNotifySink *i, REFIID r, void **p)
{
    if (IsEqualGUID(r, &IID_IUnknown) || IsEqualGUID(r, &IID_IPropertyNotifySink)) { *p = i; return S_OK; }
    *p = NULL; return E_NOINTERFACE;
}
static ULONG WINAPI Sink_AddRef(IPropertyNotifySink *i) { return InterlockedIncrement(&((Sink *)i)->ref); }
static ULONG WINAPI Sink_Release(IPropertyNotifySink *i) { return InterlockedDecrement(&((Sink *)i)->ref); }
static HRESULT WINAPI Sink_OnChanged(IPropertyNotifySink *i, DISPID d) { return S_OK; }
static HRESULT WINAPI Sink_OnRequestEdit(IPropertyNotifySink *i, DISPID d) { return S_OK; }
static const IPropertyNotifySinkVtbl SinkVtbl = { Sink_QI, Sink_AddRef, Sink_Release, Sink_OnChanged, Sink_OnRequestEdit };

static const char page[] =
    "<html><head><title>b5</title><style id=st type=\"text/css\">p{color:red}</style></head><body>"
    "<form id=f><object id=ob1></object></form><object id=ob2></object><p id=para>some text for a range</p>"
    "</body></html>";

int main(void)
{
    IHTMLDocument2 *doc; IHTMLDocument3 *doc3; IPersistFile *pf; HRESULT hr;
    WCHAR path[MAX_PATH]; IDispatch *e; VARIANT v, r; BSTR s; int i;

    setvbuf(stdout, NULL, _IONBF, 0);
    CoInitialize(NULL);
    write_file(L"b5.htm", page);
    GetFullPathNameW(L"b5.htm", MAX_PATH, path, NULL);
    CoCreateInstance(&CLSID_HTMLDocument_, NULL, CLSCTX_INPROC_SERVER, &IID_IHTMLDocument2, (void **)&doc);
    IHTMLDocument2_QueryInterface(doc, &IID_IHTMLDocument3, (void **)&doc3);
    IHTMLDocument2_QueryInterface(doc, &IID_IPersistFile, (void **)&pf);
    hr = IPersistFile_Load(pf, path, STGM_READ);
    CHECK(hr == S_OK, "load %08lx", hr);
    pump(2500);

    /* connection points: the container lists them, the connection enumerator walks the advised sinks */
    {
        IConnectionPointContainer *cpc = NULL; IEnumConnectionPoints *ecp = NULL, *ecp2 = NULL; IConnectionPoint *cp = NULL, *cps[16];
        ULONG got = 0, total = 0;
        IHTMLDocument2_QueryInterface(doc, &IID_IConnectionPointContainer, (void **)&cpc);
        CHECK(cpc != NULL, "IConnectionPointContainer");
        if (cpc) {
            hr = IConnectionPointContainer_EnumConnectionPoints(cpc, &ecp);
            CHECK(hr == S_OK && ecp, "EnumConnectionPoints %08lx", hr);
            if (ecp) {
                hr = IEnumConnectionPoints_Next(ecp, 16, cps, &total);
                CHECK(total >= 2 && (hr == S_FALSE || hr == S_OK), "listed %lu points (%08lx)", total, hr);
                for (i = 0; i < (int)total; i++) IConnectionPoint_Release(cps[i]);
                IEnumConnectionPoints_Reset(ecp);
                hr = IEnumConnectionPoints_Skip(ecp, 1); CHECK(hr == S_OK, "cp Skip %08lx", hr);
                hr = IEnumConnectionPoints_Clone(ecp, &ecp2); CHECK(hr == S_OK && ecp2, "cp Clone %08lx", hr);
                if (ecp2) {
                    got = 0; hr = IEnumConnectionPoints_Next(ecp2, 16, cps, &got);
                    CHECK(got == total - 1, "clone continues after the skipped one: %lu of %lu", got, total);
                    for (i = 0; i < (int)got; i++) IConnectionPoint_Release(cps[i]);
                    IEnumConnectionPoints_Release(ecp2);
                }
                hr = IEnumConnectionPoints_Skip(ecp, 1000); CHECK(hr == S_FALSE, "cp Skip past the end %08lx", hr);
                IEnumConnectionPoints_Release(ecp);
            }
            hr = IConnectionPointContainer_FindConnectionPoint(cpc, &IID_IPropertyNotifySink, &cp);
            CHECK(hr == S_OK && cp, "find IPropertyNotifySink point %08lx", hr);
            if (cp) {
                Sink sink[3]; DWORD cookie[3]; IEnumConnections *ec = NULL, *ec2 = NULL; CONNECTDATA cd[4];
                for (i = 0; i < 3; i++) {
                    sink[i].IPropertyNotifySink_iface.lpVtbl = (IPropertyNotifySinkVtbl *)&SinkVtbl; sink[i].ref = 1;
                    hr = IConnectionPoint_Advise(cp, (IUnknown *)&sink[i], &cookie[i]); CHECK(hr == S_OK, "Advise %08lx", hr);
                }
                IConnectionPoint_Unadvise(cp, cookie[1]);     /* a hole in the list: only live sinks count */
                hr = IConnectionPoint_EnumConnections(cp, &ec); CHECK(hr == S_OK && ec, "EnumConnections %08lx", hr);
                if (ec) {
                    hr = IEnumConnections_Skip(ec, 1); CHECK(hr == S_OK, "Skip %08lx", hr);
                    hr = IEnumConnections_Clone(ec, &ec2); CHECK(hr == S_OK && ec2, "Clone %08lx", hr);
                    got = 0; hr = IEnumConnections_Next(ec, 4, cd, &got);
                    CHECK(got == 1 && cd[0].pUnk == (IUnknown *)&sink[2], "one sink after the skipped one: %lu", got);
                    if (got) IUnknown_Release(cd[0].pUnk);
                    if (ec2) {
                        got = 0; hr = IEnumConnections_Next(ec2, 4, cd, &got);
                        CHECK(got == 1 && cd[0].pUnk == (IUnknown *)&sink[2], "clone sees the same one: %lu", got);
                        if (got) IUnknown_Release(cd[0].pUnk);
                        IEnumConnections_Release(ec2);
                    }
                    hr = IEnumConnections_Reset(ec); CHECK(hr == S_OK, "Reset %08lx", hr);
                    got = 0; hr = IEnumConnections_Next(ec, 4, cd, &got);
                    CHECK(got == 2 && cd[0].pUnk == (IUnknown *)&sink[0], "after Reset both live sinks: %lu", got);
                    for (i = 0; i < (int)got; i++) IUnknown_Release(cd[i].pUnk);
                    IEnumConnections_Reset(ec);
                    hr = IEnumConnections_Skip(ec, 2); CHECK(hr == S_OK, "Skip of both live sinks %08lx", hr);
                    got = 9; hr = IEnumConnections_Next(ec, 4, cd, &got); CHECK(got == 0, "nothing after skipping both: %lu", got);
                    IEnumConnections_Reset(ec);
                    hr = IEnumConnections_Skip(ec, 5); CHECK(hr == S_FALSE, "Skip past the end %08lx", hr);
                    IEnumConnections_Release(ec);
                }
                IConnectionPoint_Unadvise(cp, cookie[0]); IConnectionPoint_Unadvise(cp, cookie[2]);
                IConnectionPoint_Release(cp);
            }
            IConnectionPointContainer_Release(cpc);
        }
    }

    /* window: setTimeout of a number, toStaticHTML, styleMedia, clipboardData */
    {
        IHTMLWindow2 *win = NULL; IHTMLWindow3 *win3 = NULL; IHTMLWindow6 *win6 = NULL; IHTMLWindow7 *win7 = NULL;
        IHTMLDocument2_get_parentWindow(doc, &win);
        IHTMLWindow2_QueryInterface(win, &IID_IHTMLWindow3, (void **)&win3);
        IHTMLWindow2_QueryInterface(win, &IID_IHTMLWindow6, (void **)&win6);
        IHTMLWindow2_QueryInterface(win, &IID_IHTMLWindow7, (void **)&win7);
        if (win3) {
            VARIANT expr, lang; LONG id = 0;
            V_VT(&expr) = VT_I4; V_I4(&expr) = 5; V_VT(&lang) = VT_EMPTY;
            hr = IHTMLWindow3_setTimeout(win3, &expr, 10, &lang, &id);
            CHECK(hr == S_OK && id != 0, "setTimeout of a number %08lx id %ld", hr, id);
            if (hr == S_OK) IHTMLWindow2_clearTimeout(win, id);
        }
        if (win6) {
            BSTR in = SysAllocString(L"<p onclick=\"evil()\" id=keep>a<script>evil()</script><b style=\"width:expression(evil())\">b</b>"
                                     L"<a href=\"javascript:evil()\" title=ok>c</a><img src=x onerror=\"evil()\"></p>");
            BSTR out = NULL;
            hr = IHTMLWindow6_toStaticHTML(win6, in, &out);
            CHECK(hr == S_OK && out, "toStaticHTML %08lx", hr);
            if (out) {
                CHECK(!wcsstr(out, L"evil") && !wcsstr(out, L"onclick") && !wcsstr(out, L"script") && !wcsstr(out, L"expression") && !wcsstr(out, L"javascript"),
                      "script survived: %ls", out);
                CHECK(wcsstr(out, L"id=\"keep\"") && wcsstr(out, L"title=\"ok\"") && wcsstr(out, L">b</b>") && wcsstr(out, L">c</a>"),
                      "static content lost: %ls", out);
            }
            SysFreeString(in); SysFreeString(out);
            hr = IHTMLWindow6_toStaticHTML(win6, NULL, &out); CHECK(hr == S_OK && (!out || !*out), "toStaticHTML(NULL) %08lx", hr); SysFreeString(out);
        }
        if (win7) {
            IHTMLStyleMedia *media = NULL; VARIANT_BOOL m = VARIANT_FALSE; BSTR t = NULL, q;
            hr = IHTMLWindow7_get_styleMedia(win7, &media);
            CHECK(hr == S_OK && media, "styleMedia %08lx", hr);
            if (media) {
                IHTMLStyleMedia_get_type(media, &t); CHECK(t && !wcscmp(t, L"screen"), "styleMedia type %ls", t ? t : L""); SysFreeString(t);
                q = SysAllocString(L"screen"); hr = IHTMLStyleMedia_matchMedium(media, q, &m); CHECK(hr == S_OK && m == VARIANT_TRUE, "matchMedium(screen) %08lx %d", hr, m); SysFreeString(q);
                q = SysAllocString(L"print"); hr = IHTMLStyleMedia_matchMedium(media, q, &m); CHECK(hr == S_OK && m == VARIANT_FALSE, "matchMedium(print) %08lx %d", hr, m); SysFreeString(q);
                IHTMLStyleMedia_Release(media);
            }
        }
        if (win3) {
            IHTMLDataTransfer *cd = NULL; VARIANT_BOOL ok = VARIANT_FALSE; VARIANT data, ret; BSTR fmt, eff;
            hr = IHTMLWindow3_get_clipboardData(win3, &cd);
            CHECK(hr == S_OK && cd, "clipboardData %08lx", hr);
            if (cd) {
                fmt = SysAllocString(L"Text"); data = vbstr(L"clip text");
                hr = IHTMLDataTransfer_setData(cd, fmt, &data, &ok); CHECK(hr == S_OK && ok == VARIANT_TRUE, "setData %08lx %d", hr, ok);
                VariantClear(&data);
                hr = IHTMLDataTransfer_getData(cd, fmt, &ret); CHECK(hr == S_OK && V_VT(&ret) == VT_BSTR && !strcmp(vs(&ret), "clip text"), "getData %08lx '%s'", hr, vs(&ret)); VariantClear(&ret);
                hr = IHTMLDataTransfer_clearData(cd, fmt, &ok); CHECK(hr == S_OK && ok == VARIANT_TRUE, "clearData %08lx", hr);
                hr = IHTMLDataTransfer_getData(cd, fmt, &ret); CHECK(hr == S_OK && V_VT(&ret) == VT_NULL, "getData after clear: vt %d", V_VT(&ret)); VariantClear(&ret);
                SysFreeString(fmt);
                fmt = SysAllocString(L"NoSuchFormat"); hr = IHTMLDataTransfer_getData(cd, fmt, &ret); CHECK(hr == S_OK && V_VT(&ret) == VT_NULL, "unknown format vt %d", V_VT(&ret)); SysFreeString(fmt);
                eff = SysAllocString(L"copy"); hr = IHTMLDataTransfer_put_dropEffect(cd, eff); CHECK(hr == S_OK, "put dropEffect %08lx", hr); SysFreeString(eff);
                eff = NULL; IHTMLDataTransfer_get_dropEffect(cd, &eff); CHECK(eff && !wcscmp(eff, L"copy"), "dropEffect %ls", eff ? eff : L""); SysFreeString(eff);
                eff = SysAllocString(L"bogus"); hr = IHTMLDataTransfer_put_dropEffect(cd, eff); CHECK(hr == E_INVALIDARG, "bad dropEffect %08lx", hr); SysFreeString(eff);
                IHTMLDataTransfer_Release(cd);
            }
        }
        if (win7) IHTMLWindow7_Release(win7);
        if (win6) IHTMLWindow6_Release(win6);
        if (win3) IHTMLWindow3_Release(win3);
        if (win) IHTMLWindow2_Release(win);
    }

    /* the local storage of a page with a host name (res://mshtml.dll/): space left counts the characters stored */
    {
        IHTMLDocument2 *rdoc = NULL; IPersistMoniker *pm = NULL; IMoniker *mk = NULL; IHTMLWindow2 *rwin = NULL; IHTMLWindow6 *win6 = NULL;
        CoCreateInstance(&CLSID_HTMLDocument_, NULL, CLSCTX_INPROC_SERVER, &IID_IHTMLDocument2, (void **)&rdoc);
        IHTMLDocument2_QueryInterface(rdoc, &IID_IPersistMoniker, (void **)&pm);
        CreateURLMonikerEx(NULL, L"res://mshtml.dll/blank.htm", &mk, 0);
        IPersistMoniker_Load(pm, FALSE, mk, NULL, STGM_READ);
        pump(1500);
        IHTMLDocument2_get_parentWindow(rdoc, &rwin);
        IHTMLWindow2_QueryInterface(rwin, &IID_IHTMLWindow6, (void **)&win6);
        if (win6) {
            IHTMLStorage *ls = NULL; LONG before = -1, after = -1; BSTR k, val;
            hr = IHTMLWindow6_get_localStorage(win6, &ls);
            CHECK(hr == S_OK && ls, "localStorage %08lx", hr);
            if (ls) {
                k = SysAllocString(L"b5key"); val = SysAllocString(L"b5value");
                IHTMLStorage_removeItem(ls, k);
                hr = IHTMLStorage_get_remainingSpace(ls, &before); CHECK(hr == S_OK && before > 4000000, "local remainingSpace %08lx %ld", hr, before);
                IHTMLStorage_setItem(ls, k, val);
                hr = IHTMLStorage_get_remainingSpace(ls, &after); CHECK(hr == S_OK && after == before - 12, "remainingSpace after setItem %08lx %ld -> %ld", hr, before, after);
                IHTMLStorage_removeItem(ls, k);
                hr = IHTMLStorage_get_remainingSpace(ls, &after); CHECK(hr == S_OK && after == before, "remainingSpace after removeItem %ld", after);
                SysFreeString(k); SysFreeString(val);
                IHTMLStorage_Release(ls);
            }
        }
        if (win6) IHTMLWindow6_Release(win6);
        if (rwin) IHTMLWindow2_Release(rwin);
        IMoniker_Release(mk); IPersistMoniker_Release(pm); IHTMLDocument2_Release(rdoc);
    }

    /* element collection enumerator clone, the container of a document */
    {
        IHTMLElementCollection *all = NULL; IUnknown *unk = NULL; IEnumVARIANT *en = NULL, *en2 = NULL; VARIANT a, b; ULONG n1 = 0, n2 = 0;
        IMarkupContainer *mc = NULL; IHTMLDocument2 *owner = NULL;
        IHTMLDocument2_get_all(doc, &all);
        IHTMLElementCollection_get__newEnum(all, &unk);
        IUnknown_QueryInterface(unk, &IID_IEnumVARIANT, (void **)&en);
        IEnumVARIANT_Skip(en, 2);
        hr = IEnumVARIANT_Clone(en, &en2); CHECK(hr == S_OK && en2, "enum Clone %08lx", hr);
        if (en2) {
            VariantInit(&a); VariantInit(&b);
            IEnumVARIANT_Next(en, 1, &a, &n1); IEnumVARIANT_Next(en2, 1, &b, &n2);
            CHECK(n1 == 1 && n2 == 1 && V_DISPATCH(&a) == V_DISPATCH(&b), "clone is at the same place");
            VariantClear(&a); VariantClear(&b);
            IEnumVARIANT_Release(en2);
        }
        IEnumVARIANT_Release(en); IUnknown_Release(unk); IHTMLElementCollection_Release(all);
        hr = IHTMLDocument2_QueryInterface(doc, &IID_IMarkupContainer, (void **)&mc);
        if (mc) { hr = IMarkupContainer_OwningDoc(mc, &owner); CHECK(hr == S_OK && owner, "OwningDoc %08lx", hr); if (owner) IHTMLDocument2_Release(owner); IMarkupContainer_Release(mc); }
    }

    /* style sheet imports and href */
    {
        IHTMLStyleSheetsCollection *sheets = NULL; VARIANT idx, sv; IDispatch *sd = NULL; IHTMLStyleSheet *sheet = NULL; LONG ix = -5, n = -1;
        IHTMLStyleSheetRulesCollection *rules = NULL; BSTR u;
        IHTMLDocument2_get_styleSheets(doc, &sheets);
        idx = vi4(0);
        hr = IHTMLStyleSheetsCollection_item(sheets, &idx, &sv); CHECK(hr == S_OK && V_VT(&sv) == VT_DISPATCH, "styleSheets.item %08lx", hr);
        if (hr == S_OK && V_VT(&sv) == VT_DISPATCH) {
            sd = V_DISPATCH(&sv); IDispatch_QueryInterface(sd, &IID_IHTMLStyleSheet, (void **)&sheet);
            IHTMLStyleSheet_get_rules(sheet, &rules); IHTMLStyleSheetRulesCollection_get_length(rules, &n); IHTMLStyleSheetRulesCollection_Release(rules);
            CHECK(n == 1, "rules before %ld", n);
            u = SysAllocString(L"b5.css");
            hr = IHTMLStyleSheet_addImport(sheet, u, -1, &ix); CHECK(hr == S_OK && ix == 0, "addImport %08lx index %ld", hr, ix);
            SysFreeString(u);
            u = SysAllocString(L"b5b.css");
            hr = IHTMLStyleSheet_addImport(sheet, u, 0, &ix); CHECK(hr == S_OK && ix == 0, "addImport at 0 %08lx index %ld", hr, ix);
            SysFreeString(u);
            IHTMLStyleSheet_get_rules(sheet, &rules); IHTMLStyleSheetRulesCollection_get_length(rules, &n); IHTMLStyleSheetRulesCollection_Release(rules);
            CHECK(n == 3, "rules after two imports %ld", n);
            hr = IHTMLStyleSheet_removeImport(sheet, 1); CHECK(hr == S_OK, "removeImport %08lx", hr);
            hr = IHTMLStyleSheet_removeImport(sheet, 5); CHECK(hr == E_INVALIDARG, "removeImport out of range %08lx", hr);
            IHTMLStyleSheet_get_rules(sheet, &rules); IHTMLStyleSheetRulesCollection_get_length(rules, &n); IHTMLStyleSheetRulesCollection_Release(rules);
            CHECK(n == 2, "rules after a removal %ld", n);
            u = SysAllocString(L"other.css"); hr = IHTMLStyleSheet_put_href(sheet, u); CHECK(hr == S_OK, "put_href %08lx", hr); SysFreeString(u);
            { IDispatch *st = by_id(doc3, L"st"); CHECK(!strcmp(pstr(st, L"href"), "other.css") || strstr(pstr(st, L"href"), "other.css"), "owner href '%s'", pstr(st, L"href")); IDispatch_Release(st); }
            IHTMLStyleSheet_Release(sheet);
            VariantClear(&sv);
        }
        IHTMLStyleSheetsCollection_Release(sheets);
    }

    /* text range: scroll into view, help for a command */
    {
        IHTMLBodyElement *body = NULL; IHTMLElement *be = NULL; IHTMLTxtRange *range = NULL; VARIANT_BOOL ok = 5; BSTR c;
        IHTMLDocument2_get_body(doc, &be);
        IHTMLElement_QueryInterface(be, &IID_IHTMLBodyElement, (void **)&body);
        IHTMLBodyElement_createTextRange(body, &range);
        CHECK(range != NULL, "createTextRange");
        if (range) {
            hr = IHTMLTxtRange_scrollIntoView(range, VARIANT_TRUE); CHECK(hr == S_OK, "scrollIntoView(start) %08lx", hr);
            hr = IHTMLTxtRange_scrollIntoView(range, VARIANT_FALSE); CHECK(hr == S_OK, "scrollIntoView(end) %08lx", hr);
            c = SysAllocString(L"Bold"); ok = 5; hr = IHTMLTxtRange_execCommandShowHelp(range, c, &ok); CHECK(hr == S_OK && (ok == VARIANT_TRUE || ok == VARIANT_FALSE), "execCommandShowHelp %08lx %d", hr, ok); SysFreeString(c);
            c = SysAllocString(L"NoSuchCommandAtAll"); ok = 5; hr = IHTMLTxtRange_execCommandShowHelp(range, c, &ok); CHECK(hr == S_OK && ok == VARIANT_FALSE, "ShowHelp of an unknown command %08lx %d", hr, ok); SysFreeString(c);
            IHTMLTxtRange_Release(range);
        }
        IHTMLBodyElement_Release(body); IHTMLElement_Release(be);
    }

    /* object: the form around it */
    {
        IDispatch *o1 = by_id(doc3, L"ob1"), *o2 = by_id(doc3, L"ob2"); IHTMLObjectElement *obj = NULL; IHTMLFormElement *form = NULL;
        IDispatch_QueryInterface(o1, &IID_IHTMLObjectElement, (void **)&obj);
        hr = IHTMLObjectElement_get_form(obj, &form); CHECK(hr == S_OK && form, "object.form inside a form %08lx", hr);
        if (form) { IDispatch *fd = NULL; IHTMLFormElement_QueryInterface(form, &IID_IDispatch, (void **)&fd); CHECK(!strcmp(pstr(fd, L"id"), "f"), "form id '%s'", pstr(fd, L"id")); IDispatch_Release(fd); IHTMLFormElement_Release(form); }
        IHTMLObjectElement_Release(obj);
        form = NULL; obj = NULL;
        IDispatch_QueryInterface(o2, &IID_IHTMLObjectElement, (void **)&obj);
        hr = IHTMLObjectElement_get_form(obj, &form); CHECK(hr == S_OK && !form, "object.form outside a form %08lx %p", hr, form);
        IHTMLObjectElement_Release(obj);
        IDispatch_Release(o1); IDispatch_Release(o2);
    }

    IPersistFile_Release(pf); IHTMLDocument3_Release(doc3); IHTMLDocument2_Release(doc);
    pump(200);
    CoUninitialize();
    (void)s; (void)v; (void)r;
    printf(fails ? "RESULT: FAIL\n" : "RESULT: PASS\n");
    return fails != 0;
}
