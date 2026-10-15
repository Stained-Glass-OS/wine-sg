/* opengl32 pointer-getter probe: runs as 32-bit (wow64 thunks) and 64-bit */
#include <windows.h>
#include <GL/gl.h>
#include <stdio.h>
#include <string.h>
#include <GL/glext.h>

static int failed;
#define CHECK(c, ...) do { if (!(c)) { failed++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

#define GL_VERTEX_ARRAY_POINTER_EXT 0x808E
static float arr[12], arr2[12], arr3[12];

static int has_ext(const char *list, const char *name)
{
    size_t len = strlen(name);
    const char *p = list;
    while (p && (p = strstr(p, name)))
    {
        if ((p == list || p[-1] == ' ') && (p[len] == ' ' || !p[len])) return 1;
        p += len;
    }
    return 0;
}

int main(void)
{
    static const char *withheld[] = { "GL_ARB_buffer_storage", "GL_IBM_vertex_array_lists", "GL_INTEL_parallel_arrays",
        "GL_SUN_triangle_list", "GL_ATI_map_object_buffer", "GL_INTEL_map_texture", "GL_ARB_cl_event",
        "GL_EXT_x11_sync_object", "GL_NV_draw_vulkan_image" };
    PFNGLVERTEXATTRIBPOINTERPROC p_glVertexAttribPointer;
    PFNGLGETVERTEXATTRIBPOINTERVPROC p_glGetVertexAttribPointerv;
    PFNGLCLIENTACTIVETEXTUREPROC p_glClientActiveTexture;
    PFNGLGETPOINTERINDEXEDVEXTPROC p_getidx;
    PFNGLGETPOINTERI_VEXTPROC p_geti;
    PFNGLGETVERTEXARRAYPOINTERVEXTPROC p_getva;
    PFNGLGETVERTEXARRAYPOINTERI_VEXTPROC p_getvai;
    PFNGLGENVERTEXARRAYSPROC p_genva;
    PFNGLBINDVERTEXARRAYPROC p_bindva;
    WNDCLASSA wc = {0};
    HWND hwnd; HDC dc; HGLRC rc; PIXELFORMATDESCRIPTOR pfd = {0};
    const char *ext; int pf; unsigned i; void *p; GLuint vao = 0;
    int dsa;

    wc.lpfnWndProc = DefWindowProcA; wc.lpszClassName = "oglprobe"; wc.hInstance = GetModuleHandleA(NULL);
    RegisterClassA(&wc);
    hwnd = CreateWindowA("oglprobe", "p", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 0, 0, 64, 64, 0, 0, wc.hInstance, 0);
    dc = GetDC(hwnd);
    pfd.nSize = sizeof(pfd); pfd.nVersion = 1; pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA; pfd.cColorBits = 32; pfd.cDepthBits = 16;
    pf = ChoosePixelFormat(dc, &pfd); SetPixelFormat(dc, pf, &pfd);
    rc = wglCreateContext(dc); wglMakeCurrent(dc, rc);
    ext = (const char *)glGetString(GL_EXTENSIONS);
    CHECK(ext != NULL, "no extensions");
    if (!ext) return 1;

    if (sizeof(void *) == 4)
        for (i = 0; i < sizeof(withheld) / sizeof(*withheld); i++)
            CHECK(!has_ext(ext, withheld[i]), "%s advertised to a 32-bit client", withheld[i]);

    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(3, GL_FLOAT, 12, arr);
    p = (void *)0x1234;
    glGetPointerv(GL_VERTEX_ARRAY_POINTER, &p);
    CHECK(p == arr, "glGetPointerv %p != %p", p, (void *)arr);

    if (has_ext(ext, "GL_EXT_vertex_array"))
    {
        PFNGLGETPOINTERVEXTPROC p_pv = (void *)wglGetProcAddress("glGetPointervEXT");
        if (p_pv)
        {
            p = (void *)0x1234;
            p_pv(GL_VERTEX_ARRAY_POINTER_EXT, &p);
            CHECK(p == arr, "glGetPointervEXT %p != %p", p, (void *)arr);
        }
        else printf("SKIP glGetPointervEXT\n");
    }

    p_glVertexAttribPointer = (void *)wglGetProcAddress("glVertexAttribPointer");
    p_glGetVertexAttribPointerv = (void *)wglGetProcAddress("glGetVertexAttribPointerv");
    if (p_glVertexAttribPointer && p_glGetVertexAttribPointerv)
    {
        p_glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 0, arr3);
        p = (void *)0x1234;
        p_glGetVertexAttribPointerv(1, GL_VERTEX_ATTRIB_ARRAY_POINTER, &p);
        CHECK(p == arr3, "glGetVertexAttribPointerv %p != %p", p, (void *)arr3);
    }
    else printf("SKIP glGetVertexAttribPointerv\n");

    p_glClientActiveTexture = (void *)wglGetProcAddress("glClientActiveTexture");
    dsa = has_ext(ext, "GL_EXT_direct_state_access");
    printf("DSA %d\n", dsa);
    if (p_glClientActiveTexture)
    {
        p_glClientActiveTexture(GL_TEXTURE1);
        glEnableClientState(GL_TEXTURE_COORD_ARRAY);
        glTexCoordPointer(2, GL_FLOAT, 0, arr2);
        p_glClientActiveTexture(GL_TEXTURE0);
    }
    if (dsa)
    {
        p_getidx = (void *)wglGetProcAddress("glGetPointerIndexedvEXT");
        p_geti = (void *)wglGetProcAddress("glGetPointeri_vEXT");
        p_getva = (void *)wglGetProcAddress("glGetVertexArrayPointervEXT");
        p_getvai = (void *)wglGetProcAddress("glGetVertexArrayPointeri_vEXT");
        if (p_getidx)
        {
            p = (void *)0x1234;
            p_getidx(GL_TEXTURE_COORD_ARRAY_POINTER, 1, &p);
            CHECK(p == arr2, "glGetPointerIndexedvEXT %p != %p", p, (void *)arr2);
            printf("exercised glGetPointerIndexedvEXT\n");
        }
        else printf("SKIP glGetPointerIndexedvEXT\n");
        if (p_geti)
        {
            p = (void *)0x1234;
            p_geti(GL_TEXTURE_COORD_ARRAY_POINTER, 1, &p);
            CHECK(p == arr2, "glGetPointeri_vEXT %p != %p", p, (void *)arr2);
            printf("exercised glGetPointeri_vEXT\n");
        }
        else printf("SKIP glGetPointeri_vEXT\n");
        p_genva = (void *)wglGetProcAddress("glGenVertexArrays");
        p_bindva = (void *)wglGetProcAddress("glBindVertexArray");
        if (p_genva && p_bindva && (p_getva || p_getvai))
        {
            p_genva(1, &vao);
            p_bindva(vao);
            glVertexPointer(3, GL_FLOAT, 12, arr3);
            if (p_getva)
            {
                p = (void *)0x1234;
                p_getva(vao, GL_VERTEX_ARRAY_POINTER, &p);
                CHECK(p == arr3, "glGetVertexArrayPointervEXT %p != %p", p, (void *)arr3);
                printf("exercised glGetVertexArrayPointervEXT\n");
            }
            if (p_getvai)
            {
                p = (void *)0x1234;
                if (p_glVertexAttribPointer) p_glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, 0, arr2);
                p_getvai(vao, 2, GL_VERTEX_ATTRIB_ARRAY_POINTER, &p);
                CHECK(p == arr2, "glGetVertexArrayPointeri_vEXT %p != %p", p, (void *)arr2);
                printf("exercised glGetVertexArrayPointeri_vEXT\n");
                /* an invalid query leaves the destination alone, as natively */
                p = (void *)0x1234;
                p_getvai(vao, 0, GL_VERTEX_ARRAY_POINTER, &p);
                CHECK(p == (void *)0x1234, "invalid query wrote %p", p);
            }
            p_bindva(0);
        }
    }
    printf("RESULT: %s\n", failed ? "FAIL" : "PASS");
    return failed != 0;
}
