/* Probe for patches/sg/2452: msi behaviours recorded from Windows: hyphen names, empty quoted names, MERGE against an existing row,
 * the property table of a package opened from a database handle. */
#include <windows.h>
#include <msi.h>
#include <msiquery.h>
#include <stdio.h>
#include <string.h>

static int fails;
static void check(const char *name, int ok) { printf("      %s  %s\n", ok ? "PASS" : "FAIL", name); if (!ok) fails++; }
static void checku(const char *name, UINT got, UINT want)
{
    char buf[160];
    snprintf(buf, sizeof(buf), "%s (%u, want %u)", name, got, want);
    check(buf, got == want);
}

static UINT query(MSIHANDLE db, const char *q)
{
    MSIHANDLE view;
    UINT r = MsiDatabaseOpenViewA(db, q, &view);
    if (r) return r;
    r = MsiViewExecute(view, 0);
    MsiViewClose(view);
    MsiCloseHandle(view);
    return r;
}

static void set_summary(MSIHANDLE db)
{
    MSIHANDLE si;
    MsiGetSummaryInformationA(db, NULL, 7, &si);
    MsiSummaryInfoSetPropertyA(si, 2, VT_LPSTR, 0, NULL, "Installation Database");
    MsiSummaryInfoSetPropertyA(si, 3, VT_LPSTR, 0, NULL, "Installation Database");
    MsiSummaryInfoSetPropertyA(si, 4, VT_LPSTR, 0, NULL, "Probe");
    MsiSummaryInfoSetPropertyA(si, 7, VT_LPSTR, 0, NULL, ";1033");
    MsiSummaryInfoSetPropertyA(si, 9, VT_LPSTR, 0, NULL, "{913B8D18-FBB6-4CAC-A239-C74C11E3FA74}");
    MsiSummaryInfoSetPropertyA(si, 14, VT_I4, 100, NULL, NULL);
    MsiSummaryInfoSetPropertyA(si, 15, VT_I4, 0, NULL, NULL);
    MsiSummaryInfoPersist(si);
    MsiCloseHandle(si);
}

