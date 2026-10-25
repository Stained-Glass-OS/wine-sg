/* fontsfolder-probe: the system's fonts folder holds the fonts (patches/sg/6533).
 *
 *   probe scan HOSTFONT   lists C:\windows\Fonts as fontconfig does (FindFirstFile, then each file's
 *                         header), and looks for Wine's own tahoma.ttf and the host font HOSTFONT
 *   probe count           the GDI and DirectWrite family counts, and where the registry puts HOSTFONT
 * Prints name=value lines; see test/fontsfolder-gate.sh.
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <dwrite.h>
#include <stdio.h>
#include <string.h>

static int families;
static WCHAR last_family[LF_FACESIZE];

static int CALLBACK enum_proc( const LOGFONTW *lf, const TEXTMETRICW *tm, DWORD type, LPARAM lp )
{
    if (lf->lfFaceName[0] == '@') return 1;
    if (wcscmp( lf->lfFaceName, last_family )) { families++; lstrcpynW( last_family, lf->lfFaceName, LF_FACESIZE ); }
    return 1;
}

static BOOL font_header( const WCHAR *path )
{
    HANDLE file = CreateFileW( path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL );
    BYTE head[4];
    DWORD got = 0;

    if (file == INVALID_HANDLE_VALUE) return FALSE;
    ReadFile( file, head, 4, &got, NULL );
    CloseHandle( file );
    return got == 4 && (!memcmp( head, "\0\1\0\0", 4 ) || !memcmp( head, "OTTO", 4 ) || !memcmp( head, "ttcf", 4 ) ||
                        !memcmp( head, "true", 4 ));
}

int main( int argc, char **argv )
{
    setvbuf( stdout, NULL, _IONBF, 0 );
    if (argc > 2 && !strcmp( argv[1], "scan" ))
    {
        WIN32_FIND_DATAW fd;
        WCHAR host[MAX_PATH];
        HANDLE find;
        int files = 0, readable = 0, wine = 0, hostfound = 0;

        MultiByteToWideChar( CP_UTF8, 0, argv[2], -1, host, MAX_PATH );
        if ((find = FindFirstFileW( L"C:\\windows\\Fonts\\*", &fd )) != INVALID_HANDLE_VALUE)
        {
            do
            {
                WCHAR path[MAX_PATH];
                if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
                files++;
                swprintf( path, MAX_PATH, L"C:\\windows\\Fonts\\%ls", fd.cFileName );
                if (font_header( path )) readable++;
                if (!wcsicmp( fd.cFileName, L"tahoma.ttf" ) && font_header( path )) wine = 1;
                if (!wcsicmp( fd.cFileName, host ) && font_header( path )) hostfound = 1;
            } while (FindNextFileW( find, &fd ));
            FindClose( find );
        }
        printf( "folder files=%d readable=%d\n", files, readable );
        printf( "wine_font=%d\n", wine );
        printf( "host_font=%d name=%s\n", hostfound, argv[2] );
    }
    else
    {
        IDWriteFactory *factory;
        IDWriteFontCollection *collection;
        LOGFONTW lf = { 0 };
        HDC dc = GetDC( 0 );
        UINT32 dw = 0;

        lf.lfCharSet = DEFAULT_CHARSET;
        EnumFontFamiliesExW( dc, &lf, enum_proc, 0, 0 );
        ReleaseDC( 0, dc );
        if (SUCCEEDED(DWriteCreateFactory( DWRITE_FACTORY_TYPE_SHARED, &IID_IDWriteFactory, (IUnknown **)&factory )) &&
            SUCCEEDED(IDWriteFactory_GetSystemFontCollection( factory, &collection, FALSE )))
            dw = IDWriteFontCollection_GetFontFamilyCount( collection );
        printf( "gdi_families=%d dwrite_families=%u\n", families, dw );
    }
    return 0;
}
