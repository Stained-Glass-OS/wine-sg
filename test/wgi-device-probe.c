/* windows.gaming.input's device-bound stubs (patches/sg/2800), run by
 * test/wgi-device-gate.sh with a virtual gamepad (uinput, vendor 045e product
 * 028e) attached. A custom factory registered for that hardware id is handed
 * the device's IGameControllerProvider (RegisterCustomFactoryForHardwareId),
 * and the RawGameController the DLL builds for it is queried for the
 * properties that used to be stubs: the provider's IsConnected and version
 * info, the raw controller's labels, switch kinds, name and id, and the
 * controller's headset, user, wireless and battery members.
 *
 *   wgi-device-probe.exe     (prints READY once the factory is registered) */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <roapi.h>
#include <winstring.h>
#include <inspectable.h>
#include <activation.h>
#include <stdio.h>
#include <wchar.h>

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#ifndef E_BOUNDS
#define E_BOUNDS ((HRESULT)0x8000000B)
#endif
DEFINE_GUID(IID_IManagerStatics, 0x36cb66e3, 0xd0a1, 0x4986, 0xa2, 0x4c, 0x40, 0xb1, 0x37, 0xde, 0xba, 0x9e);
DEFINE_GUID(IID_IRawGameControllerStatics, 0xeb8d0792, 0xe95a, 0x4b19, 0xaf, 0xc7, 0x0a, 0x59, 0xf8, 0xbf, 0x75, 0x9e);
DEFINE_GUID(IID_IGamepadStatics, 0x8bbce529, 0xd49c, 0x39e9, 0x95, 0x60, 0xe4, 0x7d, 0xde, 0x96, 0xb7, 0xc8);
DEFINE_GUID(IID_IGameControllerImpl, 0x06e58977, 0x7684, 0x4dc5, 0xba, 0xd1, 0xcd, 0xa5, 0x2a, 0x4a, 0xa0, 0x6d);
DEFINE_GUID(IID_ICustomGameControllerFactory, 0x69a0ae5e, 0x758e, 0x4cbe, 0xac, 0xe6, 0x62, 0x15, 0x5f, 0xe9, 0x12, 0x6f);
DEFINE_GUID(IID_IGameControllerProvider, 0xe6d73982, 0x2996, 0x4559, 0xb1, 0x6c, 0x3e, 0x57, 0xd4, 0x6e, 0x58, 0xd6);
DEFINE_GUID(IID_IRawGameController, 0x7cad6d91, 0xa7e1, 0x4f71, 0x9a, 0x78, 0x33, 0xe9, 0xc5, 0xdf, 0xea, 0x62);
DEFINE_GUID(IID_IRawGameController2, 0x43c0c035, 0xbb73, 0x4756, 0xa7, 0x87, 0x3e, 0xd6, 0xbe, 0xa6, 0x17, 0xbd);
DEFINE_GUID(IID_IGameController, 0x1baf6522, 0x5f64, 0x42c5, 0x82, 0x67, 0xb9, 0xfe, 0x22, 0x15, 0xbf, 0xbd);
DEFINE_GUID(IID_IGameControllerBatteryInfo, 0xdcecc681, 0x3963, 0x4da6, 0x95, 0x5d, 0x55, 0x3f, 0x3b, 0x6f, 0x61, 0x61);
DEFINE_GUID(IID_IGamepad2, 0x3c1689bd, 0x5915, 0x4245, 0xb0, 0xc0, 0xc8, 0x9f, 0xae, 0x03, 0x08, 0xff);

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}
#define CHECKF(ok, ...) do { char _b[256]; snprintf(_b, sizeof(_b), __VA_ARGS__); check(ok, _b); } while (0)

#define VT(obj) (*(void ***)(obj))
typedef HRESULT (WINAPI *fn_out)(void *, void *);
#define CALL1(obj, slot, out) (((fn_out)VT(obj)[slot])((obj), (out)))
typedef HRESULT (WINAPI *fn_int)(void *, int, void *);
#define CALLI(obj, slot, idx, out) (((fn_int)VT(obj)[slot])((obj), (idx), (out)))

