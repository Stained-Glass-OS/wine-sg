/* dinput's FIXME stubs (patches/sg/2836), run by test/dinput-misc-gate.sh:
 * IDirectInput7::FindDevice, IDirectInputDevice8::Initialize / RunControlPanel /
 * GetImageInfo and the class factory's LockServer. The log is also checked by
 * the gate: none of them may log a FIXME. Uses the system keyboard and mouse,
 * which exist on any host.
 *
 *   dinput-misc-probe.exe */
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

static void test_find_device(HINSTANCE inst)
{
    HRESULT (WINAPI *create_ex)(HINSTANCE, DWORD, REFIID, void **, IUnknown *) =
        (void *)GetProcAddress(LoadLibraryA("dinput.dll"), "DirectInputCreateExA");
    HRESULT (WINAPI *create_exw)(HINSTANCE, DWORD, REFIID, void **, IUnknown *) =
        (void *)GetProcAddress(GetModuleHandleA("dinput.dll"), "DirectInputCreateEx");
    void *di7 = NULL;
    struct names n;
    GUID out;
    HRESULT hr;

    (void)create_ex;
    check(create_exw != NULL, "dinput exports DirectInputCreateEx");
    if (!create_exw) return;
    hr = create_exw(inst, 0x0700, &IID_IDirectInput7W_sg, &di7, NULL);
    CHECKF(hr == S_OK, "DirectInputCreateEx(IDirectInput7W) (%#lx)", hr);
    if (hr != S_OK) return;

    memset(&n, 0, sizeof(n));
    hr = ((enum_fn)VT(di7)[4])(di7, 0, collect, &n, DIEDFL_ATTACHEDONLY);
    CHECKF(hr == S_OK && n.keyboard_inst[0] && n.mouse_inst[0], "EnumDevices finds the keyboard and mouse (%#lx)", hr);

    memset(&out, 0xaa, sizeof(out));
    hr = ((find_fn)VT(di7)[8])(di7, &GUID_SysKeyboard, n.keyboard_inst, &out);
    CHECKF(hr == DI_OK && IsEqualGUID(&out, &n.keyboard_guid), "FindDevice finds the keyboard by its instance name (%#lx)", hr);
    memset(&out, 0xaa, sizeof(out));
    hr = ((find_fn)VT(di7)[8])(di7, &GUID_SysKeyboard, n.keyboard_prod, &out);
    CHECKF(hr == DI_OK && IsEqualGUID(&out, &n.keyboard_guid), "... or by its product name (%#lx)", hr);
    {
        WCHAR upper[MAX_PATH];
        lstrcpyW(upper, n.keyboard_inst);
        CharUpperW(upper);
        memset(&out, 0xaa, sizeof(out));
        hr = ((find_fn)VT(di7)[8])(di7, &GUID_SysKeyboard, upper, &out);
        CHECKF(hr == DI_OK && IsEqualGUID(&out, &n.keyboard_guid), "the name is compared without case (%#lx)", hr);
    }
    memset(&out, 0xaa, sizeof(out));
    hr = ((find_fn)VT(di7)[8])(di7, &GUID_SysMouse, n.mouse_inst, &out);
    CHECKF(hr == DI_OK && IsEqualGUID(&out, &n.mouse_guid), "FindDevice finds the mouse (%#lx)", hr);
    hr = ((find_fn)VT(di7)[8])(di7, &GUID_SysMouse, n.keyboard_inst, &out);
    CHECKF(hr == DIERR_DEVICENOTREG, "the keyboard's name is not a mouse (%#lx)", hr);
    hr = ((find_fn)VT(di7)[8])(di7, &GUID_SysKeyboard, L"No Such Device", &out);
    CHECKF(hr == DIERR_DEVICENOTREG, "an unknown name is DIERR_DEVICENOTREG (%#lx)", hr);
    hr = ((find_fn)VT(di7)[8])(di7, &IID_IUnknown, n.keyboard_inst, &out);
    CHECKF(hr == DIERR_DEVICENOTREG, "an unknown class is DIERR_DEVICENOTREG (%#lx)", hr);
    hr = ((find_fn)VT(di7)[8])(di7, &GUID_SysKeyboard, NULL, &out);
    CHECKF(hr == E_POINTER, "a NULL name is E_POINTER (%#lx)", hr);
    hr = ((find_fn)VT(di7)[8])(di7, &GUID_SysKeyboard, n.keyboard_inst, NULL);
    CHECKF(hr == E_POINTER, "a NULL output is E_POINTER (%#lx)", hr);
    IUnknown_Release((IUnknown *)di7);
}

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

    hr = IDirectInputDevice8_RunControlPanel(dev, (HWND)0xdeadbeef, 0);
    CHECKF(hr == E_HANDLE, "RunControlPanel with a bad window is E_HANDLE (%#lx)", hr);
    hr = IDirectInputDevice8_RunControlPanel(dev, NULL, ~0u);
    CHECKF(hr == DIERR_INVALIDPARAM, "RunControlPanel with flags is DIERR_INVALIDPARAM (%#lx)", hr);
    hr = IDirectInputDevice8_RunControlPanel(dev, NULL, 0);
    CHECKF(hr == DI_OK, "RunControlPanel(NULL, 0) is DI_OK (%#lx)", hr);

    memset(&header, 0, sizeof(header));
    header.dwSize = sizeof(header);
    header.dwSizeImageInfo = sizeof(DIDEVICEIMAGEINFOW);
    header.dwcViews = 9;
    header.dwBufferUsed = 9;
    hr = IDirectInputDevice8_GetImageInfo(dev, &header);
    CHECKF(hr == DI_OK && header.dwcViews == 0 && header.dwBufferUsed == 0 && header.dwcButtons >= 0 && header.dwcAxes == 0,
           "GetImageInfo: no views for a keyboard, no axes (%#lx, %lu views, %lu axes)", hr, header.dwcViews, header.dwcAxes);
    header.dwSize = 1;
    hr = IDirectInputDevice8_GetImageInfo(dev, &header);
    CHECKF(hr == DIERR_INVALIDPARAM, "GetImageInfo with a wrong size is DIERR_INVALIDPARAM (%#lx)", hr);
    hr = IDirectInputDevice8_GetImageInfo(dev, NULL);
    CHECKF(hr == E_POINTER, "GetImageInfo(NULL) is E_POINTER (%#lx)", hr);

    IDirectInputDevice8_Release(dev);
    IDirectInput8_Release(di8);
}

static void test_factory(void)
{
    HRESULT (WINAPI *get_class_object)(REFCLSID, REFIID, void **) =
        (void *)GetProcAddress(GetModuleHandleA("dinput8.dll"), "DllGetClassObject");
    IClassFactory *factory = NULL;
    HRESULT hr;

    if (get_class_object && get_class_object(&CLSID_DirectInput8_sg, &IID_IClassFactory, (void **)&factory) == S_OK)
    {
        hr = IClassFactory_LockServer(factory, TRUE);
        CHECKF(hr == S_OK, "LockServer(TRUE) is S_OK (%#lx)", hr);
        hr = IClassFactory_LockServer(factory, FALSE);
        CHECKF(hr == S_OK, "LockServer(FALSE) is S_OK (%#lx)", hr);
        IClassFactory_Release(factory);
    }
    else printf("note  no dinput8 class factory: LockServer not checked\n");
}

int main(void)
{
    HINSTANCE inst = GetModuleHandleW(NULL);

    CoInitialize(NULL);
    test_find_device(inst);
    test_device(inst);
    test_factory();
    printf("%s\n", failures ? "RESULT: FAIL" : "RESULT: PASS");
    return failures != 0;
}
