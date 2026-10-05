/* barscale-gate.sh's probe (0814):
 *   barscale-probe bar      the taskbar's height and its first button's width
 *   barscale-probe set N    Style\Scale8 = N, then SPI_SETNONCLIENTMETRICS
 *                           broadcast, as sg-shell's looks set the sizes
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int main( int argc, char **argv )
{
    HWND bar = FindWindowW( L"Shell_TrayWnd", NULL ), child;
    RECT r, c;
    DWORD_PTR res;

    /* the bar's real size, not one scaled for a program unaware of the DPI */
    SetProcessDPIAware();
    if (argc > 2 && !strcmp( argv[1], "set" ))
    {
        DWORD v = atoi( argv[2] );
        HKEY key;
        RegCreateKeyExW( HKEY_CURRENT_USER, L"Software\\Stained Glass\\Style", 0, NULL, 0, KEY_SET_VALUE, NULL, &key, NULL );
        RegSetValueExW( key, L"Scale8", 0, REG_DWORD, (BYTE *)&v, sizeof(v) );
        RegCloseKey( key );
        SendMessageTimeoutW( HWND_BROADCAST, WM_SETTINGCHANGE, SPI_SETNONCLIENTMETRICS, (LPARAM)L"WindowMetrics",
                             SMTO_ABORTIFHUNG, 3000, &res );
        return 0;
    }
    if (!bar || !GetWindowRect( bar, &r )) { printf( "nobar\n" ); return 1; }
    child = GetWindow( bar, GW_CHILD );
    c.left = c.right = 0;
    /* the widest button: Start's or a window's */
    {
        int widest = 0;
        for (; child; child = GetWindow( child, GW_HWNDNEXT ))
            if (IsWindowVisible( child ) && GetWindowRect( child, &c ) && c.right - c.left > widest &&
                c.bottom - c.top == r.bottom - r.top) widest = c.right - c.left;
        printf( "height=%ld widest=%d\n", r.bottom - r.top, widest );
    }
    return 0;
}
