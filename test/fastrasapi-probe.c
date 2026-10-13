#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <ras.h>
#include <raserror.h>

static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

int main(void)
{
    HMODULE m = LoadLibraryA("rasapi32.dll");
    DWORD (WINAPI *param)(DWORD, void *, DWORD *) = (void *)GetProcAddress(m, "RasGetAutodialParamW");
    DWORD (WINAPI *enable)(DWORD, BOOL *) = (void *)GetProcAddress(m, "RasGetAutodialEnableW");
    DWORD (WINAPI *enumaddr)(WCHAR **, DWORD *, DWORD *) = (void *)GetProcAddress(m, "RasEnumAutodialAddressesW");
    DWORD (WINAPI *getaddr)(const WCHAR *, DWORD *, void *, DWORD *, DWORD *) = (void *)GetProcAddress(m, "RasGetAutodialAddressW");
    DWORD (WINAPI *devs)(RASDEVINFOW *, DWORD *, DWORD *) = (void *)GetProcAddress(m, "RasEnumDevicesW");
    DWORD (WINAPI *dialparams)(const WCHAR *, RASDIALPARAMSW *, BOOL *) = (void *)GetProcAddress(m, "RasGetEntryDialParamsW");
    DWORD (WINAPI *proj)(HRASCONN, int, void *, DWORD *) = (void *)GetProcAddress(m, "RasGetProjectionInfoW");
    DWORD v, size, r, count, bytes;
    BOOL b;
    RASDEVINFOW di;
    RASDIALPARAMSW dp;

    CHECK(param && enable && enumaddr && getaddr && devs && dialparams && proj, "exports");

    size = sizeof(v);
    r = param(2, &v, &size);
    CHECK(r == 0 && v == 100 && size == 4, "SavedAddressesLimit %lu %lu", r, v);
    r = param(3, &v, &size);
    CHECK(r == 0 && v == 5, "FailedConnectionTimeout %lu", v);
    r = param(4, &v, &size);
    CHECK(r == 0 && v == 15, "ConnectionQueryTimeout %lu", v);
    size = 0;
    r = param(0, &v, &size);
    CHECK(r == ERROR_BUFFER_TOO_SMALL && size == 4, "param buffer too small %lu", r);
    CHECK(param(99, &v, &size) == ERROR_INVALID_PARAMETER, "bad param key");

    b = TRUE;
    CHECK(enable(1, &b) == 0 && !b, "autodial disabled by default");
    CHECK(enable(1, NULL) == ERROR_INVALID_PARAMETER, "enable NULL");

    bytes = 77; count = 77;
    CHECK(enumaddr(NULL, &bytes, &count) == 0 && bytes == 0 && count == 0, "no autodial addresses");
    bytes = 77; count = 77;
    CHECK(getaddr(L"www.example.com", NULL, NULL, &bytes, &count) == 0 && bytes == 0 && count == 0, "no autodial entries for an address");

    memset(&di, 0, sizeof(di)); di.dwSize = sizeof(di);
    size = sizeof(di); count = 0;
    r = devs(&di, &size, &count);
    CHECK(r == 0 && count == 1 && di.szDeviceName[0], "RasEnumDevicesW %lu", r);
    size = 0; count = 99;
    r = devs(&di, &size, &count);
    CHECK(r == ERROR_BUFFER_TOO_SMALL && count == 99 && size == sizeof(di), "RasEnumDevicesW zero buffer %lu", r);
    di.dwSize = 3;
    CHECK(devs(&di, &size, &count) == ERROR_INVALID_SIZE, "bad size");
    {
        DWORD (WINAPI *devsa)(RASDEVINFOA *, DWORD *, DWORD *) = (void *)GetProcAddress(m, "RasEnumDevicesA");
        DWORD (WINAPI *entw)(const WCHAR *, const WCHAR *, RASENTRYNAMEW *, DWORD *, DWORD *) = (void *)GetProcAddress(m, "RasEnumEntriesW");
        DWORD (WINAPI *enta)(const char *, const char *, RASENTRYNAMEA *, DWORD *, DWORD *) = (void *)GetProcAddress(m, "RasEnumEntriesA");
        RASDEVINFOA dia;
        RASENTRYNAMEA ena;
        RASENTRYNAMEW enw;

        memset(&dia, 0, sizeof(dia)); dia.dwSize = sizeof(dia);
        size = sizeof(dia); count = 0;
        r = devsa(&dia, &size, &count);
        CHECK(r == 0 && count == 1 && dia.szDeviceName[0] && dia.szDeviceType[0], "RasEnumDevicesA %lu", r);
        size = 0; count = 99;
        r = devsa(&dia, &size, &count);
        CHECK(r == ERROR_BUFFER_TOO_SMALL && count == 99 && size == sizeof(dia), "RasEnumDevicesA zero buffer %lu", r);
        dia.dwSize = 3;
        CHECK(devsa(&dia, &size, &count) == ERROR_INVALID_SIZE, "RasEnumDevicesA bad size");
        CHECK(devsa(NULL, &size, NULL) == ERROR_INVALID_PARAMETER, "RasEnumDevicesA NULL count");

        ena.dwSize = sizeof(ena); enw.dwSize = sizeof(enw);
        size = sizeof(ena); count = 99;
        r = enta(NULL, NULL, &ena, &size, &count);
        CHECK(r == 0 && count == 0, "RasEnumEntriesA none %lu count %lu", r, count);
        size = sizeof(enw); count = 99;
        r = entw(NULL, NULL, &enw, &size, &count);
        CHECK(r == 0 && count == 0, "RasEnumEntriesW none %lu", r);
        ena.dwSize = 2;
        CHECK(enta(NULL, NULL, &ena, &size, &count) == ERROR_INVALID_SIZE, "RasEnumEntriesA bad dwSize");
        CHECK(enta(NULL, NULL, NULL, NULL, &count) == ERROR_INVALID_PARAMETER, "RasEnumEntriesA NULL lpcb");
    }

    memset(&dp, 0, sizeof(dp)); dp.dwSize = sizeof(dp);
    wcscpy(dp.szEntryName, L"no such entry");
    r = dialparams(NULL, &dp, &b);
    CHECK(r == ERROR_CANNOT_FIND_PHONEBOOK_ENTRY, "dial params of a missing entry: %lu (struct %u)", r, (unsigned)sizeof(dp));
    CHECK(proj((HRASCONN)0x1234, 0x8021, NULL, &bytes) == ERROR_INVALID_HANDLE, "projection of a bad handle");
    CHECK(proj((HRASCONN)0x1234, 0x77, NULL, &bytes) == ERROR_INVALID_PARAMETER, "projection type");

    printf(fails ? "RESULT: FAIL (%d)\n" : "RESULT: PASS\n", fails);
    return fails != 0;
}
