#include <windows.h>
#include <msi.h>
#include <msiquery.h>
#include <stdio.h>
#include <string.h>

static int fails, checks;
#define CHECK(c, ...) do { checks++; if (!(c)) { fails++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static UINT run(MSIHANDLE db, const char *q)
{
    MSIHANDLE v;
    UINT r = MsiDatabaseOpenViewA(db, q, &v);
    if (r) return r;
    r = MsiViewExecute(v, 0);
    MsiViewClose(v);
    MsiCloseHandle(v);
    return r;
}

static int count_msi_temp(void)
{
    char dir[MAX_PATH], pat[MAX_PATH];
    WIN32_FIND_DATAA fd;
    HANDLE h;
    int n = 0;
    GetTempPathA(sizeof(dir), dir);
    sprintf(pat, "%smsi*.tmp", dir);
    h = FindFirstFileA(pat, &fd);
    if (h == INVALID_HANDLE_VALUE) return 0;
    do n++; while (FindNextFileA(h, &fd));
    FindClose(h);
    return n;
}

static void test_empty(void)
{
    MSIHANDLE pkg = 0, db = 0, view = 0, rec = 0, si = 0, pkg2 = 0;
    char buf[256];
    DWORD size;
    UINT r;
    int before = count_msi_temp();

    r = MsiOpenPackageA("", &pkg);
    CHECK(r == ERROR_SUCCESS, "MsiOpenPackage empty: %u", r);
    if (r) return;
    db = MsiGetActiveDatabase(pkg);
    CHECK(db != 0, "no active database");

    r = MsiDatabaseOpenViewA(db, "SELECT * FROM `_Tables`", &view);
    CHECK(r == 0, "open _Tables %u", r);
    MsiViewExecute(view, 0);
    r = MsiViewFetch(view, &rec);
    size = sizeof(buf); buf[0] = 0;
    MsiRecordGetStringA(rec, 1, buf, &size);
    CHECK(r == 0 && !strcmp(buf, "_Property"), "first table '%s'", buf);
    MsiCloseHandle(rec);
    r = MsiViewFetch(view, &rec);
    size = sizeof(buf); buf[0] = 0;
    MsiRecordGetStringA(rec, 1, buf, &size);
    CHECK(r == 0 && !strcmp(buf, "#_FolderCache"), "second table '%s'", buf);
    MsiCloseHandle(rec);
    MsiViewClose(view); MsiCloseHandle(view);

    CHECK(MsiDatabaseIsTablePersistentA(db, "_Property") == MSICONDITION_FALSE, "_Property persistent");
    CHECK(MsiDatabaseIsTablePersistentA(db, "#_FolderCache") == MSICONDITION_FALSE, "#_FolderCache persistent");

    r = MsiDatabaseOpenViewA(db, "SELECT * FROM `#_FolderCache`", &view);
    MsiViewExecute(view, 0);
    r = MsiViewFetch(view, &rec);
    CHECK(r == 0, "folder cache empty (%u)", r);
    MsiCloseHandle(rec);
    MsiViewClose(view); MsiCloseHandle(view);

    CHECK(MsiDatabaseOpenViewA(db, "SELECT * FROM `_Streams`", &view) == ERROR_BAD_QUERY_SYNTAX, "_Streams exists");
    CHECK(MsiDatabaseOpenViewA(db, "SELECT * FROM `_Storages`", &view) == ERROR_BAD_QUERY_SYNTAX, "_Storages exists");
    CHECK(MsiGetSummaryInformationA(db, NULL, 0, &si) == ERROR_INSTALL_PACKAGE_INVALID, "summary info present");
    CHECK(MsiDatabaseCommit(db) == ERROR_SUCCESS, "commit");

    r = MsiSetPropertyA(pkg, "FASTPROP", "v1");
    CHECK(r == 0, "set property %u", r);
    size = sizeof(buf); buf[0] = 0;
    r = MsiGetPropertyA(pkg, "FASTPROP", buf, &size);
    CHECK(r == 0 && !strcmp(buf, "v1"), "get property '%s'", buf);

    /* two empty packages are independent */
    r = MsiOpenPackageA("", &pkg2);
    CHECK(r == 0, "second empty package %u", r);
    size = sizeof(buf); buf[0] = 'x'; buf[1] = 0;
    MsiGetPropertyA(pkg2, "FASTPROP", buf, &size);
    CHECK(buf[0] == 0, "property leaked into the second package '%s'", buf);
    MsiCloseHandle(pkg2);

    MsiCloseHandle(db);
    MsiCloseHandle(pkg);
    CHECK(count_msi_temp() == before, "scratch database left behind (%d -> %d)", before, count_msi_temp());
}

