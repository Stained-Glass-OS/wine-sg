/* msiroshare gate probe (wine-sg 7726): a package that an install has open
 * can still be opened read-only. msi keeps the cached package open for
 * writing (transacted) while it installs; custom actions that read it
 * (Java's installer.exe: MsiOpenDatabase(C:\Windows\Installer\x.msi,
 * MSIDBOPEN_READONLY)) got ERROR_FUNCTION_FAILED and crashed. */
#include <windows.h>
#include <msi.h>
#include <msiquery.h>
#include <stdio.h>

static int failures;

static void check(int ok, const char *what, UINT r)
{
    printf("%s %s (%u)\n", ok ? "ok  " : "FAIL", what, r);
    if (!ok) failures++;
}

static UINT query_count(MSIHANDLE db, int *count)
{
    MSIHANDLE view, rec;
    UINT r = MsiDatabaseOpenViewA(db, "SELECT `Value` FROM `Property`", &view);
    *count = 0;
    if (r) return r;
    if ((r = MsiViewExecute(view, 0))) { MsiCloseHandle(view); return r; }
    while (!MsiViewFetch(view, &rec)) { (*count)++; MsiCloseHandle(rec); }
    MsiCloseHandle(view);
    return 0;
}

int main(void)
{
    char path[MAX_PATH];
    MSIHANDLE db, writer, reader, view;
    UINT r;
    int count;

    GetTempPathA(MAX_PATH, path);
    lstrcatA(path, "sgroshare.msi");
    DeleteFileA(path);

    r = MsiOpenDatabaseA(path, MSIDBOPEN_CREATE, &db);
    if (r) { printf("FAIL cannot create the database (%u)\nRESULT: FAIL\n", r); return 1; }
    MsiDatabaseOpenViewA(db, "CREATE TABLE `Property` (`Property` CHAR(72) NOT NULL, `Value` CHAR(0) PRIMARY KEY `Property`)", &view);
    MsiViewExecute(view, 0); MsiCloseHandle(view);
    MsiDatabaseOpenViewA(db, "INSERT INTO `Property` (`Property`, `Value`) VALUES ('ProductName', 'sg')", &view);
    MsiViewExecute(view, 0); MsiCloseHandle(view);
    MsiDatabaseCommit(db);
    MsiCloseHandle(db);

    /* the installing engine's hold on the package */
    r = MsiOpenDatabaseA(path, MSIDBOPEN_TRANSACT, &writer);
    check(!r, "the package opens transacted (as the engine opens it)", r);

    r = MsiOpenDatabaseA(path, MSIDBOPEN_READONLY, &reader);
    check(!r, "while it is open, a reader opens it read-only", r);
    if (!r)
    {
        r = query_count(reader, &count);
        check(!r && count == 1, "the reader sees its Property table", r);
        MsiCloseHandle(reader);
    }
    MsiCloseHandle(writer);

    /* with nothing else holding it, the read-only open still keeps writers out */
    r = MsiOpenDatabaseA(path, MSIDBOPEN_READONLY, &reader);
    check(!r, "read-only open of a free package", r);
    if (!r)
    {
        HANDLE h = CreateFileA(path, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
        check(h == INVALID_HANDLE_VALUE, "a free package opened read-only denies writing", GetLastError());
        if (h != INVALID_HANDLE_VALUE) CloseHandle(h);
        MsiCloseHandle(reader);
    }

    DeleteFileA(path);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
