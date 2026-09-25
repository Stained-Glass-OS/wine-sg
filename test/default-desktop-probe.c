/* default-desktop-probe: WinSta0\Default is the user's desktop
 * (patches/sg/0080), for test/default-desktop-gate.sh.
 *
 *   default-desktop-probe run     start "child" with STARTUPINFO.lpDesktop
 *                                 WinSta0\Default, as Mozilla-derived
 *                                 launchers do, and report what it saw
 *   default-desktop-probe child   the desktop it is on, and whether it can
 *                                 create and show a window there
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include <windows.h>
#include <stdio.h>

int wmain( int argc, WCHAR **argv )
{
    if (argc == 2 && !lstrcmpW( argv[1], L"child" ))
    {
        WCHAR name[64] = L"";
        HWND hwnd;
        GetUserObjectInformationW( GetThreadDesktop( GetCurrentThreadId() ), UOI_NAME, name, sizeof(name), NULL );
        hwnd = CreateWindowW( L"STATIC", L"child", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 10, 10, 200, 100, 0, 0, 0, 0 );
        printf( "desktop=%ls window=%d\n", name, hwnd != NULL );
        return 0;
    }
    if (argc == 2 && !lstrcmpW( argv[1], L"run" ))
    {
        WCHAR self[MAX_PATH], cmd[MAX_PATH + 16], mine[64] = L"";
        STARTUPINFOW si = { sizeof(si) };
        PROCESS_INFORMATION pi;
        SECURITY_ATTRIBUTES sa = { sizeof(sa), NULL, TRUE };
        HANDLE rd, wr;
        char buf[256] = "";
        DWORD got = 0;

        GetUserObjectInformationW( GetThreadDesktop( GetCurrentThreadId() ), UOI_NAME, mine, sizeof(mine), NULL );
        CreatePipe( &rd, &wr, &sa, 0 );
        SetHandleInformation( rd, HANDLE_FLAG_INHERIT, 0 );
        si.dwFlags = STARTF_USESTDHANDLES;
        si.hStdOutput = si.hStdError = wr;
        si.lpDesktop = (WCHAR *)L"WinSta0\\Default";
        GetModuleFileNameW( NULL, self, MAX_PATH );
        swprintf( cmd, ARRAYSIZE(cmd), L"\"%ls\" child", self );
        if (!CreateProcessW( NULL, cmd, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi )) { printf( "create failed\n" ); return 1; }
        CloseHandle( wr );
        WaitForSingleObject( pi.hProcess, 60000 );
        ReadFile( rd, buf, sizeof(buf) - 1, &got, NULL );
        printf( "parent=%ls\n%s", mine, buf );
        return 0;
    }
    return 2;
}
