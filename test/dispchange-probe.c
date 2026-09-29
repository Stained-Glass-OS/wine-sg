/* dispchange-probe icon|change W H|tray: a notification icon (kept until
 * killed); the desktop's size changed as Display settings changes it
 * (ChangeDisplaySettingsEx); the taskbar's rectangle and visibility and the
 * first notification icon's, in screen coordinates. (patches/sg/0481) */
#include <windows.h>
#include <shellapi.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
    if (argc > 1 && !strcmp(argv[1], "icon"))
    {
        NOTIFYICONDATAW nid = { sizeof(nid) };
        HWND hwnd = CreateWindowW(L"STATIC", L"probe", 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL);
        MSG msg;
        nid.hWnd = hwnd; nid.uID = 1; nid.uFlags = NIF_ICON | NIF_TIP;
        nid.hIcon = LoadIconW(NULL, (const WCHAR *)IDI_APPLICATION);
        lstrcpyW(nid.szTip, L"probe");
        Shell_NotifyIconW(NIM_ADD, &nid);
        while (GetMessageW(&msg, NULL, 0, 0)) DispatchMessageW(&msg);
        return 0;
    }
    if (argc > 3 && !strcmp(argv[1], "change"))
    {
        DEVMODEW dm = { .dmSize = sizeof(dm) };
        dm.dmPelsWidth = atoi(argv[2]); dm.dmPelsHeight = atoi(argv[3]);
        dm.dmFields = DM_PELSWIDTH | DM_PELSHEIGHT;
        printf("change %ld\n", ChangeDisplaySettingsExW(NULL, &dm, NULL, CDS_UPDATEREGISTRY, NULL));
        return 0;
    }
    {
        HWND tray = FindWindowW(L"Shell_TrayWnd", NULL), icon = tray ? FindWindowExW(tray, NULL, L"__wine_tray_icon", NULL) : NULL;
        RECT t = { 0 }, i = { 0 };
        if (tray) GetWindowRect(tray, &t);
        if (icon) GetWindowRect(icon, &i);
        printf("tray %d %ld %ld %ld %ld icon %ld %ld screen %d %d\n", tray ? IsWindowVisible(tray) : -1, t.left, t.top, t.right, t.bottom,
               i.left, i.top, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN));
    }
    return 0;
}
