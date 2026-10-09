/* windows.gaming.input's IInspectable and factory stubs (patches/sg/2800),
 * run by test/wgi-stubs-gate.sh on Xvfb. For every activation factory and
 * object the DLL hands out without a game controller attached it checks
 * GetIids (the exact interface set, and that each one answers QueryInterface),
 * GetTrustLevel (BaseTrust), GetRuntimeClassName, E_NOINTERFACE for an
 * unknown IID, ActivateInstance, the custom-factory registration calls, and
 * the collections the statics return (an empty IVectorView and its iterator).
 *
 *   wgi-stubs-probe.exe */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <roapi.h>
#include <winstring.h>
#include <inspectable.h>
#include <activation.h>
#include <stdio.h>
#include <wchar.h>

DEFINE_GUID(IID_IGamepadStatics, 0x8bbce529, 0xd49c, 0x39e9, 0x95, 0x60, 0xe4, 0x7d, 0xde, 0x96, 0xb7, 0xc8);
DEFINE_GUID(IID_IGamepadStatics2, 0x42676dc5, 0x0856, 0x47c4, 0x92, 0x13, 0xb3, 0x95, 0x50, 0x4c, 0x3a, 0x3c);
DEFINE_GUID(IID_IRawGameControllerStatics, 0xeb8d0792, 0xe95a, 0x4b19, 0xaf, 0xc7, 0x0a, 0x59, 0xf8, 0xbf, 0x75, 0x9e);
DEFINE_GUID(IID_IRacingWheelStatics, 0x3ac12cd5, 0x581b, 0x4936, 0x9f, 0x94, 0x69, 0xf1, 0xe6, 0x51, 0x4c, 0x7d);
DEFINE_GUID(IID_IRacingWheelStatics2, 0xe666bcaa, 0xedfd, 0x4323, 0xa9, 0xf6, 0x3c, 0x38, 0x40, 0x48, 0xd1, 0xed);
DEFINE_GUID(IID_IManagerStatics, 0x36cb66e3, 0xd0a1, 0x4986, 0xa2, 0x4c, 0x40, 0xb1, 0x37, 0xde, 0xba, 0x9e);
DEFINE_GUID(IID_IManagerStatics2, 0xeace5644, 0x19df, 0x4115, 0xb3, 0x2a, 0x27, 0x93, 0xe2, 0xae, 0xa3, 0xbb);
DEFINE_GUID(IID_ICustomGameControllerFactory, 0x69a0ae5e, 0x758e, 0x4cbe, 0xac, 0xe6, 0x62, 0x15, 0x5f, 0xe9, 0x12, 0x6f);
DEFINE_GUID(IID_IConstantForceEffect, 0x9bfa0140, 0xf3c7, 0x415c, 0xb0, 0x68, 0x0f, 0x06, 0x87, 0x34, 0xbc, 0xe0);
DEFINE_GUID(IID_IRampForceEffect, 0xf1f81259, 0x1ca6, 0x4080, 0xb5, 0x6d, 0xb4, 0x3f, 0x33, 0x54, 0xd0, 0x52);
DEFINE_GUID(IID_IPeriodicForceEffect, 0x5c5138d7, 0xfc75, 0x4d52, 0x9a, 0x0a, 0xef, 0xe4, 0xca, 0xb5, 0xfe, 0x64);
DEFINE_GUID(IID_IConditionForceEffect, 0x32d1ea68, 0x3695, 0x4e69, 0x85, 0xc0, 0xcd, 0x19, 0x44, 0x18, 0x91, 0x40);
DEFINE_GUID(IID_IForceFeedbackEffect, 0xa17fba0c, 0x2ae4, 0x48c2, 0x80, 0x63, 0xea, 0xbd, 0x07, 0x77, 0xcb, 0x89);
DEFINE_GUID(IID_IPeriodicForceEffectFactory, 0x6f62eb1a, 0x9851, 0x477b, 0xb3, 0x18, 0x35, 0xec, 0xaa, 0x15, 0x07, 0x0f);
DEFINE_GUID(IID_IConditionForceEffectFactory, 0x91a99264, 0x1810, 0x4eb6, 0xa7, 0x73, 0xbf, 0xd3, 0xb8, 0xcd, 0xdb, 0xab);
DEFINE_GUID(IID_Bogus, 0x12345678, 0x1234, 0x1234, 0x12, 0x34, 0x12, 0x34, 0x12, 0x34, 0x12, 0x34);

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}
#define CHECKF(ok, ...) do { char _b[256]; snprintf(_b, sizeof(_b), __VA_ARGS__); check(ok, _b); } while (0)

