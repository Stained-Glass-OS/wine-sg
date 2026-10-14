/* newdev.dll (patches/sg/5503): DiInstallDriver and InstallSelectedDriver.
 * A root-enumerated device of its own and small driver packages (an INF each);
 * prints one "name=value" line per fact for test/newdev-gate.sh.
 *
 *   newdev-probe.exe WORKDIR
 */
#include <windows.h>
#include <setupapi.h>
#include <stdio.h>
#include <string.h>

#ifndef DIIRFLAG_FORCE_INF
#define DIIRFLAG_FORCE_INF 0x02
#define DIIRFLAG_INSTALL_AS_SET 0x40
#endif

typedef BOOL (WINAPI *DiInstallDriverW_t)(HWND, const WCHAR *, DWORD, BOOL *);
typedef BOOL (WINAPI *InstallSelectedDriver_t)(HWND, HDEVINFO, const WCHAR *, BOOL, DWORD *);
static DiInstallDriverW_t pDiInstallDriverW;
static InstallSelectedDriver_t pInstallSelectedDriver;

static const GUID system_class = {0x4d36e97e, 0xe325, 0x11ce, {0xbf, 0xc1, 0x08, 0x00, 0x2b, 0xe1, 0x03, 0x18}};

/* an INF whose only model is hwid; installing it sets HKR\Which = which */
static void write_inf(const WCHAR *dir, const WCHAR *name, const char *hwid, const char *which, const char *date)
{
    WCHAR path[MAX_PATH];
    FILE *f;

    CreateDirectoryW(dir, NULL);
    swprintf(path, MAX_PATH, L"%ls\\%ls", dir, name);
    f = _wfopen(path, L"w");
    fprintf(f,
        "[Version]\nSignature=\"$Windows NT$\"\nClass=System\nClassGuid={4d36e97e-e325-11ce-bfc1-08002be10318}\n"
        "Provider=%%Prov%%\nDriverVer=%s,1.0.0.0\n"
        "[Manufacturer]\n%%Mfg%%=Models,NTamd64,NTx86\n"
        "[Models.NTamd64]\n%%Dev%%=Install,%s\n[Models.NTx86]\n%%Dev%%=Install,%s\n"
        "[Install]\nAddReg=Reg\n[Reg]\nHKR,,Which,,\"%s\"\n"
        "[Strings]\nProv=\"Stained Glass\"\nMfg=\"Stained Glass\"\nDev=\"SG test device\"\n",
        date, hwid, hwid, which);
    fclose(f);
}

static BOOL is_admin(void)
{
    SID_IDENTIFIER_AUTHORITY authority = { SECURITY_NT_AUTHORITY };
    BOOL member = FALSE;
    PSID sid;

    if (!AllocateAndInitializeSid(&authority, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS,
                                  0, 0, 0, 0, 0, 0, &sid)) return FALSE;
    if (!CheckTokenMembership(NULL, sid, &member)) member = FALSE;
    FreeSid(sid);
    return member;
}

/* the device's driver key: HKR "Which" and "InfPath" */
static void read_driver(HDEVINFO set, SP_DEVINFO_DATA *device, char *which, char *infpath)
{
    HKEY key = SetupDiOpenDevRegKey(set, device, DICS_FLAG_GLOBAL, 0, DIREG_DRV, KEY_READ);
    DWORD size;

    strcpy(which, "-");
    strcpy(infpath, "-");
    if (key == INVALID_HANDLE_VALUE) return;
    size = 64;
    if (RegQueryValueExA(key, "Which", NULL, NULL, (BYTE *)which, &size)) strcpy(which, "-");
    size = 64;
    if (RegQueryValueExA(key, "InfPath", NULL, NULL, (BYTE *)infpath, &size)) strcpy(infpath, "-");
    RegCloseKey(key);
}

/* is the INF in the system INF directory (where the driver store keeps its copy) */
static int staged(const WCHAR *name)
{
    WCHAR path[MAX_PATH];

    GetSystemWindowsDirectoryW(path, MAX_PATH);
    swprintf(path + wcslen(path), MAX_PATH - wcslen(path), L"\\inf\\%ls", name);
    return GetFileAttributesW(path) != INVALID_FILE_ATTRIBUTES;
}

