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

static void make_db(const char *path, int wordcount, const char *cabinet)
{
    MSIHANDLE db, si;
    char q[512];
    MsiOpenDatabaseA(path, MSIDBOPEN_CREATE, &db);
    MsiGetSummaryInformationA(db, NULL, 10, &si);
    MsiSummaryInfoSetPropertyA(si, 2, VT_LPSTR, 0, NULL, "fastmsimode");
    MsiSummaryInfoSetPropertyA(si, 7, VT_LPSTR, 0, NULL, "Intel;1033");
    MsiSummaryInfoSetPropertyA(si, 9, VT_LPSTR, 0, NULL, "{3B6A5D7E-1111-4222-8333-444455556666}");
    MsiSummaryInfoSetPropertyA(si, 14, VT_I4, 200, NULL, NULL);
    MsiSummaryInfoSetPropertyA(si, 15, VT_I4, wordcount, NULL, NULL);
    MsiSummaryInfoPersist(si);
    MsiCloseHandle(si);
    run(db, "CREATE TABLE `Property` (`Property` CHAR(72) NOT NULL, `Value` CHAR(0) NOT NULL LOCALIZABLE PRIMARY KEY `Property`)");
    run(db, "INSERT INTO `Property` (`Property`,`Value`) VALUES ('ProductCode','{7B6A5D7E-1111-4222-8333-444455556666}')");
    run(db, "INSERT INTO `Property` (`Property`,`Value`) VALUES ('ProductName','fastmsimode')");
    run(db, "INSERT INTO `Property` (`Property`,`Value`) VALUES ('ProductVersion','1.0.0')");
    run(db, "INSERT INTO `Property` (`Property`,`Value`) VALUES ('ProductLanguage','1033')");
    run(db, "INSERT INTO `Property` (`Property`,`Value`) VALUES ('Manufacturer','fast')");
    run(db, "CREATE TABLE `Media` (`DiskId` SHORT NOT NULL, `LastSequence` SHORT NOT NULL, `DiskPrompt` CHAR(64) LOCALIZABLE, `Cabinet` CHAR(255), `VolumeLabel` CHAR(32), `Source` CHAR(32) PRIMARY KEY `DiskId`)");
    sprintf(q, "INSERT INTO `Media` (`DiskId`,`LastSequence`,`Cabinet`) VALUES (1, 1, %s)", cabinet);
    run(db, q);
    run(db, "CREATE TABLE `Feature` (`Feature` CHAR(38) NOT NULL, `Feature_Parent` CHAR(38), `Title` CHAR(64) LOCALIZABLE, `Description` CHAR(255) LOCALIZABLE, `Display` SHORT, `Level` SHORT NOT NULL, `Directory_` CHAR(72), `Attributes` SHORT NOT NULL PRIMARY KEY `Feature`)");
    run(db, "CREATE TABLE `Component` (`Component` CHAR(72) NOT NULL, `ComponentId` CHAR(38), `Directory_` CHAR(72) NOT NULL, `Attributes` SHORT NOT NULL, `Condition` CHAR(255), `KeyPath` CHAR(72) PRIMARY KEY `Component`)");
    run(db, "CREATE TABLE `FeatureComponents` (`Feature_` CHAR(38) NOT NULL, `Component_` CHAR(72) NOT NULL PRIMARY KEY `Feature_`, `Component_`)");
    run(db, "CREATE TABLE `Directory` (`Directory` CHAR(72) NOT NULL, `Directory_Parent` CHAR(72), `DefaultDir` CHAR(255) NOT NULL LOCALIZABLE PRIMARY KEY `Directory`)");
    run(db, "INSERT INTO `Directory` (`Directory`,`DefaultDir`) VALUES ('TARGETDIR','SourceDir')");
    /* Fl: local-only component, Fs: source-only, Fo: optional, Fd: disallow advertise + absent, Fe: no components */
    run(db, "INSERT INTO `Feature` (`Feature`,`Level`,`Attributes`) VALUES ('Fl', 1, 0)");
    run(db, "INSERT INTO `Feature` (`Feature`,`Level`,`Attributes`) VALUES ('Fs', 1, 0)");
    run(db, "INSERT INTO `Feature` (`Feature`,`Level`,`Attributes`) VALUES ('Fo', 1, 0)");
    run(db, "INSERT INTO `Feature` (`Feature`,`Level`,`Attributes`) VALUES ('Fd', 1, 24)");
    run(db, "INSERT INTO `Feature` (`Feature`,`Level`,`Attributes`) VALUES ('Fe', 1, 0)");
    run(db, "INSERT INTO `Component` (`Component`,`ComponentId`,`Directory_`,`Attributes`) VALUES ('Cl','{AB6A5D7E-1111-4222-8333-444455556661}','TARGETDIR',0)");
    run(db, "INSERT INTO `Component` (`Component`,`ComponentId`,`Directory_`,`Attributes`) VALUES ('Cs','{AB6A5D7E-1111-4222-8333-444455556662}','TARGETDIR',1)");
    run(db, "INSERT INTO `Component` (`Component`,`ComponentId`,`Directory_`,`Attributes`) VALUES ('Co','{AB6A5D7E-1111-4222-8333-444455556663}','TARGETDIR',2)");
    run(db, "INSERT INTO `FeatureComponents` (`Feature_`,`Component_`) VALUES ('Fl','Cl')");
    run(db, "INSERT INTO `FeatureComponents` (`Feature_`,`Component_`) VALUES ('Fs','Cs')");
    run(db, "INSERT INTO `FeatureComponents` (`Feature_`,`Component_`) VALUES ('Fo','Co')");
    run(db, "INSERT INTO `FeatureComponents` (`Feature_`,`Component_`) VALUES ('Fd','Cl')");
    MsiDatabaseCommit(db);
    MsiCloseHandle(db);
}