/* ---- the custom factory and the controller object it makes ---- */
struct version_info { UINT16 major, minor, build, revision; };
static IUnknown *provider;          /* the IGameControllerProvider we were handed */
static volatile LONG created, added, removed;
static volatile LONG added_ok;
static void *added_controller;      /* the IGameController the DLL wrapped around our object */

struct obj { void *vtbl; LONG ref; };

static HRESULT WINAPI obj_QueryInterface(void *iface, REFIID iid, void **out)
{
    if (IsEqualGUID(iid, &IID_IUnknown) || IsEqualGUID(iid, &IID_IInspectable) || IsEqualGUID(iid, &IID_IAgileObject) ||
        IsEqualGUID(iid, &IID_IGameControllerImpl) || IsEqualGUID(iid, &IID_ICustomGameControllerFactory))
    {
        *out = iface;
        IUnknown_AddRef((IUnknown *)iface);
        return S_OK;
    }
    *out = NULL;
    return E_NOINTERFACE;
}
static ULONG WINAPI obj_AddRef(void *iface) { return InterlockedIncrement(&((struct obj *)iface)->ref); }
static ULONG WINAPI obj_Release(void *iface) { return InterlockedDecrement(&((struct obj *)iface)->ref); }
static HRESULT WINAPI obj_GetIids(void *iface, ULONG *count, IID **iids) { *count = 0; *iids = NULL; return S_OK; }
static HRESULT WINAPI obj_GetRuntimeClassName(void *iface, HSTRING *name) { return WindowsCreateString(L"SgTest", 6, name); }
static HRESULT WINAPI obj_GetTrustLevel(void *iface, TrustLevel *trust) { *trust = BaseTrust; return S_OK; }

/* IGameControllerImpl: Initialize(outer, provider) keeps the provider */
static HRESULT WINAPI impl_Initialize(void *iface, void *outer, void *prov)
{
    IUnknown_AddRef(provider = (IUnknown *)prov);
    return S_OK;
}
static void *impl_vtbl[] = { obj_QueryInterface, obj_AddRef, obj_Release, obj_GetIids, obj_GetRuntimeClassName,
                             obj_GetTrustLevel, impl_Initialize };
static struct obj controller_obj = { impl_vtbl, 1 };

static HRESULT WINAPI factory_Create(void *iface, void *prov, IInspectable **value)
{
    InterlockedIncrement(&created);
    *value = (IInspectable *)&controller_obj;
    IInspectable_AddRef(*value);
    return S_OK;
}
static HRESULT WINAPI factory_Added(void *iface, void *controller)
{
    added_controller = controller;
    IUnknown_AddRef((IUnknown *)controller);
    InterlockedIncrement(&added);
    return S_OK;
}
static HRESULT WINAPI factory_Removed(void *iface, void *controller) { InterlockedIncrement(&removed); return S_OK; }
static void *factory_vtbl[] = { obj_QueryInterface, obj_AddRef, obj_Release, obj_GetIids, obj_GetRuntimeClassName,
                                obj_GetTrustLevel, factory_Create, factory_Added, factory_Removed };
static struct obj factory_obj = { factory_vtbl, 1 };

static IActivationFactory *get_factory(const WCHAR *class_name)
{
    IActivationFactory *factory = NULL;
    HSTRING str;

    WindowsCreateString(class_name, wcslen(class_name), &str);
    RoGetActivationFactory(str, &IID_IActivationFactory, (void **)&factory);
    WindowsDeleteString(str);
    return factory;
}

static int wait_for(volatile LONG *value, int ms)
{
    while (ms > 0 && !*value) { Sleep(100); ms -= 100; }
    return *value != 0;
}

