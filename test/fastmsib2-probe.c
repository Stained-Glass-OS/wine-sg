/* msi.dll gap-A batch 2 probe (patch 3217): MsiGetLastErrorRecord (per-process error record, codes and
 * messages from Windows Installer Error Messages), MsiPreviewBillboard and the Billboard control (Billboard /
 * BBControl tables), MsiRemovePatches (reinstall without the patch, own patch package built here). */
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

static UINT run(MSIHANDLE db, const char *fmt, ...)
{
    char buf[2048];
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

static void run_ok(MSIHANDLE db, const char *q)
{
    UINT r = run(db, "%s", q);
    OK(r == ERROR_SUCCESS, "setup query failed (%u): %.70s", r, q);
}

/* ---------------------------------------------------------------- MsiGetLastErrorRecord */

static char *rec_str(MSIHANDLE rec, UINT field, char *buf, DWORD size)
{
    buf[0] = 0;
    MsiRecordGetStringA(rec, field, buf, &size);
    return buf;
}

/* expects the last error record: code, field 2 (the database) and fields 3 / 4 (NULL = not checked) */
static void expect_error(const char *what, UINT code, const char *db, const char *f3, const char *f4)
{
    MSIHANDLE rec = MsiGetLastErrorRecord();
    char buf[1024], text[2048];
    DWORD sz;

    OK(rec != 0, "[%s] no error record", what);
    if (!rec) return;
    OK(MsiRecordGetInteger(rec, 1) == (int)code, "[%s] error code %d, want %u", what, MsiRecordGetInteger(rec, 1), code);
    if (db) OK(!strcmp(rec_str(rec, 2, buf, sizeof(buf)), db), "[%s] database '%s', want '%s'", what, buf, db);
    if (f3) OK(!strcmp(rec_str(rec, 3, buf, sizeof(buf)), f3), "[%s] field 3 '%s', want '%s'", what, buf, f3);
    if (f4) OK(!strcmp(rec_str(rec, 4, buf, sizeof(buf)), f4), "[%s] field 4 '%s', want '%s'", what, buf, f4);
    /* the documented use: format the record with its own format string */
    sz = 0;
    OK(MsiFormatRecordA(0, rec, "", &sz) == ERROR_MORE_DATA, "[%s] format size", what);
    sz = sizeof(text);
    OK(MsiFormatRecordA(0, rec, text, &sz) == ERROR_SUCCESS && strlen(text) > 10 && !strstr(text, "[2]") && !strstr(text, "[3]"),
       "[%s] formatted message '%s'", what, text);
    MsiCloseHandle(rec);
    OK(MsiGetLastErrorRecord() == 0, "[%s] the record is released after it was returned", what);
}

static void expect_no_error(const char *what)
{
    MSIHANDLE rec = MsiGetLastErrorRecord();
    OK(rec == 0, "[%s] unexpected error record %d", what, rec ? MsiRecordGetInteger(rec, 1) : 0);
    if (rec) MsiCloseHandle(rec);
}

static void test_last_error(void)
{
    static const char *dbfile = "fastmsib2-err.msi";
    char path[MAX_PATH], fullpath[MAX_PATH];
    MSIHANDLE db = 0, db2 = 0, view = 0, rec;
    UINT r;
    FILE *f;

    GetFullPathNameA(dbfile, MAX_PATH, fullpath, NULL);

    expect_no_error("fresh process");

    /* MsiOpenDatabase: success resets, a missing file gives 2203 with the system error */
    r = MsiOpenDatabaseA("c:\\no\\such\\dir\\x.msi", MSIDBOPEN_READONLY, &db);
    OK(r != ERROR_SUCCESS, "open of a missing file: %u", r);
    expect_error("open missing", 2203, "c:\\no\\such\\dir\\x.msi", "3", NULL);
    r = MsiOpenDatabaseA("fastmsib2-missing.msi", MSIDBOPEN_READONLY, &db);
    OK(r != ERROR_SUCCESS, "open of a missing file in the cwd: %u", r);
    {
        MSIHANDLE e = MsiGetLastErrorRecord();
        char buf[512];
        OK(e && MsiRecordGetInteger(e, 1) == 2203, "cwd open: code");
        OK(e && MsiRecordGetInteger(e, 3) == 2, "cwd open: system error %d (file not found)", e ? MsiRecordGetInteger(e, 3) : -1);
        if (e) MsiCloseHandle(e);
        (void)buf;
    }
    f = fopen("fastmsib2-notmsi.msi", "wb"); fputs("this is not a Windows Installer database", f); fclose(f);
    r = MsiOpenDatabaseA("fastmsib2-notmsi.msi", MSIDBOPEN_READONLY, &db);
    OK(r != ERROR_SUCCESS, "open of a text file: %u", r);
    {
        MSIHANDLE e = MsiGetLastErrorRecord();
        OK(e && (MsiRecordGetInteger(e, 1) == 2203 || MsiRecordGetInteger(e, 1) == 2219), "text file: code %d", e ? MsiRecordGetInteger(e, 1) : 0);
        if (e) MsiCloseHandle(e);
    }
    DeleteFileA("fastmsib2-notmsi.msi");

    DeleteFileA(dbfile);
    r = MsiOpenDatabaseA(dbfile, MSIDBOPEN_CREATE, &db);
    OK(r == ERROR_SUCCESS, "create: %u", r);
    expect_no_error("create succeeded");
    run_ok(db, "CREATE TABLE `T` (`K` CHAR(72) NOT NULL, `V` LONG PRIMARY KEY `K`)");
    run_ok(db, "INSERT INTO `T` (`K`, `V`) VALUES ('a', 1)");
    expect_no_error("setup queries succeeded");

    /* MsiDatabaseOpenView: unknown table, unknown column, syntax error */
    r = MsiDatabaseOpenViewA(db, "SELECT * FROM `NoSuchTable`", &view);
    OK(r == ERROR_BAD_QUERY_SYNTAX, "unknown table: %u", r);
    expect_error("unknown table", 2228, fullpath, "NoSuchTable", "SELECT * FROM `NoSuchTable`");
    r = MsiDatabaseOpenViewA(db, "SELECT `Nope` FROM `T`", &view);
    OK(r == ERROR_BAD_QUERY_SYNTAX, "unknown column: %u", r);
    expect_error("unknown column", 2235, fullpath, "Nope", "SELECT `Nope` FROM `T`");
    r = MsiDatabaseOpenViewA(db, "SELECT FROM WHERE", &view);
    OK(r == ERROR_BAD_QUERY_SYNTAX, "syntax error: %u", r);
    expect_error("syntax error", 2232, fullpath, NULL, "SELECT FROM WHERE");

    /* the next listed function that succeeds resets the record */
    r = MsiDatabaseOpenViewA(db, "SELECT * FROM `NoSuchTable`", &view);
    OK(r != ERROR_SUCCESS, "unknown table again");
    r = MsiDatabaseOpenViewA(db, "SELECT * FROM `T`", &view);
    OK(r == ERROR_SUCCESS, "good view: %u", r);
    expect_no_error("successful OpenView resets");

    r = MsiDatabaseOpenViewA(db, "SELECT * FROM `NoSuchTable`", &db2);
    OK(r != ERROR_SUCCESS, "error before Execute");
    r = MsiViewExecute(view, 0);
    OK(r == ERROR_SUCCESS, "execute: %u", r);
    expect_no_error("successful Execute resets");
    MsiViewClose(view);
    MsiCloseHandle(view);

    /* CREATE TABLE of an existing table */
    r = run(db, "CREATE TABLE `T` (`K` CHAR(72) NOT NULL, `V` LONG PRIMARY KEY `K`)");
    OK(r == ERROR_BAD_QUERY_SYNTAX, "create existing: %u", r);
    {
        MSIHANDLE e = MsiGetLastErrorRecord();
        OK(e && MsiRecordGetInteger(e, 1) == 2204, "table exists: code %d", e ? MsiRecordGetInteger(e, 1) : 0);
        {
            char buf[64];
            OK(e && !strcmp(rec_str(e, 3, buf, sizeof(buf)), "T"), "table exists: field 3 '%s'", buf);
        }
        if (e) MsiCloseHandle(e);
    }

    /* Export / Import */
    r = MsiDatabaseExportA(db, "T", "c:\\no\\such\\dir", "t.idt");
    OK(r != ERROR_SUCCESS, "export to a missing folder: %u", r);
    {
        MSIHANDLE e = MsiGetLastErrorRecord();
        OK(e && MsiRecordGetInteger(e, 1) == 2214, "export: code %d", e ? MsiRecordGetInteger(e, 1) : 0);
        if (e) MsiCloseHandle(e);
    }
    r = MsiDatabaseImportA(db, ".", "fastmsib2-missing.idt");
    OK(r != ERROR_SUCCESS, "import of a missing file: %u", r);
    expect_error("import missing", 2215, fullpath, NULL, NULL);
    r = MsiDatabaseExportA(db, "T", ".", "fastmsib2-t.idt");
    OK(r == ERROR_SUCCESS, "export: %u", r);
    expect_no_error("export succeeded");
    DeleteFileA("fastmsib2-t.idt");

    /* summary information and commit reset it too */
    run(db, "SELECT * FROM `NoSuchTable`");
    r = MsiGetSummaryInformationA(db, NULL, 1, &rec);
    OK(r == ERROR_SUCCESS, "summary information: %u", r);
    if (!r) MsiCloseHandle(rec);
    expect_no_error("MsiGetSummaryInformation resets");
    run(db, "SELECT * FROM `NoSuchTable`");
    r = MsiDatabaseCommit(db);
    OK(r == ERROR_SUCCESS, "commit: %u", r);
    expect_no_error("MsiDatabaseCommit resets");

    /* Merge with conflicts: 2268 with the number of tables */
    {
        static const char *basefile = "fastmsib2-base.msi", *mergefile = "fastmsib2-merge.msi";
        MSIHANDLE a = 0, b = 0;

        DeleteFileA(basefile); DeleteFileA(mergefile);
        MsiOpenDatabaseA(basefile, MSIDBOPEN_CREATE, &a);
        MsiOpenDatabaseA(mergefile, MSIDBOPEN_CREATE, &b);
        run_ok(a, "CREATE TABLE `T` (`K` CHAR(72) NOT NULL, `V` LONG PRIMARY KEY `K`)");
        run_ok(b, "CREATE TABLE `T` (`K` CHAR(72) NOT NULL, `V` LONG PRIMARY KEY `K`)");
        run_ok(a, "INSERT INTO `T` (`K`, `V`) VALUES ('x', 1)");
        run_ok(b, "INSERT INTO `T` (`K`, `V`) VALUES ('x', 2)");
        r = MsiDatabaseMergeA(a, b, "MergeErrors");
        OK(r == ERROR_FUNCTION_FAILED, "merge with a conflict: %u", r);
        {
            MSIHANDLE e = MsiGetLastErrorRecord();
            OK(e && MsiRecordGetInteger(e, 1) == 2268 && MsiRecordGetInteger(e, 3) == 1, "merge conflict record: %d %d",
               e ? MsiRecordGetInteger(e, 1) : 0, e ? MsiRecordGetInteger(e, 3) : 0);
            if (e) MsiCloseHandle(e);
        }
        MsiCloseHandle(a); MsiCloseHandle(b);
        DeleteFileA(basefile); DeleteFileA(mergefile);
    }

    /* ApplyTransform with something that is not a transform: 2246 */
    CopyFileA(dbfile, "fastmsib2-copy.msi", FALSE);
    r = MsiDatabaseApplyTransformA(db, "fastmsib2-copy.msi", 0);
    DeleteFileA("fastmsib2-copy.msi");
    OK(r != ERROR_SUCCESS, "database as transform: %u", r);
    {
        MSIHANDLE e = MsiGetLastErrorRecord();
        OK(e && MsiRecordGetInteger(e, 1) == 2246, "transform record: %d", e ? MsiRecordGetInteger(e, 1) : 0);
        if (e) MsiCloseHandle(e);
    }

    (void)path;
    MsiCloseHandle(db);
    DeleteFileA(dbfile);
}


/* ---------------------------------------------------------------- Billboard */

struct find_text { const char *text; int found; };

static BOOL CALLBACK find_child_text(HWND h, LPARAM l)
{
    struct find_text *f = (struct find_text *)l;
    char buf[128];

    if (GetWindowTextA(h, buf, sizeof(buf)) && !strcmp(buf, f->text)) f->found++;
    return TRUE;
}

static BOOL CALLBACK find_top_text(HWND h, LPARAM l)
{
    EnumChildWindows(h, find_child_text, l);
    return TRUE;
}

/* how many windows with this text exist anywhere on the desktop (children included) */
static int count_text(const char *text)
{
    struct find_text f = { text, 0 };

    EnumWindows(find_top_text, (LPARAM)&f);
    return f.found;
}

static void make_billboard_package(const char *file)
{
    MSIHANDLE db = 0, si;
    UINT r = MsiOpenDatabaseA(file, MSIDBOPEN_CREATE, &db);

    OK(!r, "create billboard package: %u", r);
    MsiGetSummaryInformationA(db, NULL, 7, &si);
    MsiSummaryInfoSetPropertyA(si, 2, VT_LPSTR, 0, NULL, "Installation Database");
    MsiSummaryInfoSetPropertyA(si, 3, VT_LPSTR, 0, NULL, "Billboard test");
    MsiSummaryInfoSetPropertyA(si, 4, VT_LPSTR, 0, NULL, "Wine Hackers");
    MsiSummaryInfoSetPropertyA(si, 7, VT_LPSTR, 0, NULL, ";1033");
    MsiSummaryInfoSetPropertyA(si, 9, VT_LPSTR, 0, NULL, "{913B8D18-FBB6-4CAC-A239-C74C11E3FA76}");
    MsiSummaryInfoSetPropertyA(si, 14, VT_I4, 100, NULL, NULL);
    MsiSummaryInfoSetPropertyA(si, 15, VT_I4, 0, NULL, NULL);
    MsiSummaryInfoPersist(si);
    MsiCloseHandle(si);
    run_ok(db, "CREATE TABLE `Property` (`Property` CHAR(72) NOT NULL, `Value` LONGCHAR NOT NULL PRIMARY KEY `Property`)");
    run_ok(db, "INSERT INTO `Property` (`Property`, `Value`) VALUES ('ProductCode', '{6A1C2B40-9C3D-4E55-8A00-3217000000B1}')");
    run_ok(db, "INSERT INTO `Property` (`Property`, `Value`) VALUES ('ProductName', 'SG Billboard Test')");
    run_ok(db, "INSERT INTO `Property` (`Property`, `Value`) VALUES ('ProductVersion', '1.0.0')");
    run_ok(db, "CREATE TABLE `Directory` (`Directory` CHAR(255) NOT NULL, `Directory_Parent` CHAR(255), `DefaultDir` CHAR(255) NOT NULL PRIMARY KEY `Directory`)");
    run_ok(db, "INSERT INTO `Directory` (`Directory`, `DefaultDir`) VALUES ('TARGETDIR', 'SourceDir')");
    run_ok(db, "CREATE TABLE `Component` (`Component` CHAR(72) NOT NULL, `ComponentId` CHAR(38), `Directory_` CHAR(72) NOT NULL, `Attributes` SHORT NOT NULL, `Condition` CHAR(255), `KeyPath` CHAR(72) PRIMARY KEY `Component`)");
    run_ok(db, "INSERT INTO `Component` (`Component`, `ComponentId`, `Directory_`, `Attributes`) VALUES ('comp', '{A1B2C3D4-0000-4000-8000-0000000000B1}', 'TARGETDIR', 4)");
    run_ok(db, "CREATE TABLE `Feature` (`Feature` CHAR(38) NOT NULL, `Feature_Parent` CHAR(38), `Title` CHAR(64), `Description` CHAR(255), `Display` SHORT, `Level` SHORT NOT NULL, `Directory_` CHAR(72), `Attributes` SHORT NOT NULL PRIMARY KEY `Feature`)");
    run_ok(db, "INSERT INTO `Feature` (`Feature`, `Title`, `Level`, `Directory_`, `Attributes`) VALUES ('on', 'Installed feature', 1, 'TARGETDIR', 0)");
    run_ok(db, "INSERT INTO `Feature` (`Feature`, `Title`, `Level`, `Directory_`, `Attributes`) VALUES ('off', 'Feature that is not installed', 100, 'TARGETDIR', 0)");
    run_ok(db, "CREATE TABLE `FeatureComponents` (`Feature_` CHAR(38) NOT NULL, `Component_` CHAR(72) NOT NULL PRIMARY KEY `Feature_`, `Component_`)");
    run_ok(db, "INSERT INTO `FeatureComponents` (`Feature_`, `Component_`) VALUES ('on', 'comp')");
    run_ok(db, "CREATE TABLE `Dialog` (`Dialog` CHAR(72) NOT NULL, `HCentering` SHORT NOT NULL, `VCentering` SHORT NOT NULL, `Width` SHORT NOT NULL, `Height` SHORT NOT NULL, `Attributes` LONG, `Title` CHAR(128) LOCALIZABLE, `Control_First` CHAR(50) NOT NULL, `Control_Default` CHAR(50), `Control_Cancel` CHAR(50) PRIMARY KEY `Dialog`)");
    run_ok(db, "INSERT INTO `Dialog` (`Dialog`, `HCentering`, `VCentering`, `Width`, `Height`, `Attributes`, `Title`, `Control_First`, `Control_Default`, `Control_Cancel`) VALUES ('Action', 50, 50, 300, 200, 5, 'Action', 'Board', 'Board', 'Board')");
    run_ok(db, "CREATE TABLE `Control` (`Dialog_` CHAR(72) NOT NULL, `Control` CHAR(50) NOT NULL, `Type` CHAR(20) NOT NULL, `X` SHORT NOT NULL, `Y` SHORT NOT NULL, `Width` SHORT NOT NULL, `Height` SHORT NOT NULL, `Attributes` LONG, `Property` CHAR(50), `Text` CHAR(0) LOCALIZABLE, `Control_Next` CHAR(50), `Help` CHAR(50) LOCALIZABLE PRIMARY KEY `Dialog_`, `Control`)");
    run_ok(db, "INSERT INTO `Control` (`Dialog_`, `Control`, `Type`, `X`, `Y`, `Width`, `Height`, `Attributes`, `Property`, `Text`, `Control_Next`, `Help`) VALUES ('Action', 'Board', 'Billboard', 10, 40, 280, 150, 1, NULL, NULL, 'Plain', NULL)");
    run_ok(db, "INSERT INTO `Control` (`Dialog_`, `Control`, `Type`, `X`, `Y`, `Width`, `Height`, `Attributes`, `Property`, `Text`, `Control_Next`, `Help`) VALUES ('Action', 'Plain', 'Text', 10, 10, 280, 20, 3, NULL, 'Plain text control', 'Board', NULL)");
    run_ok(db, "CREATE TABLE `Billboard` (`Billboard` CHAR(50) NOT NULL, `Feature_` CHAR(38) NOT NULL, `Action` CHAR(50), `Ordering` SHORT PRIMARY KEY `Billboard`)");
    run_ok(db, "INSERT INTO `Billboard` (`Billboard`, `Feature_`, `Action`, `Ordering`) VALUES ('BB1', 'on', 'InstallFiles', 1)");
    run_ok(db, "INSERT INTO `Billboard` (`Billboard`, `Feature_`, `Action`, `Ordering`) VALUES ('BB2', 'on', 'InstallFiles', 2)");
    run_ok(db, "INSERT INTO `Billboard` (`Billboard`, `Feature_`, `Action`, `Ordering`) VALUES ('BBoff', 'off', 'InstallFiles', 3)");
    run_ok(db, "INSERT INTO `Billboard` (`Billboard`, `Feature_`, `Action`, `Ordering`) VALUES ('BBreg', 'on', 'RegisterProduct', 1)");
    run_ok(db, "CREATE TABLE `BBControl` (`Billboard_` CHAR(50) NOT NULL, `BBControl` CHAR(50) NOT NULL, `Type` CHAR(50) NOT NULL, `X` SHORT NOT NULL, `Y` SHORT NOT NULL, `Width` SHORT NOT NULL, `Height` SHORT NOT NULL, `Attributes` LONG, `Text` CHAR(50) LOCALIZABLE PRIMARY KEY `Billboard_`, `BBControl`)");
    run_ok(db, "INSERT INTO `BBControl` (`Billboard_`, `BBControl`, `Type`, `X`, `Y`, `Width`, `Height`, `Attributes`, `Text`) VALUES ('BB1', 'Title', 'Text', 5, 5, 200, 20, 3, 'First Billboard')");
    run_ok(db, "INSERT INTO `BBControl` (`Billboard_`, `BBControl`, `Type`, `X`, `Y`, `Width`, `Height`, `Attributes`, `Text`) VALUES ('BB1', 'Sub', 'Text', 5, 30, 200, 20, 3, 'First extra line')");
    run_ok(db, "INSERT INTO `BBControl` (`Billboard_`, `BBControl`, `Type`, `X`, `Y`, `Width`, `Height`, `Attributes`, `Text`) VALUES ('BB2', 'Title', 'Text', 5, 5, 200, 20, 3, 'Second Billboard')");
    run_ok(db, "INSERT INTO `BBControl` (`Billboard_`, `BBControl`, `Type`, `X`, `Y`, `Width`, `Height`, `Attributes`, `Text`) VALUES ('BBoff', 'Title', 'Text', 5, 5, 200, 20, 3, 'Billboard of a missing feature')");
    run_ok(db, "INSERT INTO `BBControl` (`Billboard_`, `BBControl`, `Type`, `X`, `Y`, `Width`, `Height`, `Attributes`, `Text`) VALUES ('BBreg', 'Title', 'Text', 5, 5, 200, 20, 3, 'Registering')");
    run_ok(db, "INSERT INTO `BBControl` (`Billboard_`, `BBControl`, `Type`, `X`, `Y`, `Width`, `Height`, `Attributes`, `Text`) VALUES ('BB1', 'Pushy', 'PushButton', 5, 60, 40, 20, 3, 'No buttons on billboards')");
    run_ok(db, "CREATE TABLE `EventMapping` (`Dialog_` CHAR(72) NOT NULL, `Control_` CHAR(50) NOT NULL, `Event` CHAR(50) NOT NULL, `Attribute` CHAR(50) NOT NULL PRIMARY KEY `Dialog_`, `Control_`, `Event`)");
    run_ok(db, "INSERT INTO `EventMapping` (`Dialog_`, `Control_`, `Event`, `Attribute`) VALUES ('Action', 'Board', 'SetProgress', 'Progress')");
    MsiDatabaseCommit(db);
    MsiCloseHandle(db);
}

static void show_progress(MSIHANDLE pkg, const char *action)
{
    MSIHANDLE rec = MsiCreateRecord(3);

    MsiRecordSetStringA(rec, 1, action);
    MsiRecordSetStringA(rec, 2, "Doing it");
    MsiRecordSetStringA(rec, 3, "[1]");
    MsiProcessMessage(pkg, INSTALLMESSAGE_ACTIONSTART, rec);
    MsiCloseHandle(rec);
}

static void tick(MSIHANDLE pkg)
{
    MSIHANDLE rec = MsiCreateRecord(3);

    MsiRecordSetInteger(rec, 1, 2);       /* progress report: move the bar */
    MsiRecordSetInteger(rec, 2, 512);
    MsiRecordSetInteger(rec, 3, 0);
    MsiProcessMessage(pkg, INSTALLMESSAGE_PROGRESS, rec);
    MsiCloseHandle(rec);
}

static void test_billboard(void)
{
    static const char *file = "fastmsib2-bb.msi";
    char path[MAX_PATH], name[32];
    MSIHANDLE db = 0, preview = 0, pkg = 0, rec;
    UINT r;

    GetFullPathNameA(file, MAX_PATH, path, NULL);
    DeleteFileA(path);
    make_billboard_package(path);

    /* ---- MsiPreviewBillboard on a previewed dialog */
    r = MsiOpenDatabaseA(path, MSIDBOPEN_READONLY, &db);
    OK(!r, "open: %u", r);
    r = MsiEnableUIPreview(db, &preview);
    OK(!r && preview, "enable preview: %u", r);

    OK(MsiPreviewBillboardA(0, "Board", "BB1") == ERROR_INVALID_HANDLE, "invalid handle");
    OK(MsiPreviewBillboardA(preview, "Board", "BB1") == ERROR_FUNCTION_FAILED, "no dialog yet");
    r = MsiPreviewDialogA(preview, "Action");
    OK(!r, "preview the dialog: %u", r);
    OK(count_text("Plain text control") == 1, "the previewed dialog is up");
    OK(count_text("First Billboard") == 0, "no billboard yet");

    OK(MsiPreviewBillboardA(preview, "Board", "BB1") == ERROR_SUCCESS, "BB1");
    OK(count_text("First Billboard") == 1 && count_text("First extra line") == 1, "billboard 1 shows its text controls: %d %d",
       count_text("First Billboard"), count_text("First extra line"));
    OK(count_text("No buttons on billboards") == 0, "a push button is not a billboard control");
    OK(MsiPreviewBillboardA(preview, "Board", "BB2") == ERROR_SUCCESS, "BB2");
    OK(count_text("Second Billboard") == 1 && count_text("First Billboard") == 0 && count_text("First extra line") == 0,
       "billboard 2 replaced billboard 1: %d %d", count_text("Second Billboard"), count_text("First Billboard"));
    OK(MsiPreviewBillboardA(preview, "Board", "NoSuchBillboard") == ERROR_FUNCTION_FAILED, "unknown billboard");
    OK(count_text("Second Billboard") == 1, "a failed call keeps the current billboard");
    OK(MsiPreviewBillboardA(preview, "NoSuchControl", "BB1") == ERROR_FUNCTION_FAILED, "unknown control");
    OK(MsiPreviewBillboardA(preview, "Plain", "BB1") == ERROR_FUNCTION_FAILED, "a text control hosts no billboard");
    OK(MsiPreviewBillboardA(preview, NULL, "BB1") == ERROR_FUNCTION_FAILED, "no control name");
    OK(MsiPreviewBillboardA(preview, "Board", NULL) == ERROR_SUCCESS, "a NULL billboard removes the current one");
    OK(count_text("Second Billboard") == 0, "removed");
    OK(MsiPreviewBillboardA(preview, "Board", "BB1") == ERROR_SUCCESS, "BB1 again");
    OK(count_text("First Billboard") == 1, "shown again");
    MsiPreviewDialogA(preview, NULL);
    OK(count_text("First Billboard") == 0 && count_text("Plain text control") == 0, "the preview dialog is gone");
    MsiCloseHandle(preview);
    MsiCloseHandle(db);

    /* ---- the billboard of the running action, chosen by progress messages */
    MsiSetInternalUI(INSTALLUILEVEL_FULL, NULL);
    r = MsiOpenPackageA(path, &pkg);
    OK(!r, "open package: %u", r);
    if (pkg)
    {
        OK(MsiDoActionA(pkg, "CostInitialize") == ERROR_FUNCTION_FAILED || 1, "cost");
        MsiDoActionA(pkg, "FileCost");
        MsiDoActionA(pkg, "CostFinalize");
        rec = MsiCreateRecord(1);
        MsiRecordSetStringA(rec, 0, "Action");
        r = MsiProcessMessage(pkg, INSTALLMESSAGE_SHOWDIALOG, rec);   /* modeless: returns at once */
        MsiCloseHandle(rec);
        OK(count_text("Plain text control") == 1, "the modeless dialog is up (%d)", (int)r);

        show_progress(pkg, "InstallFiles");
        tick(pkg);
        OK(count_text("First Billboard") == 1, "InstallFiles: the first billboard of an installed feature");
        OK(count_text("Billboard of a missing feature") == 0, "a feature that is not installed gets no billboard");
        tick(pkg);
        OK(count_text("First Billboard") == 1 && count_text("Second Billboard") == 0, "ticks within a second keep the billboard");
        Sleep(1100);
        tick(pkg);
        OK(count_text("Second Billboard") == 1 && count_text("First Billboard") == 0, "the next billboard of the action follows");
        Sleep(1100);
        tick(pkg);
        OK(count_text("First Billboard") == 1, "and the first one comes round again");
        show_progress(pkg, "RegisterProduct");
        tick(pkg);
        OK(count_text("Registering") == 1 && count_text("First Billboard") == 0, "another action, another billboard");
        show_progress(pkg, "RemoveFiles");
        tick(pkg);
        OK(count_text("Registering") == 0, "an action without billboards shows none");
        MsiSetInternalUI(INSTALLUILEVEL_NONE, NULL);
        MsiCloseHandle(pkg);
    }
    (void)name;
    MsiSetInternalUI(INSTALLUILEVEL_NONE, NULL);
    DeleteFileA(path);
}


/* ---------------------------------------------------------------- MsiRemovePatches */

static const char *rp_pc = "{6A1C2B40-9C3D-4E55-8A00-3217000000C1}";
static const char *rp_patch1 = "{6A1C2B40-9C3D-4E55-8A00-3217000000D1}";
static const char *rp_patch2 = "{6A1C2B40-9C3D-4E55-8A00-3217000000D2}";

static void make_product(const char *file)
{
    MSIHANDLE db = 0, si;
    UINT r = MsiOpenDatabaseA(file, MSIDBOPEN_CREATE, &db);

    OK(!r, "create product: %u", r);
    MsiGetSummaryInformationA(db, NULL, 7, &si);
    MsiSummaryInfoSetPropertyA(si, 2, VT_LPSTR, 0, NULL, "Installation Database");
    MsiSummaryInfoSetPropertyA(si, 3, VT_LPSTR, 0, NULL, "Patch removal test");
    MsiSummaryInfoSetPropertyA(si, 4, VT_LPSTR, 0, NULL, "Wine Hackers");
    MsiSummaryInfoSetPropertyA(si, 7, VT_LPSTR, 0, NULL, ";1033");
    MsiSummaryInfoSetPropertyA(si, 9, VT_LPSTR, 0, NULL, "{913B8D18-FBB6-4CAC-A239-C74C11E3FA77}");
    MsiSummaryInfoSetPropertyA(si, 14, VT_I4, 100, NULL, NULL);
    MsiSummaryInfoSetPropertyA(si, 15, VT_I4, 0, NULL, NULL);
    MsiSummaryInfoPersist(si);
    MsiCloseHandle(si);
    run_ok(db, "CREATE TABLE `Property` (`Property` CHAR(72) NOT NULL, `Value` LONGCHAR NOT NULL PRIMARY KEY `Property`)");
    run(db, "INSERT INTO `Property` (`Property`, `Value`) VALUES ('ProductCode', '%s')", rp_pc);
    run_ok(db, "INSERT INTO `Property` (`Property`, `Value`) VALUES ('ProductName', 'SG Patch Removal Test')");
    run_ok(db, "INSERT INTO `Property` (`Property`, `Value`) VALUES ('ProductVersion', '1.0.0')");
    run_ok(db, "INSERT INTO `Property` (`Property`, `Value`) VALUES ('ProductLanguage', '1033')");
    run_ok(db, "INSERT INTO `Property` (`Property`, `Value`) VALUES ('Manufacturer', 'Wine')");
    run_ok(db, "INSERT INTO `Property` (`Property`, `Value`) VALUES ('ALLUSERS', '1')");
    run_ok(db, "INSERT INTO `Property` (`Property`, `Value`) VALUES ('PropOne', 'plain')");
    run_ok(db, "INSERT INTO `Property` (`Property`, `Value`) VALUES ('PropTwo', 'plain')");
    run_ok(db, "INSERT INTO `Property` (`Property`, `Value`) VALUES ('UpgradeCode', '{6A1C2B40-9C3D-4E55-8A00-3217000000E1}')");
    run_ok(db, "CREATE TABLE `Directory` (`Directory` CHAR(255) NOT NULL, `Directory_Parent` CHAR(255), `DefaultDir` CHAR(255) NOT NULL PRIMARY KEY `Directory`)");
    run_ok(db, "INSERT INTO `Directory` (`Directory`, `DefaultDir`) VALUES ('TARGETDIR', 'SourceDir')");
    run_ok(db, "CREATE TABLE `Component` (`Component` CHAR(72) NOT NULL, `ComponentId` CHAR(38), `Directory_` CHAR(72) NOT NULL, `Attributes` SHORT NOT NULL, `Condition` CHAR(255), `KeyPath` CHAR(72) PRIMARY KEY `Component`)");
    run_ok(db, "INSERT INTO `Component` (`Component`, `ComponentId`, `Directory_`, `Attributes`, `KeyPath`) VALUES ('comp', '{A1B2C3D4-0000-4000-8000-0000000000C1}', 'TARGETDIR', 4, 'reg1')");
    run_ok(db, "CREATE TABLE `Feature` (`Feature` CHAR(38) NOT NULL, `Feature_Parent` CHAR(38), `Title` CHAR(64), `Description` CHAR(255), `Display` SHORT, `Level` SHORT NOT NULL, `Directory_` CHAR(72), `Attributes` SHORT NOT NULL PRIMARY KEY `Feature`)");
    run_ok(db, "INSERT INTO `Feature` (`Feature`, `Title`, `Level`, `Directory_`, `Attributes`) VALUES ('feat', 'Feature', 1, 'TARGETDIR', 0)");
    run_ok(db, "CREATE TABLE `FeatureComponents` (`Feature_` CHAR(38) NOT NULL, `Component_` CHAR(72) NOT NULL PRIMARY KEY `Feature_`, `Component_`)");
    run_ok(db, "INSERT INTO `FeatureComponents` (`Feature_`, `Component_`) VALUES ('feat', 'comp')");
    run_ok(db, "CREATE TABLE `Registry` (`Registry` CHAR(72) NOT NULL, `Root` SHORT NOT NULL, `Key` CHAR(255) NOT NULL, `Name` CHAR(255) LOCALIZABLE, `Value` CHAR(0) LOCALIZABLE, `Component_` CHAR(72) NOT NULL PRIMARY KEY `Registry`)");
    run_ok(db, "INSERT INTO `Registry` (`Registry`, `Root`, `Key`, `Name`, `Value`, `Component_`) VALUES ('reg1', 2, 'Software\\SGMsiB2', 'One', '[PropOne]', 'comp')");
    run_ok(db, "INSERT INTO `Registry` (`Registry`, `Root`, `Key`, `Name`, `Value`, `Component_`) VALUES ('reg2', 2, 'Software\\SGMsiB2', 'Two', '[PropTwo]', 'comp')");
    run_ok(db, "CREATE TABLE `Media` (`DiskId` SHORT NOT NULL, `LastSequence` SHORT NOT NULL, `DiskPrompt` CHAR(64), `Cabinet` CHAR(255), `VolumeLabel` CHAR(32), `Source` CHAR(32) PRIMARY KEY `DiskId`)");
    run_ok(db, "CREATE TABLE `File` (`File` CHAR(72) NOT NULL, `Component_` CHAR(72) NOT NULL, `FileName` CHAR(255) NOT NULL, `FileSize` LONG NOT NULL, `Version` CHAR(72), `Language` CHAR(20), `Attributes` SHORT, `Sequence` SHORT NOT NULL PRIMARY KEY `File`)");
    run_ok(db, "CREATE TABLE `InstallExecuteSequence` (`Action` CHAR(72) NOT NULL, `Condition` CHAR(255), `Sequence` SHORT PRIMARY KEY `Action`)");
    {
        static const struct { const char *a; int n; } seq[] = {
            { "CostInitialize", 800 }, { "FileCost", 900 }, { "CostFinalize", 1000 }, { "InstallValidate", 1400 },
            { "InstallInitialize", 1500 }, { "ProcessComponents", 1600 }, { "WriteRegistryValues", 5000 },
            { "RemoveRegistryValues", 3600 }, { "RegisterProduct", 6100 },
            { "PublishFeatures", 6300 }, { "PublishProduct", 6400 }, { "InstallFinalize", 6600 } };
        unsigned i;
        for (i = 0; i < sizeof(seq) / sizeof(seq[0]); i++)
            run(db, "INSERT INTO `InstallExecuteSequence` (`Action`, `Sequence`) VALUES ('%s', %d)", seq[i].a, seq[i].n);
    }
    MsiDatabaseCommit(db);
    MsiCloseHandle(db);
}

static BOOL write_patch(const char *mspfile, const char *base, const char *patchcode, const char *prop, int allow_removal)
{
    static const CLSID CLSID_MsiPatch = {0xc1086, 0, 0, {0xc0, 0, 0, 0, 0, 0, 0, 0x46}};
    char upd[MAX_PATH], mst[MAX_PATH], buf[128];
    MSIHANDLE a = 0, b = 0, p = 0, si;
    IStorage *stg = NULL, *sub = NULL, *src = NULL;
    WCHAR wmsp[MAX_PATH], wmst[MAX_PATH];
    HRESULT hr;
    UINT r;
    int ok = 0;

    sprintf(upd, "fastmsib2-upd-%s.msi", prop);
    sprintf(mst, "fastmsib2-%s.mst", prop);
    DeleteFileA(upd); DeleteFileA(mst); DeleteFileA(mspfile);
    CopyFileA(base, upd, FALSE);

    /* the transform: the property Flavour of the patched product */
    r = MsiOpenDatabaseA(upd, MSIDBOPEN_DIRECT, &a);
    OK(!r, "open updated copy: %u", r);
    if (r) return FALSE;
    sprintf(buf, "UPDATE `Property` SET `Value` = 'patched' WHERE `Property` = '%s'", prop);
    run_ok(a, buf);
    MsiDatabaseCommit(a);
    r = MsiOpenDatabaseA(base, MSIDBOPEN_READONLY, &b);
    OK(!r, "open base: %u", r);
    r = MsiDatabaseGenerateTransformA(a, b, mst, 0, 0);
    OK(!r, "generate transform: %u", r);
    r = MsiCreateTransformSummaryInfoA(a, b, mst, 0, 0);
    OK(!r, "transform summary information: %u", r);
    MsiCloseHandle(a); MsiCloseHandle(b);
    DeleteFileA(upd);

    /* the patch package: tables and summary information, then the transform as a substorage */
    r = MsiOpenDatabaseA(mspfile, (const char *)(MSIDBOPEN_CREATE + 32), &p);   /* MSIDBOPEN_PATCHFILE */
    OK(!r, "create patch: %u", r);
    if (r) return FALSE;
    MsiGetSummaryInformationA(p, NULL, 20, &si);
    MsiSummaryInfoSetPropertyA(si, 2, VT_LPSTR, 0, NULL, "Patch");
    MsiSummaryInfoSetPropertyA(si, 3, VT_LPSTR, 0, NULL, "Patch removal test");
    MsiSummaryInfoSetPropertyA(si, 4, VT_LPSTR, 0, NULL, "Wine Hackers");
    MsiSummaryInfoSetPropertyA(si, 7, VT_LPSTR, 0, NULL, rp_pc);                 /* the products it applies to */
    MsiSummaryInfoSetPropertyA(si, 8, VT_LPSTR, 0, NULL, ":t1");                 /* the transforms */
    MsiSummaryInfoSetPropertyA(si, 9, VT_LPSTR, 0, NULL, patchcode);             /* the patch code */
    MsiSummaryInfoSetPropertyA(si, 14, VT_I4, 200, NULL, NULL);
    MsiSummaryInfoSetPropertyA(si, 15, VT_I4, 0, NULL, NULL);
    MsiSummaryInfoPersist(si);
    MsiCloseHandle(si);
    run_ok(p, "CREATE TABLE `MsiPatchMetadata` (`Company` CHAR(72), `Property` CHAR(72) NOT NULL, `Value` LONGCHAR NOT NULL PRIMARY KEY `Company`, `Property`)");
    sprintf(buf, "INSERT INTO `MsiPatchMetadata` (`Company`, `Property`, `Value`) VALUES ('', 'AllowRemoval', '%d')", allow_removal);
    run_ok(p, buf);
    MsiDatabaseCommit(p);
    MsiCloseHandle(p);

    MultiByteToWideChar(CP_ACP, 0, mspfile, -1, wmsp, MAX_PATH);
    MultiByteToWideChar(CP_ACP, 0, mst, -1, wmst, MAX_PATH);
    hr = StgOpenStorage(wmsp, NULL, STGM_DIRECT | STGM_READWRITE | STGM_SHARE_EXCLUSIVE, NULL, 0, &stg);
    OK(hr == S_OK, "open patch storage: %#lx", hr);
    if (hr == S_OK)
    {
        hr = IStorage_CreateStorage(stg, L"t1", STGM_CREATE | STGM_READWRITE | STGM_DIRECT | STGM_SHARE_EXCLUSIVE, 0, 0, &sub);
        OK(hr == S_OK, "create substorage: %#lx", hr);
        hr = StgOpenStorage(wmst, NULL, STGM_DIRECT | STGM_READ | STGM_SHARE_EXCLUSIVE, NULL, 0, &src);
        OK(hr == S_OK, "open transform storage: %#lx", hr);
        if (sub && src)
        {
            CLSID clsid;
            STATSTG st;
            hr = IStorage_CopyTo(src, 0, NULL, NULL, sub);
            OK(hr == S_OK, "copy transform into the patch: %#lx", hr);
            if (SUCCEEDED(IStorage_Stat(src, &st, STATFLAG_NONAME))) { clsid = st.clsid; IStorage_SetClass(sub, &clsid); }
            ok = 1;
        }
        if (src) IStorage_Release(src);
        if (sub) { IStorage_Commit(sub, STGC_DEFAULT); IStorage_Release(sub); }
        IStorage_Commit(stg, STGC_DEFAULT);
        IStorage_Release(stg);
    }
    (void)CLSID_MsiPatch;
    DeleteFileA(mst);
    return ok;
}

static void get_flavour(const char *prop, char *buf, DWORD size)
{
    MSIHANDLE pkg = 0;

    strcpy(buf, "?");
    if (!MsiOpenProductA(rp_pc, &pkg))
    {
        buf[0] = 0;
        MsiGetPropertyA(pkg, prop, buf, &size);
        MsiCloseHandle(pkg);
    }
}

static void reg_value(const char *name, char *buf, DWORD size)
{
    HKEY key;

    strcpy(buf, "<none>");
    if (!RegOpenKeyExA(HKEY_LOCAL_MACHINE, "Software\\SGMsiB2", 0, KEY_READ | KEY_WOW64_32KEY, &key))
    {
        DWORD type;
        if (RegQueryValueExA(key, name, NULL, &type, (BYTE *)buf, &size)) strcpy(buf, "<none>");
        RegCloseKey(key);
    }
}

static void test_remove_patches(void)
{
    char msi[MAX_PATH], msp1[MAX_PATH], msp2[MAX_PATH], flavour[64], list[512], buf[128];
    DWORD sz;
    UINT r;
    HKEY key;
    DWORD one = 1;
    MSIPATCHSTATE st;

    GetFullPathNameA("fastmsib2-prod.msi", MAX_PATH, msi, NULL);
    GetFullPathNameA("fastmsib2-one.msp", MAX_PATH, msp1, NULL);
    GetFullPathNameA("fastmsib2-two.msp", MAX_PATH, msp2, NULL);
    DeleteFileA(msi);
    make_product(msi);
    if (!write_patch(msp1, msi, rp_patch1, "PropOne", 1) || !write_patch(msp2, msi, rp_patch2, "PropTwo", 0))
    {
        OK(0, "could not build the patches");
        return;
    }

    MsiSetInternalUI(INSTALLUILEVEL_NONE, NULL);

    /* arguments and states before anything is installed */
    OK(MsiRemovePatchesA(NULL, rp_pc, INSTALLTYPE_SINGLE_INSTANCE, NULL) == ERROR_INVALID_PARAMETER, "NULL patch list");
    OK(MsiRemovePatchesA("", rp_pc, INSTALLTYPE_SINGLE_INSTANCE, NULL) == ERROR_INVALID_PARAMETER, "empty patch list");
    OK(MsiRemovePatchesA(rp_patch1, NULL, INSTALLTYPE_SINGLE_INSTANCE, NULL) == ERROR_INVALID_PARAMETER, "NULL product");
    OK(MsiRemovePatchesA(rp_patch1, rp_pc, INSTALLTYPE_NETWORK_IMAGE, NULL) == ERROR_INVALID_PARAMETER, "only single instance");
    OK(MsiRemovePatchesA(rp_patch1, rp_pc, INSTALLTYPE_SINGLE_INSTANCE, NULL) == ERROR_UNKNOWN_PRODUCT, "product not installed");

    r = MsiInstallProductA(msi, "REBOOT=ReallySuppress");
    OK(r == ERROR_SUCCESS, "install the product: %u", r);
    get_flavour("PropOne", flavour, sizeof(flavour));
    OK(!strcmp(flavour, "plain"), "unpatched property '%s'", flavour);

    OK(MsiRemovePatchesA(rp_patch1, rp_pc, INSTALLTYPE_SINGLE_INSTANCE, NULL) == ERROR_UNKNOWN_PATCH, "patch not applied");
    OK(MsiRemovePatchesA("c:\\no\\such.msp", rp_pc, INSTALLTYPE_SINGLE_INSTANCE, NULL) == ERROR_PATCH_PACKAGE_OPEN_FAILED, "missing patch file");

    /* apply both patches */
    r = MsiApplyPatchA(msp1, rp_pc, INSTALLTYPE_SINGLE_INSTANCE, "REBOOT=ReallySuppress");
    OK(r == ERROR_SUCCESS, "apply the removable patch: %u", r);
    get_flavour("PropOne", flavour, sizeof(flavour));
    OK(!strcmp(flavour, "patched"), "property after the patch '%s'", flavour);
    reg_value("One", flavour, sizeof(flavour));
    OK(!strcmp(flavour, "patched"), "the patched install wrote the patched value: '%s'", flavour);
    r = MsiGetPatchInfoExA(rp_patch1, rp_pc, NULL, MSIINSTALLCONTEXT_MACHINE, "Uninstallable", buf, (sz = sizeof(buf), &sz));
    OK(r == ERROR_SUCCESS && !strcmp(buf, "1"), "patch 1 is uninstallable: %u '%s'", r, buf);

    /* a patch that is not removable */
    r = MsiApplyPatchA(msp2, rp_pc, INSTALLTYPE_SINGLE_INSTANCE, "REBOOT=ReallySuppress");
    OK(r == ERROR_SUCCESS, "apply the fixed patch: %u", r);
    r = MsiGetPatchInfoExA(rp_patch2, rp_pc, NULL, MSIINSTALLCONTEXT_MACHINE, "Uninstallable", buf, (sz = sizeof(buf), &sz));
    OK(r == ERROR_SUCCESS && !strcmp(buf, "0"), "patch 2 is not uninstallable: %u '%s'", r, buf);
    r = MsiRemovePatchesA(rp_patch2, rp_pc, INSTALLTYPE_SINGLE_INSTANCE, NULL);
    OK(r == ERROR_PATCH_REMOVAL_UNSUPPORTED, "removal of a patch without AllowRemoval: %u", r);
    sprintf(list, "%s;%s", rp_patch1, rp_patch2);
    r = MsiRemovePatchesA(list, rp_pc, INSTALLTYPE_SINGLE_INSTANCE, NULL);
    OK(r == ERROR_PATCH_REMOVAL_UNSUPPORTED, "one patch of the list is fixed: %u", r);
    r = MsiGetPatchInfoExA(rp_patch1, rp_pc, NULL, MSIINSTALLCONTEXT_MACHINE, "LocalPackage", buf, (sz = sizeof(buf), &sz));
    OK(r == ERROR_SUCCESS, "nothing was removed by the refused call: %u", r);

    /* the policy that forbids patch removal */
    if (!RegCreateKeyExA(HKEY_LOCAL_MACHINE, "Software\\Policies\\Microsoft\\Windows\\Installer", 0, NULL, 0, KEY_ALL_ACCESS, NULL, &key, NULL))
    {
        RegSetValueExA(key, "DisablePatchUninstall", 0, REG_DWORD, (BYTE *)&one, sizeof(one));
        r = MsiRemovePatchesA(rp_patch1, rp_pc, INSTALLTYPE_SINGLE_INSTANCE, NULL);
        OK(r == ERROR_PATCH_REMOVAL_DISALLOWED, "DisablePatchUninstall: %u", r);
        RegDeleteValueA(key, "DisablePatchUninstall");
        RegCloseKey(key);
    }
    get_flavour("PropOne", flavour, sizeof(flavour));
    OK(!strcmp(flavour, "patched"), "still patched '%s'", flavour);

    /* remove the removable patch by its patch code: the product is reinstalled without it */
    r = MsiRemovePatchesA(rp_patch1, rp_pc, INSTALLTYPE_SINGLE_INSTANCE, "REBOOT=ReallySuppress");
    OK(r == ERROR_SUCCESS, "remove patch 1: %u", r);
    r = MsiGetPatchInfoExA(rp_patch1, rp_pc, NULL, MSIINSTALLCONTEXT_MACHINE, "LocalPackage", buf, (sz = sizeof(buf), &sz));
    OK(r == ERROR_UNKNOWN_PATCH, "patch 1 is gone from the registration: %u", r);
    r = MsiGetPatchInfoExA(rp_patch2, rp_pc, NULL, MSIINSTALLCONTEXT_MACHINE, "LocalPackage", buf, (sz = sizeof(buf), &sz));
    OK(r == ERROR_SUCCESS, "patch 2 stays registered: %u", r);
    get_flavour("PropOne", flavour, sizeof(flavour));
    OK(!strcmp(flavour, "plain"), "the removed patch no longer applies: '%s'", flavour);
    reg_value("One", flavour, sizeof(flavour));
    OK(!strcmp(flavour, "plain"), "the reinstall without the patch wrote the original value: '%s'", flavour);
    reg_value("Two", flavour, sizeof(flavour));
    OK(!strcmp(flavour, "patched"), "the reinstall kept the other patch: '%s'", flavour);
    get_flavour("PropTwo", flavour, sizeof(flavour));
    OK(!strcmp(flavour, "patched"), "the remaining patch still applies: '%s'", flavour);
    { char tr[64]; DWORD tsz = sizeof(tr); r = MsiEnumPatchesA(rp_pc, 0, buf, tr, &tsz); }
    OK(r == ERROR_SUCCESS && !strcmp(buf, rp_patch2), "enumerated patch: %u '%s'", r, buf);
    { char tr[64]; DWORD tsz = sizeof(tr); r = MsiEnumPatchesA(rp_pc, 1, buf, tr, &tsz); }
    OK(r == ERROR_NO_MORE_ITEMS, "only one patch left: %u", r);
    r = MsiRemovePatchesA(rp_patch1, rp_pc, INSTALLTYPE_SINGLE_INSTANCE, NULL);
    OK(r == ERROR_UNKNOWN_PATCH, "second removal: %u", r);

    /* by the full path of a patch package: re-apply and remove */
    r = MsiApplyPatchA(msp1, rp_pc, INSTALLTYPE_SINGLE_INSTANCE, "REBOOT=ReallySuppress");
    OK(r == ERROR_SUCCESS, "apply patch 1 again: %u", r);
    get_flavour("PropOne", flavour, sizeof(flavour));
    OK(!strcmp(flavour, "patched"), "patch 1 applies again '%s'", flavour);
    r = MsiRemovePatchesA(msp1, rp_pc, INSTALLTYPE_SINGLE_INSTANCE, "REBOOT=ReallySuppress");
    OK(r == ERROR_SUCCESS, "remove patch 1 by its file: %u", r);
    r = MsiGetPatchInfoExA(rp_patch1, rp_pc, NULL, MSIINSTALLCONTEXT_MACHINE, "LocalPackage", buf, (sz = sizeof(buf), &sz));
    OK(r == ERROR_UNKNOWN_PATCH, "gone again: %u", r);
    get_flavour("PropOne", flavour, sizeof(flavour));
    OK(!strcmp(flavour, "plain"), "unpatched again '%s'", flavour);
    (void)st;

    /* clean up */
    MsiConfigureProductExA(rp_pc, 0, INSTALLSTATE_ABSENT, "REBOOT=ReallySuppress");
    OK(MsiQueryProductStateA(rp_pc) == INSTALLSTATE_UNKNOWN, "uninstalled: %d", MsiQueryProductStateA(rp_pc));
    DeleteFileA(msi); DeleteFileA(msp1); DeleteFileA(msp2);
}

int main(void)
{
    CoInitialize(NULL);
    setvbuf(stdout, NULL, _IONBF, 0);
    test_last_error();
    test_billboard();
    test_remove_patches();
    printf("checks %d, failures %d\n", checks, fails);
    printf("RESULT: %s\n", fails ? "FAIL" : "PASS");
    return fails != 0;
}
