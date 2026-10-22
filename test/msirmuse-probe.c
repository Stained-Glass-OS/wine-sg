/* msirmuse probe (patch 5952): a file in use while its product is uninstalled goes at the next
 * restart, as with Windows Installer -- MoveFileEx(MOVEFILE_DELAY_UNTIL_REBOOT), the removal returns
 * ERROR_SUCCESS_REBOOT_REQUIRED -- and so does its emptied folder. Kept, it stayed forever (DYMO
 * Connect's PnP service program after uninstall, qa4 2026-10-22).
 *
 *   msirmuse-probe.exe remove   install, hold the file open without sharing, uninstall
 *   msirmuse-probe.exe after    (after a restart, wineboot) the file and its folder are gone
 */
#include <windows.h>
#include <msi.h>
#include <msiquery.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

static int checks, fails;
#define OK(c, ...) do { checks++; if (!(c)) { fails++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); fflush(stdout); } } while (0)

static const char *silent_pc = "{5E3C0A55-7B1D-4D1E-9A00-5952000000A1}";

static UINT run(MSIHANDLE db, const char *fmt, ...)
{
    char buf[1024];
    MSIHANDLE view;
    UINT r;
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    r = MsiDatabaseOpenViewA(db, buf, &view);
    if (r) return r;
    r = MsiViewExecute(view, 0);
    if (!r) MsiViewClose(view);
    MsiCloseHandle(view);
    return r;
}

