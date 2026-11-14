/* accentall-probe: the accent reaches what was still drawn in the default purple
 * (wine-sg 7746-7748); see accentall-gate.sh.
 *
 *   accentall.exe accent RRGGBB   set Personalization's accent (DWM AccentColor)
 *   accentall.exe announce RRGGBB ...and tell everyone, as Settings does
 *   accentall.exe theme           the visual style: a check box under the pointer, in the
 *                                 accent set before the process started; the system
 *                                 colours the shell applies (RefreshImmersiveColorPolicyState);
 *                                 then a new accent, live: both follow it
 *   accentall.exe icons DIR       an icon of a program of our shell (a copy of this
 *                                 probe in DIR, ...\usr\libexec\stained-glass\...)
 *                                 takes the accent; the same icon in a program
 *                                 elsewhere keeps its purple
 *   accentall.exe title           (run from DIR) a window's class icon, shared, takes
 *                                 a new accent on WM_SETTINGCHANGE "ImmersiveColorSet"
 */
#include <windows.h>
#include <uxtheme.h>
#include <vssym32.h>
#include <shellapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static int failures;
static void ok(BOOL cond, const char *what, double got, double want)
{
    printf("%s %s (hue %.0f, want %.0f)\n", cond ? "ok  " : "FAIL", what, got, want);
    if (!cond) failures++;
}

static double hue_of(COLORREF c, double *s, double *v)
{
    double r = GetRValue(c) / 255.0, g = GetGValue(c) / 255.0, b = GetBValue(c) / 255.0;
    double hi = max(r, max(g, b)), lo = min(r, min(g, b)), d = hi - lo, h;
    *v = hi; *s = hi > 0 ? d / hi : 0;
    if (d <= 0) return -1;
    if (hi == r) h = 60 * ((g - b) / d); else if (hi == g) h = 60 * ((b - r) / d + 2); else h = 60 * ((r - g) / d + 4);
    return h < 0 ? h + 360 : h;
}

static BOOL hue_near(double a, double b)
{
    double d = fabs(a - b);
    if (a < 0) return FALSE;
    return min(d, 360 - d) <= 14;
}

/* the mean hue of a 32-bpp top-down pixel run's strongly coloured pixels */
static double mean_hue(const DWORD *bits, int count)
{
    double sx = 0, sy = 0, h, s, v;
    int i, n = 0;
    for (i = 0; i < count; i++)
    {
        DWORD p = bits[i];
        if (!(p >> 24) && (p & 0xffffff) == 0) continue;
        h = hue_of(RGB((p >> 16) & 0xff, (p >> 8) & 0xff, p & 0xff), &s, &v);
        if (h < 0 || s <= 0.45 || v <= 0.25) continue;
        sx += cos(h * M_PI / 180); sy += sin(h * M_PI / 180); n++;
    }
    if (n < 6) return -1;
    h = atan2(sy, sx) * 180 / M_PI;
    return h < 0 ? h + 360 : h;
}

static double accent_hue(COLORREF c) { double s, v; return hue_of(c, &s, &v); }