static void test_modes(void)
{
    MSIHANDLE pkg;
    UINT r;

    make_db("fastmode1.msi", 0, "'x.cab'");
    r = MsiOpenPackageA("fastmode1.msi", &pkg);
    CHECK(r == 0, "open package %u", r);
    if (r) return;
    CHECK(MsiGetMode(pkg, MSIRUNMODE_CABINET), "cabinet mode off with a cabinet");
    CHECK(!MsiGetMode(pkg, MSIRUNMODE_SOURCESHORTNAMES), "source short names on");
    CHECK(!MsiGetMode(pkg, MSIRUNMODE_TARGETSHORTNAMES), "target short names on");
    CHECK(!MsiGetMode(pkg, MSIRUNMODE_ADMIN), "admin on");
    CHECK(!MsiGetMode(pkg, MSIRUNMODE_ADVERTISE), "advertise on");
    MsiSetPropertyA(pkg, "SHORTFILENAMES", "1");
    CHECK(MsiGetMode(pkg, MSIRUNMODE_TARGETSHORTNAMES), "target short names off with SHORTFILENAMES");
    MsiSetPropertyA(pkg, "ACTION", "ADMIN");
    CHECK(MsiGetMode(pkg, MSIRUNMODE_ADMIN), "admin off with ACTION=ADMIN");
    CHECK(!MsiGetMode(pkg, MSIRUNMODE_ADVERTISE), "advertise on with ACTION=ADMIN");
    MsiSetPropertyA(pkg, "ACTION", "ADVERTISE");
    CHECK(MsiGetMode(pkg, MSIRUNMODE_ADVERTISE), "advertise off with ACTION=ADVERTISE");
    MsiCloseHandle(pkg);

    make_db("fastmode2.msi", 1, "NULL");
    r = MsiOpenPackageA("fastmode2.msi", &pkg);
    CHECK(r == 0, "open package 2 %u", r);
    if (r) return;
    CHECK(!MsiGetMode(pkg, MSIRUNMODE_CABINET), "cabinet mode on without a cabinet");
    CHECK(MsiGetMode(pkg, MSIRUNMODE_SOURCESHORTNAMES), "source short names off with word count 1");
    MsiCloseHandle(pkg);
}

static void test_valid_states(void)
{
    static const struct { const char *f; DWORD want; } t[] = {
        { "Fl", (1 << INSTALLSTATE_LOCAL) | (1 << INSTALLSTATE_ADVERTISED) | (1 << INSTALLSTATE_ABSENT) },
        { "Fs", (1 << INSTALLSTATE_SOURCE) | (1 << INSTALLSTATE_ADVERTISED) | (1 << INSTALLSTATE_ABSENT) },
        { "Fo", (1 << INSTALLSTATE_LOCAL) | (1 << INSTALLSTATE_SOURCE) | (1 << INSTALLSTATE_ADVERTISED) | (1 << INSTALLSTATE_ABSENT) },
        { "Fd", (1 << INSTALLSTATE_LOCAL) },
        { "Fe", (1 << INSTALLSTATE_LOCAL) | (1 << INSTALLSTATE_SOURCE) | (1 << INSTALLSTATE_ADVERTISED) | (1 << INSTALLSTATE_ABSENT) },
    };
    MSIHANDLE pkg;
    DWORD st;
    UINT r, i;

    r = MsiOpenPackageA("fastmode1.msi", &pkg);
    if (r) { CHECK(0, "open %u", r); return; }
    MsiDoActionA(pkg, "CostInitialize");
    for (i = 0; i < sizeof(t) / sizeof(t[0]); i++)
    {
        st = 0xdead;
        r = MsiGetFeatureValidStatesA(pkg, t[i].f, &st);
        CHECK(r == 0 && st == t[i].want, "%s: r=%u states %#lx want %#lx", t[i].f, r, st, t[i].want);
    }
    CHECK(MsiGetFeatureValidStatesA(pkg, "Nope", &st) == ERROR_UNKNOWN_FEATURE, "unknown feature");
    CHECK(MsiGetFeatureValidStatesA(pkg, "Fl", NULL) == ERROR_INVALID_PARAMETER, "null state");
    CHECK(MsiGetFeatureValidStatesA(0xdeadbee, "Fl", &st) == ERROR_INVALID_HANDLE, "bad handle");
    MsiCloseHandle(pkg);
}

