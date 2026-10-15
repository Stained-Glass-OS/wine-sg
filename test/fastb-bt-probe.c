/* bluetoothapis, windows.devices.bluetooth and windows.devices.usb on a machine without radio or USB devices */
#define COBJMACROS
#include <windows.h>
#include <roapi.h>
#include <winstring.h>
#include <stdio.h>
#include <string.h>

static int failures;
#define CHECK(c, ...) do { if (!(c)) { failures++; printf("FAIL  %s:%d  ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

typedef void *OBJ;
#define SLOT(o, n) ((*(void ***)(o))[n])
#define CALL(o, n, ret, ...) ((ret (WINAPI *)())SLOT(o, n))(o, ##__VA_ARGS__)

static const GUID IID_IActivationFactory_ = {0x00000035,0x0000,0x0000,{0xc0,0x00,0x00,0x00,0x00,0x00,0x00,0x46}};
static const GUID IID_IAsyncInfo_ = {0x00000036,0x0000,0x0000,{0xc0,0x00,0x00,0x00,0x00,0x00,0x00,0x46}};
static const GUID IID_IBluetoothAdapterStatics_ = {0x8b02fb6a,0xac4c,0x4741,{0x86,0x61,0x8e,0xab,0x7d,0x17,0xea,0x9f}};
static const GUID IID_IUsbDeviceStatics_ = {0x066b85a2,0x09b7,0x4446,{0x85,0x02,0x6f,0xe6,0xdc,0xaa,0x73,0x09}};
static const GUID guid_winusb = {0xdee824ef,0x729b,0x4a0e,{0x9c,0x14,0xb7,0x11,0x7d,0x33,0xa8,0x17}};

static OBJ get_statics(const WCHAR *cls, const GUID *iid)
{
    HSTRING s;
    OBJ st = NULL;
    HRESULT hr;

    WindowsCreateString(cls, wcslen(cls), &s);
    hr = RoGetActivationFactory(s, iid, (void **)&st);
    WindowsDeleteString(s);
    CHECK(hr == S_OK, "factory %ls: %08lx", cls, hr);
    return st;
}

/* the operation must be complete at once; returns the result object (or NULL) */
static OBJ op_result(OBJ op, const char *what)
{
    OBJ info = NULL, result = (OBJ)1;
    int status = -1;
    HRESULT hr = CALL(op, 0, HRESULT, &IID_IAsyncInfo_, &info);

    CHECK(hr == S_OK, "%s: IAsyncInfo %08lx", what, hr);
    if (hr == S_OK)
    {
        CALL(info, 7, HRESULT, &status);
        CHECK(status == 1, "%s: status %d", what, status);
        CALL(info, 2, ULONG);
    }
    hr = CALL(op, 8, HRESULT, &result);
    CHECK(hr == S_OK, "%s: GetResults %08lx", what, hr);
    return result;
}

static void check_str(HSTRING s, const WCHAR *expected, const char *what)
{
    const WCHAR *buf = WindowsGetStringRawBuffer(s, NULL);
    CHECK(buf && !wcscmp(buf, expected), "%s: got %ls", what, buf ? buf : L"(null)");
}

