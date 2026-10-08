/* More of BITS (patches/sg/1635): error descriptions, context and protocol;
 * enumerator clones; byte ranges (AddFileWithRanges, GetFileRanges, the
 * transfer of only those ranges); SetRemoteName; client certificate
 * settings; HTTP statuses BITS has no constant for (403 was taken as
 * success).
 *
 *   bitsmore-probe.exe PORT     the server of test/bitsmore-server.py
 */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <bits.h>
#include <bits2_0.h>
#include <bits2_5.h>
#include <bits3_0.h>
#include <bitsmsg.h>
#include <stdio.h>
#include <stdlib.h>

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

static int failures;

static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

static BG_JOB_STATE wait_done(IBackgroundCopyJob *job, DWORD ms)
{
    BG_JOB_STATE state = BG_JOB_STATE_QUEUED;
    DWORD start = GetTickCount();

    while (GetTickCount() - start < ms)
    {
        IBackgroundCopyJob_GetState(job, &state);
        if (state == BG_JOB_STATE_TRANSFERRED || state == BG_JOB_STATE_ERROR) break;
        Sleep(100);
    }
    return state;
}

static IBackgroundCopyFile2 *first_file(IBackgroundCopyJob *job)
{
    IEnumBackgroundCopyFiles *files;
    IBackgroundCopyFile *file = NULL;
    IBackgroundCopyFile2 *file2 = NULL;

    if (FAILED(IBackgroundCopyJob_EnumFiles(job, &files))) return NULL;
    if (IEnumBackgroundCopyFiles_Next(files, 1, &file, NULL) == S_OK)
    {
        IBackgroundCopyFile_QueryInterface(file, &IID_IBackgroundCopyFile2, (void **)&file2);
        IBackgroundCopyFile_Release(file);
    }
    IEnumBackgroundCopyFiles_Release(files);
    return file2;
}

