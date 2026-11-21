/* shlwstream-probe: shlwapi/shcore stream and registry helpers that were spec stubs (Stained Glass OS wine-sg
 * 8237; test/shlwstream-gate.sh): IStream_WriteStr/IStream_ReadStr (shcore and shlwapi 596/597),
 * IStream_WritePidl/IStream_ReadPidl (shlwapi 512/513), IStream_Copy (shlwapi 568),
 * SHRegGetValueFromHKCUHKLM and SHRegGetBoolValueFromHKCUHKLM (shlwapi 629/630).
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#define COBJMACROS
#include <windows.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <stdio.h>

static int failures;
static void check( int ok, const char *what, long got )
{
    printf( "%s  %s (%#lx)\n", ok ? "PASS" : "FAIL", what, got );
    if (!ok) failures++;
}

static void rewind_stream( IStream *s )
{
    LARGE_INTEGER zero = {{0}};
    IStream_Seek( s, zero, STREAM_SEEK_SET, NULL );
}

int main( void )
{
    HMODULE shlwapi = LoadLibraryA( "shlwapi.dll" ), shcore = LoadLibraryA( "shcore.dll" );
    HRESULT (WINAPI *pWriteStr)(IStream *, const WCHAR *) = (void *)GetProcAddress( shcore, "IStream_WriteStr" );
    HRESULT (WINAPI *pReadStr)(IStream *, WCHAR **) = (void *)GetProcAddress( shcore, "IStream_ReadStr" );
    HRESULT (WINAPI *pWriteStrL)(IStream *, const WCHAR *) = (void *)GetProcAddress( shlwapi, (const char *)597 );
    HRESULT (WINAPI *pReadStrL)(IStream *, WCHAR **) = (void *)GetProcAddress( shlwapi, (const char *)596 );
    HRESULT (WINAPI *pReadPidl)(IStream *, ITEMIDLIST **) = (void *)GetProcAddress( shlwapi, (const char *)512 );
    HRESULT (WINAPI *pWritePidl)(IStream *, const ITEMIDLIST *) = (void *)GetProcAddress( shlwapi, (const char *)513 );
    HRESULT (WINAPI *pCopy)(IStream *, IStream *, DWORD) = (void *)GetProcAddress( shlwapi, (const char *)568 );
    LSTATUS (WINAPI *pGetHKCUHKLM)(const WCHAR *, const WCHAR *, DWORD, DWORD *, void *, DWORD *) = (void *)GetProcAddress( shlwapi, (const char *)629 );
    BOOL (WINAPI *pGetBoolHKCUHKLM)(const WCHAR *, const WCHAR *, BOOL) = (void *)GetProcAddress( shlwapi, (const char *)630 );
    IStream *s, *d;
    WCHAR *a = NULL, *b = NULL;
    STATSTG st;
    ITEMIDLIST *pidl, *back = NULL;
    HKEY key;
    DWORD v = 0, size, type;
    char buf[16];
    HRESULT hr;

    CoInitialize( NULL );
    s = SHCreateMemStream( NULL, 0 );
    hr = pWriteStr( s, L"Hello, w\x00f6rld" );
    hr = hr == S_OK ? pWriteStrL( s, L"" ) : hr;
    hr = hr == S_OK ? pWriteStrL( s, L"second" ) : hr;
    check( hr == S_OK, "IStream_WriteStr (shcore, and shlwapi 597)", hr );
    IStream_Stat( s, &st, STATFLAG_NONAME );
    check( st.cbSize.QuadPart == 2 + 24 + 2 + 2 + 12, "the layout: a character count, then the characters", st.cbSize.LowPart );
    rewind_stream( s );
    hr = pReadStr( s, &a );
    check( hr == S_OK && !lstrcmpW( a, L"Hello, w\x00f6rld" ), "IStream_ReadStr gives the string back", hr );
    CoTaskMemFree( a ); a = NULL;
    hr = pReadStrL( s, &a );
    check( hr == S_OK && a && !a[0], "an empty string (shlwapi 596)", hr );
    hr = pReadStrL( s, &b );
    check( hr == S_OK && !lstrcmpW( b, L"second" ), "the next string", hr );
    CoTaskMemFree( a ); CoTaskMemFree( b ); b = NULL;
    hr = pReadStr( s, &b );
    check( FAILED(hr) && !b, "past the end: an error", hr );
    IStream_Release( s );

    /* IStream_Copy: five bytes */
    s = SHCreateMemStream( (const BYTE *)"0123456789", 10 );
    d = SHCreateMemStream( NULL, 0 );
    hr = pCopy( s, d, 5 );
    IStream_Stat( d, &st, STATFLAG_NONAME );
    rewind_stream( d );
    memset( buf, 0, sizeof(buf) );
    IStream_Read( d, buf, sizeof(buf), NULL );
    check( hr == S_OK && st.cbSize.QuadPart == 5 && !strcmp( buf, "01234" ), "IStream_Copy (shlwapi 568)", hr );
    IStream_Release( s ); IStream_Release( d );

    /* a PIDL to a stream and back */
    pidl = ILCreateFromPathW( L"C:\\windows" );
    s = SHCreateMemStream( NULL, 0 );
    hr = pWritePidl( s, pidl );
    check( hr == S_OK, "IStream_WritePidl (shlwapi 513)", hr );
    rewind_stream( s );
    hr = pReadPidl( s, &back );
    check( hr == S_OK && back && ILIsEqual( pidl, back ), "IStream_ReadPidl gives the same item list", hr );
    ILFree( back ); back = NULL;
    IStream_Stat( s, &st, STATFLAG_NONAME );
    {
        /* a truncated one is refused */
        ULARGE_INTEGER shorter;
        shorter.QuadPart = st.cbSize.QuadPart - 3;
        IStream_SetSize( s, shorter );
        rewind_stream( s );
        hr = pReadPidl( s, &back );
        check( FAILED(hr) && !back, "a truncated item list: an error", hr );
    }
    IStream_Release( s );
    ILFree( pidl );

    /* the user's value, else the machine's */
    RegCreateKeyExW( HKEY_LOCAL_MACHINE, L"Software\\SG Shlw Test", 0, NULL, 0, KEY_ALL_ACCESS, NULL, &key, NULL );
    v = 7; RegSetValueExW( key, L"Both", 0, REG_DWORD, (BYTE *)&v, sizeof(v) );
    v = 9; RegSetValueExW( key, L"MachineOnly", 0, REG_DWORD, (BYTE *)&v, sizeof(v) );
    RegSetValueExW( key, L"Flag", 0, REG_SZ, (BYTE *)L"yes", 8 );
    RegCloseKey( key );
    RegCreateKeyExW( HKEY_CURRENT_USER, L"Software\\SG Shlw Test", 0, NULL, 0, KEY_ALL_ACCESS, NULL, &key, NULL );
    v = 3; RegSetValueExW( key, L"Both", 0, REG_DWORD, (BYTE *)&v, sizeof(v) );
    RegCloseKey( key );
    size = sizeof(v); v = 0;
    check( !pGetHKCUHKLM( L"Software\\SG Shlw Test", L"Both", RRF_RT_REG_DWORD, &type, &v, &size ) && v == 3,
           "SHRegGetValueFromHKCUHKLM: the user's value first (shlwapi 629)", v );
    size = sizeof(v); v = 0;
    check( !pGetHKCUHKLM( L"Software\\SG Shlw Test", L"MachineOnly", RRF_RT_REG_DWORD, &type, &v, &size ) && v == 9,
           "else the machine's", v );
    check( pGetBoolHKCUHKLM( L"Software\\SG Shlw Test", L"Flag", FALSE ) &&
           !pGetBoolHKCUHKLM( L"Software\\SG Shlw Test", L"Missing", FALSE ), "SHRegGetBoolValueFromHKCUHKLM (shlwapi 630)", 0 );
    RegDeleteTreeW( HKEY_CURRENT_USER, L"Software\\SG Shlw Test" );
    RegDeleteTreeW( HKEY_LOCAL_MACHINE, L"Software\\SG Shlw Test" );

    printf( "RESULT: %s\n", failures ? "FAIL" : "PASS" );
    return failures != 0;
}