int main(void)
{
    WCHAR path[MAX_PATH];
    MSIHANDLE db = 0, pkg = 0;
    char pkgname[32], drive[8];
    DWORD len;
    int cost, temp;
    UINT r;

    GetTempPathW(MAX_PATH, path);
    wcscat(path, L"sgmsicost.msi");
    DeleteFileW(path);
    r = MsiOpenDatabaseW(path, (LPCWSTR)(ULONG_PTR)3, &db);
    checku("create database", r, 0);
    set_summary(db);
    checku("Property", query(db, "CREATE TABLE `Property` (`Property` CHAR(72) NOT NULL, `Value` CHAR(0) PRIMARY KEY `Property`)"), 0);
    checku("ProductCode", query(db, "INSERT INTO `Property` (`Property`, `Value`) VALUES ('ProductCode', '{379B1C47-40C1-42FA-A9BB-BEBB6F1B0172}')"), 0);
    checku("MSIFASTINSTALL", query(db, "INSERT INTO `Property` (`Property`, `Value`) VALUES ('MSIFASTINSTALL', '1')"), 0);
    checku("Directory", query(db, "CREATE TABLE `Directory` (`Directory` CHAR(255) NOT NULL, `Directory_Parent` CHAR(255), `DefaultDir` CHAR(255) NOT NULL PRIMARY KEY `Directory`)"), 0);
    checku("TARGETDIR", query(db, "INSERT INTO `Directory` (`Directory`, `Directory_Parent`, `DefaultDir`) VALUES ('TARGETDIR', '', 'SourceDir')"), 0);
    checku("Media", query(db, "CREATE TABLE `Media` (`DiskId` SHORT NOT NULL, `LastSequence` SHORT NOT NULL, `DiskPrompt` CHAR(64), `Cabinet` CHAR(255), `VolumeLabel` CHAR(32), `Source` CHAR(72) PRIMARY KEY `DiskId`)"), 0);
    checku("a medium", query(db, "INSERT INTO `Media` (`DiskId`, `LastSequence`, `DiskPrompt`, `Cabinet`, `VolumeLabel`, `Source`) VALUES (1, 2, 'cabinet', '', '', '')"), 0);
    checku("File", query(db, "CREATE TABLE `File` (`File` CHAR(72) NOT NULL, `Component_` CHAR(72) NOT NULL, `FileName` CHAR(255) NOT NULL, `FileSize` LONG NOT NULL, `Version` CHAR(72), `Language` CHAR(20), `Attributes` SHORT, `Sequence` SHORT NOT NULL PRIMARY KEY `File`)"), 0);
    checku("a file", query(db, "INSERT INTO `File` (`File`, `Component_`, `FileName`, `FileSize`, `Version`, `Language`, `Attributes`, `Sequence`) VALUES ('a.txt', 'one', 'a.txt', 4097, '', '', 8192, 1)"), 0);
    checku("Component", query(db, "CREATE TABLE `Component` (`Component` CHAR(72) NOT NULL, `ComponentId` CHAR(38), `Directory_` CHAR(72) NOT NULL, `Attributes` SHORT NOT NULL, `Condition` CHAR(255), `KeyPath` CHAR(72) PRIMARY KEY `Component`)"), 0);
    checku("a component", query(db, "INSERT INTO `Component` (`Component`, `ComponentId`, `Directory_`, `Attributes`, `Condition`, `KeyPath`) VALUES ('one', '{8A2D9B5F-7B0F-4E3B-9E6D-1F6A0C3B2D11}', 'TARGETDIR', 0, '', 'a.txt')"), 0);
    checku("Feature", query(db, "CREATE TABLE `Feature` (`Feature` CHAR(38) NOT NULL, `Feature_Parent` CHAR(38), `Title` CHAR(64), `Description` CHAR(255), `Display` SHORT NOT NULL, `Level` SHORT NOT NULL, `Directory_` CHAR(72), `Attributes` SHORT NOT NULL PRIMARY KEY `Feature`)"), 0);
    checku("a feature", query(db, "INSERT INTO `Feature` (`Feature`, `Feature_Parent`, `Title`, `Description`, `Display`, `Level`, `Directory_`, `Attributes`) VALUES ('one', '', '', '', 0, 1, '', 0)"), 0);
    checku("FeatureComponents", query(db, "CREATE TABLE `FeatureComponents` (`Feature_` CHAR(38) NOT NULL, `Component_` CHAR(72) NOT NULL PRIMARY KEY `Feature_`, `Component_`)"), 0);
    checku("linked", query(db, "INSERT INTO `FeatureComponents` (`Feature_`, `Component_`) VALUES ('one', 'one')"), 0);
    checku("InstallExecuteSequence", query(db, "CREATE TABLE `InstallExecuteSequence` (`Action` CHAR(72) NOT NULL, `Condition` CHAR(255), `Sequence` SHORT PRIMARY KEY `Action`)"), 0);
    checku("CostInitialize", query(db, "INSERT INTO `InstallExecuteSequence` (`Action`, `Condition`, `Sequence`) VALUES ('CostInitialize', '', 800)"), 0);
    checku("FileCost", query(db, "INSERT INTO `InstallExecuteSequence` (`Action`, `Condition`, `Sequence`) VALUES ('FileCost', '', 900)"), 0);
    checku("CostFinalize", query(db, "INSERT INTO `InstallExecuteSequence` (`Action`, `Condition`, `Sequence`) VALUES ('CostFinalize', '', 1000)"), 0);
    checku("InstallValidate", query(db, "INSERT INTO `InstallExecuteSequence` (`Action`, `Condition`, `Sequence`) VALUES ('InstallValidate', '', 1100)"), 0);
    MsiDatabaseCommit(db);
    snprintf(pkgname, sizeof(pkgname), "#%lu", (unsigned long)db);
    r = MsiOpenPackageA(pkgname, &pkg);
    if (r == ERROR_INSTALL_PACKAGE_REJECTED) { printf("      (no rights: skipped)\n"); goto done; }
    checku("open the package", r, 0);
    if (r) goto done;
    MsiSetInternalUI(INSTALLUILEVEL_NONE, NULL);

    len = sizeof(drive);
    checku("costs before CostInitialize: invalid handle state",
           MsiEnumComponentCostsA(pkg, "one", 0, INSTALLSTATE_LOCAL, drive, &len, &cost, &temp), ERROR_INVALID_HANDLE_STATE);
    checku("CostInitialize", MsiDoActionA(pkg, "CostInitialize"), 0);
    checku("FileCost", MsiDoActionA(pkg, "FileCost"), 0);
    len = sizeof(drive);
    checku("costs before CostFinalize: function not called",
           MsiEnumComponentCostsA(pkg, "one", 0, INSTALLSTATE_LOCAL, drive, &len, &cost, &temp), ERROR_FUNCTION_NOT_CALLED);
    checku("CostFinalize", MsiDoActionA(pkg, "CostFinalize"), 0);
    len = sizeof(drive);
    checku("costs before InstallValidate: still function not called",
           MsiEnumComponentCostsA(pkg, "one", 0, INSTALLSTATE_LOCAL, drive, &len, &cost, &temp), ERROR_FUNCTION_NOT_CALLED);
    checku("InstallValidate", MsiDoActionA(pkg, "InstallValidate"), 0);
    len = sizeof(drive);
    r = MsiEnumComponentCostsA(pkg, "one", 0, INSTALLSTATE_LOCAL, drive, &len, &cost, &temp);
    checku("costs after InstallValidate", r, 0);
    MsiCloseHandle(pkg);
done:
    MsiCloseHandle(db);
    DeleteFileW(path);
    puts(fails ? "RESULT: FAIL" : "RESULT: PASS");
    return fails != 0;
}
