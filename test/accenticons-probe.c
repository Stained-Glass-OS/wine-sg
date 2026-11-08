/* test/accenticons-gate.sh probe. "api": the icons we draw follow the accent colour
 * (HKCU\Software\Microsoft\Windows\DWM AccentColor) -- shell32's folder from ExtractIconEx,
 * user32's information icon from LoadIcon (shared: the same handle takes a new accent),
 * shell32's image list (an entry of ours made again in place), explorer.exe's own icon --
 * and a program's own icon, drawn in the very same purple, is never touched.
 * "accent RRGGBB": set the accent and tell everyone (WM_SETTINGCHANGE "ImmersiveColorSet"). */
#define COBJMACROS
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <commctrl.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

static int failures;
static void check(int ok, const char *what)
{
    printf("%s %s\n", ok ? "ok  " : "FAIL", what);
    if (!ok) failures++;
}

static void set_accent(COLORREF c, BOOL tell)
{
    DWORD v = 0xff000000 | GetBValue(c) << 16 | GetGValue(c) << 8 | GetRValue(c);
    HKEY key;
    RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\DWM", 0, NULL, 0, KEY_ALL_ACCESS, NULL, &key, NULL);
    RegSetValueExW(key, L"AccentColor", 0, REG_DWORD, (BYTE *)&v, sizeof(v));
    RegCloseKey(key);
    if (tell)
        SendMessageTimeoutW(HWND_BROADCAST, WM_SETTINGCHANGE, 0, (LPARAM)L"ImmersiveColorSet", SMTO_ABORTIFHUNG, 5000, NULL);
}

static double hue_of(double r, double g, double b, double *sat)
{
    double mx = max(r, max(g, b)), mn = min(r, min(g, b)), d = mx - mn, h;
    *sat = mx > 0 ? d / mx : 0;
    if (d <= 0) return 0;
    if (mx == r) h = 60 * ((g - b) / d);
    else if (mx == g) h = 60 * ((b - r) / d + 2);
    else h = 60 * ((r - g) / d + 4);
    return h < 0 ? h + 360 : h;
}

/* the hue most strongly coloured pixels of an icon have (opaque, saturation > 0.45); -1 if none */
static double icon_hue(HICON icon)
{
    BITMAPINFO bmi = {{ sizeof(BITMAPINFOHEADER), 32, -32, 1, 32, BI_RGB }};
    DWORD *bits;
    HDC hdc = CreateCompatibleDC(0);
    HBITMAP dib = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, (void **)&bits, NULL, 0), old;
    double sx = 0, sy = 0;
    int i, n = 0;

    old = SelectObject(hdc, dib);
    for (i = 0; i < 32 * 32; i++) bits[i] = 0xffffff;
    DrawIconEx(hdc, 0, 0, icon, 32, 32, 0, NULL, DI_NORMAL);
    GdiFlush();
    for (i = 0; i < 32 * 32; i++)
    {
        double s, h = hue_of(((bits[i] >> 16) & 0xff) / 255.0, ((bits[i] >> 8) & 0xff) / 255.0, (bits[i] & 0xff) / 255.0, &s);
        if (s < 0.45) continue;
        sx += cos(h * M_PI / 180); sy += sin(h * M_PI / 180); n++;
    }
    SelectObject(hdc, old); DeleteObject(dib); DeleteDC(hdc);
    if (n < 20) return -1;
    i = (int)(atan2(sy, sx) * 180 / M_PI + 360.5) % 360;
    return i;
}

static double hue_of_color(COLORREF c)
{
    double s;
    return hue_of(GetRValue(c) / 255.0, GetGValue(c) / 255.0, GetBValue(c) / 255.0, &s);
}

static int near_hue(double a, double b)
{
    double d = fabs(a - b);
    if (a < 0 || b < 0) return 0;
    if (d > 180) d = 360 - d;
    return d <= 14;
}