static void make_cl_db(const char *path, const char *cond)
{
    MSIHANDLE db, si;
    char q[512];
    MsiOpenDatabaseA(path, MSIDBOPEN_CREATE, &db);
    MsiGetSummaryInformationA(db, NULL, 10, &si);
    MsiSummaryInfoSetPropertyA(si, 2, VT_LPSTR, 0, NULL, "fastmsipkg");
    MsiSummaryInfoSetPropertyA(si, 7, VT_LPSTR, 0, NULL, "Intel;1033");
    MsiSummaryInfoSetPropertyA(si, 9, VT_LPSTR, 0, NULL, "{3B6A5D7E-2222-4222-8333-444455556666}");
    MsiSummaryInfoSetPropertyA(si, 14, VT_I4, 200, NULL, NULL);
    MsiSummaryInfoSetPropertyA(si, 15, VT_I4, 0, NULL, NULL);
    MsiSummaryInfoPersist(si);
    MsiCloseHandle(si);
    run(db, "CREATE TABLE `Property` (`Property` CHAR(72) NOT NULL, `Value` CHAR(0) NOT NULL LOCALIZABLE PRIMARY KEY `Property`)");
    run(db, "INSERT INTO `Property` (`Property`,`Value`) VALUES ('ProductCode','{7B6A5D7E-2222-4222-8333-444455556666}')");
    run(db, "INSERT INTO `Property` (`Property`,`Value`) VALUES ('ProductName','fastmsipkg')");
    run(db, "INSERT INTO `Property` (`Property`,`Value`) VALUES ('ProductVersion','1.0.0')");
    run(db, "INSERT INTO `Property` (`Property`,`Value`) VALUES ('ProductLanguage','1033')");
    run(db, "INSERT INTO `Property` (`Property`,`Value`) VALUES ('Manufacturer','fast')");
    run(db, "CREATE TABLE `LaunchCondition` (`Condition` CHAR(255) NOT NULL, `Description` CHAR(255) NOT NULL LOCALIZABLE PRIMARY KEY `Condition`)");
    sprintf(q, "INSERT INTO `LaunchCondition` (`Condition`,`Description`) VALUES ('%s', 'condition failed')", cond);
    run(db, q);
    run(db, "CREATE TABLE `InstallExecuteSequence` (`Action` CHAR(72) NOT NULL, `Condition` CHAR(255), `Sequence` SHORT PRIMARY KEY `Action`)");
    run(db, "INSERT INTO `InstallExecuteSequence` (`Action`,`Sequence`) VALUES ('LaunchConditions', 100)");
    MsiDatabaseCommit(db);
    MsiCloseHandle(db);
}

/* property values on the command line follow the quoting rules: PROP="a b" and PROP=a */
static void test_cmdline(void)
{
    static const struct { const char *cond, *cmd; UINT want; } t[] = {
        { "P = \"a b\"", "P=\"a b\"", ERROR_SUCCESS },
        { "P = \"a b\"", "P=a", ERROR_INSTALL_FAILURE },
        { "P = \"a\"", "P=a", ERROR_SUCCESS },
        { "P = \"a\"", "P=\"a\"", ERROR_SUCCESS },
        { "P = \"x\"\"y\"", "P=\"x\"\"\"\"y\"", ERROR_SUCCESS },
        { "P = \"\"", "P=\"\"", ERROR_SUCCESS },
        { "P = \"a\"", "P=\"a", ERROR_INVALID_COMMAND_LINE },
        { "P = \"a\"", "P=\"a\"b", ERROR_INVALID_COMMAND_LINE },
    };
    UINT i, r;

    MsiSetInternalUI(INSTALLUILEVEL_NONE, NULL);
    for (i = 0; i < sizeof(t) / sizeof(t[0]); i++)
    {
        make_cl_db("fastcl.msi", t[i].cond);
        r = MsiInstallProductA("fastcl.msi", t[i].cmd);
        CHECK(r == t[i].want, "cmd [%s] cond [%s]: got %u want %u", t[i].cmd, t[i].cond, r, t[i].want);
        DeleteFileA("fastcl.msi");
    }
}

int main(void)
{
    test_cmdline();
    test_empty();
    printf("checks %d fails %d\n", checks, fails);
    printf("RESULT: %s\n", fails ? "FAIL" : "PASS");
    return fails != 0;
}