static void install(const char *label, const WCHAR *inf, DWORD flags, HDEVINFO set, SP_DEVINFO_DATA *device)
{
    char which[64], infpath[64];
    BOOL reboot = TRUE, ret;
    DWORD error;

    SetLastError(0);
    ret = pDiInstallDriverW(NULL, inf, flags, &reboot);
    error = ret ? 0 : GetLastError();
    read_driver(set, device, which, infpath);
    printf("%s ret=%d error=%lu reboot=%d which=%s inf=%s\n", label, ret, error, reboot, which, infpath);
}

int main(int argc, char **argv)
{
    HMODULE newdev = LoadLibraryA("newdev.dll");
    WCHAR work[MAX_PATH], dir_a[MAX_PATH], dir_b[MAX_PATH], dir_set[MAX_PATH], inf_a[MAX_PATH], inf_b[MAX_PATH];
    SP_DEVINFO_DATA device = {sizeof(device)};
    BOOL wow64 = FALSE, reboot;
    HDEVINFO set;
    char which[64], infpath[64];
    BOOL ret;

    if (!newdev || argc < 2) { printf("load=FAIL\n"); return 1; }
    pDiInstallDriverW = (void *)GetProcAddress(newdev, "DiInstallDriverW");
    pInstallSelectedDriver = (void *)GetProcAddress(newdev, "InstallSelectedDriver");
    MultiByteToWideChar(CP_ACP, 0, argv[1], -1, work, MAX_PATH);
    IsWow64Process(GetCurrentProcess(), &wow64);
    printf("wow64=%d\n", wow64);

    swprintf(dir_a, MAX_PATH, L"%ls\\a", work);
    swprintf(dir_b, MAX_PATH, L"%ls\\b", work);
    swprintf(dir_set, MAX_PATH, L"%ls\\set", work);
    swprintf(inf_a, MAX_PATH, L"%ls\\sga.inf", dir_a);
    swprintf(inf_b, MAX_PATH, L"%ls\\sgb.inf", dir_b);

    if (wow64)
    {
        /* "ERROR_IN_WOW64: a 32-bit application ... in a 64-bit environment" */
        write_inf(dir_a, L"sga.inf", "Root\\SGTEST", "A", "01/02/2024");
        SetLastError(0);
        reboot = TRUE;
        ret = pDiInstallDriverW(NULL, inf_a, 0, &reboot);
        printf("wow64-install ret=%d error=%lu reboot=%d\n", ret, GetLastError(), reboot);
        SetLastError(0);
        ret = pInstallSelectedDriver(NULL, INVALID_HANDLE_VALUE, NULL, FALSE, NULL);
        printf("wow64-selected ret=%d error=%lu\n", ret, GetLastError());
        return 0;
    }

    /* the device: hardware ids from most to least specific */
    set = SetupDiCreateDeviceInfoList(&system_class, NULL);
    if (!SetupDiCreateDeviceInfoW(set, L"ROOT\\SYSTEM\\SGTEST0", &system_class, NULL, NULL, 0, &device) ||
        !SetupDiSetDeviceRegistryPropertyW(set, &device, SPDRP_HARDWAREID,
                                           (const BYTE *)L"Root\\SGTEST_SPECIFIC\0Root\\SGTEST\0",
                                           sizeof(L"Root\\SGTEST_SPECIFIC\0Root\\SGTEST\0")) ||
        !SetupDiRegisterDeviceInfo(set, &device, 0, NULL, NULL, NULL))
    {
        printf("device=FAIL error=%lu\n", GetLastError());
        return 1;
    }
    printf("device=ok\n");
    printf("is-admin=%d\n", is_admin());

    /* A matches the second id, B the first (a better match), both for 2024 */
    write_inf(dir_a, L"sga.inf", "Root\\SGTEST", "A", "01/02/2024");
    write_inf(dir_b, L"sgb.inf", "Root\\SGTEST_SPECIFIC", "B", "01/02/2024");

    install("install-a", inf_a, 0, set, &device);
    printf("staged-a=%d\n", staged(L"sga.inf"));

    /* the same package again is the one in the store already: nothing is added */
    install("install-a-again", inf_a, 0, set, &device);
    printf("staged-a-after-replace=%d\n", staged(L"sga.inf"));

    install("install-b-better", inf_b, 0, set, &device);
    install("install-a-worse", inf_a, 0, set, &device);
    install("install-a-forced", inf_a, DIIRFLAG_FORCE_INF, set, &device);

    /* a package that matches no device is still a success: it is in the store */
    write_inf(dir_set, L"sgnone.inf", "Root\\SGTEST_NOBODY", "N", "01/02/2024");
    {
        WCHAR none[MAX_PATH];

        swprintf(none, MAX_PATH, L"%ls\\sgnone.inf", dir_set);
        install("install-nomatch", none, 0, set, &device);
        printf("staged-nomatch=%d\n", staged(L"sgnone.inf"));
    }

    /* errors */
    {
        WCHAR missing[MAX_PATH];

        swprintf(missing, MAX_PATH, L"%ls\\missing.inf", work);
        install("missing", missing, 0, set, &device);
        install("bad-flags", inf_a, 0x80, set, &device);
        SetLastError(0);
        ret = pDiInstallDriverW(NULL, NULL, 0, NULL);
        printf("null-path ret=%d error=%lu\n", ret, GetLastError());
    }

    /* an INF directory as a set: staged, not installed on a device */
    write_inf(dir_set, L"sgset1.inf", "Root\\SGTEST_SETONE", "S1", "01/02/2024");
    write_inf(dir_set, L"sgset2.inf", "Root\\SGTEST_SETTWO", "S2", "01/02/2024");
    install("install-set", dir_set, DIIRFLAG_INSTALL_AS_SET, set, &device);
    printf("staged-set=%d\n", staged(L"sgset1.inf") && staged(L"sgset2.inf"));

    /* the same call as a user who is not an administrator */
    {
        SID_IDENTIFIER_AUTHORITY authority = { SECURITY_NT_AUTHORITY };
        SID_AND_ATTRIBUTES disable;
        HANDLE token, restricted, imp;

        AllocateAndInitializeSid(&authority, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS,
                                 0, 0, 0, 0, 0, 0, &disable.Sid);
        disable.Attributes = 0;
        OpenProcessToken(GetCurrentProcess(), TOKEN_ALL_ACCESS, &token);
        if (CreateRestrictedToken(token, 0, 1, &disable, 0, NULL, 0, NULL, &restricted) &&
            DuplicateTokenEx(restricted, TOKEN_ALL_ACCESS, NULL, SecurityImpersonation, TokenImpersonation, &imp) &&
            SetThreadToken(NULL, imp))
        {
            printf("restricted-is-admin=%d\n", is_admin());
            install("non-admin", inf_a, 0, set, &device);
            SetThreadToken(NULL, NULL);
        }
        else printf("restricted=FAIL error=%lu\n", GetLastError());
    }

    /* InstallSelectedDriver: the driver selected for the device goes on it */
    {
        SP_DRVINFO_DATA_W drv = {sizeof(drv)};
        SP_DEVINSTALL_PARAMS_W params = {sizeof(params)};
        DWORD flag = 0xdead;

        SetupDiSetSelectedDevice(set, &device);
        SetupDiGetDeviceInstallParamsW(set, &device, &params);
        lstrcpyW(params.DriverPath, inf_b);
        params.Flags |= DI_ENUMSINGLEINF;
        SetupDiSetDeviceInstallParamsW(set, &device, &params);
        if (SetupDiBuildDriverInfoList(set, &device, SPDIT_COMPATDRIVER) &&
            SetupDiEnumDriverInfoW(set, &device, SPDIT_COMPATDRIVER, 0, &drv) &&
            (SetupDiCallClassInstaller(DIF_SELECTBESTCOMPATDRV, set, &device) || GetLastError() == ERROR_DI_DO_DEFAULT))
        {
            SetLastError(0);
            ret = pInstallSelectedDriver(NULL, set, NULL, FALSE, &flag);
            read_driver(set, &device, which, infpath);
            printf("selected ret=%d error=%lu reboot-flag=%lu which=%s\n", ret, ret ? 0 : GetLastError(), flag, which);
            SetLastError(0);
            ret = pInstallSelectedDriver(NULL, set, L"reserved", FALSE, &flag);
            printf("selected-reserved ret=%d error=%lu\n", ret, GetLastError());
        }
        else printf("selected=FAIL error=%lu\n", GetLastError());
    }

    SetupDiRemoveDevice(set, &device);
    SetupDiDestroyDeviceInfoList(set);
    return 0;
}
