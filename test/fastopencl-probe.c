#include <windows.h>
#include <stdio.h>

static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

int main(void)
{
    HMODULE mod = LoadLibraryA("opencl.dll");
    void *(WINAPI *create_buf)(void *, unsigned long long, unsigned, int *);
    int (WINAPI *acquire)(void *, unsigned, const void *, unsigned, const void *, void *);
    int (WINAPI *objinfo)(void *, void *, void *);
    void *(WINAPI *ext)(const char *);
    int err = 0;

    CHECK(mod != NULL, "load");
    if (!mod) return 1;
    create_buf = (void *)GetProcAddress(mod, "clCreateFromGLBuffer");
    acquire = (void *)GetProcAddress(mod, "clEnqueueAcquireGLObjects");
    objinfo = (void *)GetProcAddress(mod, "clGetGLObjectInfo");
    ext = (void *)GetProcAddress(mod, "clGetExtensionFunctionAddress");
    CHECK(create_buf && acquire && objinfo && ext, "exports");

    /* contexts are created without GL sharing */
    CHECK(create_buf(NULL, 0, 1, &err) == NULL && err == -34, "create from GL buffer %d", err);
    CHECK(create_buf(NULL, 0, 1, NULL) == NULL, "null errcode");
    CHECK(acquire(NULL, 0, NULL, 0, NULL, NULL) == -60, "acquire");
    CHECK(objinfo(NULL, NULL, NULL) == -60, "object info");
    CHECK(ext("clSomeExtension") == NULL, "extension");

    printf(fails ? "RESULT: FAIL (%d)\n" : "RESULT: PASS\n", fails);
    return fails != 0;
}
