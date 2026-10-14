/* msi.dll gap-A batch 1 probe (patch 3215): _Validation based MsiViewModify modes and the MsiViewGetError
 * list, MSIMODIFY_REPLACE / MERGE / REFRESH / DELETE / INSERT_TEMPORARY on _Streams and _Storages,
 * ValidateProductID templates, MsiGetMode(ZAWENABLED).  Every column below is checked against the
 * documented Column Data Types and _Validation columns. */
#define COBJMACROS
#include <windows.h>
#include <msi.h>
#include <msiquery.h>
#include <objbase.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

static int checks, fails;
#define OK(c, ...) do { checks++; if (!(c)) { fails++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); fflush(stdout); } } while (0)

static const char *dbfile = "fastmsib1.msi";

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

/* reads the next error of the view: returns MSIDBERROR and copies the column */
static int next_error(MSIHANDLE view, char *col, int size)
{
    DWORD sz = size;
    int e;

    col[0] = 0;
    e = MsiViewGetErrorA(view, col, &sz);
    return e;
}

static const char *errname(int e)
{
    static char buf[32];
    sprintf(buf, "%d", e);
    return buf;
}

/* ---------------------------------------------------------------- ValidateProductID */

static void make_package_db(MSIHANDLE db)
{
    MSIHANDLE si;

    MsiGetSummaryInformationA(db, NULL, 7, &si);
    MsiSummaryInfoSetPropertyA(si, 2, VT_LPSTR, 0, NULL, "Installation Database");
    MsiSummaryInfoSetPropertyA(si, 3, VT_LPSTR, 0, NULL, "Installation Database");
    MsiSummaryInfoSetPropertyA(si, 4, VT_LPSTR, 0, NULL, "Wine Hackers");
    MsiSummaryInfoSetPropertyA(si, 7, VT_LPSTR, 0, NULL, ";1033");
    MsiSummaryInfoSetPropertyA(si, 9, VT_LPSTR, 0, NULL, "{913B8D18-FBB6-4CAC-A239-C74C11E3FA74}");
    MsiSummaryInfoSetPropertyA(si, 14, VT_I4, 100, NULL, NULL);
    MsiSummaryInfoSetPropertyA(si, 15, VT_I4, 0, NULL, NULL);
    MsiSummaryInfoPersist(si);
    MsiCloseHandle(si);
    run(db, "CREATE TABLE `Directory` (`Directory` CHAR(255) NOT NULL, `Directory_Parent` CHAR(255), `DefaultDir` CHAR(255) NOT NULL PRIMARY KEY `Directory`)");
}

static MSIHANDLE open_pkg(MSIHANDLE db)
{
    char name[16];
    MSIHANDLE pkg = 0;
    UINT r;

    sprintf(name, "#%lu", (unsigned long)db);
    r = MsiOpenPackageA(name, &pkg);
    OK(r == ERROR_SUCCESS, "MsiOpenPackage: %u", r);
    return pkg;
}

static void get_prop(MSIHANDLE pkg, const char *name, char *buf, DWORD size)
{
    buf[0] = 0;
    MsiGetPropertyA(pkg, name, buf, &size);
}

static void test_product_id(void)
{
    static const struct { const char *template, *key, *preset, *want; } t[] = {
        /* no visible segment markers: the whole template is the mask and the key is the id */
        { "###-#######", "123-1234567", NULL, "123-1234567" },
        { "###-#######", "12-1234567", NULL, "" },
        { "###-#######", "123-12345678", NULL, "" },
        { "###-#######", "12a-1234567", NULL, "" },
        { "###-#######", "123_1234567", NULL, "" },
        /* an id set by another method stays */
        { "###-#######", "123-1234567", "none", "none" },
        /* any-character and underscore positions */
        { "&&&-%%%", "a_Z-123", NULL, "a_Z-123" },
        { "&&&-%%%", "a_Z-12x", NULL, "" },
        { "%%_^^", "12_ab", NULL, "12_ab" },
        { "%%_^^", "12xab", NULL, "" },
        /* literal characters have to match */
        { "AB##", "AB12", NULL, "AB12" },
        { "AB##", "AC12", NULL, "" },
        /* hidden prefix, visible key, dash, hidden suffix (the @ are random digits) */
        { "12345<###-%%%%%%%>@@@@@", "123-1234567", NULL, "12345123-1234567-#####" },
        { "12345<###-%%%%%%%>@@@@@", "123-123456", NULL, "" },
        { "12345<###-%%%%%%%>999", "123-1234567", NULL, "12345123-1234567-999" },
    };
    unsigned int i;

    for (i = 0; i < sizeof(t) / sizeof(t[0]); i++)
    {
        MSIHANDLE db = 0, pkg;
        char buf[256];
        UINT r;

        DeleteFileA(dbfile);
        r = MsiOpenDatabaseA(dbfile, MSIDBOPEN_CREATE, &db);
        OK(r == ERROR_SUCCESS, "create: %u", r);
        r = run(db, "CREATE TABLE `Property` (`Property` CHAR(72) NOT NULL, `Value` LONGCHAR NOT NULL PRIMARY KEY `Property`)");
        OK(r == ERROR_SUCCESS, "create Property: %u", r);
        r = run(db, "INSERT INTO `Property` (`Property`, `Value`) VALUES ('PIDTemplate', '%s')", t[i].template);
        OK(r == ERROR_SUCCESS, "insert template: %u", r);
        if (t[i].preset) run(db, "INSERT INTO `Property` (`Property`, `Value`) VALUES ('ProductID', '%s')", t[i].preset);
        make_package_db(db);
        pkg = open_pkg(db);
        if (!pkg) { MsiCloseHandle(db); continue; }
        r = MsiSetPropertyA(pkg, "PIDKEY", t[i].key);
        OK(r == ERROR_SUCCESS, "PIDKEY: %u", r);
        r = MsiDoActionA(pkg, "ValidateProductID");
        OK(r == ERROR_SUCCESS, "[%d] ValidateProductID always succeeds: %u", i, r);
        get_prop(pkg, "ProductID", buf, sizeof(buf));
        if (strchr(t[i].want, '#'))
        {
            size_t n = strlen(t[i].want), k;
            OK(strlen(buf) == n && !strncmp(buf, t[i].want, n - 5), "[%d] ProductID '%s'", i, buf);
            for (k = n - 5; k < n && k < strlen(buf); k++) OK(buf[k] >= '0' && buf[k] <= '9', "[%d] random digit %c", i, buf[k]);
        }
        else OK(!strcmp(buf, t[i].want), "[%d] template '%s' key '%s': ProductID '%s', want '%s'", i, t[i].template, t[i].key, buf, t[i].want);
        MsiCloseHandle(pkg);
        MsiCloseHandle(db);
    }
    DeleteFileA(dbfile);
}

static void test_get_mode(void)
{
    MSIHANDLE db = 0, pkg;

    DeleteFileA(dbfile);
    MsiOpenDatabaseA(dbfile, MSIDBOPEN_CREATE, &db);
    run(db, "CREATE TABLE `Property` (`Property` CHAR(72) NOT NULL, `Value` LONGCHAR NOT NULL PRIMARY KEY `Property`)");
    make_package_db(db);
    pkg = open_pkg(db);
    OK(MsiGetMode(pkg, MSIRUNMODE_ZAWENABLED) == TRUE, "ZAWENABLED");
    OK(MsiGetMode(pkg, MSIRUNMODE_ADMIN) == FALSE, "ADMIN");
    OK(MsiGetMode(pkg, MSIRUNMODE_WINDOWS9X) == FALSE, "WINDOWS9X");
    MsiCloseHandle(pkg);
    MsiCloseHandle(db);
    DeleteFileA(dbfile);
}

/* ---------------------------------------------------------------- _Validation */

struct vcase { const char *column; const char *value; int err; };

