/* ITaskbarList3 reaches the taskbar (patches/sg/1639).
 *
 * ActivateTab, SetActiveAlt and MarkFullscreenWindow were stubs returning
 * E_NOTIMPL, and SetProgressValue, SetProgressState and SetOverlayIcon
 * returned S_OK and did nothing; the taskbar never posted
 * "TaskbarButtonCreated", which programs wait for before they use
 * ITaskbarList3. The probe runs in the shell's desktop beside explorer's
 * taskbar, waits for the message, then reads its button's pixels from the
 * screen: progress fills it (green, red on an error, yellow when paused, a
 * moving band when indeterminate), an overlay icon shows, ActivateTab marks
 * another button, and a window marked full screen has the bar behind it.
 */
#define COBJMACROS
#include <windows.h>
#include <shobjidl.h>
#include <stdio.h>
#include <string.h>

static int failures;
static HWND found_button;
static UINT button_created;

static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

static LRESULT CALLBACK wndproc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == button_created && msg)
    {
        SetPropA(hwnd, "created", (HANDLE)1);
        return 0;
    }
    if (msg == WM_PAINT)
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        FillRect(hdc, &ps.rcPaint, GetStockObject(WHITE_BRUSH));
        EndPaint(hwnd, &ps);
        return 0;
    }
    return DefWindowProcA(hwnd, msg, wp, lp);
}

