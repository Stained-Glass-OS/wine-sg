/* classdrv gate probe: the Net device setup class and its class driver list (i1014; Wintun's
 * installer in OpenVPN's MSI). Prints KEY value lines. */
#include <windows.h>
#include <initguid.h>
#include <setupapi.h>
#include <devguid.h>
#include <cfgmgr32.h>
#include <stdio.h>
#include <wchar.h>

static const char inf[] =
    "[Version]\r\nSignature=\"$Windows NT$\"\r\nClass=Net\r\nClassGuid={4d36e972-e325-11ce-bfc1-08002be10318}\r\n"
    "Provider=%%Prov%%\r\nDriverVer=11/28/2026,1.2.3.4\r\n\r\n"
    "[Manufacturer]\r\n%%Mfg%%=Models,NTamd64\r\n\r\n"
    "[Models.NTamd64]\r\n%%Desc%%=Inst,sgtestnet\r\n\r\n"
    "[Inst]\r\nCharacteristics=0x1\r\n\r\n"
    "[Strings]\r\nProv=\"Stained Glass test\"\r\nMfg=\"Stained Glass test\"\r\nDesc=\"SG Test Adapter\"\r\n";

#define NETKEY L"System\\CurrentControlSet\\Control\\Network\\{4D36E972-E325-11CE-BFC1-08002BE10318}\\"

