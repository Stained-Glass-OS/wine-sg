/* setupapi machine names: the local computer is accepted, others are not (patch 3743) */
#include <windows.h>
#include <setupapi.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

static int f;
#define CHECK(c, ...) do { if (!(c)) { f++; printf("FAIL line %d: %s: ", __LINE__, #c); printf(__VA_ARGS__); printf("\n"); } } while (0)

typedef HDEVINFO (WINAPI *create_ex_t)(const GUID *, HWND, PCWSTR, PVOID);
typedef HDEVINFO (WINAPI *getclass_ex_t)(const GUID *, PCWSTR, HWND, DWORD, HDEVINFO, PCWSTR, PVOID);
typedef HKEY (WINAPI *openclass_ex_t)(const GUID *, REGSAM, DWORD, PCWSTR, PVOID);

int main(void)
{
    static const GUID cdrom = { 0x4d36e965, 0xe325, 0x11ce, { 0xbf, 0xc1, 0x08, 0x00, 0x2b, 0xe1, 0x03, 0x18 } };
    HMODULE mod = LoadLibraryA("setupapi.dll");
    create_ex_t pCreate = (void *)GetProcAddress(mod, "SetupDiCreateDeviceInfoListExW");
    getclass_ex_t pGetClass = (void *)GetProcAddress(mod, "SetupDiGetClassDevsExW");
    openclass_ex_t pOpen = (void *)GetProcAddress(mod, "SetupDiOpenClassRegKeyExW");
    WCHAR host[MAX_COMPUTERNAME_LENGTH + 1], dns[256], name[300];
    DWORD size, err;
    HDEVINFO set;
    HKEY key;
    struct { const WCHAR *name; BOOL local; const char *what; } cases[10];
    int i, n = 0;

    size = ARRAYSIZE(host);
    CHECK(GetComputerNameExW(ComputerNameNetBIOS, host, &size), "computer name %lu", GetLastError());
    size = ARRAYSIZE(dns);
    CHECK(GetComputerNameExW(ComputerNameDnsHostname, dns, &size), "dns name %lu", GetLastError());

    cases[n].name = NULL; cases[n].local = TRUE; cases[n++].what = "NULL";
    cases[n].name = L""; cases[n].local = TRUE; cases[n++].what = "empty";
    swprintf(name, ARRAYSIZE(name), L"\\\\%ls", host);
    cases[n].name = name; cases[n].local = TRUE; cases[n++].what = "\\\\NetBIOS name";
    {
        static WCHAR lower[300];
        int k;
        swprintf(lower, ARRAYSIZE(lower), L"\\\\%ls", host);
        for (k = 2; lower[k]; k++) lower[k] = towlower(lower[k]);
        cases[n].name = lower; cases[n].local = TRUE; cases[n++].what = "lower case";
    }
    {
        static WCHAR mixed[300];
        int k;
        swprintf(mixed, ARRAYSIZE(mixed), L"\\\\%ls", host);
        for (k = 2; mixed[k]; k++) mixed[k] = (k & 1) ? towlower(mixed[k]) : towupper(mixed[k]);
        cases[n].name = mixed; cases[n].local = TRUE; cases[n++].what = "mixed case";
    }
    cases[n].name = L"\\\\fastb-no-such-machine"; cases[n].local = FALSE; cases[n++].what = "another machine";
    cases[n].name = L"\\\\"; cases[n].local = FALSE; cases[n++].what = "no name after the slashes";
    cases[n].name = host; cases[n].local = FALSE; cases[n++].what = "name without the leading slashes";
    cases[n].name = L"fastb-no-such-machine"; cases[n].local = FALSE; cases[n++].what = "bare other name";

    for (i = 0; i < n; i++)
    {
        SetLastError(0xdeadbeef);
        set = pCreate(&cdrom, NULL, cases[i].name, NULL);
        if (cases[i].local) { CHECK(set != INVALID_HANDLE_VALUE, "create (%s): %lu", cases[i].what, GetLastError()); }
        else
        {
            err = GetLastError();
            CHECK(set == INVALID_HANDLE_VALUE && err == ERROR_INVALID_MACHINENAME, "create (%s): %p error %lu", cases[i].what, set, err);
        }
        if (set != INVALID_HANDLE_VALUE) SetupDiDestroyDeviceInfoList(set);

        SetLastError(0xdeadbeef);
        set = pGetClass(&cdrom, NULL, NULL, 0, NULL, cases[i].name, NULL);
        if (cases[i].local) { CHECK(set != INVALID_HANDLE_VALUE, "getclassdevs (%s): %lu", cases[i].what, GetLastError()); }
        else
        {
            err = GetLastError();
            CHECK(set == INVALID_HANDLE_VALUE && err == ERROR_INVALID_MACHINENAME, "getclassdevs (%s): %p error %lu", cases[i].what, set, err);
        }
        if (set != INVALID_HANDLE_VALUE) SetupDiDestroyDeviceInfoList(set);

        /* with a device information set given, the machine is checked as well */
        {
            HDEVINFO given = SetupDiCreateDeviceInfoList(&cdrom, NULL);
            SetLastError(0xdeadbeef);
            set = pGetClass(&cdrom, NULL, NULL, 0, given, cases[i].name, NULL);
            if (!cases[i].local)
            {
                err = GetLastError();
                CHECK(set == INVALID_HANDLE_VALUE && err == ERROR_INVALID_MACHINENAME, "getclassdevs with a set (%s): %p error %lu",
                      cases[i].what, set, err);
            }
            else CHECK(set == given, "getclassdevs with a set (%s): %p", cases[i].what, set);
            SetupDiDestroyDeviceInfoList(given);
        }

        SetLastError(0xdeadbeef);
        key = pOpen(&cdrom, KEY_READ, DIOCR_INSTALLER, cases[i].name, NULL);
        if (cases[i].local)
        {
            /* the class key may be missing in a bare prefix, but the machine name must not be what fails */
            err = GetLastError();
            CHECK(key != INVALID_HANDLE_VALUE || err != ERROR_INVALID_MACHINENAME, "openclass (%s): error %lu", cases[i].what, err);
        }
        else
        {
            err = GetLastError();
            CHECK(key == INVALID_HANDLE_VALUE && err == ERROR_INVALID_MACHINENAME, "openclass (%s): %p error %lu", cases[i].what, key, err);
        }
        if (key != INVALID_HANDLE_VALUE) RegCloseKey(key);
    }
    printf("%s\n", f ? "RESULT: FAIL" : "RESULT: PASS");
    return f != 0;
}
