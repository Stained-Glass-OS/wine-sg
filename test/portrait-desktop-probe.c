/* portrait-desktop-probe: asks the desktop for a size, says what it got.
 *   portrait-desktop-probe W H   ->  "change=N now=WxH"
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#include <windows.h>
#include <stdio.h>

int wmain(int argc, WCHAR **argv)
{
    DEVMODEW dm = { .dmSize = sizeof(dm) }, now = { .dmSize = sizeof(now) };
    LONG r = -99;
    int tries;
    if (argc < 3) return 2;
    dm.dmPelsWidth = _wtoi(argv[1]);
    dm.dmPelsHeight = _wtoi(argv[2]);
    dm.dmFields = DM_PELSWIDTH | DM_PELSHEIGHT;
    for (tries = 0; tries < 10; tries++)   /* as Settings asks: the new size may come a moment late */
    {
        if ((r = ChangeDisplaySettingsExW(NULL, &dm, NULL, 0, NULL)) == DISP_CHANGE_SUCCESSFUL) break;
        Sleep(200);
    }
    EnumDisplaySettingsW(NULL, ENUM_CURRENT_SETTINGS, &now);
    printf("change=%ld now=%lux%lu\n", r, now.dmPelsWidth, now.dmPelsHeight);
    return 0;
}
