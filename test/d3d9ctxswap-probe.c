/* d3d9ctxswap-probe: wined3d contexts and swapchains that go away or lose their window's pixel format
 * (Stained Glass OS wine-sg 8293, 8294; test/d3d9ctxswap-gate.sh).  Sweet Home 3D: Java2D draws with
 * Direct3D 9 (a swapchain per component, dropped on resize) and Java 3D with OpenGL on the same window.
 * 1 (8293): a context that drew to an additional swapchain kept pointing at it after it was released, and
 *   the next offscreen draw read the freed swapchain's window (run with warn+heap: freed memory is filled).
 * 2 (8294): when the window's pixel format was taken by the program's own OpenGL, wined3d falls back to its
 *   backup window, and took the backup window for one with a format already: wglMakeCurrent failed and every
 *   draw after it (Lock gives no memory, the program faulted in its window procedures).
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#define COBJMACROS
#include <windows.h>
#include <d3d9.h>
#include <stdio.h>

static int fails;
static void check( int ok, const char *what, DWORD v ) { printf( "%s  %s (%#lx)\n", ok ? "PASS" : "FAIL", what, v ); if (!ok) fails++; }

static HWND make_window( int x )
{
    HWND hwnd = CreateWindowA( "static", "d3d9ctxswap", WS_POPUP | WS_VISIBLE, x, 0, 200, 150, NULL, NULL, NULL, NULL );
    UpdateWindow( hwnd );
    return hwnd;
}

static void pump(void)
{
    MSG msg;
    while (PeekMessageA( &msg, NULL, 0, 0, PM_REMOVE )) DispatchMessageA( &msg );
}

/* clear the target to colour, read it back through system memory */
static DWORD clear_and_read( IDirect3DDevice9 *dev, IDirect3DSurface9 *rt, D3DCOLOR colour )
{
    D3DSURFACE_DESC desc;
    IDirect3DSurface9 *sys;
    D3DLOCKED_RECT lr;
    DWORD ret = 0xdeadbeef;

    IDirect3DSurface9_GetDesc( rt, &desc );
    IDirect3DDevice9_SetRenderTarget( dev, 0, rt );
    IDirect3DDevice9_Clear( dev, 0, NULL, D3DCLEAR_TARGET, colour, 1.0f, 0 );
    if (FAILED(IDirect3DDevice9_CreateOffscreenPlainSurface( dev, desc.Width, desc.Height, desc.Format, D3DPOOL_SYSTEMMEM, &sys, NULL )))
        return ret;
    if (SUCCEEDED(IDirect3DDevice9_GetRenderTargetData( dev, rt, sys )) &&
        SUCCEEDED(IDirect3DSurface9_LockRect( sys, &lr, NULL, D3DLOCK_READONLY )))
    {
        ret = lr.pBits ? *(DWORD *)lr.pBits & 0xffffff : 0xbad;
        IDirect3DSurface9_UnlockRect( sys );
    }
    IDirect3DSurface9_Release( sys );
    return ret;
}

