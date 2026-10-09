/* windows.media (patches/sg/2821), run by test/wmedia-stubs-gate.sh on Xvfb:
 * the ClosedCaptionProperties factory (the member stubs were done by patch
 * 1700): its IInspectable contract, that it is not agile (as the conformance
 * test, observed on Windows, expects), the statics interface, and that DllGetClassObject and
 * DllGetActivationFactory refuse what they do not implement.
 *
 *   wmedia-stubs-probe.exe */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <roapi.h>
#include <winstring.h>
#include <inspectable.h>
#include <activation.h>
#include <stdio.h>
#include <wchar.h>

DEFINE_GUID(IID_IClosedCaptionPropertiesStatics, 0x10aa1f84, 0xcc30, 0x4141, 0xb5, 0x03, 0x52, 0x72, 0x28, 0x9e, 0x0c, 0x20);
DEFINE_GUID(IID_Bogus, 0x12345678, 0x1234, 0x1234, 0x12, 0x34, 0x12, 0x34, 0x12, 0x34, 0x12, 0x34);

#undef IInspectable_Release
#undef IInspectable_QueryInterface
#define IInspectable_Release(p) IUnknown_Release((IUnknown *)(p))
#define IInspectable_QueryInterface(p, i, o) IUnknown_QueryInterface((IUnknown *)(p), i, (void **)(o))
typedef struct { INT64 value; } EventRegistrationToken;
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#define E_BOUNDS_ ((HRESULT)0x8000000B)
#define E_CHANGED_STATE_ ((HRESULT)0x8000000C)
static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}
#define CHECKF(ok, ...) do { char _b[256]; snprintf(_b, sizeof(_b), __VA_ARGS__); check(ok, _b); } while (0)

/* raw vtable calls: slot numbers count IUnknown (3) and IInspectable (3) */
#define VT(obj) (*(void ***)(obj))
typedef HRESULT (WINAPI *f_pp)(void *, void *);
typedef HRESULT (WINAPI *f_u)(void *, UINT32);
typedef HRESULT (WINAPI *f_up)(void *, UINT32, void *);
typedef HRESULT (WINAPI *f_upp)(void *, UINT32, void *, void *);
typedef HRESULT (WINAPI *f_b)(void *, BOOLEAN);
typedef HRESULT (WINAPI *f_v)(void *);
typedef HRESULT (WINAPI *f_ppp)(void *, void *, void *, void *);
typedef HRESULT (WINAPI *f_uupp)(void *, UINT32, UINT32, void *, void *);
typedef HRESULT (WINAPI *f_q)(void *, UINT64);
#define CALL_P(o, s, a) ((f_pp)VT(o)[s])(o, a)
#define CALL_V(o, s) ((f_v)VT(o)[s])(o)
#define CALL_U(o, s, a) ((f_u)VT(o)[s])(o, a)
#define CALL_UP(o, s, a, b) ((f_up)VT(o)[s])(o, a, b)
#define CALL_UPP(o, s, a, b, c) ((f_upp)VT(o)[s])(o, a, b, c)
#define CALL_B(o, s, a) ((f_b)VT(o)[s])(o, a)

static HSTRING mk(const WCHAR *s)
{
    HSTRING h = NULL;
    WindowsCreateString(s, wcslen(s), &h);
    return h;
}
static int hs_eq(HSTRING h, const WCHAR *s)
{
    const WCHAR *b = WindowsGetStringRawBuffer(h, NULL);
    return !wcscmp(b ? b : L"", s);
}

/* a minimal COM object, to hand out as an event handler or thumbnail */
static LONG obj_ref;
static HRESULT WINAPI o_qi(IUnknown *i, REFIID r, void **o) { *o = NULL; return E_NOINTERFACE; }
static ULONG WINAPI o_addref(IUnknown *i) { return InterlockedIncrement(&obj_ref); }
static ULONG WINAPI o_release(IUnknown *i) { return InterlockedDecrement(&obj_ref); }
static IUnknownVtbl o_vtbl = { o_qi, o_addref, o_release };
static IUnknown the_obj = { &o_vtbl };

#define MAXIIDS 4
struct inspectable_case
{
    const char *name;
    const WCHAR *class_name;
    const GUID *iids[MAXIIDS];
};

static int in_list(const GUID *g, const GUID *const *list)
{
    for (; *list; list++) if (IsEqualGUID(g, *list)) return 1;
    return 0;
}

