/* glthumb-gate.sh's probe (0816): a window that draws only through OpenGL
 * (as Direct3D and WPF do under Wine: straight to its X window), green, for
 * a minute; "GL Thumb". "glthumb-probe gray": its class paints its
 * background dark gray first, as Chrome's does (0843): what Wine keeps of
 * it is then gray, not black. SPDX-License-Identifier: LGPL-2.1-or-later */
#include <windows.h>
#include <GL/gl.h>
#include <string.h>

static int gray;

static LRESULT CALLBACK proc( HWND hwnd, UINT msg, WPARAM wp, LPARAM lp )
{
    if (msg == WM_DESTROY) PostQuitMessage( 0 );
    if (msg == WM_ERASEBKGND && !gray) return 1;
    return DefWindowProcW( hwnd, msg, wp, lp );
}

int main( int argc, char **argv )
{
    WNDCLASSW wc = { CS_OWNDC, proc, 0, 0, NULL, NULL, LoadCursorW( NULL, (const WCHAR *)IDC_ARROW ), NULL, NULL, L"GlThumb" };
    PIXELFORMATDESCRIPTOR pfd = { sizeof(pfd), 1, PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER, PFD_TYPE_RGBA, 24 };
    HWND hwnd;
    HDC dc;
    HGLRC rc;
    MSG msg;
    DWORD start = GetTickCount();

    if (argc > 1 && !strcmp( argv[1], "gray" ))
    {
        gray = 1;
        wc.hbrBackground = CreateSolidBrush( RGB( 0x20, 0x20, 0x20 ) );
    }
    RegisterClassW( &wc );
    hwnd = CreateWindowW( L"GlThumb", L"GL Thumb", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 150, 120, 420, 320, NULL, NULL, NULL, NULL );
    dc = GetDC( hwnd );
    SetPixelFormat( dc, ChoosePixelFormat( dc, &pfd ), &pfd );
    rc = wglCreateContext( dc );
    wglMakeCurrent( dc, rc );
    while (GetTickCount() - start < 90000)
    {
        while (PeekMessageW( &msg, NULL, 0, 0, PM_REMOVE ))
        {
            if (msg.message == WM_QUIT) return 0;
            TranslateMessage( &msg );
            DispatchMessageW( &msg );
        }
        glClearColor( 0.1f, 0.8f, 0.2f, 1.0f );
        glClear( GL_COLOR_BUFFER_BIT );
        SwapBuffers( dc );
        Sleep( 50 );
    }
    return 0;
}