#define MAXIIDS 6
struct inspectable_case
{
    const char *name;
    const WCHAR *class_name;      /* expected GetRuntimeClassName */
    const GUID *iids[MAXIIDS];    /* expected GetIids set; NULL ends it */
};

static int in_list(const GUID *g, const GUID *const *list)
{
    for (; *list; list++) if (IsEqualGUID(g, *list)) return 1;
    return 0;
}

/* The IInspectable contract of one object. */
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
    CHECKF(hr == S_OK, "%s: GetIids returns S_OK (%#lx)", c->name, hr);
    CHECKF(hr == S_OK && count == expected, "%s: GetIids reports %lu interfaces (want %lu)", c->name, count, expected);
    if (hr == S_OK)
    {
        for (i = 0; i < count; i++)
        {
            IUnknown *unk = NULL;
            if (!in_list(&iids[i], c->iids))
            {
                all_known = 0;
                printf("note  %s: unexpected IID {%08lx-%04x-%04x-...}\n", c->name, iids[i].Data1, iids[i].Data2, iids[i].Data3);
            }
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
    CHECKF(hr == S_OK && name && !wcscmp(WindowsGetStringRawBuffer(name, NULL), c->class_name) &&
           WindowsGetStringLen(name) == wcslen(c->class_name),
           "%s: GetRuntimeClassName is %ls, length without a terminator (hr %#lx, got %ls)", c->name, c->class_name, hr,
           name ? WindowsGetStringRawBuffer(name, NULL) : L"(null)");
    WindowsDeleteString(name);

    out = (void *)0x1234;
    hr = IInspectable_QueryInterface(obj, &IID_Bogus, &out);
    CHECKF(hr == E_NOINTERFACE && !out, "%s: unknown IID gives E_NOINTERFACE and a NULL pointer (%#lx %p)", c->name, hr, out);
}

struct factory_case
{
    struct inspectable_case insp;
    HRESULT activate;             /* ActivateInstance result */
    struct inspectable_case obj;  /* when it activates: the new object */
};

static const struct factory_case factories[] =
{
    { { "Gamepad factory", L"Windows.Gaming.Input.Gamepad",
        { &IID_IActivationFactory, &IID_IGamepadStatics, &IID_IGamepadStatics2, &IID_ICustomGameControllerFactory } },
      E_NOTIMPL },
    { { "RawGameController factory", L"Windows.Gaming.Input.RawGameController",
        { &IID_IActivationFactory, &IID_IRawGameControllerStatics, &IID_ICustomGameControllerFactory } },
      E_NOTIMPL },
    { { "RacingWheel factory", L"Windows.Gaming.Input.RacingWheel",
        { &IID_IActivationFactory, &IID_IRacingWheelStatics, &IID_IRacingWheelStatics2, &IID_ICustomGameControllerFactory } },
      E_NOTIMPL },
    { { "GameControllerFactoryManager factory", L"Windows.Gaming.Input.Custom.GameControllerFactoryManager",
        { &IID_IActivationFactory, &IID_IManagerStatics, &IID_IManagerStatics2 } },
      E_NOTIMPL },
    { { "ConstantForceEffect factory", L"Windows.Gaming.Input.ForceFeedback.ConstantForceEffect",
        { &IID_IActivationFactory } },
      S_OK,
      { "ConstantForceEffect", L"Windows.Gaming.Input.ForceFeedback.ConstantForceEffect",
        { &IID_IConstantForceEffect, &IID_IForceFeedbackEffect } } },
    { { "RampForceEffect factory", L"Windows.Gaming.Input.ForceFeedback.RampForceEffect",
        { &IID_IActivationFactory } },
      S_OK,
      { "RampForceEffect", L"Windows.Gaming.Input.ForceFeedback.RampForceEffect",
        { &IID_IRampForceEffect, &IID_IForceFeedbackEffect } } },
    { { "PeriodicForceEffect factory", L"Windows.Gaming.Input.ForceFeedback.PeriodicForceEffect",
        { &IID_IActivationFactory, &IID_IPeriodicForceEffectFactory } },
      E_NOTIMPL },
    { { "ConditionForceEffect factory", L"Windows.Gaming.Input.ForceFeedback.ConditionForceEffect",
        { &IID_IActivationFactory, &IID_IConditionForceEffectFactory } },
      E_NOTIMPL },
};

static IActivationFactory *get_factory(const WCHAR *class_name)
{
    IActivationFactory *factory = NULL;
    HSTRING str;
    HRESULT hr;

    WindowsCreateString(class_name, wcslen(class_name), &str);
    hr = RoGetActivationFactory(str, &IID_IActivationFactory, (void **)&factory);
    WindowsDeleteString(str);
    if (FAILED(hr)) printf("note  RoGetActivationFactory(%ls) = %#lx\n", class_name, hr);
    return factory;
}

typedef HRESULT (WINAPI *vtbl_fn)(void *, void **);
static HRESULT vcall(void *obj, int slot, void **out)
{
    return ((vtbl_fn)(*(void ***)obj)[slot])(obj, out);
}

/* a statics' empty collection and the iterator over it */
static void check_collection(const WCHAR *class_name, const GUID *statics_iid, int getter_slot, const WCHAR *element,
                             const char *label)
{
    IActivationFactory *factory = get_factory(class_name);
    IInspectable *statics = NULL, *view = NULL, *iterable = NULL, *iter = NULL;
    struct inspectable_case c;
    WCHAR vname[200], iname[200];
    ULONG count = 0;
    IID *iids = NULL;
    char what[100];
    HRESULT hr;

    if (!factory) { check(0, "collection: factory"); return; }
    hr = IActivationFactory_QueryInterface(factory, statics_iid, (void **)&statics);
    CHECKF(hr == S_OK, "%s: statics interface", label);
    if (hr != S_OK) goto done;
    hr = vcall(statics, getter_slot, (void **)&view);
    CHECKF(hr == S_OK && view, "%s: the collection getter returns a vector view", label);
    if (hr != S_OK || !view) goto done;

    /* GetIids: the view's own IID and IIterable<T>; the second is what First() is on */
    hr = IInspectable_GetIids(view, &count, &iids);
    CHECKF(hr == S_OK && count == 2, "%s: view GetIids lists two interfaces (%#lx, %lu)", label, hr, count);
    if (hr == S_OK && count == 2)
    {
        ULONG i;
        for (i = 0; i < 2; i++)
        {
            IInspectable *itf = NULL;
            if (SUCCEEDED(IInspectable_QueryInterface(view, &iids[i], (void **)&itf)) && itf)
            {
                if (i == 1) iterable = itf; else IInspectable_Release(itf);
            }
        }
        snprintf(what, sizeof(what), "%s: both reported view IIDs answer QueryInterface", label);
        check(iterable != NULL, what);
    }
    CoTaskMemFree(iids);

    swprintf(vname, ARRAY_SIZE(vname), L"Windows.Foundation.Collections.IVectorView`1<%ls>", element);
    memset(&c, 0, sizeof(c));
    {
        TrustLevel trust = 0x5555;
        HSTRING name = NULL;
        hr = IInspectable_GetTrustLevel(view, &trust);
        CHECKF(hr == S_OK && trust == BaseTrust, "%s: view GetTrustLevel is BaseTrust", label);
        hr = IInspectable_GetRuntimeClassName(view, &name);
        CHECKF(hr == S_OK && name && !wcscmp(WindowsGetStringRawBuffer(name, NULL), vname),
               "%s: view class name is %ls", label, vname);
        WindowsDeleteString(name);
    }

    if (iterable && vcall(iterable, 6, (void **)&iter) == S_OK && iter)
    {
        TrustLevel trust = 0x5555;
        HSTRING name = NULL;
        swprintf(iname, ARRAY_SIZE(iname), L"Windows.Foundation.Collections.IIterator`1<%ls>", element);
        hr = IInspectable_GetIids(iter, &count, &iids);
        CHECKF(hr == S_OK && count == 1, "%s: iterator GetIids lists one interface (%#lx, %lu)", label, hr, count);
        CoTaskMemFree(iids);
        hr = IInspectable_GetTrustLevel(iter, &trust);
        CHECKF(hr == S_OK && trust == BaseTrust, "%s: iterator GetTrustLevel is BaseTrust", label);
        hr = IInspectable_GetRuntimeClassName(iter, &name);
        CHECKF(hr == S_OK && name && !wcscmp(WindowsGetStringRawBuffer(name, NULL), iname),
               "%s: iterator class name is %ls", label, iname);
        WindowsDeleteString(name);
    }
    else check(0, "collection: First()");
done:
    if (iter) IInspectable_Release(iter);
    if (iterable) IInspectable_Release(iterable);
    if (view) IInspectable_Release(view);
    if (statics) IInspectable_Release(statics);
    IActivationFactory_Release(factory);
}

/* IGameControllerFactoryManagerStatics: slots 6 Gip(factory, guid), 7 HardwareId, 8 Xusb */
typedef HRESULT (WINAPI *reg_gip_fn)(void *, void *, GUID);
typedef HRESULT (WINAPI *reg_hw_fn)(void *, void *, UINT16, UINT16);
typedef HRESULT (WINAPI *reg_xusb_fn)(void *, void *, int, int);

static void check_registration(void)
{
    IActivationFactory *mgr = get_factory(L"Windows.Gaming.Input.Custom.GameControllerFactoryManager");
    IActivationFactory *gp = get_factory(L"Windows.Gaming.Input.Gamepad");
    IInspectable *statics = NULL, *custom = NULL;
    HRESULT hr;

    if (!mgr || !gp) { check(0, "registration: factories"); return; }
    IActivationFactory_QueryInterface(mgr, &IID_IManagerStatics, (void **)&statics);
    IActivationFactory_QueryInterface(gp, &IID_ICustomGameControllerFactory, (void **)&custom);
    if (!statics || !custom) { check(0, "registration: interfaces"); return; }

    hr = ((reg_gip_fn)(*(void ***)statics)[6])(statics, custom, IID_Bogus);
    CHECKF(hr == S_OK, "RegisterCustomFactoryForGipInterface accepts a factory (%#lx)", hr);
    hr = ((reg_gip_fn)(*(void ***)statics)[6])(statics, NULL, IID_Bogus);
    CHECKF(hr == E_INVALIDARG, "RegisterCustomFactoryForGipInterface rejects NULL (%#lx)", hr);
    hr = ((reg_hw_fn)(*(void ***)statics)[7])(statics, custom, 0x1234, 0x5678);
    CHECKF(hr == S_OK, "RegisterCustomFactoryForHardwareId accepts a factory (%#lx)", hr);
    hr = ((reg_hw_fn)(*(void ***)statics)[7])(statics, NULL, 0x1234, 0x5678);
    CHECKF(hr == E_INVALIDARG, "RegisterCustomFactoryForHardwareId rejects NULL (%#lx)", hr);
    hr = ((reg_xusb_fn)(*(void ***)statics)[8])(statics, custom, 1, 1);
    CHECKF(hr == S_OK, "RegisterCustomFactoryForXusbType accepts a factory (%#lx)", hr);
    hr = ((reg_xusb_fn)(*(void ***)statics)[8])(statics, NULL, 1, 1);
    CHECKF(hr == E_INVALIDARG, "RegisterCustomFactoryForXusbType rejects NULL (%#lx)", hr);

    IInspectable_Release(custom);
    IInspectable_Release(statics);
    IActivationFactory_Release(gp);
    IActivationFactory_Release(mgr);
}

int main(void)
{
    HRESULT hr = RoInitialize(RO_INIT_MULTITHREADED);
    HMODULE dll;
    HRESULT (WINAPI *get_class_object)(REFCLSID, REFIID, void **);
    size_t i;
    void *obj = (void *)1;

    check(SUCCEEDED(hr), "RoInitialize");

    for (i = 0; i < ARRAY_SIZE(factories); i++)
    {
        const struct factory_case *f = &factories[i];
        IActivationFactory *factory = get_factory(f->insp.class_name);
        IInspectable *inst = NULL;

        if (!factory) { check(0, f->insp.name); continue; }
        check_inspectable((IInspectable *)factory, &f->insp);

        hr = IActivationFactory_ActivateInstance(factory, &inst);
        CHECKF(hr == f->activate, "%s: ActivateInstance is %#lx (got %#lx)", f->insp.name, f->activate, hr);
        if (hr == S_OK && inst)
        {
            check_inspectable(inst, &f->obj);
            IInspectable_Release(inst);
        }
        IActivationFactory_Release(factory);
    }

    check_collection(L"Windows.Gaming.Input.Gamepad", &IID_IGamepadStatics, 10, L"Windows.Gaming.Input.Gamepad", "Gamepads");
    check_collection(L"Windows.Gaming.Input.RawGameController", &IID_IRawGameControllerStatics, 10,
                     L"Windows.Gaming.Input.RawGameController", "RawGameControllers");
    check_collection(L"Windows.Gaming.Input.RacingWheel", &IID_IRacingWheelStatics, 10,
                     L"Windows.Gaming.Input.RacingWheel", "RacingWheels");
    check_registration();

    dll = LoadLibraryW(L"windows.gaming.input.dll");
    get_class_object = dll ? (void *)GetProcAddress(dll, "DllGetClassObject") : NULL;
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
