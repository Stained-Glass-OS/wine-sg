/* control-probe: control.exe hands off to the Control Panel App Paths names
 * (patches/sg/0072), for test/control-gate.sh.
 *
 *   control-probe ARGS...           (anything but "run") the registered
 *                                   Control Panel: records its command line
 *                                   and SG_CONTROL_HANDOFF
 *   control-probe run ARGS          CreateProcess system32\control.exe ARGS,
 *                                   as a program would, and wait for it
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include <windows.h>
#include <stdio.h>

int wmain( int argc, WCHAR **argv )
{
    if (argc == 3 && !lstrcmpW( argv[1], L"run" ))
    {
        WCHAR cmd[512], sys[MAX_PATH];
        STARTUPINFOW si = { sizeof(si) };
        PROCESS_INFORMATION pi;
        GetSystemDirectoryW( sys, MAX_PATH );
        swprintf( cmd, 512, L"\"%ls\\control.exe\" %ls", sys, argv[2] );
        if (!CreateProcessW( NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi )) { printf( "create failed\n" ); return 1; }
        WaitForSingleObject( pi.hProcess, 30000 );
        printf( "ran\n" );
        return 0;
    }
    if (argc < 2 || lstrcmpW( argv[1], L"run" ))
    {
        WCHAR env[8] = L"";
        FILE *f = _wfopen( L"C:\\standin.log", L"a" );
        GetEnvironmentVariableW( L"SG_CONTROL_HANDOFF", env, 8 );
        fwprintf( f, L"cmdline=%ls\nhandoff=%ls\n", GetCommandLineW(), env );
        fclose( f );
        return 0;
    }
    return 2;
}
