/* gdipddsurf-probe: gdiplus GdipCreateBitmapFromDirectDrawSurface, a spec stub (Stained Glass OS wine-sg 8283;
 * test/gdipddsurf-gate.sh): a 32-bit system-memory DirectDraw surface's pixels end up in the bitmap.
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#define COBJMACROS
#include <windows.h>
#include <ddraw.h>
#include <stdio.h>

typedef struct { UINT32 version; void *cb; BOOL nobg, noext; } StartupInput;
int main( void )
{
    HMODULE gp = LoadLibraryA( "gdiplus.dll" );
    int (WINAPI *pStartup)(ULONG_PTR *, const StartupInput *, void *) = (void *)GetProcAddress( gp, "GdiplusStartup" );
    int (WINAPI *pFromSurface)(IDirectDrawSurface7 *, void **) = (void *)GetProcAddress( gp, "GdipCreateBitmapFromDirectDrawSurface" );
    int (WINAPI *pGetPixel)(void *, INT, INT, DWORD *) = (void *)GetProcAddress( gp, "GdipBitmapGetPixel" );
    int (WINAPI *pSize)(void *, UINT *, UINT *) = NULL;
    StartupInput in = { 1 };
    ULONG_PTR token;
    IDirectDraw7 *dd = NULL;
    IDirectDrawSurface7 *surf = NULL;
    DDSURFACEDESC2 desc;
    void *bitmap = NULL;
    DWORD px = 0, px2 = 0;
    UINT w = 0, h = 0;
    int st, x, y, fails = 0;

    pSize = (void *)GetProcAddress( gp, "GdipGetImageWidth" );
    pStartup( &token, &in, NULL );
    if (FAILED(DirectDrawCreateEx( NULL, (void **)&dd, &IID_IDirectDraw7, NULL ))) { printf( "FAIL  DirectDrawCreateEx\nRESULT: FAIL\n" ); return 1; }
    IDirectDraw7_SetCooperativeLevel( dd, NULL, DDSCL_NORMAL );
    memset( &desc, 0, sizeof(desc) );
    desc.dwSize = sizeof(desc);
    desc.dwFlags = DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT | DDSD_PIXELFORMAT;
    desc.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN | DDSCAPS_SYSTEMMEMORY;
    desc.dwWidth = 8; desc.dwHeight = 4;
    desc.ddpfPixelFormat.dwSize = sizeof(desc.ddpfPixelFormat);
    desc.ddpfPixelFormat.dwFlags = DDPF_RGB;
    desc.ddpfPixelFormat.dwRGBBitCount = 32;
    desc.ddpfPixelFormat.dwRBitMask = 0xff0000; desc.ddpfPixelFormat.dwGBitMask = 0xff00; desc.ddpfPixelFormat.dwBBitMask = 0xff;
    if (FAILED(IDirectDraw7_CreateSurface( dd, &desc, &surf, NULL ))) { printf( "FAIL  CreateSurface\nRESULT: FAIL\n" ); return 1; }
    memset( &desc, 0, sizeof(desc) ); desc.dwSize = sizeof(desc);
    IDirectDrawSurface7_Lock( surf, NULL, &desc, DDLOCK_WAIT, NULL );
    for (y = 0; y < 4; y++) for (x = 0; x < 8; x++)
        ((DWORD *)((BYTE *)desc.lpSurface + y * desc.lPitch))[x] = (x * 30) << 16 | (y * 60) << 8 | 0x33;
    IDirectDrawSurface7_Unlock( surf, NULL );

    st = pFromSurface( surf, &bitmap );
    printf( "%s  GdipCreateBitmapFromDirectDrawSurface: Ok (%d)\n", st == 0 && bitmap ? "PASS" : "FAIL", st );
    if (st || !bitmap) { printf( "RESULT: FAIL\n" ); return 1; }
    pSize( bitmap, &w, NULL );
    pGetPixel( bitmap, 3, 2, &px );
    pGetPixel( bitmap, 7, 0, &px2 );
    printf( "%s  8 wide, pixel (3,2) = ff5a7833 (%08lx), (7,0) = ffd20033 (%08lx)\n", w == 8 && px == 0xff5a7833 && px2 == 0xffd20033 ? "PASS" : "FAIL", px, px2 );
    if (!(w == 8 && px == 0xff5a7833 && px2 == 0xffd20033)) fails++;
    printf( "%s  no surface: InvalidParameter\n", pFromSurface( NULL, &bitmap ) == 2 ? "PASS" : "FAIL" );
    printf( "RESULT: %s\n", fails ? "FAIL" : "PASS" );
    return fails != 0;
}