static HICON folder_icon(void)
{
    HICON big = NULL, small_icon = NULL;
    ExtractIconExW(L"shell32.dll", -4, &big, &small_icon, 1);   /* IDI_SHELL_FOLDER */
    if (small_icon) DestroyIcon(small_icon);
    return big;
}

int main(int argc, char **argv)
{
    static const COLORREF accents[] = { RGB(0x00, 0x78, 0xd7), RGB(0x10, 0x89, 0x3e), RGB(0x8a, 0x2b, 0xe2) };
    WCHAR self[MAX_PATH], dir[MAX_PATH], windows[MAX_PATH];
    HICON info_first = NULL;
    HIMAGELIST list = NULL;
    SHFILEINFOW sfi;
    char msg[200];
    int i, sys_index;

    setvbuf(stdout, NULL, _IONBF, 0);
    if (argc > 2 && !strcmp(argv[1], "accent"))
    {
        unsigned long v = strtoul(argv[2], NULL, 16);
        set_accent(RGB((v >> 16) & 0xff, (v >> 8) & 0xff, v & 0xff), TRUE);
        return 0;
    }
    CoInitialize(NULL);
    GetModuleFileNameW(NULL, self, MAX_PATH);
    GetWindowsDirectoryW(windows, MAX_PATH);
    lstrcpyW(dir, windows);
    sys_index = -1;
    for (i = 0; i < ARRAYSIZE(accents); i++)
    {
        COLORREF a = accents[i];
        double want = hue_of_color(a), h;
        HICON icon, info, own = NULL, own_small = NULL, expl = NULL, expl_small = NULL;
        WCHAR path[MAX_PATH];

        set_accent(a, FALSE);
        printf("accent %02x%02x%02x (hue %.0f)\n", GetRValue(a), GetGValue(a), GetBValue(a), want);
        icon = folder_icon();
        h = icon ? icon_hue(icon) : -1;
        sprintf(msg, "shell32's folder follows the accent (hue %.0f)", h);
        check(near_hue(h, want), msg);
        if (icon) DestroyIcon(icon);

        info = LoadIconW(NULL, (LPCWSTR)IDI_INFORMATION);
        h = info ? icon_hue(info) : -1;
        sprintf(msg, "user32's information icon (shared) follows it (hue %.0f)", h);
        check(near_hue(h, want), msg);
        if (!info_first) info_first = info;
        else check(info == info_first, "the shared icon keeps its handle");

        ExtractIconExW(self, 0, &own, &own_small, 1);
        h = own ? icon_hue(own) : -1;
        sprintf(msg, "the program's own icon, in the same purple, is untouched (hue %.0f)", h);
        check(near_hue(h, 271), msg);
        if (own) DestroyIcon(own);
        if (own_small) DestroyIcon(own_small);

        lstrcpyW(path, windows); lstrcatW(path, L"\\explorer.exe");
        check(ExtractIconExW(path, 0, &expl, &expl_small, 1) >= 1 && expl, "explorer.exe has its own icon (File Explorer's)");
        h = expl ? icon_hue(expl) : -1;
        sprintf(msg, "and it follows the accent (hue %.0f)", h);
        check(near_hue(h, want), msg);
        if (expl) DestroyIcon(expl);
        if (expl_small) DestroyIcon(expl_small);

        /* the shell's image list: the folder's entry, made again in place */
        if (!SHGetFileInfoW(dir, 0, &sfi, sizeof(sfi), SHGFI_SYSICONINDEX | SHGFI_LARGEICON)) sfi.iIcon = -1;
        if (sys_index < 0) sys_index = sfi.iIcon;
        else check(sfi.iIcon == sys_index, "the folder keeps its image list index");
        Shell_GetImageLists(&list, NULL);
        icon = list && sfi.iIcon >= 0 ? ImageList_GetIcon(list, sfi.iIcon, ILD_NORMAL) : NULL;
        h = icon ? icon_hue(icon) : -1;
        sprintf(msg, "the shell's image list follows it (hue %.0f)", h);
        check(near_hue(h, want), msg);
        if (icon) DestroyIcon(icon);
    }
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return 0;
}
