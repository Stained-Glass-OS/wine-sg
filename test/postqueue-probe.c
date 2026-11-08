/* A worker thread that has looked up a window handle has a message queue
 * (patches/sg/7722). PotPlayer starts its open-file worker, which calls only
 * GetParent before it signals ready; the main thread then posts the open
 * request with PostThreadMessage. With a lazily made queue the post failed,
 * the request was lost and the player stayed at "Working on it..." forever.
 *
 * Each worker below does one window-handle call (GetParent, IsWindow,
 * GetWindowLongPtrW, GetWindowRect), signals ready and waits; the main thread
 * posts it WM_APP and the worker must receive it. A worker that has done no
 * USER call has no queue: the post fails with ERROR_INVALID_THREAD_ID, as on
 * Windows, so non-GUI threads stay queue-less.
 */
#include <windows.h>
#include <stdio.h>

static int failures;
static HWND main_hwnd;

static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) failures++;
}

struct worker
{
    int kind;            /* 0 none, 1 GetParent, 2 IsWindow, 3 GetWindowLongPtrW, 4 GetWindowRect */
    HANDLE ready, go;
    LPARAM got;          /* lParam of the WM_APP received */
    BOOL gui_after_call; /* IsGUIThread(FALSE) after the call */
};

static DWORD WINAPI worker_proc(void *arg)
{
    struct worker *w = arg;
    RECT rc;
    MSG msg;

    switch (w->kind)
    {
    case 1: GetParent(main_hwnd); break;
    case 2: IsWindow(main_hwnd); break;
    case 3: GetWindowLongPtrW(main_hwnd, GWL_STYLE); break;
    case 4: GetWindowRect(main_hwnd, &rc); break;
    }
    w->gui_after_call = IsGUIThread(FALSE);
    SetEvent(w->ready);
    WaitForSingleObject(w->go, 10000);
    if (!w->kind) return 0;
    /* the request was posted before this thread ever looked at messages */
    if (MsgWaitForMultipleObjects(0, NULL, FALSE, 3000, QS_POSTMESSAGE) == WAIT_OBJECT_0 &&
        PeekMessageW(&msg, NULL, WM_APP, WM_APP, PM_REMOVE))
        w->got = msg.lParam;
    return 0;
}

static void run(int kind, const char *name)
{
    struct worker w = {kind};
    char what[200];
    HANDLE thread;
    DWORD tid, err;
    BOOL posted;

    w.ready = CreateEventW(NULL, TRUE, FALSE, NULL);
    w.go = CreateEventW(NULL, TRUE, FALSE, NULL);
    thread = CreateThread(NULL, 0, worker_proc, &w, 0, &tid);
    WaitForSingleObject(w.ready, 10000);
    SetLastError(0xdeadbeef);
    posted = PostThreadMessageW(tid, WM_APP, 0, 0x5eed + kind);
    err = GetLastError();
    SetEvent(w.go);
    WaitForSingleObject(thread, 10000);
    CloseHandle(thread);
    CloseHandle(w.ready);
    CloseHandle(w.go);

    if (kind)
    {
        snprintf(what, sizeof(what), "a worker that only called %s takes a posted message (posted %d, err %lu)", name, posted, posted ? 0 : err);
        check(posted, what);
        snprintf(what, sizeof(what), "the %s worker receives the posted request", name);
        check(w.got == 0x5eed + kind, what);
        snprintf(what, sizeof(what), "the %s worker is a GUI thread (IsGUIThread(FALSE))", name);
        check(w.gui_after_call, what);
    }
    else
    {
        snprintf(what, sizeof(what), "a worker with no USER call has no queue: post fails with ERROR_INVALID_THREAD_ID (posted %d, err %lu)", posted, err);
        check(!posted && err == ERROR_INVALID_THREAD_ID, what);
        check(!w.gui_after_call, "the no-USER worker is not a GUI thread");
    }
}

int main(void)
{
    main_hwnd = CreateWindowExW(0, L"STATIC", L"postqueue", WS_OVERLAPPEDWINDOW, 0, 0, 200, 100, NULL, NULL, NULL, NULL);
    check(main_hwnd != NULL, "the main window exists");

    run(1, "GetParent");
    run(2, "IsWindow");
    run(3, "GetWindowLongPtrW");
    run(4, "GetWindowRect");
    run(0, "nothing");

    DestroyWindow(main_hwnd);
    printf("%d failures\nRESULT: %s\n", failures, failures ? "FAIL" : "PASS");
    return failures ? 1 : 0;
}
