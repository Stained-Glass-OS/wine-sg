/* wine-sg 8412: drawing to a pbuffer's front buffer. Java 3D (Sweet Home 3D) clears a 1x1 pbuffer
 * with glDrawBuffer(GL_FRONT) and glFinish; with Mesa's software renderer the X server refused the
 * image (BadMatch on X_ShmPutImage) and the X error ended the program. Prints key=value lines;
 * the process dying prints nothing after "made=1". */
#include <windows.h>
#include <GL/gl.h>
#include <stdio.h>
typedef HANDLE (WINAPI *PFNCREATEPB)(HDC, int, int, int, const int *);
typedef HDC (WINAPI *PFNGETPBDC)(HANDLE);
typedef int (WINAPI *PFNRELEASEPBDC)(HANDLE, HDC);
typedef BOOL (WINAPI *PFNDESTROYPB)(HANDLE);
typedef BOOL (WINAPI *PFNCHOOSE)(HDC, const int *, const FLOAT *, UINT, int *, UINT *);

int main(void)
{
    HWND w = CreateWindowA("STATIC", "pbufferfront", WS_OVERLAPPEDWINDOW, 0, 0, 100, 100, 0, 0, 0, 0);
    HDC dc = GetDC(w), pdc;
    PIXELFORMATDESCRIPTOR pfd = { sizeof(pfd), 1, PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER, PFD_TYPE_RGBA, 24 };
    int fmts[8], pba[] = { 0 };
    int attr[] = { 0x202D /* WGL_DRAW_TO_PBUFFER_ARB */, 1, 0x2010 /* WGL_SUPPORT_OPENGL_ARB */, 1,
                   0x2011 /* WGL_DOUBLE_BUFFER_ARB */, 0, 0x2014 /* WGL_COLOR_BITS_ARB */, 24, 0 };
    UINT n = 0, i;
    unsigned char px[4] = { 0 };
    HGLRC rc, prc;
    HANDLE pb;
    PFNCHOOSE choose; PFNCREATEPB create; PFNGETPBDC getdc; PFNRELEASEPBDC releasedc; PFNDESTROYPB destroy;

    SetPixelFormat(dc, ChoosePixelFormat(dc, &pfd), &pfd);
    rc = wglCreateContext(dc); wglMakeCurrent(dc, rc);
    choose = (PFNCHOOSE)wglGetProcAddress("wglChoosePixelFormatARB");
    create = (PFNCREATEPB)wglGetProcAddress("wglCreatePbufferARB");
    getdc = (PFNGETPBDC)wglGetProcAddress("wglGetPbufferDCARB");
    releasedc = (PFNRELEASEPBDC)wglGetProcAddress("wglReleasePbufferDCARB");
    destroy = (PFNDESTROYPB)wglGetProcAddress("wglDestroyPbufferARB");
    if (!choose || !create || !getdc || !releasedc || !destroy || !choose(dc, attr, NULL, 8, fmts, &n) || !n)
    { printf("pbuffer=0\n"); return 0; }
    printf("pbuffer=1\n");
    for (i = 0; i < 2; i++)   /* twice: the second pbuffer after the first one is gone */
    {
        if (!(pb = create(dc, fmts[0], 1, 1, pba)) || !(pdc = getdc(pb))) { printf("made=0\n"); return 1; }
        prc = wglCreateContext(pdc);
        wglMakeCurrent(pdc, prc);
        printf("made=1\n"); fflush(stdout);
        glDrawBuffer(GL_FRONT);
        glClearColor(1, 0, 0, 1); glClear(GL_COLOR_BUFFER_BIT);
        glFinish();
        printf("front=1\n"); fflush(stdout);
        glReadBuffer(GL_FRONT);
        glReadPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, px);
        printf("readback=%d (%u,%u,%u)\n", px[0] > 200 && px[1] < 50 && px[2] < 50, px[0], px[1], px[2]);
        wglMakeCurrent(NULL, NULL); wglDeleteContext(prc);
        releasedc(pb, pdc); destroy(pb);
    }
    printf("done=1\n");
    return 0;
}