static void make_package(const char *file)
{
    MSIHANDLE db = 0;
    UINT r = MsiOpenDatabaseA(file, MSIDBOPEN_CREATE, &db);
    MSIHANDLE si;
    OK(!r, "create package: %u", r);
    MsiGetSummaryInformationA(db, NULL, 7, &si);
    MsiSummaryInfoSetPropertyA(si, 2, VT_LPSTR, 0, NULL, "Installation Database");
    MsiSummaryInfoSetPropertyA(si, 3, VT_LPSTR, 0, NULL, "In-use removal test");
    MsiSummaryInfoSetPropertyA(si, 4, VT_LPSTR, 0, NULL, "Wine Hackers");
    MsiSummaryInfoSetPropertyA(si, 7, VT_LPSTR, 0, NULL, ";1033");
    MsiSummaryInfoSetPropertyA(si, 9, VT_LPSTR, 0, NULL, "{913B8D18-FBB6-4CAC-A239-C74C11E3FA75}");
    MsiSummaryInfoSetPropertyA(si, 14, VT_I4, 100, NULL, NULL);
    MsiSummaryInfoSetPropertyA(si, 15, VT_I4, 0, NULL, NULL);
    MsiSummaryInfoPersist(si);
    MsiCloseHandle(si);
    run(db, "CREATE TABLE `Property` (`Property` CHAR(72) NOT NULL, `Value` LONGCHAR NOT NULL PRIMARY KEY `Property`)");
    run(db, "INSERT INTO `Property` (`Property`, `Value`) VALUES ('ProductCode', '%s')", silent_pc);
    run(db, "INSERT INTO `Property` (`Property`, `Value`) VALUES ('ProductName', 'SG In-use Removal Test')");
    run(db, "INSERT INTO `Property` (`Property`, `Value`) VALUES ('ProductVersion', '1.0.0')");
    run(db, "INSERT INTO `Property` (`Property`, `Value`) VALUES ('ProductLanguage', '1033')");
    run(db, "INSERT INTO `Property` (`Property`, `Value`) VALUES ('Manufacturer', 'Wine')");
    run(db, "INSERT INTO `Property` (`Property`, `Value`) VALUES ('ALLUSERS', '1')");
    run(db, "CREATE TABLE `Directory` (`Directory` CHAR(255) NOT NULL, `Directory_Parent` CHAR(255), `DefaultDir` CHAR(255) NOT NULL PRIMARY KEY `Directory`)");
    run(db, "INSERT INTO `Directory` (`Directory`, `DefaultDir`) VALUES ('TARGETDIR', 'SourceDir')");
    run(db, "INSERT INTO `Directory` (`Directory`, `Directory_Parent`, `DefaultDir`) VALUES ('INSTALLDIR', 'TARGETDIR', 'sgtarget:.')");
    run(db, "CREATE TABLE `Component` (`Component` CHAR(72) NOT NULL, `ComponentId` CHAR(38), `Directory_` CHAR(72) NOT NULL, `Attributes` SHORT NOT NULL, `Condition` CHAR(255), `KeyPath` CHAR(72) PRIMARY KEY `Component`)");
    run(db, "INSERT INTO `Component` (`Component`, `ComponentId`, `Directory_`, `Attributes`, `KeyPath`) VALUES ('comp', '{A1B2C3D4-0000-4000-8000-000000000032}', 'INSTALLDIR', 0, 'file.txt')");
    run(db, "CREATE TABLE `Feature` (`Feature` CHAR(38) NOT NULL, `Feature_Parent` CHAR(38), `Title` CHAR(64), `Description` CHAR(255), `Display` SHORT, `Level` SHORT NOT NULL, `Directory_` CHAR(72), `Attributes` SHORT NOT NULL PRIMARY KEY `Feature`)");
    run(db, "INSERT INTO `Feature` (`Feature`, `Title`, `Level`, `Directory_`, `Attributes`) VALUES ('feat', 'Feature', 1, 'INSTALLDIR', 0)");
    run(db, "CREATE TABLE `FeatureComponents` (`Feature_` CHAR(38) NOT NULL, `Component_` CHAR(72) NOT NULL PRIMARY KEY `Feature_`, `Component_`)");
    run(db, "INSERT INTO `FeatureComponents` (`Feature_`, `Component_`) VALUES ('feat', 'comp')");
    run(db, "CREATE TABLE `File` (`File` CHAR(72) NOT NULL, `Component_` CHAR(72) NOT NULL, `FileName` CHAR(255) NOT NULL, `FileSize` LONG NOT NULL, `Version` CHAR(72), `Language` CHAR(20), `Attributes` SHORT, `Sequence` SHORT NOT NULL PRIMARY KEY `File`)");
    run(db, "INSERT INTO `File` (`File`, `Component_`, `FileName`, `FileSize`, `Attributes`, `Sequence`) VALUES ('file.txt', 'comp', 'file.txt', 11, 0, 1)");
    run(db, "CREATE TABLE `Media` (`DiskId` SHORT NOT NULL, `LastSequence` SHORT NOT NULL, `DiskPrompt` CHAR(64), `Cabinet` CHAR(255), `VolumeLabel` CHAR(32), `Source` CHAR(32) PRIMARY KEY `DiskId`)");
    run(db, "INSERT INTO `Media` (`DiskId`, `LastSequence`) VALUES (1, 1)");
    run(db, "CREATE TABLE `InstallExecuteSequence` (`Action` CHAR(72) NOT NULL, `Condition` CHAR(255), `Sequence` SHORT PRIMARY KEY `Action`)");
    {
        static const struct { const char *a; int n; } seq[] = {
            { "CostInitialize", 800 }, { "FileCost", 900 }, { "CostFinalize", 1000 }, { "InstallValidate", 1400 },
            { "InstallInitialize", 1500 }, { "ProcessComponents", 1600 }, { "InstallFiles", 4000 }, { "RemoveFiles", 3500 },
            { "RegisterProduct", 6100 }, { "PublishFeatures", 6300 }, { "PublishProduct", 6400 }, { "InstallFinalize", 6600 } };
        unsigned i;
        for (i = 0; i < sizeof(seq) / sizeof(seq[0]); i++)
            run(db, "INSERT INTO `InstallExecuteSequence` (`Action`, `Sequence`) VALUES ('%s', %d)", seq[i].a, seq[i].n);
    }
    MsiDatabaseCommit(db);
    MsiCloseHandle(db);
}

static void put_file(const char *path, const char *content)
{
    FILE *f = fopen(path, "wb");
    if (f) { fputs(content, f); fclose(f); }
}