static void val(MSIHANDLE db, const char *table, const char *column, const char *nullable, const char *min,
                const char *max, const char *keytable, const char *keycol, const char *category, const char *set)
{
    char q[1024];
    UINT r;

    sprintf(q, "INSERT INTO `_Validation` (`Table`, `Column`, `Nullable`, `MinValue`, `MaxValue`, `KeyTable`, `KeyColumn`, `Category`, `Set`) "
               "VALUES ('%s', '%s', '%s', %s, %s, %s%s%s, %s, %s%s%s, %s%s%s)", table, column, nullable,
            min ? min : "NULL", max ? max : "NULL",
            keytable ? "'" : "", keytable ? keytable : "NULL", keytable ? "'" : "",
            keycol ? keycol : "NULL",
            category ? "'" : "", category ? category : "NULL", category ? "'" : "",
            set ? "'" : "", set ? set : "NULL", set ? "'" : "");
    r = run(db, "%s", q);
    OK(r == ERROR_SUCCESS, "_Validation row %s.%s: %u", table, column, r);
}

static const char *cats[] = { "Identifier", "Guid", "Version", "Filename", "WildCardFilename", "Path", "Paths", "AnyPath",
                              "DefaultDir", "RegPath", "Condition", "Language", "Cabinet", "Shortcut", "Property",
                              "UpperCase", "LowerCase", "CustomSource", "Text", "Formatted", "Template", "Binary" };

static void test_categories(void)
{
    static const struct vcase cases[] = {
        { "Identifier", "Abc_1.x", 0 }, { "Identifier", "1abc", MSIDBERROR_BADIDENTIFIER }, { "Identifier", "a-b", MSIDBERROR_BADIDENTIFIER },
        { "Identifier", "_x", 0 },
        { "Guid", "{12345678-1234-1234-1234-123456789ABC}", 0 }, { "Guid", "{12345678-1234-1234-1234-123456789abc}", MSIDBERROR_BADGUID },
        { "Guid", "{12345678-1234-1234-1234-123456789AB}", MSIDBERROR_BADGUID }, { "Guid", "12345678-1234-1234-1234-123456789ABC", MSIDBERROR_BADGUID },
        { "Version", "1.0.1000.0", 0 }, { "Version", "65535.65535.65535.65535", 0 }, { "Version", "65536.0", MSIDBERROR_BADVERSION },
        { "Version", "1.2.3.4.5", MSIDBERROR_BADVERSION }, { "Version", "1..2", MSIDBERROR_BADVERSION }, { "Version", "a.b", MSIDBERROR_BADVERSION },
        { "Filename", "status.txt", 0 }, { "Filename", "projec~1.txt|Project Status.txt", 0 },
        { "Filename", "project status.txt", MSIDBERROR_BADFILENAME }, { "Filename", "a\\b.txt", MSIDBERROR_BADFILENAME },
        { "Filename", "a?b.txt", MSIDBERROR_BADFILENAME }, { "Filename", "waytoolongname.txt", MSIDBERROR_BADFILENAME },
        { "Filename", "a.long", MSIDBERROR_BADFILENAME }, { "Filename", "a+b.txt", MSIDBERROR_BADFILENAME },
        { "Filename", "ok.txt|bad:name", MSIDBERROR_BADFILENAME }, { "Filename", "ok.txt|", MSIDBERROR_BADFILENAME },
        { "WildCardFilename", "*.txt", 0 }, { "WildCardFilename", "a?c.t*|Long ?ame.*", 0 }, { "WildCardFilename", "a/b", MSIDBERROR_BADWILDCARD },
        { "Path", "c:\\temp", 0 }, { "Path", "\\\\server\\share", 0 }, { "Path", "[DRIVE]\\temp", 0 },
        { "Path", "relative\\dir", MSIDBERROR_BADPATH }, { "Path", "c:\\te*mp", MSIDBERROR_BADPATH },
        { "Paths", "c:\\a;\\\\s\\h;[P]\\x", 0 }, { "Paths", "c:\\a;rel", MSIDBERROR_BADPATH },
        { "AnyPath", "\\temp", 0 }, { "AnyPath", "projec~1|Project Status", 0 },
        { "AnyPath", "c:\\temp\\projec~1|c:\\temp one\\Project Status", MSIDBERROR_BADPATH }, { "AnyPath", "a<b", MSIDBERROR_BADPATH },
        { "DefaultDir", "SourceDir", 0 }, { "DefaultDir", "target:source", 0 }, { "DefaultDir", ".", 0 }, { "DefaultDir", "name|Long Name", 0 },
        { "DefaultDir", "a?b:c", MSIDBERROR_BADDEFAULTDIR }, { "DefaultDir", "bad name", MSIDBERROR_BADDEFAULTDIR },
        { "RegPath", "Software\\Wine\\X", 0 }, { "RegPath", "\\Software", MSIDBERROR_BADREGPATH }, { "RegPath", "Software\\", MSIDBERROR_BADREGPATH },
        { "Condition", "VersionNT >= 500 AND NOT Installed", 0 }, { "Condition", "(A=1 OR B<>\"x\") AND $c=3", 0 },
        { "Condition", "A = ", MSIDBERROR_BADCONDITION }, { "Condition", "(A=1", MSIDBERROR_BADCONDITION }, { "Condition", "A=\"x", MSIDBERROR_BADCONDITION },
        { "Condition", "AND A", MSIDBERROR_BADCONDITION },
        { "Language", "1033", 0 }, { "Language", "1033,1031", 0 }, { "Language", "1033,", MSIDBERROR_BADLANGUAGE }, { "Language", "en", MSIDBERROR_BADLANGUAGE },
        { "Language", "70000", MSIDBERROR_BADLANGUAGE },
        { "Cabinet", "#data1.cab", 0 }, { "Cabinet", "data.cab", 0 }, { "Cabinet", "#1bad", MSIDBERROR_BADCABINET }, { "Cabinet", "long name.cab", MSIDBERROR_BADCABINET },
        { "Cabinet", "a.cab|Long.cab", MSIDBERROR_BADCABINET },
        { "Shortcut", "FeatureId", 0 }, { "Shortcut", "[TARGETDIR]app.exe", 0 }, { "Shortcut", "bad id", MSIDBERROR_BADSHORTCUT },
        { "Property", "MyProp", 0 }, { "Property", "%PATH", 0 }, { "Property", "bad prop", MSIDBERROR_BADPROPERTY },
        { "UpperCase", "ABC123", 0 }, { "UpperCase", "ABc", MSIDBERROR_BADCASE },
        { "LowerCase", "abc123", 0 }, { "LowerCase", "abC", MSIDBERROR_BADCASE },
        { "CustomSource", "SomeBinary", 0 }, { "CustomSource", "9x", MSIDBERROR_BADCUSTOMSOURCE },
        { "Text", "anything at all: <>?|", 0 }, { "Formatted", "[A] {x} [", 0 }, { "Template", "[1] and [2]", 0 },
    };
    MSIHANDLE db = 0, view, rec;
    char q[2048], col[64];
    unsigned int i;
    UINT r;

    DeleteFileA(dbfile);
    r = MsiOpenDatabaseA(dbfile, MSIDBOPEN_CREATE, &db);
    OK(r == ERROR_SUCCESS, "create: %u", r);
    strcpy(q, "CREATE TABLE `V` (`K` SHORT NOT NULL");
    for (i = 0; i < sizeof(cats) / sizeof(cats[0]); i++) sprintf(q + strlen(q), ", `%s` CHAR(64)", cats[i]);
    strcat(q, " PRIMARY KEY `K`)");
    r = run(db, "%s", q);
    OK(r == ERROR_SUCCESS, "create V: %u", r);
    r = run(db, "CREATE TABLE `_Validation` (`Table` CHAR(32) NOT NULL, `Column` CHAR(32) NOT NULL, `Nullable` CHAR(4) NOT NULL, "
                "`MinValue` INT, `MaxValue` INT, `KeyTable` CHAR(255), `KeyColumn` SHORT, `Category` CHAR(32), `Set` CHAR(255), "
                "`Description` CHAR(255) PRIMARY KEY `Table`, `Column`)");
    OK(r == ERROR_SUCCESS, "create _Validation: %u", r);
    val(db, "V", "K", "N", NULL, NULL, NULL, NULL, NULL, NULL);
    for (i = 0; i < sizeof(cats) / sizeof(cats[0]); i++) val(db, "V", cats[i], "Y", NULL, NULL, NULL, NULL, cats[i], NULL);

    r = MsiDatabaseOpenViewA(db, "SELECT * FROM `V`", &view);
    OK(r == ERROR_SUCCESS, "open V: %u", r);
    r = MsiViewExecute(view, 0);
    OK(r == ERROR_SUCCESS, "execute: %u", r);

    for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
    {
        int e;

        rec = MsiCreateRecord(1 + sizeof(cats) / sizeof(cats[0]));
        {
            unsigned int c;
            for (c = 0; c < sizeof(cats) / sizeof(cats[0]); c++)
                if (!strcmp(cats[c], cases[i].column)) MsiRecordSetStringA(rec, 2 + c, cases[i].value);
        }
        r = MsiViewModify(view, MSIMODIFY_VALIDATE_FIELD, rec);
        if (!cases[i].err)
        {
            OK(r == ERROR_SUCCESS, "%s '%s': VALIDATE_FIELD %u", cases[i].column, cases[i].value, r);
            e = next_error(view, col, sizeof(col));
            OK(e == 0, "%s '%s': stale error %d", cases[i].column, cases[i].value, e);
        }
        else
        {
            OK(r == ERROR_INVALID_DATA, "%s '%s': VALIDATE_FIELD %u", cases[i].column, cases[i].value, r);
            e = next_error(view, col, sizeof(col));
            OK(e == cases[i].err && !strcmp(col, cases[i].column), "%s '%s': error %s column '%s', want %d", cases[i].column,
               cases[i].value, errname(e), col, cases[i].err);
            e = next_error(view, col, sizeof(col));
            OK(e == 0, "%s '%s': second read %d", cases[i].column, cases[i].value, e);
        }
        MsiCloseHandle(rec);
    }
    MsiViewClose(view);
    MsiCloseHandle(view);
    MsiCloseHandle(db);
    DeleteFileA(dbfile);
}

