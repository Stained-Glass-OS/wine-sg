/* fonts-probe: fontview.exe, the Fonts folder and per-user fonts
 * (patches/sg/0183), for test/fonts-gate.sh.
 *
 *   fonts-probe run CMDLINE        CreateProcess(NULL, CMDLINE) and wait
 *   fonts-probe open FILE [VERB]   ShellExecute(VERB, FILE) -- the Run box's way
 *   fonts-probe has FAMILY         does GDI enumerate FAMILY in this new process
 *   fonts-probe title TEXT         is there a visible top-level window titled TEXT
 *   fonts-probe count EXE          how many processes run EXE
 *   fonts-probe ARGS...            (anything else) the registered program: records
 *                                  its command line in C:\standin.log
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include <windows.h>
#include <tlhelp32.h>
#include <shellapi.h>
#include <stdio.h>

static int CALLBACK fam( const LOGFONTW *lf, const TEXTMETRICW *tm, DWORD type, LPARAM lp )
{
    (void)tm; (void)type;
    if (!lstrcmpiW( lf->lfFaceName, (const WCHAR *)lp )) return 0;
    return 1;
}

int wmain( int argc, WCHAR **argv )
{
    if (argc == 3 && !lstrcmpW( argv[1], L"run" ))
    {
        STARTUPINFOW si = { sizeof(si) };
        PROCESS_INFORMATION pi;
        if (!CreateProcessW( NULL, argv[2], NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi ))
        {
            printf( "create failed %lu\n", GetLastError() );
            return 1;
        }
        WaitForSingleObject( pi.hProcess, 8000 );
        printf( "ran\n" );
        return 0;
    }
    if ((argc == 3 || argc == 4) && !lstrcmpW( argv[1], L"open" ))
    {
        /* no error box: a failure must not wait for a click */
        SHELLEXECUTEINFOW sei = { sizeof(sei) };
        sei.fMask = SEE_MASK_FLAG_NO_UI;
        sei.lpVerb = argc == 4 ? argv[3] : NULL;
        sei.lpFile = argv[2];
        sei.nShow = SW_SHOWNORMAL;
        printf( "open=%d\n", ShellExecuteExW( &sei ) ? 1 : -(int)GetLastError() );
        return 0;
    }
    if (argc == 3 && !lstrcmpW( argv[1], L"has" ))
    {
        LOGFONTW lf = { 0 };
        HDC dc = GetDC( NULL );
        lf.lfCharSet = DEFAULT_CHARSET;
        lstrcpynW( lf.lfFaceName, argv[2], LF_FACESIZE );
        printf( "has=%d\n", EnumFontFamiliesExW( dc, &lf, fam, (LPARAM)argv[2], 0 ) == 0 ? 1 : 0 );
        ReleaseDC( NULL, dc );
        return 0;
    }
    if (argc == 3 && !lstrcmpW( argv[1], L"title" ))
    {
        HWND h = FindWindowW( NULL, argv[2] );
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
