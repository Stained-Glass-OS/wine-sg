/* symgetfile-probe: dbghelp SymGetSymbolFile(W), which were spec stubs (Stained Glass OS wine-sg 8278;
 * test/symgetfile-gate.sh): the image's own file is found in a symbol store (srv*) and in a plain folder,
 * only when its index matches; a .pdb the image does not name is not found.
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#include <windows.h>
#include <stdio.h>

static int failures;
static void check( int ok, const char *what ) { printf( "%s  %s\n", ok ? "PASS" : "FAIL", what ); if (!ok) failures++; }

int main( void )
{
    HMODULE dbghelp = LoadLibraryA( "dbghelp.dll" );
    const WCHAR *(WINAPI *pStore)(HANDLE, const WCHAR *, const WCHAR *, DWORD) = (void *)GetProcAddress( dbghelp, "SymSrvStoreFileW" );
    BOOL (WINAPI *pGet)(HANDLE, const WCHAR *, const WCHAR *, DWORD, WCHAR *, size_t, WCHAR *, size_t) = (void *)GetProcAddress( dbghelp, "SymGetSymbolFileW" );
    WCHAR self[MAX_PATH], tmp[MAX_PATH], store[MAX_PATH], srv[MAX_PATH + 8], plain[MAX_PATH], other[MAX_PATH], copy[MAX_PATH];
    WCHAR sym[MAX_PATH], dbg[MAX_PATH];
    const WCHAR *stored;
    HANDLE f;
    BOOL ret;

    if (!pGet) { printf( "FAIL  export\nRESULT: FAIL\n" ); return 1; }
    GetModuleFileNameW( NULL, self, MAX_PATH );
    GetTempPathW( MAX_PATH, tmp );
    swprintf( store, MAX_PATH, L"%lssg-symget-store", tmp );
    swprintf( srv, ARRAYSIZE(srv), L"srv*%ls", store );
    stored = pStore( GetCurrentProcess(), srv, self, 0 );

    ret = pGet( NULL, srv, self, 0 /* sfImage */, sym, MAX_PATH, dbg, MAX_PATH );
    check( ret && !lstrcmpiW( sym, L"probe.exe" ) && stored && !lstrcmpiW( dbg, stored ), "sfImage in a srv* store: found, with its path" );

    swprintf( plain, MAX_PATH, L"%lssg-symget-plain", tmp );
    CreateDirectoryW( plain, NULL );
    swprintf( copy, MAX_PATH, L"%ls\\probe.exe", plain );
    CopyFileW( self, copy, FALSE );
    swprintf( other, MAX_PATH, L"%lssg-symget-none;%ls", tmp, plain );
    ret = pGet( NULL, other, self, 0, sym, MAX_PATH, dbg, MAX_PATH );
    check( ret && !lstrcmpiW( dbg, copy ), "sfImage along a path of plain folders: the folder that has it" );

    f = CreateFileW( copy, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL );
    WriteFile( f, "MZ not this one", 15, (DWORD *)&ret, NULL );
    CloseHandle( f );
    ret = pGet( NULL, plain, self, 0, sym, MAX_PATH, dbg, MAX_PATH );
    check( !ret, "a different file of that name does not count" );
    ret = pGet( NULL, srv, self, 2 /* sfPdb */, sym, MAX_PATH, dbg, MAX_PATH );
    check( !ret && GetLastError() == ERROR_FILE_NOT_FOUND, "sfPdb for an image that names no .pdb: ERROR_FILE_NOT_FOUND" );
    check( !pGet( NULL, NULL, self, 0, sym, MAX_PATH, dbg, MAX_PATH ) && GetLastError() == ERROR_INVALID_PARAMETER, "no process and no path: ERROR_INVALID_PARAMETER" );
    printf( "RESULT: %s\n", failures ? "FAIL" : "PASS" );
    return failures != 0;
}
