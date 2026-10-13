/* BITS upload and upload-reply jobs (patch 3577): argv[1] is the port of fastb-bitsup-server.py. */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <bits.h>
#include <bits1_5.h>
#include <bitsmsg.h>
#include <stdio.h>

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

static BG_JOB_STATE wait_for(IBackgroundCopyJob *job, BG_JOB_STATE a, BG_JOB_STATE b, DWORD ms)
{
    BG_JOB_STATE state = BG_JOB_STATE_QUEUED;
    DWORD start = GetTickCount();
    while (GetTickCount() - start < ms)
    {
        IBackgroundCopyJob_GetState(job, &state);
        if (state == a || state == b) break;
        Sleep(100);
    }
    return state;
}

static IBackgroundCopyManager *manager;

static IBackgroundCopyJob *new_job(BG_JOB_TYPE type, const WCHAR *url, const WCHAR *local, HRESULT *add)
{
    IBackgroundCopyJob *job = NULL;
    GUID id;
    HRESULT hr = IBackgroundCopyManager_CreateJob(manager, L"SG upload", type, &id, &job);
    if (FAILED(hr)) return NULL;
    hr = IBackgroundCopyJob_AddFile(job, url, local);
    if (add) *add = hr;
    return job;
}

static BOOL read_file(const WCHAR *name, char *buf, DWORD cap, DWORD *len)
{
    HANDLE h = CreateFileW(name, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    BOOL ok;
    if (h == INVALID_HANDLE_VALUE) return FALSE;
    ok = ReadFile(h, buf, cap, len, NULL);
    CloseHandle(h);
    return ok;
}

/* download /last into a file and compare it with the source */
static void check_uploaded(const WCHAR *port, const WCHAR *src, const WCHAR *tmp, const char *what)
{
    static char a[800000], b[800000];
    WCHAR url[100];
    DWORD la = 0, lb = 0;
    IBackgroundCopyJob *job;
    char msg[160];

    swprintf(url, 100, L"http://127.0.0.1:%ls/last", port);
    job = new_job(BG_JOB_TYPE_DOWNLOAD, url, tmp, NULL);
    IBackgroundCopyJob_Resume(job);
    wait_for(job, BG_JOB_STATE_TRANSFERRED, BG_JOB_STATE_ERROR, 30000);
    IBackgroundCopyJob_Complete(job);
    IBackgroundCopyJob_Release(job);
    read_file(src, a, sizeof(a), &la);
    read_file(tmp, b, sizeof(b), &lb);
    sprintf(msg, "%s: the server has the file (%lu of %lu bytes)", what, lb, la);
    check(la && la == lb && !memcmp(a, b, la), msg);
    DeleteFileW(tmp);
}

int main(int argc, char **argv)
{
    WCHAR port[16], dir[MAX_PATH], src[MAX_PATH], tmp[MAX_PATH], rep[MAX_PATH], url[100];
    IBackgroundCopyJob *job;
    IBackgroundCopyJob2 *job2;
    IBackgroundCopyError *err;
    BG_JOB_REPLY_PROGRESS rp;
    BG_JOB_PROGRESS prog;
    BG_JOB_STATE st;
    HRESULT hr, hr2;
    HANDLE h;
    DWORD i, w;
    BYTE *data;
    UINT64 len;
    WCHAR *str;
    char fill[1024];

    if (argc < 2) return 2;
    MultiByteToWideChar(CP_ACP, 0, argv[1], -1, port, 16);
    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    hr = CoCreateInstance(&CLSID_BackgroundCopyManager, NULL, CLSCTX_LOCAL_SERVER, &IID_IBackgroundCopyManager, (void **)&manager);
    if (FAILED(hr)) { printf("FAIL  no BITS (%#lx)\nRESULT: FAIL\n", hr); return 1; }

    GetTempPathW(MAX_PATH, dir);
    swprintf(src, MAX_PATH, L"%lssgup-src.bin", dir);
    swprintf(tmp, MAX_PATH, L"%lssgup-down.bin", dir);
    swprintf(rep, MAX_PATH, L"%lssgup-reply.bin", dir);
    /* 600000 bytes: three fragments */
    h = CreateFileW(src, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    for (i = 0; i < 600; i++)
    {
        memset(fill, 'a' + (i % 26), sizeof(fill));
        fill[0] = (char)i;
        WriteFile(h, fill, 1000, &w, NULL);
    }
    CloseHandle(h);

    /* object model rules */
    swprintf(url, 100, L"http://127.0.0.1:%ls/up", port);
    job = new_job(BG_JOB_TYPE_UPLOAD, url, src, &hr);
    check(job && hr == S_OK, "an upload job takes one http file");
    check(IBackgroundCopyJob_AddFile(job, url, src) == BG_E_TOO_MANY_FILES, "a second file: BG_E_TOO_MANY_FILES");
    IBackgroundCopyJob_Release(job);
    job = new_job(BG_JOB_TYPE_UPLOAD, L"file:///C:/x", src, &hr);
    check(hr == BG_E_PROTOCOL_NOT_AVAILABLE, "an upload to a file: URL is refused");
    IBackgroundCopyJob_Release(job);
    job = new_job(BG_JOB_TYPE_DOWNLOAD, url, tmp, NULL);
    {
        IBackgroundCopyJob2 *j2 = NULL;
        IBackgroundCopyJob_QueryInterface(job, &IID_IBackgroundCopyJob2, (void **)&j2);
        check(j2 != NULL, "IBackgroundCopyJob2");
        if (j2)
        {
            IBackgroundCopyJob2_Release(j2);
        }
        check(IBackgroundCopyJob_SetPriority(job, 99) == E_INVALIDARG, "priority out of range: E_INVALIDARG");
        check(IBackgroundCopyJob_SetNotifyFlags(job, 0x8000) == E_INVALIDARG, "unknown notify flags: E_INVALIDARG");
    }
    IBackgroundCopyJob_Release(job);

    /* a plain upload */
    swprintf(url, 100, L"http://127.0.0.1:%ls/up", port);
    job = new_job(BG_JOB_TYPE_UPLOAD, url, src, NULL);
    IBackgroundCopyJob_Resume(job);
    st = wait_for(job, BG_JOB_STATE_TRANSFERRED, BG_JOB_STATE_ERROR, 60000);
    check(st == BG_JOB_STATE_TRANSFERRED, "the upload reaches TRANSFERRED");
    memset(&prog, 0, sizeof(prog));
    IBackgroundCopyJob_GetProgress(job, &prog);
    check(prog.BytesTransferred == 600000 && prog.FilesTransferred == 1, "progress counts the bytes sent");
    hr = IBackgroundCopyJob_Complete(job);
    check(hr == S_OK, "Complete");
    check(GetFileAttributesW(src) != INVALID_FILE_ATTRIBUTES, "Complete leaves the local file in place");
    IBackgroundCopyJob_Release(job);
    check_uploaded(port, src, tmp, "plain upload");

    /* reply-less jobs refuse the reply calls */
    job = new_job(BG_JOB_TYPE_UPLOAD, url, src, NULL);
    {
        IBackgroundCopyJob2 *j2 = NULL;
        IBackgroundCopyJob_QueryInterface(job, &IID_IBackgroundCopyJob2, (void **)&j2);
        if (j2)
        {
            check(IBackgroundCopyJob2_SetReplyFileName(j2, rep) == BG_E_INVALID_STATE, "SetReplyFileName on a plain upload");
            check(IBackgroundCopyJob2_GetReplyProgress(j2, &rp) == BG_E_INVALID_STATE, "GetReplyProgress on a plain upload");
            check(IBackgroundCopyJob2_GetReplyData(j2, &data, &len) == BG_E_INVALID_STATE, "GetReplyData on a plain upload");
            IBackgroundCopyJob2_Release(j2);
        }
        else check(0, "IBackgroundCopyJob2 on upload");
    }
    /* cancel does not delete the source */
    IBackgroundCopyJob_Cancel(job);
    check(GetFileAttributesW(src) != INVALID_FILE_ATTRIBUTES, "Cancel keeps the local file of an upload");
    IBackgroundCopyJob_Release(job);

    /* the server takes half of each fragment: the rest is resent */
    swprintf(url, 100, L"http://127.0.0.1:%ls/short", port);
    job = new_job(BG_JOB_TYPE_UPLOAD, url, src, NULL);
    IBackgroundCopyJob_Resume(job);
    st = wait_for(job, BG_JOB_STATE_TRANSFERRED, BG_JOB_STATE_ERROR, 90000);
    check(st == BG_JOB_STATE_TRANSFERRED, "a server that takes parts of fragments");
    IBackgroundCopyJob_Complete(job);
    IBackgroundCopyJob_Release(job);
    check_uploaded(port, src, tmp, "resent fragments");

    /* upload with reply in memory */
    swprintf(url, 100, L"http://127.0.0.1:%ls/reply", port);
    job = new_job(BG_JOB_TYPE_UPLOAD_REPLY, url, src, NULL);
    IBackgroundCopyJob_QueryInterface(job, &IID_IBackgroundCopyJob2, (void **)&job2);
    check(job2 != NULL, "reply job has IBackgroundCopyJob2");
    str = (WCHAR *)1;
    hr = IBackgroundCopyJob2_GetReplyFileName(job2, &str);
    check(hr == S_OK && str && !*str, "no reply file name yet");
    check(IBackgroundCopyJob2_GetReplyData(job2, &data, &len) == BG_E_INVALID_STATE, "no reply data before the transfer");
    IBackgroundCopyJob_Resume(job);
    st = wait_for(job, BG_JOB_STATE_TRANSFERRED, BG_JOB_STATE_ERROR, 60000);
    check(st == BG_JOB_STATE_TRANSFERRED, "reply job transferred");
    data = NULL; len = 0;
    hr = IBackgroundCopyJob2_GetReplyData(job2, &data, &len);
    check(hr == S_OK && data && len == 12 && !memcmp(data, "REPLY:600000", 12), "the reply data is the server's answer");
    CoTaskMemFree(data);
    memset(&rp, 0, sizeof(rp));
    hr = IBackgroundCopyJob2_GetReplyProgress(job2, &rp);
    check(hr == S_OK && rp.BytesTotal == 12 && rp.BytesTransferred == 12, "reply progress");
    IBackgroundCopyJob_Complete(job);
    IBackgroundCopyJob2_Release(job2);
    IBackgroundCopyJob_Release(job);

    /* upload with a reply file */
    DeleteFileW(rep);
    job = new_job(BG_JOB_TYPE_UPLOAD_REPLY, url, src, NULL);
    IBackgroundCopyJob_QueryInterface(job, &IID_IBackgroundCopyJob2, (void **)&job2);
    check(IBackgroundCopyJob2_SetReplyFileName(job2, L"relative.bin") == E_INVALIDARG, "a relative reply file is refused");
    hr = IBackgroundCopyJob2_SetReplyFileName(job2, rep);
    check(hr == S_OK, "SetReplyFileName");
    str = NULL;
    IBackgroundCopyJob2_GetReplyFileName(job2, &str);
    check(str && !wcscmp(str, rep), "GetReplyFileName");
    CoTaskMemFree(str);
    IBackgroundCopyJob_Resume(job);
    st = wait_for(job, BG_JOB_STATE_TRANSFERRED, BG_JOB_STATE_ERROR, 60000);
    check(st == BG_JOB_STATE_TRANSFERRED, "reply-file job transferred");
    data = (BYTE *)1; len = 1;
    hr = IBackgroundCopyJob2_GetReplyData(job2, &data, &len);
    check(hr == S_OK && !data && !len, "with a reply file, GetReplyData gives nothing");
    {
        char b[64] = "";
        DWORD n = 0;
        read_file(rep, b, sizeof(b) - 1, &n);
        b[n] = 0;
        check(n == 12 && !strcmp(b, "REPLY:600000"), "the reply file has the answer");
    }
    IBackgroundCopyJob_Complete(job);
    IBackgroundCopyJob2_Release(job2);
    IBackgroundCopyJob_Release(job);
    DeleteFileW(rep);

    /* a server that is no BITS server */
    swprintf(url, 100, L"http://127.0.0.1:%ls/plain", port);
    job = new_job(BG_JOB_TYPE_UPLOAD, url, src, NULL);
    IBackgroundCopyJob_Resume(job);
    st = wait_for(job, BG_JOB_STATE_ERROR, BG_JOB_STATE_TRANSFERRED, 30000);
    check(st == BG_JOB_STATE_ERROR, "a non-BITS server: ERROR");
    err = NULL;
    hr = IBackgroundCopyJob_GetError(job, &err);
    if (hr == S_OK && err)
    {
        hr2 = 0;
        { BG_ERROR_CONTEXT c0; IBackgroundCopyError_GetError(err, &c0, &hr2); }
        check(hr2 == BG_E_REMOTE_NOT_SUPPORTED, "... with BG_E_REMOTE_NOT_SUPPORTED");
        IBackgroundCopyError_Release(err);
    }
    else check(0, "GetError");
    IBackgroundCopyJob_Cancel(job);
    IBackgroundCopyJob_Release(job);

    swprintf(url, 100, L"http://127.0.0.1:%ls/nothing", port);
    job = new_job(BG_JOB_TYPE_UPLOAD, url, src, NULL);
    IBackgroundCopyJob_Resume(job);
    st = wait_for(job, BG_JOB_STATE_ERROR, BG_JOB_STATE_TRANSFERRED, 30000);
    err = NULL;
    hr2 = 0;
    if (IBackgroundCopyJob_GetError(job, &err) == S_OK && err)
    {
        { BG_ERROR_CONTEXT c0; IBackgroundCopyError_GetError(err, &c0, &hr2); }
        IBackgroundCopyError_Release(err);
    }
    check(st == BG_JOB_STATE_ERROR && hr2 == BG_E_HTTP_ERROR_404, "an HTTP 404 on Create-Session");
    IBackgroundCopyJob_Cancel(job);
    IBackgroundCopyJob_Release(job);

    /* a missing local file */
    swprintf(url, 100, L"http://127.0.0.1:%ls/up", port);
    job = new_job(BG_JOB_TYPE_UPLOAD, url, L"C:\\no\\such\\upload.bin", NULL);
    IBackgroundCopyJob_Resume(job);
    st = wait_for(job, BG_JOB_STATE_ERROR, BG_JOB_STATE_TRANSFERRED, 30000);
    err = NULL;
    hr2 = 0;
    if (IBackgroundCopyJob_GetError(job, &err) == S_OK && err)
    {
        BG_ERROR_CONTEXT ctx = 0;
        IBackgroundCopyError_GetError(err, &ctx, &hr2);
        check(ctx == BG_ERROR_CONTEXT_LOCAL_FILE, "a missing source: the local file context");
        IBackgroundCopyError_Release(err);
    }
    check(st == BG_JOB_STATE_ERROR && (hr2 == HRESULT_FROM_WIN32(ERROR_PATH_NOT_FOUND) || hr2 == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND)), "... with the system error");
    IBackgroundCopyJob_Cancel(job);
    IBackgroundCopyJob_Release(job);

    DeleteFileW(src);
    IBackgroundCopyManager_Release(manager);
    printf("%s\n", failures ? "RESULT: FAIL" : "RESULT: PASS");
    return failures != 0;
}
