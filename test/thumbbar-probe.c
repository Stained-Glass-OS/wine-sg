/* Taskbar thumbnails (patches/sg/1659), run by test/thumbbar-gate.sh in the
 * shell's desktop beside explorer's taskbar. Hovering over a window's
 * taskbar button shows its thumbnail -- the window as it looks -- with the
 * thumbnail toolbar ITaskbarList3 gave it (ThumbBarSetImageList,
 * ThumbBarAddButtons, ThumbBarUpdateButtons: a disabled button, a hidden
 * one, tooltips); a click on a toolbar button sends the window WM_COMMAND
 * with THBN_CLICKED. Registered tabs (RegisterTab, SetTabOrder) show as
 * thumbnails of their own, in their order, with the iconic bitmap each
 * gives when asked (WM_DWMSENDICONICTHUMBNAIL, DwmSetIconicThumbnail), and
 * a click on one activates its proxy window. These were stubs. */
#define COBJMACROS
#include <windows.h>
#include <commctrl.h>
#include <shobjidl.h>
#include <dwmapi.h>
#include <stdio.h>
#include <string.h>

static int failures;
static UINT button_created;
static HWND main_hwnd, tab1, tab2, found;
static int clicked_id = -1, iconic_asks, tab1_activated;

static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

static HBITMAP solid_bitmap(int w, int h, DWORD argb)
{
    BITMAPINFO bmi = {{ sizeof(bmi.bmiHeader) }};
    DWORD *bits;
    HBITMAP bmp;
    int i;
    bmi.bmiHeader.biWidth = w;
    bmi.bmiHeader.biHeight = -h;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmp = CreateDIBSection(NULL, &bmi, DIB_RGB_COLORS, (void **)&bits, NULL, 0);
    for (i = 0; i < w * h; i++) bits[i] = argb;
    return bmp;
}

static LRESULT CALLBACK wndproc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == button_created && msg)
    {
        SetPropA(hwnd, "created", (HANDLE)1);
        return 0;
    }
    switch (msg)
    {
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        HBRUSH br = CreateSolidBrush(RGB(0, 0, 255));
        FillRect(hdc, &ps.rcPaint, br);
        DeleteObject(br);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_COMMAND:
        if (HIWORD(wp) == THBN_CLICKED) clicked_id = LOWORD(wp);
        return 0;
    case WM_DWMSENDICONICTHUMBNAIL:
    {
        HBITMAP bmp = solid_bitmap(HIWORD(lp) ? HIWORD(lp) : 100, LOWORD(lp) ? LOWORD(lp) : 60,
                                   hwnd == tab1 ? 0xffff0000 : 0xff00ff00);
        iconic_asks++;
        DwmSetIconicThumbnail(hwnd, bmp, 0);
        DeleteObject(bmp);
        return 0;
    }
    case WM_ACTIVATE:
        if (hwnd == tab1 && LOWORD(wp) != WA_INACTIVE) tab1_activated = 1;
        break;
    }
    return DefWindowProcA(hwnd, msg, wp, lp);
}

static void pump(DWORD ms)
{
    DWORD end = GetTickCount() + ms;
    MSG msg;
    do
    {
        while (PeekMessageA(&msg, 0, 0, 0, PM_REMOVE)) { TranslateMessage(&msg); DispatchMessageA(&msg); }
        Sleep(20);
    } while ((LONG)(end - GetTickCount()) > 0);
}

static BOOL CALLBACK find_button(HWND child, LPARAM lp)
{
    if ((HWND)GetWindowLongPtrA(child, GWLP_ID) == (HWND)lp && IsWindowVisible(child)) { found = child; return FALSE; }
    return TRUE;
}

static const char *find_title;
static BOOL CALLBACK find_tile(HWND child, LPARAM lp)
{
    char cls[64], text[128];
    GetClassNameA(child, cls, sizeof(cls));
    GetWindowTextA(child, text, sizeof(text));
    if (!strcmp(cls, "SGTaskbarThumbnail") && !strcmp(text, find_title)) { found = child; return FALSE; }
    return TRUE;
}

static HWND tile_of(HWND popup, const char *title)
{
    found = NULL;
    find_title = title;
    if (popup) EnumChildWindows(popup, find_tile, 0);
    return found;
}

static void move_to(int x, int y)
{
    INPUT in = {0};
    in.type = INPUT_MOUSE;
    in.mi.dx = x * 65535 / (GetSystemMetrics(SM_CXSCREEN) - 1);
    in.mi.dy = y * 65535 / (GetSystemMetrics(SM_CYSCREEN) - 1);
    in.mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE;
    SendInput(1, &in, sizeof(in));
}

