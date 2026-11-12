/* Probe for patches/sg/7738: an installed feature whose level is 0 at uninstall time is removed with the rest.
 * Anki's MSI: DesktopShortcutFeature is installed, then its Condition row gives it level 0 once the product is
 * Installed; REMOVE=ALL left it installed, and the product stayed registered (its Uninstall entry and
 * InstallProperties rewritten), though its files were gone. Here feature "desk" is level 0, and 1 when
 * DESKTOP="1", as it is when installed and not when removed. */
#include <windows.h>
#include <msi.h>
#include <msiquery.h>
#include <stdio.h>

#define PRODUCT "{6B1D4E2A-7C3F-4A58-9E21-3D5C7A9B1F40}"
static int fails;
static void check(const char *name, int ok) { printf("      %s  %s\n", ok ? "PASS" : "FAIL", name); if (!ok) fails++; }

static UINT query(MSIHANDLE db, const char *q)
{
    MSIHANDLE view;
    UINT r = MsiDatabaseOpenViewA(db, q, &view);
    if (r) { printf("      query %u: %s\n", r, q); return r; }
    r = MsiViewExecute(view, 0);
    MsiViewClose(view);
    MsiCloseHandle(view);
    return r;
}

static const char *const sql[] = {
    "CREATE TABLE `Property` (`Property` CHAR(72) NOT NULL, `Value` CHAR(0) PRIMARY KEY `Property`)",
    "INSERT INTO `Property` (`Property`, `Value`) VALUES ('ProductCode', '" PRODUCT "')",
    "INSERT INTO `Property` (`Property`, `Value`) VALUES ('ProductName', 'SG Level Zero')",
    "INSERT INTO `Property` (`Property`, `Value`) VALUES ('ProductVersion', '1.0.0')",
    "INSERT INTO `Property` (`Property`, `Value`) VALUES ('Manufacturer', 'Stained Glass gate')",
    "INSERT INTO `Property` (`Property`, `Value`) VALUES ('ProductLanguage', '1033')",
    "INSERT INTO `Property` (`Property`, `Value`) VALUES ('UpgradeCode', '{0C4E8A71-2B9D-4F36-8A15-6E7D9C2B3A58}')",
    "CREATE TABLE `Directory` (`Directory` CHAR(255) NOT NULL, `Directory_Parent` CHAR(255), `DefaultDir` CHAR(255) NOT NULL PRIMARY KEY `Directory`)",
    "INSERT INTO `Directory` (`Directory`, `Directory_Parent`, `DefaultDir`) VALUES ('TARGETDIR', '', 'SourceDir')",
    "INSERT INTO `Directory` (`Directory`, `Directory_Parent`, `DefaultDir`) VALUES ('ProgramFilesFolder', 'TARGETDIR', '.')",
    "INSERT INTO `Directory` (`Directory`, `Directory_Parent`, `DefaultDir`) VALUES ('APPDIR', 'ProgramFilesFolder', 'SGLevelZero')",
    "INSERT INTO `Directory` (`Directory`, `Directory_Parent`, `DefaultDir`) VALUES ('DESKDIR', 'APPDIR', 'desk')",
    "CREATE TABLE `Component` (`Component` CHAR(72) NOT NULL, `ComponentId` CHAR(38), `Directory_` CHAR(72) NOT NULL, `Attributes` SHORT NOT NULL, `Condition` CHAR(255), `KeyPath` CHAR(72) PRIMARY KEY `Component`)",
    "INSERT INTO `Component` (`Component`, `ComponentId`, `Directory_`, `Attributes`, `Condition`, `KeyPath`) VALUES ('main', '{A3F1C7B2-5D84-4E9A-B6C0-2F8E1D7A4B39}', 'APPDIR', 0, '', '')",
    "INSERT INTO `Component` (`Component`, `ComponentId`, `Directory_`, `Attributes`, `Condition`, `KeyPath`) VALUES ('desk', '{C7E2A9D4-1B63-4F8E-9A05-7D3B6C1E8F42}', 'DESKDIR', 0, '', '')",
    "CREATE TABLE `CreateFolder` (`Directory_` CHAR(72) NOT NULL, `Component_` CHAR(72) NOT NULL PRIMARY KEY `Directory_`, `Component_`)",
    "INSERT INTO `CreateFolder` (`Directory_`, `Component_`) VALUES ('APPDIR', 'main')",
    "INSERT INTO `CreateFolder` (`Directory_`, `Component_`) VALUES ('DESKDIR', 'desk')",
    "CREATE TABLE `Feature` (`Feature` CHAR(38) NOT NULL, `Feature_Parent` CHAR(38), `Title` CHAR(64), `Description` CHAR(255), `Display` SHORT NOT NULL, `Level` SHORT NOT NULL, `Directory_` CHAR(72), `Attributes` SHORT NOT NULL PRIMARY KEY `Feature`)",
    "INSERT INTO `Feature` (`Feature`, `Feature_Parent`, `Title`, `Description`, `Display`, `Level`, `Directory_`, `Attributes`) VALUES ('main', '', '', '', 0, 1, '', 0)",
    "INSERT INTO `Feature` (`Feature`, `Feature_Parent`, `Title`, `Description`, `Display`, `Level`, `Directory_`, `Attributes`) VALUES ('desk', '', '', '', 0, 0, '', 0)",
    "CREATE TABLE `Condition` (`Feature_` CHAR(38) NOT NULL, `Level` SHORT NOT NULL, `Condition` CHAR(255) PRIMARY KEY `Feature_`, `Level`)",
    "INSERT INTO `Condition` (`Feature_`, `Level`, `Condition`) VALUES ('desk', 1, 'DESKTOP=""1""')",
    "CREATE TABLE `FeatureComponents` (`Feature_` CHAR(38) NOT NULL, `Component_` CHAR(72) NOT NULL PRIMARY KEY `Feature_`, `Component_`)",
    "INSERT INTO `FeatureComponents` (`Feature_`, `Component_`) VALUES ('main', 'main')",
    "INSERT INTO `FeatureComponents` (`Feature_`, `Component_`) VALUES ('desk', 'desk')",
    "CREATE TABLE `InstallExecuteSequence` (`Action` CHAR(72) NOT NULL, `Condition` CHAR(255), `Sequence` SHORT PRIMARY KEY `Action`)",
    "INSERT INTO `InstallExecuteSequence` (`Action`, `Condition`, `Sequence`) VALUES ('CostInitialize', '', 800)",
    "INSERT INTO `InstallExecuteSequence` (`Action`, `Condition`, `Sequence`) VALUES ('FileCost', '', 900)",
    "INSERT INTO `InstallExecuteSequence` (`Action`, `Condition`, `Sequence`) VALUES ('CostFinalize', '', 1000)",
    "INSERT INTO `InstallExecuteSequence` (`Action`, `Condition`, `Sequence`) VALUES ('InstallValidate', '', 1400)",
    "INSERT INTO `InstallExecuteSequence` (`Action`, `Condition`, `Sequence`) VALUES ('InstallInitialize', '', 1500)",
    "INSERT INTO `InstallExecuteSequence` (`Action`, `Condition`, `Sequence`) VALUES ('ProcessComponents', '', 1600)",
    "INSERT INTO `InstallExecuteSequence` (`Action`, `Condition`, `Sequence`) VALUES ('UnpublishFeatures', '', 1800)",
    "INSERT INTO `InstallExecuteSequence` (`Action`, `Condition`, `Sequence`) VALUES ('RemoveFolders', '', 3600)",
    "INSERT INTO `InstallExecuteSequence` (`Action`, `Condition`, `Sequence`) VALUES ('CreateFolders', '', 3700)",
    "INSERT INTO `InstallExecuteSequence` (`Action`, `Condition`, `Sequence`) VALUES ('RegisterUser', '', 6000)",
    "INSERT INTO `InstallExecuteSequence` (`Action`, `Condition`, `Sequence`) VALUES ('RegisterProduct', '', 6100)",
    "INSERT INTO `InstallExecuteSequence` (`Action`, `Condition`, `Sequence`) VALUES ('PublishFeatures', '', 6300)",
    "INSERT INTO `InstallExecuteSequence` (`Action`, `Condition`, `Sequence`) VALUES ('PublishProduct', '', 6400)",
    "INSERT INTO `InstallExecuteSequence` (`Action`, `Condition`, `Sequence`) VALUES ('InstallFinalize', '', 6600)",
    NULL
};