/* the PendingFileRenameOperations pairs as "source>dest|" */
static void pending(char *out, int size)
{
    WCHAR buf[8192];
    DWORD sz = sizeof(buf) - 2 * sizeof(WCHAR), type, i, n = 0;
    HKEY key;

    out[0] = 0;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SYSTEM\\CurrentControlSet\\Control\\Session Manager", 0, KEY_READ, &key)) return;
    memset(buf, 0, sizeof(buf));
    if (!RegQueryValueExW(key, L"PendingFileRenameOperations", NULL, &type, (BYTE *)buf, &sz) && type == REG_MULTI_SZ)
    {
        WCHAR *p = buf;
        while (*p && n < (DWORD)size - 600)
        {
            WCHAR *dst = p + lstrlenW(p) + 1;
            n += snprintf(out + n, size - n, "%ls>%ls|", p, dst);
            p = dst + lstrlenW(dst) + 1;
        }
    }
    (void)i;
    RegCloseKey(key);
}

int main(int argc, char **argv)
{
    char buf[8192];
    UINT r;

    setvbuf(stdout, NULL, _IONBF, 0);
    if (argc > 1 && !strcmp(argv[1], "remove"))
    {
        HANDLE hold;
        const char *pkg = "C:\\sgsrc\\msirmuse.msi";
        char *a, *b;

        CreateDirectoryA("C:\\sgsrc", NULL);
        put_file("C:\\sgsrc\\file.txt", "hello world");
        DeleteFileA(pkg);
        make_package(pkg);
        MsiSetInternalUI(INSTALLUILEVEL_NONE, NULL);
        r = MsiInstallProductA(pkg, "TARGETDIR=C:\\sgdir ALLUSERS=1 REBOOT=ReallySuppress");
        OK(r == ERROR_SUCCESS, "install: %u", r);
        OK(GetFileAttributesA("C:\\sgdir\\sgtarget\\file.txt") != INVALID_FILE_ATTRIBUTES, "the file is installed");

        /* the person's program has it open, sharing nothing (a running program's image is the same to DeleteFile) */
        hold = CreateFileA("C:\\sgdir\\sgtarget\\file.txt", GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);
        OK(hold != INVALID_HANDLE_VALUE, "hold the file: %lu", GetLastError());
        /* the package keeps no record of TARGETDIR: given again, as an installer of its own folder finds it (AppSearch) */
        r = MsiConfigureProductExA(silent_pc, 0, INSTALLSTATE_ABSENT, "TARGETDIR=C:\\sgdir REBOOT=ReallySuppress");
        OK(r == ERROR_SUCCESS_REBOOT_REQUIRED, "uninstall with the file in use: %u (Windows: 3010, ERROR_SUCCESS_REBOOT_REQUIRED)", r);
        OK(MsiQueryProductStateA(silent_pc) == INSTALLSTATE_UNKNOWN, "the product is uninstalled: %d", MsiQueryProductStateA(silent_pc));
        pending(buf, sizeof(buf));
        printf("# pending: %s\n", buf);
        a = strstr(buf, "sgtarget\\file.txt>|");
        b = strstr(buf, "sgtarget\\>|");
        OK(a != NULL, "the file in use is queued for deletion at the restart: %s", buf);
        OK(b != NULL && (!a || b > a), "its folder is queued after it: %s", buf);
        CloseHandle(hold);
        OK(GetFileAttributesA("C:\\sgdir\\sgtarget\\file.txt") != INVALID_FILE_ATTRIBUTES, "until then the file stays");
        DeleteFileA(pkg);
    }
    else if (argc > 1 && !strcmp(argv[1], "after"))
    {
        OK(GetFileAttributesA("C:\\sgdir\\sgtarget\\file.txt") == INVALID_FILE_ATTRIBUTES, "after the restart the file is gone");
        OK(GetFileAttributesA("C:\\sgdir\\sgtarget") == INVALID_FILE_ATTRIBUTES, "and its folder");
        pending(buf, sizeof(buf));
        OK(!strstr(buf, "sgtarget"), "nothing of it is pending any more: %s", buf);
    }
    else { printf("usage: msirmuse-probe.exe remove|after\n"); return 2; }
    printf("checks %d, failures %d\n", checks, fails);
    printf("RESULT: %s\n", fails ? "FAIL" : "PASS");
    return fails != 0;
}
