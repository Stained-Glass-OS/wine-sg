/* ReplaceFileW merges the replaced file's information, and other small
 * stubs (patches/sg/1616).
 *
 *  - ReplaceFileW gives the replacement the replaced file's attributes
 *    (hidden here; a safe save kept the new file's), and takes
 *    REPLACEFILE_WRITE_THROUGH;
 *  - CallNtPowerInformation(SystemPowerInformation) answers (it was "not
 *    implemented", and for 32-bit programs an unsupported level);
 *  - GetPwrCapabilities reports the machine's sleep states: argv[1] says
 *    whether the kernel offers suspend to RAM ("s3") and to disk ("s4");
 *  - HeapSetInformation(HeapEnableTerminationOnCorruption) is taken,
 *    HeapOptimizeResources checks its version;
 *  - SetProcessShutdownParameters refuses levels past 0x4ff;
 *  - WofIsExternalFile: a missing file is an error, a file without WOF
 *    backing is not external.
 */
#include <windows.h>
#include <powrprof.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

typedef LONG (WINAPI *cnpi_t)(int, void *, ULONG, void *, ULONG);
typedef BOOLEAN (WINAPI *caps_t)(SYSTEM_POWER_CAPABILITIES *);
typedef HRESULT (WINAPI *wof_t)(const WCHAR *, BOOL *, ULONG *, void *, ULONG *);
typedef struct { ULONG MaxIdlenessAllowed, Idleness, TimeRemaining; UCHAR CoolingMode; } power_info;

static int failures;

static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) failures++;
}

static void make(const WCHAR *path, const char *text)
{
    DWORD written;
    HANDLE h = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    WriteFile(h, text, strlen(text), &written, NULL);
    CloseHandle(h);
}

int main(int argc, char **argv)
{
    HMODULE pp = LoadLibraryA("powrprof.dll"), wof = LoadLibraryA("wofutil.dll");
    cnpi_t pCallNtPowerInformation = (void *)GetProcAddress(pp, "CallNtPowerInformation");
    caps_t pGetPwrCapabilities = (void *)GetProcAddress(pp, "GetPwrCapabilities");
    wof_t pWofIsExternalFile = wof ? (void *)GetProcAddress(wof, "WofIsExternalFile") : NULL;
    WCHAR dir[MAX_PATH], oldf[MAX_PATH], newf[MAX_PATH], missing[MAX_PATH];
    SYSTEMTIME st = { 2001, 3, 0, 14, 12, 0, 0, 0 };
    FILETIME created, got;
    WIN32_FILE_ATTRIBUTE_DATA data;
    SYSTEM_POWER_CAPABILITIES caps;
    power_info pi;
    struct { DWORD version, flags; } optimize = { 2, 0 };
    DWORD level, flags;
    HANDLE h;
    char buf[32] = { 0 };
    DWORD read;
    BOOL ok, ext = 2;
    LONG status;
    HRESULT hr;

    GetTempPathW(MAX_PATH, dir);
    swprintf(oldf, MAX_PATH, L"%lsreplace-old.txt", dir);
    swprintf(newf, MAX_PATH, L"%lsreplace-new.txt", dir);
    SetFileAttributesW(oldf, FILE_ATTRIBUTE_NORMAL);
    make(oldf, "old");
    SystemTimeToFileTime(&st, &created);
    h = CreateFileW(oldf, FILE_WRITE_ATTRIBUTES, 0, NULL, OPEN_EXISTING, 0, NULL);
    SetFileTime(h, &created, NULL, NULL);
    CloseHandle(h);
    SetFileAttributesW(oldf, FILE_ATTRIBUTE_HIDDEN);
    make(newf, "new contents");
    ok = ReplaceFileW(oldf, newf, NULL, REPLACEFILE_WRITE_THROUGH, NULL, NULL);
    printf("ReplaceFileW: ok %d err %lu\n", ok, ok ? 0 : GetLastError());
    check(ok, "ReplaceFileW with REPLACEFILE_WRITE_THROUGH");
    h = CreateFileW(oldf, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    ReadFile(h, buf, sizeof(buf) - 1, &read, NULL);
    CloseHandle(h);
    check(!strcmp(buf, "new contents"), "the replaced file has the new contents");
    GetFileAttributesExW(oldf, GetFileExInfoStandard, &data);
    got = data.ftCreationTime;
    printf("creation %08lx%08lx (want %08lx%08lx), attributes %#lx\n", got.dwHighDateTime, got.dwLowDateTime,
           created.dwHighDateTime, created.dwLowDateTime, data.dwFileAttributes);
    /* (a creation time cannot be set on Linux file systems, so it is not checked) */
    check(data.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN, "... and its hidden attribute");
    SetFileAttributesW(oldf, FILE_ATTRIBUTE_NORMAL);
    DeleteFileW(oldf);

    memset(&pi, 0xcc, sizeof(pi));
    status = pCallNtPowerInformation(12 /* SystemPowerInformation */, NULL, 0, &pi, sizeof(pi));
    printf("SystemPowerInformation: %#lx max %lu idle %lu\n", status, pi.MaxIdlenessAllowed, pi.Idleness);
    check(!status && pi.MaxIdlenessAllowed == 100 && pi.Idleness <= 100, "CallNtPowerInformation(SystemPowerInformation)");

    memset(&caps, 0, sizeof(caps));
    ok = pGetPwrCapabilities(&caps);
    printf("capabilities: S1 %d S3 %d S4 %d batteries %d lid %d throttle %d (%d%%)\n", caps.SystemS1, caps.SystemS3,
           caps.SystemS4, caps.SystemBatteriesPresent, caps.LidPresent, caps.ProcessorThrottle, caps.ProcessorMinThrottle);
    if (argc > 1)
        check(ok && caps.SystemS3 == (strstr(argv[1], "s3") != NULL) && caps.SystemS4 == (strstr(argv[1], "s4") != NULL),
              "GetPwrCapabilities reports the kernel's sleep states");

    check(HeapSetInformation(NULL, HeapEnableTerminationOnCorruption, NULL, 0), "HeapEnableTerminationOnCorruption is taken");
    SetLastError(0xdeadbeef);
    check(!HeapSetInformation(GetProcessHeap(), 3, &optimize, sizeof(optimize)), "HeapOptimizeResources checks its version");

    SetLastError(0xdeadbeef);
    ok = SetProcessShutdownParameters(0x500, 0);
    check(!ok && GetLastError() == ERROR_INVALID_PARAMETER, "SetProcessShutdownParameters refuses level 0x500");
    ok = SetProcessShutdownParameters(0x3ff, SHUTDOWN_NORETRY) && GetProcessShutdownParameters(&level, &flags);
    check(ok && level == 0x3ff && flags == SHUTDOWN_NORETRY, "... and keeps 0x3ff");

    if (pWofIsExternalFile)
    {
        swprintf(missing, MAX_PATH, L"%lsno-such-wof-file", dir);
        hr = pWofIsExternalFile(missing, &ext, NULL, NULL, NULL);
        check(hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND), "WofIsExternalFile of a missing file fails");
        hr = pWofIsExternalFile(newf[0] ? dir : newf, &ext, NULL, NULL, NULL);
        check(hr == S_OK && ext == FALSE, "... of a plain file: not external");
    }

    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
