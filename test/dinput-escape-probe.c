/* dinput's IDirectInputDevice8::Escape and SendDeviceData (patches/sg/2839), run
 * by test/dinput-escape-gate.sh on the system keyboard (no force feedback): the
 * results are those dinput's conformance tests record from native. The gate
 * fails when either logs a FIXME.
 *
 *   dinput-escape-probe.exe */
#define COBJMACROS
#define DIRECTINPUT_VERSION 0x0800
#include <windows.h>
#include <initguid.h>
#include <dinput.h>
#include <stdio.h>
#include <string.h>

DEFINE_GUID(IID_IDirectInput7W_sg, 0x9a4cb685, 0x236d, 0x11d3, 0x8e, 0x9d, 0x00, 0xc0, 0x4f, 0x68, 0x44, 0xae);
DEFINE_GUID(CLSID_DirectInput8_sg, 0x25e609e4, 0xb259, 0x11cf, 0xbf, 0xc7, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00);

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}
#define CHECKF(ok, ...) do { char _b[256]; snprintf(_b, sizeof(_b), __VA_ARGS__); check(ok, _b); } while (0)

struct names { WCHAR keyboard_inst[MAX_PATH], keyboard_prod[MAX_PATH], mouse_inst[MAX_PATH]; GUID keyboard_guid, mouse_guid; };

static BOOL CALLBACK collect(const DIDEVICEINSTANCEW *inst, void *ctx)
{
    struct names *n = ctx;
    if (IsEqualGUID(&inst->guidInstance, &GUID_SysKeyboard))
    {
        lstrcpyW(n->keyboard_inst, inst->tszInstanceName);
        lstrcpyW(n->keyboard_prod, inst->tszProductName);
        n->keyboard_guid = inst->guidInstance;
    }
    else if (IsEqualGUID(&inst->guidInstance, &GUID_SysMouse))
    {
        lstrcpyW(n->mouse_inst, inst->tszInstanceName);
        n->mouse_guid = inst->guidInstance;
    }
    return DIENUM_CONTINUE;
}

typedef HRESULT (WINAPI *find_fn)(void *, const GUID *, const WCHAR *, GUID *);
typedef HRESULT (WINAPI *enum_fn)(void *, DWORD, LPDIENUMDEVICESCALLBACKW, void *, DWORD);
typedef HRESULT (WINAPI *init_fn)(void *, HINSTANCE, DWORD);
#define VT(o) (*(void ***)(o))

