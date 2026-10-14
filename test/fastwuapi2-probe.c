/* fastwuapi2-probe: wuapi asynchronous search, the installer and the downloader without updates, the web proxy of a
 * session, ShowSettingsDialog. Late binding through IDispatch (mingw has no wuapi.h). */
#define COBJMACROS
#include <windows.h>
#include <ole2.h>
#include <oleauto.h>
#include <stdio.h>
#include <string.h>

static int checks, fails;
#define CHECK(cond, ...) do { checks++; if (!(cond)) { fails++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static const GUID CLSID_UpdateSession = {0x4cb43d7f, 0x7eee, 0x4906, {0x86, 0x98, 0x60, 0xda, 0x1c, 0x38, 0xf2, 0xfe}};
static const GUID CLSID_AutomaticUpdates = {0xbfe18e9c, 0x6d87, 0x4450, {0xb3, 0x7c, 0xe0, 0x2f, 0x0b, 0x37, 0x38, 0x03}};
static const GUID CLSID_UpdateInstaller = {0xd2e0fe7f, 0xd23e, 0x48e1, {0x93, 0xc0, 0x6f, 0xa8, 0xcc, 0x34, 0x64, 0x74}};
static const GUID IID_ISearchCompletedCallback_ = {0x88aee058, 0xd4b0, 0x4725, {0xa2, 0xf1, 0x81, 0x4a, 0x67, 0xae, 0x96, 0x4c}};

#define WU_E_NO_UPDATE         ((HRESULT)0x80240024)
#define WU_E_INVALID_CRITERIA  ((HRESULT)0x80240032)
#define WU_E_INVALID_OPERATION ((HRESULT)0x80240036)

/* late binding */
static HRESULT call(IDispatch *d, const WCHAR *name, WORD flags, VARIANT *args, int nargs, VARIANT *res)
{
    DISPID id, put = DISPID_PROPERTYPUT;
    DISPPARAMS dp;
    EXCEPINFO ei;
    UINT err = 0;
    HRESULT hr;
    OLECHAR *n = (OLECHAR *)name;
    VARIANT local;

    hr = IDispatch_GetIDsOfNames(d, &IID_NULL, &n, 1, LOCALE_USER_DEFAULT, &id);
    if (FAILED(hr)) return hr;
    dp.rgvarg = args;
    dp.cArgs = nargs;
    dp.cNamedArgs = (flags & DISPATCH_PROPERTYPUT) ? 1 : 0;
    dp.rgdispidNamedArgs = (flags & DISPATCH_PROPERTYPUT) ? &put : NULL;
    memset(&ei, 0, sizeof(ei));
    if (!res) { res = &local; VariantInit(&local); }
    hr = IDispatch_Invoke(d, id, &IID_NULL, LOCALE_USER_DEFAULT, flags, &dp, res, &ei, &err);
    if (hr == DISP_E_EXCEPTION) hr = ei.scode;
    return hr;
}

static VARIANT vbstr(const WCHAR *s) { VARIANT v; VariantInit(&v); V_VT(&v) = VT_BSTR; V_BSTR(&v) = SysAllocString(s); return v; }
static VARIANT vbool(BOOL b) { VARIANT v; VariantInit(&v); V_VT(&v) = VT_BOOL; V_BOOL(&v) = b ? VARIANT_TRUE : VARIANT_FALSE; return v; }
static VARIANT vdisp(IDispatch *d) { VARIANT v; VariantInit(&v); V_VT(&v) = VT_DISPATCH; V_DISPATCH(&v) = d; return v; }
static VARIANT vnull(void) { VARIANT v; VariantInit(&v); V_VT(&v) = VT_UNKNOWN; V_UNKNOWN(&v) = NULL; return v; }
static VARIANT vi4(LONG l) { VARIANT v; VariantInit(&v); V_VT(&v) = VT_I4; V_I4(&v) = l; return v; }

static IDispatch *get_obj(IDispatch *d, const WCHAR *name)
{
    VARIANT res;
    HRESULT hr;
    IDispatch *ret = NULL;

    VariantInit(&res);
    hr = call(d, name, DISPATCH_PROPERTYGET, NULL, 0, &res);
    if (hr == S_OK && V_VT(&res) == VT_DISPATCH) { ret = V_DISPATCH(&res); IDispatch_AddRef(ret); }
    VariantClear(&res);
    return ret;
}