static BOOL uninstall_entry(void)
{
    HKEY key;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\" PRODUCT,
                      0, KEY_READ | KEY_WOW64_32KEY, &key)) return FALSE;
    RegCloseKey(key);
    return TRUE;
}

int main(void)
{
    char path[MAX_PATH];
    MSIHANDLE db, si;
    UINT r;
    int i;

    GetTempPathA(MAX_PATH, path);
    strcat(path, "sglevel0.msi");
    DeleteFileA(path);
    if ((r = MsiOpenDatabaseA(path, (LPCSTR)MSIDBOPEN_CREATE, &db))) { printf("create %u\n", r); return 1; }
    MsiGetSummaryInformationA(db, NULL, 7, &si);
    MsiSummaryInfoSetPropertyA(si, 2, VT_LPSTR, 0, NULL, "Installation Database");
    MsiSummaryInfoSetPropertyA(si, 4, VT_LPSTR, 0, NULL, "Probe");
    MsiSummaryInfoSetPropertyA(si, 7, VT_LPSTR, 0, NULL, ";1033");
    MsiSummaryInfoSetPropertyA(si, 9, VT_LPSTR, 0, NULL, "{2E7B4C19-8A3D-4F65-B0C2-9D1E6A5F7B83}");
    MsiSummaryInfoSetPropertyA(si, 14, VT_I4, 200, NULL, NULL);
    MsiSummaryInfoSetPropertyA(si, 15, VT_I4, 2, NULL, NULL);
    MsiSummaryInfoPersist(si);
    MsiCloseHandle(si);
    for (i = 0; sql[i]; i++) if (query(db, sql[i])) fails++;
    MsiDatabaseCommit(db);
    MsiCloseHandle(db);
    MsiSetInternalUI(INSTALLUILEVEL_NONE, NULL);

    r = MsiInstallProductA(path, "ALLUSERS=1 DESKTOP=1");
    check("installs", r == 0);
    check("both features installed", MsiQueryFeatureStateA(PRODUCT, "main") == INSTALLSTATE_LOCAL &&
                                     MsiQueryFeatureStateA(PRODUCT, "desk") == INSTALLSTATE_LOCAL);
    check("registered (Uninstall entry)", uninstall_entry());
    r = MsiConfigureProductA(PRODUCT, INSTALLLEVEL_DEFAULT, INSTALLSTATE_ABSENT);
    printf("      uninstall %u, product state %d, desk %d\n", r, MsiQueryProductStateA(PRODUCT), MsiQueryFeatureStateA(PRODUCT, "desk"));
    check("uninstall succeeds", r == 0);
    check("the product is gone, its level-0 feature with it", MsiQueryProductStateA(PRODUCT) == INSTALLSTATE_UNKNOWN);
    check("its Uninstall entry is gone", !uninstall_entry());
    if (MsiQueryProductStateA(PRODUCT) != INSTALLSTATE_UNKNOWN) MsiInstallProductA(path, "REMOVE=ALL");
    DeleteFileA(path);
    puts(fails ? "RESULT: FAIL" : "RESULT: PASS");
    return fails != 0;
}
