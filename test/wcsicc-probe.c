/* test/wcsicc-gate.sh's probe (7716): WcsCreateIccProfile, as Qt asks for each screen.
 * Opens the system's sRGB profile with WcsOpenColorProfile, makes an ICC profile of it, and prints
 * whether it is a handle, an ICC profile ('acsp'), the same bytes, and independent of the source.
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#include <windows.h>
#include <icm.h>
#include <stdio.h>
#include <string.h>

typedef HPROFILE (WINAPI *create_func)( HPROFILE, DWORD );

int main( void )
{
    WCHAR path[MAX_PATH];
    DWORD len = ARRAYSIZE(path), size1 = 0, size2 = 0;
    PROFILE src = { PROFILE_FILENAME };
    HMODULE mscms = LoadLibraryA( "mscms.dll" );
    create_func create = (create_func)GetProcAddress( mscms, "WcsCreateIccProfile" );
    HPROFILE h1, h2;
    BYTE *b1, *b2;

    setvbuf( stdout, NULL, _IONBF, 0 );
    if (!GetColorDirectoryW( NULL, path, &len )) return 1;
    lstrcatW( path, L"\\srgb color space profile.icm" );
    src.pProfileData = path;
    src.cbDataSize = (lstrlenW( path ) + 1) * sizeof(WCHAR);
    h1 = WcsOpenColorProfileW( &src, NULL, NULL, PROFILE_READ, FILE_SHARE_READ, OPEN_EXISTING, 0 );
    printf( "Source=%d\n", h1 != NULL );
    if (!h1 || !create) return 1;
    h2 = create( h1, 0 );
    printf( "Created=%d\n", h2 != NULL );
    if (!h2) return 1;
    GetColorProfileFromHandle( h1, NULL, &size1 );
    GetColorProfileFromHandle( h2, NULL, &size2 );
    b1 = malloc( size1 ); b2 = malloc( size2 );
    GetColorProfileFromHandle( h1, b1, &size1 );
    GetColorProfileFromHandle( h2, b2, &size2 );
    printf( "Icc=%d\n", size2 > 40 && !memcmp( b2 + 36, "acsp", 4 ) );
    printf( "Same=%d\n", size1 == size2 && !memcmp( b1, b2, size1 ) );
    CloseColorProfile( h1 );
    size2 = 0;
    GetColorProfileFromHandle( h2, NULL, &size2 );
    printf( "Independent=%d\n", size2 == size1 );
    printf( "Closed=%d\n", CloseColorProfile( h2 ) );
    printf( "Bad=%d\n", create( (HPROFILE)0xdead, 0 ) == NULL );
    return 0;
}
