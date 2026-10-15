#include <windows.h>
#include <bluetoothapis.h>
#include <stdio.h>
#include <string.h>
static int failed;
#define CHECK(c, ...) do { if (!(c)) { printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); failed++; } } while (0)

static const GUID a2dp = { 0x0000110b, 0x0000, 0x1000, { 0x80, 0x00, 0x00, 0x80, 0x5f, 0x9b, 0x34, 0xfb } };
static const GUID hfp  = { 0x0000111e, 0x0000, 0x1000, { 0x80, 0x00, 0x00, 0x80, 0x5f, 0x9b, 0x34, 0xfb } };
static const GUID other = { 0x00001101, 0x0000, 0x1000, { 0x80, 0x00, 0x00, 0x80, 0x5f, 0x9b, 0x34, 0xfb } };

static int count_devices(BOOL auth, BOOL remembered, BOOL unknown, BOOL connected, BLUETOOTH_DEVICE_INFO *first)
{
    BLUETOOTH_DEVICE_SEARCH_PARAMS p = { sizeof(p), auth, remembered, unknown, connected, FALSE, 1, NULL };
    BLUETOOTH_DEVICE_INFO info = { sizeof(info) };
    HBLUETOOTH_DEVICE_FIND find;
    int n = 0;

    find = BluetoothFindFirstDevice(&p, &info);
    if (!find) { CHECK(GetLastError() == ERROR_NO_MORE_ITEMS, "no device error %lu", GetLastError()); return 0; }
    do { if (n == 0 && first) *first = info; n++; } while (BluetoothFindNextDevice(find, &info));
    CHECK(GetLastError() == ERROR_NO_MORE_ITEMS, "end error %lu", GetLastError());
    CHECK(BluetoothFindDeviceClose(find), "close");
    return n;
}

