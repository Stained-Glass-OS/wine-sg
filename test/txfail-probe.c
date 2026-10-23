/* txfail-probe: kernel32's *Transacted file calls (Stained Glass OS wine-sg 6061), for test/txfail-gate.sh.
 * Works in C:\txf. Prints KEY=VALUE lines: "ret error" pairs.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include <windows.h>
#include <stdio.h>

static HANDLE (WINAPI *pCreateTransaction)(SECURITY_ATTRIBUTES *, GUID *, DWORD, DWORD, DWORD, DWORD, WCHAR *);
static BOOL (WINAPI *pCommitTransaction)(HANDLE);
static BOOL (WINAPI *pRollbackTransaction)(HANDLE);
static HANDLE (WINAPI *pCreateFileTransactedW)(LPCWSTR, DWORD, DWORD, SECURITY_ATTRIBUTES *, DWORD, DWORD, HANDLE, HANDLE, USHORT *, void *);
static HANDLE (WINAPI *pCreateFileTransactedA)(LPCSTR, DWORD, DWORD, SECURITY_ATTRIBUTES *, DWORD, DWORD, HANDLE, HANDLE, USHORT *, void *);
static BOOL (WINAPI *pDeleteFileTransactedW)(LPCWSTR, HANDLE);
static BOOL (WINAPI *pMoveFileTransactedW)(LPCWSTR, LPCWSTR, LPPROGRESS_ROUTINE, void *, DWORD, HANDLE);
static BOOL (WINAPI *pCopyFileTransactedW)(LPCWSTR, LPCWSTR, LPPROGRESS_ROUTINE, void *, BOOL *, DWORD, HANDLE);
static BOOL (WINAPI *pCreateDirectoryTransactedW)(LPCWSTR, LPCWSTR, SECURITY_ATTRIBUTES *, HANDLE);
static BOOL (WINAPI *pRemoveDirectoryTransactedW)(LPCWSTR, HANDLE);
static BOOL (WINAPI *pSetFileAttributesTransactedW)(LPCWSTR, DWORD, HANDLE);
static BOOL (WINAPI *pGetFileAttributesTransactedW)(LPCWSTR, GET_FILEEX_INFO_LEVELS, void *, HANDLE);
static HANDLE (WINAPI *pFindFirstFileTransactedW)(LPCWSTR, FINDEX_INFO_LEVELS, void *, FINDEX_SEARCH_OPS, void *, DWORD, HANDLE);
static DWORD (WINAPI *pGetFullPathNameTransactedW)(LPCWSTR, DWORD, LPWSTR, LPWSTR *, HANDLE);
static DWORD (WINAPI *pGetLongPathNameTransactedW)(LPCWSTR, LPWSTR, DWORD, HANDLE);
static DWORD (WINAPI *pGetCompressedFileSizeTransactedW)(LPCWSTR, DWORD *, HANDLE);
static BOOL (WINAPI *pCreateHardLinkTransactedW)(LPCWSTR, LPCWSTR, SECURITY_ATTRIBUTES *, HANDLE);
static BOOLEAN (WINAPI *pCreateSymbolicLinkTransactedW)(LPCWSTR, LPCWSTR, DWORD, HANDLE);

static int exists( const WCHAR *path ) { return GetFileAttributesW( path ) != INVALID_FILE_ATTRIBUTES; }

int main( void )
{
    HMODULE k = GetModuleHandleA( "kernel32" ), ktm = LoadLibraryA( "ktmw32" );
    HANDLE tx, event, h;
    WIN32_FILE_ATTRIBUTE_DATA fad;
    WIN32_FIND_DATAW fd;
    WCHAR buf[MAX_PATH];
    DWORD ret, err;
    BOOL ok;

#define K(name) p##name = (void *)GetProcAddress( k, #name )
    K(CreateFileTransactedW); K(CreateFileTransactedA); K(DeleteFileTransactedW); K(MoveFileTransactedW);
    K(CopyFileTransactedW); K(CreateDirectoryTransactedW); K(RemoveDirectoryTransactedW);
    K(SetFileAttributesTransactedW); K(GetFileAttributesTransactedW); K(FindFirstFileTransactedW);
    K(GetFullPathNameTransactedW); K(GetLongPathNameTransactedW); K(GetCompressedFileSizeTransactedW);
    K(CreateHardLinkTransactedW); K(CreateSymbolicLinkTransactedW);
    pCreateTransaction = (void *)GetProcAddress( ktm, "CreateTransaction" );
    pCommitTransaction = (void *)GetProcAddress( ktm, "CommitTransaction" );
    pRollbackTransaction = (void *)GetProcAddress( ktm, "RollbackTransaction" );

    CreateDirectoryW( L"C:\\txf", NULL );
    CloseHandle( CreateFileW( L"C:\\txf\\old.txt", GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL ) );
    CreateDirectoryW( L"C:\\txf\\olddir", NULL );

    tx = pCreateTransaction( NULL, NULL, 0, 0, 0, 0, NULL );
    printf( "CreateTransaction=%d\n", tx != INVALID_HANDLE_VALUE );
    event = CreateEventW( NULL, FALSE, FALSE, NULL );

    /* the handle is checked first */
    SetLastError( 0xdeadbeef );
    h = pCreateFileTransactedW( L"C:\\txf\\new.txt", GENERIC_WRITE, 0, NULL, CREATE_NEW, 0, NULL, event, NULL, NULL );
    err = GetLastError();
    printf( "CreateFileEvent=%d %lu\n", h == INVALID_HANDLE_VALUE, err );
    SetLastError( 0xdeadbeef );
    h = pCreateFileTransactedW( L"C:\\txf\\new.txt", GENERIC_WRITE, 0, NULL, CREATE_NEW, 0, NULL, NULL, NULL, NULL );
    err = GetLastError();
    printf( "CreateFileNull=%d %lu\n", h == INVALID_HANDLE_VALUE, err );
    SetLastError( 0xdeadbeef );
    ok = pDeleteFileTransactedW( L"C:\\txf\\old.txt", (HANDLE)(ULONG_PTR)0x1234 );
    err = GetLastError();
    printf( "DeleteBogus=%d %lu %d\n", ok, err, exists( L"C:\\txf\\old.txt" ) );

    /* a transaction: every call fails as on a volume without transaction support, changing nothing */
    SetLastError( 0xdeadbeef );
    h = pCreateFileTransactedW( L"C:\\txf\\new.txt", GENERIC_WRITE, 0, NULL, CREATE_NEW, 0, NULL, tx, NULL, NULL );
    err = GetLastError();
    printf( "CreateFile=%d %lu %d\n", h == INVALID_HANDLE_VALUE, err, exists( L"C:\\txf\\new.txt" ) );
    SetLastError( 0xdeadbeef );
    h = pCreateFileTransactedA( "C:\\txf\\newa.txt", GENERIC_WRITE, 0, NULL, CREATE_NEW, 0, NULL, tx, NULL, NULL );
    err = GetLastError();
    printf( "CreateFileA=%d %lu %d\n", h == INVALID_HANDLE_VALUE, err, exists( L"C:\\txf\\newa.txt" ) );
    SetLastError( 0xdeadbeef );
    ok = pDeleteFileTransactedW( L"C:\\txf\\old.txt", tx );
    err = GetLastError();
    printf( "Delete=%d %lu %d\n", ok, err, exists( L"C:\\txf\\old.txt" ) );
    SetLastError( 0xdeadbeef );
    ok = pMoveFileTransactedW( L"C:\\txf\\old.txt", L"C:\\txf\\moved.txt", NULL, NULL, 0, tx );
    err = GetLastError();
    printf( "Move=%d %lu %d %d\n", ok, err, exists( L"C:\\txf\\old.txt" ), exists( L"C:\\txf\\moved.txt" ) );
    SetLastError( 0xdeadbeef );
    ok = pCopyFileTransactedW( L"C:\\txf\\old.txt", L"C:\\txf\\copy.txt", NULL, NULL, NULL, 0, tx );
    err = GetLastError();
    printf( "Copy=%d %lu %d\n", ok, err, exists( L"C:\\txf\\copy.txt" ) );
    SetLastError( 0xdeadbeef );
    ok = pCreateDirectoryTransactedW( NULL, L"C:\\txf\\newdir", NULL, tx );
    err = GetLastError();
    printf( "CreateDirectory=%d %lu %d\n", ok, err, exists( L"C:\\txf\\newdir" ) );
    SetLastError( 0xdeadbeef );
    ok = pRemoveDirectoryTransactedW( L"C:\\txf\\olddir", tx );
    err = GetLastError();
    printf( "RemoveDirectory=%d %lu %d\n", ok, err, exists( L"C:\\txf\\olddir" ) );
    SetLastError( 0xdeadbeef );
    ok = pSetFileAttributesTransactedW( L"C:\\txf\\old.txt", FILE_ATTRIBUTE_READONLY, tx );
    err = GetLastError();
    printf( "SetAttributes=%d %lu %d\n", ok, err, !(GetFileAttributesW( L"C:\\txf\\old.txt" ) & FILE_ATTRIBUTE_READONLY) );
    SetLastError( 0xdeadbeef );
    ok = pGetFileAttributesTransactedW( L"C:\\txf\\old.txt", GetFileExInfoStandard, &fad, tx );
    err = GetLastError();
    printf( "GetAttributes=%d %lu\n", ok, err );
    SetLastError( 0xdeadbeef );
    h = pFindFirstFileTransactedW( L"C:\\txf\\*", FindExInfoStandard, &fd, FindExSearchNameMatch, NULL, 0, tx );
    err = GetLastError();
    printf( "FindFirst=%d %lu\n", h == INVALID_HANDLE_VALUE, err );
    SetLastError( 0xdeadbeef );
    ret = pGetFullPathNameTransactedW( L"old.txt", MAX_PATH, buf, NULL, tx );
    err = GetLastError();
    printf( "FullPath=%lu %lu\n", ret, err );
    SetLastError( 0xdeadbeef );
    ret = pGetLongPathNameTransactedW( L"C:\\txf\\old.txt", buf, MAX_PATH, tx );
    err = GetLastError();
    printf( "LongPath=%lu %lu\n", ret, err );
    SetLastError( 0xdeadbeef );
    ret = pGetCompressedFileSizeTransactedW( L"C:\\txf\\old.txt", NULL, tx );
    err = GetLastError();
    printf( "CompressedSize=%d %lu\n", ret == INVALID_FILE_SIZE, err );
    SetLastError( 0xdeadbeef );
    ok = pCreateHardLinkTransactedW( L"C:\\txf\\hard.txt", L"C:\\txf\\old.txt", NULL, tx );
    err = GetLastError();
    printf( "HardLink=%d %lu %d\n", ok, err, exists( L"C:\\txf\\hard.txt" ) );
    SetLastError( 0xdeadbeef );
    ok = pCreateSymbolicLinkTransactedW( L"C:\\txf\\sym.txt", L"C:\\txf\\old.txt", 0, tx );
    err = GetLastError();
    printf( "SymLink=%d %lu %d\n", ok, err, exists( L"C:\\txf\\sym.txt" ) );

    /* the transaction itself, with no work in it, commits; another rolls back */
    printf( "Commit=%d\n", pCommitTransaction( tx ) );
    SetLastError( 0xdeadbeef );
    ok = pCommitTransaction( tx );
    err = GetLastError();
    printf( "CommitAgain=%d %lu\n", ok, err );
    CloseHandle( tx );
    tx = pCreateTransaction( NULL, NULL, 0, 0, 0, 0, NULL );
    printf( "Rollback=%d\n", pRollbackTransaction( tx ) );
    CloseHandle( tx );
    printf( "done=1\n" );
    return 0;
}