int main(int argc, char **argv)
{
    int port = argc > 1 ? atoi(argv[1]) : 0;
    IBackgroundCopyManager *manager;
    IBackgroundCopyJob *job;
    IBackgroundCopyJob3 *job2;
    IBackgroundCopyJobHttpOptions *http;
    IEnumBackgroundCopyJobs *jobs, *clone;
    IBackgroundCopyError *error;
    IBackgroundCopyFile2 *file;
    BG_FILE_RANGE ranges[2] = {{ 2, 4 }, { 10, 3 }}, bad[2] = {{ 2, 10 }, { 5, 3 }}, *got;
    BG_ERROR_CONTEXT context;
    BG_CERT_STORE_LOCATION location;
    WCHAR url[128], dir[MAX_PATH], dst[MAX_PATH], *str, *str2;
    BYTE *hash;
    ULONG count1 = 0, count2 = 0;
    DWORD n;
    char data[64];
    HRESULT hr, code;
    HANDLE h;
    GUID id;

    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    hr = CoCreateInstance(&CLSID_BackgroundCopyManager, NULL, CLSCTX_LOCAL_SERVER, &IID_IBackgroundCopyManager, (void **)&manager);
    if (FAILED(hr)) { printf("FAIL  no BITS (%#lx)\nRESULT: FAIL\n", hr); return 1; }
    GetTempPathW(MAX_PATH, dir);
    swprintf(dst, MAX_PATH, L"%lssg_bits_ranges.bin", dir);

    /* error descriptions */
    str = NULL;
    hr = IBackgroundCopyManager_GetErrorDescription(manager, BG_E_HTTP_ERROR_404, 0x409, &str);
    printf("404: %ls\n", str ? str : L"");
    check(hr == S_OK && str && wcsstr(str, L"404"), "GetErrorDescription: BITS's own errors");
    CoTaskMemFree(str);
    str = NULL;
    hr = IBackgroundCopyManager_GetErrorDescription(manager, HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND), 0x409, &str);
    check(hr == S_OK && str && *str, "... and the system's");
    CoTaskMemFree(str);

    /* a job in error describes itself */
    IBackgroundCopyManager_CreateJob(manager, L"SG missing", BG_JOB_TYPE_DOWNLOAD, &id, &job);
    IBackgroundCopyJob_AddFile(job, L"C:\\windows\\no such file.bin", dst);
    IBackgroundCopyJob_Resume(job);
    check(wait_done(job, 20000) == BG_JOB_STATE_ERROR, "a missing source: the job is in error");
    if (SUCCEEDED(IBackgroundCopyJob_GetError(job, &error)))
    {
        str = NULL;
        hr = IBackgroundCopyError_GetErrorContextDescription(error, 0x409, &str);
        check(hr == S_OK && str && *str, "GetErrorContextDescription");
        CoTaskMemFree(str);
        str = NULL;
        hr = IBackgroundCopyError_GetProtocol(error, &str);
        printf("protocol %ls\n", str ? str : L"");
        check(hr == S_OK && str && !wcscmp(str, L"file"), "GetProtocol");
        CoTaskMemFree(str);
        str = NULL;
        hr = IBackgroundCopyError_GetErrorDescription(error, 0x409, &str);
        check(hr == S_OK && str && *str, "GetErrorDescription of the job's error");
        CoTaskMemFree(str);
        IBackgroundCopyError_Release(error);
    }
    else check(0, "GetError");
    IBackgroundCopyJob_Cancel(job);
    IBackgroundCopyJob_Release(job);

    /* clones */
    IBackgroundCopyManager_EnumJobs(manager, 0, &jobs);
    hr = IEnumBackgroundCopyJobs_Clone(jobs, &clone);
    if (hr == S_OK) { IEnumBackgroundCopyJobs_GetCount(jobs, &count1); IEnumBackgroundCopyJobs_GetCount(clone, &count2); IEnumBackgroundCopyJobs_Release(clone); }
    check(hr == S_OK && count1 == count2, "IEnumBackgroundCopyJobs::Clone");
    IEnumBackgroundCopyJobs_Release(jobs);

    /* ranges */
    IBackgroundCopyManager_CreateJob(manager, L"SG ranges", BG_JOB_TYPE_DOWNLOAD, &id, &job);
    IBackgroundCopyJob_QueryInterface(job, &IID_IBackgroundCopyJob3, (void **)&job2);
    swprintf(url, ARRAY_SIZE(url), L"http://127.0.0.1:%d/data.bin", port);
    check(IBackgroundCopyJob3_AddFileWithRanges(job2, url, dst, 2, bad) == BG_E_OVERLAPPING_RANGES,
          "overlapping ranges are refused");
    check(IBackgroundCopyJob3_AddFileWithRanges(job2, L"file://C:/x.bin", dst, 2, ranges) == BG_E_INVALID_RANGE,
          "ranges of a file:// source are refused");
    hr = IBackgroundCopyJob3_AddFileWithRanges(job2, url, dst, 2, ranges);
    check(hr == S_OK, "AddFileWithRanges");
    file = first_file(job);
    n = 0; got = NULL;
    hr = file ? IBackgroundCopyFile2_GetFileRanges(file, &n, &got) : E_FAIL;
    check(hr == S_OK && n == 2 && got && got[1].InitialOffset == 10 && got[1].Length == 3, "GetFileRanges");
    CoTaskMemFree(got);
    {
        IEnumBackgroundCopyFiles *files, *files2;
        if (SUCCEEDED(IBackgroundCopyJob_EnumFiles(job, &files)))
        {
            hr = IEnumBackgroundCopyFiles_Clone(files, &files2);
            check(hr == S_OK, "IEnumBackgroundCopyFiles::Clone");
            if (hr == S_OK) IEnumBackgroundCopyFiles_Release(files2);
            IEnumBackgroundCopyFiles_Release(files);
        }
    }
    hr = file ? IBackgroundCopyFile2_SetRemoteName(file, url) : E_FAIL;
    str = NULL;
    if (file) IBackgroundCopyFile2_GetRemoteName(file, &str);
    check(hr == S_OK && str && !wcscmp(str, url), "SetRemoteName on a suspended job");
    CoTaskMemFree(str);
    check(file && IBackgroundCopyFile2_SetRemoteName(file, L"gopher://x/y") == BG_E_PROTOCOL_NOT_AVAILABLE,
          "... an unknown protocol is refused");
    if (file) IBackgroundCopyFile2_Release(file);

    /* client certificate settings */
    IBackgroundCopyJob_QueryInterface(job, &IID_IBackgroundCopyJobHttpOptions, (void **)&http);
    if (http)
    {
        hr = IBackgroundCopyJobHttpOptions_SetClientCertificateByName(http, BG_CERT_STORE_LOCATION_CURRENT_USER, L"MY", L"sg-test");
        str = str2 = NULL; hash = NULL;
        IBackgroundCopyJobHttpOptions_GetClientCertificate(http, &location, &str, &hash, &str2);
        check(hr == S_OK && location == BG_CERT_STORE_LOCATION_CURRENT_USER && str && !wcscmp(str, L"MY") &&
              str2 && !wcscmp(str2, L"sg-test") && !hash, "client certificate by name reads back");
        CoTaskMemFree(str); CoTaskMemFree(str2); CoTaskMemFree(hash);
        check(IBackgroundCopyJobHttpOptions_RemoveClientCertificate(http) == S_OK, "RemoveClientCertificate");
        IBackgroundCopyJobHttpOptions_Release(http);
    }
    else check(0, "IBackgroundCopyJobHttpOptions");

    DeleteFileW(dst);
    IBackgroundCopyJob_Resume(job);
    check(wait_done(job, 20000) == BG_JOB_STATE_TRANSFERRED, "the ranges are transferred");
    IBackgroundCopyJob_Complete(job);
    memset(data, 0, sizeof(data));
    h = CreateFileW(dst, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    n = 0;
    if (h != INVALID_HANDLE_VALUE) { ReadFile(h, data, sizeof(data), &n, NULL); CloseHandle(h); }
    printf("downloaded %lu bytes: %.*s\n", n, (int)n, data);
    check(n == 7 && !memcmp(data, "2345:;<", 7), "... and only they, one after the other");
    DeleteFileW(dst);

    /* a status BITS has no constant for */
    IBackgroundCopyManager_CreateJob(manager, L"SG forbidden", BG_JOB_TYPE_DOWNLOAD, &id, &job);
    swprintf(url, ARRAY_SIZE(url), L"http://127.0.0.1:%d/forbidden", port);
    IBackgroundCopyJob_AddFile(job, url, dst);
    IBackgroundCopyJob_Resume(job);
    check(wait_done(job, 20000) == BG_JOB_STATE_ERROR, "a 403 puts the job in error");
    code = 0;
    if (SUCCEEDED(IBackgroundCopyJob_GetError(job, &error)))
    {
        IBackgroundCopyError_GetError(error, &context, &code);
        IBackgroundCopyError_Release(error);
    }
    printf("403 error %#lx\n", code);
    check(code == (HRESULT)0x80190193, "... as BITS's HTTP error 403");
    IBackgroundCopyJob_Cancel(job);

    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
