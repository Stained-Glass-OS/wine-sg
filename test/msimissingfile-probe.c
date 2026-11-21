/* msimissingfile-probe: msi MsiInstallMissingFile, which was a spec stub (Stained Glass OS wine-sg 8250;
 * test/msimissingfile-gate.sh): the file's component (File table of the cached package) is reinstalled.
 * The builder is test/fastmsiprov-probe.c's with a File table row on the registry-keyed component.
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#include <windows.h>
#include <msi.h>
#include <msiquery.h>
#include <stdio.h>
#include <string.h>

static int fails, checks;
#define CHECK(c, ...) do { checks++; if (!(c)) { fails++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static const char *tables[] = {
    "CREATE TABLE `Property` (`Property` CHAR(72) NOT NULL, `Value` CHAR(0) NOT NULL LOCALIZABLE PRIMARY KEY `Property`)",
    "CREATE TABLE `Directory` (`Directory` CHAR(72) NOT NULL, `Directory_Parent` CHAR(72), `DefaultDir` CHAR(255) NOT NULL LOCALIZABLE PRIMARY KEY `Directory`)",
    "CREATE TABLE `Component` (`Component` CHAR(72) NOT NULL, `ComponentId` CHAR(38), `Directory_` CHAR(72) NOT NULL, `Attributes` SHORT NOT NULL, `Condition` CHAR(255), `KeyPath` CHAR(72) PRIMARY KEY `Component`)",
    "CREATE TABLE `Feature` (`Feature` CHAR(38) NOT NULL, `Feature_Parent` CHAR(38), `Title` CHAR(64) LOCALIZABLE, `Description` CHAR(255) LOCALIZABLE, `Display` SHORT, `Level` SHORT NOT NULL, `Directory_` CHAR(72), `Attributes` SHORT NOT NULL PRIMARY KEY `Feature`)",
    "CREATE TABLE `FeatureComponents` (`Feature_` CHAR(38) NOT NULL, `Component_` CHAR(72) NOT NULL PRIMARY KEY `Feature_`, `Component_`)",
    "CREATE TABLE `InstallExecuteSequence` (`Action` CHAR(72) NOT NULL, `Condition` CHAR(255), `Sequence` SHORT PRIMARY KEY `Action`)",
    "CREATE TABLE `Registry` (`Registry` CHAR(72) NOT NULL, `Root` SHORT NOT NULL, `Key` CHAR(255) NOT NULL LOCALIZABLE, `Name` CHAR(255) LOCALIZABLE, `Value` CHAR(0) LOCALIZABLE, `Component_` CHAR(72) NOT NULL PRIMARY KEY `Registry`)",
    "CREATE TABLE `File` (`File` CHAR(72) NOT NULL, `Component_` CHAR(72) NOT NULL, `FileName` CHAR(255) NOT NULL LOCALIZABLE, `FileSize` LONG NOT NULL, `Version` CHAR(72), `Language` CHAR(20), `Attributes` SHORT, `Sequence` SHORT NOT NULL PRIMARY KEY `File`)",
    NULL
};


static const char *rows[] = {
    "INSERT INTO `Property` (`Property`, `Value`) VALUES ('ProductName', 'sgmissingfile')",
    "INSERT INTO `Property` (`Property`, `Value`) VALUES ('ProductCode', '{7E8A5F22-5555-4111-8111-111111111111}')",
    "INSERT INTO `Property` (`Property`, `Value`) VALUES ('ProductVersion', '1.0.0')",
    "INSERT INTO `Property` (`Property`, `Value`) VALUES ('ProductLanguage', '1033')",
    "INSERT INTO `Property` (`Property`, `Value`) VALUES ('Manufacturer', 'sg')",
    "INSERT INTO `Directory` (`Directory`, `Directory_Parent`, `DefaultDir`) VALUES ('TARGETDIR', '', 'SourceDir')",
    "INSERT INTO `Component` (`Component`, `ComponentId`, `Directory_`, `Attributes`, `Condition`, `KeyPath`) VALUES ('comp', '{7E8A5F22-5555-4333-8333-333333333333}', 'TARGETDIR', 4, '', 'r1')",
    "INSERT INTO `Feature` (`Feature`, `Feature_Parent`, `Title`, `Description`, `Display`, `Level`, `Directory_`, `Attributes`) VALUES ('feat', '', 'f', 'f', 1, 1, 'TARGETDIR', 0)",
    "INSERT INTO `FeatureComponents` (`Feature_`, `Component_`) VALUES ('feat', 'comp')",
    "INSERT INTO `Registry` (`Registry`, `Root`, `Key`, `Name`, `Value`, `Component_`) VALUES ('r1', 1, 'Software\\sgmissingfile', 'v', 'x', 'comp')",
    "INSERT INTO `InstallExecuteSequence` (`Action`, `Condition`, `Sequence`) VALUES ('CostInitialize', '', 800)",
    "INSERT INTO `InstallExecuteSequence` (`Action`, `Condition`, `Sequence`) VALUES ('FileCost', '', 900)",
    "INSERT INTO `InstallExecuteSequence` (`Action`, `Condition`, `Sequence`) VALUES ('CostFinalize', '', 1000)",
    "INSERT INTO `InstallExecuteSequence` (`Action`, `Condition`, `Sequence`) VALUES ('InstallValidate', '', 1400)",
    "INSERT INTO `InstallExecuteSequence` (`Action`, `Condition`, `Sequence`) VALUES ('InstallInitialize', '', 1500)",
    "INSERT INTO `InstallExecuteSequence` (`Action`, `Condition`, `Sequence`) VALUES ('ProcessComponents', '', 1600)",
    "INSERT INTO `InstallExecuteSequence` (`Action`, `Condition`, `Sequence`) VALUES ('RemoveRegistryValues', '', 2600)",
    "INSERT INTO `InstallExecuteSequence` (`Action`, `Condition`, `Sequence`) VALUES ('WriteRegistryValues', '', 5000)",
    "INSERT INTO `InstallExecuteSequence` (`Action`, `Condition`, `Sequence`) VALUES ('RegisterProduct', '', 6100)",
    "INSERT INTO `InstallExecuteSequence` (`Action`, `Condition`, `Sequence`) VALUES ('PublishFeatures', '', 6300)",
    "INSERT INTO `InstallExecuteSequence` (`Action`, `Condition`, `Sequence`) VALUES ('PublishProduct', '', 6400)",
    "INSERT INTO `InstallExecuteSequence` (`Action`, `Condition`, `Sequence`) VALUES ('InstallFinalize', '', 6600)",
    "INSERT INTO `File` (`File`, `Component_`, `FileName`, `FileSize`, `Version`, `Language`, `Attributes`, `Sequence`) VALUES ('f1', 'comp', 'SGAPP~1.DLL|sg app helper.dll', 10, '', '', 0, 1)",
    NULL
};
static UINT sql(MSIHANDLE db, const char *q)
{
    MSIHANDLE view;
    UINT r = MsiDatabaseOpenViewA(db, q, &view);
    if (r) return r;
    r = MsiViewExecute(view, 0);
    MsiViewClose(view);
    MsiCloseHandle(view);
    return r;
}

static int build_msi(const char *path, const char *const *extra)
{
    MSIHANDLE db, si;
    UINT r;
    int i;

    DeleteFileA(path);
    r = MsiOpenDatabaseA(path, MSIDBOPEN_CREATEDIRECT, &db);
    if (r) { printf("open db %u\n", r); return 0; }
    for (i = 0; tables[i]; i++) { r = sql(db, tables[i]); if (r) printf("table %d: %u\n", i, r); }
    for (i = 0; rows[i]; i++) { r = sql(db, rows[i]); if (r) printf("row %d: %u\n", i, r); }
    for (i = 0; extra && extra[i]; i++) { r = sql(db, extra[i]); if (r) printf("extra %d: %u\n", i, r); }
    MsiDatabaseCommit(db);
    r = MsiGetSummaryInformationA(db, NULL, 20, &si);
    if (!r)
    {
        MsiSummaryInfoSetPropertyA(si, 2, VT_LPSTR, 0, NULL, "Installation Database");
        MsiSummaryInfoSetPropertyA(si, 9, VT_LPSTR, 0, NULL, "{7E8A5F22-5555-4444-8444-444444444444}");
        MsiSummaryInfoSetPropertyA(si, 7, VT_LPSTR, 0, NULL, ";1033");
        MsiSummaryInfoSetPropertyA(si, 14, VT_I4, 200, NULL, NULL);
        MsiSummaryInfoSetPropertyA(si, 15, VT_I4, 0, NULL, NULL);
        MsiSummaryInfoPersist(si);
        MsiCloseHandle(si);
    }
    MsiCloseHandle(db);
    return 1;
}


#define PROD "{7E8A5F22-5555-4111-8111-111111111111}"
#define KEY "Software\\sgmissingfile"

int main(void)
{
    char dir[MAX_PATH], msi[MAX_PATH];
    HKEY key;
    UINT r;

    GetTempPathA(sizeof(dir), dir);
    strcat(dir, "sgmissingfile");
    CreateDirectoryA(dir, NULL);
    sprintf(msi, "%s\\t.msi", dir);
    RegDeleteKeyA(HKEY_CURRENT_USER, KEY);
    CHECK(build_msi(msi, NULL), "build msi");
    MsiSetInternalUI(INSTALLUILEVEL_NONE, NULL);

    CHECK(MsiInstallMissingFileA(PROD, "sg app helper.dll") == ERROR_UNKNOWN_PRODUCT, "unknown product before install");
    r = MsiInstallProductA(msi, "REBOOT=ReallySuppress");
    CHECK(r == ERROR_SUCCESS, "install: %u", r);

    CHECK(MsiInstallMissingFileA(NULL, "x.dll") == ERROR_INVALID_PARAMETER, "null product");
    CHECK(MsiInstallMissingFileA(PROD, NULL) == ERROR_INVALID_PARAMETER, "null file");
    r = MsiInstallMissingFileA(PROD, "not-in-package.dll");
    CHECK(r != ERROR_SUCCESS && r != ERROR_CALL_NOT_IMPLEMENTED, "a file the package does not have: %u", r);

    RegDeleteKeyA(HKEY_CURRENT_USER, KEY);
    r = MsiInstallMissingFileA(PROD, "sg app helper.dll");
    CHECK(r == ERROR_SUCCESS, "missing file by long name: %u", r);
    CHECK(RegOpenKeyA(HKEY_CURRENT_USER, KEY, &key) == 0, "its component reinstalled (key back)");
    RegCloseKey(key);

    RegDeleteKeyA(HKEY_CURRENT_USER, KEY);
    r = MsiInstallMissingFileA(PROD, "C:\\Program Files\\x\\SGAPP~1.DLL");
    CHECK(r == ERROR_SUCCESS, "by short name in a full path: %u", r);
    CHECK(RegOpenKeyA(HKEY_CURRENT_USER, KEY, &key) == 0, "reinstalled again");
    RegCloseKey(key);

    r = MsiConfigureProductA(PROD, INSTALLLEVEL_DEFAULT, INSTALLSTATE_ABSENT);
    printf("uninstall: %u\n", r);
    RegDeleteKeyA(HKEY_CURRENT_USER, KEY);
    DeleteFileA(msi);
    RemoveDirectoryA(dir);
    printf("checks %d fails %d\n", checks, fails);
    printf("RESULT: %s\n", fails ? "FAIL" : "PASS");
    return fails != 0;
}
