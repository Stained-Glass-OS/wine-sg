/* wallpaper-probe: set the desktop wallpaper as the Control Panel does, for
 * test/wallpaper-gate.sh (patches/sg/0069).
 *
 *   wallpaper-probe PATH STYLE TILE   WallpaperStyle/TileWallpaper, then
 *                                     SPI_SETDESKWALLPAPER with SPIF_SENDCHANGE
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include <windows.h>
#include <stdio.h>

int main( int argc, char **argv )
{
    WCHAR path[MAX_PATH], style[8], tile[8];
    HKEY key;

    if (argc != 4) return 2;
    MultiByteToWideChar( CP_ACP, 0, argv[1], -1, path, MAX_PATH );
    MultiByteToWideChar( CP_ACP, 0, argv[2], -1, style, 8 );
    MultiByteToWideChar( CP_ACP, 0, argv[3], -1, tile, 8 );
    RegCreateKeyExW( HKEY_CURRENT_USER, L"Control Panel\\Desktop", 0, NULL, 0, KEY_SET_VALUE, NULL, &key, NULL );
    RegSetValueExW( key, L"WallpaperStyle", 0, REG_SZ, (BYTE *)style, (lstrlenW( style ) + 1) * sizeof(WCHAR) );
    RegSetValueExW( key, L"TileWallpaper", 0, REG_SZ, (BYTE *)tile, (lstrlenW( tile ) + 1) * sizeof(WCHAR) );
    RegCloseKey( key );
    printf( "set=%d\n", SystemParametersInfoW( SPI_SETDESKWALLPAPER, 0, path, SPIF_UPDATEINIFILE | SPIF_SENDCHANGE ) );
    return 0;
}
