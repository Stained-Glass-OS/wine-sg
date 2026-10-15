/* kernelbase + conhost: console aliases (AddConsoleAlias, GetConsoleAlias*, GetConsoleAliases*, GetConsoleAliasExes*;
 * patch 3739). Runs as a console program. */
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

static int f;
#define CHECK(c, ...) do { if (!(c)) { f++; printf("FAIL line %d: %s: ", __LINE__, #c); printf(__VA_ARGS__); printf("\n"); } } while (0)

static BOOL (WINAPI *pGetConsoleAliasExesA)(char *, DWORD);

int main(void)
{
    WCHAR buf[256], expect[256];
    char abuf[256];
    DWORD r, n;
    BOOL ok;

    pGetConsoleAliasExesA = NULL;
    ok = AddConsoleAliasW(L"ll", L"dir /w $*", L"cmd.exe");
    if (!ok && GetLastError() == ERROR_INVALID_ACCESS) { printf("FAIL: the gate gave no real console\nRESULT: FAIL\n"); return 1; }
    CHECK(ok, "add: %lu", GetLastError());

    memset(buf, 0, sizeof(buf));
    r = GetConsoleAliasW(L"ll", buf, sizeof(buf), L"cmd.exe");
    CHECK(r == (wcslen(L"dir /w $*") + 1) * sizeof(WCHAR) && !wcscmp(buf, L"dir /w $*"), "get: %lu '%ls'", r, buf);
    memset(buf, 0, sizeof(buf));
    r = GetConsoleAliasW(L"LL", buf, sizeof(buf), L"CMD.EXE");
    CHECK(r == (wcslen(L"dir /w $*") + 1) * sizeof(WCHAR) && !wcscmp(buf, L"dir /w $*"), "names are case-insensitive: %lu", r);
    SetLastError(0xdeadbeef);
    r = GetConsoleAliasW(L"ll", buf, 4, L"cmd.exe");
    CHECK(r == 0 && GetLastError() == ERROR_INSUFFICIENT_BUFFER, "short buffer: %lu %lu", r, GetLastError());
    SetLastError(0xdeadbeef);
    r = GetConsoleAliasW(L"nope", buf, sizeof(buf), L"cmd.exe");
    CHECK(r == 0 && GetLastError() == ERROR_NOT_FOUND, "missing alias: %lu %lu", r, GetLastError());
    SetLastError(0xdeadbeef);
    r = GetConsoleAliasW(L"ll", buf, sizeof(buf), L"other.exe");
    CHECK(r == 0 && GetLastError() == ERROR_NOT_FOUND, "other executable: %lu %lu", r, GetLastError());

    CHECK(AddConsoleAliasW(L"gg", L"git $*", L"cmd.exe"), "second alias");
    CHECK(AddConsoleAliasW(L"x", L"y", L"a.exe"), "other executable alias");

    /* lists */
    wcscpy(expect, L"ll=dir /w $*"); n = wcslen(expect) + 1; wcscpy(expect + n, L"gg=git $*"); n += wcslen(expect + n) + 1;
    r = GetConsoleAliasesLengthW(L"cmd.exe");
    CHECK(r == n * sizeof(WCHAR), "aliases length %lu, want %lu", r, n * sizeof(WCHAR));
    memset(buf, 0, sizeof(buf));
    r = GetConsoleAliasesW(buf, sizeof(buf), L"cmd.exe");
    CHECK(r == n * sizeof(WCHAR) && !memcmp(buf, expect, r), "aliases %lu", r);
    SetLastError(0xdeadbeef);
    r = GetConsoleAliasesW(buf, 10, L"cmd.exe");
    CHECK(r == 0 && GetLastError() == ERROR_INSUFFICIENT_BUFFER, "aliases, short buffer: %lu %lu", r, GetLastError());
    r = GetConsoleAliasesLengthW(L"none.exe");
    CHECK(r == 0, "an executable with no aliases: %lu", r);

    wcscpy(expect, L"cmd.exe"); n = wcslen(expect) + 1; wcscpy(expect + n, L"a.exe"); n += wcslen(expect + n) + 1;
    r = GetConsoleAliasExesLengthW();
    CHECK(r == n * sizeof(WCHAR), "exes length %lu, want %lu", r, n * sizeof(WCHAR));
    memset(buf, 0, sizeof(buf));
    r = GetConsoleAliasExesW(buf, sizeof(buf));
    CHECK(r == n * sizeof(WCHAR) && !memcmp(buf, expect, r), "exes %lu", r);
    SetLastError(0xdeadbeef);
    r = GetConsoleAliasExesW(buf, 6);
    CHECK(r == 0 && GetLastError() == ERROR_INSUFFICIENT_BUFFER, "exes, short buffer: %lu %lu", r, GetLastError());

    /* A forms */
    memset(abuf, 0, sizeof(abuf));
    r = GetConsoleAliasA((char *)"gg", abuf, sizeof(abuf), (char *)"cmd.exe");
    CHECK(r == strlen("git $*") + 1 && !strcmp(abuf, "git $*"), "A get: %lu '%s'", r, abuf);
    r = GetConsoleAliasesLengthA((char *)"cmd.exe");
    CHECK(r == strlen("ll=dir /w $*") + 1 + strlen("gg=git $*") + 1, "A aliases length %lu", r);
    memset(abuf, 0, sizeof(abuf));
    r = GetConsoleAliasesA(abuf, sizeof(abuf), (char *)"cmd.exe");
    CHECK(r == strlen("ll=dir /w $*") + 1 + strlen("gg=git $*") + 1 && !strcmp(abuf, "ll=dir /w $*") &&
          !strcmp(abuf + strlen(abuf) + 1, "gg=git $*"), "A aliases %lu", r);
    r = GetConsoleAliasExesLengthA();
    CHECK(r == strlen("cmd.exe") + 1 + strlen("a.exe") + 1, "A exes length %lu", r);
    memset(abuf, 0, sizeof(abuf));
    r = GetConsoleAliasExesA(abuf, sizeof(abuf));
    CHECK(r == strlen("cmd.exe") + 1 + strlen("a.exe") + 1 && !strcmp(abuf, "cmd.exe") && !strcmp(abuf + 8, "a.exe"), "A exes %lu", r);
    CHECK(AddConsoleAliasA((char *)"aa", (char *)"bb", (char *)"cmd.exe"), "A add");
    memset(buf, 0, sizeof(buf));
    r = GetConsoleAliasW(L"aa", buf, sizeof(buf), L"cmd.exe");
    CHECK(r == 6 && !wcscmp(buf, L"bb"), "A add visible to W: %lu", r);
    CHECK(AddConsoleAliasW(L"aa", NULL, L"cmd.exe"), "remove aa");

    /* replace */
    CHECK(AddConsoleAliasW(L"ll", L"ls", L"CMD.EXE"), "replace");
    memset(buf, 0, sizeof(buf));
    r = GetConsoleAliasW(L"ll", buf, sizeof(buf), L"cmd.exe");
    CHECK(r == 6 && !wcscmp(buf, L"ls"), "replaced value: %lu '%ls'", r, buf);
    r = GetConsoleAliasesLengthW(L"cmd.exe");
    CHECK(r == (wcslen(L"ll=ls") + 1 + wcslen(L"gg=git $*") + 1) * sizeof(WCHAR), "no duplicate after replace: %lu", r);
    r = GetConsoleAliasExesLengthW();
    CHECK(r == (8 + 6) * sizeof(WCHAR), "no duplicate executable: %lu", r);

    /* delete */
    CHECK(AddConsoleAliasW(L"ll", NULL, L"cmd.exe"), "delete");
    SetLastError(0xdeadbeef);
    r = GetConsoleAliasW(L"ll", buf, sizeof(buf), L"cmd.exe");
    CHECK(r == 0 && GetLastError() == ERROR_NOT_FOUND, "deleted: %lu %lu", r, GetLastError());
    r = GetConsoleAliasesLengthW(L"cmd.exe");
    CHECK(r == (wcslen(L"gg=git $*") + 1) * sizeof(WCHAR), "after delete: %lu", r);
    CHECK(AddConsoleAliasW(L"x", L"", L"a.exe"), "an empty target deletes");
    r = GetConsoleAliasExesLengthW();
    CHECK(r == 8 * sizeof(WCHAR), "an executable with no alias left is gone: %lu", r);
    CHECK(AddConsoleAliasW(L"never", NULL, L"cmd.exe"), "deleting a missing alias");
    CHECK(AddConsoleAliasW(L"never", NULL, L"nobody.exe"), "deleting for an unknown executable");
    r = GetConsoleAliasExesLengthW();
    CHECK(r == 8 * sizeof(WCHAR), "unknown executable not created by a delete: %lu", r);

    /* arguments */
    SetLastError(0xdeadbeef);
    CHECK(!AddConsoleAliasW(NULL, L"a", L"cmd.exe") && GetLastError() == ERROR_INVALID_PARAMETER, "NULL source: %lu", GetLastError());
    SetLastError(0xdeadbeef);
    CHECK(!AddConsoleAliasW(L"a", L"b", NULL) && GetLastError() == ERROR_INVALID_PARAMETER, "NULL exe: %lu", GetLastError());
    SetLastError(0xdeadbeef);
    r = GetConsoleAliasesLengthW(NULL);
    CHECK(r == 0 && GetLastError() == ERROR_INVALID_PARAMETER, "NULL exe, length: %lu %lu", r, GetLastError());
    SetLastError(0xdeadbeef);
    r = GetConsoleAliasW(L"gg", NULL, 10, L"cmd.exe");
    CHECK(r == 0 && GetLastError() == ERROR_INVALID_PARAMETER, "NULL buffer: %lu %lu", r, GetLastError());

    AddConsoleAliasW(L"gg", NULL, L"cmd.exe");
    printf("%s\n", f ? "RESULT: FAIL" : "RESULT: PASS");
    return f != 0;
}