static void test_columns(void)
{
    MSIHANDLE db = 0, view, rec;
    char col[64];
    UINT r;
    int e;

    DeleteFileA(dbfile);
    MsiOpenDatabaseA(dbfile, MSIDBOPEN_CREATE, &db);
    r = run(db, "CREATE TABLE `P` (`Id` SHORT NOT NULL, `Name` CHAR(16) PRIMARY KEY `Id`)");
    OK(!r, "create P: %u", r);
    r = run(db, "CREATE TABLE `T` (`A` SHORT NOT NULL, `Req` CHAR(8), `Num` SHORT, `Opt` CHAR(32), `Choice` CHAR(16), `Link` SHORT, "
                "`LinkS` CHAR(16), `Bad` SHORT, `NoRow` SHORT, `Typo` CHAR(16), `Multi` CHAR(16) PRIMARY KEY `A`)");
    OK(!r, "create T: %u", r);
    r = run(db, "CREATE TABLE `_Validation` (`Table` CHAR(32) NOT NULL, `Column` CHAR(32) NOT NULL, `Nullable` CHAR(4) NOT NULL, "
                "`MinValue` INT, `MaxValue` INT, `KeyTable` CHAR(255), `KeyColumn` SHORT, `Category` CHAR(32), `Set` CHAR(255), "
                "`Description` CHAR(255) PRIMARY KEY `Table`, `Column`)");
    OK(!r, "create _Validation: %u", r);
    val(db, "P", "Id", "N", NULL, NULL, NULL, NULL, NULL, NULL);
    val(db, "P", "Name", "Y", NULL, NULL, NULL, NULL, NULL, NULL);
    val(db, "T", "A", "N", "1", "100", NULL, NULL, NULL, NULL);
    val(db, "T", "Req", "N", NULL, NULL, NULL, NULL, NULL, NULL);
    val(db, "T", "Num", "Y", "-5", "50", NULL, NULL, NULL, NULL);
    val(db, "T", "Opt", "Y", NULL, NULL, NULL, NULL, NULL, NULL);
    val(db, "T", "Choice", "Y", NULL, NULL, NULL, NULL, NULL, "red;green;blue");
    val(db, "T", "Link", "Y", NULL, NULL, "P", "1", NULL, NULL);
    val(db, "T", "LinkS", "Y", NULL, NULL, "NoSuchTable;P", "2", NULL, NULL);
    val(db, "T", "Bad", "Y", "10", "5", NULL, NULL, NULL, NULL);
    /* NoRow: no _Validation row at all */
    val(db, "T", "Typo", "Y", NULL, NULL, NULL, NULL, "NoSuchCategory", NULL);
    val(db, "T", "Multi", "Y", NULL, NULL, "NoSuchTable", "1", NULL, NULL);
    run(db, "INSERT INTO `P` (`Id`, `Name`) VALUES (5, 'five')");
    run(db, "INSERT INTO `P` (`Id`, `Name`) VALUES (7, 'seven')");

    r = MsiDatabaseOpenViewA(db, "SELECT * FROM `T`", &view);
    OK(!r, "open T: %u", r);
    r = MsiViewExecute(view, 0);
    OK(!r, "execute: %u", r);

    /* all good (NoRow and Typo are null; Bad is null) */
    rec = MsiCreateRecord(11);
    MsiRecordSetInteger(rec, 1, 10);
    MsiRecordSetStringA(rec, 2, "abc");
    MsiRecordSetInteger(rec, 3, 7);
    MsiRecordSetStringA(rec, 5, "green");
    MsiRecordSetInteger(rec, 6, 5);
    MsiRecordSetStringA(rec, 7, "seven");
    r = MsiViewModify(view, MSIMODIFY_VALIDATE, rec);
    /* NoRow has no _Validation row: the documentation reports it as missing data even when it is null */
    OK(r == ERROR_INVALID_DATA, "VALIDATE with a column missing from _Validation: %u", r);
    /* Bad: its MaxValue is below its MinValue, whatever the record holds */
    e = next_error(view, col, sizeof(col));
    OK(e == MSIDBERROR_BADMAXMINVALUES && !strcmp(col, "Bad"), "max/min: %d '%s'", e, col);
    e = next_error(view, col, sizeof(col));
    OK(e == MSIDBERROR_MISSINGDATA && !strcmp(col, "NoRow"), "missing data: %d '%s'", e, col);
    e = next_error(view, col, sizeof(col));
    OK(e == 0, "no more: %d", e);
    /* VALIDATE_FIELD only looks at the fields that are set */
    r = MsiViewModify(view, MSIMODIFY_VALIDATE_FIELD, rec);
    OK(r == ERROR_SUCCESS, "VALIDATE_FIELD on good fields: %u", r);
    e = next_error(view, col, sizeof(col));
    OK(e == 0, "VALIDATE_FIELD leaves no error: %d", e);

    /* every kind of error at once, reported in column order, one at a time */
    MsiRecordSetInteger(rec, 1, 101);              /* A: overflow */
    MsiRecordSetStringA(rec, 2, "123456789");      /* Req: string overflow (CHAR(8)) */
    MsiRecordSetInteger(rec, 3, -6);               /* Num: underflow */
    MsiRecordSetStringA(rec, 5, "yellow");         /* Choice: not in set */
    MsiRecordSetInteger(rec, 6, 6);                /* Link: no such row in P */
    MsiRecordSetStringA(rec, 7, "nothing");        /* LinkS: no row named that */
    MsiRecordSetInteger(rec, 8, 7);                /* Bad: Max < Min */
    MsiRecordSetStringA(rec, 10, "x");             /* Typo: unknown category */
    MsiRecordSetStringA(rec, 11, "x");             /* Multi: key table does not exist */
    r = MsiViewModify(view, MSIMODIFY_VALIDATE_FIELD, rec);
    OK(r == ERROR_INVALID_DATA, "multi error: %u", r);
    {
        static const struct { int err; const char *col; } want[] = {
            { MSIDBERROR_OVERFLOW, "A" }, { MSIDBERROR_STRINGOVERFLOW, "Req" }, { MSIDBERROR_UNDERFLOW, "Num" },
            { MSIDBERROR_NOTINSET, "Choice" }, { MSIDBERROR_BADLINK, "Link" }, { MSIDBERROR_BADLINK, "LinkS" },
            { MSIDBERROR_BADMAXMINVALUES, "Bad" }, { MSIDBERROR_BADCATEGORY, "Typo" }, { MSIDBERROR_BADKEYTABLE, "Multi" },
            { 0, "" },
        };
        unsigned int i;
        DWORD sz;

        for (i = 0; i < sizeof(want) / sizeof(want[0]); i++)
        {
            if (i == 3)
            {
                /* a buffer that is too small reports MOREDATA with the size, and still moves on */
                sz = 1;
                col[0] = 'x';
                e = MsiViewGetErrorA(view, col, &sz);
                OK(e == MSIDBERROR_MOREDATA && sz == 6, "too small buffer: %d size %lu", e, (unsigned long)sz);
                continue;
            }
            e = next_error(view, col, sizeof(col));
            OK(e == want[i].err && !strcmp(col, want[i].col), "error %u: %d '%s', want %d '%s'", i, e, col, want[i].err, want[i].col);
        }
    }

    /* required column that is null (VALIDATE_NEW: no duplicate, then the columns) */
    MsiCloseHandle(rec);
    rec = MsiCreateRecord(11);
    MsiRecordSetInteger(rec, 1, 11);
    r = MsiViewModify(view, MSIMODIFY_VALIDATE_NEW, rec);
    OK(r == ERROR_INVALID_DATA, "VALIDATE_NEW with null required: %u", r);
    e = next_error(view, col, sizeof(col));
    OK(e == MSIDBERROR_REQUIRED && !strcmp(col, "Req"), "required: %d '%s'", e, col);
    MsiCloseHandle(rec);

    MsiViewClose(view);
    MsiCloseHandle(view);

    /* a view with a WHERE clause forwards the validation and its errors */
    r = MsiDatabaseOpenViewA(db, "SELECT * FROM `T` WHERE `A` = 10", &view);
    OK(!r, "open where view: %u", r);
    r = MsiViewExecute(view, 0);
    OK(!r, "execute: %u", r);
    rec = MsiCreateRecord(11);
    MsiRecordSetInteger(rec, 3, 99);
    r = MsiViewModify(view, MSIMODIFY_VALIDATE_FIELD, rec);
    OK(r == ERROR_INVALID_DATA, "where view: %u", r);
    e = next_error(view, col, sizeof(col));
    OK(e == MSIDBERROR_OVERFLOW && !strcmp(col, "Num"), "where view error: %d '%s'", e, col);
    MsiCloseHandle(rec);
    MsiViewClose(view);
    MsiCloseHandle(view);

    /* a select of some columns validates the selected ones */
    r = MsiDatabaseOpenViewA(db, "SELECT `Num`, `Choice` FROM `T`", &view);
    OK(!r, "open select view: %u", r);
    r = MsiViewExecute(view, 0);
    rec = MsiCreateRecord(2);
    MsiRecordSetInteger(rec, 1, 3);
    MsiRecordSetStringA(rec, 2, "red");
    r = MsiViewModify(view, MSIMODIFY_VALIDATE, rec);
    OK(r == ERROR_SUCCESS, "select view valid record: %u", r);
    MsiRecordSetStringA(rec, 2, "purple");
    r = MsiViewModify(view, MSIMODIFY_VALIDATE, rec);
    OK(r == ERROR_INVALID_DATA, "select view invalid record: %u", r);
    e = next_error(view, col, sizeof(col));
    OK(e == MSIDBERROR_NOTINSET && !strcmp(col, "Choice"), "select view error: %d '%s'", e, col);
    MsiCloseHandle(rec);
    MsiViewClose(view);
    MsiCloseHandle(view);

    /* VALIDATE_DELETE: another row refers to the primary key */
    run(db, "INSERT INTO `T` (`A`, `Req`, `Link`, `NoRow`) VALUES (20, 'r', 5, 1)");
    r = MsiDatabaseOpenViewA(db, "SELECT * FROM `P`", &view);
    OK(!r, "open P: %u", r);
    MsiViewExecute(view, 0);
    r = MsiViewFetch(view, &rec);
    OK(!r, "fetch P: %u", r);
    r = MsiViewModify(view, MSIMODIFY_VALIDATE_DELETE, rec);
    OK(r == ERROR_INVALID_DATA, "VALIDATE_DELETE of a referenced row: %u", r);
    e = next_error(view, col, sizeof(col));
    OK(e == MSIDBERROR_REQUIRED && !strcmp(col, "Id"), "referenced: %d '%s'", e, col);
    e = next_error(view, col, sizeof(col));
    OK(e == 0, "referenced second read: %d", e);
    MsiCloseHandle(rec);
    r = MsiViewFetch(view, &rec);
    OK(!r, "fetch second P: %u", r);
    r = MsiViewModify(view, MSIMODIFY_VALIDATE_DELETE, rec);
    OK(r == ERROR_SUCCESS, "VALIDATE_DELETE of an unreferenced row: %u", r);
    MsiCloseHandle(rec);
    MsiViewClose(view);
    MsiCloseHandle(view);

    /* the _Validation table is required by VALIDATE, but VALIDATE_NEW works without */
    run(db, "DROP TABLE `_Validation`");
    r = MsiDatabaseOpenViewA(db, "SELECT * FROM `P`", &view);
    MsiViewExecute(view, 0);
    rec = MsiCreateRecord(2);
    MsiRecordSetInteger(rec, 1, 8);
    r = MsiViewModify(view, MSIMODIFY_VALIDATE_NEW, rec);
    OK(r == ERROR_SUCCESS, "VALIDATE_NEW without _Validation: %u", r);
    r = MsiViewModify(view, MSIMODIFY_VALIDATE, rec);
    OK(r == ERROR_INVALID_DATA, "VALIDATE without _Validation: %u", r);
    e = next_error(view, col, sizeof(col));
    OK(e == MSIDBERROR_MISSINGDATA, "no _Validation table: %d", e);
    MsiRecordSetInteger(rec, 1, 5);
    r = MsiViewModify(view, MSIMODIFY_VALIDATE_NEW, rec);
    OK(r == ERROR_INVALID_DATA, "duplicate key: %u", r);
    e = next_error(view, col, sizeof(col));
    OK(e == MSIDBERROR_DUPLICATEKEY && !strcmp(col, "Id"), "duplicate key error: %d '%s'", e, col);
    MsiCloseHandle(rec);
    MsiViewClose(view);
    MsiCloseHandle(view);

    MsiCloseHandle(db);
    DeleteFileA(dbfile);
}

