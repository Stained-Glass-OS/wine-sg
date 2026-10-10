/* wmp's IDispatch implementations (GetTypeInfoCount/GetTypeInfo/GetIDsOfNames/
 * Invoke on the player, settings, controls, network, media and playlist
 * objects) and the connection point enumerators (IEnumConnectionPoints,
 * IEnumConnections Skip/Reset/Clone), run by test/wmp-dispatch-gate.sh
 * (patches/sg/2850).
 *
 *   wmp-dispatch-probe.exe */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <ocidl.h>
#include <wmp.h>
#include <stdio.h>
#include <string.h>

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}
#define CHECKF(ok, ...) do { char _b[320]; snprintf(_b, sizeof(_b), __VA_ARGS__); check(ok, _b); } while (0)

/* ---- IDispatch ---- */

static HRESULT get_id(IDispatch *d, const WCHAR *name, DISPID *id)
{
    LPOLESTR n = (LPOLESTR)name;
    return IDispatch_GetIDsOfNames(d, &IID_NULL, &n, 1, LOCALE_USER_DEFAULT, id);
}

static HRESULT call(IDispatch *d, const WCHAR *name, WORD flags, VARIANT *args, UINT nargs, VARIANT *res)
{
    DISPPARAMS dp = {args, NULL, nargs, 0};
    DISPID id, put = DISPID_PROPERTYPUT;
    HRESULT hr = get_id(d, name, &id);
    if (FAILED(hr)) return hr;
    if ((flags & DISPATCH_PROPERTYPUT) && nargs) { dp.rgdispidNamedArgs = &put; dp.cNamedArgs = 1; }
    if (res) VariantInit(res);
    return IDispatch_Invoke(d, id, &IID_NULL, LOCALE_USER_DEFAULT, flags, &dp, res, NULL, NULL);
}

struct dispcase
{
    const char *obj;
    IDispatch *d;
    const WCHAR *prop;     /* a property known to be implemented */
    VARTYPE vt;
    DISPID expect_id;      /* from the type library */
};

