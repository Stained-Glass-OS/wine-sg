/* sti: IStillImageW::GetDeviceList/GetDeviceInfo/GetDeviceValue/GetSTILaunchInformation/WriteToErrorLog (patch 3617) */
#define COBJMACROS
#include <windows.h>
#include <stdio.h>
#include <string.h>
#define ARRAY_SIZE(a) (sizeof(a)/sizeof((a)[0]))
#include <sti.h>

static int f;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); f++; } } while (0)

static const WCHAR classpath[] = L"SYSTEM\\CurrentControlSet\\Control\\Class\\{6bdd1fc6-810f-11d0-bec7-08002be2092f}";

typedef HRESULT (WINAPI *create_t)(HINSTANCE, DWORD, PSTIW *, LPUNKNOWN);

static PSTIW make(void)
{
    HMODULE sti = LoadLibraryA("sti.dll");
    create_t create = (create_t)GetProcAddress(sti, "StiCreateInstanceW");
    PSTIW p = NULL;

    if (!create || FAILED(create(GetModuleHandleA(NULL), STI_VERSION_REAL | STI_VERSION_FLAG_UNICODE, &p, NULL))) return NULL;
    return p;
}

static void add_device(const WCHAR *sub, DWORD type, const WCHAR *id, const WCHAR *vendor, const WCHAR *desc)
{
    HKEY key;
    WCHAR path[300];

    swprintf(path, 300, L"%ls\\%ls", classpath, sub);
    RegCreateKeyExW(HKEY_LOCAL_MACHINE, path, 0, NULL, 0, KEY_ALL_ACCESS, NULL, &key, NULL);
    RegSetValueExW(key, L"DeviceType", 0, REG_DWORD, (BYTE *)&type, 4);
    RegSetValueExW(key, L"DeviceID", 0, REG_SZ, (BYTE *)id, (wcslen(id) + 1) * 2);
    RegSetValueExW(key, L"Vendor", 0, REG_SZ, (BYTE *)vendor, (wcslen(vendor) + 1) * 2);
    RegSetValueExW(key, L"DriverDesc", 0, REG_SZ, (BYTE *)desc, (wcslen(desc) + 1) * 2);
    RegSetValueExW(key, L"Custom", 0, REG_DWORD, (BYTE *)&type, 4);
    RegCloseKey(key);
}

static int child(void)
{
    PSTIW sti = make();
    WCHAR dev[STI_MAX_INTERNAL_NAME_LENGTH] = {0}, ev[40] = {0};
    DWORD code = 99;
    HRESULT hr;

    if (!sti) return 2;
    hr = IStillImage_GetSTILaunchInformation(sti, dev, &code, ev);
    if (hr != S_OK || wcscmp(dev, L"Dev0001") || wcscmp(ev, L"{12345678-0000-0000-0000-000000000001}") || !code) return 3;
    return 0;
}