static void check_provider(void)
{
    struct version_info fw = {9, 9, 9, 9}, hw = {9, 9, 9, 9};
    UINT16 vid = 0, pid = 0;
    BOOLEAN connected = FALSE;
    HRESULT hr;

    check(provider != NULL, "the factory was given a provider");
    if (!provider) return;
    hr = CALL1(provider, 7, &pid);
    CHECKF(hr == S_OK && pid == 0x028e, "provider HardwareProductId is 028e (%#lx, %04x)", hr, pid);
    hr = CALL1(provider, 8, &vid);
    CHECKF(hr == S_OK && vid == 0x045e, "provider HardwareVendorId is 045e (%#lx, %04x)", hr, vid);
    hr = CALL1(provider, 10, &connected);
    CHECKF(hr == S_OK && connected, "provider IsConnected is TRUE (%#lx, %d)", hr, connected);
    hr = CALL1(provider, 6, &fw);
    CHECKF(hr == S_OK && !fw.major && !fw.minor && !fw.build && !fw.revision,
           "provider FirmwareVersionInfo is 0.0.0.0 (%#lx, %u.%u.%u.%u)", hr, fw.major, fw.minor, fw.build, fw.revision);
    hr = CALL1(provider, 9, &hw);
    CHECKF(hr == S_OK && !hw.build && !hw.revision, "provider HardwareVersionInfo has no build or revision (%#lx, %u.%u.%u.%u)",
           hr, hw.major, hw.minor, hw.build, hw.revision);
    CHECKF(hr == S_OK && hw.major == 1 && hw.minor == 0x10, "provider HardwareVersionInfo is the device's bcdDevice 1.16 (%u.%u)",
           hw.major, hw.minor);
}

static void check_raw(IUnknown *ctl)
{
    void *raw = NULL, *raw2 = NULL;
    INT32 axes = -1, buttons = -1, switches = -1, label = -1, kind = -1;
    UINT16 pid = 0, vid = 0;
    HSTRING str = NULL;
    void *haptics = NULL;
    HRESULT hr;

    hr = IUnknown_QueryInterface(ctl, &IID_IRawGameController, &raw);
    check(hr == S_OK, "the controller is an IRawGameController");
    hr = IUnknown_QueryInterface(ctl, &IID_IRawGameController2, &raw2);
    check(hr == S_OK, "the controller is an IRawGameController2");
    if (!raw || !raw2) return;

    CALL1(raw, 6, &axes);
    CALL1(raw, 7, &buttons);
    CALL1(raw, 11, &switches);
    CHECKF(axes == 6 && switches == 1 && buttons > 0, "raw counts: %d axes, %d buttons, %d switches", axes, buttons, switches);
    CALL1(raw, 9, &pid);
    CALL1(raw, 10, &vid);
    CHECKF(pid == 0x028e && vid == 0x045e, "raw product and vendor ids (%04x %04x)", pid, vid);

    label = 99;
    hr = CALLI(raw, 12, 0, &label);
    CHECKF(hr == S_OK && label == 0, "GetButtonLabel(0) is None (%#lx, %d)", hr, label);
    hr = CALLI(raw, 12, buttons - 1, &label);
    CHECKF(hr == S_OK, "GetButtonLabel(last) works (%#lx)", hr);
    hr = CALLI(raw, 12, buttons, &label);
    CHECKF(hr == E_BOUNDS, "GetButtonLabel(ButtonCount) is E_BOUNDS (%#lx)", hr);
    hr = CALLI(raw, 12, -1, &label);
    CHECKF(hr == E_BOUNDS, "GetButtonLabel(-1) is E_BOUNDS (%#lx)", hr);

    hr = CALLI(raw, 14, 0, &kind);
    CHECKF(hr == S_OK && kind == 2, "GetSwitchKind(0) is EightWay (%#lx, %d)", hr, kind);
    hr = CALLI(raw, 14, switches, &kind);
    CHECKF(hr == E_BOUNDS, "GetSwitchKind(SwitchCount) is E_BOUNDS (%#lx)", hr);

    hr = CALL1(raw2, 6, &haptics);
    CHECKF(hr == S_OK && haptics, "SimpleHapticsControllers is an (empty) view (%#lx)", hr);
    if (haptics)
    {
        UINT32 size = 7;
        hr = CALL1(haptics, 7, &size);
        CHECKF(hr == S_OK && size == 0, "SimpleHapticsControllers is empty (%#lx, %u)", hr, size);
        IUnknown_Release((IUnknown *)haptics);
    }
    hr = CALL1(raw2, 8, &str);
    CHECKF(hr == S_OK && str && !wcscmp(WindowsGetStringRawBuffer(str, NULL), L"HID-compliant game controller") &&
           WindowsGetStringLen(str) == 29,
           "DisplayName is the HID game controller description (%#lx, %ls)", hr, str ? WindowsGetStringRawBuffer(str, NULL) : L"(null)");
    WindowsDeleteString(str);
    str = NULL;
    hr = CALL1(raw2, 7, &str);
    CHECKF(hr == S_OK && str && !wcsncmp(WindowsGetStringRawBuffer(str, NULL), L"{wgi/nrid/", 10) &&
           WindowsGetStringRawBuffer(str, NULL)[WindowsGetStringLen(str) - 1] == L'}' &&
           WindowsGetStringLen(str) == wcslen(WindowsGetStringRawBuffer(str, NULL)),
           "NonRoamableId is {wgi/nrid/...} (%#lx, %ls)", hr, str ? WindowsGetStringRawBuffer(str, NULL) : L"(null)");
    {
        HSTRING again = NULL;
        hr = CALL1(raw2, 7, &again);
        CHECKF(hr == S_OK && str && again && !wcscmp(WindowsGetStringRawBuffer(str, NULL), WindowsGetStringRawBuffer(again, NULL)),
               "NonRoamableId is the same each time (%#lx)", hr);
        WindowsDeleteString(again);
    }
    WindowsDeleteString(str);
    IUnknown_Release((IUnknown *)raw);
    IUnknown_Release((IUnknown *)raw2);
}

