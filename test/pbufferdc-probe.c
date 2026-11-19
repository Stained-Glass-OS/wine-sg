/* SetPixelFormat on a pbuffer's DC (patches/sg/7905). Java 3D (Sweet Home 3D) takes the DC of
 * a pbuffer with wglGetPbufferDCARB, sets the pbuffer's own pixel format on it and makes its
 * offscreen context there; Wine refused the format ("not a valid window DC") and the program
 * ended with "a fatal error in the 3D rendering system". Prints key=1 per check. */
#include <windows.h>
#include <stdio.h>

#define WGL_DRAW_TO_PBUFFER_ARB 0x202D
#define WGL_SUPPORT_OPENGL_ARB  0x2010
#define WGL_PIXEL_TYPE_ARB      0x2013
#define WGL_TYPE_RGBA_ARB       0x202B
#define WGL_COLOR_BITS_ARB      0x2014
#define WGL_DEPTH_BITS_ARB      0x2022
#define WGL_DOUBLE_BUFFER_ARB   0x2011
typedef HANDLE (WINAPI *create_pbuffer_fn)(HDC, int, int, int, const int *);
typedef HDC (WINAPI *get_pbuffer_dc_fn)(HANDLE);
typedef int (WINAPI *release_pbuffer_dc_fn)(HANDLE, HDC);
typedef BOOL (WINAPI *destroy_pbuffer_fn)(HANDLE);
typedef BOOL (WINAPI *choose_fn)(HDC, const int *, const FLOAT *, UINT, int *, UINT *);

int main(void)
{
    PIXELFORMATDESCRIPTOR pfd = { sizeof(pfd), 1, PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,
                                  PFD_TYPE_RGBA, 32, 0,0,0,0,0,0, 0, 0, 0, 0,0,0,0, 24, 0, 0, PFD_MAIN_PLANE };
    static const int attribs[] = { WGL_DRAW_TO_PBUFFER_ARB, 1, WGL_SUPPORT_OPENGL_ARB, 1, WGL_PIXEL_TYPE_ARB,
                                   WGL_TYPE_RGBA_ARB, WGL_COLOR_BITS_ARB, 24, WGL_DEPTH_BITS_ARB, 16,
                                   WGL_DOUBLE_BUFFER_ARB, 0, 0 };
    create_pbuffer_fn pCreate; get_pbuffer_dc_fn pGetDC; release_pbuffer_dc_fn pReleaseDC;
    destroy_pbuffer_fn pDestroy; choose_fn pChoose;
    HWND hwnd; HDC hdc, pdc; HGLRC ctx, pctx; HANDLE pb;
    int fmt, formats[64], other = 0, cur; UINT n = 0, i;

    hwnd = CreateWindowA( "static", "pbufferdc", WS_POPUP, 0, 0, 64, 64, NULL, NULL, NULL, NULL );
    hdc = GetDC( hwnd );
    fmt = ChoosePixelFormat( hdc, &pfd );
    if (!SetPixelFormat( hdc, fmt, &pfd ) || !(ctx = wglCreateContext( hdc )) || !wglMakeCurrent( hdc, ctx ))
    { printf( "gl=0\ndone=1\n" ); return 0; }
    printf( "gl=1\n" );
    pCreate = (void *)wglGetProcAddress( "wglCreatePbufferARB" );
    pGetDC = (void *)wglGetProcAddress( "wglGetPbufferDCARB" );
    pReleaseDC = (void *)wglGetProcAddress( "wglReleasePbufferDCARB" );
    pDestroy = (void *)wglGetProcAddress( "wglDestroyPbufferARB" );
    pChoose = (void *)wglGetProcAddress( "wglChoosePixelFormatARB" );
    if (!pCreate || !pGetDC || !pChoose || !pChoose( hdc, attribs, NULL, 64, formats, &n ) || !n)
    { printf( "pbuffer=0\ndone=1\n" ); return 0; }
    if (!(pb = pCreate( hdc, formats[0], 1, 1, NULL )) || !(pdc = pGetDC( pb )))
    { printf( "pbuffer=0\ndone=1\n" ); return 0; }
    printf( "pbuffer=1\n" );
    cur = GetPixelFormat( pdc );
    printf( "getformat=%d\n", cur == formats[0] );
    for (i = 1; i <= (UINT)DescribePixelFormat( hdc, 1, 0, NULL ) && !other; i++) if ((int)i != cur) other = i;
    wglMakeCurrent( NULL, NULL );
    printf( "sameformat=%d\n", SetPixelFormat( pdc, cur, NULL ) == TRUE );
    printf( "otherformat=%d\n", SetPixelFormat( pdc, other, NULL ) == FALSE );
    printf( "stillformat=%d\n", GetPixelFormat( pdc ) == cur );
    pctx = wglCreateContext( pdc );
    printf( "context=%d\n", pctx && wglMakeCurrent( pdc, pctx ) );
    wglMakeCurrent( NULL, NULL );
    if (pctx) wglDeleteContext( pctx );
    printf( "screendc=%d\n", SetPixelFormat( GetDC( NULL ), cur, NULL ) == FALSE );
    pReleaseDC( pb, pdc ); pDestroy( pb );
    wglDeleteContext( ctx ); ReleaseDC( hwnd, hdc ); DestroyWindow( hwnd );
    printf( "done=1\n" );
    return 0;
}
