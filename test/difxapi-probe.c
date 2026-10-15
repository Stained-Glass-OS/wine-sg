/* difxapi.dll (patches/sg/5504): DriverPackagePreinstall / Install / Uninstall /
 * GetPath, the log callbacks and INSTALLERINFO, against a device of its own and
 * small driver packages. One "name=value" line per fact for test/difxapi-gate.sh.
 *
 *   difxapi-probe.exe WORKDIR
 */
#include <windows.h>
#include <setupapi.h>
#include <wincrypt.h>
#include <mscat.h>
#include <stdio.h>
#include <string.h>

#ifndef DRIVER_PACKAGE_REPAIR
#define DRIVER_PACKAGE_REPAIR 0x1
#define DRIVER_PACKAGE_SILENT 0x2
#define DRIVER_PACKAGE_FORCE 0x4
#define DRIVER_PACKAGE_ONLY_IF_DEVICE_PRESENT 0x8
#define DRIVER_PACKAGE_LEGACY_MODE 0x10
#define DRIVER_PACKAGE_DELETE_FILES 0x20
#endif

typedef struct { WCHAR *app, *name, *product, *mfg; } INFO_W;
typedef struct { char *app, *name, *product, *mfg; } INFO_A;
typedef void (__cdecl *LOG_W)(int, DWORD, const WCHAR *, void *);
typedef void (__cdecl *LOG_A)(int, DWORD, const char *, void *);
typedef void (WINAPI *LOGS_W)(int, DWORD, const WCHAR *, void *);

static DWORD (WINAPI *pPreinstallW)(const WCHAR *, DWORD);
static DWORD (WINAPI *pPreinstallA)(const char *, DWORD);
static DWORD (WINAPI *pInstallW)(const WCHAR *, DWORD, const INFO_W *, BOOL *);
static DWORD (WINAPI *pInstallA)(const char *, DWORD, const INFO_A *, BOOL *);
static DWORD (WINAPI *pUninstallW)(const WCHAR *, DWORD, const INFO_W *, BOOL *);
static DWORD (WINAPI *pGetPathW)(const WCHAR *, WCHAR *, DWORD *);
static DWORD (WINAPI *pGetPathA)(const char *, char *, DWORD *);
static void (WINAPI *pSetLogW)(LOG_W, void *);
static void (WINAPI *pSetLogA)(LOG_A, void *);
static void (WINAPI *pSetDifxLogW)(LOGS_W, void *);

static const GUID system_class = {0x4d36e97e, 0xe325, 0x11ce, {0xbf, 0xc1, 0x08, 0x00, 0x2b, 0xe1, 0x03, 0x18}};
static WCHAR work[MAX_PATH];

/* --- the log --- */
static int log_events, log_errors, log_success, log_has_path;
static DWORD log_last_error;
static void *log_ctx_seen;
static WCHAR log_path[MAX_PATH];
static int loga_events, logs_events;

static void __cdecl log_w(int type, DWORD error, const WCHAR *text, void *ctx)
{
    log_events++;
    log_ctx_seen = ctx;
    if (type == 3) { log_errors++; log_last_error = error; }
    if (type == 0) log_success++;
    if (log_path[0] && wcsstr(text, log_path)) log_has_path = 1;
}

static void __cdecl log_a(int type, DWORD error, const char *text, void *ctx)
{
    (void)type; (void)error; (void)text; (void)ctx;
    loga_events++;
}

static void WINAPI log_s(int type, DWORD error, const WCHAR *text, void *ctx)
{
    (void)type; (void)error; (void)text; (void)ctx;
    logs_events++;
}

