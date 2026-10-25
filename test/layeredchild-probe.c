/* A layered child window (6106): Windows 8 lets a child be WS_EX_LAYERED, and JetBrains' runtime
 * (PyCharm, IntelliJ) draws its caption buttons in one, filled by UpdateLayeredWindow. Wine gave it
 * an X window of its own at the root, placed at its parent-relative position: the buttons showed
 * at the screen's top edge and stayed there when the window moved, and the frame's own paint went
 * over the child. The probe: a blue popup with a red layered child (UpdateLayeredWindow, per-pixel
 * alpha, WS_EX_TRANSPARENT), moved once; it reports where the child should show, and whether a
 * click on the child reaches the frame (a transparent child lets the pointer through). */
#include <windows.h>
#include <stdio.h>

static FILE *out;
static HWND frame, child;
static int clicks;

static LRESULT CALLBACK frame_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_PAINT)
    {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(hwnd, &ps);
        RECT rc;
        HBRUSH brush = CreateSolidBrush(RGB(0, 0, 255));
        GetClientRect(hwnd, &rc);
        FillRect(dc, &rc, brush);
        DeleteObject(brush);
        EndPaint(hwnd, &ps);
        return 0;
    }
    if (msg == WM_LBUTTONDOWN)
    {
        fprintf(out, "frame_click=%d,%d\n", (short)LOWORD(lp), (short)HIWORD(lp));
        fflush(out);
        clicks++;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

static void report(const char *name)
{
    RECT rc;
    GetWindowRect(child, &rc);
    fprintf(out, "%s=%ld,%ld,%ld,%ld\n", name, rc.left, rc.top, rc.right, rc.bottom);
    fflush(out);
}

int main(int argc, char **argv)
{
    WNDCLASSW wc = {0};
    BITMAPINFO bi = {{sizeof(BITMAPINFOHEADER), 80, -40, 1, 32, BI_RGB}};
    POINT src = {0, 0};
    SIZE size = {80, 40};
    BLENDFUNCTION blend = {AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
    HDC screen, mem;
    HBITMAP bitmap;
    DWORD *bits;
    MSG msg;
    int i;

    if (argc < 2 || !(out = fopen(argv[1], "w"))) return 2;
    wc.lpfnWndProc = frame_proc;
    wc.hInstance = GetModuleHandleW(NULL);
    wc.lpszClassName = L"SgLayeredChildFrame";
    RegisterClassW(&wc);
    wc.lpfnWndProc = DefWindowProcW;
    wc.lpszClassName = L"SgLayeredChild";
    RegisterClassW(&wc);
    frame = CreateWindowW(L"SgLayeredChildFrame", L"layered child probe", WS_POPUP | WS_VISIBLE,
                          200, 150, 400, 300, NULL, NULL, wc.hInstance, NULL);
    child = CreateWindowExW(WS_EX_LAYERED | WS_EX_TRANSPARENT, L"SgLayeredChild", L"", WS_CHILD,
                            300, 0, 80, 40, frame, NULL, wc.hInstance, NULL);
    screen = GetDC(0);
    mem = CreateCompatibleDC(screen);
    bitmap = CreateDIBSection(mem, &bi, DIB_RGB_COLORS, (void **)&bits, NULL, 0);
    for (i = 0; i < 80 * 40; i++) bits[i] = 0xffff0000;   /* opaque red, premultiplied */
    SelectObject(mem, bitmap);
    fprintf(out, "ulw=%d\n", UpdateLayeredWindow(child, screen, NULL, &size, mem, &src, 0, &blend, ULW_ALPHA));
    ShowWindow(child, SW_SHOWNA);
    UpdateWindow(frame);
    report("child");
    fprintf(out, "ready=1\n");
    fflush(out);

    SetTimer(NULL, 1, 3000, NULL);
    for (i = 0; GetMessageW(&msg, NULL, 0, 0); )
    {
        if (msg.message == WM_TIMER)
        {
            if (++i == 1)
            {
                SetWindowPos(frame, NULL, 100, 300, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
                RedrawWindow(frame, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
                report("moved");
                fprintf(out, "moved_ready=1\n");
                fflush(out);
            }
            else if (i > 6) break;
            continue;
        }
        DispatchMessageW(&msg);
    }
    fprintf(out, "clicks=%d\n", clicks);
    fclose(out);
    return 0;
}
