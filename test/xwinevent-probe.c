/* xwinevent gate probe (wine-sg 8403): an out-of-context WinEvent hook on another process's thread.
 * mpv embedded in SMPlayer (--wid) follows its parent window's size through
 * SetWinEventHook(EVENT_OBJECT_LOCATIONCHANGE, NULL module, SMPlayer's pid and thread,
 * WINEVENT_OUTOFCONTEXT); wineserver refused a thread hook of another process without a module.
 *   xwinevent-probe.exe          start a target process, hook its thread, have it resize its window
 *   xwinevent-probe.exe target   (the other process) make a window, resize it on request
 * Prints KEY=1 for each check that holds, then done=1. */
#include <windows.h>
#include <stdio.h>

static LONG events;
static HWND target_hwnd;

static void CALLBACK hook_proc(HWINEVENTHOOK hook, DWORD event, HWND hwnd, LONG obj, LONG child, DWORD tid, DWORD time)
{
    if (event == EVENT_OBJECT_LOCATIONCHANGE && hwnd == target_hwnd && obj == OBJID_WINDOW) events++;
}

static void pump(DWORD ms)
{
    DWORD start = GetTickCount();
    MSG msg;
    while (GetTickCount() - start < ms)
    {
        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) DispatchMessageW(&msg);
        Sleep(10);
    }
}

int main(int argc, char **argv)
{
    HANDLE ready = CreateEventW(NULL, FALSE, FALSE, L"Local\\sg-xwinevent-ready");
    HANDLE go = CreateEventW(NULL, FALSE, FALSE, L"Local\\sg-xwinevent-go");
    HANDLE done = CreateEventW(NULL, FALSE, FALSE, L"Local\\sg-xwinevent-done");

    if (argc > 1 && !strcmp(argv[1], "target"))
    {
        HWND hwnd = CreateWindowW(L"static", L"sg-xwinevent-target", WS_OVERLAPPEDWINDOW, 10, 10, 200, 100, NULL, NULL, NULL, NULL);
        HWND child = CreateWindowW(L"static", L"", WS_CHILD | WS_VISIBLE, 0, 0, 50, 50, hwnd, NULL, NULL, NULL);
        int i;
        ShowWindow(hwnd, SW_SHOW);
        SetEvent(ready);
        if (WaitForSingleObject(go, 20000)) return 1;
        /* SMPlayer resizing the widget mpv draws into */
        for (i = 0; i < 3; i++) { SetWindowPos(child, NULL, 0, 0, 60 + 20 * i, 60, SWP_NOZORDER | SWP_NOACTIVATE); pump(50); }
        SetEvent(done);
        pump(3000);
        return 0;
    }
    else
    {
        STARTUPINFOA si = { sizeof(si) };
        PROCESS_INFORMATION pi;
        char cmd[MAX_PATH + 16], self[MAX_PATH];
        HWINEVENTHOOK hook, hook_in;
        HWND top;
        DWORD pid = 0, tid;

        GetModuleFileNameA(NULL, self, MAX_PATH);
        snprintf(cmd, sizeof(cmd), "\"%s\" target", self);
        if (!CreateProcessA(NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) { printf("start=0\ndone=1\n"); return 0; }
        if (WaitForSingleObject(ready, 20000)) { printf("start=0\ndone=1\n"); return 0; }
        top = FindWindowW(L"static", L"sg-xwinevent-target");
        target_hwnd = GetWindow(top, GW_CHILD);
        tid = GetWindowThreadProcessId(top, &pid);
        printf("start=%d\n", top && target_hwnd && pid == pi.dwProcessId);

        hook = SetWinEventHook(EVENT_OBJECT_LOCATIONCHANGE, EVENT_OBJECT_LOCATIONCHANGE, NULL, hook_proc,
                               pid, tid, WINEVENT_OUTOFCONTEXT);
        printf("hook=%d (error %lu)\n", hook != NULL, hook ? 0 : GetLastError());
        /* an in-context hook into another process still needs a module */
        hook_in = SetWinEventHook(EVENT_OBJECT_LOCATIONCHANGE, EVENT_OBJECT_LOCATIONCHANGE, NULL, hook_proc,
                                  pid, tid, WINEVENT_INCONTEXT);
        printf("incontext=%d\n", hook_in == NULL);
        if (hook_in) UnhookWinEvent(hook_in);

        SetEvent(go);
        WaitForSingleObject(done, 20000);
        pump(1500);
        printf("events: %ld\n", events);
        printf("delivered=%d\n", events >= 3);
        if (hook) UnhookWinEvent(hook);
        WaitForSingleObject(pi.hProcess, 10000);
        printf("done=1\n");
        return 0;
    }
}
