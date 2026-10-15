/* A window in a virtual desktop for test/fastxcfg-gate.sh: prints the X window id of the window and
 * its rectangle each time it changes, until it has held a changed rectangle for a second. */
#include <windows.h>
#include <stdio.h>

static LRESULT CALLBACK proc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    if (m == WM_DESTROY) PostQuitMessage(0);
    return DefWindowProcA(h, m, w, l);
}

int main(int argc, char **argv)
{
    FILE *out = argc > 1 ? fopen(argv[1], "w") : stdout;
    WNDCLASSA wc = { 0 };
    HWND hwnd;
    RECT r, last = { 0 }, first;
    DWORD start, changed = 0;
    MSG msg;

    wc.lpfnWndProc = proc;
    wc.hInstance = GetModuleHandleA(NULL);
    wc.lpszClassName = "sgxcfg";
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    RegisterClassA(&wc);
    hwnd = CreateWindowA("sgxcfg", "sgxcfg", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 20, 30, 300, 200, NULL, NULL, wc.hInstance, NULL);
    SetForegroundWindow(hwnd);
    start = GetTickCount();
    GetWindowRect(hwnd, &first);
    while (GetTickCount() - start < 15000)
    {
        while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) { TranslateMessage(&msg); DispatchMessageA(&msg); }
        GetWindowRect(hwnd, &r);
        if (!EqualRect(&r, &last))
        {
            fprintf(out, "RECT=%ld,%ld,%ld,%ld\n", r.left, r.top, r.right, r.bottom); fflush(out);
            if (!changed && last.right && !EqualRect(&r, &first)) changed = GetTickCount();
            last = r;
        }
        if (GetTickCount() - start > 2000)
        {
            static int announced;
            if (!announced)
            {
                announced = 1;
                fprintf(out, "XID=%lx\n", (unsigned long)(ULONG_PTR)GetPropA(hwnd, "__wine_x11_whole_window"));
                fflush(out);
            }
        }
        if (changed && GetTickCount() - changed > 1500) break;
        Sleep(50);
    }
    fprintf(out, "DONE\n"); fflush(out);
    return 0;
}
