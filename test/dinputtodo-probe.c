/* dinput's device properties and device types (patches/sg/2875), run by
 * test/dinputtodo-gate.sh. Values are those of dinput's conformance tests
 * (joystick8.c, device8.c, force_feedback.c: todo_wine before). Keyboard and
 * mouse exist on any host; the HID gamepad checks need the gate's virtual pad
 * (045e:028e) and are skipped with a note when no game controller shows up.
 *
 *   dinputtodo-probe.exe [pad] */
#define COBJMACROS
#define DIRECTINPUT_VERSION 0x0800
#include <windows.h>
#include <initguid.h>
#include <dinput.h>
#include <hidusage.h>
#include <stdio.h>
#include <string.h>

DEFINE_GUID(IID_IDirectInput7W_sg, 0x9a4cb685, 0x236d, 0x11d3, 0x8e, 0x9d, 0x00, 0xc0, 0x4f, 0x68, 0x44, 0xae);
DEFINE_GUID(IID_IDirectInputDevice7W_sg, 0x57d7c6bd, 0x2356, 0x11d3, 0x8e, 0x9d, 0x00, 0xc0, 0x4f, 0x68, 0x44, 0xae);

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}
#define CHECKF(ok, ...) do { char _b[300]; snprintf(_b, sizeof(_b), __VA_ARGS__); check(ok, _b); } while (0)

typedef HRESULT (WINAPI *enum7_fn)(void *, DWORD, LPDIENUMDEVICESCALLBACKW, void *, DWORD);
typedef HRESULT (WINAPI *create7_fn)(void *, REFGUID, REFIID, void **, IUnknown *);
#define VT(o) (*(void ***)(o))

struct found { DIDEVICEINSTANCEW mouse, keyboard, pad; int have_pad; };

static BOOL CALLBACK collect(const DIDEVICEINSTANCEW *inst, void *ctx)
{
    struct found *f = ctx;
    if (IsEqualGUID(&inst->guidInstance, &GUID_SysMouse)) f->mouse = *inst;
    else if (IsEqualGUID(&inst->guidInstance, &GUID_SysKeyboard)) f->keyboard = *inst;
    else if (!f->have_pad && (inst->dwDevType & DIDEVTYPE_HID)) { f->pad = *inst; f->have_pad = 1; }
    return DIENUM_CONTINUE;
}

static DIPROPDWORD dword_prop(DWORD how, DWORD obj, DWORD data)
{
    DIPROPDWORD p = {{sizeof(DIPROPDWORD), sizeof(DIPROPHEADER), obj, how}, data};
    return p;
}

static DIPROPSTRING string_prop(const WCHAR *init)
{
    DIPROPSTRING p = {{sizeof(DIPROPSTRING), sizeof(DIPROPHEADER), 0, DIPH_DEVICE}};
    lstrcpyW(p.wsz, init);
    return p;
}

static void test_legacy_instances(HINSTANCE inst, struct found *v7)
{
    HRESULT (WINAPI *create_ex)(HINSTANCE, DWORD, REFIID, void **, IUnknown *) =
        (void *)GetProcAddress(LoadLibraryA("dinput.dll"), "DirectInputCreateEx");
    void *di7 = NULL;
    HRESULT hr;

    if (!create_ex) { check(0, "dinput exports DirectInputCreateEx"); return; }
    hr = create_ex(inst, 0x0700, &IID_IDirectInput7W_sg, &di7, NULL);
    CHECKF(hr == S_OK, "DirectInputCreateEx(0x700) (%#lx)", hr);
    if (hr != S_OK) return;
    memset(v7, 0, sizeof(*v7));
    hr = ((enum7_fn)VT(di7)[4])(di7, 0, collect, v7, DIEDFL_ATTACHEDONLY);
    CHECKF(hr == S_OK, "version 0x700 EnumDevices (%#lx)", hr);
    CHECKF(v7->mouse.dwDevType == (DIDEVTYPE_MOUSE | (DIDEVTYPEMOUSE_UNKNOWN << 8)),
           "version 0x700 mouse type is DIDEVTYPEMOUSE_UNKNOWN (%#lx)", v7->mouse.dwDevType);
    IUnknown_Release((IUnknown *)di7);
}