int main(int argc, char **argv)
{
    PSTIW sti;
    HRESULT hr;
    DWORD n = 99;
    STI_DEVICE_INFORMATIONW *list = NULL;
    void *info = NULL;
    WCHAR cmd[MAX_PATH + 200];
    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    DWORD exitcode = 100, type, size;
    BYTE data[16];

    setvbuf(stdout, NULL, _IONBF, 0);
    if (argc > 1 && !strcmp(argv[1], "child")) return child();

    sti = make();
    CHECK(sti != NULL);
    if (!sti) { puts("RESULT: FAIL"); return 1; }

    /* no devices registered */
    hr = IStillImage_GetDeviceList(sti, 0, 0, &n, (void **)&list);
    CHECK(hr == S_OK && n == 0 && !list);
    hr = IStillImage_GetDeviceList(sti, 0, 0, NULL, (void **)&list);
    CHECK(hr == E_INVALIDARG);

    add_device(L"0000", MAKELONG(0, StiDeviceTypeScanner), L"Scan0001", L"Acme", L"Acme Flatbed");
    add_device(L"0001", MAKELONG(0, StiDeviceTypeDigitalCamera), L"Cam0001", L"Photo Inc", L"Photo Cam");

    hr = IStillImage_GetDeviceList(sti, 0, 0, &n, (void **)&list);
    CHECK(hr == S_OK && n == 2 && list);
    if (list && n == 2)
    {
        CHECK(list[0].dwSize == sizeof(list[0]));
        CHECK(!wcscmp(list[0].szDeviceInternalName, L"Scan0001"));
        CHECK(list[0].pszVendorDescription && !wcscmp(list[0].pszVendorDescription, L"Acme"));
        CHECK(list[0].pszDeviceDescription && !wcscmp(list[0].pszDeviceDescription, L"Acme Flatbed"));
        CHECK(!wcscmp(list[1].szDeviceInternalName, L"Cam0001"));
        CHECK(GET_STIDEVICE_TYPE(list[1].DeviceType) == StiDeviceTypeDigitalCamera);
        LocalFree(list);
    }
    list = NULL;
    hr = IStillImage_GetDeviceList(sti, StiDeviceTypeScanner, 0, &n, (void **)&list);
    CHECK(hr == S_OK && n == 1 && list && !wcscmp(list[0].szDeviceInternalName, L"Scan0001"));
    if (list) LocalFree(list);
    list = NULL;
    hr = IStillImage_GetDeviceList(sti, StiDeviceTypeStreamingVideo, 0, &n, (void **)&list);
    CHECK(hr == S_OK && n == 0 && !list);

    hr = IStillImage_GetDeviceInfo(sti, L"cam0001", &info);
    CHECK(hr == S_OK && info && !wcscmp(((STI_DEVICE_INFORMATIONW *)info)->pszDeviceDescription, L"Photo Cam"));
    if (info) LocalFree(info);
    info = (void *)1;
    hr = IStillImage_GetDeviceInfo(sti, L"Nothing", &info);
    CHECK(FAILED(hr) && !info);

    size = sizeof(data);
    hr = IStillImage_GetDeviceValue(sti, L"Cam0001", L"Custom", &type, data, &size);
    CHECK(hr == S_OK && type == REG_DWORD && size == 4 && *(DWORD *)data == MAKELONG(0, StiDeviceTypeDigitalCamera));
    size = sizeof(data);
    hr = IStillImage_GetDeviceValue(sti, L"Cam0001", L"NoSuchValue", &type, data, &size);
    CHECK(FAILED(hr));
    hr = IStillImage_GetDeviceValue(sti, L"Nothing", L"Custom", &type, data, &size);
    CHECK(FAILED(hr));

    {
        BOOL state = FALSE;
        typedef HRESULT (WINAPI *hw_t)(void *, const WCHAR *, BOOL *);
        hw_t hw = (hw_t)(*(void ***)sti)[13];
        hr = hw(sti, L"Cam0001", &state);
        CHECK(hr == S_OK && state == TRUE);
        hr = hw(sti, L"Nothing", &state);
        CHECK(FAILED(hr));
    }

    hr = ((HRESULT (WINAPI *)(void *, DWORD, const WCHAR *))(*(void ***)sti)[17])(sti, 1, L"sti probe message");
    CHECK(hr == S_OK);
    hr = ((HRESULT (WINAPI *)(void *, DWORD, const WCHAR *))(*(void ***)sti)[17])(sti, 1, NULL);
    CHECK(hr == E_INVALIDARG);

    /* the launch information comes from the command line */
    {
        WCHAR self[MAX_PATH];
        GetModuleFileNameW(NULL, self, MAX_PATH);
        swprintf(cmd, ARRAY_SIZE(cmd), L"\"%ls\" child /StiDevice:Dev0001 /StiEvent:{12345678-0000-0000-0000-000000000001}", self);
        if (CreateProcessW(NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
        {
            WaitForSingleObject(pi.hProcess, 30000);
            GetExitCodeProcess(pi.hProcess, &exitcode);
            CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
        }
        CHECK(exitcode == 0);
        {
            WCHAR dev[STI_MAX_INTERNAL_NAME_LENGTH], ev[40];
            DWORD code;
            hr = IStillImage_GetSTILaunchInformation(sti, dev, &code, ev);
            CHECK(hr == HRESULT_FROM_WIN32(ERROR_NOT_FOUND));
        }
    }

    RegDeleteKeyW(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Control\\Class\\{6bdd1fc6-810f-11d0-bec7-08002be2092f}\\0000");
    RegDeleteKeyW(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Control\\Class\\{6bdd1fc6-810f-11d0-bec7-08002be2092f}\\0001");
    IStillImage_Release(sti);
    puts(f ? "RESULT: FAIL" : "RESULT: PASS");
    return f;
}
