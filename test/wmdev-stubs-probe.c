/* windows.media.devices (patches/sg/2822), run by test/wmdev-stubs-gate.sh on
 * Xvfb: the MediaDevice factory's IInspectable contract, the three selector
 * strings, the argument checks of the default-device ids, the default-device
 * changed event registrations, and which runtime classes the DLL hands out.
 *
 *   wmdev-stubs-probe.exe */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <roapi.h>
#include <winstring.h>
#include <inspectable.h>
#include <activation.h>
#include <stdio.h>
#include <wchar.h>

DEFINE_GUID(IID_IMediaDeviceStatics, 0xaa2d9a40, 0x909f, 0x4bba, 0xbf, 0x8b, 0x0c, 0x0d, 0x29, 0x6f, 0x14, 0xf0);
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


static const WCHAR *capture_sel = L"System.Devices.InterfaceClassGuid:=\"{2eef81be-33fa-4800-9670-1cd474972c3f}\" AND System.Devices.InterfaceEnabled:=System.StructuredQueryType.Boolean#True";
static const WCHAR *render_sel = L"System.Devices.InterfaceClassGuid:=\"{e6327cad-dcec-4949-ae8a-991e976a79d2}\" AND System.Devices.InterfaceEnabled:=System.StructuredQueryType.Boolean#True";
static const WCHAR *video_sel = L"System.Devices.InterfaceClassGuid:=\"{e5323777-f976-4f5b-9b55-b94699c46e44}\" AND System.Devices.InterfaceEnabled:=System.StructuredQueryType.Boolean#True";

static const struct { const char *name; int slot; const WCHAR **want; } selectors[] =
{
    { "GetAudioCaptureSelector", 6, &capture_sel },
    { "GetAudioRenderSelector", 7, &render_sel },
    { "GetVideoCaptureSelector", 8, &video_sel },
};

static void check_events(void *statics, int add, const char *name)
{
    typedef HRESULT (WINAPI *ev_add_fn)(void *, void *, EventRegistrationToken *);
    typedef HRESULT (WINAPI *ev_rem_fn)(void *, EventRegistrationToken);
    ev_add_fn add_fn = (ev_add_fn)VT(statics)[add];
    ev_rem_fn rem_fn = (ev_rem_fn)VT(statics)[add + 1];
    EventRegistrationToken t1 = {0}, t2 = {0}, t3 = {5};
    HRESULT hr;
    LONG base;

    obj_ref = 1;
    base = obj_ref;
    hr = add_fn(statics, &the_obj, &t1);
    CHECKF(hr == S_OK && t1.value != 0, "%s: add returns a token (%#lx)", name, hr);
    CHECKF(obj_ref == base + 1, "%s: the handler is referenced (%ld)", name, obj_ref - base);
    hr = add_fn(statics, &the_obj, &t2);
    CHECKF(hr == S_OK && t2.value != 0 && t2.value != t1.value, "%s: a second add gives another token", name);
    hr = add_fn(statics, &the_obj, NULL);
    CHECKF(hr == E_POINTER, "%s: NULL token is E_POINTER (%#lx)", name, hr);
    hr = add_fn(statics, NULL, &t3);
    CHECKF(hr == E_INVALIDARG, "%s: NULL handler is E_INVALIDARG (%#lx)", name, hr);
    hr = rem_fn(statics, t1);
    CHECKF(hr == S_OK && obj_ref == base + 1, "%s: remove releases the handler (%ld)", name, obj_ref - base);
    hr = rem_fn(statics, t1);
    CHECKF(hr == S_OK && obj_ref == base + 1, "%s: removing it again is harmless", name);
    hr = rem_fn(statics, t2);
    CHECKF(hr == S_OK && obj_ref == base, "%s: removing the second", name);
}

int main(void)
{
    HRESULT hr = RoInitialize(RO_INIT_MULTITHREADED);
    HSTRING cls = mk(L"Windows.Media.Devices.MediaDevice");
    IActivationFactory *factory = NULL;
    struct inspectable_case fc = { "MediaDevice factory", L"Windows.Media.Devices.MediaDevice",
                                   { &IID_IActivationFactory, &IID_IMediaDeviceStatics } };
    void *statics = NULL;
    IInspectable *inst = (IInspectable *)1;
    HMODULE dll;
    HRESULT (WINAPI *get_factory)(HSTRING, IActivationFactory **);
    HRESULT (WINAPI *get_class_object)(REFCLSID, REFIID, void **);
    void *obj = (void *)1;
    size_t i;

    check(SUCCEEDED(hr), "RoInitialize");
    hr = RoGetActivationFactory(cls, &IID_IActivationFactory, (void **)&factory);
    WindowsDeleteString(cls);
    if (FAILED(hr)) { check(0, "RoGetActivationFactory"); return 1; }

    check_inspectable((IInspectable *)factory, &fc);
    hr = IActivationFactory_ActivateInstance(factory, &inst);
    CHECKF(hr == E_NOTIMPL && !inst, "ActivateInstance is E_NOTIMPL with a NULL result (%#lx)", hr);
    hr = IActivationFactory_QueryInterface(factory, &IID_IMediaDeviceStatics, &statics);
    CHECKF(hr == S_OK && statics, "IMediaDeviceStatics");
    if (statics)
    {
        for (i = 0; i < ARRAY_SIZE(selectors); i++)
        {
            HSTRING s = NULL;
            hr = CALL_P(statics, selectors[i].slot, &s);
            CHECKF(hr == S_OK && hs_eq(s, *selectors[i].want), "%s is the interface-class query (%#lx, %ls)", selectors[i].name, hr,
                   s ? WindowsGetStringRawBuffer(s, NULL) : L"(null)");
            WindowsDeleteString(s);
            hr = CALL_P(statics, selectors[i].slot, NULL);
            CHECKF(hr == E_POINTER, "%s: NULL out pointer is E_POINTER (%#lx)", selectors[i].name, hr);
        }
        hr = CALL_UP(statics, 9, 0, NULL);
        CHECKF(hr == E_POINTER, "GetDefaultAudioCaptureId: NULL out pointer is E_POINTER (%#lx)", hr);
        hr = CALL_UP(statics, 10, 0, NULL);
        CHECKF(hr == E_POINTER, "GetDefaultAudioRenderId: NULL out pointer is E_POINTER (%#lx)", hr);
        check_events(statics, 11, "DefaultAudioCaptureDeviceChanged");
        check_events(statics, 13, "DefaultAudioRenderDeviceChanged");
        IUnknown_Release((IUnknown *)statics);
    }
    IActivationFactory_Release(factory);

    dll = LoadLibraryW(L"windows.media.devices.dll");
    get_factory = dll ? (void *)GetProcAddress(dll, "DllGetActivationFactory") : NULL;
    get_class_object = dll ? (void *)GetProcAddress(dll, "DllGetClassObject") : NULL;
    if (get_factory)
    {
        static const WCHAR *others[] = { L"Windows.Media.Devices.DefaultAudioCaptureDeviceChangedEventArgs",
                                         L"Windows.Media.Devices.NoSuchClass" };
        for (i = 0; i < ARRAY_SIZE(others); i++)
        {
            HSTRING h = mk(others[i]);
            IActivationFactory *f = (IActivationFactory *)1;
            hr = get_factory(h, &f);
            CHECKF(hr == CLASS_E_CLASSNOTAVAILABLE && !f, "DllGetActivationFactory(%ls) is CLASS_E_CLASSNOTAVAILABLE (%#lx)", others[i], hr);
            if (hr == S_OK && f) IActivationFactory_Release(f);
            WindowsDeleteString(h);
        }
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
