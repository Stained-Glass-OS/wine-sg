/* stgswmr-probe: structured storage (Stained Glass OS wine-sg 6006): direct
 * single-writer mode and IDirectWriterLock (seen from another process),
 * MoveElementTo, SetElementTimes, stream LockRegion, STGM_NOSCRATCH. */
#define COBJMACROS
#include <windows.h>
#include <ole2.h>
#include <stdio.h>
#include <string.h>

static int failures;

static void check( int ok, const char *what, long got )
{
    if (ok) printf( "ok    %s\n", what );
    else { printf( "FAIL  %s (got %#lx)\n", what, got ); failures++; }
}

/* child: can it lock the writer's byte of the file? */
static int child( const WCHAR *file )
{
    OVERLAPPED ov = {0};
    HANDLE h = CreateFileW( file, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL );
    BOOL locked;

    if (h == INVALID_HANDLE_VALUE) return 3;
    ov.Offset = 0x7fffffe3;
    locked = LockFileEx( h, LOCKFILE_EXCLUSIVE_LOCK | LOCKFILE_FAIL_IMMEDIATELY, 0, 1, 0, &ov );
    if (locked) UnlockFileEx( h, 0, 1, 0, &ov );
    CloseHandle( h );
    return locked ? 0 : 1;
}

static int run_child( const WCHAR *file )
{
    WCHAR cmd[MAX_PATH * 2], self[MAX_PATH];
    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    DWORD code = 99;

    GetModuleFileNameW( NULL, self, MAX_PATH );
    swprintf( cmd, ARRAYSIZE(cmd), L"\"%ls\" child \"%ls\"", self, file );
    if (!CreateProcessW( NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi )) return 98;
    WaitForSingleObject( pi.hProcess, 20000 );
    GetExitCodeProcess( pi.hProcess, &code );
    CloseHandle( pi.hProcess );
    CloseHandle( pi.hThread );
    return code;
}

static BOOL has_element( IStorage *stg, const WCHAR *name, DWORD type )
{
    IEnumSTATSTG *e;
    STATSTG st;
    BOOL found = FALSE;
    IStorage_EnumElements( stg, 0, NULL, 0, &e );
    while (IEnumSTATSTG_Next( e, 1, &st, NULL ) == S_OK)
    {
        if (!lstrcmpW( st.pwcsName, name ) && st.type == type) found = TRUE;
        CoTaskMemFree( st.pwcsName );
    }
    IEnumSTATSTG_Release( e );
    return found;
}

