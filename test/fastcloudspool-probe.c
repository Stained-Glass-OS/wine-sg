#include <windows.h>
#include <stdio.h>
static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)
int main(void)
{
    HMODULE m = LoadLibraryA("spoolss.dll");
    BOOL (WINAPI *build)(WCHAR ***, DWORD *);
    HANDLE (WINAPI *revert)(void);
    BOOL (WINAPI *imp)(HANDLE);
    WCHAR **names; DWORD n, i, len = 100; WCHAR host[100];
    HANDLE tok, t2;
    CHECK(m != NULL, "load");
    if (!m) { printf("RESULT: FAIL\n"); return 1; }
    build = (void *)GetProcAddress(m, "BuildOtherNamesFromMachineName");
    revert = (void *)GetProcAddress(m, "RevertToPrinterSelf");
    imp = (void *)GetProcAddress(m, "ImpersonatePrinterClient");
    names = NULL; n = 0;
    CHECK(build(&names, &n), "build");
    CHECK(n >= 3 && names && names[0][0] == 0, "n=%lu", n);
    GetComputerNameW(host, &len);
    CHECK(n >= 2 && !lstrcmpW(names[1], host), "host name entry");
    CHECK(n >= 3 && !lstrcmpW(names[n - 1], L"::1"), "last entry ::1");
    for (i = 2; i + 1 < n; i++) CHECK(names[i][0] && lstrlenW(names[i]) < 50, "entry %lu", i);
    CHECK(HeapFree(GetProcessHeap(), 0, names), "free block");
    SetLastError(0xdeadbeef);
    CHECK(!build(NULL, &n) && GetLastError() == ERROR_INVALID_PARAMETER, "NULL arg");

    /* not impersonating */
    CHECK(revert() == NULL, "revert while not impersonating");
    CHECK(imp(NULL), "impersonate NULL");
    CHECK(!OpenThreadToken(GetCurrentThread(), TOKEN_QUERY, TRUE, &t2), "no thread token");
    /* impersonating */
    CHECK(ImpersonateSelf(SecurityImpersonation), "ImpersonateSelf");
    CHECK(OpenThreadToken(GetCurrentThread(), TOKEN_QUERY, TRUE, &t2), "token present");
    CloseHandle(t2);
    tok = revert();
    CHECK(tok != NULL, "revert returns token");
    CHECK(!OpenThreadToken(GetCurrentThread(), TOKEN_QUERY, TRUE, &t2), "reverted");
    CHECK(imp(tok), "resume impersonation");
    CHECK(OpenThreadToken(GetCurrentThread(), TOKEN_QUERY, TRUE, &t2), "impersonating again");
    CloseHandle(t2);
    RevertToSelf();
    printf(fails ? "RESULT: FAIL\n" : "RESULT: PASS\n");
    return fails != 0;
}
