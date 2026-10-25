/* fontsnoz-probe: GDI knows Wine's own families (Tahoma, Marlett) -- loaded from Wine's data
 * directory, not only through the registry's paths. Prints name=value lines. */
#include <windows.h>
#include <stdio.h>

static int CALLBACK found( const LOGFONTW *lf, const TEXTMETRICW *tm, DWORD type, LPARAM lp )
{
    ++*(int *)lp;
    return 0;
}

static int has_family( HDC dc, const WCHAR *name )
{
    LOGFONTW lf = { 0 };
    int n = 0;
    lf.lfCharSet = DEFAULT_CHARSET;
    lstrcpyW( lf.lfFaceName, name );
    EnumFontFamiliesExW( dc, &lf, found, (LPARAM)&n, 0 );
    return n > 0;
}

int main( void )
{
    HDC dc = GetDC( 0 );
    printf( "tahoma=%d\n", has_family( dc, L"Tahoma" ) );
    printf( "marlett=%d\n", has_family( dc, L"Marlett" ) );
    ReleaseDC( 0, dc );
    return 0;
}
