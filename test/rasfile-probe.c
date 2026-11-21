/* rasfile-probe: shcore CreateRandomAccessStreamOnFile, which was a spec stub (Stained Glass OS wine-sg 8252;
 * test/rasfile-gate.sh): ReadWrite makes/opens the file, what is written through the stream lands in it; Read
 * opens an existing file only.
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#define COBJMACROS
#include <windows.h>
#include <objidl.h>
#include <stdio.h>
#include <string.h>

static int fails;
static void check( const char *name, int ok, long v ) { printf( "%s  %s (%#lx)\n", ok ? "PASS" : "FAIL", name, v ); if (!ok) fails++; }
static const IID IID_IRandomAccessStream_ = { 0x905a0fe1, 0xbc53, 0x11df, { 0x8c, 0x49, 0x00, 0x1e, 0x4f, 0xc6, 0x86, 0xda } };

int main( void )
{
    HMODULE shcore = LoadLibraryA( "shcore.dll" );
    HRESULT (WINAPI *on_file)(const WCHAR *, DWORD, REFIID, void **) = (void *)GetProcAddress( shcore, "CreateRandomAccessStreamOnFile" );
    HRESULT (WINAPI *over_ras)(IUnknown *, REFIID, void **) = (void *)GetProcAddress( shcore, "CreateStreamOverRandomAccessStream" );
    WCHAR path[MAX_PATH];
    IUnknown *ras = NULL;
    IStream *s = NULL;
    char buf[32] = { 0 };
    ULONG n = 0;
    DWORD got = 0;
    HANDLE f;
    HRESULT hr;

    if (!on_file || !over_ras) { puts( "FAIL  exports\nRESULT: FAIL" ); return 1; }
    CoInitializeEx( NULL, COINIT_MULTITHREADED );
    GetTempPathW( MAX_PATH, path );
    wcscat( path, L"sg-rasfile.bin" );
    DeleteFileW( path );

    hr = on_file( path, 0, &IID_IRandomAccessStream_, (void **)&ras );
    check( "Read of a missing file fails", FAILED(hr) && !ras, hr );
    hr = on_file( path, 1, &IID_IRandomAccessStream_, (void **)&ras );
    check( "ReadWrite makes the file and gives an IRandomAccessStream", hr == S_OK && ras, hr );
    if (!ras) { puts( "RESULT: FAIL" ); return 1; }
    hr = over_ras( ras, &IID_IStream, (void **)&s );
    if (s) hr = IStream_Write( s, "random access", 13, &n );
    check( "written through it", hr == S_OK && n == 13, hr );
    if (s) IStream_Release( s );
    IUnknown_Release( ras );
    ras = NULL;
    f = CreateFileW( path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL );
    ReadFile( f, buf, sizeof(buf), &got, NULL );
    CloseHandle( f );
    check( "the bytes are in the file", got == 13 && !memcmp( buf, "random access", 13 ), got );

    hr = on_file( path, 0, &IID_IRandomAccessStream_, (void **)&ras );
    check( "Read opens it now", hr == S_OK && ras, hr );
    s = NULL;
    memset( buf, 0, sizeof(buf) );
    if (ras && SUCCEEDED(over_ras( ras, &IID_IStream, (void **)&s )))
    {
        IStream_Read( s, buf, sizeof(buf), &n );
        check( "and reads the bytes", n == 13 && !memcmp( buf, "random access", 13 ), n );
        IStream_Release( s );
    }
    else check( "and reads the bytes", 0, 0 );
    if (ras) IUnknown_Release( ras );
    check( "a bad access mode: E_INVALIDARG", on_file( path, 7, &IID_IRandomAccessStream_, (void **)&ras ) == E_INVALIDARG, 0 );
    DeleteFileW( path );
    puts( fails ? "RESULT: FAIL" : "RESULT: PASS" );
    return fails != 0;
}
