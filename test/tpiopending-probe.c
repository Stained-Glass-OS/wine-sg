/* tpiopending-probe: a thread-pool worker that has an overlapped read pending
 * must not retire (and so cancel the read) when it goes idle.
 *
 * On Windows a thread's exit cancels the I/O it started (unless the file is
 * bound to a completion port), and the thread pool therefore keeps a worker
 * alive while ThreadIsIoPending says it still has I/O in flight. Qt 6's
 * QProcess (QWindowsPipeReader) relies on that: it re-issues ReadFile on the
 * child's stdout/stderr pipe from inside its thread-pool wait callback. Wine
 * retired idle workers after 5 s regardless, the server cancelled the read,
 * Qt took the pipe for broken, and digiKam waited for ever for exiftool's
 * answer (qa4, 2026-11-23).
 *
 * Rows:
 *   1 ThreadIsIoPending is FALSE for a thread with nothing in flight
 *   2 ThreadIsIoPending is TRUE for the worker while its read is pending
 *   3 after 8 s idle the worker's read is still pending and completes with
 *     the data written then (the bug: cancelled, ERROR_OPERATION_ABORTED)
 *   4 control: a plain thread that exits with a read pending has it
 *     cancelled -- the Windows rule the fix must keep
 */
#include <windows.h>
#include <winternl.h>
#include <stdio.h>
#include <string.h>

#define ThreadIsIoPendingClass 16
typedef NTSTATUS (WINAPI *pNtQIT)(HANDLE, int, void *, ULONG, ULONG *);
static pNtQIT pNtQueryInformationThread;

static int failures;
static void row(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) failures++;
}

struct reader
{
    HANDLE srv, cli;
    OVERLAPPED ov;
    char buf[64];
    DWORD tid;
    BOOL pending;
};

static BOOL make_pipe(struct reader *r, int n)
{
    char name[128];
    sprintf(name, "\\\\.\\pipe\\sg-tpiopending-%lu-%d", GetCurrentProcessId(), n);
    r->srv = CreateNamedPipeA(name, PIPE_ACCESS_INBOUND | FILE_FLAG_OVERLAPPED,
                              PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT, 1, 4096, 4096, 0, NULL);
    if (r->srv == INVALID_HANDLE_VALUE) return FALSE;
    r->cli = CreateFileA(name, GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    if (r->cli == INVALID_HANDLE_VALUE) return FALSE;
    memset(&r->ov, 0, sizeof(r->ov));
    r->ov.hEvent = CreateEventA(NULL, FALSE, FALSE, NULL);  /* auto-reset, as Qt's */
    return TRUE;
}

static void start_read(struct reader *r)
{
    r->tid = GetCurrentThreadId();
    r->pending = !ReadFile(r->srv, r->buf, sizeof(r->buf), NULL, &r->ov)
                 && GetLastError() == ERROR_IO_PENDING;
}

static int io_pending(DWORD tid)
{
    BOOL val = 2;
    HANDLE h = OpenThread(THREAD_QUERY_INFORMATION, FALSE, tid);
    NTSTATUS st;
    if (!h) return -1;  /* the thread is gone */
    st = pNtQueryInformationThread(h, ThreadIsIoPendingClass, &val, sizeof(val), NULL);
    CloseHandle(h);
    if (st) return -2;
    return val ? 1 : 0;
}

/* pool workers: the reader goes idle first, its siblings 1.5 s later, so the
 * reader is the first whose idle time runs out and the one the pool retires */
static struct reader pool_rd;
static volatile LONG started;
static void CALLBACK work_cb(PTP_CALLBACK_INSTANCE inst, void *ctx, PTP_WORK work)
{
    int me = InterlockedIncrement(&started);
    while (started < 4) Sleep(10);  /* four distinct workers */
    if (me == 1) start_read(&pool_rd);
    else Sleep(1500);
}

static struct reader thr_rd;
static DWORD WINAPI thread_proc(void *arg)
{
    start_read(&thr_rd);
    return 0;
}

int main(int argc, char **argv)
{
    DWORD idle = argc > 1 ? atoi(argv[1]) : 8000, n = 0, tid, err;
    PTP_WORK work[4];
    HANDLE th;
    BOOL ok;
    int i, p;

    setvbuf(stdout, NULL, _IONBF, 0);
    pNtQueryInformationThread = (pNtQIT)GetProcAddress(GetModuleHandleA("ntdll.dll"), "NtQueryInformationThread");

    /* 1 */
    th = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)Sleep, (void *)5000, 0, &tid);
    Sleep(100);
    p = io_pending(tid);
    printf("   idle thread: ThreadIsIoPending=%d\n", p);
    row(p == 0, "ThreadIsIoPending is FALSE for a thread with no I/O in flight");

    /* 2, 3 */
    if (!make_pipe(&pool_rd, 1)) { printf("FAIL  pipe: %lu\n", GetLastError()); return 1; }
    /* four work objects: one object submitted four times runs its callbacks
     * one after another on fewer threads */
    for (i = 0; i < 4; i++)
    {
        work[i] = CreateThreadpoolWork(work_cb, NULL, NULL);
        SubmitThreadpoolWork(work[i]);
    }
    for (i = 0; i < 4; i++) WaitForThreadpoolWorkCallbacks(work[i], FALSE);
    printf("   read issued on pool worker %04lx, pending=%d\n", pool_rd.tid, pool_rd.pending);
    if (!pool_rd.pending) { printf("FAIL  the read did not pend\n"); return 1; }
    p = io_pending(pool_rd.tid);
    printf("   worker: ThreadIsIoPending=%d\n", p);
    row(p == 1, "ThreadIsIoPending is TRUE for the worker while its read is pending");

    Sleep(idle);
    p = io_pending(pool_rd.tid);
    printf("   after %lu ms idle: worker %s\n", idle, p == -1 ? "has exited" : "is alive");
    WriteFile(pool_rd.cli, "exiftool {ready}", 16, &n, NULL);
    WaitForSingleObject(pool_rd.ov.hEvent, 5000);
    ok = GetOverlappedResult(pool_rd.srv, &pool_rd.ov, &n, FALSE);
    err = ok ? 0 : GetLastError();
    printf("   pool read: ok=%d bytes=%lu error=%lu\n", ok, n, err);
    row(ok && n == 16 && !memcmp(pool_rd.buf, "exiftool {ready}", 16),
        "an idle pool worker keeps its pending read; the data written later arrives");
    for (i = 0; i < 4; i++) CloseThreadpoolWork(work[i]);

    /* 4 */
    if (!make_pipe(&thr_rd, 2)) { printf("FAIL  pipe 2: %lu\n", GetLastError()); return 1; }
    th = CreateThread(NULL, 0, thread_proc, NULL, 0, &tid);
    WaitForSingleObject(th, 5000);
    CloseHandle(th);
    WaitForSingleObject(thr_rd.ov.hEvent, 5000);
    ok = GetOverlappedResult(thr_rd.srv, &thr_rd.ov, &n, FALSE);
    err = ok ? 0 : GetLastError();
    printf("   plain thread exited: ok=%d error=%lu\n", ok, err);
    row(thr_rd.pending && !ok && err == ERROR_OPERATION_ABORTED,
        "a plain thread's exit still cancels its pending read (as on Windows)");

    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures ? 1 : 0;
}
