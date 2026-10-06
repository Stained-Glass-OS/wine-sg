/* barhidpi-gate.sh's hands (0880): opens Task View as the taskbar's Task
 * View button does (its click, WM_COMMAND to the bar).
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#include <windows.h>

int main( void )
{
    HWND bar = FindWindowW( L"Shell_TrayWnd", NULL ), button;
    if (!bar || !(button = GetDlgItem( bar, 0x5654 ))) return 1;
    PostMessageW( bar, WM_COMMAND, MAKEWPARAM( 0x5654, BN_CLICKED ), (LPARAM)button );
    return 0;
}