static void set_accent(const char *hex)
{
    unsigned long v = strtoul(hex, NULL, 16);
    DWORD abgr = 0xff000000 | (v & 0xff) << 16 | (v & 0xff00) | (v >> 16 & 0xff);
    HKEY key;
    if (!RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\DWM", 0, NULL, 0, KEY_SET_VALUE, NULL, &key, NULL))
    {
        RegSetValueExW(key, L"AccentColor", 0, REG_DWORD, (BYTE *)&abgr, sizeof(abgr));
        RegCloseKey(key);
    }
}

static COLORREF read_accent(void)
{
    DWORD v = 0, size = sizeof(v);
    RegGetValueW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\DWM", L"AccentColor", RRF_RT_REG_DWORD, NULL, &v, &size);
    return v & 0xffffff;
}

static double checkbox_hue(void)
{
    BITMAPINFO bmi = {{ sizeof(BITMAPINFOHEADER), 32, -32, 1, 32, BI_RGB }};
    DWORD *bits;
    HDC dc = CreateCompatibleDC(NULL);
    HBITMAP bmp = CreateDIBSection(dc, &bmi, DIB_RGB_COLORS, (void **)&bits, NULL, 0);
    RECT r = { 0, 0, 32, 32 };
    HTHEME theme;
    double h = -1;

    SelectObject(dc, bmp);
    FillRect(dc, &r, GetStockObject(WHITE_BRUSH));
    if ((theme = OpenThemeData(NULL, L"BUTTON")))
    {
        SetRect(&r, 0, 0, 26, 26);
        DrawThemeBackground(theme, dc, BP_CHECKBOX, CBS_UNCHECKEDHOT, &r, NULL);   /* hot: drawn in the accent */
        GdiFlush();
        h = mean_hue(bits, 32 * 32);
        CloseThemeData(theme);
    }
    DeleteDC(dc);
    DeleteObject(bmp);
    return h;
}

static double icon_hue(HICON icon)
{
    ICONINFO info;
    BITMAPINFO bmi = {{ sizeof(BITMAPINFOHEADER), 0, 0, 1, 32, BI_RGB }};
    DWORD bits[64 * 64];
    BITMAP bm;
    HDC dc;
    double h = -1;

    if (!icon || !GetIconInfo(icon, &info)) return -1;
    dc = CreateCompatibleDC(NULL);
    if (info.hbmColor && GetObjectW(info.hbmColor, sizeof(bm), &bm) && bm.bmWidth <= 64 && bm.bmHeight <= 64)
    {
        bmi.bmiHeader.biWidth = bm.bmWidth;
        bmi.bmiHeader.biHeight = -bm.bmHeight;
        if (GetDIBits(dc, info.hbmColor, 0, bm.bmHeight, bits, &bmi, DIB_RGB_COLORS)) h = mean_hue(bits, bm.bmWidth * bm.bmHeight);
    }
    DeleteDC(dc);
    if (info.hbmColor) DeleteObject(info.hbmColor);
    if (info.hbmMask) DeleteObject(info.hbmMask);
    return h;
}

static void refresh_policy(void)   /* what the shell does on "ImmersiveColorSet" */
{
    void (WINAPI *refresh)(void) = (void *)GetProcAddress(LoadLibraryW(L"uxtheme.dll"), MAKEINTRESOURCEA(104));
    if (refresh) refresh();
}

static int do_theme(void)
{
    COLORREF accent = read_accent();
    double h;

    if (!IsThemeActive()) { printf("FAIL the visual style is not active\n"); return 1; }
    printf("accent %06lx\n", (unsigned long)accent);
    h = checkbox_hue();
    ok(hue_near(h, accent_hue(accent)), "theme: a check box under the pointer is drawn in the accent set before the program started", h, accent_hue(accent));
    refresh_policy();
    h = accent_hue(GetSysColor(COLOR_HIGHLIGHT));
    ok(hue_near(h, accent_hue(accent)), "theme: the selection's colour (COLOR_HIGHLIGHT) is the accent's", h, accent_hue(accent));
    h = accent_hue(GetSysColor(COLOR_HOTLIGHT));
    ok(hue_near(h, accent_hue(accent)), "theme: the hot-track colour (COLOR_HOTLIGHT) too", h, accent_hue(accent));

    set_accent("ca5010");   /* a new accent, live */
    refresh_policy();
    accent = read_accent();
    h = checkbox_hue();
    ok(hue_near(h, accent_hue(accent)), "theme: a new accent, live: a check box drawn again takes it", h, accent_hue(accent));
    h = accent_hue(GetSysColor(COLOR_HIGHLIGHT));
    ok(hue_near(h, accent_hue(accent)), "theme: ...and the selection's colour", h, accent_hue(accent));
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}

static int do_icons(const char *dir)
{
    WCHAR self[MAX_PATH], copy[MAX_PATH], wdir[MAX_PATH];
    HICON large = 0;
    COLORREF accent = read_accent();
    double h;

    GetModuleFileNameW(NULL, self, MAX_PATH);
    MultiByteToWideChar(CP_ACP, 0, dir, -1, wdir, MAX_PATH);
    swprintf(copy, MAX_PATH, L"%ls\\accentall.exe", wdir);
    ExtractIconExW(copy, 0, &large, NULL, 1);
    h = icon_hue(large);
    ok(hue_near(h, accent_hue(accent)), "icons: our shell's program's icon (in usr\\libexec\\stained-glass) takes the accent", h, accent_hue(accent));
    if (large) DestroyIcon(large);
    large = 0;
    ExtractIconExW(self, 0, &large, NULL, 1);
    h = icon_hue(large);
    ok(hue_near(h, 271), "icons: another program's icon in the same purple keeps it", h, 271);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}

static LRESULT CALLBACK proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) { return DefWindowProcW(hwnd, msg, wp, lp); }