static void test_device(HINSTANCE inst)
{
    IDirectInput8W *di8;
    IDirectInputDevice8W *dev;
    DIDEVICEIMAGEINFOHEADERW header;
    HRESULT hr;

    hr = DirectInput8Create(inst, DIRECTINPUT_VERSION, &IID_IDirectInput8W, (void **)&di8, NULL);
    CHECKF(hr == S_OK, "DirectInput8Create (%#lx)", hr);
    if (hr != S_OK) return;
    hr = IDirectInput8_CreateDevice(di8, &GUID_SysKeyboard, &dev, NULL);
    CHECKF(hr == S_OK, "CreateDevice(keyboard) (%#lx)", hr);
    if (hr != S_OK) return;

    /* ground truth: dinput's tests joystick8.c / device8.c (todo_wine before) */
    hr = IDirectInputDevice8_Initialize(dev, inst, 0, &GUID_SysKeyboard);
    CHECKF(hr == DIERR_NOTINITIALIZED, "Initialize with version 0 is DIERR_NOTINITIALIZED (%#lx)", hr);
    hr = IDirectInputDevice8_Initialize(dev, inst, DIRECTINPUT_VERSION - 1, &GUID_NULL);
    CHECKF(hr == DIERR_BETADIRECTINPUTVERSION, "Initialize with an older version is DIERR_BETADIRECTINPUTVERSION (%#lx)", hr);
    hr = IDirectInputDevice8_Initialize(dev, inst, DIRECTINPUT_VERSION + 1, &GUID_NULL);
    CHECKF(hr == DIERR_OLDDIRECTINPUTVERSION, "Initialize with a newer version is DIERR_OLDDIRECTINPUTVERSION (%#lx)", hr);
    hr = IDirectInputDevice8_Initialize(dev, inst, DIRECTINPUT_VERSION, NULL);
    CHECKF(hr == E_POINTER, "Initialize with no guid is E_POINTER (%#lx)", hr);
    hr = IDirectInputDevice8_Initialize(dev, NULL, DIRECTINPUT_VERSION, &GUID_NULL);
    CHECKF(hr == DIERR_INVALIDPARAM, "Initialize with no instance is DIERR_INVALIDPARAM (%#lx)", hr);
    hr = IDirectInputDevice8_Initialize(dev, inst, DIRECTINPUT_VERSION, &GUID_NULL);
    CHECKF(hr == REGDB_E_CLASSNOTREG, "Initialize as GUID_NULL is REGDB_E_CLASSNOTREG (%#lx)", hr);
    hr = IDirectInputDevice8_Initialize(dev, inst, DIRECTINPUT_VERSION, &GUID_SysMouse);
    CHECKF(hr == REGDB_E_CLASSNOTREG, "a keyboard is not initialized as the mouse (%#lx)", hr);
    hr = IDirectInputDevice8_Initialize(dev, inst, DIRECTINPUT_VERSION, &GUID_SysKeyboardEm);
    CHECKF(hr == DI_OK, "Initialize as the emulated keyboard is DI_OK (%#lx)", hr);
    hr = IDirectInputDevice8_Initialize(dev, inst, DIRECTINPUT_VERSION, &GUID_SysKeyboard);
    CHECKF(hr == DI_OK, "Initialize as the keyboard again is DI_OK (%#lx)", hr);

    {
        DIEFFESCAPE escape;
        DIDEVICEOBJECTDATA objdata = {0};
        DWORD res = 1;
        char buffer[20] = {0};

        hr = IDirectInputDevice8_Escape(dev, NULL);
        CHECKF(hr == E_POINTER, "Escape(NULL) is E_POINTER (%#lx)", hr);
        memset(&escape, 0, sizeof(escape));
        hr = IDirectInputDevice8_Escape(dev, &escape);
        CHECKF(hr == DIERR_INVALIDPARAM, "Escape with a zero size is DIERR_INVALIDPARAM (%#lx)", hr);
        escape.dwSize = sizeof(escape) + 1;
        hr = IDirectInputDevice8_Escape(dev, &escape);
        CHECKF(hr == DIERR_INVALIDPARAM, "Escape with a too large size is DIERR_INVALIDPARAM (%#lx)", hr);
        escape.dwSize = sizeof(escape);
        escape.lpvInBuffer = buffer;
        escape.cbInBuffer = 10;
        escape.lpvOutBuffer = buffer + 10;
        escape.cbOutBuffer = 10;
        hr = IDirectInputDevice8_Escape(dev, &escape);
        CHECKF(hr == DIERR_UNSUPPORTED, "Escape on a device without force feedback is DIERR_UNSUPPORTED (%#lx)", hr);

        objdata.dwOfs = 0xd;
        objdata.dwData = 0x80;
        hr = IDirectInputDevice8_SendDeviceData(dev, sizeof(objdata), &objdata, &res, 0xdeadbeef);
        CHECKF(hr == DIERR_INVALIDPARAM, "SendDeviceData with unknown flags is DIERR_INVALIDPARAM (%#lx)", hr);
        hr = IDirectInputDevice8_SendDeviceData(dev, sizeof(objdata), &objdata, &res, 1);
        CHECKF(hr == DIERR_INVALIDPARAM, "SendDeviceData(DISDD_CONTINUE) on a keyboard is DIERR_INVALIDPARAM (%#lx)", hr);
        hr = IDirectInputDevice8_SendDeviceData(dev, sizeof(objdata), &objdata, &res, 0);
        CHECKF(hr == DIERR_INVALIDPARAM, "SendDeviceData on a keyboard is DIERR_INVALIDPARAM (%#lx)", hr);
    }

    IDirectInputDevice8_Release(dev);
    IDirectInput8_Release(di8);
}

int main(void)
{
    HINSTANCE inst = GetModuleHandleW(NULL);

    CoInitialize(NULL);
    test_device(inst);
    printf("%s\n", failures ? "RESULT: FAIL" : "RESULT: PASS");
    return failures != 0;
}
