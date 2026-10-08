/* kernelbase functions that were stubs (patches/sg/1605).
 *
 *  - QueryInterruptTimePrecise / QueryUnbiasedInterruptTimePrecise read the
 *    clock now (they gave the last tick's value);
 *  - FindFirstStreamW / FindNextStreamW list a file's data stream
 *    ("::$DATA", its size), none for a directory; FindClose closes the
 *    handle; GetFileInformationByHandleEx(FileStreamInfo) answers;
 *  - RaiseFailFastException ends the process with the exception's code;
 *  - EnumDeviceDrivers and GetDeviceDriverBaseName/FileName list the
 *    kernel and drivers;
 *  - EnumPageFilesW reports the swap space as the page file.
 */
#include <windows.h>
#include <psapi.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

typedef void (WINAPI *query_time_t)(ULONGLONG *);
typedef HANDLE (WINAPI *find_first_stream_t)(const WCHAR *, int, void *, DWORD);
typedef BOOL (WINAPI *find_next_stream_t)(HANDLE, void *);
typedef void (WINAPI *fail_fast_t)(EXCEPTION_RECORD *, CONTEXT *, DWORD);
typedef struct { LARGE_INTEGER StreamSize; WCHAR cStreamName[MAX_PATH + 36]; } stream_data;

static int failures, page_files;
static WCHAR page_file_name[MAX_PATH];

static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) failures++;
}

static BOOL CALLBACK page_file_cb(void *ctx, ENUM_PAGE_FILE_INFORMATION *info, const WCHAR *name)
{
    page_files++;
    lstrcpynW(page_file_name, name, MAX_PATH);
    printf("page file %ls: %lu pages, %lu in use\n", name, (unsigned long)info->TotalSize, (unsigned long)info->TotalInUse);
    return TRUE;
}

