/* firstwindow-probe: several threads of one program make their first windows at the same moment,
 * as a program's splash screen and main window, or the delete question's dialog and a thread looking
 * for it, do (test/firstwindow-gate.sh). Prints "made N of M" and "desktop=<class>". */
#include <windows.h>
#include <stdio.h>

#define THREADS 4
static HANDLE go;
static LONG made;

static DWORD WINAPI make_window(void *arg)
{
    HWND hwnd;
    WaitForSingleObject(go, INFINITE);
    hwnd = CreateWindowExW(0, L"STATIC", L"firstwindow", WS_OVERLAPPEDWINDOW, 10, 10, 200, 100, 0, 0, 0, 0);
    if (hwnd) { InterlockedIncrement(&made); DestroyWindow(hwnd); }
    return 0;
}

int wmain(void)
{
    HANDLE th[THREADS];
    WCHAR cls[64] = L"";
    int i;

    go = CreateEventW(NULL, TRUE, FALSE, NULL);
    for (i = 0; i < THREADS; i++) th[i] = CreateThread(NULL, 0, make_window, NULL, 0, NULL);
    Sleep(100);
    SetEvent(go);
    WaitForMultipleObjects(THREADS, th, TRUE, 60000);
    GetClassNameW(GetDesktopWindow(), cls, 64);
    wprintf(L"made %ld of %d\n", made, THREADS);
    wprintf(L"desktop=%ls\n", cls);
    return 0;
}