int wmain( int argc, WCHAR **argv )
{
    WCHAR file[MAX_PATH], file2[MAX_PATH];
    IDirectWriterLock *lock = NULL;
    IStorage *stg, *dest, *sub, *sub2;
    IStream *stm;
    STATSTG st;
    FILETIME ft;
    ULARGE_INTEGER off, len;
    char buf[16] = {0};
    ULONG got;
    HRESULT hr;

    if (argc > 2 && !lstrcmpW( argv[1], L"child" )) return child( argv[2] );

    CoInitialize( NULL );
    GetTempPathW( MAX_PATH, file );
    lstrcpyW( file2, file );
    lstrcatW( file, L"sgswmr.stg" );
    lstrcatW( file2, L"sgswmr2.stg" );
    DeleteFileW( file );
    DeleteFileW( file2 );

    /* the single writer of direct SWMR mode */
    hr = StgCreateDocfile( file, STGM_CREATE | STGM_READWRITE | STGM_SHARE_DENY_WRITE | STGM_DIRECT_SWMR, 0, &stg );
    check( hr == S_OK, "StgCreateDocfile in direct SWMR writer mode", hr );
    if (hr == S_OK)
    {
        hr = IStorage_QueryInterface( stg, &IID_IDirectWriterLock, (void **)&lock );
        check( hr == S_OK, "IDirectWriterLock", hr );
    }
    if (lock)
    {
        check( IDirectWriterLock_HaveWriteAccess( lock ) == S_FALSE, "no write access to begin with", 0 );
        check( IDirectWriterLock_ReleaseWriteAccess( lock ) == STG_E_ACCESSDENIED, "nothing to release", 0 );
        { int c = run_child( file ); check( c == 0, "another process can take the writer lock while it is free", c ); }
        hr = IDirectWriterLock_WaitForWriteAccess( lock, 1000 );
        check( hr == S_OK && IDirectWriterLock_HaveWriteAccess( lock ) == S_OK, "WaitForWriteAccess gives write access", hr );
        { int c = run_child( file ); check( c == 1, "another process cannot take it now", c ); }
        hr = IDirectWriterLock_ReleaseWriteAccess( lock );
        check( hr == S_OK && IDirectWriterLock_HaveWriteAccess( lock ) == S_FALSE, "ReleaseWriteAccess", hr );
        { int c = run_child( file ); check( c == 0, "and another process can again", c ); }
        IDirectWriterLock_Release( lock );
    }
    if (stg) IStorage_Release( stg );

    /* MoveElementTo, SetElementTimes */
    StgCreateDocfile( file2, STGM_CREATE | STGM_READWRITE | STGM_SHARE_EXCLUSIVE, 0, &stg );
    IStorage_CreateStream( stg, L"a", STGM_READWRITE | STGM_SHARE_EXCLUSIVE, 0, 0, &stm );
    IStream_Write( stm, "contents", 8, NULL );
    IStream_Release( stm );
    IStorage_CreateStorage( stg, L"s", STGM_READWRITE | STGM_SHARE_EXCLUSIVE, 0, 0, &sub );
    IStorage_CreateStream( sub, L"x", STGM_READWRITE | STGM_SHARE_EXCLUSIVE, 0, 0, &stm );
    IStream_Write( stm, "inner", 5, NULL );
    IStream_Release( stm );
    IStorage_Release( sub );
    IStorage_CreateStorage( stg, L"dest", STGM_READWRITE | STGM_SHARE_EXCLUSIVE, 0, 0, &dest );

    hr = IStorage_MoveElementTo( stg, L"a", dest, L"b", STGMOVE_COPY );
    check( hr == S_OK && has_element( stg, L"a", STGTY_STREAM ) && has_element( dest, L"b", STGTY_STREAM ),
           "MoveElementTo(STGMOVE_COPY) copies a stream", hr );
    IStorage_OpenStream( dest, L"b", NULL, STGM_READ | STGM_SHARE_EXCLUSIVE, 0, &stm );
    IStream_Read( stm, buf, sizeof(buf), &got );
    IStream_Release( stm );
    check( got == 8 && !memcmp( buf, "contents", 8 ), "with its data", got );
    hr = IStorage_MoveElementTo( stg, L"s", dest, L"t", STGMOVE_MOVE );
    check( hr == S_OK && !has_element( stg, L"s", STGTY_STORAGE ) && has_element( dest, L"t", STGTY_STORAGE ),
           "MoveElementTo(STGMOVE_MOVE) moves a storage", hr );
    IStorage_OpenStorage( dest, L"t", NULL, STGM_READ | STGM_SHARE_EXCLUSIVE, NULL, 0, &sub2 );
    check( sub2 && has_element( sub2, L"x", STGTY_STREAM ), "with what it holds", 0 );
    if (sub2) IStorage_Release( sub2 );
    check( IStorage_MoveElementTo( stg, L"nothere", dest, L"z", STGMOVE_COPY ) == STG_E_FILENOTFOUND,
           "MoveElementTo of a missing element: STG_E_FILENOTFOUND", 0 );
    check( IStorage_MoveElementTo( stg, L"a", dest, L"z", 7 ) == STG_E_INVALIDFLAG, "MoveElementTo bad flag: STG_E_INVALIDFLAG", 0 );

    ft.dwLowDateTime = 0x12345678;
    ft.dwHighDateTime = 0x01d00000;
    hr = IStorage_SetElementTimes( dest, L"t", NULL, NULL, &ft );
    IStorage_OpenStorage( dest, L"t", NULL, STGM_READ | STGM_SHARE_EXCLUSIVE, NULL, 0, &sub2 );
    memset( &st, 0, sizeof(st) );
    if (sub2) { IStorage_Stat( sub2, &st, STATFLAG_NONAME ); IStorage_Release( sub2 ); }
    check( hr == S_OK && st.mtime.dwLowDateTime == ft.dwLowDateTime && st.mtime.dwHighDateTime == ft.dwHighDateTime,
           "SetElementTimes sets the modification time", st.mtime.dwLowDateTime );
    check( IStorage_SetElementTimes( dest, L"nothere", NULL, NULL, &ft ) == STG_E_FILENOTFOUND, "SetElementTimes of a missing element", 0 );

    /* streams of a compound file lock no regions */
    IStorage_OpenStream( stg, L"a", NULL, STGM_READWRITE | STGM_SHARE_EXCLUSIVE, 0, &stm );
    off.QuadPart = 0;
    len.QuadPart = 4;
    check( IStream_LockRegion( stm, off, len, LOCK_EXCLUSIVE ) == STG_E_INVALIDFUNCTION, "IStream::LockRegion: STG_E_INVALIDFUNCTION", 0 );
    check( IStream_UnlockRegion( stm, off, len, LOCK_EXCLUSIVE ) == STG_E_INVALIDFUNCTION, "IStream::UnlockRegion: STG_E_INVALIDFUNCTION", 0 );
    IStream_Release( stm );
    IStorage_Release( dest );
    IStorage_Release( stg );

    /* transacted without a scratch file */
    hr = StgCreateDocfile( file2, STGM_CREATE | STGM_READWRITE | STGM_SHARE_EXCLUSIVE | STGM_TRANSACTED | STGM_NOSCRATCH, 0, &stg );
    check( hr == S_OK, "STGM_TRANSACTED | STGM_NOSCRATCH", hr );
    if (hr == S_OK)
    {
        IStorage_CreateStream( stg, L"n", STGM_READWRITE | STGM_SHARE_EXCLUSIVE, 0, 0, &stm );
        IStream_Release( stm );
        check( IStorage_Commit( stg, STGC_DEFAULT ) == S_OK, "and Commit", 0 );
        IStorage_Release( stg );
    }

    DeleteFileW( file );
    DeleteFileW( file2 );
    printf( "RESULT: %s\n", failures ? "FAIL" : "PASS" );
    return failures != 0;
}
