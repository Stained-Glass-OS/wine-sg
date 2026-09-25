/* handoff-probe: the system32 names that hand off to App Paths
 * (patches/sg/0120, 0121, 0122), for test/handoff-gate.sh.
 *
 *   handoff-probe run CMDLINE     CreateProcess(NULL, CMDLINE) -- the search a
 *                                 program's "calc.exe" gets -- and wait for it
 *   handoff-probe find CLASS [TAG] is a visible top-level window of CLASS there
 *   handoff-probe count EXE       how many processes run EXE
 *   handoff-probe open FILE       ShellExecute(FILE) -- the Run box's and a
 *                                 shortcut's way (admintools-gate.sh)
 *   handoff-probe ARGS...         (anything else) the registered program: records
 *                                 its command line in C:\standin.log
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include <windows.h>
#include <tlhelp32.h>
#include <shellapi.h>
#include <stdio.h>

int wmain( int argc, WCHAR **argv )
{
    if (argc == 3 && !lstrcmpW( argv[1], L"run" ))
    {
        STARTUPINFOW si = { sizeof(si) };
        PROCESS_INFORMATION pi;
        DWORD code = 0;
        if (!CreateProcessW( NULL, argv[2], NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi ))
        {
            printf( "create failed %lu\n", GetLastError() );
            return 1;
        }
        if (WaitForSingleObject( pi.hProcess, 8000 )) printf( "still running\n" );
        else { GetExitCodeProcess( pi.hProcess, &code ); printf( "ran exit=%lu\n", code ); }
        return 0;
    }
    if (argc == 3 && !lstrcmpW( argv[1], L"open" ))
    {
        INT_PTR r = (INT_PTR)ShellExecuteW( NULL, NULL, argv[2], NULL, NULL, SW_SHOWNORMAL );
        printf( "open=%d\n", r > 32 ? 1 : (int)r );
        return 0;
    }
    if (argc >= 3 && !lstrcmpW( argv[1], L"find" ))
    {
        HWND h = FindWindowW( argv[2], NULL );
        printf( "found=%d\n", h && IsWindowVisible( h ) ? 1 : 0 );
        return 0;
    }
    if (argc == 3 && !lstrcmpW( argv[1], L"count" ))
    {
        PROCESSENTRY32W pe = { sizeof(pe) };
        HANDLE snap = CreateToolhelp32Snapshot( TH32CS_SNAPPROCESS, 0 );
        int n = 0;
        if (Process32FirstW( snap, &pe ))
            do if (!lstrcmpiW( pe.szExeFile, argv[2] )) n++; while (Process32NextW( snap, &pe ));
        printf( "count=%d\n", n );
        return 0;
    }
    {
        FILE *f = _wfopen( L"C:\\standin.log", L"a" );
        fwprintf( f, L"cmdline=%ls\n", GetCommandLineW() );
        fclose( f );
    }
    return 0;
}