/* a callback object: a script function (dispatch identifier 0), or the interface of C++ */
typedef struct cbvt
{
    HRESULT (WINAPI *QueryInterface)(void *, REFIID, void **);
    ULONG (WINAPI *AddRef)(void *);
    ULONG (WINAPI *Release)(void *);
    HRESULT (WINAPI *Invoke)(void *, IDispatch *, IDispatch *);
} cbvt;

struct cb
{
    IDispatch disp;
    const cbvt *like;
    LONG ref;
    BOOL as_interface, any;
    volatile LONG calls;
    DWORD thread;
    IDispatch *job, *args;
    int nargs;
    HANDLE event;
    DWORD delay;
};

static struct cb *cb_of_disp(IDispatch *d) { return (struct cb *)d; }
static struct cb *cb_of_like(void *l) { return CONTAINING_RECORD(l, struct cb, like); }

static void cb_record(struct cb *c, IDispatch *job, IDispatch *args, int nargs)
{
    c->thread = GetCurrentThreadId();
    c->nargs = nargs;
    if (c->delay) Sleep(c->delay);
    if (job) { IDispatch_AddRef(job); c->job = job; }
    if (args) { IDispatch_AddRef(args); c->args = args; }
    InterlockedIncrement(&c->calls);
    SetEvent(c->event);
}

static HRESULT WINAPI cb_QI(IDispatch *d, REFIID riid, void **o)
{
    struct cb *c = cb_of_disp(d);

    if (IsEqualIID(riid, &IID_IUnknown) || (IsEqualIID(riid, &IID_IDispatch) && !c->as_interface) ||
        (c->any && !IsEqualIID(riid, &IID_ISearchCompletedCallback_))) *o = d;
    else if (IsEqualIID(riid, &IID_ISearchCompletedCallback_) && c->as_interface) *o = &c->like;
    else { *o = NULL; return E_NOINTERFACE; }
    InterlockedIncrement(&c->ref);
    return S_OK;
}
static ULONG WINAPI cb_AddRef(IDispatch *d) { return InterlockedIncrement(&cb_of_disp(d)->ref); }
static ULONG WINAPI cb_Release(IDispatch *d) { return InterlockedDecrement(&cb_of_disp(d)->ref); }
static HRESULT WINAPI cb_TIC(IDispatch *d, UINT *n) { *n = 0; return S_OK; }
static HRESULT WINAPI cb_TI(IDispatch *d, UINT i, LCID l, ITypeInfo **t) { return E_NOTIMPL; }
static HRESULT WINAPI cb_GIN(IDispatch *d, REFIID r, LPOLESTR *n, UINT c, LCID l, DISPID *i) { return DISP_E_UNKNOWNNAME; }
static HRESULT WINAPI cb_Invoke(IDispatch *d, DISPID id, REFIID r, LCID l, WORD f, DISPPARAMS *p, VARIANT *res, EXCEPINFO *e, UINT *a)
{
    if (id != DISPID_VALUE) return DISP_E_MEMBERNOTFOUND;
    cb_record(cb_of_disp(d), p->cArgs > 1 && V_VT(&p->rgvarg[1]) == VT_DISPATCH ? V_DISPATCH(&p->rgvarg[1]) : NULL,
              p->cArgs > 0 && V_VT(&p->rgvarg[0]) == VT_DISPATCH ? V_DISPATCH(&p->rgvarg[0]) : NULL, p->cArgs);
    return S_OK;
}
static const IDispatchVtbl cb_vtbl = { cb_QI, cb_AddRef, cb_Release, cb_TIC, cb_TI, cb_GIN, cb_Invoke };

static HRESULT WINAPI like_QI(void *l, REFIID r, void **o) { return cb_QI(&cb_of_like(l)->disp, r, o); }
static ULONG WINAPI like_AddRef(void *l) { return InterlockedIncrement(&cb_of_like(l)->ref); }
static ULONG WINAPI like_Release(void *l) { return InterlockedDecrement(&cb_of_like(l)->ref); }
static HRESULT WINAPI like_Invoke(void *l, IDispatch *job, IDispatch *args) { cb_record(cb_of_like(l), job, args, 2); return S_OK; }
static const cbvt like_vtbl = { like_QI, like_AddRef, like_Release, like_Invoke };

