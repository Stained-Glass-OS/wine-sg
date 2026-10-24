/* schemedesktop-probe.c (wine-sg 6090): switching the colour scheme with the app mode (light <-> dark) keeps the
 * person's desktop colour. KEY=VALUE lines. */
#include <windows.h>
#include <stdio.h>

static void rgb(const char *key, COLORREF c)
{
    printf("%s=%u,%u,%u\n", key, GetRValue(c), GetGValue(c), GetBValue(c));
}

static COLORREF reg_background(void)
{
    char buf[32] = "";
    DWORD size = sizeof(buf);
    int r = 0, g = 0, b = 0;
    RegGetValueA(HKEY_CURRENT_USER, "Control Panel\\Colors", "Background", RRF_RT_REG_SZ, NULL, buf, &size);
    sscanf(buf, "%d %d %d", &r, &g, &b);
    return RGB(r, g, b);
}

static void set_mode(DWORD light)
{
    void (WINAPI *refresh)(void);
    HMODULE ux = LoadLibraryA("uxtheme.dll");
    RegSetKeyValueA(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                    "AppsUseLightTheme", REG_DWORD, &light, sizeof(light));
    refresh = (void *)GetProcAddress(ux, MAKEINTRESOURCEA(104));  /* RefreshImmersiveColorPolicyState */
    if (refresh) refresh();
    Sleep(500);
}

int main(void)
{
    INT index = COLOR_BACKGROUND;
    COLORREF magenta = RGB(255, 0, 255), window_before;
    const char *value = "255 0 255";

    /* as Settings > Personalization > Background > Solid colour does */
    RegSetKeyValueA(HKEY_CURRENT_USER, "Control Panel\\Colors", "Background", REG_SZ, value, strlen(value) + 1);
    SetSysColors(1, &index, &magenta);
    rgb("before", GetSysColor(COLOR_BACKGROUND));
    window_before = GetSysColor(COLOR_WINDOW);
    set_mode(0);
    rgb("dark", GetSysColor(COLOR_BACKGROUND));
    rgb("dark_reg", reg_background());
    printf("dark_window_changed=%d\n", GetSysColor(COLOR_WINDOW) != window_before);
    set_mode(1);
    rgb("light", GetSysColor(COLOR_BACKGROUND));
    rgb("light_reg", reg_background());
    printf("light_window_back=%d\n", GetSysColor(COLOR_WINDOW) == window_before);
    printf("done=1\n");
    return 0;
}
