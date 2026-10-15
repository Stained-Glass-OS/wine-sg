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
    "CREATE TABLE `IniFile` (`IniFile` CHAR(72) NOT NULL, `FileName` CHAR(255) NOT NULL LOCALIZABLE, `DirProperty` CHAR(72), `Section` CHAR(96) NOT NULL LOCALIZABLE, `Key` CHAR(128) NOT NULL LOCALIZABLE, `Value` CHAR(255) NOT NULL LOCALIZABLE, `Action` SHORT NOT NULL, `Component_` CHAR(72) NOT NULL PRIMARY KEY `IniFile`)",
    "CREATE TABLE `RemoveIniFile` (`RemoveIniFile` CHAR(72) NOT NULL, `FileName` CHAR(255) NOT NULL LOCALIZABLE, `DirProperty` CHAR(72), `Section` CHAR(96) NOT NULL LOCALIZABLE, `Key` CHAR(128) NOT NULL LOCALIZABLE, `Value` CHAR(255) LOCALIZABLE, `Action` SHORT NOT NULL, `Component_` CHAR(72) NOT NULL PRIMARY KEY `RemoveIniFile`)",
    "CREATE TABLE `InstallExecuteSequence` (`Action` CHAR(72) NOT NULL, `Condition` CHAR(255), `Sequence` SHORT PRIMARY KEY `Action`)",
    NULL
};

static const char *rows[] = {
    "INSERT INTO `Property` (`Property`, `Value`) VALUES ('ProductName', 'fastini')",
    "INSERT INTO `Property` (`Property`, `Value`) VALUES ('ProductCode', '{7E8A5F11-1111-4111-8111-111111111111}')",
    "INSERT INTO `Property` (`Property`, `Value`) VALUES ('ProductVersion', '1.0.0')",
    "INSERT INTO `Property` (`Property`, `Value`) VALUES ('ProductLanguage', '1033')",
    "INSERT INTO `Property` (`Property`, `Value`) VALUES ('Manufacturer', 'sg')",
    "INSERT INTO `Property` (`Property`, `Value`) VALUES ('UpgradeCode', '{7E8A5F11-2222-4222-8222-222222222222}')",
    "INSERT INTO `Directory` (`Directory`, `Directory_Parent`, `DefaultDir`) VALUES ('TARGETDIR', '', 'SourceDir')",
    "INSERT INTO `Component` (`Component`, `ComponentId`, `Directory_`, `Attributes`, `Condition`, `KeyPath`) VALUES ('comp', '{7E8A5F11-3333-4333-8333-333333333333}', 'TARGETDIR', 0, '', '')",
    "INSERT INTO `Feature` (`Feature`, `Feature_Parent`, `Title`, `Description`, `Display`, `Level`, `Directory_`, `Attributes`) VALUES ('feat', '', 'f', 'f', 1, 1, 'TARGETDIR', 0)",
    "INSERT INTO `FeatureComponents` (`Feature_`, `Component_`) VALUES ('feat', 'comp')",
    "INSERT INTO `IniFile` (`IniFile`, `FileName`, `DirProperty`, `Section`, `Key`, `Value`, `Action`, `Component_`) VALUES ('i1', 'fast.ini', 'TARGETDIR', 'sec', 'list', 'alpha', 3, 'comp')",
    "INSERT INTO `IniFile` (`IniFile`, `FileName`, `DirProperty`, `Section`, `Key`, `Value`, `Action`, `Component_`) VALUES ('i2', 'fast.ini', 'TARGETDIR', 'sec', 'list', 'beta', 3, 'comp')",
    "INSERT INTO `IniFile` (`IniFile`, `FileName`, `DirProperty`, `Section`, `Key`, `Value`, `Action`, `Component_`) VALUES ('i3', 'fast.ini', 'TARGETDIR', 'sec', 'list', 'ALPHA', 3, 'comp')",
    "INSERT INTO `IniFile` (`IniFile`, `FileName`, `DirProperty`, `Section`, `Key`, `Value`, `Action`, `Component_`) VALUES ('i4', 'fast.ini', 'TARGETDIR', 'sec', 'plain', 'one', 0, 'comp')",
    "INSERT INTO `InstallExecuteSequence` (`Action`, `Condition`, `Sequence`) VALUES ('CostInitialize', '', 800)",
    "INSERT INTO `InstallExecuteSequence` (`Action`, `Condition`, `Sequence`) VALUES ('FileCost', '', 900)",
    "INSERT INTO `InstallExecuteSequence` (`Action`, `Condition`, `Sequence`) VALUES ('CostFinalize', '', 1000)",
    "INSERT INTO `InstallExecuteSequence` (`Action`, `Condition`, `Sequence`) VALUES ('InstallValidate', '', 1400)",
    "INSERT INTO `InstallExecuteSequence` (`Action`, `Condition`, `Sequence`) VALUES ('InstallInitialize', '', 1500)",
    "INSERT INTO `InstallExecuteSequence` (`Action`, `Condition`, `Sequence`) VALUES ('ProcessComponents', '', 1600)",
    "INSERT INTO `InstallExecuteSequence` (`Action`, `Condition`, `Sequence`) VALUES ('RemoveIniValues', '', 3000)",
    "INSERT INTO `InstallExecuteSequence` (`Action`, `Condition`, `Sequence`) VALUES ('WriteIniValues', '', 3100)",
    "INSERT INTO `InstallExecuteSequence` (`Action`, `Condition`, `Sequence`) VALUES ('RegisterProduct', '', 6100)",
    "INSERT INTO `InstallExecuteSequence` (`Action`, `Condition`, `Sequence`) VALUES ('InstallFinalize', '', 6600)",
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
        MsiSummaryInfoSetPropertyA(si, 9, VT_LPSTR, 0, NULL, "{7E8A5F11-4444-4444-8444-444444444444}");
        MsiSummaryInfoSetPropertyA(si, 7, VT_LPSTR, 0, NULL, ";1033");
        MsiSummaryInfoSetPropertyA(si, 14, VT_I4, 200, NULL, NULL);
        MsiSummaryInfoSetPropertyA(si, 15, VT_I4, 0, NULL, NULL);
        MsiSummaryInfoPersist(si);
        MsiCloseHandle(si);
    }
    MsiCloseHandle(db);
    return 1;
}