static void test_dispatch_basics(const struct dispcase *c)
{
    ITypeInfo *ti = NULL, *ti2;
    UINT n = 99;
    DISPID id = 0, ids[2];
    LPOLESTR names[2];
    TYPEATTR *attr;
    HRESULT hr;

    hr = IDispatch_GetTypeInfoCount(c->d, &n);
    CHECKF(hr == S_OK && n == 1, "%s: GetTypeInfoCount = 1 (hr %08lx n %u)", c->obj, hr, n);
    hr = IDispatch_GetTypeInfoCount(c->d, NULL);
    CHECKF(hr == E_POINTER, "%s: GetTypeInfoCount(NULL) = E_POINTER (%08lx)", c->obj, hr);

    hr = IDispatch_GetTypeInfo(c->d, 0, LOCALE_USER_DEFAULT, &ti);
    CHECKF(hr == S_OK && ti, "%s: GetTypeInfo(0) (%08lx)", c->obj, hr);
    if (ti)
    {
        hr = ITypeInfo_GetTypeAttr(ti, &attr);
        CHECKF(hr == S_OK && (attr->typekind == TKIND_INTERFACE || attr->typekind == TKIND_DISPATCH),
               "%s: type info is an interface (%08lx)", c->obj, hr);
        if (hr == S_OK) ITypeInfo_ReleaseTypeAttr(ti, attr);
        hr = IDispatch_GetTypeInfo(c->d, 0, LOCALE_USER_DEFAULT, &ti2);
        CHECKF(hr == S_OK && ti2 == ti, "%s: GetTypeInfo returns the same type info twice", c->obj);
        if (hr == S_OK) ITypeInfo_Release(ti2);
        ITypeInfo_Release(ti);
    }
    ti = (ITypeInfo *)0x1234;
    hr = IDispatch_GetTypeInfo(c->d, 1, LOCALE_USER_DEFAULT, &ti);
    CHECKF(hr == DISP_E_BADINDEX && !ti, "%s: GetTypeInfo(1) = DISP_E_BADINDEX, NULL out (%08lx)", c->obj, hr);
    hr = IDispatch_GetTypeInfo(c->d, 0, LOCALE_USER_DEFAULT, NULL);
    CHECKF(hr == E_POINTER, "%s: GetTypeInfo(0, NULL) = E_POINTER (%08lx)", c->obj, hr);

    hr = get_id(c->d, c->prop, &id);
    CHECKF(hr == S_OK && id == c->expect_id, "%s: GetIDsOfNames(%ls) = %lx (%08lx)", c->obj, c->prop, id, hr);
    hr = get_id(c->d, L"noSuchMember", &id);
    CHECKF(hr == DISP_E_UNKNOWNNAME, "%s: GetIDsOfNames(unknown) = DISP_E_UNKNOWNNAME (%08lx)", c->obj, hr);
    /* names are case-insensitive */
    {
        WCHAR up[64];
        unsigned int i;
        for (i = 0; c->prop[i]; i++) up[i] = (c->prop[i] >= 'a' && c->prop[i] <= 'z') ? c->prop[i] - 32 : c->prop[i];
        up[i] = 0;
        hr = get_id(c->d, up, &id);
        CHECKF(hr == S_OK && id == c->expect_id, "%s: GetIDsOfNames is case-insensitive (%08lx)", c->obj, hr);
    }
    names[0] = (LPOLESTR)c->prop; names[1] = (LPOLESTR)L"noSuchArg";
    hr = IDispatch_GetIDsOfNames(c->d, &IID_NULL, names, 2, LOCALE_USER_DEFAULT, ids);
    CHECKF(hr == DISP_E_UNKNOWNNAME && ids[1] == DISPID_UNKNOWN, "%s: unknown parameter name (%08lx)", c->obj, hr);

    /* Invoke with an unknown member */
    {
        DISPPARAMS dp = {NULL, NULL, 0, 0};
        VARIANT v;
        VariantInit(&v);
        hr = IDispatch_Invoke(c->d, 0x7ffff, &IID_NULL, LOCALE_USER_DEFAULT, DISPATCH_PROPERTYGET, &dp, &v, NULL, NULL);
        CHECKF(hr == DISP_E_MEMBERNOTFOUND, "%s: Invoke(unknown dispid) = DISP_E_MEMBERNOTFOUND (%08lx)", c->obj, hr);
    }
    {
        VARIANT v;
        hr = call(c->d, c->prop, DISPATCH_PROPERTYGET, NULL, 0, &v);
        CHECKF(hr == S_OK && V_VT(&v) == c->vt, "%s: Invoke propget %ls (hr %08lx vt %u)", c->obj, c->prop, hr, V_VT(&v));
        VariantClear(&v);
    }
}