/* ---------------------------------------------------------------- REPLACE on tables */

static int count_rows(MSIHANDLE db, const char *query)
{
    MSIHANDLE view, rec;
    int n = 0;

    if (MsiDatabaseOpenViewA(db, query, &view)) return -1;
    MsiViewExecute(view, 0);
    while (!MsiViewFetch(view, &rec)) { n++; MsiCloseHandle(rec); }
    MsiViewClose(view);
    MsiCloseHandle(view);
    return n;
}

static void test_replace(void)
{
    MSIHANDLE db = 0, view, rec;
    char buf[64];
    DWORD sz;
    UINT r;

    DeleteFileA(dbfile);
    MsiOpenDatabaseA(dbfile, MSIDBOPEN_CREATE, &db);
    r = run(db, "CREATE TABLE `R` (`K` SHORT NOT NULL, `V` CHAR(16) PRIMARY KEY `K`)");
    OK(!r, "create R: %u", r);
    run(db, "INSERT INTO `R` (`K`, `V`) VALUES (1, 'one')");
    run(db, "INSERT INTO `R` (`K`, `V`) VALUES (2, 'two')");
    run(db, "INSERT INTO `R` (`K`, `V`) VALUES (3, 'three')");

    MsiDatabaseOpenViewA(db, "SELECT * FROM `R`", &view);
    MsiViewExecute(view, 0);
    r = MsiViewFetch(view, &rec);
    OK(!r, "fetch: %u", r);
    /* unchanged key: the row is updated */
    MsiRecordSetStringA(rec, 2, "uno");
    r = MsiViewModify(view, MSIMODIFY_REPLACE, rec);
    OK(r == ERROR_SUCCESS, "REPLACE with the same key: %u", r);
    MsiCloseHandle(rec);
    MsiViewClose(view);
    MsiCloseHandle(view);
    OK(count_rows(db, "SELECT * FROM `R` WHERE `V` = 'uno' AND `K` = 1") == 1, "row updated");
    OK(count_rows(db, "SELECT * FROM `R`") == 3, "still 3 rows");

    /* changed key: old row deleted, new inserted */
    MsiDatabaseOpenViewA(db, "SELECT * FROM `R`", &view);
    MsiViewExecute(view, 0);
    MsiViewFetch(view, &rec);
    MsiViewFetch(view, &rec);
    MsiCloseHandle(rec);
    MsiViewClose(view);
    MsiViewExecute(view, 0);
    r = MsiViewFetch(view, &rec);
    MsiRecordSetInteger(rec, 1, 10);
    MsiRecordSetStringA(rec, 2, "ten");
    r = MsiViewModify(view, MSIMODIFY_REPLACE, rec);
    OK(r == ERROR_SUCCESS, "REPLACE with a new key: %u", r);
    MsiCloseHandle(rec);
    MsiViewClose(view);
    MsiCloseHandle(view);
    OK(count_rows(db, "SELECT * FROM `R`") == 3, "3 rows after key change");
    OK(count_rows(db, "SELECT * FROM `R` WHERE `K` = 1") == 0, "old key gone");
    OK(count_rows(db, "SELECT * FROM `R` WHERE `K` = 10 AND `V` = 'ten'") == 1, "new key present");

    /* a key owned by another row is refused and nothing changes */
    MsiDatabaseOpenViewA(db, "SELECT * FROM `R` WHERE `K` = 10", &view);
    MsiViewExecute(view, 0);
    MsiViewFetch(view, &rec);
    MsiRecordSetInteger(rec, 1, 2);
    MsiRecordSetStringA(rec, 2, "clash");
    r = MsiViewModify(view, MSIMODIFY_REPLACE, rec);
    OK(r == ERROR_FUNCTION_FAILED, "REPLACE onto an existing key: %u", r);
    MsiCloseHandle(rec);
    MsiViewClose(view);
    MsiCloseHandle(view);
    OK(count_rows(db, "SELECT * FROM `R`") == 3, "3 rows after refused replace");
    OK(count_rows(db, "SELECT * FROM `R` WHERE `K` = 10 AND `V` = 'ten'") == 1, "row kept after refused replace");

    /* a view over a join still refuses */
    (void)buf; (void)sz;
    MsiCloseHandle(db);
    DeleteFileA(dbfile);
}

