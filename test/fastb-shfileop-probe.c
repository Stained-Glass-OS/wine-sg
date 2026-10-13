/* IFileOperation CopyItem/DeleteItem/RenameItem/NewItem and the *Items variants (patch 3599). */
#define COBJMACROS
#define INITGUID
#include <windows.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <stdio.h>
#include <string.h>

static int fails, checks;
#define CHECK(c, ...) do { checks++; if (!(c)) { fails++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static char log_buf[8192];
static void logf_(const char *fmt, ...)
{
    va_list ap; char tmp[512];
    va_start(ap, fmt); vsnprintf(tmp, sizeof(tmp), fmt, ap); va_end(ap);
    strcat(log_buf, tmp); strcat(log_buf, ";");
}

static const char *base_name(IShellItem *item)
{
    static char out[4][MAX_PATH]; static int n;
    WCHAR *s; char *o = out[n++ & 3];
    o[0] = 0;
    if (item && SUCCEEDED(IShellItem_GetDisplayName(item, SIGDN_FILESYSPATH, &s)))
    {
        WCHAR *b = wcsrchr(s, '\\');
        WideCharToMultiByte(CP_ACP, 0, b ? b + 1 : s, -1, o, MAX_PATH, NULL, NULL);
        CoTaskMemFree(s);
    }
    return o;
}
static const char *nm(const WCHAR *w)
{
    static char out[4][MAX_PATH]; static int n; char *o = out[n++ & 3];
    o[0] = 0;
    if (w) WideCharToMultiByte(CP_ACP, 0, w, -1, o, MAX_PATH, NULL, NULL);
    return o;
}

typedef struct { IFileOperationProgressSink iface; LONG ref; } Sink;
static HRESULT WINAPI s_QI(IFileOperationProgressSink *i, REFIID r, void **o)
{
    if (IsEqualIID(r, &IID_IUnknown) || IsEqualIID(r, &IID_IFileOperationProgressSink)) { *o = i; return S_OK; }
    *o = NULL; return E_NOINTERFACE;
}
static ULONG WINAPI s_AddRef(IFileOperationProgressSink *i) { return 2; }
static ULONG WINAPI s_Release(IFileOperationProgressSink *i) { return 1; }
static HRESULT WINAPI s_Start(IFileOperationProgressSink *i) { logf_("Start"); return S_OK; }
static HRESULT WINAPI s_Finish(IFileOperationProgressSink *i, HRESULT hr) { logf_("Finish"); return S_OK; }
static HRESULT WINAPI s_PreRename(IFileOperationProgressSink *i, DWORD f, IShellItem *it, LPCWSTR n) { logf_("PreRename %s>%s", base_name(it), nm(n)); return S_OK; }
static HRESULT WINAPI s_PostRename(IFileOperationProgressSink *i, DWORD f, IShellItem *it, LPCWSTR n, HRESULT hr, IShellItem *nw) { logf_("PostRename %s>%s %s %d", base_name(it), nm(n), base_name(nw), SUCCEEDED(hr)); return S_OK; }
static HRESULT WINAPI s_PreMove(IFileOperationProgressSink *i, DWORD f, IShellItem *it, IShellItem *d, LPCWSTR n) { logf_("PreMove %s", base_name(it)); return S_OK; }
static HRESULT WINAPI s_PostMove(IFileOperationProgressSink *i, DWORD f, IShellItem *it, IShellItem *d, LPCWSTR n, HRESULT hr, IShellItem *nw) { logf_("PostMove %s %s %d", base_name(it), base_name(nw), SUCCEEDED(hr)); return S_OK; }
static HRESULT WINAPI s_PreCopy(IFileOperationProgressSink *i, DWORD f, IShellItem *it, IShellItem *d, LPCWSTR n) { logf_("PreCopy %s>%s/%s", base_name(it), base_name(d), nm(n)); return S_OK; }
static HRESULT WINAPI s_PostCopy(IFileOperationProgressSink *i, DWORD f, IShellItem *it, IShellItem *d, LPCWSTR n, HRESULT hr, IShellItem *nw) { logf_("PostCopy %s %s %d", base_name(it), base_name(nw), SUCCEEDED(hr)); return S_OK; }
static HRESULT WINAPI s_PreDelete(IFileOperationProgressSink *i, DWORD f, IShellItem *it) { logf_("PreDelete %s", base_name(it)); return S_OK; }
static HRESULT WINAPI s_PostDelete(IFileOperationProgressSink *i, DWORD f, IShellItem *it, HRESULT hr, IShellItem *nw) { logf_("PostDelete %s %d", base_name(it), SUCCEEDED(hr)); return S_OK; }
static HRESULT WINAPI s_PreNew(IFileOperationProgressSink *i, DWORD f, IShellItem *d, LPCWSTR n) { logf_("PreNew %s", nm(n)); return S_OK; }
static HRESULT WINAPI s_PostNew(IFileOperationProgressSink *i, DWORD f, IShellItem *d, LPCWSTR n, LPCWSTR t, DWORD a, HRESULT hr, IShellItem *nw) { logf_("PostNew %s %s %d", nm(n), base_name(nw), SUCCEEDED(hr)); return S_OK; }
static HRESULT WINAPI s_Update(IFileOperationProgressSink *i, UINT a, UINT b) { return S_OK; }
static HRESULT WINAPI s_Reset(IFileOperationProgressSink *i) { return S_OK; }
static HRESULT WINAPI s_Pause(IFileOperationProgressSink *i) { return S_OK; }
static HRESULT WINAPI s_Resume(IFileOperationProgressSink *i) { return S_OK; }
static IFileOperationProgressSinkVtbl sink_vtbl = {
    s_QI, s_AddRef, s_Release, s_Start, s_Finish, s_PreRename, s_PostRename, s_PreMove, s_PostMove,
    s_PreCopy, s_PostCopy, s_PreDelete, s_PostDelete, s_PreNew, s_PostNew, s_Update, s_Reset, s_Pause, s_Resume };
