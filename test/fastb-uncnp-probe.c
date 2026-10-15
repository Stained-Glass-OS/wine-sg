/* shell32 (3703): SHValidateUNC success path against a test network provider (no real network) */
#include <windows.h>
#include <shlobj.h>
#include <stdio.h>

static int failed;
#define CHECK(c, ...) do { if (!(c)) { failed++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

int main(void)
{
    BOOL (WINAPI *pValidate)(HWND, PWSTR, UINT) = (void *)GetProcAddress(LoadLibraryA("shell32.dll"), (LPCSTR)173);
    WCHAR path[MAX_PATH];
    BOOL ret;

    if (!pValidate) { printf("FAIL no SHValidateUNC\nRESULT: FAIL\n"); return 1; }

    lstrcpyW(path, L"\\\\sgfake\\ok\\dir\\file.txt");
    ret = pValidate(NULL, path, VALIDATEUNC_NOUI);
    CHECK(ret, "a share the provider knows: TRUE");
    CHECK(!lstrcmpW(path, L"\\\\sgfake\\ok\\dir\\file.txt"), "without the connect flag the path is unchanged: %ls", path);

    lstrcpyW(path, L"\\\\sgfake\\nope\\dir\\file.txt");
    ret = pValidate(NULL, path, VALIDATEUNC_NOUI);
    CHECK(!ret, "a share it does not know: FALSE");
    CHECK(!lstrcmpW(path, L"\\\\sgfake\\nope\\dir\\file.txt"), "and the path is unchanged");

    lstrcpyW(path, L"\\\\SGFAKE\\OK");
    ret = pValidate(NULL, path, VALIDATEUNC_NOUI);
    CHECK(ret, "the share alone, other case: TRUE");

    /* VALIDATEUNC_CONNECT: a drive letter, and the path becomes the path on that drive */
    lstrcpyW(path, L"\\\\sgfake\\ok\\dir\\file.txt");
    ret = pValidate(NULL, path, VALIDATEUNC_CONNECT | VALIDATEUNC_NOUI);
    CHECK(ret, "connect: TRUE");
    CHECK(path[1] == ':' && path[0] >= 'A' && path[0] <= 'Z' && !lstrcmpW(path + 2, L"\\dir\\file.txt"), "the path is on the new drive: %ls", path);

    lstrcpyW(path, L"\\\\sgfake\\ok");
    ret = pValidate(NULL, path, VALIDATEUNC_CONNECT | VALIDATEUNC_NOUI);
    CHECK(ret && path[1] == ':' && !path[2], "connect, the share alone: %ls", path);

    lstrcpyW(path, L"\\\\sgfake\\nope\\x");
    ret = pValidate(NULL, path, VALIDATEUNC_CONNECT | VALIDATEUNC_NOUI);
    CHECK(!ret && !lstrcmpW(path, L"\\\\sgfake\\nope\\x"), "connect to an unknown share: FALSE, path unchanged");

    /* without NOUI the provider is asked to connect interactively, which it refuses */
    lstrcpyW(path, L"\\\\sgfake\\ok\\x");
    ret = pValidate(NULL, path, 0);
    CHECK(!ret, "interactive: the test provider refuses");

    /* not UNC, and bad flags */
    lstrcpyW(path, L"C:\\windows");
    CHECK(!pValidate(NULL, path, VALIDATEUNC_NOUI), "not a UNC path");
    lstrcpyW(path, L"\\\\sgfake\\ok");
    CHECK(!pValidate(NULL, path, 0x100), "unknown flag");
    CHECK(!pValidate(NULL, NULL, VALIDATEUNC_NOUI), "NULL path");

    printf("failures: %d\nRESULT: %s\n", failed, failed ? "FAIL" : "PASS");
    return failed != 0;
}