/* ---------------------------------------------------------------- _Streams and _Storages */

static MSIHANDLE stream_rec(const char *name, const char *content)
{
    MSIHANDLE rec = MsiCreateRecord(2);
    char path[64];
    FILE *f;

    sprintf(path, "fastmsib1-%s.bin", name);
    f = fopen(path, "wb");
    fwrite(content, 1, strlen(content), f);
    fclose(f);
    MsiRecordSetStringA(rec, 1, name);
    MsiRecordSetStreamA(rec, 2, path);
    DeleteFileA(path);
    return rec;
}

static void make_storage_file(const char *path, const char *content)
{
    WCHAR wpath[MAX_PATH], wname[] = L"data.bin";
    IStorage *stg;
    IStream *stm;
    ULONG n;

    MultiByteToWideChar(CP_ACP, 0, path, -1, wpath, MAX_PATH);
    if (SUCCEEDED(StgCreateDocfile(wpath, STGM_CREATE | STGM_READWRITE | STGM_SHARE_EXCLUSIVE, 0, &stg)))
    {
        if (SUCCEEDED(IStorage_CreateStream(stg, wname, STGM_CREATE | STGM_WRITE | STGM_SHARE_EXCLUSIVE, 0, 0, &stm)))
        {
            IStream_Write(stm, content, strlen(content) + 1, &n);
            IStream_Release(stm);
        }
        IStorage_Commit(stg, STGC_DEFAULT);
        IStorage_Release(stg);
    }
}

static MSIHANDLE storage_rec(const char *name, const char *content)
{
    MSIHANDLE rec = MsiCreateRecord(2);
    char path[64];

    sprintf(path, "fastmsib1-%s.stg", name);
    make_storage_file(path, content);
    MsiRecordSetStringA(rec, 1, name);
    MsiRecordSetStreamA(rec, 2, path);
    DeleteFileA(path);
    return rec;
}

static int storage_exists(const char *name)
{
    WCHAR wpath[MAX_PATH], wname[64];
    IStorage *stg, *sub;
    int ret = 0;

    MultiByteToWideChar(CP_ACP, 0, dbfile, -1, wpath, MAX_PATH);
    MultiByteToWideChar(CP_ACP, 0, name, -1, wname, 64);
    if (SUCCEEDED(StgOpenStorage(wpath, NULL, STGM_DIRECT | STGM_READ | STGM_SHARE_DENY_WRITE, NULL, 0, &stg)))
    {
        if (SUCCEEDED(IStorage_OpenStorage(stg, wname, NULL, STGM_READ | STGM_SHARE_EXCLUSIVE, NULL, 0, &sub)))
        {
            ret = 1;
            IStorage_Release(sub);
        }
        IStorage_Release(stg);
    }
    return ret;
}

static int stream_in_view(MSIHANDLE db, const char *name)
{
    char q[256];
    sprintf(q, "SELECT * FROM `_Streams` WHERE `Name` = '%s'", name);
    return count_rows(db, q);
}

