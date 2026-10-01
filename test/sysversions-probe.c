/* The versions installers look for in the system's files (patches/sg/0636),
 * the way TortoiseGit's MSI looks: a package made here with its AppSearch,
 * Signature and DrLocator rows -- shell32.dll in [SystemFolder] at least
 * 6.3.14392.0 ("Windows 10 1607 or later") and vcruntime140.dll in
 * [System64Folder] at least 14.42 ("the latest Visual C++ runtime") -- then
 * its AppSearch action. Prints:
 *   win10=<the property WIN10_1607_FOUND, or ->  vcredist=<VC_REDIST_INSTALLED, or ->
 *   shell32=<file version>  vcruntime140=<file version>  vcruntime140_1=<file version>
 */
#include <windows.h>
#include <msi.h>
#include <msiquery.h>
#include <stdio.h>
#include <propidl.h>

static void run(MSIHANDLE db, const char *sql)
{
    MSIHANDLE view;
    if (MsiDatabaseOpenViewA(db, sql, &view) == ERROR_SUCCESS) {
        MsiViewExecute(view, 0);
        MsiViewClose(view);
        MsiCloseHandle(view);
    } else printf("sql failed: %s\n", sql);
}

static void version(const char *path, const char *name)
{
    DWORD h, n = GetFileVersionInfoSizeA(path, &h);
    VS_FIXEDFILEINFO *fi;
    UINT len;
    char *buf = n ? malloc(n) : NULL;
    if (buf && GetFileVersionInfoA(path, 0, n, buf) && VerQueryValueA(buf, "\\", (void **)&fi, &len))
        printf("%s=%lu.%lu.%lu.%lu\n", name, HIWORD(fi->dwFileVersionMS), LOWORD(fi->dwFileVersionMS),
               HIWORD(fi->dwFileVersionLS), LOWORD(fi->dwFileVersionLS));
    else printf("%s=none\n", name);
    free(buf);
}