static void cb_init(struct cb *c, BOOL as_interface)
{
    memset(c, 0, sizeof(*c));
    c->disp.lpVtbl = &cb_vtbl;
    c->like = &like_vtbl;
    c->ref = 1;
    c->as_interface = as_interface;
    c->event = CreateEventW(NULL, FALSE, FALSE, NULL);
}

static IDispatch *create(const GUID *clsid)
{
    IDispatch *d = NULL;
    HRESULT hr = CoCreateInstance(clsid, NULL, CLSCTX_INPROC_SERVER, &IID_IDispatch, (void **)&d);
    CHECK(hr == S_OK && d, "create %08lx", hr);
    return d;
}

static LONG count_of(IDispatch *coll)
{
    VARIANT v;
    LONG n = -1;

    VariantInit(&v);
    if (call(coll, L"Count", DISPATCH_PROPERTYGET, NULL, 0, &v) == S_OK) n = V_I4(&v);
    return n;
}

static void test_search(void)
{
    IDispatch *session = create(&CLSID_UpdateSession), *searcher, *job = NULL, *result, *updates;
    struct cb c;
    VARIANT args[3], res;
    HRESULT hr;
    int mode;

    if (!session) return;
    VariantInit(&res);
    hr = call(session, L"CreateUpdateSearcher", DISPATCH_METHOD, NULL, 0, &res);
    CHECK(hr == S_OK && V_VT(&res) == VT_DISPATCH, "searcher %08lx", hr);
    if (hr != S_OK) return;
    searcher = V_DISPATCH(&res);

    for (mode = 0; mode < 2; mode++)
    {
        cb_init(&c, mode);
        /* BeginSearch(criteria, onCompleted, state): arguments right to left */
        args[2] = vbstr(L"IsInstalled=0");
        args[1] = vdisp(&c.disp);
        args[0] = vi4(42);
        VariantInit(&res);
        hr = call(searcher, L"BeginSearch", DISPATCH_METHOD, args, 3, &res);
        CHECK(hr == S_OK && V_VT(&res) == VT_DISPATCH, "begin search %d %08lx", mode, hr);
        if (hr != S_OK) continue;
        job = V_DISPATCH(&res);
        {
            DWORD w = WaitForSingleObject(c.event, 10000);
            CHECK(w == WAIT_OBJECT_0, "the callback is called (%s)", mode ? "interface" : "script");
        }
        CHECK(c.calls == 1 && c.nargs == 2, "called once with two arguments: %ld %d", c.calls, c.nargs);
        CHECK(c.thread != GetCurrentThreadId(), "called on another thread");
        CHECK(c.args != NULL, "arguments object");
        if (c.job)
        {
            /* the job given to the callback is the job returned */
            IUnknown *u1 = NULL, *u2 = NULL;
            IDispatch_QueryInterface(c.job, &IID_IUnknown, (void **)&u1);
            IDispatch_QueryInterface(job, &IID_IUnknown, (void **)&u2);
            CHECK(u1 == u2, "the same job");
            if (u1) IUnknown_Release(u1);
            if (u2) IUnknown_Release(u2);
        }
        else CHECK(0, "no job in the callback");

        VariantInit(&res);
        hr = call(job, L"AsyncState", DISPATCH_PROPERTYGET, NULL, 0, &res);
        CHECK(hr == S_OK && V_VT(&res) == VT_I4 && V_I4(&res) == 42, "state %08lx", hr);
        hr = call(job, L"CleanUp", DISPATCH_METHOD, NULL, 0, NULL);
        CHECK(hr == S_OK, "clean up %08lx", hr);
        VariantInit(&res);
        hr = call(job, L"IsCompleted", DISPATCH_PROPERTYGET, NULL, 0, &res);
        CHECK(hr == S_OK && V_BOOL(&res) == VARIANT_TRUE, "completed");
        hr = call(job, L"RequestAbort", DISPATCH_METHOD, NULL, 0, NULL);
        CHECK(hr == S_OK, "request abort %08lx", hr);

        /* EndSearch(job) */
        args[0] = vdisp(job);
        VariantInit(&res);
        hr = call(searcher, L"EndSearch", DISPATCH_METHOD, args, 1, &res);
        CHECK(hr == S_OK && V_VT(&res) == VT_DISPATCH, "end search %08lx", hr);
        if (hr == S_OK)
        {
            result = V_DISPATCH(&res);
            VariantInit(&res);
            hr = call(result, L"ResultCode", DISPATCH_PROPERTYGET, NULL, 0, &res);
            CHECK(hr == S_OK && V_I4(&res) == 2, "succeeded: %ld", V_I4(&res));
            updates = get_obj(result, L"Updates");
            CHECK(updates && count_of(updates) == 0, "no updates");
            if (updates) IDispatch_Release(updates);
            IDispatch_Release(result);
        }
        VariantInit(&res);
        hr = call(searcher, L"EndSearch", DISPATCH_METHOD, args, 1, &res);
        CHECK(hr == WU_E_INVALID_OPERATION, "end search again %08lx", hr);
        IDispatch_Release(job);
        if (c.job) IDispatch_Release(c.job);
        if (c.args) IDispatch_Release(c.args);
    }

    /* CleanUp waits for the callback */
    cb_init(&c, FALSE);
    c.delay = 400;
    args[2] = vbstr(L"IsInstalled=0");
    args[1] = vdisp(&c.disp);
    args[0] = vi4(1);
    VariantInit(&res);
    hr = call(searcher, L"BeginSearch", DISPATCH_METHOD, args, 3, &res);
    if (hr == S_OK)
    {
        job = V_DISPATCH(&res);
        hr = call(job, L"CleanUp", DISPATCH_METHOD, NULL, 0, NULL);
        CHECK(hr == S_OK && c.calls == 1, "clean up waited for the callback: %ld", c.calls);
        IDispatch_Release(job);
    }

    /* no callback; a criteria that is not one */
    args[2] = vbstr(L"");
    args[1] = vnull();
    args[0] = vi4(0); V_VT(&args[0]) = VT_EMPTY;
    VariantInit(&res);
    hr = call(searcher, L"BeginSearch", DISPATCH_METHOD, args, 3, &res);
    CHECK(hr == S_OK && V_VT(&res) == VT_DISPATCH, "begin with an empty criteria %08lx", hr);
    if (hr == S_OK)
    {
        job = V_DISPATCH(&res);
        call(job, L"CleanUp", DISPATCH_METHOD, NULL, 0, NULL);
        args[0] = vdisp(job);
        VariantInit(&res);
        hr = call(searcher, L"EndSearch", DISPATCH_METHOD, args, 1, &res);
        CHECK(hr == WU_E_INVALID_CRITERIA, "invalid criteria %08lx", hr);
        IDispatch_Release(job);
    }

    /* not a job of the searcher */
    cb_init(&c, FALSE);
    c.any = TRUE;
    args[0] = vdisp(&c.disp);
    VariantInit(&res);
    hr = call(searcher, L"EndSearch", DISPATCH_METHOD, args, 1, &res);
    CHECK(hr == E_INVALIDARG, "a job that is not one %08lx", hr);
    IDispatch_Release(searcher);
    IDispatch_Release(session);
}