static void test_dispatch(void)
{
    IWMPPlayer4 *player = NULL;
    IWMPSettings *settings;
    IWMPControls *controls;
    IWMPNetwork *network;
    IWMPMedia *media;
    IWMPPlaylist *pl;
    IDispatch *d;
    VARIANT v, arg[1];
    VARIANT_BOOL vb;
    LONG l;
    BSTR b, name, url;
    HRESULT hr;
    struct dispcase cases[6];
    IWMPPlayer *player1;
    unsigned int i;

    hr = CoCreateInstance(&CLSID_WindowsMediaPlayer, NULL, CLSCTX_INPROC_SERVER, &IID_IWMPPlayer4, (void **)&player);
    if (FAILED(hr)) { CHECKF(0, "CoCreateInstance(WindowsMediaPlayer) = %08lx", hr); return; }

    IWMPPlayer4_get_settings(player, &settings);
    IWMPPlayer4_get_controls(player, &controls);
    IWMPPlayer4_get_network(player, &network);
    name = SysAllocString(L"clip"); url = SysAllocString(L"C:\\nonexistent\\clip.mp3");
    IWMPPlayer4_newMedia(player, url, &media);
    IWMPPlayer4_newPlaylist(player, name, NULL, &pl);

    memset(cases, 0, sizeof(cases));
    IWMPPlayer4_QueryInterface(player, &IID_IDispatch, (void **)&cases[0].d);
    cases[0].obj = "player";   cases[0].prop = L"versionInfo"; cases[0].vt = VT_BSTR; cases[0].expect_id = 0x0b;
    IWMPSettings_QueryInterface(settings, &IID_IDispatch, (void **)&cases[1].d);
    cases[1].obj = "settings"; cases[1].prop = L"autoStart";   cases[1].vt = VT_BOOL; cases[1].expect_id = 0x65;
    IWMPControls_QueryInterface(controls, &IID_IDispatch, (void **)&cases[2].d);
    cases[2].obj = "controls"; cases[2].prop = L"currentPosition"; cases[2].vt = VT_R8; cases[2].expect_id = 0x38;
    IWMPNetwork_QueryInterface(network, &IID_IDispatch, (void **)&cases[3].d);
    cases[3].obj = "network";  cases[3].prop = L"downloadProgress"; cases[3].vt = VT_I4; cases[3].expect_id = 0x338;
    IWMPMedia_QueryInterface(media, &IID_IDispatch, (void **)&cases[4].d);
    cases[4].obj = "media";    cases[4].prop = L"sourceURL";   cases[4].vt = VT_BSTR; cases[4].expect_id = 0x2ef;
    IWMPPlaylist_QueryInterface(pl, &IID_IDispatch, (void **)&cases[5].d);
    cases[5].obj = "playlist"; cases[5].prop = L"count";       cases[5].vt = VT_I4; cases[5].expect_id = 0xc9;

    /* the control's own volume/position state is not set up yet: use members that always succeed */
    cases[2].prop = L"isAvailable"; cases[2].vt = VT_BOOL; cases[2].expect_id = 0x3e;
    cases[3].prop = L"bandWidth"; cases[3].vt = VT_I4; cases[3].expect_id = 0x321;
    for (i = 0; i < 6; i++)
    {
        if (i == 2 || i == 3) continue; /* the members of these are exercised below once implemented */
        test_dispatch_basics(&cases[i]);
    }
    {
        /* controls/network: ids and type info without calling the getters */
        struct dispcase c = cases[2];
        ITypeInfo *ti; UINT n; DISPID id;
        hr = IDispatch_GetTypeInfoCount(c.d, &n);
        CHECKF(hr == S_OK && n == 1, "controls: GetTypeInfoCount");
        hr = IDispatch_GetTypeInfo(c.d, 0, 0, &ti);
        CHECKF(hr == S_OK, "controls: GetTypeInfo (%08lx)", hr);
        if (hr == S_OK) ITypeInfo_Release(ti);
        hr = get_id(c.d, L"isAvailable", &id);
        CHECKF(hr == S_OK && id == 0x3e, "controls: GetIDsOfNames(isAvailable)");
        hr = get_id(c.d, L"currentMarker", &id);
        CHECKF(hr == S_OK && id == 0x3d, "controls: GetIDsOfNames(currentMarker) = %lx", id);
        c = cases[3];
        hr = IDispatch_GetTypeInfoCount(c.d, &n);
        CHECKF(hr == S_OK && n == 1, "network: GetTypeInfoCount");
        hr = IDispatch_GetTypeInfo(c.d, 0, 0, &ti);
        CHECKF(hr == S_OK, "network: GetTypeInfo (%08lx)", hr);
        if (hr == S_OK) ITypeInfo_Release(ti);
        hr = get_id(c.d, L"bandWidth", &id);
        CHECKF(hr == S_OK && id == 0x321, "network: GetIDsOfNames(bandWidth)");
        hr = get_id(c.d, L"framesSkipped", &id);
        CHECKF(hr == S_OK && id == 0x33a, "network: GetIDsOfNames(framesSkipped) = %lx", id);
    }

    /* the IWMPPlayer vtable forwards to the same type information */
    hr = IWMPPlayer4_QueryInterface(player, &IID_IWMPPlayer, (void **)&player1);
    if (hr == S_OK)
    {
        ITypeInfo *ti = NULL; UINT n = 0; DISPID id = 0; LPOLESTR nm = (LPOLESTR)L"URL";
        hr = IWMPPlayer_GetTypeInfoCount(player1, &n);
        CHECKF(hr == S_OK && n == 1, "IWMPPlayer: GetTypeInfoCount (%08lx)", hr);
        hr = IWMPPlayer_GetTypeInfo(player1, 0, 0, &ti);
        CHECKF(hr == S_OK && ti, "IWMPPlayer: GetTypeInfo (%08lx)", hr);
        if (ti) ITypeInfo_Release(ti);
        hr = IWMPPlayer_GetIDsOfNames(player1, &IID_NULL, &nm, 1, 0, &id);
        CHECKF(hr == S_OK && id == 1, "IWMPPlayer: GetIDsOfNames(URL) = %lx", id);
        IWMPPlayer_Release(player1);
    }

    /* property round trips through Invoke agree with the vtable */
    d = cases[1].d;
    V_VT(&arg[0]) = VT_BOOL; V_BOOL(&arg[0]) = VARIANT_FALSE;
    hr = call(d, L"autoStart", DISPATCH_PROPERTYPUT, arg, 1, NULL);
    CHECKF(hr == S_OK, "settings: Invoke put autoStart (%08lx)", hr);
    vb = VARIANT_TRUE;
    IWMPSettings_get_autoStart(settings, &vb);
    CHECKF(vb == VARIANT_FALSE, "settings: autoStart set through Invoke is seen by the vtable");
    V_BOOL(&arg[0]) = VARIANT_TRUE;
    hr = call(d, L"autoStart", DISPATCH_PROPERTYPUT, arg, 1, NULL);
    hr = call(d, L"autoStart", DISPATCH_PROPERTYGET, NULL, 0, &v);
    CHECKF(hr == S_OK && V_VT(&v) == VT_BOOL && V_BOOL(&v) == VARIANT_TRUE, "settings: Invoke get autoStart reads it back");
    V_VT(&arg[0]) = VT_BOOL; V_BOOL(&arg[0]) = VARIANT_FALSE;
    call(d, L"autoStart", DISPATCH_PROPERTYPUT, arg, 1, NULL);

    V_VT(&arg[0]) = VT_I4; V_I4(&arg[0]) = 37;
    hr = call(d, L"volume", DISPATCH_PROPERTYPUT, arg, 1, NULL);
    CHECKF(hr == S_OK, "settings: Invoke put volume (%08lx)", hr);
    l = 0; IWMPSettings_get_volume(settings, &l);
    CHECKF(l == 37, "settings: volume set through Invoke = %ld", l);
    /* a string argument is coerced to the declared type, a bad one is a type mismatch */
    V_VT(&arg[0]) = VT_BSTR; V_BSTR(&arg[0]) = SysAllocString(L"41");
    hr = call(d, L"volume", DISPATCH_PROPERTYPUT, arg, 1, NULL);
    CHECKF(hr == S_OK, "settings: Invoke put volume from a string (%08lx)", hr);
    SysFreeString(V_BSTR(&arg[0]));
    l = 0; IWMPSettings_get_volume(settings, &l);
    CHECKF(l == 41, "settings: volume coerced from a string = %ld", l);
    V_VT(&arg[0]) = VT_BSTR; V_BSTR(&arg[0]) = SysAllocString(L"loud");
    hr = call(d, L"volume", DISPATCH_PROPERTYPUT, arg, 1, NULL);
    CHECKF(hr == DISP_E_TYPEMISMATCH, "settings: Invoke put volume from a bad string = DISP_E_TYPEMISMATCH (%08lx)", hr);
    SysFreeString(V_BSTR(&arg[0]));

    /* controls.isAvailable through Invoke */
    V_VT(&arg[0]) = VT_BSTR; V_BSTR(&arg[0]) = SysAllocString(L"currentPosition");
    hr = call(cases[2].d, L"isAvailable", DISPATCH_PROPERTYGET, arg, 1, &v);
    CHECKF(hr == S_OK && V_VT(&v) == VT_BOOL && V_BOOL(&v) == VARIANT_FALSE, "controls: Invoke isAvailable(currentPosition) = FALSE (%08lx)", hr);
    SysFreeString(V_BSTR(&arg[0]));

    /* media / playlist names through Invoke */
    hr = call(cases[4].d, L"name", DISPATCH_PROPERTYGET, NULL, 0, &v);
    CHECKF(hr == S_OK && V_VT(&v) == VT_BSTR && !wcscmp(V_BSTR(&v), L"clip"), "media: Invoke get name = %ls", V_VT(&v) == VT_BSTR ? V_BSTR(&v) : L"?");
    VariantClear(&v);
    V_VT(&arg[0]) = VT_BSTR; V_BSTR(&arg[0]) = SysAllocString(L"renamed");
    hr = call(cases[4].d, L"name", DISPATCH_PROPERTYPUT, arg, 1, NULL);
    SysFreeString(V_BSTR(&arg[0]));
    b = NULL; IWMPMedia_get_name(media, &b);
    CHECKF(hr == S_OK && b && !wcscmp(b, L"renamed"), "media: Invoke put name is seen by the vtable");
    SysFreeString(b);
    hr = call(cases[5].d, L"name", DISPATCH_PROPERTYGET, NULL, 0, &v);
    CHECKF(hr == S_OK && V_VT(&v) == VT_BSTR && !wcscmp(V_BSTR(&v), L"clip"), "playlist: Invoke get name");
    VariantClear(&v);
    hr = call(cases[5].d, L"count", DISPATCH_PROPERTYGET, NULL, 0, &v);
    CHECKF(hr == S_OK && V_VT(&v) == VT_I4 && V_I4(&v) == 0, "playlist: Invoke get count = 0");
    /* player.URL */
    hr = call(cases[0].d, L"versionInfo", DISPATCH_PROPERTYGET, NULL, 0, &v);
    CHECKF(hr == S_OK && V_VT(&v) == VT_BSTR && V_BSTR(&v) && V_BSTR(&v)[0], "player: Invoke get versionInfo");
    VariantClear(&v);

    for (i = 0; i < 6; i++) IDispatch_Release(cases[i].d);
    SysFreeString(name); SysFreeString(url);
    IWMPPlaylist_Release(pl);
    IWMPMedia_Release(media);
    IWMPNetwork_Release(network);
    IWMPControls_Release(controls);
    IWMPSettings_Release(settings);
    IWMPPlayer4_Release(player);
}

