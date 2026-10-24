/* dwmframe-probe.c (wine-sg 6085): a window whose client area covers its caption and extends the DWM frame
 * 32 px into it, as Paint.NET and SumatraPDF do. It paints the client black (transparent to DWM) with a blue
 * square in the band, asks DwmDefWindowProc first on WM_NCHITTEST, and writes what it sees to a file:
 *   dwmframe-probe.exe OUTFILE */
#include <windows.h>
#include <dwmapi.h>
#include <stdio.h>

static FILE *out;
static HWND window;

static LRESULT CALLBACK proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    LRESULT res;

    if (DwmDefWindowProc(hwnd, msg, wp, lp, &res)) return res;
    switch (msg)
    {
    case WM_NCCALCSIZE:
        return 0;  /* the client area is the whole window */
    case WM_NCHITTEST:
    {
        POINT pt = { (short)LOWORD(lp), (short)HIWORD(lp) };
        ScreenToClient(hwnd, &pt);
        return pt.y < 32 ? HTCAPTION : HTCLIENT;
    }
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(hwnd, &ps);
        RECT rc, sq = { 40, 6, 60, 26 };
        HBRUSH blue = CreateSolidBrush(RGB(0, 0, 255));
        GetClientRect(hwnd, &rc);
        FillRect(dc, &rc, GetStockObject(BLACK_BRUSH));
        FillRect(dc, &sq, blue);
        DeleteObject(blue);
        {
            /* opaque black, as GDI+ or Direct2D draw it (alpha 255): it stays black over the frame */
            BITMAPINFO bi = {{ sizeof(BITMAPINFOHEADER), 20, -20, 1, 32, BI_RGB }};
            DWORD *bits;
            HBITMAP bm = CreateDIBSection(dc, &bi, DIB_RGB_COLORS, (void **)&bits, NULL, 0);
            HDC mem = CreateCompatibleDC(dc);
            int i;
            for (i = 0; i < 400; i++) bits[i] = 0xff000000;
            SelectObject(mem, bm);
            BitBlt(dc, 80, 6, 20, 20, mem, 0, 0, SRCCOPY);
            DeleteDC(mem);
            DeleteObject(bm);
        }
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_DESTROY:
        fprintf(out, "closed=1\n");
        fflush(out);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

static LRESULT hit(int x, int y)
{
    RECT rc;
    GetWindowRect(window, &rc);
    return SendMessageW(window, WM_NCHITTEST, 0, MAKELPARAM(rc.left + x, rc.top + y));
}

int main(int argc, char **argv)
{
    WNDCLASSW wc = {0};
    MARGINS margins = { 0, 0, 32, 0 };
    RECT buttons, rc;
    MSG msg;
    int w;

    if (argc < 2 || !(out = fopen(argv[1], "w"))) return 2;
    wc.lpfnWndProc = proc;
    wc.hInstance = GetModuleHandleW(NULL);
    wc.hCursor = LoadCursorW(NULL, (const WCHAR *)IDC_ARROW);
    wc.lpszClassName = L"SgDwmFrame";
    RegisterClassW(&wc);
    window = CreateWindowW(L"SgDwmFrame", L"DWM frame probe", WS_OVERLAPPEDWINDOW, 100, 100, 400, 240,
                           NULL, NULL, wc.hInstance, NULL);
    fprintf(out, "extend=%#lx\n", DwmExtendFrameIntoClientArea(window, &margins));
    ShowWindow(window, SW_SHOWNORMAL);
    UpdateWindow(window);
    GetWindowRect(window, &rc);
    fprintf(out, "window=%ld,%ld,%ld,%ld\n", rc.left, rc.top, rc.right, rc.bottom);
    if (SUCCEEDED(DwmGetWindowAttribute(window, DWMWA_CAPTION_BUTTON_BOUNDS, &buttons, sizeof(buttons))))
    {
        fprintf(out, "buttons=%ld,%ld,%ld,%ld\n", buttons.left, buttons.top, buttons.right, buttons.bottom);
        w = (buttons.right - buttons.left) / 3;
        fprintf(out, "hit_close=%Id\n", hit(buttons.right - w / 2, (buttons.top + buttons.bottom) / 2));
        fprintf(out, "hit_max=%Id\n", hit(buttons.right - w - w / 2, (buttons.top + buttons.bottom) / 2));
        fprintf(out, "hit_min=%Id\n", hit(buttons.left + w / 2, (buttons.top + buttons.bottom) / 2));
    }
    fprintf(out, "hit_band=%Id\n", hit(150, 16));
    fprintf(out, "hit_client=%Id\n", hit(150, 150));
    fprintf(out, "ready=1\n");
    fflush(out);
    SetTimer(window, 1, 60000, NULL);
    while (GetMessageW(&msg, NULL, 0, 0))
    {
        if (msg.message == WM_TIMER) break;
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    fclose(out);
    return 0;
}
