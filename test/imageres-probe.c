/* The system's images (patches/sg/9635, i1012): imageres.dll and shell32.dll icons
 * programs load by resource id, as WireGuard for Windows' window does through
 * SHDefExtractIcon of system32\<dll>.dll. Prints "ICON dll id ok". */
#include <windows.h>
#include <shlobj.h>
#include <stdio.h>

static void one( const char *dll, int id, int size )
{
    WCHAR path[MAX_PATH];
    HICON icon = NULL;
    HRESULT hr;
    ICONINFO info;
    BITMAP bm = {0};

    GetSystemDirectoryW( path, MAX_PATH );
    swprintf( path + lstrlenW( path ), MAX_PATH - lstrlenW( path ), L"\\%hs.dll", dll );
    hr = SHDefExtractIconW( path, id, 0, NULL, &icon, MAKELONG( 0, size ) );
    if (hr == S_OK && icon && GetIconInfo( icon, &info ))
    {
        GetObjectW( info.hbmColor ? info.hbmColor : info.hbmMask, sizeof(bm), &bm );
        if (info.hbmColor) DeleteObject( info.hbmColor );
        DeleteObject( info.hbmMask );
    }
    printf( "ICON %s %d %d %ld\n", dll, id, hr == S_OK && icon != NULL, bm.bmWidth );
    if (icon) DestroyIcon( icon );
}

int main( void )
{
    static const int imageres[] = { 1, -2, -3, -4, -78, -106, -174, -1002, -1003, -1004, -1005, -1028 };
    static const int shell32[] = { -16739, -258, -240 };
    unsigned int i;
    UINT n;

    for (i = 0; i < ARRAYSIZE(imageres); i++) one( "imageres", imageres[i], 16 );
    for (i = 0; i < ARRAYSIZE(shell32); i++) one( "shell32", shell32[i], 16 );
    n = ExtractIconExW( L"imageres.dll", -1, NULL, NULL, 0 );
    printf( "COUNT %u\n", n );
    fflush( stdout );
    return 0;
}
