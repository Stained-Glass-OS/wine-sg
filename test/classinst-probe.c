/* classinst gate probe (wine-sg 8409): a local window class registered with an instance below 0x10000
 * that is not a module -- a process handle, as NAPS2's worker passes Process.GetCurrentProcess().Handle,
 * a new handle each call -- is found with another such value, as on Windows (the high word matches).
 * Prints KEY=1 per check, then done=1. */
#include <windows.h>
#include <stdio.h>

static UINT msgid;
static HWND wnd;
static HANDLE done_event;

static LRESULT CALLBACK proc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    if (m == msgid) { SetEvent(done_event); return 0; }
    return DefWindowProcW(h, m, w, l);
}

static DWORD WINAPI poster(void *arg)
{
    PostMessageW(wnd, msgid, 0, 0);
    WaitForSingleObject(done_event, 5000);
    PostThreadMessageW((DWORD)(ULONG_PTR)arg, WM_QUIT, 0, 0);
    return 0;
}

int main(void)
{
    WNDCLASSW wc = {0}, info;
    HANDLE h1 = OpenProcess(PROCESS_ALL_ACCESS, FALSE, GetCurrentProcessId());
    HANDLE h2 = OpenProcess(PROCESS_ALL_ACCESS, FALSE, GetCurrentProcessId());
    HWND big;
    MSG m;
    ULONG_PTR deadline;

    printf("bits: %d handles %p %p\n", (int)sizeof(void *) * 8, h1, h2);
    printf("handles=%d\n", h1 && h2 && h1 != h2 && !((ULONG_PTR)h1 >> 16) && !((ULONG_PTR)h2 >> 16));
    wc.lpszClassName = L"SgClassInstPump";
    wc.lpfnWndProc = proc;
    wc.hInstance = h1;
    printf("register=%d\n", RegisterClassW(&wc) != 0);

    /* the same value finds it (as before) */
    printf("same=%d\n", GetClassInfoW(h1, L"SgClassInstPump", &info) != 0);
    /* another small value: GetClassInfo and CreateWindowEx find it, as on Windows */
    printf("classinfo=%d\n", GetClassInfoW(h2, L"SgClassInstPump", &info) != 0);
    wnd = CreateWindowExW(0, L"SgClassInstPump", L"", 0, 0, 0, 0, 0, NULL, NULL, h2, NULL);
    printf("window: %p err %lu\nwindow=%d\n", wnd, GetLastError(), wnd != NULL);

    /* what NAPS2 does with that window: another thread posts, this thread's loop runs it */
    msgid = RegisterWindowMessageW(L"SgClassInstRun");
    done_event = CreateEventW(NULL, TRUE, FALSE, NULL);
    if (wnd)
    {
        CreateThread(NULL, 0, poster, (void *)(ULONG_PTR)GetCurrentThreadId(), 0, NULL);
        deadline = GetTickCount() + 6000;
        while (GetTickCount() < deadline && GetMessageW(&m, NULL, 0, 0) > 0) DispatchMessageW(&m);
    }
    printf("posted=%d\n", WaitForSingleObject(done_event, 0) == WAIT_OBJECT_0);

    /* instances whose high words differ still do not find each other */
    wc.lpszClassName = L"SgClassInstBig";
    wc.hInstance = (HINSTANCE)0x12340000;
    RegisterClassW(&wc);
    big = CreateWindowExW(0, L"SgClassInstBig", L"", 0, 0, 0, 0, 0, NULL, NULL, (HINSTANCE)0x56780000, NULL);
    printf("bigapart=%d\n", big == NULL && !GetClassInfoW((HINSTANCE)0x56780000, L"SgClassInstBig", &info));
    printf("done=1\n");
    return 0;
}