/* --- packages --- */
static void write_pkg(const WCHAR *sub, const WCHAR *name, const char *hwid, const char *which,
                      int catalog /* 0 none, 1 named and there, 2 named and missing */, int pnp)
{
    WCHAR dir[MAX_PATH], path[MAX_PATH], cat[MAX_PATH];
    FILE *f;

    swprintf(dir, MAX_PATH, L"%ls\\%ls", work, sub);
    CreateDirectoryW(dir, NULL);
    swprintf(path, MAX_PATH, L"%ls\\%ls", dir, name);
    f = _wfopen(path, L"w");
    fprintf(f, "[Version]\nSignature=\"$Windows NT$\"\nClass=System\nClassGuid={4d36e97e-e325-11ce-bfc1-08002be10318}\n"
               "Provider=%%Prov%%\nDriverVer=01/02/2024,1.0.0.0\n");
    if (catalog) fprintf(f, "CatalogFile=%ls.cat\n", name);
    if (pnp)
        fprintf(f, "[Manufacturer]\n%%Mfg%%=Models,NTamd64,NTx86\n"
                   "[Models.NTamd64]\n%%Dev%%=Install,%s\n[Models.NTx86]\n%%Dev%%=Install,%s\n"
                   "[Install]\nAddReg=Reg\n[Reg]\nHKR,,Which,,\"%s\"\n", hwid, hwid, which);
    else
        fprintf(f, "[DefaultInstall]\nAddReg=Reg\n[Reg]\nHKLM,Software\\SgNotPnp,x,,1\n");
    fprintf(f, "[Strings]\nProv=\"Stained Glass\"\nMfg=\"Stained Glass\"\nDev=\"SG test device\"\n");
    fclose(f);
    if (catalog == 1)
    {
        /* a catalog that is well formed, listing the INF; it is not signed: the
         * signature of a package is not checked (see the notes) */
        static const GUID inf_subject = {0xdeadbeef, 0x1, 0x2, {3, 4, 5, 6, 7, 8, 9, 10}};
        HANDLE catalog;
        GUID guid = inf_subject;

        swprintf(cat, MAX_PATH, L"%ls\\%ls.cat", dir, name);
        catalog = CryptCATOpen(cat, CRYPTCAT_OPEN_CREATENEW, 0, CRYPTCAT_VERSION_1, 0);
        if (catalog != INVALID_HANDLE_VALUE)
        {
            WCHAR file[MAX_PATH];

            wcscpy(file, name);
            CryptCATPutMemberInfo(catalog, file, L"0123456789ABCDEF0123456789ABCDEF01234567", &guid, 0, 0, NULL);
            CryptCATPersistStore(catalog);
            CryptCATClose(catalog);
        }
        else printf("catalog=FAIL %lu\n", GetLastError());
    }
}