static void test_mouse_keyboard(IDirectInput8W *di8, const struct found *v8, HINSTANCE inst)
{
    static const WCHAR junk[] = L"junk";
    IDirectInputDevice8W *dev;
    DIDEVICEINSTANCEW devinst = {sizeof(devinst)};
    DIDEVCAPS caps = {sizeof(caps)};
    DIPROPDWORD pd;
    DIPROPSTRING ps;
    DIPROPPOINTER pp = {{sizeof(DIPROPPOINTER), sizeof(DIPROPHEADER), DIK_A, DIPH_BYOFFSET}, 0};
    HRESULT hr;
    int i;

    CHECKF(!lstrcmpW(v8->mouse.tszProductName, L"Mouse"), "mouse product name is Mouse (%ls)", v8->mouse.tszProductName);
    CHECKF(!lstrcmpW(v8->keyboard.tszProductName, L"Keyboard"), "keyboard product name is Keyboard (%ls)",
           v8->keyboard.tszProductName);
    CHECKF(v8->mouse.dwDevType == (DI8DEVTYPE_MOUSE | (DI8DEVTYPEMOUSE_UNKNOWN << 8)),
           "mouse type is DI8DEVTYPEMOUSE_UNKNOWN (%#lx)", v8->mouse.dwDevType);

    for (i = 0; i < 2; i++)
    {
        const char *name = i ? "keyboard" : "mouse";
        hr = IDirectInput8_CreateDevice(di8, i ? &GUID_SysKeyboard : &GUID_SysMouse, &dev, NULL);
        CHECKF(hr == S_OK, "CreateDevice(%s) (%#lx)", name, hr);
        if (hr != S_OK) continue;

        hr = IDirectInputDevice8_GetDeviceInfo(dev, &devinst);
        CHECKF(hr == S_OK && !lstrcmpW(devinst.tszProductName, i ? L"Keyboard" : L"Mouse"),
               "%s GetDeviceInfo product name (%ls)", name, devinst.tszProductName);
        hr = IDirectInputDevice8_GetCapabilities(dev, &caps);
        CHECKF(hr == S_OK && caps.dwFirmwareRevision == 0 && caps.dwHardwareRevision == 0,
               "%s capabilities have no firmware or hardware revision (%lu, %lu)", name,
               caps.dwFirmwareRevision, caps.dwHardwareRevision);
        if (!i) CHECKF(caps.dwDevType == (DI8DEVTYPE_MOUSE | (DI8DEVTYPEMOUSE_UNKNOWN << 8)),
                       "mouse capabilities type is DI8DEVTYPEMOUSE_UNKNOWN (%#lx)", caps.dwDevType);

        pd = dword_prop(DIPH_DEVICE, 0, 0xdeadbeef);
        hr = IDirectInputDevice8_GetProperty(dev, DIPROP_AXISMODE, &pd.diph);
        CHECKF(hr == DI_OK && pd.dwData == DIPROPAXISMODE_ABS, "%s default axis mode is absolute (%#lx, %lu)", name, hr, pd.dwData);
        if (!i)
        {
            hr = IDirectInputDevice8_SetDataFormat(dev, &c_dfDIMouse2);
            CHECKF(hr == DI_OK, "mouse SetDataFormat (%#lx)", hr);
            pd.dwData = 0xdeadbeef;
            hr = IDirectInputDevice8_GetProperty(dev, DIPROP_AXISMODE, &pd.diph);
            CHECKF(hr == DI_OK && pd.dwData == DIPROPAXISMODE_REL, "mouse axis mode is relative with c_dfDIMouse2 (%#lx, %lu)", hr, pd.dwData);
        }

        ps = string_prop(junk);
        hr = IDirectInputDevice8_GetProperty(dev, DIPROP_USERNAME, &ps.diph);
        CHECKF(hr == DI_NOEFFECT && !ps.wsz[0], "%s user name is empty with DI_NOEFFECT (%#lx, %ls)", name, hr, ps.wsz);
        hr = IDirectInputDevice8_GetProperty(dev, DIPROP_TYPENAME, &ps.diph);
        CHECKF(hr == DIERR_UNSUPPORTED, "%s has no type name (%#lx)", name, hr);
        pd = dword_prop(DIPH_DEVICE, 0, 0);
        hr = IDirectInputDevice8_GetProperty(dev, DIPROP_JOYSTICKID, &pd.diph);
        CHECKF(hr == DIERR_UNSUPPORTED, "%s has no joystick id (%#lx)", name, hr);

        if (i)
        {
            hr = IDirectInputDevice8_SetDataFormat(dev, &c_dfDIKeyboard);
            CHECKF(hr == DI_OK, "keyboard SetDataFormat (%#lx)", hr);
            pp.uData = 0xdeadbeef;
            hr = IDirectInputDevice8_GetProperty(dev, DIPROP_APPDATA, &pp.diph);
            CHECKF(hr == DI_OK && pp.uData == (DWORD_PTR)-1, "keyboard app data after SetDataFormat is -1 (%#lx, %#llx)", hr, (unsigned long long)pp.uData);
            pp.uData = 7;
            hr = IDirectInputDevice8_SetProperty(dev, DIPROP_APPDATA, &pp.diph);
            CHECKF(hr == DI_OK, "keyboard SetProperty APPDATA (%#lx)", hr);
            pp.uData = 0;
            hr = IDirectInputDevice8_GetProperty(dev, DIPROP_APPDATA, &pp.diph);
            CHECKF(hr == DI_OK && pp.uData == 7, "keyboard app data reads back (%#lx, %#llx)", hr, (unsigned long long)pp.uData);
        }
        IDirectInputDevice8_Release(dev);
    }
}

