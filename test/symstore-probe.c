/* symstore-probe: dbghelp SymSrvStoreFile(W), which were spec stubs (Stained Glass OS wine-sg 8277;
 * test/symstore-gate.sh): a file goes into a symbol store at <store>\<name>\<index>\<name>, the store is
 * marked (SymSrvIsStore), an existing copy is refused unless overwritten or passed, a pointer is a file.ptr.
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#include <windows.h>
#include <stdio.h>

static int failures;
static void check( int ok, const char *what ) { printf( "%s  %s\n", ok ? "PASS" : "FAIL", what ); if (!ok) failures++; }

int main( int argc, char **argv )
{
    HMODULE dbghelp = LoadLibraryA( "dbghelp.dll" );
    const WCHAR *(WINAPI *pStore)(HANDLE, const WCHAR *, const WCHAR *, DWORD) = (void *)GetProcAddress( dbghelp, "SymSrvStoreFileW" );
    BOOL (WINAPI *pIndex)(HANDLE, const WCHAR *, const WCHAR *, WCHAR *, size_t, DWORD) = (void *)GetProcAddress( dbghelp, "SymSrvGetFileIndexStringW" );
    BOOL (WINAPI *pIsStore)(HANDLE, const WCHAR *) = (void *)GetProcAddress( dbghelp, "SymSrvIsStoreW" );
    BOOL (WINAPI *pInit)(HANDLE, const char *, BOOL) = (void *)GetProcAddress( dbghelp, "SymInitialize" );
    WCHAR self[MAX_PATH], store[MAX_PATH], srv[MAX_PATH + 8], index[64], want[MAX_PATH * 2];
    const WCHAR *got;
    char text[MAX_PATH * 2] = { 0 };
    DWORD len = 0;
    HANDLE f;

    if (!pStore) { printf( "FAIL  export\nRESULT: FAIL\n" ); return 1; }
    pInit( GetCurrentProcess(), NULL, FALSE );
    GetModuleFileNameW( NULL, self, MAX_PATH );
    GetTempPathW( MAX_PATH, store );
    wcscat( store, L"sg-symstore" );
    swprintf( srv, ARRAYSIZE(srv), L"srv*%ls", store );
    pIndex( GetCurrentProcess(), NULL, self, index, ARRAYSIZE(index), 0 );
    swprintf( want, ARRAYSIZE(want), L"%ls\\probe.exe\\%ls\\probe.exe", store, index );

    got = pStore( GetCurrentProcess(), srv, self, 0 );
    check( got && !lstrcmpiW( got, want ), "stored at <store>\\probe.exe\\<index>\\probe.exe" );
    check( GetFileAttributesW( want ) != INVALID_FILE_ATTRIBUTES, "  the copy is there" );
    check( pIsStore( GetCurrentProcess(), store ), "  and the folder is now a symbol store" );
    check( !pStore( GetCurrentProcess(), srv, self, 0 ) && GetLastError() == ERROR_ALREADY_EXISTS, "again: ERROR_ALREADY_EXISTS" );
    got = pStore( GetCurrentProcess(), srv, self, 0x40 /* PASS_IF_EXISTS */ );
    check( got && !lstrcmpiW( got, want ), "with SYMSTOREOPT_PASS_IF_EXISTS: its path" );
    got = pStore( GetCurrentProcess(), store, self, 0x02 /* OVERWRITE */ );
    check( got != NULL, "a plain folder path with SYMSTOREOPT_OVERWRITE" );
    got = pStore( GetCurrentProcess(), srv, self, 0x04 /* RETURNINDEX */ );
    check( got && !lstrcmpW( got, index ), "SYMSTOREOPT_RETURNINDEX: the index" );
    got = pStore( GetCurrentProcess(), srv, self, 0x08 /* POINTER */ );
    if (got)
    {
        f = CreateFileW( got, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL );
        ReadFile( f, text, sizeof(text) - 1, &len, NULL );
        CloseHandle( f );
    }
    check( got && !strncmp( text, "PATH:", 5 ), "SYMSTOREOPT_POINTER: a file.ptr naming the file" );
    printf( "RESULT: %s\n", failures ? "FAIL" : "PASS" );
    return failures != 0;
}