static int read_ini(const char *key, char *buf, DWORD size, const char *path)
{
    buf[0] = 0;
    GetPrivateProfileStringA("sec", key, "<none>", buf, size, path);
    return 1;
}

int main(void)
{
    char dir[MAX_PATH], msi[MAX_PATH], ini[MAX_PATH], cmd[MAX_PATH + 64], buf[256];
    UINT r;

    GetTempPathA(sizeof(dir), dir);
    strcat(dir, "fastmsiini");
    CreateDirectoryA(dir, NULL);
    sprintf(msi, "%s\\t.msi", dir);
    sprintf(ini, "%s\\fast.ini", dir);
    DeleteFileA(ini);

    CHECK(build_msi(msi, NULL), "build msi");
    sprintf(cmd, "TARGETDIR=%s REBOOT=ReallySuppress", dir);
    MsiSetInternalUI(INSTALLUILEVEL_NONE, NULL);
    r = MsiInstallProductA(msi, cmd);
    CHECK(r == ERROR_SUCCESS, "install: %u", r);

    read_ini("plain", buf, sizeof(buf), ini);
    CHECK(!strcmp(buf, "one"), "plain value: '%s'", buf);
    read_ini("list", buf, sizeof(buf), ini);
    CHECK(!strcmp(buf, "alpha,beta"), "tags appended once, in order: '%s'", buf);

    /* a second install keeps the list the same */
    r = MsiInstallProductA(msi, cmd);
    printf("reinstall: %u\n", r);
    read_ini("list", buf, sizeof(buf), ini);
    CHECK(!strcmp(buf, "alpha,beta"), "list after reinstall: '%s'", buf);

    /* RemoveIniFile: remove one tag of an existing list */
    {
        const char *extra[3];
        WritePrivateProfileStringA("sec", "list", "x,y,z", ini);
        extra[0] = "INSERT INTO `RemoveIniFile` (`RemoveIniFile`, `FileName`, `DirProperty`, `Section`, `Key`, `Value`, `Action`, `Component_`) VALUES ('r1', 'fast.ini', 'TARGETDIR', 'sec', 'list', 'y', 4, 'comp')";
        extra[1] = "UPDATE `Property` SET `Value` = '{7E8A5F11-5555-4555-8555-555555555555}' WHERE `Property` = 'ProductCode'";
        extra[2] = NULL;
        CHECK(build_msi(msi, extra), "build msi 2");
        sprintf(cmd, "TARGETDIR=%s REBOOT=ReallySuppress", dir);
        r = MsiInstallProductA(msi, cmd);
        CHECK(r == ERROR_SUCCESS, "install 2: %u", r);
        read_ini("list", buf, sizeof(buf), ini);
        CHECK(!strcmp(buf, "x,z,alpha,beta"), "RemoveTag then AddTag: '%s'", buf);
        printf("list2=%s\n", buf);
    }

    printf("checks %d fails %d\n", checks, fails);
    printf(fails ? "RESULT: FAIL\n" : "RESULT: PASS\n");
    return fails != 0;
}
