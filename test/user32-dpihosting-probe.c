/* Probe for patches/sg/5205: GetWindowDpiHostingBehavior is the hosting behavior of the thread
 * that created the window at that time, permanent afterwards, visible to the window's own
 * WM_NCCREATE, per thread, and DPI_HOSTING_BEHAVIOR_INVALID for a bad handle.
 * Table driven; last line "RESULT: PASS|FAIL". */
#include <windows.h>
#include <stdio.h>

static int failures;
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

typedef int (WINAPI *get_win_t)(HWND);
typedef int (WINAPI *set_thread_t)(int);
typedef int (WINAPI *get_thread_t)(void);
static get_win_t pGetWindowDpiHostingBehavior;
static set_thread_t pSetThreadDpiHostingBehavior;
static get_thread_t pGetThreadDpiHostingBehavior;
static int seen_in_create = -100;

static LRESULT CALLBACK proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_NCCREATE) seen_in_create = pGetWindowDpiHostingBehavior(hwnd);
    return DefWindowProcA(hwnd, msg, wp, lp);
}

static HWND make(void)
{
    return CreateWindowA("SgDpiHost", "t", WS_OVERLAPPED, 0, 0, 10, 10, NULL, NULL, GetModuleHandleA(NULL), NULL);
}

static HANDLE created_event, done_event;

static DWORD WINAPI thread_main(void *arg)
{
    HWND *out = arg;
    *out = make();
    SetEvent(created_event);
    WaitForSingleObject(done_event, 10000);   /* a window dies with its thread */
    return 0;
}

int main(void)
{
    static const struct { int set; int expect; } steps[] = { {0, 0}, {1, 1}, {0, 0}, {1, 1} };
    HMODULE u32 = GetModuleHandleA("user32.dll");
    WNDCLASSA wc = {0};
    HWND wins[4], other = NULL, bad;
    HANDLE th;
    unsigned int i;
    int prev, got;

    pGetWindowDpiHostingBehavior = (get_win_t)GetProcAddress(u32, "GetWindowDpiHostingBehavior");
    pSetThreadDpiHostingBehavior = (set_thread_t)GetProcAddress(u32, "SetThreadDpiHostingBehavior");
    pGetThreadDpiHostingBehavior = (get_thread_t)GetProcAddress(u32, "GetThreadDpiHostingBehavior");
    if (!pGetWindowDpiHostingBehavior || !pSetThreadDpiHostingBehavior)
    {
        printf("ok   not available\nRESULT: PASS\n");
        return 0;
    }
    wc.lpfnWndProc = proc; wc.hInstance = GetModuleHandleA(NULL); wc.lpszClassName = "SgDpiHost";
    RegisterClassA(&wc);

    for (i = 0; i < ARRAYSIZE(steps); i++)
    {
        prev = pSetThreadDpiHostingBehavior(steps[i].set);
        check(prev == (i ? steps[i - 1].set : 0), "step %u: SetThreadDpiHostingBehavior(%d) returned %d", i, steps[i].set, prev);
        seen_in_create = -100;
        wins[i] = make();
        check(wins[i] != NULL, "step %u: window created", i);
        got = pGetWindowDpiHostingBehavior(wins[i]);
        check(got == steps[i].expect, "step %u: window made with thread behavior %d is %d", i, steps[i].set, got);
        check(seen_in_create == steps[i].expect, "step %u: WM_NCCREATE already sees %d", i, seen_in_create);
    }
    /* the thread is MIXED now: earlier windows keep what they were made with */
    for (i = 0; i < ARRAYSIZE(steps); i++)
    {
        got = pGetWindowDpiHostingBehavior(wins[i]);
        check(got == steps[i].expect, "window %u is still %d (%d)", i, steps[i].expect, got);
    }
    /* the other thread has its own behavior: default */
    created_event = CreateEventA(NULL, FALSE, FALSE, NULL);
    done_event = CreateEventA(NULL, FALSE, FALSE, NULL);
    th = CreateThread(NULL, 0, thread_main, &other, 0, NULL);
    WaitForSingleObject(created_event, 10000);
    check(other != NULL, "window created by another thread");
    got = pGetWindowDpiHostingBehavior(other);
    check(got == 0, "the other thread's window is DEFAULT, not this thread's MIXED (%d)", got);
    SetEvent(done_event);
    WaitForSingleObject(th, 10000);
    CloseHandle(th);
    check(pGetThreadDpiHostingBehavior() == 1, "this thread is still MIXED");

    bad = (HWND)0xdeadbeef;
    SetLastError(0xdeadbeef);
    got = pGetWindowDpiHostingBehavior(bad);
    check(got == -1 && GetLastError() == ERROR_INVALID_WINDOW_HANDLE, "invalid handle: %d, error %lu", got, GetLastError());
    SetLastError(0xdeadbeef);
    got = pGetWindowDpiHostingBehavior(NULL);
    check(got == -1, "NULL handle: %d", got);

    pSetThreadDpiHostingBehavior(0);
    for (i = 0; i < ARRAYSIZE(steps); i++) DestroyWindow(wins[i]);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