static void click_at(int x, int y)
{
    INPUT in[2] = {{0}};
    move_to(x, y);
    pump(150);
    in[0].type = in[1].type = INPUT_MOUSE;
    in[0].mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
    in[1].mi.dwFlags = MOUSEEVENTF_LEFTUP;
    SendInput(2, in, sizeof(INPUT));
}

static void center(HWND hwnd, int *x, int *y, int dy)
{
    RECT rc;
    GetWindowRect(hwnd, &rc);
    *x = (rc.left + rc.right) / 2;
    *y = (rc.top + rc.bottom) / 2 + dy;
}

static HWND hover(HWND button)
{
    HWND popup = NULL;
    int x, y, i;
    move_to(500, 300);
    pump(800);
    center(button, &x, &y, 0);
    move_to(x - 3, y);
    pump(100);
    move_to(x, y);
    for (i = 0; i < 40 && !(popup = FindWindowA("SGTaskbarThumbnails", NULL)); i++) pump(100);
    pump(500);
    return popup;
}

static DWORD pixel_at(HWND tile, int fx, int fy)
{
    RECT rc;
    HDC screen = GetDC(NULL);
    DWORD px;
    GetWindowRect(tile, &rc);
    px = GetPixel(screen, rc.left + (rc.right - rc.left) * fx / 100, rc.top + (rc.bottom - rc.top) * fy / 100);
    ReleaseDC(NULL, screen);
    return px;
}

