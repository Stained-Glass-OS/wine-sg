/* d3drm (patches/sg/2656): the search path and the default texture colours and shades of IDirect3DRM (and 2, 3).
 * Vtable slots are called by index (IDirect3DRM: SetSearchPath 25; IDirect3DRM3: 25). */
#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

static int failures;

static void check(int ok, const char *fmt, ...)
{
    char buf[256];
    va_list args;

    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    printf("%s  %s\n", ok ? "PASS" : "FAIL", buf);
    if (!ok) failures++;
}

#define DDERR(n) ((HRESULT)(0x88760000 | (n)))
#define BADVALUE DDERR(790)

typedef HRESULT (WINAPI *PFN_CREATE)(void **);
typedef HRESULT (STDMETHODCALLTYPE *f_str)(void *, const char *);
typedef HRESULT (STDMETHODCALLTYPE *f_get)(void *, DWORD *, char *);
typedef HRESULT (STDMETHODCALLTYPE *f_dword)(void *, DWORD);
typedef HRESULT (STDMETHODCALLTYPE *f_qi)(void *, REFIID, void **);

#define SLOT(obj, i) ((*(void ***)(obj))[i])

static const GUID IID_Drm3 = { 0x4516ec83, 0x8f20, 0x11d0, { 0x9b, 0x6d, 0x00, 0x00, 0xc0, 0x78, 0x1b, 0xc3 } };

static void check_path(void *drm, int base, const char *expect, const char *what)
{
    DWORD size = 0;
    char buf[64];
    HRESULT hr;

    memset(buf, 0x7f, sizeof(buf));
    hr = ((f_get)SLOT(drm, base + 2))(drm, &size, NULL);
    check(hr == S_OK && size == strlen(expect) + 1, "%s: the size is %lu (hr %#lx, expected %lu)", what, size, hr, (unsigned long)strlen(expect) + 1);
    size = sizeof(buf);
    hr = ((f_get)SLOT(drm, base + 2))(drm, &size, buf);
    check(hr == S_OK && !strcmp(buf, expect), "%s: '%s' (hr %#lx, expected '%s')", what, buf, hr, expect);
}

int main(void)
{
    HMODULE mod = LoadLibraryA("d3drm.dll");
    PFN_CREATE create = mod ? (PFN_CREATE)GetProcAddress(mod, "Direct3DRMCreate") : NULL;
    void *drm1 = NULL, *drm3 = NULL;
    DWORD size;
    char buf[64];
    HRESULT hr;

    if (!create || FAILED(create(&drm1))) { printf("FAIL  no Direct3DRM\nRESULT: FAIL\n"); return 1; }
    if (FAILED(((f_qi)SLOT(drm1, 0))(drm1, &IID_Drm3, &drm3))) { printf("FAIL  no IDirect3DRM3\nRESULT: FAIL\n"); return 1; }

    check_path(drm1, 25, "", "initially");
    check(((f_str)SLOT(drm1, 25))(drm1, "c:\\a") == S_OK, "SetSearchPath");
    check_path(drm1, 25, "c:\\a", "after Set");
    check(((f_str)SLOT(drm1, 26))(drm1, "d:\\b") == S_OK, "AddSearchPath");
    check_path(drm1, 25, "c:\\a;d:\\b", "after Add");
    check_path(drm3, 25, "c:\\a;d:\\b", "through IDirect3DRM3");
    check(((f_str)SLOT(drm3, 25))(drm3, "e:\\c") == S_OK, "IDirect3DRM3::SetSearchPath");
    check_path(drm1, 25, "e:\\c", "Set through 3, read through 1");

    size = 3;
    memset(buf, 0x7f, sizeof(buf));
    hr = ((f_get)SLOT(drm1, 27))(drm1, &size, buf);
    check(hr == BADVALUE && size == 5, "a small buffer: hr %#lx, size %lu", hr, size);
    check(((f_str)SLOT(drm1, 25))(drm1, NULL) == BADVALUE, "SetSearchPath(NULL)");
    check(((f_str)SLOT(drm1, 26))(drm1, NULL) == BADVALUE, "AddSearchPath(NULL)");
    check(((f_get)SLOT(drm1, 27))(drm1, NULL, buf) == BADVALUE, "GetSearchPath(NULL size)");

    check(((f_dword)SLOT(drm1, 28))(drm1, 16) == S_OK, "SetDefaultTextureColors(16)");
    check(((f_dword)SLOT(drm1, 28))(drm1, 0) == BADVALUE, "SetDefaultTextureColors(0)");
    check(((f_dword)SLOT(drm3, 28))(drm3, 256) == S_OK, "IDirect3DRM3::SetDefaultTextureColors(256)");
    check(((f_dword)SLOT(drm1, 29))(drm1, 32) == S_OK, "SetDefaultTextureShades(32)");
    check(((f_dword)SLOT(drm3, 29))(drm3, 0) == BADVALUE, "IDirect3DRM3::SetDefaultTextureShades(0)");

    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