int main( void )
{
    IDirect3D9 *d3d = Direct3DCreate9( D3D_SDK_VERSION );
    D3DPRESENT_PARAMETERS pp = { 0 };
    IDirect3DDevice9 *dev;
    IDirect3DSwapChain9 *sc;
    IDirect3DSurface9 *bb, *primary_bb, *off;
    HWND a = make_window( 0 ), b = make_window( 220 ), c = make_window( 440 );
    HRESULT hr;
    DWORD v;

    pp.Windowed = TRUE;
    pp.SwapEffect = D3DSWAPEFFECT_COPY;
    pp.BackBufferFormat = D3DFMT_X8R8G8B8;
    pp.BackBufferWidth = 200; pp.BackBufferHeight = 150;
    pp.hDeviceWindow = a;
    hr = IDirect3D9_CreateDevice( d3d, 0, D3DDEVTYPE_HAL, a, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &pp, &dev );
    if (FAILED(hr)) { printf( "FAIL  no device (%#lx)\nRESULT: FAIL\n", hr ); return 1; }
    IDirect3DDevice9_GetBackBuffer( dev, 0, 0, D3DBACKBUFFER_TYPE_MONO, &primary_bb );

    /* 1: draw to an additional swapchain, release it, draw offscreen */
    pp.hDeviceWindow = b;
    IDirect3DDevice9_CreateAdditionalSwapChain( dev, &pp, &sc );
    IDirect3DSwapChain9_GetBackBuffer( sc, 0, D3DBACKBUFFER_TYPE_MONO, &bb );
    IDirect3DDevice9_BeginScene( dev );
    v = clear_and_read( dev, bb, 0x00ff00 );
    IDirect3DDevice9_EndScene( dev );
    IDirect3DSwapChain9_Present( sc, NULL, NULL, NULL, NULL, 0 );
    check( v == 0x00ff00, "1: the additional swapchain is drawn", v );
    IDirect3DDevice9_SetRenderTarget( dev, 0, primary_bb );
    IDirect3DSurface9_Release( bb );
    IDirect3DSwapChain9_Release( sc );
    DestroyWindow( b );
    pump();
    IDirect3DDevice9_CreateRenderTarget( dev, 64, 64, D3DFMT_X8R8G8B8, D3DMULTISAMPLE_NONE, 0, FALSE, &off, NULL );
    v = clear_and_read( dev, off, 0x0000ff );
    check( v == 0x0000ff, "1: then an offscreen target is drawn", v );
    IDirect3DDevice9_SetRenderTarget( dev, 0, primary_bb );
    IDirect3DSurface9_Release( off );

    /* 2: the window's pixel format is taken by the program's own OpenGL, then the window is resized */
    pp.hDeviceWindow = c;
    IDirect3DDevice9_CreateAdditionalSwapChain( dev, &pp, &sc );
    IDirect3DSwapChain9_GetBackBuffer( sc, 0, D3DBACKBUFFER_TYPE_MONO, &bb );
    v = clear_and_read( dev, bb, 0xff0000 );
    IDirect3DSwapChain9_Present( sc, NULL, NULL, NULL, NULL, 0 );
    check( v == 0xff0000, "2: drawn before the program sets its own pixel format", v );
    {
        PIXELFORMATDESCRIPTOR pfd = { sizeof(pfd), 1, PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,
                                      PFD_TYPE_RGBA, 32, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 24, 8 };
        HDC dc = GetDC( c );
        int count = DescribePixelFormat( dc, 1, sizeof(pfd), NULL ), i, chosen = 0;
        PIXELFORMATDESCRIPTOR d;

        /* a format with depth and stencil and double buffering that wined3d does not use for this swapchain */
        for (i = count; i >= 1 && !chosen; i--)
            if (DescribePixelFormat( dc, i, sizeof(d), &d ) && (d.dwFlags & PFD_SUPPORT_OPENGL) &&
                (d.dwFlags & PFD_DRAW_TO_WINDOW) && d.cDepthBits >= 24 && d.cStencilBits && d.cAccumBits) chosen = i;
        if (!chosen) chosen = ChoosePixelFormat( dc, &pfd );
        check( SetPixelFormat( dc, chosen, &d ), "2: the program sets its own pixel format", chosen );
        ReleaseDC( c, dc );
    }
    SetWindowPos( c, NULL, 0, 0, 260, 190, SWP_NOMOVE | SWP_NOZORDER );
    pump();
    v = clear_and_read( dev, bb, 0x00ffff );
    IDirect3DSwapChain9_Present( sc, NULL, NULL, NULL, NULL, 0 );
    check( v == 0x00ffff, "2: still drawn after (through the backup window)", v );
    v = clear_and_read( dev, bb, 0xffff00 );
    check( v == 0xffff00, "2: and again", v );
    IDirect3DSurface9_Release( bb );
    IDirect3DSwapChain9_Release( sc );
    IDirect3DSurface9_Release( primary_bb );
    IDirect3DDevice9_Release( dev );
    IDirect3D9_Release( d3d );
    printf( "RESULT: %s\n", fails ? "FAIL" : "PASS" );
    return fails != 0;
}
