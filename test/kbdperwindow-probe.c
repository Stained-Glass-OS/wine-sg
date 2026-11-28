/* kbdperwindow-probe NAME X (patches/sg/9161, test/kbdperwindow-gate.sh): a window "sgperwindow NAME" at x X that
 * logs, prefixed with NAME: ACTIVE when it comes to the front, CHANGE hkl on WM_INPUTLANGCHANGE, NOW hkl
 * (GetKeyboardLayout) each time it is activated, DONE on the character q. With "local" or "global" as a third argument it
 * first sets SPI_SETTHREADLOCALINPUTSETTINGS on or off (and logs SPI set-result value-read-back). Gives up after 60 s. */
#include <windows.h>
#include <stdio.h>
#ifndef SPI_GETTHREADLOCALINPUTSETTINGS
#define SPI_GETTHREADLOCALINPUTSETTINGS 0x104E
#define SPI_SETTHREADLOCALINPUTSETTINGS 0x104F
#endif

static const char *name;

static LRESULT CALLBACK wndproc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg)
    {
    case WM_ACTIVATE:
        if (LOWORD(wp) != WA_INACTIVE) { printf("%s ACTIVE\n", name); fflush(stdout); SetTimer(hwnd, 2, 400, NULL); }
        break;
    case WM_INPUTLANGCHANGE:
        printf("%s CHANGE %08lx\n", name, (unsigned long)lp); fflush(stdout);
        break;
    case WM_CHAR:
        if (wp == 'q') { printf("%s DONE\n", name); fflush(stdout); PostQuitMessage(0); }
        return 0;
    case WM_TIMER:
        if (wp == 2)
        {
            KillTimer(hwnd, 2);
            printf("%s NOW %08lx\n", name, (unsigned long)(ULONG_PTR)GetKeyboardLayout(0)); fflush(stdout);
            return 0;
        }
        printf("%s TIMEOUT\n", name); fflush(stdout);
        PostQuitMessage(1);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

int main(int argc, char **argv)
{
    WNDCLASSA wc = {0};
    char title[64];
    HWND hwnd;
    MSG msg;

    name = argc > 1 ? argv[1] : "A";
    if (argc > 3)
    {
        BOOL on = !strcmp(argv[3], "local");
        BOOL set = SystemParametersInfoW(SPI_SETTHREADLOCALINPUTSETTINGS, 0, (void *)(ULONG_PTR)on, SPIF_UPDATEINIFILE), back = 2;
        SystemParametersInfoW(SPI_GETTHREADLOCALINPUTSETTINGS, 0, &back, 0);
        printf("%s SPI %d %d\n", name, set, back); fflush(stdout);
    }
    wc.lpfnWndProc = wndproc;
    wc.hInstance = GetModuleHandleW(NULL);
    wc.lpszClassName = "sgperwindow";
    RegisterClassA(&wc);
    snprintf(title, sizeof(title), "sgperwindow %s", name);
    hwnd = CreateWindowA("sgperwindow", title, WS_OVERLAPPEDWINDOW | WS_VISIBLE, argc > 2 ? atoi(argv[2]) : 50, 50,
                         300, 200, NULL, NULL, wc.hInstance, NULL);
    SetForegroundWindow(hwnd);
    SetTimer(hwnd, 1, 60000, NULL);
    printf("%s READY\n", name); fflush(stdout);
    while (GetMessageA(&msg, NULL, 0, 0) > 0)
    {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
    return 0;
}
