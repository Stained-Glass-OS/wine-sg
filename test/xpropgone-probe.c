/* xpropgone-probe: its window's X window is destroyed from outside (as when
 * another thread or program destroys it meanwhile); setting the window's
 * title then makes winex11 change a property of a window that is gone. The
 * process must live on (patches/sg/0808).
 *   xpropgone-probe.exe FLAGDIR   prints "XID <id>", waits for FLAGDIR\gone,
 *                                 sets the title, prints "ALIVE" */
#include <windows.h>
#include <stdio.h>

int main(int argc, char **argv)
{
    char path[MAX_PATH];
    HWND hwnd;
    MSG msg;
    int i;

    if (argc < 2) return 2;
    hwnd = CreateWindowExA(0, "STATIC", "probe", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 10, 10, 200, 100, NULL, NULL, NULL, NULL);
    for (i = 0; i < 20; i++) { while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) DispatchMessageA(&msg); Sleep(50); }
    printf("XID %lx\n", (unsigned long)(ULONG_PTR)GetPropA(hwnd, "__wine_x11_whole_window"));
    fflush(stdout);
    snprintf(path, sizeof(path), "%s\\gone", argv[1]);
    for (i = 0; i < 400 && GetFileAttributesA(path) == INVALID_FILE_ATTRIBUTES; i++) Sleep(50);
    for (i = 0; i < 10; i++)
    {
        char title[32];
        snprintf(title, sizeof(title), "title %d", i);
        SetWindowTextA(hwnd, title);
        while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) DispatchMessageA(&msg);
        Sleep(50);
    }
    printf("ALIVE\n");
    fflush(stdout);
    return 0;
}
