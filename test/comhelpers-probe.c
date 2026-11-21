/* comhelpers-probe: MonikerRelativePathTo (ole32), CoQueryAuthenticationServices (combase/ole32) and
 * OleLoadPictureFileEx (oleaut32), which were spec stubs (Stained Glass OS wine-sg 8240;
 * test/comhelpers-gate.sh).
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#define COBJMACROS
#include <windows.h>
#include <ole2.h>
#include <olectl.h>
#include <stdio.h>

static int failures;
static void check( int ok, const char *what, long got )
{
    printf( "%s  %s (%#lx)\n", ok ? "PASS" : "FAIL", what, got );
    if (!ok) failures++;
}

static IMoniker *item( const WCHAR *name )
{
    IMoniker *m = NULL;
    CreateItemMoniker( L"!", name, &m );
    return m;
}

static IMoniker *comp( IMoniker *a, IMoniker *b )
{
    IMoniker *m = NULL;
    CreateGenericComposite( a, b, &m );
    return m;
}

/* an icon file of two 1-bit images, 16x16 and 32x32 */
static void write_icon( const WCHAR *path )
{
    static const int sizes[2] = { 16, 32 };
    BYTE buf[4096] = {0};
    DWORD pos = 6 + 2 * 16, i, written;
    HANDLE f;

    buf[2] = 1; buf[4] = 2;
    for (i = 0; i < 2; i++)
    {
        int s = sizes[i], row = ((s + 31) / 32) * 4, len = 40 + 8 + 2 * row * s;
        BYTE *e = buf + 6 + 16 * i, *img = buf + pos;
        BITMAPINFOHEADER *bih = (BITMAPINFOHEADER *)img;
        e[0] = s; e[1] = s; e[4] = 1; e[6] = 1;
        *(DWORD *)(e + 8) = len; *(DWORD *)(e + 12) = pos;
        bih->biSize = 40; bih->biWidth = s; bih->biHeight = 2 * s; bih->biPlanes = 1; bih->biBitCount = 1;
        img[40 + 4] = img[40 + 5] = img[40 + 6] = 0xff;     /* palette: black, white */
        memset( img + 48, 0xaa, row * s );                  /* XOR bits */
        pos += len;
    }
    f = CreateFileW( path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL );
    WriteFile( f, buf, pos, &written, NULL );
    CloseHandle( f );
}

int main( void )
{
    HMODULE ole32 = GetModuleHandleA( "ole32.dll" ), oleaut = LoadLibraryA( "oleaut32.dll" );
    HRESULT (WINAPI *pRelPath)(IMoniker *, IMoniker *, IMoniker **, BOOL) = (void *)GetProcAddress( ole32, "MonikerRelativePathTo" );
    HRESULT (WINAPI *pQueryAuth)(DWORD *, SOLE_AUTHENTICATION_SERVICE **) = (void *)GetProcAddress( ole32, "CoQueryAuthenticationServices" );
    HRESULT (WINAPI *pLoadEx)(VARIANT, DWORD, DWORD, DWORD, IDispatch **) = (void *)GetProcAddress( oleaut, (const char *)402 );
    IMoniker *a = item( L"a" ), *src, *dest, *rel = NULL, *back = NULL;
    SOLE_AUTHENTICATION_SERVICE *svc = NULL;
    WCHAR path[MAX_PATH];
    IDispatch *disp = NULL;
    IPicture *pic;
    OLE_XSIZE_HIMETRIC w16 = 0, w32 = 0;
    DWORD count = 99;
    VARIANT v;
    HRESULT hr;

    CoInitialize( NULL );
    src = comp( a, item( L"b" ) );
    dest = comp( a, item( L"c" ) );
    hr = pRelPath( src, dest, &rel, FALSE );
    if (rel) IMoniker_ComposeWith( src, rel, FALSE, &back );
    check( hr == S_OK && back && IMoniker_IsEqual( back, dest ) == S_OK,
           "MonikerRelativePathTo: a!b + relative path = a!c", hr );
    if (rel) IMoniker_Release( rel );
    rel = NULL;
    hr = pRelPath( item( L"x" ), dest, &rel, FALSE );
    check( hr == MK_S_HIM && rel && IMoniker_IsEqual( rel, dest ) == S_OK, "no common prefix: the destination, MK_S_HIM", hr );

    hr = pQueryAuth( &count, &svc );
    check( hr == S_OK && count == 0 && !svc, "CoQueryAuthenticationServices before CoInitializeSecurity: none", count );
    CoInitializeSecurity( NULL, -1, NULL, NULL, RPC_C_AUTHN_LEVEL_CONNECT, RPC_C_IMP_LEVEL_IDENTIFY, NULL, EOAC_NONE, NULL );
    hr = pQueryAuth( &count, &svc );
    check( hr == S_OK && count == 2 && svc && svc[0].dwAuthnSvc == RPC_C_AUTHN_WINNT, "after CoInitializeSecurity(-1): COM's own", count );
    CoTaskMemFree( svc );

    GetTempPathW( MAX_PATH, path );
    lstrcatW( path, L"sgcomhelpers.ico" );
    write_icon( path );
    V_VT(&v) = VT_BSTR;
    V_BSTR(&v) = SysAllocString( path );
    hr = pLoadEx( v, 16, 16, LP_DEFAULT, &disp );
    if (disp && SUCCEEDED(IDispatch_QueryInterface( disp, &IID_IPicture, (void **)&pic )))
    {
        IPicture_get_Width( pic, &w16 );
        IPicture_Release( pic );
        IDispatch_Release( disp );
    }
    check( hr == S_OK && w16 > 0, "OleLoadPictureFileEx loads the icon at 16x16", hr );
    disp = NULL;
    hr = pLoadEx( v, 32, 32, LP_DEFAULT, &disp );
    if (disp && SUCCEEDED(IDispatch_QueryInterface( disp, &IID_IPicture, (void **)&pic )))
    {
        IPicture_get_Width( pic, &w32 );
        IPicture_Release( pic );
        IDispatch_Release( disp );
    }
    printf( "      widths (HIMETRIC): %ld, %ld\n", w16, w32 );
    check( hr == S_OK && w32 > w16, "and at 32x32 the larger image", hr );
    V_VT(&v) = VT_I4;
    check( pLoadEx( v, 16, 16, 0, &disp ) == CTL_E_FILENOTFOUND, "a name that is no string: CTL_E_FILENOTFOUND", 0 );
    DeleteFileW( path );
    printf( "RESULT: %s\n", failures ? "FAIL" : "PASS" );
    return failures != 0;
}