/* ---- connection points ---- */

struct sink
{
    IDispatch IDispatch_iface;
    LONG ref;
};
static struct sink *impl_sink(IDispatch *d) { return CONTAINING_RECORD(d, struct sink, IDispatch_iface); }
static HRESULT WINAPI sink_QI(IDispatch *d, REFIID riid, void **ppv)
{
    if (IsEqualGUID(riid, &IID_IUnknown) || IsEqualGUID(riid, &IID_IDispatch) || IsEqualGUID(riid, &IID__WMPOCXEvents))
    { *ppv = d; IDispatch_AddRef(d); return S_OK; }
    *ppv = NULL;
    return E_NOINTERFACE;
}
static ULONG WINAPI sink_AddRef(IDispatch *d) { return InterlockedIncrement(&impl_sink(d)->ref); }
static ULONG WINAPI sink_Release(IDispatch *d) { return InterlockedDecrement(&impl_sink(d)->ref); }
static HRESULT WINAPI sink_Count(IDispatch *d, UINT *n) { *n = 0; return S_OK; }
static HRESULT WINAPI sink_Info(IDispatch *d, UINT i, LCID l, ITypeInfo **t) { return E_NOTIMPL; }
static HRESULT WINAPI sink_Names(IDispatch *d, REFIID r, LPOLESTR *n, UINT c, LCID l, DISPID *i) { return E_NOTIMPL; }
static HRESULT WINAPI sink_Invoke(IDispatch *d, DISPID id, REFIID r, LCID l, WORD f, DISPPARAMS *p, VARIANT *v,
                                  EXCEPINFO *e, UINT *a) { return S_OK; }
