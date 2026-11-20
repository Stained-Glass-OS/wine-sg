/* gdipprofile-probe: GdipSetImageAttributesOutputChannelColorProfile (Stained Glass OS wine-sg 8222). With a
 * CMYK profile the output channel separates the colours through it (sRGB to that profile), as computed here
 * with mscms; a bare file name is one in the colour directory. It returned NotImplemented and the channels
 * stayed the plain conversion. argv[1]: a CMYK ICC profile (Windows path). Prints name=value lines.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include <windows.h>
#include <icm.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef INDEX_DONT_CARE
#define INDEX_DONT_CARE 0
#endif

typedef int GpStatus;
typedef struct { UINT v; void *a, *b; BOOL c, d; } StartupInput;

static HMODULE g;
#define FN(name) static FARPROC p_##name
FN(GdiplusStartup); FN(GdipCreateBitmapFromScan0); FN(GdipGetImageGraphicsContext); FN(GdipBitmapGetPixel);
FN(GdipCreateImageAttributes); FN(GdipSetImageAttributesOutputChannel); FN(GdipDrawImageRectRectI);
FN(GdipSetImageAttributesOutputChannelColorProfile);
FN(GdipBitmapSetPixel); FN(GdipDeleteGraphics); FN(GdipDisposeImage); FN(GdipDisposeImageAttributes);
#define CALL(n, sig, ...) ((GpStatus (WINAPI *) sig) p_##n)(__VA_ARGS__)

/* the channel's gray through the profile, by mscms directly */
static int expected( const WCHAR *path, DWORD rgb, int channel )
{
    PROFILE src = { PROFILE_FILENAME }, dst = { PROFILE_FILENAME };
    WCHAR srgb[MAX_PATH];
    DWORD size = sizeof(srgb), intents[2] = { INTENT_PERCEPTUAL, INTENT_PERCEPTUAL };
    HPROFILE profiles[2];
    HTRANSFORM xform;
    BYTE in[4], out[4];

    GetStandardColorSpaceProfileW( NULL, LCS_sRGB, srgb, &size );
    src.pProfileData = srgb; src.cbDataSize = (lstrlenW( srgb ) + 1) * sizeof(WCHAR);
    dst.pProfileData = (void *)path; dst.cbDataSize = (lstrlenW( path ) + 1) * sizeof(WCHAR);
    profiles[0] = OpenColorProfileW( &src, PROFILE_READ, FILE_SHARE_READ, OPEN_EXISTING );
    profiles[1] = OpenColorProfileW( &dst, PROFILE_READ, FILE_SHARE_READ, OPEN_EXISTING );
    if (!profiles[0] || !profiles[1]) return -1;
    xform = CreateMultiProfileTransform( profiles, 2, intents, 2, BEST_MODE, INDEX_DONT_CARE );
    if (!xform) return -1;
    memcpy( in, &rgb, 4 );
    TranslateBitmapBits( xform, in, BM_xRGBQUADS, 1, 1, 4, out, BM_KYMCQUADS, 4, NULL, 0 );
    DeleteColorTransform( xform );
    CloseColorProfile( profiles[0] );
    CloseColorProfile( profiles[1] );
    return 255 - out[channel];
}

static int plain( DWORD rgb, int channel )
{
    int c = 255 - ((rgb >> 16) & 0xff), m = 255 - ((rgb >> 8) & 0xff), y = 255 - (rgb & 0xff), k = min( c, min( m, y ) );
    return 255 - (channel == 0 ? c : channel == 1 ? m : channel == 2 ? y : k);
}

/* draw a 4x4 picture of rgb through the attributes, the gray at the middle */
static int drawn( void *attr, DWORD rgb )
{
    void *src = NULL, *dst = NULL, *gfx = NULL;
    DWORD c = 0;
    int x, y;

    CALL(GdipCreateBitmapFromScan0, (INT, INT, INT, int, BYTE *, void **), 4, 4, 0, 0x26200a, NULL, &src);
    for (y = 0; y < 4; y++) for (x = 0; x < 4; x++) CALL(GdipBitmapSetPixel, (void *, INT, INT, DWORD), src, x, y, 0xff000000 | rgb);
    CALL(GdipCreateBitmapFromScan0, (INT, INT, INT, int, BYTE *, void **), 4, 4, 0, 0x26200a, NULL, &dst);
    CALL(GdipGetImageGraphicsContext, (void *, void **), dst, &gfx);
    CALL(GdipDrawImageRectRectI, (void *, void *, INT, INT, INT, INT, INT, INT, INT, INT, int, void *, void *, void *),
         gfx, src, 0, 0, 4, 4, 0, 0, 4, 4, 2 /* UnitPixel */, attr, NULL, NULL);
    CALL(GdipBitmapGetPixel, (void *, INT, INT, DWORD *), dst, 2, 2, &c);
    CALL(GdipDeleteGraphics, (void *), gfx);
    CALL(GdipDisposeImage, (void *), dst);
    CALL(GdipDisposeImage, (void *), src);
    return c & 0xff;
}

