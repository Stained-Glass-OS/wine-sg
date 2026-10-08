/* the window test/wshshell.js activates and types into: it writes what it
 * receives to the file named on its command line */
#include <windows.h>
#include <stdio.h>

static FILE *out;

static LRESULT CALLBACK proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg)
    {
    case WM_CHAR: case WM_SYSCHAR:
        fprintf(out, "%s%02x;", msg == WM_SYSCHAR ? "s" : "c", (unsigned int)wp);
        fflush(out);
        return 0;
    case WM_KEYDOWN:
        if (wp == VK_F5) { fprintf(out, "F5;"); fflush(out); }
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcA(hwnd, msg, wp, lp);
}

int main(int argc, char **argv)
{
    WNDCLASSA wc = { 0, proc, 0, 0, NULL, NULL, NULL, (HBRUSH)(COLOR_WINDOW + 1), NULL, "SGKeysTarget" };
    MSG msg;

    if (argc < 2 || !(out = fopen(argv[1], "w"))) return 1;
    wc.hInstance = GetModuleHandleA(NULL);
    RegisterClassA(&wc);
    CreateWindowA("SGKeysTarget", "SG Keys Target Window", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 200, 200, 300, 200,
                  NULL, NULL, wc.hInstance, NULL);
    SetTimer(NULL, 0, 60000, NULL);
    while (GetMessageA(&msg, NULL, 0, 0))
    {
        if (msg.message == WM_TIMER && !msg.hwnd) break;
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
    fclose(out);
    return 0;
}