int main(void)
{
    WNDCLASSA wc = {0};
    ITaskbarList3 *tbl;
    HIMAGELIST himl;
    THUMBBUTTON tb[3];
    HWND tray, button, popup, b101, b102, b103, t_main, ta, tb_;
    BOOL on = TRUE;
    int x, y, i;
    DWORD px;
    RECT ra, rb;

    CoInitialize(NULL);
    InitCommonControls();
    button_created = RegisterWindowMessageA("TaskbarButtonCreated");
    wc.lpfnWndProc = wndproc;
    wc.hInstance = GetModuleHandleA(NULL);
    wc.hCursor = LoadCursorA(NULL, (LPCSTR)IDC_ARROW);
    wc.lpszClassName = "SGThumbProbe";
    RegisterClassA(&wc);
    main_hwnd = CreateWindowA("SGThumbProbe", "SG Thumb Main", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 200, 80, 400, 300,
                              NULL, NULL, NULL, NULL);
    for (i = 0; i < 50 && !GetPropA(main_hwnd, "created"); i++) pump(100);
    check(GetPropA(main_hwnd, "created") != NULL, "TaskbarButtonCreated");
    tray = FindWindowA("Shell_TrayWnd", NULL);
    found = NULL;
    EnumChildWindows(tray, find_button, (LPARAM)main_hwnd);
    button = found;
    check(button != NULL, "the window's taskbar button");
    if (FAILED(CoCreateInstance(&CLSID_TaskbarList, NULL, CLSCTX_INPROC_SERVER, &IID_ITaskbarList3, (void **)&tbl)) || !button)
    {
        printf("RESULT: FAIL\n");
        return 1;
    }
    ITaskbarList3_HrInit(tbl);

    himl = ImageList_Create(16, 16, ILC_COLOR32 | ILC_MASK, 2, 0);
    ImageList_AddIcon(himl, LoadIconA(NULL, (LPCSTR)IDI_INFORMATION));
    ImageList_AddIcon(himl, LoadIconA(NULL, (LPCSTR)IDI_WARNING));
    check(ITaskbarList3_ThumbBarSetImageList(tbl, main_hwnd, himl) == S_OK, "ThumbBarSetImageList");
    memset(tb, 0, sizeof(tb));
    for (i = 0; i < 3; i++)
    {
        tb[i].dwMask = THB_BITMAP | THB_TOOLTIP | THB_FLAGS;
        tb[i].iId = 101 + i;
        tb[i].iBitmap = i % 2;
    }
    wcscpy(tb[0].szTip, L"Play");
    wcscpy(tb[1].szTip, L"Next");
    tb[1].dwFlags = THBF_DISABLED;
    wcscpy(tb[2].szTip, L"Hidden");
    tb[2].dwFlags = THBF_HIDDEN;
    check(ITaskbarList3_ThumbBarAddButtons(tbl, main_hwnd, 3, tb) == S_OK, "ThumbBarAddButtons");
    check(ITaskbarList3_ThumbBarAddButtons(tbl, main_hwnd, 8, tb) == E_INVALIDARG, "more than seven buttons: E_INVALIDARG");
    check(ITaskbarList3_SetThumbnailTooltip(tbl, main_hwnd, L"SG main tip") == S_OK, "SetThumbnailTooltip");
    pump(300);

    popup = hover(button);
    check(popup && IsWindowVisible(popup), "hovering over the button shows its thumbnails");
    t_main = tile_of(popup, "SG Thumb Main");
    check(t_main != NULL, "the window's thumbnail, titled as the window");
    if (t_main)
    {
        px = pixel_at(t_main, 50, 60);
        printf("  thumbnail centre %06lx\n", px);
        check(GetBValue(px) > 160 && GetRValue(px) < 80 && GetGValue(px) < 80, "the thumbnail shows the window (blue)");
    }
    b101 = popup ? GetDlgItem(popup, 101) : NULL;
    b102 = popup ? GetDlgItem(popup, 102) : NULL;
    b103 = popup ? GetDlgItem(popup, 103) : NULL;
    check(b101 && IsWindowEnabled(b101) && b102 && !IsWindowEnabled(b102) && !b103,
          "the toolbar: a button, a disabled one, the hidden one not shown");
    if (b101)
    {
        center(b101, &x, &y, 0);
        click_at(x, y);
        for (i = 0; i < 30 && clicked_id < 0; i++) pump(100);
        check(clicked_id == 101, "a toolbar button sends WM_COMMAND THBN_CLICKED with its id");
    }

    /* the toolbar updated while shown */
    tb[0].dwFlags = THBF_HIDDEN;
    tb[0].dwMask = THB_FLAGS;
    ITaskbarList3_ThumbBarUpdateButtons(tbl, main_hwnd, 1, tb);
    pump(500);
    popup = FindWindowA("SGTaskbarThumbnails", NULL);
    check(popup && !GetDlgItem(popup, 101) && GetDlgItem(popup, 102), "ThumbBarUpdateButtons hides a button");

    /* tabs */
    tab1 = CreateWindowA("SGThumbProbe", "SG Tab One", WS_POPUP, 0, 0, 10, 10, NULL, NULL, NULL, NULL);
    tab2 = CreateWindowA("SGThumbProbe", "SG Tab Two", WS_POPUP, 0, 0, 10, 10, NULL, NULL, NULL, NULL);
    DwmSetWindowAttribute(tab1, DWMWA_HAS_ICONIC_BITMAP, &on, sizeof(on));
    DwmSetWindowAttribute(tab1, DWMWA_FORCE_ICONIC_REPRESENTATION, &on, sizeof(on));
    DwmSetWindowAttribute(tab2, DWMWA_HAS_ICONIC_BITMAP, &on, sizeof(on));
    DwmSetWindowAttribute(tab2, DWMWA_FORCE_ICONIC_REPRESENTATION, &on, sizeof(on));
    check(ITaskbarList3_RegisterTab(tbl, tab1, main_hwnd) == S_OK && ITaskbarList3_RegisterTab(tbl, tab2, main_hwnd) == S_OK,
          "RegisterTab");
    check(ITaskbarList3_SetTabOrder(tbl, tab2, tab1) == S_OK, "SetTabOrder");
    check(ITaskbarList3_SetTabActive(tbl, tab1, main_hwnd, 0) == S_OK, "SetTabActive");
    popup = hover(button);
    ta = tile_of(popup, "SG Tab One");
    tb_ = tile_of(popup, "SG Tab Two");
    check(ta && tb_ && !tile_of(popup, "SG Thumb Main"), "the tabs show in the window's place");
    if (ta && tb_)
    {
        GetWindowRect(ta, &ra);
        GetWindowRect(tb_, &rb);
        check(rb.left < ra.left, "in the order SetTabOrder gave");
        for (i = 0; i < 20 && iconic_asks < 2; i++) pump(100);
        pump(500);
        check(iconic_asks >= 2, "each tab is asked for its iconic thumbnail");
        px = pixel_at(ta, 50, 60);
        printf("  tab one thumbnail %06lx\n", px);
        check(GetRValue(px) > 160 && GetGValue(px) < 80, "the tab's thumbnail is the bitmap it gave (red)");
        center(ta, &x, &y, 0);
        click_at(x, y);
        for (i = 0; i < 30 && !tab1_activated; i++) pump(100);
        check(tab1_activated, "clicking a tab's thumbnail activates its proxy window");
    }
    check(ITaskbarList3_UnregisterTab(tbl, tab1) == S_OK, "UnregisterTab");
    check(ITaskbarList3_UnregisterTab(tbl, tab1) == E_INVALIDARG, "UnregisterTab of no tab: E_INVALIDARG");

    move_to(500, 300);
    pump(500);
    ITaskbarList3_Release(tbl);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    fflush(stdout);
    return failures != 0;
}
