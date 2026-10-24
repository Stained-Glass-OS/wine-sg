/* iconcross-probe.c (wine-sg 6083): a program whose icon is solid red, and ExtractIconExW's results.
 *   iconcross-probe.exe window      a top-level window with the red icon as its class icon, for 60 s
 *   iconcross-probe.exe extract F   ExtractIconExW(F, 0, &large, &small, 1) with a dirty stack: KEY=VALUE lines */
#include <windows.h>
#include <shellapi.h>
#include <stdio.h>

static LRESULT CALLBACK proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_DESTROY) PostQuitMessage(0);
    return DefWindowProcW(hwnd, msg, wp, lp);
}

/* leave a pattern where the next calls' stack frames will be */
static void __attribute__((noinline)) dirty_stack(void)
{
    volatile unsigned char buf[16384];
    unsigned int i;
    for (i = 0; i < sizeof(buf); i++) buf[i] = 0x5a;
}

int wmain(int argc, WCHAR **argv)
{
    if (argc >= 2 && !wcscmp(argv[1], L"window"))
    {
        WNDCLASSW wc = {0};
        MSG msg;
        HWND hwnd;
        wc.lpfnWndProc = proc;
        wc.hInstance = GetModuleHandleW(NULL);
        wc.hIcon = LoadIconW(wc.hInstance, MAKEINTRESOURCEW(1));
        wc.hCursor = LoadCursorW(NULL, (const WCHAR *)IDC_ARROW);
        wc.hbrBackground = GetStockObject(WHITE_BRUSH);
        wc.lpszClassName = L"SgIconCross";
        RegisterClassW(&wc);
        /* no system menu: no caption icon, so the only red on screen is what the shell draws */
        hwnd = CreateWindowW(L"SgIconCross", L"Icon cross probe", WS_OVERLAPPED | WS_CAPTION | WS_THICKFRAME | WS_VISIBLE,
                             20, 20, 260, 160, NULL, NULL, wc.hInstance, NULL);
        SetTimer(hwnd, 1, 60000, NULL);
        while (GetMessageW(&msg, NULL, 0, 0) && msg.message != WM_TIMER) DispatchMessageW(&msg);
        return 0;
    }
    if (argc >= 3 && !wcscmp(argv[1], L"extract"))
    {
        HICON large = (HICON)0x1234, small = (HICON)0x5678;
        UINT n;
        dirty_stack();
        n = ExtractIconExW(argv[2], 0, &large, &small, 1);
        printf("count=%u\n", n);
        printf("large_null=%d\n", large == NULL);
        printf("small_null=%d\n", small == NULL);
        {
            ICONINFO info;
            printf("large_icon=%d\n", large && GetIconInfo(large, &info));
        }
        printf("done=1\n");
        return 0;
    }
    return 2;
}