typedef HRESULT (WINAPI *fn_add)(void *, void *, INT64 *);
typedef HRESULT (WINAPI *fn_rem)(void *, INT64);

static void check_game_controller(IUnknown *ctl)
{
    void *gc = NULL, *battery = NULL;
    IID *iids = NULL;
    ULONG count = 0, i;
    int have_gc = 0, have_battery = 0;
    INT64 token = 0;
    BOOLEAN wireless = 7;
    void *headset = (void *)1, *user = (void *)1, *report = (void *)1;
    HRESULT hr;
    int slot;

    hr = IUnknown_QueryInterface(ctl, &IID_IGameController, &gc);
    check(hr == S_OK, "the controller is an IGameController");
    hr = IUnknown_QueryInterface(ctl, &IID_IGameControllerBatteryInfo, &battery);
    check(hr == S_OK, "the controller is an IGameControllerBatteryInfo");
    if (!gc || !battery) return;

    hr = IInspectable_GetIids((IInspectable *)gc, &count, &iids);
    CHECKF(hr == S_OK && count >= 2, "controller GetIids lists its interfaces (%#lx, %lu)", hr, count);
    for (i = 0; hr == S_OK && i < count; i++)
    {
        IUnknown *unk = NULL;
        if (IsEqualGUID(&iids[i], &IID_IGameController)) have_gc++;
        if (IsEqualGUID(&iids[i], &IID_IGameControllerBatteryInfo)) have_battery++;
        if (FAILED(IUnknown_QueryInterface((IUnknown *)gc, &iids[i], (void **)&unk)) || !unk)
            CHECKF(0, "controller GetIids entry %lu answers QueryInterface", i);
        else IUnknown_Release(unk);
    }
    CHECKF(have_gc == 1 && have_battery == 1, "controller GetIids has IGameController and IGameControllerBatteryInfo once each (%d, %d)",
           have_gc, have_battery);
    CoTaskMemFree(iids);

    /* HeadsetConnected(6/7), HeadsetDisconnected(8/9), UserChanged(10/11): add(handler, token*), remove(token) */
    for (slot = 6; slot <= 10; slot += 2)
    {
        static const char *names[] = {"HeadsetConnected", "HeadsetDisconnected", "UserChanged"};
        token = 0;
        hr = ((fn_add)VT(gc)[slot])(gc, &obj_QueryInterface, &token);
        CHECKF(hr == S_OK && token, "add_%s accepts a handler and gives a token (%#lx)", names[(slot - 6) / 2], hr);
        hr = ((fn_rem)VT(gc)[slot + 1])(gc, token);
        CHECKF(hr == S_OK, "remove_%s accepts the token (%#lx)", names[(slot - 6) / 2], hr);
        hr = ((fn_add)VT(gc)[slot])(gc, NULL, &token);
        CHECKF(hr == E_INVALIDARG, "add_%s rejects a NULL handler (%#lx)", names[(slot - 6) / 2], hr);
    }

    hr = CALL1(gc, 12, &headset);
    CHECKF(hr == S_OK && !headset, "Headset is NULL: none attached (%#lx)", hr);
    hr = CALL1(gc, 13, &wireless);
    CHECKF(hr == S_OK && !wireless, "IsWireless is FALSE (%#lx, %d)", hr, wireless);
    hr = CALL1(gc, 14, &user);
    CHECKF(hr == S_OK && !user, "User is NULL (%#lx)", hr);
    hr = CALL1(battery, 6, &report);
    CHECKF(hr == S_OK && !report, "TryGetBatteryReport succeeds with no report (%#lx)", hr);

    IUnknown_Release((IUnknown *)gc);
    IUnknown_Release((IUnknown *)battery);
}

