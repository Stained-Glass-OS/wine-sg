/* wglGetDefaultProcAddress (patch 3580) */
#include <windows.h>
#include <stdio.h>
int main(void)
{
    HMODULE gl = LoadLibraryA("opengl32.dll");
    PROC (WINAPI *def)(LPCSTR);
    int f = 0;
    if (!gl) { puts("FAIL load\nRESULT: FAIL"); return 1; }
    def = (void *)GetProcAddress(gl, "wglGetDefaultProcAddress");
    if (!def) { puts("FAIL export\nRESULT: FAIL"); return 1; }
    if (def("glBegin") != GetProcAddress(gl, "glBegin") || !def("glBegin")) { puts("FAIL glBegin"); f++; }
    if (def("glVertex3f") != GetProcAddress(gl, "glVertex3f") || !def("glVertex3f")) { puts("FAIL glVertex3f"); f++; }
    if (def("wglCreateContext")) { puts("FAIL non-gl name"); f++; }
    if (def("glNoSuchFunction")) { puts("FAIL unknown"); f++; }
    if (def(NULL)) { puts("FAIL null"); f++; }
    puts(f ? "RESULT: FAIL" : "RESULT: PASS");
    return f;
}
