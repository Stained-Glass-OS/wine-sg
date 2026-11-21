/* mediaclass-probe: urlmon RegisterMediaTypeClass / FindMediaTypeClass, which were spec stubs (Stained Glass OS
 * wine-sg 8249; test/mediaclass-gate.sh): a bind context's media type -> CLSID override is found, the MIME
 * database is the fallback, another bind context does not see the override.
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#define COBJMACROS
#include <windows.h>
#include <objbase.h>
#include <stdio.h>

static const CLSID clsid_a = { 0x11111111, 0x2222, 0x3333, { 0x44, 0x44, 0x55, 0x55, 0x55, 0x55, 0x55, 0x01 } };
static const CLSID clsid_b = { 0x11111111, 0x2222, 0x3333, { 0x44, 0x44, 0x55, 0x55, 0x55, 0x55, 0x55, 0x02 } };
static const CLSID clsid_db = { 0x11111111, 0x2222, 0x3333, { 0x44, 0x44, 0x55, 0x55, 0x55, 0x55, 0x55, 0x03 } };

static int failures;
static void check( int ok, const char *what, long got )
{
    printf( "%s  %s (%#lx)\n", ok ? "PASS" : "FAIL", what, got );
    if (!ok) failures++;
}

int main( void )
{
    HMODULE urlmon = LoadLibraryA( "urlmon.dll" );
    HRESULT (WINAPI *pRegister)(IBindCtx *, UINT, const char **, CLSID *, DWORD) = (void *)GetProcAddress( urlmon, "RegisterMediaTypeClass" );
    HRESULT (WINAPI *pFind)(IBindCtx *, const char *, CLSID *, DWORD) = (void *)GetProcAddress( urlmon, "FindMediaTypeClass" );
    const char *types[2] = { "application/x-sg-one", "application/x-sg-two" };
    CLSID clsids[2] = { clsid_a, clsid_b }, got;
    IBindCtx *bctx, *other;
    HKEY key;
    HRESULT hr;

    CoInitialize( NULL );
    CreateBindCtx( 0, &bctx );
    CreateBindCtx( 0, &other );
    RegCreateKeyA( HKEY_LOCAL_MACHINE, "Software\\Classes\\MIME\\Database\\Content Type\\application/x-sg-db", &key );
    RegSetValueExA( key, "CLSID", 0, REG_SZ, (const BYTE *)"{11111111-2222-3333-4444-555555555503}", 39 );
    RegCloseKey( key );

    hr = pRegister( bctx, 2, types, clsids, 0 );
    check( hr == S_OK, "RegisterMediaTypeClass of two types", hr );
    hr = pFind( bctx, "application/x-sg-two", &got, 0 );
    check( hr == S_OK && IsEqualCLSID( &got, &clsid_b ), "FindMediaTypeClass: the bind context's override", hr );
    hr = pFind( bctx, "APPLICATION/X-SG-ONE", &got, 0 );
    check( hr == S_OK && IsEqualCLSID( &got, &clsid_a ), "  case-insensitive", hr );
    clsids[0] = clsid_b;
    pRegister( bctx, 1, types, clsids, 0 );
    hr = pFind( bctx, "application/x-sg-one", &got, 0 );
    check( hr == S_OK && IsEqualCLSID( &got, &clsid_b ), "  a second registration replaces the first", hr );
    hr = pFind( other, "application/x-sg-one", &got, 0 );
    check( FAILED(hr), "another bind context does not see it", hr );
    hr = pFind( bctx, "application/x-sg-db", &got, 0 );
    check( hr == S_OK && IsEqualCLSID( &got, &clsid_db ), "the MIME database's CLSID otherwise", hr );
    hr = pFind( NULL, "application/x-sg-db", &got, 0 );
    check( hr == S_OK && IsEqualCLSID( &got, &clsid_db ), "  also with no bind context", hr );
    check( pRegister( bctx, 0, types, clsids, 0 ) == E_INVALIDARG, "no types: E_INVALIDARG", 0 );
    check( pFind( bctx, NULL, &got, 0 ) == E_INVALIDARG, "no type: E_INVALIDARG", 0 );
    IBindCtx_Release( other );
    IBindCtx_Release( bctx );
    printf( "RESULT: %s\n", failures ? "FAIL" : "PASS" );
    return failures != 0;
}