static void check_gamepad(IUnknown *gp_statics)
{
    static const struct { UINT32 button; int label; const char *name; } labels[] =
    {
        {0x1, 3, "Menu -> XboxMenu"}, {0x2, 4, "View -> XboxView"}, {0x4, 9, "A -> XboxA"}, {0x8, 10, "B -> XboxB"},
        {0x10, 11, "X -> XboxX"}, {0x20, 12, "Y -> XboxY"}, {0x40, 5, "DPadUp -> XboxUp"}, {0x80, 6, "DPadDown -> XboxDown"},
        {0x100, 7, "DPadLeft -> XboxLeft"}, {0x200, 8, "DPadRight -> XboxRight"},
        {0x400, 13, "LeftShoulder -> XboxLeftBumper"}, {0x800, 16, "RightShoulder -> XboxRightBumper"},
        {0x1000, 15, "LeftThumbstick -> XboxLeftStickButton"}, {0x2000, 18, "RightThumbstick -> XboxRightStickButton"},
        {0x4000, 19, "Paddle1 -> XboxPaddle1"}, {0x8000, 20, "Paddle2 -> XboxPaddle2"},
        {0x10000, 21, "Paddle3 -> XboxPaddle3"}, {0x20000, 22, "Paddle4 -> XboxPaddle4"}, {0, 0, "None -> None"},
        {0x40000, 0, "an unknown button -> None"},
    };
    IUnknown *view = NULL, *pad = NULL, *gp2 = NULL;
    UINT32 size = 0;
    HRESULT hr;
    int i;

    for (i = 0; i < 100; i++)
    {
        hr = CALL1(gp_statics, 10, &view);
        if (hr == S_OK && view && SUCCEEDED(CALL1(view, 7, &size)) && size >= 1) break;
        if (view) IUnknown_Release(view);
        view = NULL;
        Sleep(100);
    }
    CHECKF(view && size == 1, "Gamepads lists the XInput gamepad (%u)", size);
    if (!view || !size) return;
    hr = CALLI(view, 6, 0, &pad);
    check(hr == S_OK && pad, "Gamepads.GetAt(0)");
    if (pad && SUCCEEDED(IUnknown_QueryInterface(pad, &IID_IGamepad2, (void **)&gp2)))
    {
        for (i = 0; i < ARRAY_SIZE(labels); i++)
        {
            int label = -1;
            hr = ((HRESULT (WINAPI *)(void *, UINT32, void *))VT(gp2)[6])(gp2, labels[i].button, &label);
            CHECKF(hr == S_OK && label == labels[i].label, "GetButtonLabel: %s (%#lx, %d)", labels[i].name, hr, label);
        }
        IUnknown_Release(gp2);
    }
    else check(0, "the gamepad is an IGamepad2");
    if (pad) IUnknown_Release(pad);
    IUnknown_Release(view);
}

