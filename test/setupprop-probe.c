/* setupapi batch (patches/sg/2051), run by test/setupprop-gate.sh: registry
 * properties of a device (unknown property, deleting one by setting no data)
 * and SetupDiGetClassDevs with an interface class and an enumerator string
 * that is not the id of one device.
 *
 *   setupprop-probe.exe */
#include <windows.h>
#include <setupapi.h>
#include <stdio.h>
#include <string.h>

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}
#define CHECK(expr) check((expr), #expr)

static const GUID guid = { 0xdeadbeef, 0x3f65, 0x11db, { 0xb7, 0x04, 0x00, 0x11, 0x95, 0x5c, 0x2b, 0xdb } };

int main(void)
{
    SP_DEVINFO_DATA device = { sizeof(device) };
    HDEVINFO set, list;
    char buf[64] = "";
    WCHAR wbuf[64] = L"";
    DWORD size, err;
    BOOL ret;

    set = SetupDiGetClassDevsA(&guid, NULL, 0, DIGCF_DEVICEINTERFACE);
    CHECK(set != INVALID_HANDLE_VALUE);
    CHECK(SetupDiCreateDeviceInfoA(set, "LEGACY_BOGUS", &guid, NULL, NULL, DICD_GENERATE_ID, &device));

    /* an unknown property */
    SetLastError(0xdeadbeef);
    ret = SetupDiGetDeviceRegistryPropertyA(set, &device, -1, NULL, NULL, 0, NULL);
    CHECK(!ret && GetLastError() == ERROR_INVALID_REG_PROPERTY);
    SetLastError(0xdeadbeef);
    ret = SetupDiSetDeviceRegistryPropertyA(set, &device, -1, NULL, 0);
    CHECK(!ret && GetLastError() == ERROR_INVALID_REG_PROPERTY);
    SetLastError(0xdeadbeef);
    ret = SetupDiGetDeviceRegistryPropertyW(set, &device, -1, NULL, NULL, 0, NULL);
    CHECK(!ret && GetLastError() == ERROR_INVALID_REG_PROPERTY);
    SetLastError(0xdeadbeef);
    ret = SetupDiSetDeviceRegistryPropertyW(set, &device, -1, NULL, 0);
    CHECK(!ret && GetLastError() == ERROR_INVALID_REG_PROPERTY);

    /* no data deletes a property that is there, and fails when it is not */
    ret = SetupDiSetDeviceRegistryPropertyA(set, &device, SPDRP_FRIENDLYNAME, NULL, 0);
    CHECK(!ret);
    CHECK(SetupDiSetDeviceRegistryPropertyA(set, &device, SPDRP_FRIENDLYNAME, (BYTE *)"Bogus", sizeof("Bogus")));
    CHECK(SetupDiGetDeviceRegistryPropertyA(set, &device, SPDRP_FRIENDLYNAME, NULL, (BYTE *)buf, sizeof(buf), &size) && !strcmp(buf, "Bogus"));
    CHECK(SetupDiSetDeviceRegistryPropertyA(set, &device, SPDRP_FRIENDLYNAME, NULL, 0));
    SetLastError(0xdeadbeef);
    ret = SetupDiGetDeviceRegistryPropertyA(set, &device, SPDRP_FRIENDLYNAME, NULL, (BYTE *)buf, sizeof(buf), &size);
    CHECK(!ret && GetLastError() == ERROR_INVALID_DATA);
    CHECK(!SetupDiSetDeviceRegistryPropertyA(set, &device, SPDRP_FRIENDLYNAME, NULL, 0));

    ret = SetupDiSetDeviceRegistryPropertyW(set, &device, SPDRP_DEVICEDESC, NULL, 0);
    CHECK(!ret);
    CHECK(SetupDiSetDeviceRegistryPropertyW(set, &device, SPDRP_DEVICEDESC, (BYTE *)L"Wide", sizeof(L"Wide")));
    CHECK(SetupDiGetDeviceRegistryPropertyW(set, &device, SPDRP_DEVICEDESC, NULL, (BYTE *)wbuf, sizeof(wbuf), &size) && !wcscmp(wbuf, L"Wide"));
    CHECK(SetupDiSetDeviceRegistryPropertyW(set, &device, SPDRP_DEVICEDESC, NULL, 0));
    SetLastError(0xdeadbeef);
    ret = SetupDiGetDeviceRegistryPropertyW(set, &device, SPDRP_DEVICEDESC, NULL, (BYTE *)wbuf, sizeof(wbuf), &size);
    err = GetLastError();
    CHECK(!ret && err == ERROR_INVALID_DATA);
    SetupDiDestroyDeviceInfoList(set);

    /* an interface list needs the id of one device */
    SetLastError(0xdeadbeef);
    list = SetupDiGetClassDevsA(NULL, "ROOT", NULL, DIGCF_DEVICEINTERFACE | DIGCF_ALLCLASSES);
    CHECK(list == INVALID_HANDLE_VALUE && GetLastError() == ERROR_INVALID_DATA);
    SetLastError(0xdeadbeef);
    list = SetupDiGetClassDevsA(NULL, "ROOT\\LEGACY_BOGUS", NULL, DIGCF_DEVICEINTERFACE | DIGCF_ALLCLASSES);
    CHECK(list == INVALID_HANDLE_VALUE && GetLastError() == ERROR_INVALID_DATA);
    SetLastError(0xdeadbeef);
    list = SetupDiGetClassDevsA(&guid, "ROOT\\LEGACY_BOGUS", NULL, DIGCF_DEVICEINTERFACE);
    CHECK(list == INVALID_HANDLE_VALUE && GetLastError() == ERROR_INVALID_DATA);
    list = SetupDiGetClassDevsA(&guid, "ROOT\\LEGACY_BOGUS\\foo", NULL, DIGCF_DEVICEINTERFACE);
    CHECK(list != INVALID_HANDLE_VALUE);
    if (list != INVALID_HANDLE_VALUE) SetupDiDestroyDeviceInfoList(list);
    list = SetupDiGetClassDevsA(NULL, "ROOT", NULL, DIGCF_ALLCLASSES);
    CHECK(list != INVALID_HANDLE_VALUE);                       /* devices, not interfaces: an enumerator is fine */
    if (list != INVALID_HANDLE_VALUE) SetupDiDestroyDeviceInfoList(list);

    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures ? 1 : 0;
}