int main(void)
{
    BLUETOOTH_FIND_RADIO_PARAMS rp = { sizeof(rp) };
    BLUETOOTH_DEVICE_SEARCH_PARAMS bad = { sizeof(bad) - 4 };
    BLUETOOTH_DEVICE_INFO info = { sizeof(info) }, first, headset = { sizeof(headset) };
    HBLUETOOTH_RADIO_FIND rfind;
    HANDLE radio;
    GUID guids[8];
    DWORD ret, n;
    BLUETOOTH_ADDRESS addr;
    int i;

    rfind = BluetoothFindFirstRadio(&rp, &radio);
    CHECK(rfind != NULL, "no radio, error %lu", GetLastError());
    if (!rfind) { printf("RESULT: FAIL\n"); return 1; }
    BluetoothFindRadioClose(rfind);

    CHECK(!BluetoothFindFirstDevice(&bad, &info) && GetLastError() == ERROR_INVALID_PARAMETER, "bad params");
    CHECK(!BluetoothFindFirstDevice(NULL, &info) && GetLastError() == ERROR_INVALID_PARAMETER, "null params");

    CHECK(count_devices(TRUE, TRUE, TRUE, TRUE, &first) == 2, "all devices");
    CHECK(count_devices(TRUE, FALSE, FALSE, FALSE, &first) == 1, "authenticated");
    CHECK(first.Address.ullLong == 0xAABBCCDDEE01ull, "address %I64x", first.Address.ullLong);
    CHECK(!wcscmp(first.szName, L"Fake Headset"), "name %ls", first.szName);
    CHECK(first.fConnected && first.fAuthenticated && first.fRemembered, "flags %d %d %d", first.fConnected, first.fAuthenticated, first.fRemembered);
    CHECK(first.ulClassofDevice == 0x240404, "class %lx", first.ulClassofDevice);
    CHECK(count_devices(FALSE, FALSE, TRUE, FALSE, &first) == 1, "unknown");
    CHECK(first.Address.ullLong == 0x112233445566ull && !wcscmp(first.szName, L"Scanned Phone") && !first.fConnected && !first.fRemembered,
          "unknown device %I64x %ls", first.Address.ullLong, first.szName);
    CHECK(count_devices(FALSE, FALSE, FALSE, TRUE, NULL) == 1, "connected");
    CHECK(count_devices(FALSE, FALSE, FALSE, FALSE, NULL) == 0, "none asked for");

    /* the radio given, and not */
    {
        BLUETOOTH_DEVICE_SEARCH_PARAMS p = { sizeof(p), TRUE, TRUE, TRUE, TRUE, FALSE, 1, radio };
        HBLUETOOTH_DEVICE_FIND find = BluetoothFindFirstDevice(&p, &info);
        CHECK(find != NULL, "with the radio");
        if (find) BluetoothFindDeviceClose(find);
    }

    addr.ullLong = 0xAABBCCDDEE01ull;
    memset(&headset, 0, sizeof(headset)); headset.dwSize = sizeof(headset); headset.Address = addr;
    ret = BluetoothGetDeviceInfo(NULL, &headset);
    CHECK(ret == ERROR_SUCCESS && !wcscmp(headset.szName, L"Fake Headset") && headset.fConnected, "get info %lu %ls", ret, headset.szName);
    ret = BluetoothGetDeviceInfo(radio, &headset);
    CHECK(ret == ERROR_SUCCESS, "get info with radio %lu", ret);
    info.dwSize = sizeof(info); info.Address.ullLong = 0x010203040506ull;
    ret = BluetoothGetDeviceInfo(NULL, &info);
    CHECK(ret == ERROR_NOT_FOUND, "unknown address %lu", ret);
    info.dwSize = 3;
    ret = BluetoothGetDeviceInfo(NULL, &info);
    CHECK(ret == ERROR_INVALID_PARAMETER, "bad size %lu", ret);

    n = 8;
    ret = BluetoothEnumerateInstalledServices(NULL, &headset, &n, guids);
    CHECK(ret == ERROR_SUCCESS && n == 2 && IsEqualGUID(&guids[0], &a2dp) && IsEqualGUID(&guids[1], &hfp), "services %lu n %lu", ret, n);
    n = 1;
    ret = BluetoothEnumerateInstalledServices(NULL, &headset, &n, guids);
    CHECK(ret == ERROR_MORE_DATA && n == 2, "services small %lu n %lu", ret, n);
    n = 0;
    ret = BluetoothEnumerateInstalledServices(NULL, &headset, &n, NULL);
    CHECK(ret == ERROR_SUCCESS && n == 2, "services count %lu n %lu", ret, n);

    ret = BluetoothSetServiceState(radio, &headset, (GUID *)&a2dp, BLUETOOTH_SERVICE_ENABLE);
    CHECK(ret == ERROR_SUCCESS, "enable %lu", ret);
    ret = BluetoothSetServiceState(NULL, &headset, (GUID *)&hfp, BLUETOOTH_SERVICE_DISABLE);
    CHECK(ret == ERROR_SUCCESS, "disable %lu", ret);
    ret = BluetoothSetServiceState(NULL, &headset, (GUID *)&other, BLUETOOTH_SERVICE_ENABLE);
    CHECK(ret == ERROR_SERVICE_DOES_NOT_EXIST, "no such service %lu", ret);
    ret = BluetoothSetServiceState(NULL, &headset, (GUID *)&a2dp, 7);
    CHECK(ret == ERROR_INVALID_PARAMETER, "bad flags %lu", ret);

    wcscpy(headset.szName, L"Renamed Headset");
    ret = BluetoothUpdateDeviceRecord(&headset);
    CHECK(ret == ERROR_SUCCESS, "update %lu", ret);
    memset(&info, 0, sizeof(info)); info.dwSize = sizeof(info); info.Address = addr;
    ret = BluetoothGetDeviceInfo(NULL, &info);
    CHECK(ret == ERROR_SUCCESS && !wcscmp(info.szName, L"Renamed Headset"), "renamed %lu %ls", ret, info.szName);

    CHECK(BluetoothEnableDiscovery(radio, TRUE), "enable discovery %lu", GetLastError());
    CHECK(BluetoothEnableDiscovery(NULL, FALSE), "disable discovery %lu", GetLastError());

    addr.ullLong = 0x112233445566ull;
    ret = BluetoothRemoveDevice(&addr);
    CHECK(ret == ERROR_SUCCESS, "remove %lu", ret);
    CHECK(count_devices(FALSE, FALSE, TRUE, FALSE, NULL) == 0, "removed device is gone");
    ret = BluetoothRemoveDevice(&addr);
    CHECK(ret == ERROR_NOT_FOUND, "remove again %lu", ret);
    ret = BluetoothRemoveDevice(NULL);
    CHECK(ret == ERROR_INVALID_PARAMETER, "remove null %lu", ret);

    {
        BOOL (WINAPI *is_version)(UCHAR, UCHAR) = (void *)GetProcAddress(GetModuleHandleA("bluetoothapis.dll"), "BluetoothIsVersionAvailable");
        CHECK(is_version != NULL, "no BluetoothIsVersionAvailable");
        if (is_version)
        {
            CHECK(is_version(2, 1), "2.1 is available");
            CHECK(is_version(4, 0), "4.0 is available");
            CHECK(!is_version(4, 1), "4.1 is not available");
            CHECK(!is_version(5, 0), "5.0 is not available");
        }
    }
    CloseHandle(radio);
    for (i = 0; i < 0; i++) ;
    printf(failed ? "RESULT: FAIL\n" : "RESULT: PASS\n");
    return failed;
}
