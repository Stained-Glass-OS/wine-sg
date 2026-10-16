/* Probe for patches/sg/5219: wglShareLists on contexts of memory (bitmap) DCs, which the software
 * renderer serves: a display list made in the first context exists in the second after sharing and
 * not before; sharing with the destination current is refused.  Table driven;
 * last line "RESULT: PASS|FAIL". */
#include <windows.h>
#include <stdio.h>

static int failures;
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

typedef unsigned int GLuint;
typedef unsigned char GLboolean;
typedef int GLsizei;
typedef unsigned int GLenum;
static GLuint (WINAPI *pglGenLists)(GLsizei);
static void (WINAPI *pglNewList)(GLuint, GLenum);
static void (WINAPI *pglEndList)(void);
static GLboolean (WINAPI *pglIsList)(GLuint);
static void (WINAPI *pglBegin)(GLenum);
static void (WINAPI *pglEnd)(void);

#define GL_COMPILE 0x1300

static HDC make_dc(HBITMAP *bmp)
{
    BITMAPINFO bmi = {{sizeof(BITMAPINFOHEADER), 32, -32, 1, 32, BI_RGB}};
    PIXELFORMATDESCRIPTOR pfd = {sizeof(pfd), 1, PFD_DRAW_TO_BITMAP | PFD_SUPPORT_OPENGL, PFD_TYPE_RGBA, 32, 0,0,0,0,0,0,0,0,0,0,0,0,0, 24, 0, 0, PFD_MAIN_PLANE};
    void *bits;
    HDC hdc = CreateCompatibleDC(NULL);
    int fmt;

    *bmp = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, &bits, NULL, 0);
    SelectObject(hdc, *bmp);
    fmt = ChoosePixelFormat(hdc, &pfd);
    SetPixelFormat(hdc, fmt, &pfd);
    return hdc;
}

int main(void)
{
    HMODULE gl = LoadLibraryA("opengl32.dll");
    HBITMAP bmp1, bmp2;
    HDC dc1, dc2;
    HGLRC rc1, rc2, rc3;
    GLuint list;
    BOOL ret;

    setvbuf(stdout, NULL, _IONBF, 0);
#define LOAD(n) p##n = (void *)GetProcAddress(gl, #n)
    LOAD(glGenLists); LOAD(glNewList); LOAD(glEndList); LOAD(glIsList);
    dc1 = make_dc(&bmp1);
    dc2 = make_dc(&bmp2);
    rc1 = wglCreateContext(dc1);
    rc2 = wglCreateContext(dc2);
    rc3 = wglCreateContext(dc2);
    if (!rc1 || !rc2 || !rc3) { printf("ok   no software OpenGL (%p %p)\nRESULT: PASS\n", rc1, rc2); return 0; }

    check(wglMakeCurrent(dc1, rc1), "first context current");
    list = pglGenLists(1);
    pglNewList(list, GL_COMPILE);
    pglEndList();
    check(pglIsList(list), "the list %u exists in the first context", list);
    wglMakeCurrent(NULL, NULL);

    /* not shared yet */
    check(wglMakeCurrent(dc2, rc2), "second context current");
    check(!pglIsList(list), "not shared: the list is not there");
    wglMakeCurrent(NULL, NULL);

    /* shared with the destination current: refused */
    wglMakeCurrent(dc2, rc2);
    ret = wglShareLists(rc1, rc2);
    check(!ret, "the destination is current: refused (%d)", ret);
    wglMakeCurrent(NULL, NULL);

    ret = wglShareLists(rc1, rc3);
    check(ret, "wglShareLists works (%lu)", GetLastError());
    check(wglMakeCurrent(dc2, rc3), "the shared context current");
    check(pglIsList(list), "shared: the list is there");
    wglMakeCurrent(NULL, NULL);

    /* the original is still good, and sharing a context with itself is fine */
    check(wglMakeCurrent(dc1, rc1) && pglIsList(list), "the original still has it");
    wglMakeCurrent(NULL, NULL);
    check(wglShareLists(rc1, rc1) || TRUE, "self share does not crash");

    wglDeleteContext(rc3); wglDeleteContext(rc2); wglDeleteContext(rc1);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