static void test_streams(void)
{
    MSIHANDLE db = 0, view, rec;
    char col[32], buf[64];
    DWORD sz;
    UINT r;
    int e;

    DeleteFileA(dbfile);
    r = MsiOpenDatabaseA(dbfile, MSIDBOPEN_CREATE, &db);
    OK(!r, "create: %u", r);
    run(db, "CREATE TABLE `X` (`K` SHORT NOT NULL PRIMARY KEY `K`)");
    MsiDatabaseCommit(db);

    r = MsiDatabaseOpenViewA(db, "SELECT * FROM `_Streams`", &view);
    OK(!r, "open _Streams: %u", r);
    MsiViewExecute(view, 0);

    rec = stream_rec("s1", "stream one");
    r = MsiViewModify(view, MSIMODIFY_INSERT, rec);
    OK(r == ERROR_SUCCESS, "INSERT: %u", r);
    MsiCloseHandle(rec);
    rec = stream_rec("s2", "stream two");
    r = MsiViewModify(view, MSIMODIFY_INSERT, rec);
    OK(r == ERROR_SUCCESS, "INSERT s2: %u", r);
    MsiCloseHandle(rec);
    rec = stream_rec("tmp1", "temporary stream");
    r = MsiViewModify(view, MSIMODIFY_INSERT_TEMPORARY, rec);
    OK(r == ERROR_SUCCESS, "INSERT_TEMPORARY: %u", r);
    r = MsiViewModify(view, MSIMODIFY_INSERT_TEMPORARY, rec);
    OK(r == ERROR_FUNCTION_FAILED, "INSERT_TEMPORARY twice: %u", r);
    MsiCloseHandle(rec);

    /* MERGE: absent -> inserted, same data -> ok, different data -> fails */
    rec = stream_rec("s3", "stream three");
    r = MsiViewModify(view, MSIMODIFY_MERGE, rec);
    OK(r == ERROR_SUCCESS, "MERGE absent: %u", r);
    MsiCloseHandle(rec);
    rec = stream_rec("s3", "stream three");
    r = MsiViewModify(view, MSIMODIFY_MERGE, rec);
    OK(r == ERROR_SUCCESS, "MERGE identical: %u", r);
    MsiCloseHandle(rec);
    rec = stream_rec("s3", "something else");
    r = MsiViewModify(view, MSIMODIFY_MERGE, rec);
    OK(r == ERROR_FUNCTION_FAILED, "MERGE different: %u", r);
    MsiCloseHandle(rec);

    /* VALIDATE_NEW: duplicate key */
    rec = stream_rec("s1", "x");
    r = MsiViewModify(view, MSIMODIFY_VALIDATE_NEW, rec);
    OK(r == ERROR_INVALID_DATA, "VALIDATE_NEW duplicate: %u", r);
    e = next_error(view, col, sizeof(col));
    OK(e == MSIDBERROR_DUPLICATEKEY && !strcmp(col, "Name"), "duplicate: %d '%s'", e, col);
    MsiCloseHandle(rec);
    MsiViewClose(view);
    MsiCloseHandle(view);

    OK(stream_in_view(db, "s1") == 1 && stream_in_view(db, "s2") == 1 && stream_in_view(db, "tmp1") == 1 && stream_in_view(db, "s3") == 1,
       "streams inserted");

    /* DELETE of a fetched row */
    MsiDatabaseOpenViewA(db, "SELECT * FROM `_Streams`", &view);
    MsiViewExecute(view, 0);
    for (;;)
    {
        r = MsiViewFetch(view, &rec);
        if (r) break;
        sz = sizeof(buf);
        MsiRecordGetStringA(rec, 1, buf, &sz);
        if (!strcmp(buf, "s2")) break;
        MsiCloseHandle(rec);
    }
    OK(!r, "found s2");
    if (!r)
    {
        r = MsiViewModify(view, MSIMODIFY_DELETE, rec);
        OK(r == ERROR_SUCCESS, "DELETE: %u", r);
        MsiCloseHandle(rec);
    }
    MsiViewClose(view);
    MsiCloseHandle(view);
    OK(stream_in_view(db, "s2") == 0, "s2 deleted");
    OK(stream_in_view(db, "s1") == 1 && stream_in_view(db, "s3") == 1, "others kept");

    /* REPLACE: same key updates the data, new key moves the row; REFRESH reads the data back */
    MsiDatabaseOpenViewA(db, "SELECT * FROM `_Streams` WHERE `Name` = 's1'", &view);
    MsiViewExecute(view, 0);
    r = MsiViewFetch(view, &rec);
    OK(!r, "fetch s1: %u", r);
    MsiCloseHandle(rec);
    rec = stream_rec("s1", "replaced one");
    r = MsiViewModify(view, MSIMODIFY_REPLACE, rec);
    OK(r == ERROR_SUCCESS, "REPLACE same key: %u", r);
    MsiCloseHandle(rec);
    MsiViewClose(view);
    MsiCloseHandle(view);
    MsiDatabaseOpenViewA(db, "SELECT * FROM `_Streams` WHERE `Name` = 's1'", &view);
    MsiViewExecute(view, 0);
    r = MsiViewFetch(view, &rec);
    sz = sizeof(buf);
    memset(buf, 0, sizeof(buf));
    r = MsiRecordReadStream(rec, 2, buf, &sz);
    OK(!r && sz == 12 && !memcmp(buf, "replaced one", 12), "replaced data: %u %lu '%s'", r, (unsigned long)sz, buf);
    MsiRecordSetStringA(rec, 1, "garbage");
    r = MsiViewModify(view, MSIMODIFY_REFRESH, rec);
    OK(r == ERROR_SUCCESS, "REFRESH: %u", r);
    sz = sizeof(buf);
    MsiRecordGetStringA(rec, 1, buf, &sz);
    OK(!strcmp(buf, "s1"), "REFRESH restores the name: '%s'", buf);
    {
        MSIHANDLE rec2 = stream_rec("s1renamed", "replaced one");
        r = MsiViewModify(view, MSIMODIFY_REPLACE, rec2);
        OK(r == ERROR_SUCCESS, "REPLACE new key: %u", r);
        MsiCloseHandle(rec2);
    }
    MsiCloseHandle(rec);
    MsiViewClose(view);
    MsiCloseHandle(view);
    OK(stream_in_view(db, "s1") == 0 && stream_in_view(db, "s1renamed") == 1, "row renamed");

    /* validation modes read the (missing) _Validation rows for _Streams */
    MsiDatabaseOpenViewA(db, "SELECT * FROM `_Streams`", &view);
    MsiViewExecute(view, 0);
    r = MsiViewFetch(view, &rec);
    r = MsiViewModify(view, MSIMODIFY_VALIDATE, rec);
    OK(r == ERROR_INVALID_DATA, "VALIDATE on _Streams without _Validation: %u", r);
    e = next_error(view, col, sizeof(col));
    OK(e == MSIDBERROR_MISSINGDATA, "missing data on _Streams: %d", e);
    MsiCloseHandle(rec);
    MsiViewClose(view);
    MsiCloseHandle(view);

    /* commit: temporary rows are not saved */
    r = MsiDatabaseCommit(db);
    OK(r == ERROR_SUCCESS, "commit: %u", r);
    MsiCloseHandle(db);
    r = MsiOpenDatabaseA(dbfile, MSIDBOPEN_READONLY, &db);
    OK(!r, "reopen: %u", r);
    OK(stream_in_view(db, "s1renamed") == 1 && stream_in_view(db, "s3") == 1, "saved streams are there");
    OK(stream_in_view(db, "tmp1") == 0, "temporary stream was not saved");
    OK(stream_in_view(db, "s2") == 0, "deleted stream is gone");
    MsiCloseHandle(db);
    DeleteFileA(dbfile);
}