static void test_pad(IDirectInput8W *di8, const struct found *v8, const struct found *v7)
{
    IDirectInputDevice8W *dev;
    DIDEVICEINSTANCEW devinst = {sizeof(devinst)};
    DIPROPDWORD pd;
    DIPROPSTRING ps;
    DIPROPPOINTER pp = {{sizeof(DIPROPPOINTER), sizeof(DIPROPHEADER), MAKELONG(HID_USAGE_GENERIC_X, HID_USAGE_PAGE_GENERIC),
                         DIPH_BYUSAGE}, 0};
    HRESULT hr;

    if (!v8->have_pad) { printf("note  no HID game controller present: pad checks skipped\n"); return; }
    printf("note  game controller: %ls\n", v8->pad.tszProductName);
    hr = IDirectInput8_CreateDevice(di8, &v8->pad.guidInstance, &dev, NULL);
    CHECKF(hr == S_OK, "CreateDevice(pad) (%#lx)", hr);
    if (hr != S_OK) return;

    pd = dword_prop(DIPH_DEVICE, 0, 0xdeadbeef);
    hr = IDirectInputDevice8_GetProperty(dev, DIPROP_JOYSTICKID, &pd.diph);
    CHECKF(hr == DI_OK && pd.dwData == 0, "pad joystick id is its position in the enumeration, 0 (%#lx, %lu)", hr, pd.dwData);
    pd.dwData = 0xdeadbeef;
    hr = IDirectInputDevice8_GetProperty(dev, DIPROP_AXISMODE, &pd.diph);
    CHECKF(hr == DI_OK && pd.dwData == DIPROPAXISMODE_ABS, "pad default axis mode is absolute (%#lx, %lu)", hr, pd.dwData);
    hr = IDirectInputDevice8_GetProperty(dev, DIPROP_APPDATA, &pp.diph);
    CHECKF(hr == DIERR_NOTINITIALIZED, "pad app data before a data format is DIERR_NOTINITIALIZED (%#lx)", hr);
    pd.dwData = DIPROPAXISMODE_REL;
    hr = IDirectInputDevice8_SetProperty(dev, DIPROP_AXISMODE, &pd.diph);
    CHECKF(hr == DI_OK, "pad SetProperty AXISMODE (%#lx)", hr);
    pd.dwData = 0xdeadbeef;
    hr = IDirectInputDevice8_GetProperty(dev, DIPROP_AXISMODE, &pd.diph);
    CHECKF(hr == DI_OK && pd.dwData == DIPROPAXISMODE_REL, "pad axis mode reads back relative (%#lx, %lu)", hr, pd.dwData);
    hr = IDirectInputDevice8_SetProperty(dev, DIPROP_JOYSTICKID, &pd.diph);
    CHECKF(hr == DIERR_UNSUPPORTED, "pad joystick id cannot be set (%#lx)", hr);
    pd.dwData = 0xdeadbeef;
    hr = IDirectInputDevice8_SetProperty(dev, DIPROP_CALIBRATION, &pd.diph);
    CHECKF(hr == DIERR_INVALIDPARAM, "pad SetProperty CALIBRATION is DIERR_INVALIDPARAM (%#lx)", hr);

    ps = string_prop(L"");
    hr = IDirectInputDevice8_GetProperty(dev, DIPROP_TYPENAME, &ps.diph);
    CHECKF(hr == DI_OK && !lstrcmpW(ps.wsz, L"VID_045E&PID_028E"), "pad type name is VID_045E&PID_028E (%#lx, %ls)", hr, ps.wsz);
    ps = string_prop(L"product name");
    hr = IDirectInputDevice8_SetProperty(dev, DIPROP_PRODUCTNAME, &ps.diph);
    CHECKF(hr == DI_OK, "pad SetProperty PRODUCTNAME is accepted (%#lx)", hr);
    ps = string_prop(L"");
    hr = IDirectInputDevice8_GetProperty(dev, DIPROP_PRODUCTNAME, &ps.diph);
    CHECKF(hr == DI_OK && !lstrcmpW(ps.wsz, v8->pad.tszProductName), "... and the product name does not change (%#lx, %ls)", hr, ps.wsz);
    ps = string_prop(L"junk");
    hr = IDirectInputDevice8_GetProperty(dev, DIPROP_USERNAME, &ps.diph);
    CHECKF(hr == DI_NOEFFECT && !ps.wsz[0], "pad user name is empty with DI_NOEFFECT (%#lx, %ls)", hr, ps.wsz);
    hr = IDirectInputDevice8_GetDeviceInfo(dev, &devinst);
    CHECKF(hr == S_OK && GET_DIDEVICE_TYPE(devinst.dwDevType) == DI8DEVTYPE_GAMEPAD, "pad is a gamepad (%#lx)", devinst.dwDevType);
    IDirectInputDevice8_Release(dev);

    CHECKF(v7->have_pad && v7->pad.dwDevType == (DIDEVTYPE_HID | (DIDEVTYPEJOYSTICK_GAMEPAD << 8) | DIDEVTYPE_JOYSTICK),
           "version 0x700 reports the pad as DIDEVTYPEJOYSTICK_GAMEPAD (%#lx)", v7->pad.dwDevType);
}

int main(void)
{
    HINSTANCE inst = GetModuleHandleW(NULL);
    IDirectInput8W *di8;
    struct found v7, v8;
    HRESULT hr;

    CoInitialize(NULL);
    memset(&v7, 0, sizeof(v7));
    memset(&v8, 0, sizeof(v8));
    test_legacy_instances(inst, &v7);

    hr = DirectInput8Create(inst, DIRECTINPUT_VERSION, &IID_IDirectInput8W, (void **)&di8, NULL);
    CHECKF(hr == S_OK, "DirectInput8Create (%#lx)", hr);
    if (hr == S_OK)
    {
        hr = IDirectInput8_EnumDevices(di8, DI8DEVCLASS_ALL, collect, &v8, DIEDFL_ATTACHEDONLY);
        CHECKF(hr == S_OK, "EnumDevices (%#lx)", hr);
        test_mouse_keyboard(di8, &v8, inst);
        test_pad(di8, &v8, &v7);
        IDirectInput8_Release(di8);
    }
    printf("%s\n", failures ? "RESULT: FAIL" : "RESULT: PASS");
    return failures != 0;
}
