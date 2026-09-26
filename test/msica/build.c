/* Build msica-gate.sh's package: build.exe OUT.msi CA.dll */
#include <windows.h>
#include <msi.h>
#include <msiquery.h>
#include <stdio.h>

static UINT run(MSIHANDLE db, const char *sql)
{
    MSIHANDLE view;
    UINT r = MsiDatabaseOpenViewA(db, sql, &view);
    if (!r) { r = MsiViewExecute(view, 0); MsiCloseHandle(view); }
    if (r) printf("sql failed %u: %s\n", r, sql);
    return r;
}

int main(int argc, char **argv)
{
    static const char *sql[] = {
        "CREATE TABLE `Property` (`Property` CHAR(72) NOT NULL, `Value` CHAR(0) NOT NULL LOCALIZABLE PRIMARY KEY `Property`)",
        "INSERT INTO `Property` (`Property`, `Value`) VALUES ('ProductCode', '{3B8E0B7A-3F3A-4C8B-9E36-5A1F0C2D7E41}')",
        "INSERT INTO `Property` (`Property`, `Value`) VALUES ('ProductName', 'sg msica gate')",
        "INSERT INTO `Property` (`Property`, `Value`) VALUES ('ProductVersion', '1.0.0')",
        "INSERT INTO `Property` (`Property`, `Value`) VALUES ('ProductLanguage', '1033')",
        "INSERT INTO `Property` (`Property`, `Value`) VALUES ('Manufacturer', 'Stained Glass OS')",
        "CREATE TABLE `Directory` (`Directory` CHAR(72) NOT NULL, `Directory_Parent` CHAR(72), `DefaultDir` CHAR(255) NOT NULL LOCALIZABLE PRIMARY KEY `Directory`)",
        "INSERT INTO `Directory` (`Directory`, `Directory_Parent`, `DefaultDir`) VALUES ('TARGETDIR', '', 'SourceDir')",
        "CREATE TABLE `Feature` (`Feature` CHAR(38) NOT NULL, `Feature_Parent` CHAR(38), `Title` CHAR(64) LOCALIZABLE, `Description` CHAR(255) LOCALIZABLE, `Display` SHORT, `Level` SHORT NOT NULL, `Directory_` CHAR(72), `Attributes` SHORT NOT NULL PRIMARY KEY `Feature`)",
        "INSERT INTO `Feature` (`Feature`, `Feature_Parent`, `Title`, `Description`, `Display`, `Level`, `Directory_`, `Attributes`) VALUES ('Main', '', 'Main', '', 1, 1, 'TARGETDIR', 0)",
        "CREATE TABLE `Binary` (`Name` CHAR(72) NOT NULL, `Data` OBJECT NOT NULL PRIMARY KEY `Name`)",
        "CREATE TABLE `CustomAction` (`Action` CHAR(72) NOT NULL, `Type` SHORT NOT NULL, `Source` CHAR(72), `Target` CHAR(255) PRIMARY KEY `Action`)",
        "INSERT INTO `CustomAction` (`Action`, `Type`, `Source`, `Target`) VALUES ('ThrowCA', 1, 'cadll', 'Throw')",
        "INSERT INTO `CustomAction` (`Action`, `Type`, `Source`, `Target`) VALUES ('MarkCA', 1, 'cadll', 'Mark')",
        "CREATE TABLE `InstallExecuteSequence` (`Action` CHAR(72) NOT NULL, `Condition` CHAR(255), `Sequence` SHORT PRIMARY KEY `Action`)",
        "INSERT INTO `InstallExecuteSequence` (`Action`, `Condition`, `Sequence`) VALUES ('CostInitialize', '', 800)",
        "INSERT INTO `InstallExecuteSequence` (`Action`, `Condition`, `Sequence`) VALUES ('FileCost', '', 900)",
        "INSERT INTO `InstallExecuteSequence` (`Action`, `Condition`, `Sequence`) VALUES ('CostFinalize', '', 1000)",
        "INSERT INTO `InstallExecuteSequence` (`Action`, `Condition`, `Sequence`) VALUES ('ThrowCA', '', 1100)",
        "INSERT INTO `InstallExecuteSequence` (`Action`, `Condition`, `Sequence`) VALUES ('MarkCA', '', 1200)",
        "INSERT INTO `InstallExecuteSequence` (`Action`, `Condition`, `Sequence`) VALUES ('InstallValidate', '', 1400)",
        "INSERT INTO `InstallExecuteSequence` (`Action`, `Condition`, `Sequence`) VALUES ('InstallInitialize', '', 1500)",
        "INSERT INTO `InstallExecuteSequence` (`Action`, `Condition`, `Sequence`) VALUES ('InstallFinalize', '', 6600)",
    };
    MSIHANDLE db, rec, view, si;
    unsigned int i;

    if (argc < 3) return 2;
    DeleteFileA(argv[1]);
    if (MsiOpenDatabaseA(argv[1], MSIDBOPEN_CREATE, &db)) { printf("create failed\n"); return 1; }
    for (i = 0; i < ARRAYSIZE(sql); i++) if (run(db, sql[i])) return 1;

    rec = MsiCreateRecord(2);
    MsiRecordSetStringA(rec, 1, "cadll");
    if (MsiRecordSetStreamA(rec, 2, argv[2])) { printf("stream failed\n"); return 1; }
    MsiDatabaseOpenViewA(db, "INSERT INTO `Binary` (`Name`, `Data`) VALUES (?, ?)", &view);
    if (MsiViewExecute(view, rec)) { printf("binary failed\n"); return 1; }
    MsiCloseHandle(view); MsiCloseHandle(rec);

    MsiGetSummaryInformationA(db, NULL, 10, &si);
    MsiSummaryInfoSetPropertyA(si, 7, VT_LPSTR, 0, NULL, sizeof(void *) == 8 ? "x64;1033" : "Intel;1033");
    MsiSummaryInfoSetPropertyA(si, 9, VT_LPSTR, 0, NULL, "{7C1D6E2B-9A40-4F1E-8C3D-2B5E6F7A8B90}");
    MsiSummaryInfoSetPropertyA(si, 14, VT_I4, 200, NULL, NULL);
    MsiSummaryInfoSetPropertyA(si, 15, VT_I4, 2, NULL, NULL);
    MsiSummaryInfoPersist(si); MsiCloseHandle(si);
    if (MsiDatabaseCommit(db)) { printf("commit failed\n"); return 1; }
    MsiCloseHandle(db);
    printf("built\n");
    return 0;
}
