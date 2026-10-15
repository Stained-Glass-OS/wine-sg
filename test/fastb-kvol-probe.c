/* kernel32: FindFirstVolumeMountPoint, SetVolumeMountPointA, SetFirmwareEnvironmentVariableA (patch 3736).
 * The A forms must behave as the W forms; with no folder mount points the enumeration ends at once. */
#include <windows.h>
#include <stdio.h>
#include <string.h>

static int f;
#define CHECK(c, ...) do { if (!(c)) { f++; printf("FAIL line %d: %s: ", __LINE__, #c); printf(__VA_ARGS__); printf("\n"); } } while (0)

int main(void)
{
    char bufA[MAX_PATH];
    WCHAR bufW[MAX_PATH];
    HANDLE h;
    DWORD errW, errA;
    BOOL okW, okA;

    SetLastError(0xdeadbeef);
    h = FindFirstVolumeMountPointW(L"C:\\", bufW, MAX_PATH);
    CHECK(h == INVALID_HANDLE_VALUE && GetLastError() == ERROR_NO_MORE_FILES, "W on C:\\ : %p %lu", h, GetLastError());
    SetLastError(0xdeadbeef);
    h = FindFirstVolumeMountPointA("C:\\", bufA, MAX_PATH);
    CHECK(h == INVALID_HANDLE_VALUE && GetLastError() == ERROR_NO_MORE_FILES, "A on C:\\ : %p %lu", h, GetLastError());
    SetLastError(0xdeadbeef);
    h = FindFirstVolumeMountPointA("Q:\\", bufA, MAX_PATH);
    CHECK(h == INVALID_HANDLE_VALUE && GetLastError() != ERROR_NO_MORE_FILES && GetLastError() != 0xdeadbeef, "A on a missing volume: %lu", GetLastError());
    SetLastError(0xdeadbeef);
    h = FindFirstVolumeMountPointW(L"C:", bufW, MAX_PATH);
    CHECK(h == INVALID_HANDLE_VALUE && GetLastError() != ERROR_NO_MORE_FILES && GetLastError() != 0xdeadbeef, "W without the backslash: %lu", GetLastError());
    SetLastError(0xdeadbeef);
    h = FindFirstVolumeMountPointW(NULL, bufW, MAX_PATH);
    CHECK(h == INVALID_HANDLE_VALUE && GetLastError() == ERROR_INVALID_PARAMETER, "NULL root: %lu", GetLastError());
    SetLastError(0xdeadbeef);
    h = FindFirstVolumeMountPointA("C:\\", NULL, MAX_PATH);
    CHECK(h == INVALID_HANDLE_VALUE && GetLastError() == ERROR_INVALID_PARAMETER, "NULL buffer: %lu", GetLastError());
    SetLastError(0xdeadbeef);
    h = FindFirstVolumeMountPointA("C:\\", bufA, 0);
    CHECK(h == INVALID_HANDLE_VALUE && GetLastError() == ERROR_INVALID_PARAMETER, "zero length: %lu", GetLastError());
    SetLastError(0xdeadbeef);
    okA = FindVolumeMountPointClose(INVALID_HANDLE_VALUE);
    CHECK(!okA && GetLastError() == ERROR_INVALID_HANDLE, "close of nothing: %d %lu", okA, GetLastError());

    SetLastError(0xdeadbeef);
    okW = SetVolumeMountPointW(L"C:\\", L"\\\\?\\Volume{00000000-0000-0000-0000-000000000000}\\"); errW = GetLastError();
    SetLastError(0xdeadbeef);
    okA = SetVolumeMountPointA("C:\\", "\\\\?\\Volume{00000000-0000-0000-0000-000000000000}\\"); errA = GetLastError();
    CHECK(okA == okW && errA == errW, "SetVolumeMountPoint A %d/%lu, W %d/%lu", okA, errA, okW, errW);
    SetLastError(0xdeadbeef);
    okA = SetVolumeMountPointA(NULL, NULL); errA = GetLastError();
    SetLastError(0xdeadbeef);
    okW = SetVolumeMountPointW(NULL, NULL); errW = GetLastError();
    CHECK(okA == okW && errA == errW, "NULL arguments: A %d/%lu, W %d/%lu", okA, errA, okW, errW);

    SetLastError(0xdeadbeef);
    okW = SetFirmwareEnvironmentVariableW(L"FastbVar", L"{8BE4DF61-93CA-11D2-AA0D-00E098032B8C}", (void *)"x", 1); errW = GetLastError();
    SetLastError(0xdeadbeef);
    okA = SetFirmwareEnvironmentVariableA("FastbVar", "{8BE4DF61-93CA-11D2-AA0D-00E098032B8C}", (void *)"x", 1); errA = GetLastError();
    CHECK(okA == okW && errA == errW && errA != 0xdeadbeef, "SetFirmwareEnvironmentVariable A %d/%lu, W %d/%lu", okA, errA, okW, errW);
    SetLastError(0xdeadbeef);
    okA = SetFirmwareEnvironmentVariableA(NULL, NULL, NULL, 0); errA = GetLastError();
    SetLastError(0xdeadbeef);
    okW = SetFirmwareEnvironmentVariableW(NULL, NULL, NULL, 0); errW = GetLastError();
    CHECK(okA == okW && errA == errW, "NULL arguments: A %d/%lu, W %d/%lu", okA, errA, okW, errW);

    printf("%s\n", f ? "RESULT: FAIL" : "RESULT: PASS");
    return f != 0;
}