int main(int argc, char **argv)
{
    char path[MAX_PATH], val[256], sys[MAX_PATH], wow[MAX_PATH];
    MSIHANDLE db, si, pkg;
    DWORD n;
    UINT r;

    GetTempPathA(MAX_PATH, path);
    strcat(path, "sysversions.msi");
    DeleteFileA(path);
    if (MsiOpenDatabaseA(path, MSIDBOPEN_CREATE, &db)) { printf("nodb\n"); return 1; }
    run(db, "CREATE TABLE `Property` (`Property` CHAR(72) NOT NULL, `Value` CHAR(0) NOT NULL PRIMARY KEY `Property`)");
    run(db, "CREATE TABLE `Signature` (`Signature` CHAR(72) NOT NULL, `FileName` CHAR(255) NOT NULL, `MinVersion` CHAR(20), "
            "`MaxVersion` CHAR(20), `MinSize` LONG, `MaxSize` LONG, `MinDate` LONG, `MaxDate` LONG, `Languages` CHAR(255) "
            "PRIMARY KEY `Signature`)");
    run(db, "CREATE TABLE `DrLocator` (`Signature_` CHAR(72) NOT NULL, `Parent` CHAR(72), `Path` CHAR(255), `Depth` SHORT "
            "PRIMARY KEY `Signature_`, `Parent`, `Path`)");
    run(db, "CREATE TABLE `AppSearch` (`Property` CHAR(72) NOT NULL, `Signature_` CHAR(72) NOT NULL "
            "PRIMARY KEY `Property`, `Signature_`)");
    run(db, "INSERT INTO `Property` (`Property`, `Value`) VALUES ('ProductCode', '{6E2D73C1-6F1B-4E4B-9D2A-3C1B7A2E4F01}')");
    run(db, "INSERT INTO `Property` (`Property`, `Value`) VALUES ('ProductLanguage', '1033')");
    run(db, "INSERT INTO `Property` (`Property`, `Value`) VALUES ('ProductName', 'sysversions')");
    run(db, "INSERT INTO `Property` (`Property`, `Value`) VALUES ('ProductVersion', '1.0.0')");
    run(db, "INSERT INTO `Property` (`Property`, `Value`) VALUES ('Manufacturer', 'Stained Glass gate')");
    run(db, "INSERT INTO `Signature` (`Signature`, `FileName`, `MinVersion`) VALUES ('searchFile10', 'shell32.dll', '6.3.14392.0')");
    run(db, "INSERT INTO `Signature` (`Signature`, `FileName`, `MinVersion`) VALUES ('searchFileVCRedist', 'vcruntime140.dll', '14.42')");
    run(db, "INSERT INTO `DrLocator` (`Signature_`, `Parent`, `Path`, `Depth`) VALUES ('searchFile10', '', '[SystemFolder]', 0)");
    run(db, "INSERT INTO `DrLocator` (`Signature_`, `Parent`, `Path`, `Depth`) VALUES ('searchSystem10', 'searchFile10', '', 0)");
    run(db, "INSERT INTO `DrLocator` (`Signature_`, `Parent`, `Path`, `Depth`) VALUES ('searchSystemVCRedist', '', '[System64Folder]', 0)");
    run(db, "INSERT INTO `DrLocator` (`Signature_`, `Parent`, `Path`, `Depth`) VALUES ('searchFileVCRedist', 'searchSystemVCRedist', '', 0)");
    run(db, "INSERT INTO `AppSearch` (`Property`, `Signature_`) VALUES ('WIN10_1607_FOUND', 'searchSystem10')");
    run(db, "INSERT INTO `AppSearch` (`Property`, `Signature_`) VALUES ('VC_REDIST_INSTALLED', 'searchFileVCRedist')");
    MsiGetSummaryInformationA(db, NULL, 10, &si);
    MsiSummaryInfoSetPropertyA(si, PIDSI_TEMPLATE, VT_LPSTR, 0, NULL, "x64;1033");
    MsiSummaryInfoSetPropertyA(si, PIDSI_REVNUMBER, VT_LPSTR, 0, NULL, "{2B7C0E4A-91D3-4F6A-8E25-6C3D1F0A9B11}");
    MsiSummaryInfoSetPropertyA(si, PIDSI_PAGECOUNT, VT_I4, 200, NULL, NULL);
    MsiSummaryInfoSetPropertyA(si, PIDSI_WORDCOUNT, VT_I4, 0, NULL, NULL);
    MsiSummaryInfoPersist(si);
    MsiCloseHandle(si);
    MsiDatabaseCommit(db);
    MsiCloseHandle(db);

    MsiSetInternalUI(INSTALLUILEVEL_NONE, NULL);
    if ((r = MsiOpenPackageA(path, &pkg))) { printf("nopackage=%u\n", r); return 1; }
    r = MsiDoActionA(pkg, "AppSearch");
    n = sizeof(val); val[0] = 0; MsiGetPropertyA(pkg, "WIN10_1607_FOUND", val, &n);
    printf("appsearch=%u\nwin10=%s\n", r, val[0] ? val : "-");
    n = sizeof(val); val[0] = 0; MsiGetPropertyA(pkg, "VC_REDIST_INSTALLED", val, &n);
    printf("vcredist=%s\n", val[0] ? val : "-");
    MsiCloseHandle(pkg);
    DeleteFileA(path);

    GetSystemDirectoryA(sys, MAX_PATH);
    GetSystemWow64DirectoryA(wow, MAX_PATH);
    snprintf(path, MAX_PATH, "%s\\shell32.dll", wow); version(path, "shell32");
    snprintf(path, MAX_PATH, "%s\\vcruntime140.dll", sys); version(path, "vcruntime140");
    snprintf(path, MAX_PATH, "%s\\vcruntime140_1.dll", sys); version(path, "vcruntime140_1");
    return 0;
}