static void test_work(void)
{
    IDispatch *session = create(&CLSID_UpdateSession), *installer = create(&CLSID_UpdateInstaller), *downloader = NULL;
    IDispatch *updates, *coll;
    struct cb fake;
    VARIANT args[4], res;
    HRESULT hr;
    int i;
    struct { IDispatch *obj; const WCHAR *names[5]; const char *what; } kinds[2];

    if (!session || !installer) return;
    VariantInit(&res);
    hr = call(session, L"CreateUpdateDownloader", DISPATCH_METHOD, NULL, 0, &res);
    CHECK(hr == S_OK && V_VT(&res) == VT_DISPATCH, "downloader %08lx", hr);
    if (hr == S_OK) downloader = V_DISPATCH(&res);

    /* nothing to work on: the updates are not set, or an empty collection */
    for (i = 0; i < 2; i++)
    {
        if (i == 1)
        {
            VariantInit(&res);
            hr = call(session, L"CreateUpdateDownloader", DISPATCH_METHOD, NULL, 0, &res);
        }
        hr = call(installer, L"Install", DISPATCH_METHOD, NULL, 0, &res);
        CHECK(hr == WU_E_NO_UPDATE, "install %d %08lx", i, hr);
        hr = call(installer, L"Uninstall", DISPATCH_METHOD, NULL, 0, &res);
        CHECK(hr == WU_E_NO_UPDATE, "uninstall %d %08lx", i, hr);
        args[0] = vbstr(L"title");
        hr = call(installer, L"RunWizard", DISPATCH_METHOD, args, 1, &res);
        CHECK(hr == WU_E_NO_UPDATE, "run wizard %d %08lx", i, hr);
        args[2] = vnull();
        args[1] = vnull();
        args[0] = vi4(0); V_VT(&args[0]) = VT_EMPTY;
        hr = call(installer, L"BeginInstall", DISPATCH_METHOD, args, 3, &res);
        CHECK(hr == WU_E_NO_UPDATE, "begin install %d %08lx", i, hr);
        hr = call(installer, L"BeginUninstall", DISPATCH_METHOD, args, 3, &res);
        CHECK(hr == WU_E_NO_UPDATE, "begin uninstall %d %08lx", i, hr);
        if (downloader)
        {
            hr = call(downloader, L"Download", DISPATCH_METHOD, NULL, 0, &res);
            CHECK(hr == WU_E_NO_UPDATE, "download %d %08lx", i, hr);
            hr = call(downloader, L"BeginDownload", DISPATCH_METHOD, args, 3, &res);
            CHECK(hr == WU_E_NO_UPDATE, "begin download %d %08lx", i, hr);
        }
        if (i == 0)
        {
            /* an empty collection set as the updates */
            VariantInit(&res);
            hr = call(installer, L"Updates", DISPATCH_PROPERTYGET, NULL, 0, &res);
            CHECK(hr == S_OK && V_VT(&res) == VT_DISPATCH, "updates collection %08lx", hr);
            if (hr == S_OK)
            {
                coll = V_DISPATCH(&res);
                args[0] = vdisp(coll);
                hr = call(installer, L"Updates", DISPATCH_PROPERTYPUT, args, 1, NULL);
                CHECK(hr == S_OK, "put updates %08lx", hr);
                if (downloader) call(downloader, L"Updates", DISPATCH_PROPERTYPUT, args, 1, NULL);
            }
        }
    }

    /* updates that the agent cannot reach */
    memset(&fake, 0, sizeof(fake));
    cb_init(&fake, FALSE);
    fake.any = TRUE;
    updates = get_obj(installer, L"Updates");
    if (updates)
    {
        LONG idx = -1;
        args[0] = vdisp(&fake.disp);
        VariantInit(&res);
        hr = call(updates, L"Add", DISPATCH_METHOD, args, 1, &res);
        if (hr == S_OK && count_of(updates) == 1)
        {
            hr = call(installer, L"Install", DISPATCH_METHOD, NULL, 0, &res);
            CHECK(hr == WU_E_INVALID_OPERATION, "install with an update %08lx", hr);
            if (downloader)
            {
                IDispatch *dcoll = get_obj(downloader, L"Updates");
                if (dcoll)
                {
                    args[0] = vdisp(&fake.disp);
                    call(dcoll, L"Add", DISPATCH_METHOD, args, 1, &res);
                    hr = call(downloader, L"Download", DISPATCH_METHOD, NULL, 0, &res);
                    CHECK(hr == WU_E_INVALID_OPERATION, "download with an update %08lx", hr);
                    IDispatch_Release(dcoll);
                }
            }
        }
        else CHECK(0, "could not add a fake update %08lx", hr);
        (void)idx;
        IDispatch_Release(updates);
    }

    /* the end calls need a job from the begin calls */
    args[1] = vdisp(NULL); V_VT(&args[1]) = VT_EMPTY;
    args[0] = vdisp(&fake.disp);
    hr = call(installer, L"EndInstall", DISPATCH_METHOD, args, 1, &res);
    CHECK(hr == E_INVALIDARG, "end install of nothing %08lx", hr);
    hr = call(installer, L"EndUninstall", DISPATCH_METHOD, args, 1, &res);
    CHECK(hr == E_INVALIDARG, "end uninstall of nothing %08lx", hr);
    if (downloader)
    {
        hr = call(downloader, L"EndDownload", DISPATCH_METHOD, args, 1, &res);
        CHECK(hr == E_INVALIDARG, "end download of nothing %08lx", hr);
    }
    (void)kinds;
    IDispatch_Release(installer);
    if (downloader) IDispatch_Release(downloader);
    IDispatch_Release(session);
}