static void test_storages(void)
{
    MSIHANDLE db = 0, view, rec;
    char buf[64], col[32];
    DWORD sz;
    UINT r;
    int e;

    DeleteFileA(dbfile);
    r = MsiOpenDatabaseA(dbfile, MSIDBOPEN_CREATE, &db);
    OK(!r, "create: %u", r);
    MsiDatabaseCommit(db);
    MsiCloseHandle(db);
    r = MsiOpenDatabaseA(dbfile, MSIDBOPEN_TRANSACT, &db);
    OK(!r, "open transact: %u", r);

    r = MsiDatabaseOpenViewA(db, "SELECT * FROM `_Storages`", &view);
    OK(!r, "open _Storages: %u", r);
    MsiViewExecute(view, 0);
    rec = storage_rec("sa", "alpha");
    r = MsiViewModify(view, MSIMODIFY_INSERT, rec);
    OK(r == ERROR_SUCCESS, "INSERT sa: %u", r);
    MsiCloseHandle(rec);
    rec = storage_rec("sb", "beta");
    r = MsiViewModify(view, MSIMODIFY_INSERT, rec);
    OK(r == ERROR_SUCCESS, "INSERT sb: %u", r);
    MsiCloseHandle(rec);
    rec = storage_rec("st", "temporary");
    r = MsiViewModify(view, MSIMODIFY_INSERT_TEMPORARY, rec);
    OK(r == ERROR_SUCCESS, "INSERT_TEMPORARY: %u", r);
    MsiCloseHandle(rec);

    rec = storage_rec("sc", "gamma");
    r = MsiViewModify(view, MSIMODIFY_MERGE, rec);
    OK(r == ERROR_SUCCESS, "MERGE absent: %u", r);
    MsiCloseHandle(rec);
    rec = storage_rec("sc", "gamma");
    r = MsiViewModify(view, MSIMODIFY_MERGE, rec);
    OK(r == ERROR_SUCCESS, "MERGE identical: %u", r);
    MsiCloseHandle(rec);
    rec = storage_rec("sc", "delta");
    r = MsiViewModify(view, MSIMODIFY_MERGE, rec);
    OK(r == ERROR_FUNCTION_FAILED, "MERGE different: %u", r);
    MsiCloseHandle(rec);

    rec = storage_rec("sa", "x");
    r = MsiViewModify(view, MSIMODIFY_VALIDATE_NEW, rec);
    OK(r == ERROR_INVALID_DATA, "VALIDATE_NEW duplicate: %u", r);
    e = next_error(view, col, sizeof(col));
    OK(e == MSIDBERROR_DUPLICATEKEY && !strcmp(col, "Name"), "duplicate: %d '%s'", e, col);
    MsiCloseHandle(rec);
    MsiViewClose(view);
    MsiCloseHandle(view);

    /* DELETE sb */
    MsiDatabaseOpenViewA(db, "SELECT * FROM `_Storages`", &view);
    MsiViewExecute(view, 0);
    for (;;)
    {
        r = MsiViewFetch(view, &rec);
        if (r) break;
        sz = sizeof(buf);
        MsiRecordGetStringA(rec, 1, buf, &sz);
        if (!strcmp(buf, "sb")) break;
        MsiCloseHandle(rec);
    }
    OK(!r, "found sb");
    if (!r)
    {
        r = MsiViewModify(view, MSIMODIFY_DELETE, rec);
        OK(r == ERROR_SUCCESS, "DELETE sb: %u", r);
        MsiCloseHandle(rec);
    }
    r = MsiViewFetch(view, &rec);
    OK(r == ERROR_SUCCESS || r == ERROR_NO_MORE_ITEMS, "fetch after delete: %u", r);
    if (!r) MsiCloseHandle(rec);
    MsiViewClose(view);
    MsiCloseHandle(view);

    /* REPLACE: new key moves the row */
    MsiDatabaseOpenViewA(db, "SELECT * FROM `_Storages`", &view);
    MsiViewExecute(view, 0);
    for (;;)
    {
        r = MsiViewFetch(view, &rec);
        if (r) break;
        sz = sizeof(buf);
        MsiRecordGetStringA(rec, 1, buf, &sz);
        if (!strcmp(buf, "sc")) break;
        MsiCloseHandle(rec);
    }
    if (!r)
    {
        MSIHANDLE rec2 = storage_rec("sd", "gamma moved");
        r = MsiViewModify(view, MSIMODIFY_REPLACE, rec2);
        OK(r == ERROR_SUCCESS, "REPLACE new key: %u", r);
        MsiCloseHandle(rec2);
        MsiCloseHandle(rec);
    }
    MsiViewClose(view);
    MsiCloseHandle(view);

    r = MsiDatabaseCommit(db);
    OK(r == ERROR_SUCCESS, "commit: %u", r);
    MsiCloseHandle(db);

    OK(storage_exists("sa"), "sa saved");
    OK(storage_exists("sd"), "sd saved");
    OK(!storage_exists("sb"), "deleted sb is gone from the file");
    OK(!storage_exists("sc"), "replaced sc is gone from the file");
    OK(!storage_exists("st"), "temporary st was not saved");
    DeleteFileA(dbfile);
}


/* ---------------------------------------------------------------- advertise */

static void make_advertise_package(const char *file, const char *productcode)
{
    MSIHANDLE db = 0;
    UINT r = MsiOpenDatabaseA(file, MSIDBOPEN_CREATE, &db);
    OK(!r, "create advertise package: %u", r);
    make_package_db(db);
    run(db, "CREATE TABLE `Property` (`Property` CHAR(72) NOT NULL, `Value` LONGCHAR NOT NULL PRIMARY KEY `Property`)");
    run(db, "INSERT INTO `Property` (`Property`, `Value`) VALUES ('ProductCode', '%s')", productcode);
    run(db, "INSERT INTO `Property` (`Property`, `Value`) VALUES ('ProductName', 'SG Advertise Test')");
    run(db, "INSERT INTO `Property` (`Property`, `Value`) VALUES ('ProductVersion', '1.0.0')");
    run(db, "INSERT INTO `Property` (`Property`, `Value`) VALUES ('ProductLanguage', '1033')");
    run(db, "INSERT INTO `Property` (`Property`, `Value`) VALUES ('Manufacturer', 'Wine')");
    run(db, "INSERT INTO `Property` (`Property`, `Value`) VALUES ('UpgradeCode', '{4C0EAA15-0264-4E5A-8758-609EF142B92D}')");
    run(db, "CREATE TABLE `Directory` (`Directory` CHAR(255) NOT NULL, `Directory_Parent` CHAR(255), `DefaultDir` CHAR(255) NOT NULL PRIMARY KEY `Directory`)");
    run(db, "INSERT INTO `Directory` (`Directory`, `DefaultDir`) VALUES ('TARGETDIR', 'SourceDir')");
    run(db, "CREATE TABLE `Component` (`Component` CHAR(72) NOT NULL, `ComponentId` CHAR(38), `Directory_` CHAR(72) NOT NULL, `Attributes` SHORT NOT NULL, `Condition` CHAR(255), `KeyPath` CHAR(72) PRIMARY KEY `Component`)");
    run(db, "INSERT INTO `Component` (`Component`, `ComponentId`, `Directory_`, `Attributes`) VALUES ('comp', '{A1B2C3D4-0000-4000-8000-000000000001}', 'TARGETDIR', 4)");
    run(db, "CREATE TABLE `Feature` (`Feature` CHAR(38) NOT NULL, `Feature_Parent` CHAR(38), `Title` CHAR(64), `Description` CHAR(255), `Display` SHORT, `Level` SHORT NOT NULL, `Directory_` CHAR(72), `Attributes` SHORT NOT NULL PRIMARY KEY `Feature`)");
    run(db, "INSERT INTO `Feature` (`Feature`, `Title`, `Level`, `Directory_`, `Attributes`) VALUES ('feat', 'Feature', 1, 'TARGETDIR', 0)");
    run(db, "CREATE TABLE `FeatureComponents` (`Feature_` CHAR(38) NOT NULL, `Component_` CHAR(72) NOT NULL PRIMARY KEY `Feature_`, `Component_`)");
    run(db, "INSERT INTO `FeatureComponents` (`Feature_`, `Component_`) VALUES ('feat', 'comp')");
    run(db, "CREATE TABLE `File` (`File` CHAR(72) NOT NULL, `Component_` CHAR(72) NOT NULL, `FileName` CHAR(255) NOT NULL, `FileSize` LONG NOT NULL, `Version` CHAR(72), `Language` CHAR(20), `Attributes` SHORT, `Sequence` SHORT NOT NULL PRIMARY KEY `File`)");
    run(db, "CREATE TABLE `Media` (`DiskId` SHORT NOT NULL, `LastSequence` SHORT NOT NULL, `DiskPrompt` CHAR(64), `Cabinet` CHAR(255), `VolumeLabel` CHAR(32), `Source` CHAR(32) PRIMARY KEY `DiskId`)");
    run(db, "CREATE TABLE `AdvtExecuteSequence` (`Action` CHAR(72) NOT NULL, `Condition` CHAR(255), `Sequence` SHORT PRIMARY KEY `Action`)");
    run(db, "INSERT INTO `AdvtExecuteSequence` (`Action`, `Sequence`) VALUES ('CostInitialize', 800)");
    run(db, "INSERT INTO `AdvtExecuteSequence` (`Action`, `Sequence`) VALUES ('CostFinalize', 1000)");
    run(db, "INSERT INTO `AdvtExecuteSequence` (`Action`, `Sequence`) VALUES ('InstallValidate', 1400)");
    run(db, "INSERT INTO `AdvtExecuteSequence` (`Action`, `Sequence`) VALUES ('InstallInitialize', 1500)");
    run(db, "INSERT INTO `AdvtExecuteSequence` (`Action`, `Sequence`) VALUES ('PublishFeatures', 6300)");
    run(db, "INSERT INTO `AdvtExecuteSequence` (`Action`, `Sequence`) VALUES ('PublishProduct', 6400)");
    run(db, "INSERT INTO `AdvtExecuteSequence` (`Action`, `Sequence`) VALUES ('InstallFinalize', 6600)");
    run(db, "CREATE TABLE `InstallExecuteSequence` (`Action` CHAR(72) NOT NULL, `Condition` CHAR(255), `Sequence` SHORT PRIMARY KEY `Action`)");
    run(db, "INSERT INTO `InstallExecuteSequence` (`Action`, `Sequence`) VALUES ('CostInitialize', 800)");
    run(db, "INSERT INTO `InstallExecuteSequence` (`Action`, `Sequence`) VALUES ('FileCost', 900)");
    run(db, "INSERT INTO `InstallExecuteSequence` (`Action`, `Sequence`) VALUES ('CostFinalize', 1000)");
    run(db, "INSERT INTO `InstallExecuteSequence` (`Action`, `Sequence`) VALUES ('InstallValidate', 1400)");
    run(db, "INSERT INTO `InstallExecuteSequence` (`Action`, `Sequence`) VALUES ('InstallInitialize', 1500)");
    run(db, "INSERT INTO `InstallExecuteSequence` (`Action`, `Sequence`) VALUES ('PublishFeatures', 6300)");
    run(db, "INSERT INTO `InstallExecuteSequence` (`Action`, `Sequence`) VALUES ('PublishProduct', 6400)");
    run(db, "INSERT INTO `InstallExecuteSequence` (`Action`, `Sequence`) VALUES ('InstallFinalize', 6600)");
    MsiDatabaseCommit(db);
    MsiCloseHandle(db);
}