int main( int argc, char **argv )
{
    static const struct { DWORD rgb; int channel; const char *name; } cases[] =
    {
        { 0x808080, 0, "gray_c" }, { 0x808080, 3, "gray_k" }, { 0x0000ff, 1, "blue_m" }, { 0x00ff00, 2, "green_y" },
    };
    StartupInput in = { 1 };
    ULONG_PTR token;
    WCHAR path[MAX_PATH], dir[MAX_PATH], local[MAX_PATH];
    DWORD size = sizeof(dir);
    void *attr = NULL;
    GpStatus st;
    int i, all = 1, differs = 0;

    if (argc < 2) return 1;
    MultiByteToWideChar( CP_ACP, 0, argv[1], -1, path, MAX_PATH );
    setvbuf( stdout, NULL, _IONBF, 0 );
    g = LoadLibraryA( "gdiplus.dll" );
#define L(n) p_##n = GetProcAddress( g, #n )
    L(GdiplusStartup); L(GdipCreateBitmapFromScan0); L(GdipGetImageGraphicsContext); L(GdipBitmapGetPixel);
    L(GdipCreateImageAttributes); L(GdipSetImageAttributesOutputChannel); L(GdipDrawImageRectRectI);
    L(GdipSetImageAttributesOutputChannelColorProfile);
    L(GdipBitmapSetPixel); L(GdipDeleteGraphics); L(GdipDisposeImage); L(GdipDisposeImageAttributes);
    if (CALL(GdiplusStartup, (ULONG_PTR *, StartupInput *, void *), &token, &in, NULL)) { printf( "no_gdiplus=1\n" ); return 1; }

    CALL(GdipCreateImageAttributes, (void **), &attr);
    st = CALL(GdipSetImageAttributesOutputChannelColorProfile, (void *, int, BOOL, const WCHAR *), attr, 0, TRUE, path);
    printf( "set_profile=%d status %d\n", st == 0, st );
    for (i = 0; i < ARRAYSIZE(cases); i++)
    {
        int want = expected( path, cases[i].rgb, cases[i].channel ), naive = plain( cases[i].rgb, cases[i].channel ), got;
        CALL(GdipSetImageAttributesOutputChannel, (void *, int, BOOL, int), attr, 0, TRUE, cases[i].channel);
        got = drawn( attr, cases[i].rgb );
        printf( "      %s: got %d, through the profile %d, plain %d\n", cases[i].name, got, want, naive );
        if (want < 0 || abs( got - want ) > 3) all = 0;
        if (abs( want - naive ) > 10) differs = 1;
    }
    printf( "through_profile=%d\n", all );
    printf( "profile_differs=%d\n", differs );

    /* a bare name: the profile in the colour directory */
    GetColorDirectoryW( NULL, dir, &size );
    swprintf( local, MAX_PATH, L"%ls\\sgprofile-cmyk.icc", dir );
    if (!CopyFileW( path, local, FALSE )) printf( "      copy to %ls failed: %lu\n", local, GetLastError() );
    st = CALL(GdipSetImageAttributesOutputChannelColorProfile, (void *, int, BOOL, const WCHAR *), attr, 0, TRUE,
              L"sgprofile-cmyk.icc");
    CALL(GdipSetImageAttributesOutputChannel, (void *, int, BOOL, int), attr, 0, TRUE, 0);
    i = drawn( attr, 0x808080 );
    printf( "bare_name=%d status %d got %d want %d\n", st == 0 && abs( i - expected( local, 0x808080, 0 ) ) <= 3, st, i,
            expected( local, 0x808080, 0 ) );
    DeleteFileW( local );

    /* off again: the plain conversion */
    CALL(GdipSetImageAttributesOutputChannelColorProfile, (void *, int, BOOL, const WCHAR *), attr, 0, FALSE, NULL);
    i = drawn( attr, 0x808080 );
    printf( "profile_off=%d got %d\n", abs( i - plain( 0x808080, 0 ) ) <= 1, i );

    st = CALL(GdipSetImageAttributesOutputChannelColorProfile, (void *, int, BOOL, const WCHAR *), attr, 0, TRUE, NULL);
    printf( "null_name=%d status %d\n", st == 2 /* InvalidParameter */, st );
    st = CALL(GdipSetImageAttributesOutputChannelColorProfile, (void *, int, BOOL, const WCHAR *), NULL, 0, TRUE, path);
    printf( "null_attr=%d status %d\n", st == 2, st );
    printf( "done=1\n" );
    return 0;
}