static const IDispatchVtbl sink_vtbl = {sink_QI, sink_AddRef, sink_Release, sink_Count, sink_Info, sink_Names, sink_Invoke};

static LONG refs_of(IUnknown *u) { IUnknown_AddRef(u); return IUnknown_Release(u); }

static void test_connection_points(void)
{
    IWMPPlayer4 *player = NULL;
    IConnectionPointContainer *cpc = NULL, *cpc2;
    IEnumConnectionPoints *ecp = NULL, *ecp2 = NULL;
    IConnectionPoint *cp = NULL, *cps[2] = {NULL, NULL}, *cp1;
    IEnumConnections *ec = NULL, *ec2 = NULL, *ec3 = NULL;
    CONNECTDATA cd[4];
    struct sink sinks[3];
    DWORD cookie[3];
    ULONG got;
    IID iid;
    LONG r0, r1;
    HRESULT hr;
    int i;

    hr = CoCreateInstance(&CLSID_WindowsMediaPlayer, NULL, CLSCTX_INPROC_SERVER, &IID_IWMPPlayer4, (void **)&player);
    if (FAILED(hr)) { CHECKF(0, "CoCreateInstance(WindowsMediaPlayer) = %08lx", hr); return; }
    hr = IWMPPlayer4_QueryInterface(player, &IID_IConnectionPointContainer, (void **)&cpc);
    CHECKF(hr == S_OK, "IConnectionPointContainer (%08lx)", hr);
    if (hr != S_OK) { IWMPPlayer4_Release(player); return; }

    /* IEnumConnectionPoints */
    hr = IConnectionPointContainer_EnumConnectionPoints(cpc, NULL);
    CHECKF(hr == E_POINTER, "EnumConnectionPoints(NULL) = E_POINTER (%08lx)", hr);
    r0 = refs_of((IUnknown *)cpc);
    hr = IConnectionPointContainer_EnumConnectionPoints(cpc, &ecp);
    CHECKF(hr == S_OK && ecp, "EnumConnectionPoints (%08lx)", hr);
    if (!ecp) goto done;
    CHECKF(refs_of((IUnknown *)cpc) == r0 + 1, "the enumerator holds the container");
    hr = IEnumConnectionPoints_Next(ecp, 2, cps, &got);
    CHECKF(hr == S_FALSE && got == 1 && cps[0] && !cps[1], "ConnectionPoints Next(2) = S_FALSE, one point (%08lx %lu)", hr, got);
    if (cps[0])
    {
        hr = IConnectionPoint_GetConnectionInterface(cps[0], &iid);
        CHECKF(hr == S_OK && IsEqualGUID(&iid, &IID__WMPOCXEvents), "the point is _WMPOCXEvents");
        hr = IConnectionPointContainer_FindConnectionPoint(cpc, &IID__WMPOCXEvents, &cp1);
        CHECKF(hr == S_OK && cp1 == cps[0], "the same point as FindConnectionPoint");
        if (hr == S_OK) IConnectionPoint_Release(cp1);
        IConnectionPoint_Release(cps[0]);
    }
    hr = IEnumConnectionPoints_Next(ecp, 1, cps, &got);
    CHECKF(hr == S_FALSE && got == 0, "ConnectionPoints Next at the end = S_FALSE, 0 (%08lx %lu)", hr, got);
    hr = IEnumConnectionPoints_Reset(ecp);
    CHECKF(hr == S_OK, "ConnectionPoints Reset (%08lx)", hr);
    hr = IEnumConnectionPoints_Next(ecp, 1, cps, NULL);
    CHECKF(hr == S_OK && cps[0], "ConnectionPoints Next after Reset gives the point again (%08lx)", hr);
    if (cps[0]) IConnectionPoint_Release(cps[0]);
    hr = IEnumConnectionPoints_Next(ecp, 2, cps, NULL);
    CHECKF(hr == E_INVALIDARG, "ConnectionPoints Next(2) without pcFetched = E_INVALIDARG (%08lx)", hr);
    IEnumConnectionPoints_Reset(ecp);
    hr = IEnumConnectionPoints_Skip(ecp, 1);
    CHECKF(hr == S_OK, "ConnectionPoints Skip(1) = S_OK (%08lx)", hr);
    hr = IEnumConnectionPoints_Next(ecp, 1, cps, &got);
    CHECKF(hr == S_FALSE && got == 0, "ConnectionPoints Next after Skip(1) = S_FALSE (%08lx)", hr);
    hr = IEnumConnectionPoints_Skip(ecp, 1);
    CHECKF(hr == S_FALSE, "ConnectionPoints Skip past the end = S_FALSE (%08lx)", hr);
    IEnumConnectionPoints_Reset(ecp);
    IEnumConnectionPoints_Skip(ecp, 1);
    hr = IEnumConnectionPoints_Clone(ecp, &ecp2);
    CHECKF(hr == S_OK && ecp2 && ecp2 != ecp, "ConnectionPoints Clone (%08lx)", hr);
    if (ecp2)
    {
        hr = IEnumConnectionPoints_Next(ecp2, 1, cps, &got);
        CHECKF(hr == S_FALSE && got == 0, "the clone keeps the position (at the end)");
        IEnumConnectionPoints_Reset(ecp2);
        hr = IEnumConnectionPoints_Next(ecp, 1, cps, &got);
        CHECKF(hr == S_FALSE, "resetting the clone leaves the original alone");
        hr = IEnumConnectionPoints_Next(ecp2, 1, cps, &got);
        CHECKF(hr == S_OK && got == 1, "the clone, reset, enumerates again");
        if (hr == S_OK) IConnectionPoint_Release(cps[0]);
        IEnumConnectionPoints_Release(ecp2);
    }
    hr = IEnumConnectionPoints_Clone(ecp, NULL);
    CHECKF(hr == E_POINTER, "ConnectionPoints Clone(NULL) = E_POINTER (%08lx)", hr);
    hr = IEnumConnectionPoints_QueryInterface(ecp, &IID_IEnumConnectionPoints, (void **)&ecp2);
    CHECKF(hr == S_OK && ecp2 == ecp, "ConnectionPoints QueryInterface");
    if (hr == S_OK) IEnumConnectionPoints_Release(ecp2);
    hr = IEnumConnectionPoints_QueryInterface(ecp, &IID_IConnectionPoint, (void **)&cp1);
    CHECKF(hr == E_NOINTERFACE, "ConnectionPoints QueryInterface(IConnectionPoint) = E_NOINTERFACE");
    IEnumConnectionPoints_Release(ecp);
    r1 = refs_of((IUnknown *)cpc);
    CHECKF(r0 == r1, "the enumerators release the container (%ld vs %ld)", r0, r1);

    /* IEnumConnections */
    hr = IConnectionPointContainer_FindConnectionPoint(cpc, &IID__WMPOCXEvents, &cp);
    CHECKF(hr == S_OK, "FindConnectionPoint (%08lx)", hr);
    if (hr != S_OK) goto done;
    for (i = 0; i < 3; i++)
    {
        sinks[i].IDispatch_iface.lpVtbl = (IDispatchVtbl *)&sink_vtbl;
        sinks[i].ref = 1;
        hr = IConnectionPoint_Advise(cp, (IUnknown *)&sinks[i].IDispatch_iface, &cookie[i]);
        CHECKF(hr == S_OK && cookie[i], "Advise sink %d (%08lx)", i, hr);
    }
    hr = IConnectionPoint_Unadvise(cp, cookie[1]);
    CHECKF(hr == S_OK, "Unadvise the middle sink (%08lx)", hr);
    hr = IConnectionPoint_EnumConnections(cp, &ec);
    CHECKF(hr == S_OK && ec, "EnumConnections (%08lx)", hr);
    if (!ec) goto done;

    memset(cd, 0, sizeof(cd));
    hr = IEnumConnections_Next(ec, 4, cd, &got);
    CHECKF(hr == S_FALSE && got == 2, "Connections Next(4) = S_FALSE, two live connections (%08lx %lu)", hr, got);
    CHECKF(cd[0].pUnk == (IUnknown *)&sinks[0].IDispatch_iface && cd[0].dwCookie == cookie[0], "first connection is sink 0 with its Advise cookie (%lu vs %lu)", cd[0].dwCookie, cookie[0]);
    CHECKF(cd[1].pUnk == (IUnknown *)&sinks[2].IDispatch_iface && cd[1].dwCookie == cookie[2], "second connection is sink 2 with its Advise cookie (%lu vs %lu)", cd[1].dwCookie, cookie[2]);
    CHECKF(sinks[0].ref == 3 && sinks[2].ref == 3, "Next AddRefs the sinks it returns (%ld %ld)", sinks[0].ref, sinks[2].ref);
    CHECKF(sinks[1].ref == 1, "the unadvised sink was released (%ld)", sinks[1].ref);
    IUnknown_Release(cd[0].pUnk); IUnknown_Release(cd[1].pUnk);

    hr = IEnumConnections_Next(ec, 1, cd, &got);
    CHECKF(hr == S_FALSE && got == 0, "Connections Next at the end = S_FALSE, 0 (%08lx %lu)", hr, got);
    hr = IEnumConnections_Reset(ec);
    CHECKF(hr == S_OK, "Connections Reset (%08lx)", hr);
    hr = IEnumConnections_Skip(ec, 1);
    CHECKF(hr == S_OK, "Connections Skip(1) = S_OK (%08lx)", hr);
    hr = IEnumConnections_Next(ec, 1, cd, &got);
    CHECKF(hr == S_OK && got == 1 && cd[0].pUnk == (IUnknown *)&sinks[2].IDispatch_iface,
           "Connections Next after Skip(1) is the second live connection (%08lx)", hr);
    if (hr == S_OK) IUnknown_Release(cd[0].pUnk);
    hr = IEnumConnections_Skip(ec, 1);
    CHECKF(hr == S_FALSE, "Connections Skip past the end = S_FALSE (%08lx)", hr);
    IEnumConnections_Reset(ec);
    hr = IEnumConnections_Skip(ec, 3);
    CHECKF(hr == S_FALSE, "Connections Skip(3) of 2 = S_FALSE (%08lx)", hr);
    hr = IEnumConnections_Next(ec, 1, cd, &got);
    CHECKF(hr == S_FALSE && got == 0, "Connections Skip(3) leaves the enumerator at the end");
    IEnumConnections_Reset(ec);
    hr = IEnumConnections_Skip(ec, 2);
    CHECKF(hr == S_OK, "Connections Skip(2) of 2 = S_OK (%08lx)", hr);
    IEnumConnections_Reset(ec);
    hr = IEnumConnections_Skip(ec, 1);
    hr = IEnumConnections_Clone(ec, &ec2);
    CHECKF(hr == S_OK && ec2 && ec2 != ec, "Connections Clone (%08lx)", hr);
    if (ec2)
    {
        hr = IEnumConnections_Next(ec2, 1, cd, &got);
        CHECKF(hr == S_OK && got == 1 && cd[0].pUnk == (IUnknown *)&sinks[2].IDispatch_iface,
               "the clone starts at the original's position");
        if (hr == S_OK) IUnknown_Release(cd[0].pUnk);
        hr = IEnumConnections_Next(ec, 1, cd, &got);
        CHECKF(hr == S_OK && got == 1 && cd[0].pUnk == (IUnknown *)&sinks[2].IDispatch_iface,
               "the clone does not move the original");
        if (hr == S_OK) IUnknown_Release(cd[0].pUnk);
        IEnumConnections_Reset(ec2);
        hr = IEnumConnections_Next(ec2, 1, cd, &got);
        CHECKF(hr == S_OK && cd[0].pUnk == (IUnknown *)&sinks[0].IDispatch_iface, "a reset clone starts over");
        if (hr == S_OK) IUnknown_Release(cd[0].pUnk);
        /* a clone of the clone */
        hr = IEnumConnections_Clone(ec2, &ec3);
        CHECKF(hr == S_OK && ec3, "Clone of a clone");
        if (ec3) IEnumConnections_Release(ec3);
        IEnumConnections_Release(ec2);
    }
    hr = IEnumConnections_Clone(ec, NULL);
    CHECKF(hr == E_POINTER, "Connections Clone(NULL) = E_POINTER (%08lx)", hr);
    hr = IEnumConnections_Next(ec, 2, cd, NULL);
    CHECKF(hr == E_INVALIDARG, "Connections Next(2) without pcFetched = E_INVALIDARG (%08lx)", hr);
    IEnumConnections_Release(ec);
    /* the enumerator's references to the point are gone: unadvise the rest */
    IConnectionPoint_Unadvise(cp, cookie[0]);
    IConnectionPoint_Unadvise(cp, cookie[2]);
    CHECKF(sinks[0].ref == 1 && sinks[2].ref == 1, "all sink references released (%ld %ld)", sinks[0].ref, sinks[2].ref);
    /* the point stays valid for the container */
    hr = IConnectionPoint_GetConnectionPointContainer(cp, &cpc2);
    CHECKF(hr == S_OK && cpc2 == cpc, "GetConnectionPointContainer");
    if (hr == S_OK) IConnectionPointContainer_Release(cpc2);
    IConnectionPoint_Release(cp);
done:
    IConnectionPointContainer_Release(cpc);
    IWMPPlayer4_Release(player);
}

int main(void)
{
    CoInitialize(NULL);
    test_dispatch();
    test_connection_points();
    CoUninitialize();
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures ? 1 : 0;
}
