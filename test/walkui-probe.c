/* walkui gate probe: common controls as a Walk program (WireGuard's window) drives them (i1013).
 * Prints KEY value lines. */
#include <windows.h>
#include <commctrl.h>
#include <stdio.h>
#include <string.h>

static WNDPROC orig_tb, orig_lv;

/* Walk's ToolBar answers WM_WINDOWPOSCHANGED itself (TB_AUTOSIZE) and never passes it on: the
 * toolbar gets no WM_SIZE */
static LRESULT CALLBACK walk_tb(HWND h, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_WINDOWPOSCHANGED)
    {
        if (!(((WINDOWPOS *)lp)->flags & SWP_NOSIZE)) SendMessageW(h, TB_AUTOSIZE, 0, 0);
        return 0;
    }
    return CallWindowProcW(orig_tb, h, msg, wp, lp);
}

/* Walk's TableView answers WM_ERASEBKGND of its list views itself */
static LRESULT CALLBACK walk_lv(HWND h, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_ERASEBKGND) return 1;
    return CallWindowProcW(orig_lv, h, msg, wp, lp);
}

/* a black 32-bit DIB the size of the control, printed into as Walk prints its tab control */
static COLORREF print_pixel(HWND h, LPARAM flags, int x, int y)
{
    BITMAPINFO bi = {{ sizeof(BITMAPINFOHEADER) }};
    RECT rc; HDC dc; HBITMAP bmp, old; void *bits; COLORREF c;
    GetClientRect(h, &rc);
    bi.bmiHeader.biWidth = rc.right; bi.bmiHeader.biHeight = -rc.bottom;
    bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32; bi.bmiHeader.biCompression = BI_RGB;
    dc = CreateCompatibleDC(0);
    bmp = CreateDIBSection(dc, &bi, DIB_RGB_COLORS, &bits, NULL, 0);
    old = SelectObject(dc, bmp);
    memset(bits, 0, rc.right * rc.bottom * 4);
    SendMessageW(h, WM_PRINTCLIENT, (WPARAM)dc, flags);
    c = GetPixel(dc, x, y);
    SelectObject(dc, old); DeleteObject(bmp); DeleteDC(dc);
    return c;
}

