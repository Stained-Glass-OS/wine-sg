/* Probe for patches/sg/2445: msi behaviours recorded from Windows: hyphen names, empty quoted names, MERGE against an existing row,
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

static void check_merge(MSIHANDLE view, int a, int b, UINT want, const char *name)
{
    MSIHANDLE rec = MsiCreateRecord(2);
    UINT r;
    MsiRecordSetInteger(rec, 1, a);
    MsiRecordSetInteger(rec, 2, b);
    r = MsiViewModify(view, MSIMODIFY_MERGE, rec);
    checku(name, r, want);
    MsiCloseHandle(rec);
}

int main(void)
{
    WCHAR path[MAX_PATH];
    MSIHANDLE db = 0, view = 0, pkg = 0;
    char buf[64], pkgname[32];
    DWORD size;
    UINT r;

    GetTempPathW(MAX_PATH, path);
    wcscat(path, L"sgmsiground.msi");
    DeleteFileW(path);
    r = MsiOpenDatabaseW(path, (LPCWSTR)(ULONG_PTR)3, &db);
    checku("create database", r, 0);

    /* names */
    checku("create table `-a`", query(db, "CREATE TABLE `-a` (`b` CHAR NOT NULL PRIMARY KEY `b`)"), 0);
    checku("SELECT * FROM -a (an unquoted name may start with a hyphen)", query(db, "SELECT * FROM -a"), 0);
    checku("create table `t`", query(db, "CREATE TABLE `t` (`a` CHAR NOT NULL, `b` CHAR PRIMARY KEY `a`)"), 0);
    checku("SELECT `` FROM `t`: a quoted name with nothing in it", query(db, "SELECT `` FROM `t` WHERE `t`.`b` = 'x'"), ERROR_BAD_QUERY_SYNTAX);
    checku("SELECT `t`.`b`, `` FROM `t`", query(db, "SELECT `t`.`b`, `` FROM `t` WHERE `t`.`b` = 'x'"), ERROR_BAD_QUERY_SYNTAX);
    checku("SELECT '' FROM `t`: the empty string is fine", query(db, "SELECT '' FROM `t` WHERE `t`.`b` = 'x'"), 0);
    checku("create table `n`", query(db, "CREATE TABLE `n` (`a` INT NOT NULL, `b` INT PRIMARY KEY `a`)"), 0);
    checku("insert a negative number", query(db, "INSERT INTO `n` (`a`, `b`) VALUES (1, -5)"), 0);
    checku("compare with a negative number still parses", query(db, "SELECT * FROM `n` WHERE `b` = -5"), 0);

    /* merge */
    checku("create table T", query(db, "CREATE TABLE `T` (`A` SHORT, `B` SHORT PRIMARY KEY `A`)"), 0);
    checku("insert (1,2)", query(db, "INSERT INTO `T` (`A`, `B`) VALUES (1, 2)"), 0);
    MsiDatabaseOpenViewA(db, "SELECT * FROM `T`", &view);
    MsiViewExecute(view, 0);
    check_merge(view, 1, 2, 0, "merge the very same row");
    check_merge(view, 1, 3, ERROR_FUNCTION_FAILED, "merge a row with the same key and other data");
    check_merge(view, 2, 3, 0, "merge a new row");
    MsiViewClose(view);
    MsiCloseHandle(view);

    /* a package from a database handle: its properties do not outlive it */
    set_summary(db);
    checku("create Directory table", query(db, "CREATE TABLE `Directory` (`Directory` CHAR(255) NOT NULL, `Directory_Parent` CHAR(255), `DefaultDir` CHAR(255) NOT NULL PRIMARY KEY `Directory`)"), 0);
    checku("create CustomAction table", query(db, "CREATE TABLE `CustomAction` (`Action` CHAR(72) NOT NULL, `Type` SHORT NOT NULL, `Source` CHAR(72), `Target` CHAR(255) PRIMARY KEY `Action`)"), 0);
    checku("add the SetProp action", query(db, "INSERT INTO `CustomAction` (`Action`, `Type`, `Source`, `Target`) VALUES ('SetProp', 51, 'MYPROP', 'grape')"), 0);
    snprintf(pkgname, sizeof(pkgname), "#%lu", (unsigned long)db);
    r = MsiOpenPackageA(pkgname, &pkg);
    if (r == ERROR_INSTALL_PACKAGE_REJECTED) { printf("      (no rights: package checks skipped)\n"); goto done; }
    checku("open the package", r, 0);
    if (!r)
    {
        checku("run the custom action", MsiDoActionA(pkg, "SetProp"), 0);
        size = sizeof(buf); strcpy(buf, "kiwi");
        MsiGetPropertyA(pkg, "MYPROP", buf, &size);
        check("the property is set", !strcmp(buf, "grape"));
        MsiCloseHandle(pkg);
        pkg = 0;
        r = MsiOpenPackageA(pkgname, &pkg);
        checku("open the package again", r, 0);
        if (!r)
        {
            size = sizeof(buf); strcpy(buf, "kiwi");
            MsiGetPropertyA(pkg, "MYPROP", buf, &size);
            check("the property is gone", buf[0] == 0 && size == 0);
            MsiCloseHandle(pkg);
        }
    }
done:
    MsiCloseHandle(db);
    DeleteFileW(path);
    puts(fails ? "RESULT: FAIL" : "RESULT: PASS");
    return fails != 0;
}