static DWORD run_child(const char *exe, const char *arg)
{
    char cmd[MAX_PATH + 32];
    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    DWORD code = 0;

    snprintf(cmd, sizeof(cmd), "\"%s\" %s", exe, arg);
    if (!CreateProcessA(NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) return 0;
    WaitForSingleObject(pi.hProcess, 20000);
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return code;
}

int main(int argc, char **argv)
{
    HMODULE k32 = GetModuleHandleA("kernel32.dll");
    HMODULE kb = GetModuleHandleA("kernelbase.dll");
    query_time_t pQueryInterruptTimePrecise = (void *)GetProcAddress(kb, "QueryInterruptTimePrecise");
    query_time_t pQueryUnbiasedInterruptTimePrecise = (void *)GetProcAddress(kb, "QueryUnbiasedInterruptTimePrecise");
    query_time_t pQueryInterruptTime = (void *)GetProcAddress(kb, "QueryInterruptTime");
    find_first_stream_t pFindFirstStreamW = (void *)GetProcAddress(k32, "FindFirstStreamW");
    find_next_stream_t pFindNextStreamW = (void *)GetProcAddress(k32, "FindNextStreamW");
    fail_fast_t pRaiseFailFastException = (void *)GetProcAddress(k32, "RaiseFailFastException");
    WCHAR dir[MAX_PATH], path[MAX_PATH], name[MAX_PATH];
    BYTE buf[1024];
    stream_data sd;
    ULONGLONG t1, t2, coarse;
    LARGE_INTEGER freq, c1, c2;
    void *drivers[64];
    DWORD needed, len, written, code;
    HANDLE h, file;
    MEMORYSTATUSEX ms;
    BOOL ok;

    if (argc > 1 && !strcmp(argv[1], "failfast"))
    {
        EXCEPTION_RECORD rec = {0};
        rec.ExceptionCode = 0xc0000409;
        rec.NumberParameters = 1;
        rec.ExceptionInformation[0] = 7;
        pRaiseFailFastException(&rec, NULL, 0);
        return 1;
    }
    if (argc > 1 && !strcmp(argv[1], "failfast-null"))
    {
        pRaiseFailFastException(NULL, NULL, 0);
        return 1;
    }

    /* precise interrupt time */
    pQueryInterruptTimePrecise(&t1);
    QueryPerformanceCounter(&c1);
    do QueryPerformanceCounter(&c2); while (c2.QuadPart - c1.QuadPart < 20000);  /* 2 ms at 10 MHz */
    QueryPerformanceFrequency(&freq);
    pQueryInterruptTimePrecise(&t2);
    pQueryInterruptTime(&coarse);
    printf("precise %llu -> %llu (+%llu), coarse %llu, qpc freq %lld\n", t1, t2, t2 - t1, coarse, freq.QuadPart);
    check(t2 - t1 >= 15000 && t2 - t1 < 1000000, "QueryInterruptTimePrecise follows the clock within the tick");
    check(t2 + 1000000 > coarse && coarse + 1000000 > t2, "... and agrees with QueryInterruptTime");
    pQueryUnbiasedInterruptTimePrecise(&t1);
    Sleep(5);
    pQueryUnbiasedInterruptTimePrecise(&t2);
    check(t2 > t1 && t2 - t1 >= 40000, "QueryUnbiasedInterruptTimePrecise advances");

    /* streams */
    GetTempPathW(MAX_PATH, dir);
    swprintf(path, MAX_PATH, L"%lskbmisc-stream.bin", dir);
    file = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    memset(buf, 'z', sizeof(buf));
    WriteFile(file, buf, 1000, &written, NULL);
    CloseHandle(file);
    memset(&sd, 0, sizeof(sd));
    h = pFindFirstStreamW(path, 0, &sd, 0);
    printf("FindFirstStreamW: %p err %lu name %ls size %lld\n", h, h == INVALID_HANDLE_VALUE ? GetLastError() : 0,
           sd.cStreamName, sd.StreamSize.QuadPart);
    check(h != INVALID_HANDLE_VALUE && !wcscmp(sd.cStreamName, L"::$DATA") && sd.StreamSize.QuadPart == 1000,
          "FindFirstStreamW gives the file's data stream and its size");
    SetLastError(0xdeadbeef);
    ok = pFindNextStreamW(h, &sd);
    check(!ok && GetLastError() == ERROR_HANDLE_EOF, "FindNextStreamW: no more streams (ERROR_HANDLE_EOF)");
    check(FindClose(h), "FindClose closes the stream handle");
    SetLastError(0xdeadbeef);
    h = pFindFirstStreamW(dir, 0, &sd, 0);
    check(h == INVALID_HANDLE_VALUE && GetLastError() == ERROR_HANDLE_EOF, "a directory has no data stream");
    swprintf(name, MAX_PATH, L"%lsno-such-file.bin", dir);
    SetLastError(0xdeadbeef);
    h = pFindFirstStreamW(name, 0, &sd, 0);
    check(h == INVALID_HANDLE_VALUE && GetLastError() == ERROR_FILE_NOT_FOUND, "a missing file: ERROR_FILE_NOT_FOUND");
    file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    memset(buf, 0, sizeof(buf));
    ok = GetFileInformationByHandleEx(file, FileStreamInfo, buf, sizeof(buf));
    check(ok && ((FILE_STREAM_INFO *)buf)->StreamSize.QuadPart == 1000 &&
          ((FILE_STREAM_INFO *)buf)->StreamNameLength == 14, "GetFileInformationByHandleEx(FileStreamInfo) answers");
    CloseHandle(file);
    DeleteFileW(path);

    /* fail fast */
    code = run_child(argv[0], "failfast");
    printf("fail-fast child exit code %#lx\n", code);
    check(code == 0xc0000409, "RaiseFailFastException ends the process with the exception's code");
    code = run_child(argv[0], "failfast-null");
    printf("fail-fast (no record) child exit code %#lx\n", code);
    check(code == 0xc0000602, "... STATUS_FAIL_FAST_EXCEPTION without a record");

    /* device drivers */
    needed = 0;
    ok = EnumDeviceDrivers(drivers, sizeof(drivers), &needed);
    printf("EnumDeviceDrivers: ok %d, %lu drivers\n", ok, needed / (DWORD)sizeof(void *));
    check(ok && needed >= sizeof(void *), "EnumDeviceDrivers lists the kernel and drivers");
    len = needed ? GetDeviceDriverBaseNameW(drivers[0], name, MAX_PATH) : 0;
    printf("first driver: %ls (%lu)\n", len ? name : L"", len);
    check(len && !_wcsicmp(name, L"ntoskrnl.exe"), "GetDeviceDriverBaseNameW names the kernel");
    len = needed ? GetDeviceDriverFileNameW(drivers[0], name, MAX_PATH) : 0;
    printf("its file: %ls\n", len ? name : L"");
    check(len && wcsstr(name, L"\\system32\\ntoskrnl.exe"), "GetDeviceDriverFileNameW gives its path");
    check(!GetDeviceDriverBaseNameW((void *)0x1234, name, MAX_PATH), "an unknown base has no name");

    /* page files */
    ms.dwLength = sizeof(ms);
    GlobalMemoryStatusEx(&ms);
    ok = EnumPageFilesW((PENUM_PAGE_FILE_CALLBACKW)page_file_cb, NULL);
    check(ok, "EnumPageFilesW succeeds");
    if (ms.ullTotalPageFile > ms.ullTotalPhys)
        check(page_files == 1 && !wcscmp(page_file_name, L"C:\\pagefile.sys"), "the swap space is reported as the page file");
    else
        check(page_files == 0, "no swap: no page file");

    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