static void test_transactions(void)
{
    MSIHANDLE id = 0, id2 = 0;
    HANDLE ev = NULL, ev2 = NULL;
    UINT r;

    CHECK(MsiEndTransaction(1) == ERROR_ACCESS_DENIED, "end without transaction");
    CHECK(MsiBeginTransactionA(NULL, 0, &id, &ev) == ERROR_INVALID_PARAMETER, "begin null name");
    CHECK(MsiBeginTransactionA("t", 0, NULL, &ev) == ERROR_INVALID_PARAMETER, "begin null id");
    CHECK(MsiBeginTransactionA("t", 0x80, &id, &ev) == ERROR_INVALID_PARAMETER, "begin bad attrs");
    r = MsiBeginTransactionA("t", 1, &id, &ev);
    CHECK(r == 0 && id && ev && ev != (HANDLE)0xdeadbeef, "begin %u id %lu ev %p", r, id, ev);
    CHECK(WaitForSingleObject(ev, 0) == WAIT_TIMEOUT, "event signalled at start");
    CHECK(MsiBeginTransactionA("u", 0, &id2, &ev2) == ERROR_INSTALL_ALREADY_RUNNING, "second begin");
    CHECK(MsiJoinTransaction(id + 1, 0, &ev2) == ERROR_INVALID_HANDLE_STATE, "join bad id");
    CHECK(MsiJoinTransaction(id, 0, NULL) == ERROR_INVALID_PARAMETER, "join null event");
    r = MsiJoinTransaction(id, 2, &ev2);
    CHECK(r == 0 && ev2 && WaitForSingleObject(ev, 0) == WAIT_OBJECT_0, "join %u: old owner not told", r);
    CHECK(WaitForSingleObject(ev2, 0) == WAIT_TIMEOUT, "new event signalled");
    CHECK(MsiEndTransaction(7) == ERROR_INVALID_PARAMETER, "end bad state");
    CHECK(MsiEndTransaction(1) == 0, "end commit");
    CHECK(MsiEndTransaction(0) == ERROR_ACCESS_DENIED, "end twice");
    CHECK(MsiJoinTransaction(id, 0, &ev2) == ERROR_INVALID_HANDLE_STATE, "join after end");
    r = MsiBeginTransactionA("again", 0, &id, &ev);
    CHECK(r == 0, "begin after end %u", r);
    CHECK(MsiEndTransaction(0) == 0, "end rollback");
}

static void test_patchlist(void)
{
    MSIHANDLE *recs = NULL;
    DWORD n = 5;

    CHECK(MsiGetPatchFileListA(NULL, "a.msp", &n, &recs) == ERROR_INVALID_PARAMETER, "null product");
    CHECK(MsiGetPatchFileListA("{7B6A5D7E-1111-4222-8333-444455556666}", "", &n, &recs) == ERROR_INVALID_PARAMETER, "empty list");
    CHECK(MsiGetPatchFileListA("{7B6A5D7E-1111-4222-8333-444455556666}", "a.msp", NULL, &recs) == ERROR_INVALID_PARAMETER, "null count");
    CHECK(MsiGetPatchFileListA("{7B6A5D7E-1111-4222-8333-444455556666}", "a.msp", &n, &recs) == ERROR_FUNCTION_FAILED, "unknown product");
}

int main(void)
{
    test_modes();
    test_valid_states();
    test_transactions();
    test_patchlist();
    DeleteFileA("fastmode1.msi");
    DeleteFileA("fastmode2.msi");
    printf("checks %d fails %d\n", checks, fails);
    printf("RESULT: %s\n", fails ? "FAIL" : "PASS");
    return fails != 0;
}
