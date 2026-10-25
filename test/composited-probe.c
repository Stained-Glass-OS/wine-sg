/* composited-probe.c (wine-sg 6100): the children of a WS_EX_COMPOSITED window paint bottom to top.
 * A white parent (200,150 400x300, WS_EX_COMPOSITED) holds a red child over its lower part (created
 * first: a new child window goes to the bottom of the z-order, so the first is the top) and a gray
 * child covering all of it (created last: the bottom), neither clipping its siblings -- as Opera's
 * installer's button strip and screens.
 *   composited-probe.exe OUTFILE    writes ready=1 after showing, redrawn=1 after a RedrawWindow
 *                                   of the parent with RDW_UPDATENOW 4 s later; up 15 s */
#include <windows.h>
#include <stdio.h>

static LRESULT CALLBACK proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_PAINT)
    {
        PAINTSTRUCT ps;
        RECT rc;
        HDC dc = BeginPaint(hwnd, &ps);
        GetClientRect(hwnd, &rc);
        FillRect(dc, &rc, (HBRUSH)GetClassLongPtrW(hwnd, GCLP_HBRBACKGROUND));
        EndPaint(hwnd, &ps);
        return 0;
    }
    if (msg == WM_ERASEBKGND) return 1;
    return DefWindowProcW(hwnd, msg, wp, lp);
}

static void reg(const WCHAR *name, COLORREF color)
{
    WNDCLASSW wc = {0};
    wc.lpfnWndProc = proc;
    wc.hInstance = GetModuleHandleW(NULL);
    wc.hbrBackground = CreateSolidBrush(color);
    wc.lpszClassName = name;
    RegisterClassW(&wc);
}

static void pump(DWORD ms)
{
    DWORD start = GetTickCount();
    MSG msg;
    while (GetTickCount() - start < ms)
    {
        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) DispatchMessageW(&msg);
        Sleep(20);
    }
}

int main(int argc, char **argv)
{
    HWND top;
    FILE *f;

    if (argc < 2) return 2;
    reg(L"SgCompTop", RGB(255, 255, 255));
    reg(L"SgCompGray", RGB(128, 128, 128));
    reg(L"SgCompRed", RGB(255, 0, 0));
    top = CreateWindowExW(WS_EX_COMPOSITED, L"SgCompTop", L"composited probe", WS_POPUP, 200, 150, 400, 300,
                          NULL, NULL, NULL, NULL);
    /* the red strip first (the top), the gray screen last (the bottom) */
    CreateWindowW(L"SgCompRed", L"", WS_CHILD | WS_VISIBLE, 0, 200, 400, 100, top, NULL, NULL, NULL);
    CreateWindowW(L"SgCompGray", L"", WS_CHILD | WS_VISIBLE, 0, 0, 400, 300, top, NULL, NULL, NULL);
    ShowWindow(top, SW_SHOW);
    pump(3000);
    if (!(f = fopen(argv[1], "w"))) return 2;
    fprintf(f, "ready=1\n");
    fflush(f);
    pump(4000);
    RedrawWindow(top, NULL, NULL, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_UPDATENOW);
    pump(500);
    fprintf(f, "redrawn=1\n");
    fclose(f);
    pump(8000);
    return 0;
}