static void pump(DWORD ms)
{
    DWORD end = GetTickCount() + ms;
    MSG msg;

    do
    {
        while (PeekMessageA(&msg, 0, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }
        Sleep(20);
    } while ((LONG)(end - GetTickCount()) > 0);
}

static BOOL CALLBACK find_button(HWND child, LPARAM lp)
{
    if ((HWND)GetWindowLongPtrA(child, GWLP_ID) == (HWND)lp && IsWindowVisible(child))
    {
        found_button = child;
        return FALSE;
    }
    return TRUE;
}

static HWND button_of(HWND tray, HWND hwnd)
{
    found_button = NULL;
    EnumChildWindows(tray, find_button, (LPARAM)hwnd);
    return found_button;
}

/* the button's pixels, from the screen */
struct shot { int w, h; DWORD *bits; };

static BOOL grab(HWND button, struct shot *shot)
{
    BITMAPINFO bmi = {{ sizeof(bmi.bmiHeader) }};
    HDC screen, mem;
    HBITMAP bmp;
    RECT rc;
    void *bits;

    if (!GetWindowRect(button, &rc)) return FALSE;
    shot->w = rc.right - rc.left;
    shot->h = rc.bottom - rc.top;
    bmi.bmiHeader.biWidth = shot->w;
    bmi.bmiHeader.biHeight = -shot->h;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    screen = GetDC(NULL);
    mem = CreateCompatibleDC(screen);
    bmp = CreateDIBSection(mem, &bmi, DIB_RGB_COLORS, &bits, NULL, 0);
    SelectObject(mem, bmp);
    BitBlt(mem, 0, 0, shot->w, shot->h, screen, rc.left, rc.top, SRCCOPY);
    free(shot->bits);
    shot->bits = malloc(shot->w * shot->h * 4);
    memcpy(shot->bits, bits, shot->w * shot->h * 4);
    DeleteDC(mem);
    DeleteObject(bmp);
    ReleaseDC(NULL, screen);
    return TRUE;
}

static int close_to(DWORD px, int r, int g, int b)
{
    int pr = (px >> 16) & 0xff, pg = (px >> 8) & 0xff, pb = px & 0xff;
    return abs(pr - r) < 40 && abs(pg - g) < 40 && abs(pb - b) < 40;
}

/* how many pixels of a colour, and the rightmost column that has any */
static int count(const struct shot *s, int r, int g, int b, int *left, int *right)
{
    int x, y, n = 0;
    if (left) *left = s->w;
    if (right) *right = -1;
    for (y = 0; y < s->h; y++)
        for (x = 0; x < s->w; x++)
            if (close_to(s->bits[y * s->w + x], r, g, b))
            {
                n++;
                if (left && x < *left) *left = x;
                if (right && x > *right) *right = x;
            }
    return n;
}

#define GREEN  0x06,0xB0,0x25
#define RED    0xE8,0x11,0x23
#define YELLOW 0xFF,0xC1,0x07
#define BLUE   0x00,0x00,0xFF

static HICON blue_icon(void)
{
    BITMAPINFO bmi = {{ sizeof(bmi.bmiHeader), 16, 16, 1, 32 }};
    ICONINFO info = { TRUE };
    DWORD *bits;
    HICON icon;
    int i;

    info.hbmColor = CreateDIBSection(NULL, &bmi, DIB_RGB_COLORS, (void **)&bits, NULL, 0);
    for (i = 0; i < 256; i++) bits[i] = 0xff0000ff;
    info.hbmMask = CreateBitmap(16, 16, 1, 1, NULL);
    icon = CreateIconIndirect(&info);
    DeleteObject(info.hbmColor);
    DeleteObject(info.hbmMask);
    return icon;
}

static HWND make_window(const char *title, int x)
{
    HWND hwnd = CreateWindowA("SGProgressProbe", title, WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                              x, 100, 300, 200, NULL, NULL, GetModuleHandleA(NULL), NULL);
    return hwnd;
}

int main(void)
{
    WNDCLASSA wc = { 0, wndproc, 0, 0, NULL, NULL, NULL, NULL, NULL, "SGProgressProbe" };
    struct shot shot = { 0 }, shot2 = { 0 };
    ITaskbarList3 *list;
    HWND hwnd, other, tray, button, other_button;
    int n, n2, left, right, left2, right2, i, area;
    DWORD accent, px;
    HICON icon;
    HRESULT hr;

    CoInitialize(NULL);
    wc.hInstance = GetModuleHandleA(NULL);
    wc.hCursor = LoadCursorA(NULL, (const char *)IDC_ARROW);
    RegisterClassA(&wc);
    button_created = RegisterWindowMessageA("TaskbarButtonCreated");

    tray = FindWindowA("Shell_TrayWnd", NULL);
    check(tray != NULL, "the taskbar is there");
    if (!tray) { printf("RESULT: FAIL\n"); return 1; }

    hwnd = make_window("Progress One", 100);
    SetForegroundWindow(hwnd);
    for (i = 0; i < 150 && !GetPropA(hwnd, "created"); i++) pump(100);
    check(GetPropA(hwnd, "created") != NULL, "the taskbar posts TaskbarButtonCreated to a new window");
    for (i = 0; i < 50 && !(button = button_of(tray, hwnd)); i++) pump(100);
    check(button != NULL, "the window has a button");
    if (!button) { printf("RESULT: FAIL\n"); return 1; }
    pump(800);

    hr = CoCreateInstance(&CLSID_TaskbarList, NULL, CLSCTX_INPROC_SERVER, &IID_ITaskbarList3, (void **)&list);
    check(hr == S_OK, "CoCreateInstance(CLSID_TaskbarList)");
    if (hr != S_OK) { printf("RESULT: FAIL\n"); return 1; }
    ITaskbarList3_HrInit(list);

    grab(button, &shot);
    area = shot.w * shot.h;
    printf("button %dx%d\n", shot.w, shot.h);
    n = count(&shot, GREEN, NULL, NULL);
    check(n == 0, "no progress shown at first");

    hr = ITaskbarList3_SetProgressValue(list, hwnd, 50, 100);
    pump(700);
    grab(button, &shot);
    n = count(&shot, GREEN, &left, &right);
    printf("50%%: %d green pixels, columns %d..%d of %d\n", n, left, right, shot.w);
    check(hr == S_OK && n > area / 5 && left <= 3 && right > shot.w * 4 / 10 && right < shot.w * 6 / 10,
          "SetProgressValue(50 of 100) fills the left half green");

    hr = ITaskbarList3_SetProgressState(list, hwnd, TBPF_ERROR);
    pump(700);
    grab(button, &shot);
    n = count(&shot, RED, NULL, &right);
    n2 = count(&shot, GREEN, NULL, NULL);
    printf("error: %d red, %d green\n", n, n2);
    check(hr == S_OK && n > area / 5 && n2 == 0, "TBPF_ERROR shows it red");

    ITaskbarList3_SetProgressState(list, hwnd, TBPF_PAUSED);
    ITaskbarList3_SetProgressValue(list, hwnd, 100, 100);
    pump(700);
    grab(button, &shot);
    n = count(&shot, YELLOW, NULL, &right);
    printf("paused at 100%%: %d yellow, rightmost %d\n", n, right);
    check(n > area / 2 && right >= shot.w - 4, "paused stays paused (yellow) when the value changes, and fills it");

    hr = ITaskbarList3_SetProgressState(list, hwnd, TBPF_NOPROGRESS);
    pump(700);
    grab(button, &shot);
    n = count(&shot, YELLOW, NULL, NULL) + count(&shot, GREEN, NULL, NULL) + count(&shot, RED, NULL, NULL);
    check(hr == S_OK && n == 0, "TBPF_NOPROGRESS takes it away");

    hr = ITaskbarList3_SetProgressState(list, hwnd, 3);
    check(hr == E_INVALIDARG, "an unknown progress state is refused");

    ITaskbarList3_SetProgressState(list, hwnd, TBPF_INDETERMINATE);
    pump(500);
    grab(button, &shot);
    n = count(&shot, GREEN, &left, &right);
    pump(300);
    grab(button, &shot2);
    n2 = count(&shot2, GREEN, &left2, &right2);
    printf("indeterminate: %d green at %d..%d, then %d at %d..%d\n", n, left, right, n2, left2, right2);
    check(n > 0 && n2 > 0 && (left != left2 || right != right2), "TBPF_INDETERMINATE shows a moving band");
    ITaskbarList3_SetProgressState(list, hwnd, TBPF_NOPROGRESS);

    icon = blue_icon();
    hr = ITaskbarList3_SetOverlayIcon(list, hwnd, icon, L"Blue");
    DestroyIcon(icon);  /* the taskbar keeps its own copy */
    pump(700);
    grab(button, &shot);
    n = count(&shot, BLUE, NULL, NULL);
    printf("overlay: %d blue pixels\n", n);
    check(hr == S_OK && n >= 50, "SetOverlayIcon shows the icon on the button");
    ITaskbarList3_SetOverlayIcon(list, hwnd, NULL, NULL);
    pump(700);
    grab(button, &shot);
    check(count(&shot, BLUE, NULL, NULL) == 0, "SetOverlayIcon(NULL) takes it away");

    /* ActivateTab: another window's button shows as the active one */
    other = make_window("Progress Two", 450);
    for (i = 0; i < 150 && !(other_button = button_of(tray, other)); i++) pump(100);
    SetForegroundWindow(hwnd);
    pump(1000);
    grab(button, &shot);
    /* the active button's colour: the middle of its bottom edge region */
    accent = shot.bits[(shot.h - 2) * shot.w + shot.w / 2] & 0xffffff;
    grab(other_button, &shot2);
    px = shot2.bits[(shot2.h - 2) * shot2.w + shot2.w / 2] & 0xffffff;
    printf("active bottom %06lx, other %06lx\n", accent, px);
    hr = ITaskbarList3_ActivateTab(list, other);
    pump(800);
    grab(other_button, &shot2);
    grab(button, &shot);
    printf("after ActivateTab: other %06lx, first %06lx\n", shot2.bits[(shot2.h - 2) * shot2.w + shot2.w / 2] & 0xffffff,
           shot.bits[(shot.h - 2) * shot.w + shot.w / 2] & 0xffffff);
    check(hr == S_OK && accent != px &&
          (shot2.bits[(shot2.h - 2) * shot2.w + shot2.w / 2] & 0xffffff) == accent &&
          (shot.bits[(shot.h - 2) * shot.w + shot.w / 2] & 0xffffff) != accent,
          "ActivateTab shows the other window's button as the active one, without activating it");
    check(GetForegroundWindow() == hwnd, "... and the foreground window stays");

    /* MarkFullscreenWindow: the bar goes behind the window while it is in front */
    check(GetWindowLongA(tray, GWL_EXSTYLE) & WS_EX_TOPMOST, "the bar is topmost");
    hr = ITaskbarList3_MarkFullscreenWindow(list, hwnd, TRUE);
    pump(800);
    check(hr == S_OK && !(GetWindowLongA(tray, GWL_EXSTYLE) & WS_EX_TOPMOST),
          "MarkFullscreenWindow(TRUE): the bar is no longer topmost while the window is in front");
    ITaskbarList3_MarkFullscreenWindow(list, hwnd, FALSE);
    pump(800);
    check(GetWindowLongA(tray, GWL_EXSTYLE) & WS_EX_TOPMOST, "MarkFullscreenWindow(FALSE): topmost again");

    /* calls for windows that are not there succeed, as on Windows */
    check(ITaskbarList3_SetProgressValue(list, (HWND)0xdeadbeef, 1, 2) == S_OK &&
          ITaskbarList3_ActivateTab(list, NULL) == S_OK && ITaskbarList3_SetActiveAlt(list, NULL) == S_OK,
          "invalid windows are accepted (S_OK)");

    ITaskbarList3_Release(list);
    DestroyWindow(other);
    DestroyWindow(hwnd);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