static int run(const WCHAR *cmdline)
{
    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    WCHAR buf[512];
    DWORD code = 255;
    lstrcpynW(buf, cmdline, ARRAYSIZE(buf));
    if (!CreateProcessW(NULL, buf, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) return -1;
    WaitForSingleObject(pi.hProcess, 60000);
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
    return code;
}

/* a VPN's adapter as OpenVPN's tapctl makes, disables and removes it: a ROOT\NET device for the
 * hardware ID, DIF_REGISTERDEVICE, DiInstallDevice with the driver store's best driver, then its
 * NetCfgInstanceId and connection; renamed with netsh; disabled (DIF_PROPERTYCHANGE); removed */
static void adapter(void)
{
    HDEVINFO set = SetupDiCreateDeviceInfoList(&GUID_DEVCLASS_NET, NULL);
    SP_DEVINFO_DATA dev = { sizeof(dev) };
    static const WCHAR hwid[] = L"sgtestnet\0";
    WCHAR id[64] = {0}, path[MAX_PATH], name[128] = {0}, pnp[128] = {0}, inst[128] = {0}, cmd[300];
    DWORD size, type, len;
    BOOL reboot = TRUE, ok;
    HKEY key;
    ULONG status = 0, problem = 0;
    SP_PROPCHANGE_PARAMS pc = {{ sizeof(SP_CLASSINSTALL_HEADER), DIF_PROPERTYCHANGE }, DICS_DISABLE, DICS_FLAG_GLOBAL, 0};
    SP_PROPCHANGE_PARAMS back = {{ sizeof(SP_CLASSINSTALL_HEADER) }};
    SP_REMOVEDEVICE_PARAMS rm = {{ sizeof(SP_CLASSINSTALL_HEADER), DIF_REMOVE }, DI_REMOVEDEVICE_GLOBAL, 0};

    ok = SetupDiCreateDeviceInfoW(set, L"Net", &GUID_DEVCLASS_NET, NULL, NULL, DICD_GENERATE_ID, &dev) &&
         SetupDiSetDeviceRegistryPropertyW(set, &dev, SPDRP_HARDWAREID, (const BYTE *)hwid, sizeof(hwid)) &&
         SetupDiCallClassInstaller(DIF_REGISTERDEVICE, set, &dev);
    printf("REGISTER %d\n", ok);
    if (!ok) return;
    SetupDiGetDeviceInstanceIdW(set, &dev, inst, ARRAYSIZE(inst), NULL);
    {
        /* mingw resolves no DiInstallDevice: at run time, as tapctl does */
        BOOL (WINAPI *install)(HWND, HDEVINFO, SP_DEVINFO_DATA *, SP_DRVINFO_DATA_W *, DWORD, BOOL *) =
            (void *)GetProcAddress(LoadLibraryW(L"newdev.dll"), "DiInstallDevice");
        ok = install && install(NULL, set, &dev, NULL, 0, &reboot);
    }
    printf("DIINSTALL %d err %lu reboot %d\n", ok, ok ? 0 : GetLastError(), reboot);
    if ((key = SetupDiOpenDevRegKey(set, &dev, DICS_FLAG_GLOBAL, 0, DIREG_DRV, KEY_READ)) != INVALID_HANDLE_VALUE)
    {
        size = sizeof(id);
        RegQueryValueExW(key, L"NetCfgInstanceId", NULL, &type, (BYTE *)id, &size);
        RegCloseKey(key);
    }
    swprintf(path, ARRAYSIZE(path), NETKEY L"%ls\\Connection", id);
    if (!RegOpenKeyExW(HKEY_LOCAL_MACHINE, path, 0, KEY_READ, &key))
    {
        size = sizeof(name); RegQueryValueExW(key, L"Name", NULL, NULL, (BYTE *)name, &size);
        size = sizeof(pnp); RegQueryValueExW(key, L"PnpInstanceID", NULL, NULL, (BYTE *)pnp, &size);
        RegCloseKey(key);
    }
    len = lstrlenW(id);
    printf("NETCFG %d connection %d named %d\n", len == 38 && id[0] == '{', !_wcsicmp(pnp, inst), !wcsncmp(name, L"Ethernet", 8));

    swprintf(cmd, ARRAYSIZE(cmd), L"netsh interface set interface name=\"%ls\" newname=\"SG Test VPN\"", name);
    printf("NETSH %d", run(cmd));
    name[0] = 0;
    if (!RegOpenKeyExW(HKEY_LOCAL_MACHINE, path, 0, KEY_READ, &key))
    {
        size = sizeof(name); RegQueryValueExW(key, L"Name", NULL, NULL, (BYTE *)name, &size);
        RegCloseKey(key);
    }
    printf(" renamed %d\n", !wcscmp(name, L"SG Test VPN"));

    ok = SetupDiSetClassInstallParamsW(set, &dev, &pc.ClassInstallHeader, sizeof(pc)) &&
         SetupDiGetClassInstallParamsW(set, &dev, &back.ClassInstallHeader, sizeof(back), &size) &&
         back.StateChange == DICS_DISABLE;
    printf("PARAMS %d\n", ok);
    ok = SetupDiCallClassInstaller(DIF_PROPERTYCHANGE, set, &dev);
    CM_Get_DevNode_Status(&status, &problem, dev.DevInst, 0);
    printf("DISABLE %d problem %lu\n", ok, problem);

    ok = SetupDiSetClassInstallParamsW(set, &dev, &rm.ClassInstallHeader, sizeof(rm)) &&
         SetupDiCallClassInstaller(DIF_REMOVE, set, &dev);
    printf("REMOVE %d connection_gone %d\n", ok, RegOpenKeyExW(HKEY_LOCAL_MACHINE, path, 0, KEY_READ, &key) != 0);
    SetupDiDestroyDeviceInfoList(set);
}

/* the class driver list of a Net device information set, as Wintun's RemoveWintun walks it:
 * how many drivers, and whether one of them is ours (its INF file name back) */
static int find_ours(WCHAR *inf_name, int *count, int *types_ok)
{
    HDEVINFO set = SetupDiGetClassDevsW(&GUID_DEVCLASS_NET, NULL, NULL, 0);
    SP_DRVINFO_DATA_W drv;
    DWORD i, size;
    int found = 0, built;

    *count = 0; *types_ok = 1;
    if (set == INVALID_HANDLE_VALUE) return -1;
    built = SetupDiBuildDriverInfoList(set, NULL, SPDIT_CLASSDRIVER);
    if (!built) { printf("BUILD_ERR %lu\n", GetLastError()); SetupDiDestroyDeviceInfoList(set); return -1; }
    for (i = 0;; i++)
    {
        SP_DRVINFO_DETAIL_DATA_W *detail;
        drv.cbSize = sizeof(drv);
        if (!SetupDiEnumDriverInfoW(set, NULL, SPDIT_CLASSDRIVER, i, &drv))
        {
            if (GetLastError() != ERROR_NO_MORE_ITEMS) printf("ENUM_ERR %lu\n", GetLastError());
            break;
        }
        (*count)++;
        if (drv.DriverType != SPDIT_CLASSDRIVER) *types_ok = 0;
        SetupDiGetDriverInfoDetailW(set, NULL, &drv, NULL, 0, &size);
        detail = calloc(1, size);
        detail->cbSize = sizeof(*detail);
        if (SetupDiGetDriverInfoDetailW(set, NULL, &drv, detail, size, &size) && !_wcsicmp(detail->HardwareID, L"sgtestnet"))
        {
            WCHAR *p = wcsrchr(detail->InfFileName, '\\');
            wcscpy(inf_name, p ? p + 1 : detail->InfFileName);
            found = 1;
        }
        free(detail);
    }
    printf("DESTROY %d\n", SetupDiDestroyDriverInfoList(set, NULL, SPDIT_CLASSDRIVER));
    SetupDiDestroyDeviceInfoList(set);
    return found;
}

int main(void)
{
    WCHAR name[64] = {0}, path[MAX_PATH], dest[MAX_PATH], ours[MAX_PATH] = {0}, again[MAX_PATH] = {0};
    GUID guid; DWORD n = 0, len; HANDLE f; int count, types_ok, found;

    found = SetupDiClassNameFromGuidW(&GUID_DEVCLASS_NET, name, ARRAYSIZE(name), NULL);
    printf("CLASSNAME %d %ls\n", found, name);
    memset(&guid, 0, sizeof(guid));
    found = SetupDiClassGuidsFromNameW(L"Net", &guid, 1, &n);
    printf("CLASSGUID %d %d\n", found, n == 1 && IsEqualGUID(&guid, &GUID_DEVCLASS_NET));

    /* no driver of ours yet: the list still builds (RemoveWintun's first step) */
    found = find_ours(ours, &count, &types_ok);
    printf("BEFORE %d\n", found);

    GetTempPathW(MAX_PATH, path); wcscat(path, L"sgtestnet.inf");
    f = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    WriteFile(f, inf, sizeof(inf) - 1, &len, NULL); CloseHandle(f);
    if (!SetupCopyOEMInfW(path, NULL, SPOST_PATH, 0, dest, MAX_PATH, NULL, NULL)) { printf("COPY_ERR %lu\n", GetLastError()); return 1; }
    found = find_ours(ours, &count, &types_ok);
    printf("STORED %d %d types %d\n", found, count, types_ok);
    adapter();
    printf("UNINSTALL %d\n", found == 1 && SetupUninstallOEMInfW(ours, SUOI_FORCEDELETE, NULL));
    found = find_ours(again, &count, &types_ok);
    printf("AFTER %d\n", found);
    DeleteFileW(path);
    fflush(stdout);
    return 0;
}
