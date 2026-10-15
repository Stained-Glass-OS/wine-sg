#include <windows.h>
#include <setupapi.h>
#include <newdev.h>
#include <stdio.h>
#include <wchar.h>
static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)
static const GUID sysclass = {0x4d36e97d, 0xe325, 0x11ce, {0xbf, 0xc1, 0x08, 0x00, 0x2b, 0xe1, 0x03, 0x18}};

static void write_inf_id(const char *dir, const char *name, const char *date, const char *ver, int marker, const char *id)
{
    char path[MAX_PATH]; FILE *f;
    sprintf(path, "%s\\%s", dir, name);
    f = fopen(path, "w");
    fprintf(f, "[Version]\nSignature=\"$WINDOWS NT$\"\nClass=System\nClassGuid={4D36E97D-E325-11CE-BFC1-08002BE10318}\n"
               "Provider=%%P%%\nDriverVer=%s,%s\n\n[Manufacturer]\n%%M%%=Mfg,NTamd64\n\n[Mfg]\n%%D%%=Inst,%s\n"
               "[Mfg.NTamd64]\n%%D%%=Inst,%s\n\n[Inst]\nAddReg=Reg\n\n[Reg]\nHKLM,Software\\SGDiTest,Marker,0x10001,%d\n"
               "HKLM,Software\\SGDiTest,Count,0x10008,1\n\n[Strings]\nP=\"SG\"\nM=\"SG\"\nD=\"SG DiInstall test device\"\n",
            date, ver, id, id, marker);
    fclose(f);
}
static void write_inf(const char *dir, const char *name, const char *date, const char *ver, int marker)
{
    write_inf_id(dir, name, date, ver, marker, "ROOT\\SG_DITEST");
}
static DWORD reg_marker(void)
{
    HKEY k; DWORD v = 0, n = sizeof(v);
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "Software\\SGDiTest", 0, KEY_READ, &k)) return 0xffffffff;
    if (RegQueryValueExA(k, "Marker", NULL, NULL, (BYTE *)&v, &n)) v = 0xfffffffe;
    RegCloseKey(k); return v;
}
int main(int argc, char **argv)
{
    char dir[MAX_PATH], inf[MAX_PATH];
    WCHAR winf[MAX_PATH];
    HDEVINFO set; SP_DEVINFO_DATA dev = {sizeof(dev)};
    BOOL reboot = TRUE, ret;
    HMODULE nd = LoadLibraryA("newdev.dll");
    BOOL (WINAPI *diw)(HWND, const WCHAR *, DWORD, BOOL *) = (void *)GetProcAddress(nd, "DiInstallDriverW");
    BOOL (WINAPI *dia)(HWND, const char *, DWORD, BOOL *) = (void *)GetProcAddress(nd, "DiInstallDriverA");

    GetTempPathA(sizeof(dir), dir); strcat(dir, "sgditest"); CreateDirectoryA(dir, NULL);
    sprintf(inf, "%s\\a.inf", dir);
    MultiByteToWideChar(CP_ACP, 0, inf, -1, winf, MAX_PATH);

    if (argc > 1 && !strcmp(argv[1], "wow64"))
    {
        write_inf(dir, "a.inf", "01/01/2020", "1.0.0.0", 1);
        SetLastError(0); ret = diw(NULL, winf, 0, NULL);
        CHECK(!ret && GetLastError() == ERROR_IN_WOW64, "wow64: %d %lu", ret, GetLastError());
        printf(fails ? "RESULT: FAIL\n" : "RESULT: PASS\n");
        return fails != 0;
    }

    SetLastError(0); ret = diw(NULL, L"C:\\no\\such\\file.inf", 0, NULL);
    CHECK(!ret && GetLastError() == ERROR_FILE_NOT_FOUND, "missing inf: %d %lu", ret, GetLastError());
    write_inf(dir, "a.inf", "01/01/2020", "1.0.0.0", 1);
    SetLastError(0); ret = diw(NULL, winf, 0x100, NULL);
    CHECK(!ret && GetLastError() == ERROR_INVALID_FLAGS, "bad flags: %d %lu", ret, GetLastError());
    SetLastError(0); ret = diw(NULL, NULL, 0, NULL);
    CHECK(!ret && GetLastError() == ERROR_INVALID_PARAMETER, "NULL path: %d %lu", ret, GetLastError());

    /* a present root device the INF supports */
    set = SetupDiCreateDeviceInfoList(&sysclass, NULL);
    CHECK(SetupDiCreateDeviceInfoW(set, L"SG_DITEST", &sysclass, NULL, NULL, DICD_GENERATE_ID, &dev), "create %lu", GetLastError());
    CHECK(SetupDiSetDeviceRegistryPropertyW(set, &dev, SPDRP_HARDWAREID, (const BYTE *)L"ROOT\\SG_DITEST\0", sizeof(L"ROOT\\SG_DITEST\0")), "hwid %lu", GetLastError());
    CHECK(SetupDiSetDeviceRegistryPropertyW(set, &dev, SPDRP_COMPATIBLEIDS, (const BYTE *)L"ROOT\\SG_DITEST_C\0", sizeof(L"ROOT\\SG_DITEST_C\0")), "compat %lu", GetLastError());
    CHECK(SetupDiCallClassInstaller(DIF_REGISTERDEVICE, set, &dev), "register %lu", GetLastError());

    {
        SP_DEVINSTALL_PARAMS_W dp = {sizeof(dp)};
        SP_DRVINFO_DATA_W di = {sizeof(di)};
        SP_DRVINSTALL_PARAMS ip = {sizeof(ip)};
        SYSTEMTIME st;

        write_inf(dir, "a.inf", "03/04/2020", "1.2.3.4", 1);
        SetupDiGetDeviceInstallParamsW(set, &dev, &dp);
        wcscpy(dp.DriverPath, winf); dp.Flags |= DI_ENUMSINGLEINF;
        SetupDiSetDeviceInstallParamsW(set, &dev, &dp);
        CHECK(SetupDiBuildDriverInfoList(set, &dev, SPDIT_COMPATDRIVER), "build list %lu", GetLastError());
        CHECK(SetupDiEnumDriverInfoW(set, &dev, SPDIT_COMPATDRIVER, 0, &di), "enum driver %lu", GetLastError());
        FileTimeToSystemTime(&di.DriverDate, &st);
        CHECK(st.wYear == 2020 && st.wMonth == 3 && st.wDay == 4, "DriverDate %u-%u-%u", st.wMonth, st.wDay, st.wYear);
        CHECK(di.DriverVersion == ((1ull << 48) | (2ull << 32) | (3ull << 16) | 4), "DriverVersion %I64x", di.DriverVersion);
        CHECK(SetupDiGetDriverInstallParamsW(set, &dev, &di, &ip) && ip.Rank == 0 && (ip.Flags & DNF_COMPATIBLE_DRIVER),
              "get params rank %lu flags %lx err %lu", ip.Rank, ip.Flags, GetLastError());
        ip.Rank = 5; ip.PrivateData = 0x1234;
        CHECK(SetupDiSetDriverInstallParamsW(set, &dev, &di, &ip), "set params %lu", GetLastError());
        ip.Rank = 0; ip.PrivateData = 0;
        CHECK(SetupDiGetDriverInstallParamsW(set, &dev, &di, &ip) && ip.Rank == 5 && ip.PrivateData == 0x1234, "params round trip %lu %Ix", ip.Rank, ip.PrivateData);
        ip.cbSize = 3;
        SetLastError(0);
        CHECK(!SetupDiGetDriverInstallParamsW(set, &dev, &di, &ip) && GetLastError() == ERROR_INVALID_USER_BUFFER, "bad cbSize %lu", GetLastError());
        SetupDiDestroyDriverInfoList(set, &dev, SPDIT_COMPATDRIVER);
        dp.DriverPath[0] = 0; dp.Flags &= ~DI_ENUMSINGLEINF;
        SetupDiSetDeviceInstallParamsW(set, &dev, &dp);
        write_inf(dir, "a.inf", "01/01/2020", "1.0.0.0", 1);
    }
    reboot = TRUE;
    ret = diw(NULL, winf, 0, &reboot);
    CHECK(ret, "install %lu", GetLastError());
    CHECK(reg_marker() == 1, "first install ran the INF: marker %lu", reg_marker());
    CHECK(reboot == FALSE, "no reboot");
    /* an older INF does not replace the driver, FORCE does */
    write_inf(dir, "a.inf", "01/01/2019", "0.9.0.0", 2);
    CHECK(diw(NULL, winf, 0, NULL), "older %lu", GetLastError());
    CHECK(reg_marker() == 1, "older INF left the driver alone: marker %lu", reg_marker());
    CHECK(diw(NULL, winf, DIIRFLAG_FORCE_INF, NULL), "forced %lu", GetLastError());
    CHECK(reg_marker() == 2, "forced older INF installed: marker %lu", reg_marker());
    /* a newer one does */
    write_inf(dir, "a.inf", "06/01/2021", "2.0.0.0", 3);
    CHECK(dia(NULL, inf, 0, NULL), "newer (A) %lu", GetLastError());
    CHECK(reg_marker() == 3, "newer INF installed: marker %lu", reg_marker());

    /* a driver that matches only a compatible ID does not beat one that matches the hardware ID, however new */
    write_inf_id(dir, "a.inf", "01/01/2023", "9.0.0.0", 4, "ROOT\\SG_DITEST_C");
    CHECK(diw(NULL, winf, 0, NULL), "compat-id INF %lu", GetLastError());
    CHECK(reg_marker() == 3, "compatible-ID INF did not replace the hardware-ID driver: marker %lu", reg_marker());

    SetupDiCallClassInstaller(DIF_REMOVE, set, &dev);
    SetupDiDestroyDeviceInfoList(set);
    RegDeleteKeyA(HKEY_LOCAL_MACHINE, "Software\\SGDiTest");
    printf(fails ? "RESULT: FAIL\n" : "RESULT: PASS\n");
    return fails != 0;
}
