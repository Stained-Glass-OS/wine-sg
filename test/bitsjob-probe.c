/* BITS job properties and behaviour (patches/sg/1634). These were stubs:
 * GetTimes, GetOwner, SetDisplayName, priority, retry delay, no-progress
 * timeout, error count, proxy settings, TakeOwnership, the notify command
 * line, ReplaceRemotePrefix, file ACL and peer caching flags, owner
 * integrity level and elevation, maximum download time, Suspend; errors
 * were never retried, JobError never called, the notify command line never
 * run, and added files never checked. */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <bits.h>
#include <bits3_0.h>
#include <bitsmsg.h>
#include <stdio.h>

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

static int failures;

static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

static BG_JOB_STATE wait_state(IBackgroundCopyJob *job, BG_JOB_STATE want, DWORD ms)
{
    BG_JOB_STATE state = BG_JOB_STATE_QUEUED;
    DWORD start = GetTickCount();

    while (GetTickCount() - start < ms)
    {
        IBackgroundCopyJob_GetState(job, &state);
        if (state == want) break;
        Sleep(100);
    }
    return state;
}

int main(int argc, char **argv)
{
    IBackgroundCopyManager *manager;
    IBackgroundCopyJob *job, *job2, *job3;
    IBackgroundCopyJob3 *job3i;
    IBackgroundCopyJob4 *job4;
    IEnumBackgroundCopyFiles *files;
    IBackgroundCopyFile *file;
    BG_JOB_PROXY_USAGE usage;
    BG_JOB_PRIORITY prio;
    BG_JOB_TIMES times;
    BG_JOB_STATE state;
    WCHAR dir[MAX_PATH], src[MAX_PATH], dst[MAX_PATH], marker[MAX_PATH], params[MAX_PATH * 2], *str, *str2;
    ULONG value, count;
    DWORD flags;
    BOOL elevated;
    GUID id;
    HRESULT hr;
    HANDLE h;

    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    hr = CoCreateInstance(&CLSID_BackgroundCopyManager, NULL, CLSCTX_LOCAL_SERVER, &IID_IBackgroundCopyManager, (void **)&manager);
    if (FAILED(hr)) { printf("FAIL  no BITS (%#lx)\nRESULT: FAIL\n", hr); return 1; }

    hr = IBackgroundCopyManager_CreateJob(manager, L"SG BITS probe", BG_JOB_TYPE_DOWNLOAD, &id, &job);
    check(hr == S_OK, "CreateJob");
    IBackgroundCopyJob_QueryInterface(job, &IID_IBackgroundCopyJob3, (void **)&job3i);
    IBackgroundCopyJob_QueryInterface(job, &IID_IBackgroundCopyJob4, (void **)&job4);

    str = NULL;
    hr = IBackgroundCopyJob_GetOwner(job, &str);
    printf("owner %ls\n", str ? str : L"(none)");
    check(hr == S_OK && str && !wcsncmp(str, L"S-1-5-", 6), "GetOwner: the owner's SID");
    CoTaskMemFree(str);
    memset(&times, 0, sizeof(times));
    hr = IBackgroundCopyJob_GetTimes(job, &times);
    check(hr == S_OK && times.CreationTime.dwHighDateTime && times.ModificationTime.dwHighDateTime, "GetTimes");
    hr = IBackgroundCopyJob_SetDisplayName(job, L"SG renamed");
    str = NULL;
    IBackgroundCopyJob_GetDisplayName(job, &str);
    check(hr == S_OK && str && !wcscmp(str, L"SG renamed"), "SetDisplayName");
    CoTaskMemFree(str);
    hr = IBackgroundCopyJob_SetPriority(job, BG_JOB_PRIORITY_HIGH);
    prio = -1;
    IBackgroundCopyJob_GetPriority(job, &prio);
    check(hr == S_OK && prio == BG_JOB_PRIORITY_HIGH, "priority");
    IBackgroundCopyJob_SetMinimumRetryDelay(job, 45);
    value = 0;
    IBackgroundCopyJob_GetMinimumRetryDelay(job, &value);
    check(value == 45, "minimum retry delay");
    IBackgroundCopyJob_SetNoProgressTimeout(job, 1234);
    value = 0;
    IBackgroundCopyJob_GetNoProgressTimeout(job, &value);
    check(value == 1234, "no-progress timeout");
    value = 7;
    hr = IBackgroundCopyJob_GetErrorCount(job, &value);
    check(hr == S_OK && value == 0, "error count, none yet");
    hr = IBackgroundCopyJob_SetProxySettings(job, BG_JOB_PROXY_USAGE_OVERRIDE, L"proxy.example:8080", L"<local>");
    str = str2 = NULL;
    IBackgroundCopyJob_GetProxySettings(job, &usage, &str, &str2);
    check(hr == S_OK && usage == BG_JOB_PROXY_USAGE_OVERRIDE && str && !wcscmp(str, L"proxy.example:8080") &&
          str2 && !wcscmp(str2, L"<local>"), "proxy settings");
    CoTaskMemFree(str); CoTaskMemFree(str2);
    check(IBackgroundCopyJob_SetProxySettings(job, BG_JOB_PROXY_USAGE_NO_PROXY, L"x:1", NULL) == E_INVALIDARG,
          "... a proxy list without the override is refused");
    IBackgroundCopyJob_SetProxySettings(job, BG_JOB_PROXY_USAGE_PRECONFIG, NULL, NULL);
    check(IBackgroundCopyJob_TakeOwnership(job) == S_OK, "TakeOwnership");
    if (job3i)
    {
        hr = IBackgroundCopyJob3_SetFileACLFlags(job3i, BG_COPY_FILE_OWNER | BG_COPY_FILE_DACL);
        flags = 0;
        IBackgroundCopyJob3_GetFileACLFlags(job3i, &flags);
        check(hr == S_OK && flags == (BG_COPY_FILE_OWNER | BG_COPY_FILE_DACL), "file ACL flags");
    }
    if (job4)
    {
        IBackgroundCopyJob4_SetPeerCachingFlags(job4, BG_JOB_ENABLE_PEERCACHING_CLIENT);
        flags = 0;
        hr = IBackgroundCopyJob4_GetPeerCachingFlags(job4, &flags);
        check(hr == S_OK && flags == BG_JOB_ENABLE_PEERCACHING_CLIENT, "peer caching flags");
        value = 0;
        hr = IBackgroundCopyJob4_GetOwnerIntegrityLevel(job4, &value);
        check(hr == S_OK && value >= SECURITY_MANDATORY_LOW_RID, "owner integrity level");
        hr = IBackgroundCopyJob4_GetOwnerElevationState(job4, &elevated);
        check(hr == S_OK, "owner elevation state");
        IBackgroundCopyJob4_SetMaximumDownloadTime(job4, 3600);
        value = 0;
        IBackgroundCopyJob4_GetMaximumDownloadTime(job4, &value);
        check(value == 3600, "maximum download time");
    }
    else check(0, "IBackgroundCopyJob4");

    /* files */
    check(IBackgroundCopyJob_AddFile(job, L"ftp://example.org/x", L"C:\\x") == BG_E_PROTOCOL_NOT_AVAILABLE,
          "an ftp file: BG_E_PROTOCOL_NOT_AVAILABLE");
    check(IBackgroundCopyJob_AddFile(job, L"http://example.org/x", L"relative.bin") == E_INVALIDARG,
          "a relative local name is refused");
    IBackgroundCopyJob_AddFile(job, L"http://old.example/dir/a.bin", L"C:\\windows\\temp\\a.bin");
    hr = job3i ? IBackgroundCopyJob3_ReplaceRemotePrefix(job3i, L"http://old.example", L"https://new.example") : E_FAIL;
    str = NULL;
    if (SUCCEEDED(IBackgroundCopyJob_EnumFiles(job, &files)) && IEnumBackgroundCopyFiles_Next(files, 1, &file, NULL) == S_OK)
    {
        IBackgroundCopyFile_GetRemoteName(file, &str);
        IBackgroundCopyFile_Release(file);
        IEnumBackgroundCopyFiles_Release(files);
    }
    printf("remote name %ls\n", str ? str : L"");
    check(hr == S_OK && str && !wcscmp(str, L"https://new.example/dir/a.bin"), "ReplaceRemotePrefix");
    CoTaskMemFree(str);
    IBackgroundCopyJob_Cancel(job);

    /* a local copy, and the notify command line when it is done */
    GetTempPathW(MAX_PATH, dir);
    swprintf(src, MAX_PATH, L"%lssg_bits_src.txt", dir);
    swprintf(dst, MAX_PATH, L"%lssg_bits_dst.txt", dir);
    swprintf(marker, MAX_PATH, L"%lssg_bits_marker.txt", dir);
    DeleteFileW(dst); DeleteFileW(marker);
    h = CreateFileW(src, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    WriteFile(h, "bits", 4, &flags, NULL);
    CloseHandle(h);
    hr = IBackgroundCopyManager_CreateJob(manager, L"SG BITS copy", BG_JOB_TYPE_DOWNLOAD, &id, &job2);
    IBackgroundCopyJob_AddFile(job2, src, dst);
    swprintf(params, ARRAY_SIZE(params), L"cmd.exe /c echo done> \"%ls\"", marker);
    {
        IBackgroundCopyJob2 *j2 = NULL;
        IBackgroundCopyJob_QueryInterface(job2, &IID_IBackgroundCopyJob2, (void **)&j2);
        hr = j2 ? IBackgroundCopyJob2_SetNotifyCmdLine(j2, L"C:\\windows\\system32\\cmd.exe", params) : E_FAIL;
        str = str2 = NULL;
        if (j2) { IBackgroundCopyJob2_GetNotifyCmdLine(j2, &str, &str2); IBackgroundCopyJob2_Release(j2); }
    }
    check(hr == S_OK && str && !wcscmp(str, L"C:\\windows\\system32\\cmd.exe") && str2 && !wcscmp(str2, params),
          "SetNotifyCmdLine reads back");
    CoTaskMemFree(str); CoTaskMemFree(str2);
    IBackgroundCopyJob_Resume(job2);
    state = wait_state(job2, BG_JOB_STATE_TRANSFERRED, 20000);
    check(state == BG_JOB_STATE_TRANSFERRED, "the local copy is transferred");
    for (count = 0; count < 50 && GetFileAttributesW(marker) == INVALID_FILE_ATTRIBUTES; count++) Sleep(100);
    check(GetFileAttributesW(marker) != INVALID_FILE_ATTRIBUTES, "... and the notify command line ran");
    memset(&times, 0, sizeof(times));
    IBackgroundCopyJob_GetTimes(job2, &times);
    check(times.TransferCompletionTime.dwHighDateTime != 0, "... and the completion time is kept");
    IBackgroundCopyJob_Complete(job2);
    check(IBackgroundCopyJob_Suspend(job2) == BG_E_INVALID_STATE, "Suspend of a completed job: BG_E_INVALID_STATE");

    /* a refused connection is retried, then gives up when there is no progress */
    hr = IBackgroundCopyManager_CreateJob(manager, L"SG BITS retry", BG_JOB_TYPE_DOWNLOAD, &id, &job3);
    IBackgroundCopyJob_AddFile(job3, L"http://127.0.0.1:9/nothing", dst);
    IBackgroundCopyJob_SetMinimumRetryDelay(job3, 1);
    IBackgroundCopyJob_SetNoProgressTimeout(job3, 4);
    DeleteFileW(marker);
    {
        IBackgroundCopyJob2 *j2 = NULL;
        IBackgroundCopyJob_QueryInterface(job3, &IID_IBackgroundCopyJob2, (void **)&j2);
        if (j2) { IBackgroundCopyJob2_SetNotifyCmdLine(j2, L"C:\\windows\\system32\\cmd.exe", params); IBackgroundCopyJob2_Release(j2); }
    }
    IBackgroundCopyJob_Resume(job3);
    state = wait_state(job3, BG_JOB_STATE_TRANSIENT_ERROR, 15000);
    printf("after the first try: state %d\n", state);
    check(state == BG_JOB_STATE_TRANSIENT_ERROR, "a refused connection is a transient error");
    hr = IBackgroundCopyJob_Suspend(job3);
    check(hr == S_OK && wait_state(job3, BG_JOB_STATE_SUSPENDED, 2000) == BG_JOB_STATE_SUSPENDED, "Suspend");
    IBackgroundCopyJob_Resume(job3);
    state = wait_state(job3, BG_JOB_STATE_ERROR, 20000);
    value = 0;
    IBackgroundCopyJob_GetErrorCount(job3, &value);
    printf("final state %d, errors %lu\n", state, value);
    check(state == BG_JOB_STATE_ERROR && value >= 2, "... retried, then in error after the no-progress timeout");
    for (count = 0; count < 50 && GetFileAttributesW(marker) == INVALID_FILE_ATTRIBUTES; count++) Sleep(100);
    check(GetFileAttributesW(marker) != INVALID_FILE_ATTRIBUTES, "... and the notify command line ran for the error");
    IBackgroundCopyJob_Cancel(job3);

    DeleteFileW(src); DeleteFileW(dst); DeleteFileW(marker);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