int main(int argc, char **argv)
{
    INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_BAR_CLASSES | ICC_TAB_CLASSES | ICC_LISTVIEW_CLASSES };
    HWND parent, box, tb, tab, lv;
    HIMAGELIST il; HICON ico; SIZE ideal = {0}, ideal_wide = {0}; RECT r, tr;
    TBBUTTON b[5]; TCITEMW item = { TCIF_TEXT };
    COLORREF c, bk = RGB(10, 200, 30);
    HDC dc;
    int i;

    InitCommonControlsEx(&icc);
    parent = CreateWindowExW(0, L"STATIC", L"walkui", WS_POPUP | WS_VISIBLE, 0, 0, 640, 480, 0, 0, 0, 0);

    /* 1. a wrapping list-style toolbar in a box of its own, the box sized by the layout to the
     *    toolbar's ideal width (WireGuard: Add Tunnel, Remove, Export) */
    box = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE, 0, 0, 30, 200, parent, 0, 0, 0);
    tb = CreateWindowExW(0, TOOLBARCLASSNAMEW, NULL, WS_CHILD | WS_VISIBLE | CCS_TOP | CCS_NODIVIDER |
                         TBSTYLE_FLAT | TBSTYLE_LIST | TBSTYLE_TOOLTIPS | TBSTYLE_WRAPABLE | TBSTYLE_TRANSPARENT,
                         0, 0, 0, 0, box, (HMENU)1, 0, 0);
    orig_tb = (WNDPROC)SetWindowLongPtrW(tb, GWLP_WNDPROC, (LONG_PTR)walk_tb);
    SendMessageW(tb, TB_BUTTONSTRUCTSIZE, sizeof(TBBUTTON), 0);
    SendMessageW(tb, TB_SETEXTENDEDSTYLE, 0, TBSTYLE_EX_DRAWDDARROWS | TBSTYLE_EX_MIXEDBUTTONS);
    il = ImageList_Create(16, 16, ILC_COLOR32 | ILC_MASK, 3, 0);
    ico = LoadIconW(NULL, (LPCWSTR)IDI_APPLICATION);
    for (i = 0; i < 3; i++) ImageList_AddIcon(il, ico);
    SendMessageW(tb, TB_SETIMAGELIST, 0, (LPARAM)il);
    memset(b, 0, sizeof(b));
    b[0].iBitmap = 0; b[0].idCommand = 100; b[0].fsState = TBSTATE_ENABLED;
    b[0].fsStyle = BTNS_AUTOSIZE | BTNS_SHOWTEXT | BTNS_DROPDOWN; b[0].iString = (INT_PTR)L"Add Tunnel";
    b[1].iBitmap = 8; b[1].fsStyle = BTNS_SEP;
    b[2].iBitmap = 1; b[2].idCommand = 101; b[2].fsStyle = BTNS_AUTOSIZE; b[2].iString = (INT_PTR)L"Remove";
    b[3].iBitmap = 8; b[3].fsStyle = BTNS_SEP;
    b[4].iBitmap = 2; b[4].idCommand = 102; b[4].fsStyle = BTNS_AUTOSIZE; b[4].iString = (INT_PTR)L"Export";
    SendMessageW(tb, TB_ADDBUTTONSW, 5, (LPARAM)b);
    SendMessageW(tb, TB_AUTOSIZE, 0, 0);
    SetWindowPos(tb, 0, 0, 0, 30, 22, SWP_NOZORDER | SWP_NOMOVE);   /* laid out narrow first */
    SendMessageW(tb, TB_GETIDEALSIZE, FALSE, (LPARAM)&ideal);
    SetWindowPos(box, 0, 0, 0, ideal.cx, 200, SWP_NOZORDER | SWP_NOMOVE);
    SetWindowPos(tb, 0, 0, 0, ideal.cx, 22, SWP_NOZORDER | SWP_NOMOVE);
    SendMessageW(tb, TB_GETIDEALSIZE, FALSE, (LPARAM)&ideal_wide);
    SendMessageW(tb, TB_GETITEMRECT, 4, (LPARAM)&tr);
    GetClientRect(tb, &r);
    printf("TB_IDEAL %ld %ld last %ld\n", ideal.cx, ideal_wide.cx, tr.right);
    printf("TB_ROWS %ld\n", (long)SendMessageW(tb, TB_GETROWS, 0, 0));

    /* 2. a tab control printed into a black bitmap with PRF_ERASEBKGND: the strip beside the tabs */
    tab = CreateWindowExW(0, WC_TABCONTROLW, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
                          200, 0, 400, 200, parent, (HMENU)2, 0, 0);
    item.pszText = (WCHAR *)L"Tunnels"; SendMessageW(tab, TCM_INSERTITEMW, 0, (LPARAM)&item);
    item.pszText = (WCHAR *)L"Log"; SendMessageW(tab, TCM_INSERTITEMW, 1, (LPARAM)&item);
    c = print_pixel(tab, PRF_CLIENT | PRF_CHILDREN | PRF_ERASEBKGND, 390, 5);
    printf("TAB_STRIP %06lx\n", c);

    /* 3. a double-buffered report list view whose WM_ERASEBKGND is answered elsewhere */
    lv = CreateWindowExW(0, WC_LISTVIEWW, NULL, WS_CHILD | WS_VISIBLE | LVS_REPORT, 0, 220, 200, 200, parent, (HMENU)3, 0, 0);
    SendMessageW(lv, LVM_SETEXTENDEDLISTVIEWSTYLE, LVS_EX_DOUBLEBUFFER, LVS_EX_DOUBLEBUFFER);
    SendMessageW(lv, LVM_SETBKCOLOR, 0, bk);
    orig_lv = (WNDPROC)SetWindowLongPtrW(lv, GWLP_WNDPROC, (LONG_PTR)walk_lv);
    UpdateWindow(parent);
    /* what is behind it is red; a repaint must cover it with the list's own background */
    dc = GetDC(lv); GetClientRect(lv, &r);
    FillRect(dc, &r, GetStockObject(BLACK_BRUSH));
    { HBRUSH red = CreateSolidBrush(RGB(255, 0, 0)); FillRect(dc, &r, red); DeleteObject(red); }
    ReleaseDC(lv, dc);
    InvalidateRect(lv, NULL, FALSE);
    UpdateWindow(lv);
    dc = GetDC(lv); c = GetPixel(dc, 100, 150); ReleaseDC(lv, dc);
    printf("LV_PAINT %06lx want %06lx\n", c, bk);
    printf("LV_PRINT %06lx want %06lx\n", print_pixel(lv, PRF_CLIENT, 100, 150), bk);
    fflush(stdout);
    return 0;
}
