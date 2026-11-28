/* test/nocode2-gate.sh: exports that had no code, batch 2 (patches/sg/8015-8016).
 *   8015 rasdlg RasDialDlgA/W (mRemoteNG's delay import)
 *   8016 newdev DiUninstallDevice (Chromium's interactive_ui_tests import)
 * Run by the prefix's owner (an administrator); the standard-user case on a thread without the
 * Administrators group. */
#include <windows.h>
#include <stdio.h>
#include <ras.h>
#include <raserror.h>
#include <rasdlg.h>
#include <setupapi.h>
#include <cfgmgr32.h>

static int failures;
#define CHECK(c, ...) do { if (c) { printf("ok    "); printf(__VA_ARGS__); printf("\n"); } \
                           else { failures++; printf("FAIL  "); printf(__VA_ARGS__); printf("\n"); } } while (0)

static void *get(const char *dll, const char *name)
{
    HMODULE m = LoadLibraryA(dll);
    void *p = m ? (void *)GetProcAddress(m, name) : NULL;
    if (!p) { failures++; printf("FAIL  %s!%s not found\n", dll, name); }
    return p;
}

static void test_rasdialdlg(void)
{
    BOOL (WINAPI *pRasDialDlgW)(LPWSTR, LPWSTR, LPWSTR, RASDIALDLG *) = get("rasdlg.dll", "RasDialDlgW");
    BOOL (WINAPI *pRasDialDlgA)(LPSTR, LPSTR, LPSTR, RASDIALDLG *) = get("rasdlg.dll", "RasDialDlgA");
    RASDIALDLG info;
    RASENTRYW entry;
    WCHAR pbk[MAX_PATH];
    DWORD ret;
    BOOL ok;

    if (!pRasDialDlgW || !pRasDialDlgA) return;

    memset(&info, 0, sizeof(info));
    info.dwSize = sizeof(info) - 1;
    ok = pRasDialDlgW(NULL, (WCHAR *)L"sg nocode2", NULL, &info);
    CHECK(!ok && info.dwError == ERROR_INVALID_SIZE, "a structure of another size: %d %lu", ok, info.dwError);
    info.dwSize = sizeof(info);
    ok = pRasDialDlgW(NULL, NULL, NULL, &info);
    CHECK(!ok && info.dwError == ERROR_INVALID_PARAMETER, "no entry: %d %lu", ok, info.dwError);
    ok = pRasDialDlgW(NULL, (WCHAR *)L"sg no such entry", NULL, &info);
    CHECK(!ok && info.dwError == ERROR_CANNOT_FIND_PHONEBOOK_ENTRY, "an entry that does not exist: %d %lu", ok, info.dwError);
    info.dwError = 0;
    ok = pRasDialDlgA(NULL, (char *)"sg no such entry", NULL, &info);
    CHECK(!ok && info.dwError == ERROR_CANNOT_FIND_PHONEBOOK_ENTRY, "RasDialDlgA, the same: %d %lu", ok, info.dwError);

    /* an entry of a phone book of our own: found, but there is no dialler for it here */
    GetTempPathW(MAX_PATH, pbk);
    wcscat(pbk, L"nocode2.pbk");
    DeleteFileW(pbk);
    memset(&entry, 0, sizeof(entry));
    entry.dwSize = sizeof(entry);
    entry.dwfOptions = RASEO_RemoteDefaultGateway;
    entry.dwfNetProtocols = RASNP_Ip;
    entry.dwFramingProtocol = RASFP_Ppp;
    wcscpy(entry.szLocalPhoneNumber, L"5551234");
    wcscpy(entry.szDeviceType, L"modem");
    wcscpy(entry.szDeviceName, L"Standard Modem");
    entry.dwType = RASET_Phone;
    ret = RasSetEntryPropertiesW(pbk, L"sg nocode2", &entry, sizeof(entry), NULL, 0);
    CHECK(ret == 0, "RasSetEntryProperties %lu", ret);
    info.dwError = 0;
    ok = pRasDialDlgW(pbk, (WCHAR *)L"sg nocode2", NULL, &info);
    CHECK(!ok && info.dwError != 0, "the entry is dialled and the failure reported: %d %lu", ok, info.dwError);
    DeleteFileW(pbk);
}

