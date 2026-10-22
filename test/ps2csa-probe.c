/* ps2csa-probe: mscms GetPS2ColorSpaceArray / GetPS2ColorRenderingDictionary: PostScript Level 2
 * color resources made from a profile (Stained Glass OS wine-sg 6020).   ps2csa-probe.exe */
#include <windows.h>
#include <icm.h>
#include <stdio.h>
#include <string.h>

static int failures;

static void check( int ok, const char *what, long got )
{
    if (ok) printf( "ok    %s\n", what );
    else { printf( "FAIL  %s (got %#lx)\n", what, got ); failures++; }
}

int main( void )
{
    char path[MAX_PATH];
    DWORD size = sizeof(path), need;
    PROFILE prof;
    HPROFILE h;
    BOOL ret, binary;
    BYTE *buf;

    if (!GetStandardColorSpaceProfileA( NULL, LCS_sRGB, path, &size ))
    {
        printf( "FAIL  no sRGB profile (%lu)\nRESULT: FAIL\n", GetLastError() );
        return 1;
    }
    prof.dwType = PROFILE_FILENAME;
    prof.pProfileData = path;
    prof.cbDataSize = strlen( path ) + 1;
    h = OpenColorProfileA( &prof, PROFILE_READ, FILE_SHARE_READ, OPEN_EXISTING );
    check( h != NULL, "the sRGB profile", GetLastError() );
    if (!h) { printf( "RESULT: FAIL\n" ); return 1; }

    /* the color space array */
    need = 0;
    binary = TRUE;
    SetLastError( 0xdeadbeef );
    ret = GetPS2ColorSpaceArray( h, INTENT_PERCEPTUAL, CSA_RGB, NULL, &need, &binary );
    check( !ret && GetLastError() == ERROR_INSUFFICIENT_BUFFER && need > 100,
           "no buffer: ERROR_INSUFFICIENT_BUFFER and the size", need );
    buf = calloc( 1, need + 1 );
    size = need;
    ret = GetPS2ColorSpaceArray( h, INTENT_PERCEPTUAL, CSA_RGB, buf, &size, &binary );
    check( ret && size > 0 && size <= need, "GetPS2ColorSpaceArray", GetLastError() );
    check( !binary, "in ASCII", binary );
    check( ret && (strstr( (char *)buf, "/CIEBasedABC" ) || strstr( (char *)buf, "/CIEBasedDEF" )),
           "a CIE-based color space for an RGB profile", 0 );
    check( ret && strstr( (char *)buf, "/WhitePoint" ) != NULL, "with its white point", 0 );
    free( buf );
    size = 4;
    buf = calloc( 1, 4 );
    SetLastError( 0xdeadbeef );
    ret = GetPS2ColorSpaceArray( h, INTENT_PERCEPTUAL, CSA_RGB, buf, &size, &binary );
    check( !ret && GetLastError() == ERROR_INSUFFICIENT_BUFFER && size == need, "a small buffer gets the size", size );
    free( buf );
    SetLastError( 0xdeadbeef );
    ret = GetPS2ColorSpaceArray( h, INTENT_PERCEPTUAL, 0, NULL, &need, &binary );
    check( !ret && GetLastError() == ERROR_INVALID_PARAMETER, "type 0: ERROR_INVALID_PARAMETER", GetLastError() );

    /* the color rendering dictionary */
    need = 0;
    SetLastError( 0xdeadbeef );
    ret = GetPS2ColorRenderingDictionary( h, INTENT_RELATIVE_COLORIMETRIC, NULL, &need, &binary );
    check( !ret && GetLastError() == ERROR_INSUFFICIENT_BUFFER && need > 100, "the CRD's size", need );
    buf = calloc( 1, need + 1 );
    size = need;
    ret = GetPS2ColorRenderingDictionary( h, INTENT_RELATIVE_COLORIMETRIC, buf, &size, &binary );
    check( ret && strstr( (char *)buf, "/ColorRenderingType" ) != NULL, "GetPS2ColorRenderingDictionary", GetLastError() );
    free( buf );

    CloseColorProfile( h );
    printf( "RESULT: %s\n", failures ? "FAIL" : "PASS" );
    return failures != 0;
}