static int do_title(void)
{
    WNDCLASSW wc = { 0 };
    HICON icon;
    HWND hwnd;
    double h;

    set_accent("ca5010");
    icon = LoadImageW(GetModuleHandleW(NULL), MAKEINTRESOURCEW(1), IMAGE_ICON, 32, 32, LR_SHARED);
    h = icon_hue(icon);
    ok(hue_near(h, accent_hue(read_accent())), "title: our program's window icon (shared) is made in the accent", h, accent_hue(read_accent()));
    wc.lpfnWndProc = proc; wc.hInstance = GetModuleHandleW(NULL); wc.hIcon = icon; wc.lpszClassName = L"accentall";
    RegisterClassW(&wc);
    hwnd = CreateWindowW(L"accentall", L"accentall", WS_OVERLAPPEDWINDOW, 0, 0, 200, 100, NULL, NULL, wc.hInstance, NULL);
    set_accent("0078d7");
    SendMessageW(hwnd, WM_SETTINGCHANGE, 0, (LPARAM)L"ImmersiveColorSet");
    h = icon_hue((HICON)GetClassLongPtrW(hwnd, GCLP_HICON));
    ok(hue_near(h, accent_hue(read_accent())), "title: a new accent (WM_SETTINGCHANGE \"ImmersiveColorSet\"): the window's icon takes it", h,
       accent_hue(read_accent()));
    h = icon_hue((HICON)GetClassLongPtrW(hwnd, GCLP_HICONSM));
    ok(hue_near(h, accent_hue(read_accent())), "title: ...and the small icon its title bar draws (made from it when the class was registered)", h,
       accent_hue(read_accent()));
    DestroyWindow(hwnd);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}

int main(int argc, char **argv)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    if (argc > 2 && !strcmp(argv[1], "accent")) { set_accent(argv[2]); return 0; }
    if (argc > 2 && !strcmp(argv[1], "announce"))   /* as Settings does */
    {
        DWORD_PTR r;
        set_accent(argv[2]);
        SendMessageTimeoutW(HWND_BROADCAST, WM_SETTINGCHANGE, 0, (LPARAM)L"ImmersiveColorSet", SMTO_ABORTIFHUNG, 2000, &r);
        return 0;
    }
    if (argc > 1 && !strcmp(argv[1], "theme")) return do_theme();
    if (argc > 2 && !strcmp(argv[1], "icons")) return do_icons(argv[2]);
    if (argc > 1 && !strcmp(argv[1], "title")) return do_title();
    if (argc > 2 && !strcmp(argv[1], "classicon"))   /* another program's window class's small icon: its hue */
    {
        WCHAR cls[64];
        HWND w;
        MultiByteToWideChar(CP_ACP, 0, argv[2], -1, cls, 64);
        w = FindWindowW(cls, NULL);
        HICON icon = NULL;
        /* (a class's icon is its process's own: asked of the window, as the taskbar does) */
        if (w) SendMessageTimeoutW(w, WM_GETICON, ICON_SMALL2, 0, SMTO_ABORTIFHUNG, 2000, (DWORD_PTR *)&icon);
        printf("hue %.0f window %p icon %p\n", icon ? icon_hue(icon) : -1.0, w, icon);
        return 0;
    }
    printf("usage: accentall.exe accent RRGGBB | theme | icons DIR | title\n");
    return 2;
}
