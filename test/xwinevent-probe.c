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

static LRESULT CALLBACK msg_hook(int code, WPARAM wp, LPARAM lp) { return CallNextHookEx(NULL, code, wp, lp); }

static void CALLBACK any_proc(HWINEVENTHOOK hook, DWORD event, HWND hwnd, LONG obj, LONG child, DWORD tid, DWORD time)
{
    if (event == EVENT_OBJECT_LOCATIONCHANGE) events++;
}

/* (8404) another account's or desktop's program:
 *   xwinevent-probe.exe xtarget FILE [desktop]   make a window (on a desktop of its own when
 *                                                asked), write "pid tid" to FILE, announce its child's
 *                                                EVENT_OBJECT_LOCATIONCHANGE every 100 ms for 20 s
 *   xwinevent-probe.exe xhook FILE [desktop]     hook the thread FILE names (from that desktop): out of context with no
 *                                                module, and in context (WH_GETMESSAGE, this module);
 *                                                count its events for 4 s */
static int cross(int argc, char **argv)
{
    if (!strcmp(argv[1], "xtarget"))
    {
        HWND hwnd, child;
        FILE *f;
        int i;
        if (argc > 3)
        {
            HDESK desk = CreateDesktopA(argv[3], NULL, NULL, 0, GENERIC_ALL, NULL);
            if (!desk || !SetThreadDesktop(desk)) { printf("desktop failed %lu\n", GetLastError()); return 1; }
        }
        /* a message-only window: another account's programs here have no display to show one on */
        hwnd = CreateWindowW(L"static", L"sg-xwinevent-x", 0, 0, 0, 0, 0, HWND_MESSAGE, NULL, NULL, NULL);
        child = hwnd;
        if (!hwnd) { printf("window failed %lu\n", GetLastError()); return 1; }
        if (!(f = fopen(argv[2], "w"))) return 1;
        fprintf(f, "%lu %lu\n", GetCurrentProcessId(), GetCurrentThreadId());
        fclose(f);
        for (i = 0; i < 200; i++)
        {
            /* the event itself, as a program announces one */
            NotifyWinEvent(EVENT_OBJECT_LOCATIONCHANGE, child, OBJID_WINDOW, CHILDID_SELF);
            pump(100);
        }
        return 0;
    }
    else
    {
        DWORD pid = 0, tid = 0;
        HWINEVENTHOOK hook;
        HHOOK mod;
        FILE *f = fopen(argv[2], "r");
        if (!f || fscanf(f, "%lu %lu", &pid, &tid) != 2) { printf("xfile=0\n"); return 1; }
        fclose(f);
        if (argc > 3)   /* on the target's desktop */
        {
            HDESK desk = OpenDesktopA(argv[3], 0, FALSE, GENERIC_ALL);
            if (!desk || !SetThreadDesktop(desk)) { printf("xdesktop=0 %lu\n", GetLastError()); return 1; }
        }
        hook = SetWinEventHook(EVENT_OBJECT_LOCATIONCHANGE, EVENT_OBJECT_LOCATIONCHANGE, NULL, any_proc, pid, tid,
                               WINEVENT_OUTOFCONTEXT);
        printf("xhook=%d\n", hook != NULL);
        pump(3000);
        printf("xevents=%d\n", events > 0);
        mod = SetWindowsHookExA(WH_GETMESSAGE, msg_hook, GetModuleHandleA(NULL), tid);
        printf("xmodhook=%d\n", mod != NULL);
        if (hook) UnhookWinEvent(hook);
        if (mod) UnhookWindowsHookEx(mod);
        return 0;
    }
}

int main(int argc, char **argv)
{
    HANDLE ready, go, done;

    if (argc > 2 && (!strcmp(argv[1], "xtarget") || !strcmp(argv[1], "xhook"))) return cross(argc, argv);
    ready = CreateEventW(NULL, FALSE, FALSE, L"Local\\sg-xwinevent-ready");
    go = CreateEventW(NULL, FALSE, FALSE, L"Local\\sg-xwinevent-go");
    done = CreateEventW(NULL, FALSE, FALSE, L"Local\\sg-xwinevent-done");

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
