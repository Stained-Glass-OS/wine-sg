/* combase CoWaitForMultipleObjects (patches/sg/1712): its CWMO_* flags are not
 * CoWaitForMultipleHandles' COWAIT_* flags. CWMO_DISPATCH_CALLS (1) must not
 * become COWAIT_WAITALL, CWMO_DISPATCH_WINDOW_MESSAGES (2) must not become
 * COWAIT_ALERTABLE, and CWMO_DEFAULT dispatches nothing, even in an STA. */
#include <windows.h>
#include <objbase.h>
#include <stdio.h>

static int fails;
#define CHECK(c, ...) do { if (c) printf("PASS  %s\n", #c); else { printf("FAIL  %s: ", #c); printf(__VA_ARGS__); printf("\n"); fails++; } } while (0)

#define CWMO_DEFAULT 0
#define CWMO_DISPATCH_CALLS 1
#define CWMO_DISPATCH_WINDOW_MESSAGES 2
#define RPC_S_CALLPENDING_HR ((HRESULT)0x80010115)

static HRESULT (WINAPI *pWait)(DWORD, DWORD, ULONG, const HANDLE *, DWORD *);
static volatile LONG sent_seen, apc_ran;
static HWND win;

static LRESULT CALLBACK wndproc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_USER + 7) { InterlockedIncrement(&sent_seen); return 42; }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

static DWORD WINAPI sender(void *arg)
{
    DWORD_PTR res = 0;
    Sleep(50);
    SendMessageTimeoutW(win, WM_USER + 7, 0, 0, SMTO_NORMAL, 2000, &res);
    return 0;
}

static DWORD WINAPI setter(void *arg)
{
    Sleep(50);
    SetEvent((HANDLE)arg);
    return 0;
}

static void CALLBACK apc(ULONG_PTR p) { InterlockedIncrement(&apc_ran); }

/* two events, only the second gets signalled: the wait must end on it (any, not all) */
static void test_any(DWORD flags, const char *what)
{
    HANDLE ev[2], th;
    DWORD idx = 0xdead, t0;
    HRESULT hr;

    ev[0] = CreateEventW(NULL, TRUE, FALSE, NULL);
    ev[1] = CreateEventW(NULL, TRUE, FALSE, NULL);
    th = CreateThread(NULL, 0, setter, ev[1], 0, NULL);
    t0 = GetTickCount();
    hr = pWait(flags, 3000, 2, ev, &idx);
    CHECK(hr == S_OK && idx == 1 && GetTickCount() - t0 < 2000, "%s: hr %#lx index %lu after %lu ms",
          what, hr, idx, GetTickCount() - t0);
    WaitForSingleObject(th, INFINITE);
    CloseHandle(th); CloseHandle(ev[0]); CloseHandle(ev[1]);
}

/* a message sent from another thread during the wait: dispatched or not */
static void test_sent(DWORD flags, BOOL dispatched, const char *what)
{
    HANDLE ev = CreateEventW(NULL, TRUE, FALSE, NULL), th;
    DWORD idx;
    HRESULT hr;
    LONG during;

    sent_seen = 0;
    th = CreateThread(NULL, 0, sender, NULL, 0, NULL);
    hr = pWait(flags, 500, 1, &ev, &idx);
    during = sent_seen;
    CHECK(hr == RPC_S_CALLPENDING_HR, "%s: times out: %#lx", what, hr);
    CHECK(dispatched ? during == 1 : during == 0, "%s: sent message handled during the wait %ld time(s)", what, during);
    /* let the sender finish either way */
    while (WaitForSingleObject(th, 0) == WAIT_TIMEOUT)
    {
        MSG msg;
        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) DispatchMessageW(&msg);
        Sleep(10);
    }
    CloseHandle(th); CloseHandle(ev);
}

/* a user APC queued before the wait: CoWaitForMultipleObjects is never alertable */
static void test_apc(DWORD flags, const char *what)
{
    HANDLE ev = CreateEventW(NULL, TRUE, FALSE, NULL);
    DWORD idx = 0xdead;
    HRESULT hr;

    apc_ran = 0;
    QueueUserAPC(apc, GetCurrentThread(), 0);
    hr = pWait(flags, 100, 1, &ev, &idx);
    CHECK(apc_ran == 0 && hr == RPC_S_CALLPENDING_HR, "%s: APC ran %ld, hr %#lx", what, apc_ran, hr);
    SleepEx(0, TRUE);
    CloseHandle(ev);
}

int main(void)
{
    HMODULE combase = LoadLibraryA("combase.dll");
    HANDLE ev[57];
    WNDCLASSW wc = {0};
    DWORD idx;
    int i;

    pWait = (void *)GetProcAddress(combase, "CoWaitForMultipleObjects");
    CHECK(pWait != NULL, "exported");
    if (!pWait) { printf("RESULT: FAIL\n"); return 1; }

    for (i = 0; i < 57; i++) ev[i] = CreateEventW(NULL, TRUE, TRUE, NULL);
    CHECK(pWait(4, 0, 1, ev, &idx) == E_INVALIDARG, "unknown flag rejected");
    CHECK(pWait(0, 0, 57, ev, &idx) == E_INVALIDARG, "more than 56 handles rejected");
    idx = 0xdead;
    CHECK(pWait(0, 0, 56, ev, &idx) == S_OK && idx == 0, "56 handles: index %lu", idx);
    CHECK(pWait(0, 0, 1, ev, NULL) == E_INVALIDARG, "no index pointer");
    for (i = 0; i < 57; i++) CloseHandle(ev[i]);

    /* no apartment */
    test_any(CWMO_DEFAULT, "no apartment, default");
    test_any(CWMO_DISPATCH_CALLS, "no apartment, dispatch calls");
    test_apc(CWMO_DISPATCH_WINDOW_MESSAGES, "no apartment, dispatch window messages");

    /* single-threaded apartment with a window */
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    wc.lpfnWndProc = wndproc; wc.hInstance = GetModuleHandleW(NULL); wc.lpszClassName = L"cwmo";
    RegisterClassW(&wc);
    win = CreateWindowW(L"cwmo", L"cwmo", WS_OVERLAPPEDWINDOW, 0, 0, 100, 100, NULL, NULL, NULL, NULL);
    CHECK(win != NULL, "window");
    test_any(CWMO_DEFAULT, "STA, default");
    test_any(CWMO_DISPATCH_CALLS, "STA, dispatch calls");
    test_any(CWMO_DISPATCH_WINDOW_MESSAGES, "STA, dispatch window messages");
    test_any(CWMO_DISPATCH_CALLS | CWMO_DISPATCH_WINDOW_MESSAGES, "STA, both");
    test_apc(CWMO_DISPATCH_WINDOW_MESSAGES, "STA, dispatch window messages");
    test_apc(CWMO_DEFAULT, "STA, default");
    test_sent(CWMO_DEFAULT, FALSE, "STA, default");
    test_sent(CWMO_DISPATCH_WINDOW_MESSAGES, TRUE, "STA, dispatch window messages");
    DestroyWindow(win);
    CoUninitialize();

    /* multithreaded apartment */
    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    test_any(CWMO_DISPATCH_CALLS, "MTA, dispatch calls");
    test_apc(CWMO_DISPATCH_WINDOW_MESSAGES, "MTA, dispatch window messages");
    CoUninitialize();

    printf("RESULT: %s\n", fails ? "FAIL" : "PASS");
    return fails != 0;
}