static void pkg_path(WCHAR *out, const WCHAR *sub, const WCHAR *name)
{
    swprintf(out, MAX_PATH, L"%ls\\%ls\\%ls", work, sub, name);
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

static void read_which(HDEVINFO set, SP_DEVINFO_DATA *device, char *which)
{
    HKEY key = SetupDiOpenDevRegKey(set, device, DICS_FLAG_GLOBAL, 0, DIREG_DRV, KEY_READ);
    DWORD size = 64;

    strcpy(which, "-");
    if (key == INVALID_HANDLE_VALUE) return;
    if (RegQueryValueExA(key, "Which", NULL, NULL, (BYTE *)which, &size)) strcpy(which, "-");
    RegCloseKey(key);
}

static HDEVINFO dev_set;
static SP_DEVINFO_DATA dev;

static void show(const char *label, DWORD ret, int with_device)
{
    char which[64];

    if (with_device)
    {
        read_which(dev_set, &dev, which);
        printf("%s %#lx which=%s\n", label, ret, which);
    }
    else printf("%s %#lx\n", label, ret);
}

static INFO_W info1 = { L"app-one", L"App One", L"Product One", L"Maker One" };
static INFO_W info2 = { L"app-two", L"App Two", L"Product Two", L"Maker Two" };
static INFO_W info_bad = { L"app-bad", L"", L"Product", L"Maker" };

int main(int argc, char **argv)
{
    HMODULE difx = LoadLibraryA("difxapi.dll");
    WCHAR a[MAX_PATH], b[MAX_PATH], c[MAX_PATH], nodev[MAX_PATH], unsigned_inf[MAX_PATH], nocat[MAX_PATH], notpnp[MAX_PATH];
    WCHAR sysinf[MAX_PATH], path[MAX_PATH], toolong[400];
    char patha[MAX_PATH];
    BOOL wow64 = FALSE, reboot;
    DWORD ret, count, i;

    if (!difx || argc < 2) { printf("load=FAIL\n"); return 1; }
#define GET(var, name) var = (void *)GetProcAddress(difx, name)
    GET(pPreinstallW, "DriverPackagePreinstallW"); GET(pPreinstallA, "DriverPackagePreinstallA");
    GET(pInstallW, "DriverPackageInstallW"); GET(pInstallA, "DriverPackageInstallA");
    GET(pUninstallW, "DriverPackageUninstallW");
    GET(pGetPathW, "DriverPackageGetPathW"); GET(pGetPathA, "DriverPackageGetPathA");
    GET(pSetLogW, "DIFXAPISetLogCallbackW"); GET(pSetLogA, "DIFXAPISetLogCallbackA");
    GET(pSetDifxLogW, "SetDifxLogCallbackW");
    MultiByteToWideChar(CP_ACP, 0, argv[1], -1, work, MAX_PATH);
    IsWow64Process(GetCurrentProcess(), &wow64);
    printf("wow64=%d\n", wow64);

    pkg_path(a, L"a", L"sga.inf");
    pkg_path(b, L"b", L"sgb.inf");
    pkg_path(c, L"c", L"sgc.inf");
    pkg_path(nodev, L"n", L"sgn.inf");
    pkg_path(unsigned_inf, L"u", L"sgu.inf");
    pkg_path(nocat, L"m", L"sgm.inf");
    pkg_path(notpnp, L"p", L"sgp.inf");

    if (wow64)
    {
        write_pkg(L"a", L"sga.inf", "Root\\SGTEST", "A", 1, 1);
        ret = pPreinstallW(a, 0); printf("wow64-preinstall %#lx\n", ret);
        ret = pInstallW(a, 0, NULL, &reboot); printf("wow64-install %#lx\n", ret);
        ret = pUninstallW(a, 0, NULL, &reboot); printf("wow64-uninstall %#lx\n", ret);
        count = MAX_PATH; ret = pGetPathW(a, path, &count); printf("wow64-getpath %#lx\n", ret);
        return 0;
    }

    dev_set = SetupDiCreateDeviceInfoList(&system_class, NULL);
    dev.cbSize = sizeof(dev);
    if (!SetupDiCreateDeviceInfoW(dev_set, L"ROOT\\SYSTEM\\SGTEST1", &system_class, NULL, NULL, 0, &dev))
    { printf("device=FAIL create %lu\n", GetLastError()); return 1; }
    if (!SetupDiSetDeviceRegistryPropertyW(dev_set, &dev, SPDRP_HARDWAREID,
                                           (const BYTE *)L"Root\\SGTEST_SPECIFIC\0Root\\SGTEST\0",
                                           sizeof(L"Root\\SGTEST_SPECIFIC\0Root\\SGTEST\0")))
    { printf("device=FAIL property %lu\n", GetLastError()); return 1; }
    if (!SetupDiRegisterDeviceInfo(dev_set, &dev, 0, NULL, NULL, NULL))
    { printf("device=FAIL register %lu\n", GetLastError()); return 1; }
    printf("device=ok\nis-admin=%d\n", is_admin());

    write_pkg(L"a", L"sga.inf", "Root\\SGTEST", "A", 1, 1);                 /* second id */
    write_pkg(L"b", L"sgb.inf", "Root\\SGTEST_SPECIFIC", "B", 1, 1);        /* first id */
    write_pkg(L"n", L"sgn.inf", "Root\\SGTEST_NOBODY", "N", 1, 1);          /* no device */
    write_pkg(L"u", L"sgu.inf", "Root\\SGTEST", "U", 0, 1);                 /* no catalog line */
    write_pkg(L"m", L"sgm.inf", "Root\\SGTEST", "M", 2, 1);                 /* catalog missing */
    write_pkg(L"p", L"sgp.inf", "", "", 1, 0);                              /* not a PnP driver */
    SetCurrentDirectoryW(L"C:\\");

    /* --- arguments and paths --- */
    show("pre-null", pPreinstallW(NULL, 0), 0);
    show("pre-badflags", pPreinstallW(a, 0x1000), 0);
    show("pre-delete-files-flag", pPreinstallW(a, DRIVER_PACKAGE_DELETE_FILES), 0);
    show("pre-relative", pPreinstallW(L"sga.inf", 0), 0);
    show("pre-missing", pPreinstallW(L"C:\\no\\such\\dir\\x.inf", 0), 0);
    for (i = 0; i < 399; i++) toolong[i] = (i < 3) ? L"C:\\"[i] : 'x';
    toolong[399] = 0;
    show("pre-toolong", pPreinstallW(toolong, 0), 0);
    GetSystemWindowsDirectoryW(sysinf, MAX_PATH);
    lstrcatW(sysinf, L"\\inf\\input.inf");
    show("pre-in-system-inf", pPreinstallW(sysinf, 0), 0);
    show("pre-unsigned", pPreinstallW(unsigned_inf, 0), 0);
    show("pre-catalog-missing", pPreinstallW(nocat, 0), 0);
    show("pre-not-pnp", pPreinstallW(notpnp, 0), 0);

    /* --- preinstall and the store --- */
    show("pre-a", pPreinstallW(a, 0), 1);
    count = MAX_PATH;
    ret = pGetPathW(a, path, &count);
    printf("getpath-a %#lx filerepository=%d endswith-sga=%d count-is-len+1=%d\n", ret,
           !!wcsstr(path, L"FileRepository") || !!wcsstr(path, L"filerepository"),
           wcslen(path) > 8 && !wcsicmp(path + wcslen(path) - 8, L"\\sga.inf"), count == wcslen(path) + 1);
    count = 0;
    ret = pGetPathW(a, NULL, &count);
    printf("getpath-size %#lx count-is-len+1=%d\n", ret, count == wcslen(path) + 1);
    count = 5;
    ret = pGetPathW(a, path, &count);
    printf("getpath-small %#lx count-is-len\n", ret);
    show("pre-a-again", pPreinstallW(a, 0), 0);
    show("pre-a-repair", pPreinstallW(a, DRIVER_PACKAGE_REPAIR), 0);
    show("pre-a-silent", pPreinstallW(a, DRIVER_PACKAGE_SILENT), 0);
    show("pre-only-if-present-nodev", pPreinstallW(nodev, DRIVER_PACKAGE_ONLY_IF_DEVICE_PRESENT), 0);
    count = MAX_PATH;
    ret = pGetPathW(nodev, path, &count);
    show("getpath-nodev-not-stored", ret, 0);
    show("pre-n", pPreinstallW(nodev, 0), 0);
    count = MAX_PATH;
    show("getpath-nodev-stored", pGetPathW(nodev, path, &count), 0);
    count = MAX_PATH;
    show("getpath-missing", pGetPathW(L"C:\\no\\such\\dir\\x.inf", path, &count), 0);
    count = MAX_PATH;
    show("getpath-system-inf", pGetPathW(sysinf, path, &count), 0);
    show("getpath-null-count", pGetPathW(a, path, NULL), 0);

    /* --- install --- */
    show("bad-installerinfo", pInstallW(a, 0, &info_bad, &reboot), 0);
    reboot = TRUE;
    ret = pInstallW(a, 0, NULL, &reboot);
    {
        char which[64];

        read_which(dev_set, &dev, which);
        printf("install-a %#lx reboot=%d which=%s\n", ret, reboot, which);
    }
    show("install-b-better", pInstallW(b, 0, NULL, &reboot), 1);
    show("install-a-worse", pInstallW(a, 0, NULL, &reboot), 1);
    show("install-a-forced", pInstallW(a, DRIVER_PACKAGE_FORCE, NULL, &reboot), 1);
    show("install-only-if-present-forced-a", pInstallW(a, DRIVER_PACKAGE_ONLY_IF_DEVICE_PRESENT | DRIVER_PACKAGE_FORCE, NULL, &reboot), 1);
    show("install-nodev", pInstallW(nodev, 0, NULL, &reboot), 0);
    show("pre-unsigned-install", pInstallW(unsigned_inf, 0, NULL, &reboot), 0);
    show("pre-only-if-present-worse", pPreinstallW(a, DRIVER_PACKAGE_ONLY_IF_DEVICE_PRESENT), 0);

    /* the ANSI forms */
    WideCharToMultiByte(CP_ACP, 0, a, -1, patha, MAX_PATH, NULL, NULL);
    show("pre-a-ansi", pPreinstallA(patha, 0), 0);
    {
        INFO_A ia = { "app-ansi", "App Ansi", "Product Ansi", "Maker Ansi" };

        show("install-ansi-forced", pInstallA(patha, DRIVER_PACKAGE_FORCE, &ia, &reboot), 1);
    }
    {
        char buf[MAX_PATH];

        count = MAX_PATH;
        ret = pGetPathA(patha, buf, &count);
        printf("getpath-ansi %#lx endswith-sga=%d count-is-len+1=%d\n", ret,
               strlen(buf) > 8 && !stricmp(buf + strlen(buf) - 8, "\\sga.inf"), count == strlen(buf) + 1);
    }

    /* --- applications: the package is kept while another one uses it --- */
    write_pkg(L"c", L"sgc.inf", "Root\\SGTEST_NOBODY2", "C", 1, 1);
    show("app1-install", pInstallW(c, 0, &info1, &reboot), 0);
    show("app2-install", pInstallW(c, 0, &info2, &reboot), 0);
    show("app1-uninstall-others-remain", pUninstallW(c, 0, &info1, &reboot), 0);
    count = MAX_PATH;
    show("still-in-store", pGetPathW(c, path, &count), 0);
    show("app1-uninstall-again-others-remain", pUninstallW(c, 0, &info1, &reboot), 0);
    show("uninstall-bad-installerinfo", pUninstallW(c, 0, &info_bad, &reboot), 0);
    show("uninstall-bad-flags", pUninstallW(c, DRIVER_PACKAGE_REPAIR, &info2, &reboot), 0);
    show("app2-uninstall-last", pUninstallW(c, 0, &info2, &reboot), 0);
    count = MAX_PATH;
    show("gone-from-store", pGetPathW(c, path, &count), 0);
    show("uninstall-not-in-store", pUninstallW(c, 0, &info2, &reboot), 0);
    show("uninstall-missing-file", pUninstallW(L"C:\\no\\such\\dir\\x.inf", 0, NULL, &reboot), 0);
    /* forced, with another application still associated */
    write_pkg(L"c", L"sgc.inf", "Root\\SGTEST_NOBODY2", "C", 1, 1);
    pInstallW(c, 0, &info1, &reboot);
    pInstallW(c, 0, &info2, &reboot);
    show("uninstall-force-with-others", pUninstallW(c, DRIVER_PACKAGE_FORCE, &info1, &reboot), 0);
    count = MAX_PATH;
    show("gone-after-force", pGetPathW(c, path, &count), 0);
    /* a package with no application */
    show("plain-install", pInstallW(c, 0, NULL, &reboot), 0);
    show("plain-uninstall", pUninstallW(c, 0, NULL, &reboot), 0);

    /* --- the log callback --- */
    wcscpy(log_path, a);
    pSetLogW(log_w, (void *)0x1234);
    log_events = log_errors = log_success = log_has_path = 0;
    pPreinstallW(a, DRIVER_PACKAGE_REPAIR);
    printf("log-events-success=%d success-event=%d path-in-text=%d ctx=%d\n", log_events > 0, log_success > 0,
           log_has_path, log_ctx_seen == (void *)0x1234);
    log_events = log_errors = 0;
    pPreinstallW(L"C:\\no\\such\\dir\\x.inf", 0);
    printf("log-error-event=%d error-code=%lu\n", log_errors > 0, log_last_error);
    pSetLogA(log_a, NULL);
    log_events = loga_events = 0;
    pPreinstallW(a, DRIVER_PACKAGE_REPAIR);
    printf("log-replaced: old=%d new=%d\n", log_events, loga_events > 0);
    pSetDifxLogW(log_s, NULL);
    loga_events = logs_events = 0;
    pPreinstallW(a, DRIVER_PACKAGE_REPAIR);
    printf("log-stdcall-replaced: old=%d new=%d\n", loga_events, logs_events > 0);
    pSetDifxLogW(NULL, NULL);
    logs_events = 0;
    pPreinstallW(a, DRIVER_PACKAGE_REPAIR);
    printf("log-unregistered: events=%d\n", logs_events);

    /* --- the same calls as a user who is not an administrator --- */
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
            show("user-preinstall", pPreinstallW(a, 0), 0);
            show("user-install", pInstallW(a, 0, NULL, &reboot), 0);
            show("user-uninstall", pUninstallW(a, 0, NULL, &reboot), 0);
            count = MAX_PATH;
            show("user-getpath", pGetPathW(a, path, &count), 0);
            SetThreadToken(NULL, NULL);
        }
        else printf("restricted=FAIL %lu\n", GetLastError());
    }

    /* tidy: the package of the test device */
    pUninstallW(a, DRIVER_PACKAGE_FORCE, NULL, &reboot);
    SetupDiRemoveDevice(dev_set, &dev);
    SetupDiDestroyDeviceInfoList(dev_set);
    return 0;
}