static IFileOperationProgressSink sink = { &sink_vtbl };

static IShellItem *item_for(const WCHAR *path)
{
    IShellItem *it = NULL;
    HRESULT hr = SHCreateItemFromParsingName(path, NULL, &IID_IShellItem, (void **)&it);
    CHECK(hr == S_OK, "SHCreateItemFromParsingName %ls: %lx", path, hr);
    return it;
}
static BOOL exists(const WCHAR *p) { return GetFileAttributesW(p) != INVALID_FILE_ATTRIBUTES; }
static void write_file(const WCHAR *p, const char *data)
{
    DWORD n; HANDLE f = CreateFileW(p, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    WriteFile(f, data, strlen(data), &n, NULL); CloseHandle(f);
}
static BOOL file_is(const WCHAR *p, const char *data)
{
    char buf[64] = {0}; DWORD n = 0;
    HANDLE f = CreateFileW(p, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (f == INVALID_HANDLE_VALUE) return FALSE;
    ReadFile(f, buf, sizeof(buf) - 1, &n, NULL); CloseHandle(f);
    return !strcmp(buf, data);
}
static IFileOperation *new_op(void)
{
    IFileOperation *op = NULL;
    HRESULT hr = CoCreateInstance(&CLSID_FileOperation, NULL, CLSCTX_INPROC_SERVER, &IID_IFileOperation, (void **)&op);
    CHECK(hr == S_OK, "CoCreateInstance FileOperation %lx", hr);
    if (op) IFileOperation_SetOperationFlags(op, FOF_NO_UI);
    return op;
}
static void run(IFileOperation *op, const char *expect_log, const char *what)
{
    HRESULT hr;
    log_buf[0] = 0;
    hr = IFileOperation_PerformOperations(op);
    CHECK(hr == S_OK, "%s: PerformOperations %lx", what, hr);
    CHECK(!strcmp(log_buf, expect_log), "%s: events\n  got  %s\n  want %s", what, log_buf, expect_log);
    IFileOperation_Release(op);
}

int main(void)
{
    WCHAR base[MAX_PATH], p1[MAX_PATH], p2[MAX_PATH], p3[MAX_PATH], tmp[MAX_PATH];
    IFileOperation *op;
    IShellItem *base_it, *a, *b;
    HRESULT hr;
    BOOL aborted;

    CoInitialize(NULL);
    GetTempPathW(MAX_PATH, tmp);
    swprintf(base, MAX_PATH, L"%lssgfileop%lu", tmp, GetCurrentProcessId());
    {
        SHFILEOPSTRUCTW fo = {0};
        WCHAR from[MAX_PATH + 1] = {0};
        wcscpy(from, base);
        fo.wFunc = FO_DELETE; fo.pFrom = from; fo.fFlags = FOF_NO_UI;
        SHFileOperationW(&fo);
    }
    CreateDirectoryW(base, NULL);
    base_it = item_for(base);
    if (!base_it) { printf("RESULT: FAIL\n"); return 1; }

    /* NewItem: folder and file */
    op = new_op();
    hr = IFileOperation_NewItem(op, base_it, FILE_ATTRIBUTE_DIRECTORY, L"d1", NULL, &sink);
    CHECK(hr == S_OK, "NewItem dir %lx", hr);
    hr = IFileOperation_NewItem(op, base_it, FILE_ATTRIBUTE_NORMAL, L"f1.txt", NULL, &sink);
    CHECK(hr == S_OK, "NewItem file %lx", hr);
    CHECK(IFileOperation_NewItem(op, NULL, 0, L"x", NULL, NULL) == E_INVALIDARG, "NewItem NULL folder");
    run(op, "PreNew d1;PostNew d1 d1 1;PreNew f1.txt;PostNew f1.txt f1.txt 1;", "NewItem");
    swprintf(p1, MAX_PATH, L"%ls\\d1", base);
    CHECK((GetFileAttributesW(p1) & FILE_ATTRIBUTE_DIRECTORY) != 0, "d1 is a directory");
    swprintf(p2, MAX_PATH, L"%ls\\f1.txt", base);
    CHECK(exists(p2) && !(GetFileAttributesW(p2) & FILE_ATTRIBUTE_DIRECTORY), "f1.txt exists");
    write_file(p2, "hello");

    /* CopyItem: file with a new name, into d1; directory with content */
    a = item_for(p2);
    b = item_for(p1);
    op = new_op();
    hr = IFileOperation_CopyItem(op, a, b, L"copy.txt", &sink);
    CHECK(hr == S_OK, "CopyItem %lx", hr);
    CHECK(IFileOperation_CopyItem(op, NULL, b, NULL, NULL) == E_INVALIDARG, "CopyItem NULL item");
    CHECK(IFileOperation_CopyItem(op, a, NULL, NULL, NULL) == E_INVALIDARG, "CopyItem NULL folder");
    run(op, "PreCopy f1.txt>d1/copy.txt;PostCopy f1.txt copy.txt 1;", "CopyItem");
    swprintf(p3, MAX_PATH, L"%ls\\d1\\copy.txt", base);
    CHECK(file_is(p3, "hello"), "copied content");
    CHECK(file_is(p2, "hello"), "source kept");

    swprintf(p3, MAX_PATH, L"%ls\\d1\\sub", base);
    CreateDirectoryW(p3, NULL);
    swprintf(p3, MAX_PATH, L"%ls\\d1\\sub\\in.txt", base);
    write_file(p3, "inner");
    IShellItem_Release(a);
    swprintf(p3, MAX_PATH, L"%ls\\d1\\sub", base);
    a = item_for(p3);
    op = new_op();
    hr = IFileOperation_CopyItem(op, a, base_it, L"subcopy", NULL);
    CHECK(hr == S_OK, "CopyItem dir %lx", hr);
    run(op, "", "CopyItem dir (no sink events: none registered)");
    swprintf(p3, MAX_PATH, L"%ls\\subcopy\\in.txt", base);
    CHECK(file_is(p3, "inner"), "directory copied with content");

    /* RenameItem */
    IShellItem_Release(a);
    a = item_for(p2);
    op = new_op();
    hr = IFileOperation_RenameItem(op, a, L"g1.txt", &sink);
    CHECK(hr == S_OK, "RenameItem %lx", hr);
    CHECK(IFileOperation_RenameItem(op, a, NULL, NULL) == E_INVALIDARG, "RenameItem NULL name");
    run(op, "PreRename f1.txt>g1.txt;PostRename f1.txt>g1.txt g1.txt 1;", "RenameItem");
    swprintf(p3, MAX_PATH, L"%ls\\g1.txt", base);
    CHECK(file_is(p3, "hello") && !exists(p2), "renamed");

    /* MoveItems / CopyItems / DeleteItems with an IShellItemArray */
    {
        IShellItemArray *arr = NULL;
        PIDLIST_ABSOLUTE pidl[2];
        WCHAR q1[MAX_PATH], q2[MAX_PATH];
        swprintf(q1, MAX_PATH, L"%ls\\d1\\copy.txt", base);
        swprintf(q2, MAX_PATH, L"%ls\\g1.txt", base);
        pidl[0] = ILCreateFromPathW(q1); pidl[1] = ILCreateFromPathW(q2);
        hr = SHCreateShellItemArrayFromIDLists(2, (PCIDLIST_ABSOLUTE_ARRAY)pidl, &arr);
        CHECK(hr == S_OK, "item array %lx", hr);
        ILFree(pidl[0]); ILFree(pidl[1]);
        if (arr)
        {
            IShellItem_Release(b);
            swprintf(p1, MAX_PATH, L"%ls\\subcopy", base);
            b = item_for(p1);
            op = new_op();
            hr = IFileOperation_CopyItems(op, (IUnknown *)arr, b);
            CHECK(hr == S_OK, "CopyItems %lx", hr);
            run(op, "", "CopyItems");
            swprintf(p3, MAX_PATH, L"%ls\\subcopy\\copy.txt", base);
            CHECK(file_is(p3, "hello"), "CopyItems copy.txt");
            swprintf(p3, MAX_PATH, L"%ls\\subcopy\\g1.txt", base);
            CHECK(file_is(p3, "hello"), "CopyItems g1.txt");

            op = new_op();
            hr = IFileOperation_DeleteItems(op, (IUnknown *)arr);
            CHECK(hr == S_OK, "DeleteItems %lx", hr);
            run(op, "", "DeleteItems");
            CHECK(!exists(q1) && !exists(q2), "DeleteItems removed both");
            IShellItemArray_Release(arr);
        }
    }

    /* DeleteItem on a directory, and a failing copy marks the operation aborted */
    IShellItem_Release(a);
    swprintf(p1, MAX_PATH, L"%ls\\subcopy", base);
    a = item_for(p1);
    op = new_op();
    hr = IFileOperation_DeleteItem(op, a, &sink);
    CHECK(hr == S_OK, "DeleteItem %lx", hr);
    run(op, "PreDelete subcopy;PostDelete subcopy 1;", "DeleteItem");
    CHECK(!exists(p1), "directory deleted");

    op = new_op();
    hr = IFileOperation_CopyItem(op, a, base_it, L"never", &sink);
    CHECK(hr == S_OK, "CopyItem of a missing item queues %lx", hr);
    log_buf[0] = 0;
    IFileOperation_PerformOperations(op);
    CHECK(strstr(log_buf, "PostCopy subcopy  0") != NULL, "failed copy reports failure: %s", log_buf);
    aborted = FALSE;
    hr = IFileOperation_GetAnyOperationsAborted(op, &aborted);
    CHECK(hr == S_OK && aborted, "aborted after failure: %lx %d", hr, aborted);
    IFileOperation_Release(op);

    /* progress setters accept values */
    op = new_op();
    CHECK(IFileOperation_SetProgressMessage(op, L"copying") == S_OK, "SetProgressMessage");
    CHECK(IFileOperation_SetOwnerWindow(op, NULL) == S_OK, "SetOwnerWindow");
    CHECK(IFileOperation_SetProgressDialog(op, NULL) == S_OK, "SetProgressDialog");
    IFileOperation_Release(op);

    IShellItem_Release(a); IShellItem_Release(b); IShellItem_Release(base_it);
    {
        SHFILEOPSTRUCTW fo = {0};
        WCHAR from[MAX_PATH + 1] = {0};
        wcscpy(from, base);
        fo.wFunc = FO_DELETE; fo.pFrom = from; fo.fFlags = FOF_NO_UI;
        SHFileOperationW(&fo);
    }
    printf("%d checks, %d failures\n", checks, fails);
    printf("RESULT: %s\n", fails ? "FAIL" : "PASS");
    return fails != 0;
}