static void check_inspectable(IInspectable *obj, const struct inspectable_case *c)
{
    ULONG count = 0, i, expected = 0;
    IID *iids = NULL;
    TrustLevel trust = 0x5555;
    HSTRING name = NULL;
    HRESULT hr;
    void *out;
    int all_known = 1, all_qi = 1;

    while (c->iids[expected]) expected++;

    hr = IInspectable_GetIids(obj, &count, &iids);
    CHECKF(hr == S_OK && count == expected, "%s: GetIids reports %lu interfaces (want %lu, hr %#lx)", c->name, count, expected, hr);
    if (hr == S_OK)
    {
        for (i = 0; i < count; i++)
        {
            IUnknown *unk = NULL;
            if (!in_list(&iids[i], c->iids)) all_known = 0;
            if (FAILED(IInspectable_QueryInterface(obj, &iids[i], (void **)&unk)) || !unk) all_qi = 0;
            else IUnknown_Release(unk);
        }
        CHECKF(all_known, "%s: every reported IID is an expected one", c->name);
        CHECKF(all_qi, "%s: every reported IID answers QueryInterface", c->name);
        CoTaskMemFree(iids);
    }
    hr = IInspectable_GetTrustLevel(obj, &trust);
    CHECKF(hr == S_OK && trust == BaseTrust, "%s: GetTrustLevel is BaseTrust (hr %#lx, trust %d)", c->name, hr, trust);
    hr = IInspectable_GetRuntimeClassName(obj, &name);
    CHECKF(hr == S_OK && name && hs_eq(name, c->class_name) && WindowsGetStringLen(name) == wcslen(c->class_name),
           "%s: GetRuntimeClassName is %ls (hr %#lx)", c->name, c->class_name, hr);
    WindowsDeleteString(name);
    out = (void *)0x1234;
    hr = IInspectable_QueryInterface(obj, &IID_Bogus, &out);
    CHECKF(hr == E_NOINTERFACE && !out, "%s: unknown IID gives E_NOINTERFACE and NULL (%#lx %p)", c->name, hr, out);
}


int main(void)
{
    HRESULT hr = RoInitialize(RO_INIT_MULTITHREADED);
    HSTRING cls = mk(L"Windows.Media.ClosedCaptioning.ClosedCaptionProperties");
    IActivationFactory *factory = NULL;
    struct inspectable_case fc = { "ClosedCaptionProperties factory", L"Windows.Media.ClosedCaptioning.ClosedCaptionProperties",
                                   { &IID_IClosedCaptionPropertiesStatics } };
    void *statics = NULL, *agile = NULL;
    IInspectable *inst = (IInspectable *)1;
    HMODULE dll;
    HRESULT (WINAPI *get_factory)(HSTRING, IActivationFactory **);
    HRESULT (WINAPI *get_class_object)(REFCLSID, REFIID, void **);
    void *obj = (void *)1;
    HSTRING h;
    IActivationFactory *f = (IActivationFactory *)1;

    check(SUCCEEDED(hr), "RoInitialize");
    hr = RoGetActivationFactory(cls, &IID_IActivationFactory, (void **)&factory);
    WindowsDeleteString(cls);
    if (FAILED(hr)) { check(0, "RoGetActivationFactory"); return 1; }

    check_inspectable((IInspectable *)factory, &fc);
    hr = IActivationFactory_ActivateInstance(factory, &inst);
    CHECKF(hr == E_NOTIMPL, "ActivateInstance is E_NOTIMPL (%#lx)", hr);
    hr = IActivationFactory_QueryInterface(factory, &IID_IAgileObject, &agile);
    CHECKF(hr == E_NOINTERFACE && !agile, "the factory is not agile (%#lx)", hr);
    if (agile) IUnknown_Release((IUnknown *)agile);
    agile = NULL;
    hr = IActivationFactory_QueryInterface(factory, &IID_IClosedCaptionPropertiesStatics, &statics);
    CHECKF(hr == S_OK && statics, "IClosedCaptionPropertiesStatics");
    if (statics)
    {
        hr = IInspectable_QueryInterface(statics, &IID_IAgileObject, &agile);
        CHECKF(hr == E_NOINTERFACE && !agile, "the statics interface is not agile either (%#lx)", hr);
        if (agile) IUnknown_Release((IUnknown *)agile);
        IUnknown_Release((IUnknown *)statics);
    }
    IActivationFactory_Release(factory);

    dll = LoadLibraryW(L"windows.media.dll");
    get_factory = dll ? (void *)GetProcAddress(dll, "DllGetActivationFactory") : NULL;
    get_class_object = dll ? (void *)GetProcAddress(dll, "DllGetClassObject") : NULL;
    if (get_factory)
    {
        h = mk(L"Windows.Media.NoSuchClass");
        hr = get_factory(h, &f);
        CHECKF(hr == CLASS_E_CLASSNOTAVAILABLE && !f, "DllGetActivationFactory(unknown) is CLASS_E_CLASSNOTAVAILABLE (%#lx)", hr);
        WindowsDeleteString(h);
    }
    else check(0, "DllGetActivationFactory export");
    if (get_class_object)
    {
        hr = get_class_object(&IID_Bogus, &IID_IUnknown, &obj);
        CHECKF(hr == CLASS_E_CLASSNOTAVAILABLE && !obj, "DllGetClassObject: CLASS_E_CLASSNOTAVAILABLE and a NULL pointer (%#lx)", hr);
    }
    else check(0, "DllGetClassObject export");

    RoUninitialize();
    printf("%s\n", failures ? "RESULT: FAIL" : "RESULT: PASS");
    return failures != 0;
}
