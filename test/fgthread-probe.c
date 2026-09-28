/* fgthread-probe: a hidden window another thread positions without
 * SWP_NOACTIVATE -- Firefox's full-screen transition window -- does not take
 * the foreground from the window that has it; one of the foreground thread's
 * own hidden windows still does (Windows' behaviour, user32 tests) */
#include <windows.h>
#include <stdio.h>

static HWND main_wnd;
static volatile LONG deactivated;

static LRESULT CALLBACK proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (hwnd == main_wnd && msg == WM_ACTIVATE && LOWORD(wp) == WA_INACTIVE) InterlockedIncrement(&deactivated);
    return DefWindowProcW(hwnd, msg, wp, lp);
}

static void pump(DWORD ms)
{
    DWORD end = GetTickCount() + ms;
    MSG msg;
    while ((LONG)(end - GetTickCount()) > 0)
    {
        while (PeekMessageW(&msg, 0, 0, 0, PM_REMOVE)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
        Sleep(10);
    }
}

static HWND transition, thread_active;
static HANDLE made, done;

static DWORD WINAPI transition_thread(void *arg)
{
    MSG msg;
    transition = CreateWindowExW(0, L"fgprobe", L"", 0, 0, 0, 0, 0, 0, 0, 0, 0);
    SetWindowLongW(transition, GWL_STYLE, 0);
    SetWindowLongW(transition, GWL_EXSTYLE, WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW);
    SetWindowPos(transition, HWND_TOPMOST, 0, 0, 640, 480, 0);
    thread_active = GetActiveWindow();
    SetEvent(made);
    ShowWindow(transition, SW_SHOWNA);
    while (WaitForSingleObject(done, 10) == WAIT_TIMEOUT)
        while (PeekMessageW(&msg, 0, 0, 0, PM_REMOVE)) DispatchMessageW(&msg);
    DestroyWindow(transition);
    return 0;
}

int main(void)
{
    WNDCLASSW wc = {0};
    HANDLE thread;
    HWND own;

    wc.lpfnWndProc = proc;
    wc.lpszClassName = L"fgprobe";
    wc.hbrBackground = GetStockObject(BLACK_BRUSH);
    RegisterClassW(&wc);
    main_wnd = CreateWindowExW(0, L"fgprobe", L"browser", WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                               0, 0, 640, 480, 0, 0, 0, 0);
    SetForegroundWindow(main_wnd);
    pump(500);
    printf("start %d\n", GetForegroundWindow() == main_wnd);
    deactivated = 0;

    made = CreateEventW(NULL, TRUE, FALSE, NULL);
    done = CreateEventW(NULL, TRUE, FALSE, NULL);
    thread = CreateThread(NULL, 0, transition_thread, NULL, 0, NULL);
    while (MsgWaitForMultipleObjects(1, &made, FALSE, INFINITE, QS_ALLINPUT) != WAIT_OBJECT_0) pump(0);
    pump(500);
    printf("other thread: foreground kept %d, deactivated %ld, its own active %d\n",
           GetForegroundWindow() == main_wnd, deactivated, thread_active == transition);
    SetEvent(done);
    while (MsgWaitForMultipleObjects(1, &thread, FALSE, INFINITE, QS_ALLINPUT) != WAIT_OBJECT_0) pump(0);

    own = CreateWindowExW(0, L"fgprobe", L"", WS_POPUP, 0, 0, 10, 10, 0, 0, 0, 0);
    SetWindowPos(own, HWND_TOPMOST, 0, 0, 20, 20, 0);
    pump(200);
    printf("same thread: hidden window active %d\n", GetActiveWindow() == own && GetForegroundWindow() == own);
    return 0;
}