static HANDLE restricted_thread_token(void)
{
    HANDLE proc, restricted, imp = NULL;
    SID_IDENTIFIER_AUTHORITY nt = { SECURITY_NT_AUTHORITY };
    PSID admins;
    SID_AND_ATTRIBUTES sa;

    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ALL_ACCESS, &proc)) return NULL;
    AllocateAndInitializeSid(&nt, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &admins);
    sa.Sid = admins; sa.Attributes = 0;
    if (CreateRestrictedToken(proc, DISABLE_MAX_PRIVILEGE, 1, &sa, 0, NULL, 0, NULL, &restricted))
    {
        DuplicateTokenEx(restricted, TOKEN_ALL_ACCESS, NULL, SecurityImpersonation, TokenImpersonation, &imp);
        CloseHandle(restricted);
    }
    CloseHandle(proc);
    FreeSid(admins);
    return imp;
}

static const GUID class_guid = {0x5d6a8a1e,0x2b8e,0x4d7c,{0x9a,0x51,0x3e,0x6f,0x10,0x22,0x33,0x10}};

static void test_diuninstall(void)
{
    BOOL (WINAPI *pDiUninstallDevice)(HWND, HDEVINFO, SP_DEVINFO_DATA *, DWORD, BOOL *) = get("newdev.dll", "DiUninstallDevice");
    SP_DEVINFO_DATA data = { sizeof(data) }, other = { sizeof(other) };
    HDEVINFO set, set2;
    BOOL reboot = TRUE, ok;
    HANDLE imp;
    DWORD err;

    if (!pDiUninstallDevice) return;
    set = SetupDiCreateDeviceInfoList(&class_guid, NULL);
    ok = SetupDiCreateDeviceInfoW(set, L"Root\\SGNOCODE2\\0000", &class_guid, NULL, NULL, 0, &data);
    CHECK(ok, "SetupDiCreateDeviceInfo %#lx", GetLastError());
    ok = SetupDiRegisterDeviceInfo(set, &data, 0, NULL, NULL, NULL);
    CHECK(ok, "SetupDiRegisterDeviceInfo %#lx", GetLastError());

    SetLastError(0xdeadbeef);
    ok = pDiUninstallDevice(NULL, set, &data, 1, &reboot);
    err = GetLastError();
    CHECK(!ok && err == ERROR_INVALID_FLAGS && !reboot, "flags 1: %d %lu", ok, err);
    SetLastError(0xdeadbeef);
    ok = pDiUninstallDevice(NULL, set, NULL, 0, &reboot);
    CHECK(!ok && GetLastError() == ERROR_INVALID_PARAMETER, "no device: %lu", GetLastError());

    imp = restricted_thread_token();
    if (imp && SetThreadToken(NULL, imp))
    {
        SetLastError(0xdeadbeef);
        ok = pDiUninstallDevice(NULL, set, &data, 0, &reboot);
        err = GetLastError();
        RevertToSelf();
        CHECK(!ok && err == ERROR_ACCESS_DENIED, "a standard user: %d %lu", ok, err);
    }
    else CHECK(0, "a thread without the Administrators group");
    if (imp) CloseHandle(imp);

    set2 = SetupDiCreateDeviceInfoList(&class_guid, NULL);
    ok = SetupDiOpenDeviceInfoW(set2, L"ROOT\\SGNOCODE2\\0000", NULL, 0, &other);
    CHECK(ok, "the device is still there");
    SetupDiDestroyDeviceInfoList(set2);

    reboot = TRUE;
    ok = pDiUninstallDevice(NULL, set, &data, 0, &reboot);
    CHECK(ok && !reboot, "DiUninstallDevice: %d %#lx, reboot %d", ok, ok ? 0 : GetLastError(), reboot);
    SetupDiDestroyDeviceInfoList(set);

    set2 = SetupDiCreateDeviceInfoList(&class_guid, NULL);
    SetLastError(0xdeadbeef);
    ok = SetupDiOpenDeviceInfoW(set2, L"ROOT\\SGNOCODE2\\0000", NULL, 0, &other);
    CHECK(!ok && GetLastError() == ERROR_NO_SUCH_DEVINST, "the device is gone: %d %#lx", ok, GetLastError());
    SetupDiDestroyDeviceInfoList(set2);
}

int main(void)
{
    test_rasdialdlg();
    test_diuninstall();
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