static void test_au(void)
{
    IDispatch *au = create(&CLSID_AutomaticUpdates);
    HRESULT hr;

    if (!au) return;
    hr = call(au, L"ShowSettingsDialog", DISPATCH_METHOD, NULL, 0, NULL);
    CHECK(hr == S_OK, "show settings dialog %08lx", hr);
    IDispatch_Release(au);
}

static HWND find_cred_dialog;
static BOOL CALLBACK find_dlg(HWND h, LPARAM lp)
{
    char title[100];

    GetWindowTextA(h, title, sizeof(title));
    if (!strcmp(title, "Proxy Login") && IsWindowVisible(h)) { *(HWND *)lp = h; return FALSE; }
    return TRUE;
}

static DWORD main_thread;
static DWORD WINAPI cancel_thread(void *arg)
{
    HWND dlg = NULL;
    int i;

    for (i = 0; i < 200 && !dlg; i++) { Sleep(50); EnumThreadWindows(main_thread, find_dlg, (LPARAM)&dlg); }
    if (!dlg) return 1;
    PostMessageW(dlg, WM_COMMAND, IDCANCEL, 0);
    return 0;
}

static void test_proxy(void)
{
    IDispatch *s1 = create(&CLSID_UpdateSession), *s2 = create(&CLSID_UpdateSession), *p1, *p1b, *p2, *list;
    VARIANT args[2], res;
    HRESULT hr;
    HANDLE th;

    if (!s1 || !s2) return;
    p1 = get_obj(s1, L"WebProxy");
    CHECK(p1 != NULL, "web proxy of a session");
    if (!p1) return;
    p1b = get_obj(s1, L"WebProxy");
    {
        IUnknown *u1 = NULL, *u2 = NULL;
        IDispatch_QueryInterface(p1, &IID_IUnknown, (void **)&u1);
        IDispatch_QueryInterface(p1b, &IID_IUnknown, (void **)&u2);
        CHECK(u1 == u2, "the session has one");
        if (u1) IUnknown_Release(u1);
        if (u2) IUnknown_Release(u2);
    }
    VariantInit(&res);
    hr = call(p1, L"AutoDetect", DISPATCH_PROPERTYGET, NULL, 0, &res);
    CHECK(hr == S_OK && V_BOOL(&res) == VARIANT_TRUE, "auto detect by default");
    VariantInit(&res);
    hr = call(p1, L"BypassProxyOnLocal", DISPATCH_PROPERTYGET, NULL, 0, &res);
    CHECK(hr == S_OK && V_BOOL(&res) == VARIANT_FALSE, "bypass local off");
    VariantInit(&res);
    hr = call(p1, L"ReadOnly", DISPATCH_PROPERTYGET, NULL, 0, &res);
    CHECK(hr == S_OK && V_BOOL(&res) == VARIANT_FALSE, "not read only");
    VariantInit(&res);
    hr = call(p1, L"Address", DISPATCH_PROPERTYGET, NULL, 0, &res);
    CHECK(hr == S_OK && (V_VT(&res) == VT_BSTR) && (!V_BSTR(&res) || !V_BSTR(&res)[0]), "no address");

    args[0] = vbstr(L"proxy.example.org:3128");
    hr = call(p1, L"Address", DISPATCH_PROPERTYPUT, args, 1, NULL);
    CHECK(hr == S_OK, "put address %08lx", hr);
    args[0] = vbstr(L"someone");
    call(p1, L"UserName", DISPATCH_PROPERTYPUT, args, 1, NULL);
    args[0] = vbool(TRUE);
    call(p1, L"BypassProxyOnLocal", DISPATCH_PROPERTYPUT, args, 1, NULL);
    args[0] = vbool(FALSE);
    call(p1, L"AutoDetect", DISPATCH_PROPERTYPUT, args, 1, NULL);
    args[0] = vbstr(L"secret");
    hr = call(p1, L"SetPassword", DISPATCH_METHOD, args, 1, NULL);
    CHECK(hr == S_OK, "set password %08lx", hr);
    VariantInit(&res);
    call(p1b, L"Address", DISPATCH_PROPERTYGET, NULL, 0, &res);
    CHECK(V_VT(&res) == VT_BSTR && V_BSTR(&res) && !lstrcmpW(V_BSTR(&res), L"proxy.example.org:3128"), "address read back");
    VariantClear(&res);
    call(p1b, L"UserName", DISPATCH_PROPERTYGET, NULL, 0, &res);
    CHECK(V_VT(&res) == VT_BSTR && V_BSTR(&res) && !lstrcmpW(V_BSTR(&res), L"someone"), "user read back");
    VariantClear(&res);
    call(p1b, L"BypassProxyOnLocal", DISPATCH_PROPERTYGET, NULL, 0, &res);
    CHECK(V_BOOL(&res) == VARIANT_TRUE, "bypass local read back");
    VariantInit(&res);
    call(p1b, L"AutoDetect", DISPATCH_PROPERTYGET, NULL, 0, &res);
    CHECK(V_BOOL(&res) == VARIANT_FALSE, "auto detect read back");

    list = get_obj(p1, L"BypassList");
    CHECK(list != NULL && count_of(list) == 0, "bypass list");
    if (list)
    {
        args[0] = vbstr(L"*.example.com");
        hr = call(list, L"Add", DISPATCH_METHOD, args, 1, NULL);
        CHECK(hr == S_OK, "add to the bypass list %08lx", hr);
        IDispatch_Release(list);
        list = get_obj(p1, L"BypassList");
        CHECK(list && count_of(list) == 1, "the list is the proxy's");
        if (list) IDispatch_Release(list);
    }

    /* the other session has its own; put replaces */
    p2 = get_obj(s2, L"WebProxy");
    VariantInit(&res);
    call(p2, L"Address", DISPATCH_PROPERTYGET, NULL, 0, &res);
    CHECK(V_VT(&res) == VT_BSTR && (!V_BSTR(&res) || !V_BSTR(&res)[0]), "the other session is not changed");
    args[0] = vdisp(p1);
    hr = call(s2, L"WebProxy", DISPATCH_PROPERTYPUT, args, 1, NULL);
    CHECK(hr == S_OK, "put web proxy %08lx", hr);
    {
        IDispatch *now = get_obj(s2, L"WebProxy");
        VariantInit(&res);
        if (now) call(now, L"Address", DISPATCH_PROPERTYGET, NULL, 0, &res);
        CHECK(now && V_VT(&res) == VT_BSTR && V_BSTR(&res) && !lstrcmpW(V_BSTR(&res), L"proxy.example.org:3128"),
              "the proxy that was put is returned");
        if (now) IDispatch_Release(now);
    }

    /* asking for the credentials: the dialog can be cancelled */
    main_thread = GetCurrentThreadId();
    th = CreateThread(NULL, 0, cancel_thread, NULL, 0, NULL);
    args[1] = vnull();
    args[0] = vbstr(L"Proxy Login");
    hr = call(p1, L"PromptForCredentials", DISPATCH_METHOD, args, 2, NULL);
    WaitForSingleObject(th, 12000);
    CloseHandle(th);
    CHECK(hr == HRESULT_FROM_WIN32(ERROR_CANCELLED), "cancelled credentials %08lx", hr);

    IDispatch_Release(p1);
    IDispatch_Release(p1b);
    IDispatch_Release(p2);
    IDispatch_Release(s1);
    IDispatch_Release(s2);
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    printf("-- test_au\n");
    test_au();
    printf("-- test_search\n");
    test_search();
    printf("-- test_work\n");
    test_work();
    printf("-- test_proxy\n");
    test_proxy();
    printf("checks %d fails %d\n", checks, fails);
    printf("RESULT: %s\n", fails ? "FAIL" : "PASS");
    return fails != 0;
}
