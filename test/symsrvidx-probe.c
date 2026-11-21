/* symsrvidx-probe: dbghelp SymSrvGetFileIndexString(W) and SymSrvIsStore(W), which were spec stubs (Stained Glass
 * OS wine-sg 8243; test/symsrvidx-gate.sh): an image's index string is its time stamp and image size in hex, as
 * symbol stores file it; a directory with pingme.txt or 000Admin is a store.
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#include <windows.h>
#include <stdio.h>

static int failures;
static void check( int ok, const char *what, long got )
{
    printf( "%s  %s (%#lx)\n", ok ? "PASS" : "FAIL", what, got );
    if (!ok) failures++;
}

int main( int argc, char **argv )
{
    HMODULE dbghelp = LoadLibraryA( "dbghelp.dll" );
    BOOL (WINAPI *pIndexW)(HANDLE, const WCHAR *, const WCHAR *, WCHAR *, size_t, DWORD) = (void *)GetProcAddress( dbghelp, "SymSrvGetFileIndexStringW" );
    BOOL (WINAPI *pIndexA)(HANDLE, const char *, const char *, char *, size_t, DWORD) = (void *)GetProcAddress( dbghelp, "SymSrvGetFileIndexString" );
    BOOL (WINAPI *pIsStoreA)(HANDLE, const char *) = (void *)GetProcAddress( dbghelp, "SymSrvIsStore" );
    BOOL (WINAPI *pIsStoreW)(HANDLE, const WCHAR *) = (void *)GetProcAddress( dbghelp, "SymSrvIsStoreW" );
    IMAGE_NT_HEADERS *nt = (IMAGE_NT_HEADERS *)((BYTE *)GetModuleHandleA( NULL ) + ((IMAGE_DOS_HEADER *)GetModuleHandleA( NULL ))->e_lfanew);
    WCHAR self[MAX_PATH], index[64];
    char selfA[MAX_PATH], indexA[64], want[64], dir[MAX_PATH], sub[MAX_PATH];
    HANDLE f;
    BOOL ret;

    GetModuleFileNameW( NULL, self, MAX_PATH );
    GetModuleFileNameA( NULL, selfA, MAX_PATH );
    sprintf( want, "%08lX%lx", nt->FileHeader.TimeDateStamp, nt->OptionalHeader.SizeOfImage );
    ret = pIndexW( GetCurrentProcess(), NULL, self, index, ARRAYSIZE(index), 0 );
    printf( "      index %ls, expected %s\n", index, want );
    {
        char got[64];
        WideCharToMultiByte( CP_ACP, 0, index, -1, got, sizeof(got), NULL, NULL );
        check( ret && !strcmp( got, want ), "SymSrvGetFileIndexStringW of an image: time stamp + image size", ret );
    }
    ret = pIndexA( GetCurrentProcess(), NULL, selfA, indexA, sizeof(indexA), 0 );
    check( ret && !strcmp( indexA, want ), "SymSrvGetFileIndexString (ANSI)", ret );
    SetLastError( 0 );
    ret = pIndexW( GetCurrentProcess(), NULL, self, index, 4, 0 );
    check( !ret && GetLastError() == ERROR_INSUFFICIENT_BUFFER, "too small a buffer", GetLastError() );
    ret = pIndexW( GetCurrentProcess(), NULL, L"C:\\no\\such\\file.exe", index, ARRAYSIZE(index), 0 );
    check( !ret, "a missing file fails", GetLastError() );

    GetTempPathA( MAX_PATH, dir );
    strcat( dir, "sg-symstore" );
    CreateDirectoryA( dir, NULL );
    check( !pIsStoreA( GetCurrentProcess(), dir ), "SymSrvIsStore: an empty directory is no store", 0 );
    sprintf( sub, "%s\\pingme.txt", dir );
    f = CreateFileA( sub, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL );
    CloseHandle( f );
    check( pIsStoreA( GetCurrentProcess(), dir ), "one with pingme.txt is", 0 );
    DeleteFileA( sub );
    sprintf( sub, "%s\\000Admin", dir );
    CreateDirectoryA( sub, NULL );
    check( pIsStoreW( GetCurrentProcess(), L"C:\\windows" ) == FALSE, "C:\\windows is not", 0 );
    {
        WCHAR dirW[MAX_PATH];
        MultiByteToWideChar( CP_ACP, 0, dir, -1, dirW, MAX_PATH );
        check( pIsStoreW( GetCurrentProcess(), dirW ), "one with 000Admin is (W)", 0 );
    }
    RemoveDirectoryA( sub );
    RemoveDirectoryA( dir );
    printf( "RESULT: %s\n", failures ? "FAIL" : "PASS" );
    return failures != 0;
}