int main(int argc, char **argv)
{
    HRESULT hr = RoInitialize(RO_INIT_MULTITHREADED);
    IActivationFactory *mgr, *raw_f;
    IUnknown *statics = NULL, *raw_statics = NULL, *view = NULL, *ctl = NULL;
    UINT32 size = 0;
    int i;

    check(SUCCEEDED(hr), "RoInitialize");
    mgr = get_factory(L"Windows.Gaming.Input.Custom.GameControllerFactoryManager");
    raw_f = get_factory(L"Windows.Gaming.Input.RawGameController");
    if (!mgr || !raw_f) { check(0, "activation factories"); return 1; }
    IActivationFactory_QueryInterface(mgr, &IID_IManagerStatics, (void **)&statics);
    IActivationFactory_QueryInterface(raw_f, &IID_IRawGameControllerStatics, (void **)&raw_statics);

    /* RegisterCustomFactoryForHardwareId is slot 7 */
    {
        typedef HRESULT (WINAPI *reg_fn)(void *, void *, UINT16, UINT16);
        hr = ((reg_fn)VT(statics)[7])(statics, &factory_obj, 0x045e, 0x028e);
        CHECKF(hr == S_OK, "RegisterCustomFactoryForHardwareId(045e, 028e) (%#lx)", hr);
    }
    printf("READY\n");
    fflush(stdout);

    /* the gate now plugs the virtual gamepad in */
    check(wait_for(&created, 30000), "the registered factory was asked to create a controller for the device");
    check(wait_for(&added, 15000), "the registered factory was told the controller was added");
    check_provider();

    for (i = 0; i < 100; i++)
    {
        hr = CALL1(raw_statics, 10, &view);
        if (hr == S_OK && view && SUCCEEDED(CALL1(view, 7, &size)) && size >= 1) break;
        if (view) IUnknown_Release(view);
        view = NULL;
        Sleep(100);
    }
    CHECKF(view && size == 1, "RawGameControllers lists the device's raw controller (%u)", size);
    if (view && size)
    {
        hr = CALLI(view, 6, 0, &ctl);
        check(hr == S_OK && ctl, "RawGameControllers.GetAt(0)");
    }
    if (ctl)
    {
        check_raw(ctl);
        check_game_controller(ctl);
        IUnknown_Release(ctl);
    }
    {
        IActivationFactory *gp_f = get_factory(L"Windows.Gaming.Input.Gamepad");
        IUnknown *gp_statics = NULL;
        if (gp_f) IActivationFactory_QueryInterface(gp_f, &IID_IGamepadStatics, (void **)&gp_statics);
        if (gp_statics) check_gamepad(gp_statics);
        else check(0, "Gamepad statics");
    }
    if (added_controller)
    {
        void *gc = NULL;
        check(SUCCEEDED(IUnknown_QueryInterface((IUnknown *)added_controller, &IID_IGameController, &gc)) && gc,
              "the controller the factory was told about is an IGameController");
        if (gc) check_game_controller((IUnknown *)gc);
        if (gc) IUnknown_Release((IUnknown *)gc);
    }

    printf("%s\n", failures ? "RESULT: FAIL" : "RESULT: PASS");
    fflush(stdout);
    if (argc > 1) Sleep(2000);
    return failures != 0;
}