int main(void)
{
    HMODULE bt = LoadLibraryA("bluetoothapis.dll");
    DWORD (WINAPI *get_radio_info)(HANDLE, void *) = (void *)GetProcAddress(bt, "BluetoothGetRadioInfo");
    HANDLE (WINAPI *find_first)(void *, void *) = (void *)GetProcAddress(bt, "BluetoothFindFirstDevice");
    BOOL (WINAPI *find_next)(HANDLE, void *) = (void *)GetProcAddress(bt, "BluetoothFindNextDevice");
    BOOL (WINAPI *find_close)(HANDLE) = (void *)GetProcAddress(bt, "BluetoothFindDeviceClose");
    DWORD (WINAPI *get_device_info)(HANDLE, void *) = (void *)GetProcAddress(bt, "BluetoothGetDeviceInfo");
    BOOL (WINAPI *is_connectable)(HANDLE) = (void *)GetProcAddress(bt, "BluetoothIsConnectable");
    BOOL (WINAPI *is_discoverable)(HANDLE) = (void *)GetProcAddress(bt, "BluetoothIsDiscoverable");
    HANDLE (WINAPI *find_first_radio)(void *, HANDLE *) = (void *)GetProcAddress(bt, "BluetoothFindFirstRadio");
    BOOL no_radio;
    char radio_info[1024], dev_info[1024], params[64];
    HANDLE h, radio;
    DWORD ret;
    OBJ st, op, res;
    HSTRING str, out;
    HRESULT hr;
    int i;

    RoInitialize(RO_INIT_MULTITHREADED);
    CHECK(bt && get_radio_info && find_first && find_next && find_close && get_device_info && is_connectable && is_discoverable,
          "exports");
    if (!bt) goto end;

    {
        DWORD fp[1] = { 4 };   /* BLUETOOTH_FIND_RADIO_PARAMS: dwSize only */
        h = find_first_radio(fp, &radio);
        no_radio = !h;
        if (h) CloseHandle(radio);
    }

    /* radio info */
    memset(radio_info, 0, sizeof(radio_info));
    *(DWORD *)radio_info = 520;   /* sizeof(BLUETOOTH_RADIO_INFO) */
    ret = get_radio_info(NULL, NULL);
    CHECK(ret == ERROR_INVALID_PARAMETER, "NULL info: %lu", ret);
    *(DWORD *)radio_info = 3;
    ret = get_radio_info(NULL, radio_info);
    CHECK(ret == ERROR_REVISION_MISMATCH, "wrong size: %lu", ret);
    *(DWORD *)radio_info = 520;
    ret = get_radio_info(NULL, radio_info);
    CHECK(ret == ERROR_INVALID_HANDLE, "NULL radio: %lu", ret);
    ret = get_radio_info((HANDLE)0x1234, radio_info);
    CHECK(ret == ERROR_INVALID_HANDLE, "bad radio: %lu", ret);
    if (no_radio)
    {
        SetLastError(0xdeadbeef);
        CHECK(!is_connectable(NULL), "no radio: not connectable");
        CHECK(!is_discoverable(NULL), "no radio: not discoverable");
    }

    /* device search */
    memset(params, 0, sizeof(params));
    memset(dev_info, 0, sizeof(dev_info));
    *(DWORD *)params = 40;       /* sizeof(BLUETOOTH_DEVICE_SEARCH_PARAMS) on x64 */
    *(DWORD *)dev_info = 560;    /* sizeof(BLUETOOTH_DEVICE_INFO) */
    SetLastError(0xdeadbeef);
    h = find_first(NULL, dev_info);
    CHECK(!h && GetLastError() == ERROR_INVALID_PARAMETER, "NULL params: %lu", GetLastError());
    *(DWORD *)dev_info = 4;
    SetLastError(0xdeadbeef);
    h = find_first(params, dev_info);
    CHECK(!h && GetLastError() == ERROR_INVALID_PARAMETER, "bad info size: %lu", GetLastError());
    *(DWORD *)dev_info = 560;
    SetLastError(0xdeadbeef);
    h = find_first(params, dev_info);
    CHECK(!h && GetLastError() == ERROR_NO_MORE_ITEMS, "no device: %lu", GetLastError());
    SetLastError(0xdeadbeef);
    CHECK(!find_next(NULL, dev_info) && GetLastError() == ERROR_INVALID_HANDLE, "FindNext NULL: %lu", GetLastError());
    SetLastError(0xdeadbeef);
    CHECK(!find_close(NULL) && GetLastError() == ERROR_INVALID_HANDLE, "FindClose NULL: %lu", GetLastError());
    ret = get_device_info(NULL, NULL);
    CHECK(ret == ERROR_INVALID_PARAMETER, "GetDeviceInfo NULL: %lu", ret);
    ret = get_device_info(NULL, dev_info);
    CHECK(ret == ERROR_NOT_FOUND, "GetDeviceInfo unknown: %lu", ret);

    /* the WinRT adapter */
    st = get_statics(L"Windows.Devices.Bluetooth.BluetoothAdapter", &IID_IBluetoothAdapterStatics_);
    if (st)
    {
        hr = CALL(st, 8, HRESULT, &op);
        CHECK(hr == S_OK && op, "GetDefaultAsync %08lx", hr);
        if (op)
        {
            res = op_result(op, "GetDefaultAsync");
            if (no_radio) CHECK(res == NULL, "default adapter is null without radio");
            else CHECK(res != NULL, "default adapter with a radio");
            if (res) CALL(res, 2, ULONG);
            CALL(op, 2, ULONG);
        }
        WindowsCreateString(L"\\\\?\\no-such-bluetooth-radio", 27, &str);
        hr = CALL(st, 7, HRESULT, str, &op);
        CHECK(hr == S_OK && op, "FromIdAsync %08lx", hr);
        if (op)
        {
            res = op_result(op, "FromIdAsync");
            CHECK(res == NULL, "unknown id gives a null adapter");
            CALL(op, 2, ULONG);
        }
        WindowsDeleteString(str);
        hr = CALL(st, 7, HRESULT, NULL, NULL);
        CHECK(hr == E_POINTER, "FromIdAsync NULL operation %08lx", hr);
        CALL(st, 2, ULONG);
    }

    /* WinRT USB */
    st = get_statics(L"Windows.Devices.Usb.UsbDevice", &IID_IUsbDeviceStatics_);
    if (st)
    {
        GUID g = guid_winusb;
        hr = CALL(st, 7, HRESULT, g, &out);
        CHECK(hr == S_OK, "GuidOnly %08lx", hr);
        if (hr == S_OK)
        {
            check_str(out, L"System.Devices.InterfaceClassGuid:=\"{DEE824EF-729B-4A0E-9C14-B7117D33A817}\""
                           L" AND System.Devices.InterfaceEnabled:=System.StructuredQueryType.Boolean#True", "GuidOnly");
            WindowsDeleteString(out);
        }
        hr = CALL(st, 6, HRESULT, 0x1234u, 0x5678u, g, &out);
        CHECK(hr == S_OK, "GetDeviceSelector %08lx", hr);
        if (hr == S_OK)
        {
            check_str(out, L"System.Devices.InterfaceClassGuid:=\"{DEE824EF-729B-4A0E-9C14-B7117D33A817}\""
                           L" AND System.Devices.InterfaceEnabled:=System.StructuredQueryType.Boolean#True"
                           L" AND System.DeviceInterface.WinUsb.UsbVendorId:=4660"
                           L" AND System.DeviceInterface.WinUsb.UsbProductId:=22136", "GetDeviceSelector");
            WindowsDeleteString(out);
        }
        hr = CALL(st, 7, HRESULT, g, NULL);
        CHECK(hr == E_INVALIDARG, "GuidOnly NULL %08lx", hr);
        hr = CALL(st, 9, HRESULT, NULL, &out);
        CHECK(hr == E_INVALIDARG, "ClassSelector NULL %08lx", hr);
        WindowsCreateString(L"\\\\?\\usb#no-such-device", 21, &str);
        hr = CALL(st, 10, HRESULT, str, &op);
        CHECK(hr == S_OK && op, "UsbDevice.FromIdAsync %08lx", hr);
        if (op)
        {
            res = op_result(op, "UsbDevice.FromIdAsync");
            CHECK(res == NULL, "unknown id gives a null device");
            CALL(op, 2, ULONG);
        }
        WindowsDeleteString(str);
        CALL(st, 2, ULONG);
    }
    (void)i;
end:
    printf("%s\n", failures ? "RESULT: FAIL" : "RESULT: PASS");
    return failures != 0;
}
