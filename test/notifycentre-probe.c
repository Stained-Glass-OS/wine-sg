/* notifycentre-gate.sh's probe (0815):
 *   notifycentre-probe balloon   a notification-area icon ("Balloon Probe")
 *                                shows a balloon, then goes
 *   sg-notify.exe ARGS...        (this probe copied so) stands in for
 *                                sg-notify: writes ARGS to C:\notify-args.txt
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#include <windows.h>
#include <shellapi.h>
#include <stdio.h>
#include <string.h>

int main( int argc, char **argv )
{
    if (strstr( argv[0], "sg-notify" ))
    {
        FILE *f = fopen( "C:\\notify-args.txt", "w" );
        int i;
        for (i = 1; f && i < argc; i++) fprintf( f, "%s ", argv[i] );
        if (f) fclose( f );
        return 0;
    }
    if (argc > 1 && !strcmp( argv[1], "balloon" ))
    {
        NOTIFYICONDATAW nid = { sizeof(nid) };
        HWND hwnd = CreateWindowW( L"STATIC", L"probe", 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL );
        nid.hWnd = hwnd;
        nid.uID = 1;
        nid.uFlags = NIF_ICON | NIF_TIP;
        nid.hIcon = LoadIconW( NULL, (const WCHAR *)IDI_INFORMATION );
        lstrcpyW( nid.szTip, L"Balloon Probe" );
        printf( "add=%d\n", Shell_NotifyIconW( NIM_ADD, &nid ) );
        nid.uFlags = NIF_INFO;
        lstrcpyW( nid.szInfoTitle, L"Backup finished" );
        lstrcpyW( nid.szInfo, L"Your files were copied." );
        nid.dwInfoFlags = NIIF_INFO;
        printf( "balloon=%d\n", Shell_NotifyIconW( NIM_MODIFY, &nid ) );
        Sleep( 1500 );
        Shell_NotifyIconW( NIM_DELETE, &nid );
        return 0;
    }
    return 2;
}