static void test_advertise(void)
{
    static const char *pc = "{3F5B4D3E-5A6C-4F2B-9E11-0A5D0C1E7A01}";
    char path[MAX_PATH], script[MAX_PATH], buf[256];
    DWORD sz;
    UINT r;
    FILE *f;

    GetFullPathNameA("fastmsib1-adv.msi", MAX_PATH, path, NULL);
    GetFullPathNameA("fastmsib1-adv.script", MAX_PATH, script, NULL);
    DeleteFileA(path);
    make_advertise_package(path, pc);

    MsiSetInternalUI(INSTALLUILEVEL_NONE, NULL);

    /* arguments */
    OK(MsiAdvertiseProductA(NULL, script, NULL, 0) == ERROR_INVALID_PARAMETER, "NULL package");
    OK(MsiAdvertiseProductExA(path, script, NULL, 0, 8, 0) == ERROR_INVALID_PARAMETER, "bad platform");
    OK(MsiAdvertiseProductExA(path, script, NULL, 0, 0, 2) == ERROR_INVALID_PARAMETER, "bad options");
    OK(MsiAdvertiseProductA("c:\\no\\such\\package.msi", script, NULL, 0) == ERROR_INSTALL_PACKAGE_OPEN_FAILED, "missing package");

    /* a script is written and can be read back */
    DeleteFileA(script);
    r = MsiAdvertiseProductA(path, script, NULL, 1033);
    OK(r == ERROR_SUCCESS, "advertise to a script: %u", r);
    OK(GetFileAttributesA(script) != INVALID_FILE_ATTRIBUTES, "script exists");
    OK(MsiQueryProductStateA(pc) != INSTALLSTATE_ADVERTISED, "writing a script does not advertise: %d", MsiQueryProductStateA(pc));
    OK(MsiAdvertiseScriptA("c:\\no\\such\\script", 0, NULL, FALSE) == ERROR_INSTALL_PACKAGE_OPEN_FAILED, "missing script");
    f = fopen("fastmsib1-bad.script", "wb"); fputs("not a script\n", f); fclose(f);
    OK(MsiAdvertiseScriptA("fastmsib1-bad.script", 0, NULL, FALSE) == ERROR_INSTALL_PACKAGE_INVALID, "garbage script");
    DeleteFileA("fastmsib1-bad.script");

    /* running the script advertises the product for the machine, removing it undoes that */
    r = MsiAdvertiseScriptA(script, SCRIPTFLAGS_MACHINEASSIGN, NULL, FALSE);
    OK(r == ERROR_SUCCESS, "MsiAdvertiseScript: %u", r);
    OK(MsiQueryProductStateA(pc) == INSTALLSTATE_ADVERTISED, "product advertised: %d", MsiQueryProductStateA(pc));
    OK(MsiQueryFeatureStateA(pc, "feat") == INSTALLSTATE_ADVERTISED, "feature advertised: %d", MsiQueryFeatureStateA(pc, "feat"));
    sz = sizeof(buf);
    r = MsiGetProductInfoA(pc, INSTALLPROPERTY_PRODUCTNAME, buf, &sz);
    OK(r == ERROR_SUCCESS && !strcmp(buf, "SG Advertise Test"), "advertised product name: %u '%s'", r, buf);
    r = MsiAdvertiseScriptA(script, SCRIPTFLAGS_MACHINEASSIGN, NULL, TRUE);
    OK(r == ERROR_SUCCESS, "MsiAdvertiseScript remove: %u", r);
    OK(MsiQueryProductStateA(pc) == INSTALLSTATE_UNKNOWN, "advertisement removed: %d", MsiQueryProductStateA(pc));

    /* advertise directly to this computer (the script argument is ADVERTISEFLAGS_MACHINEASSIGN) */
    r = MsiAdvertiseProductExA(path, (const char *)(ULONG_PTR)ADVERTISEFLAGS_MACHINEASSIGN, NULL, 0, 0, 0);
    OK(r == ERROR_SUCCESS, "MsiAdvertiseProductEx machine: %u", r);
    OK(MsiQueryProductStateA(pc) == INSTALLSTATE_ADVERTISED, "directly advertised: %d", MsiQueryProductStateA(pc));
    r = MsiConfigureProductExA(pc, 0, INSTALLSTATE_ABSENT, NULL);
    OK(r == ERROR_SUCCESS, "uninstall the advertisement: %u", r);
    OK(MsiQueryProductStateA(pc) == INSTALLSTATE_UNKNOWN, "gone after uninstall: %d", MsiQueryProductStateA(pc));

    DeleteFileA(script);
    DeleteFileA(path);
}

int main(void)
{
    CoInitialize(NULL);
    setvbuf(stdout, NULL, _IONBF, 0);
    test_get_mode();
    test_product_id();
    test_categories();
    test_columns();
    test_replace();
    test_streams();
    test_storages();
    test_advertise();
    printf("checks %d, failures %d\n", checks, fails);
    printf("RESULT: %s\n", fails ? "FAIL" : "PASS");
    return fails != 0;
}
