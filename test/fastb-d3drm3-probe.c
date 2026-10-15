/* d3drm, fast mode round 2 (patch 3594): frame texture topology, material and material override. */
#define COBJMACROS
#include <initguid.h>
#include <windows.h>
#include <d3drm.h>
#include <stdio.h>
#include <string.h>

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

/* a 2x2 24 bit DIB, rows padded to 4 bytes */
static const unsigned char dib[] =
{
    40,0,0,0, 2,0,0,0, 2,0,0,0, 1,0, 24,0, 0,0,0,0, 16,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
    0,0,255, 0,0,255, 0,0,  0,0,255, 0,0,255, 0,0
};

static void test_resource(IDirect3DRM3 *d3drm3)
{
    char path[MAX_PATH], copy[MAX_PATH];
    IDirect3DRMTexture3 *tex = NULL;
    D3DRMIMAGE *image;
    HMODULE module;
    HANDLE update;
    HRESULT hr;

    GetModuleFileNameA(NULL, path, sizeof(path));
    strcpy(copy, path);
    strcpy(copy + strlen(copy) - 4, "-res.exe");
    CopyFileA(path, copy, FALSE);
    update = BeginUpdateResourceA(copy, FALSE);
    check(update && UpdateResourceA(update, (LPCSTR)RT_BITMAP, "TEX", MAKELANGID(0, 0), (void *)dib, sizeof(dib)) &&
            EndUpdateResourceA(update, FALSE), "resource added to a copy of the test program");
    module = LoadLibraryExA(copy, NULL, LOAD_LIBRARY_AS_DATAFILE);
    check(module != NULL, "copy loaded as data");
    if (!module)
        return;

    hr = IDirect3DRM3_LoadTextureFromResource(d3drm3, module, "TEX", (const char *)RT_BITMAP, &tex);
    check(hr == D3DRM_OK && tex, "LoadTextureFromResource with a bitmap resource");
    image = tex ? IDirect3DRMTexture3_GetImage(tex) : NULL;
    check(image && image->width == 2 && image->height == 2, "the texture has the image of the resource");
    if (tex) IDirect3DRMTexture3_Release(tex);

    tex = (void *)0xdeadbeef;
    hr = IDirect3DRM3_LoadTextureFromResource(d3drm3, module, "NOPE", (const char *)RT_BITMAP, &tex);
    check(hr == D3DRMERR_BADOBJECT && !tex, "LoadTextureFromResource with a missing resource");
    FreeLibrary(module);
    DeleteFileA(copy);
}

int main(void)
{
    HRESULT (WINAPI *create)(IDirect3DRM **);
    IDirect3DRM *d3drm = NULL;
    IDirect3DRM3 *d3drm3 = NULL;
    IDirect3DRMFrame *frame1 = NULL;
    IDirect3DRMFrame2 *frame2 = NULL;
    IDirect3DRMFrame3 *frame3 = NULL;
    IDirect3DRMMaterial2 *mat = NULL, *got = NULL;
    IDirect3DRMMaterial *got1 = NULL;
    D3DRMMATERIALOVERRIDE ov, ov2;
    BOOL u = 7, v = 7;
    HRESULT hr;

    create = (void *)GetProcAddress(LoadLibraryA("d3drm.dll"), "Direct3DRMCreate");
    if (!create || FAILED(create(&d3drm))) { printf("FAIL  no d3drm\nRESULT: FAIL\n"); return 1; }
    IDirect3DRM_QueryInterface(d3drm, &IID_IDirect3DRM3, (void **)&d3drm3);
    IDirect3DRM_CreateFrame(d3drm, NULL, &frame1);
    IDirect3DRMFrame_QueryInterface(frame1, &IID_IDirect3DRMFrame2, (void **)&frame2);
    IDirect3DRMFrame_QueryInterface(frame1, &IID_IDirect3DRMFrame3, (void **)&frame3);

    hr = IDirect3DRMFrame_GetTextureTopology(frame1, &u, &v);
    check(hr == D3DRM_OK && !u && !v, "texture topology starts unwrapped");
    hr = IDirect3DRMFrame_SetTextureTopology(frame1, TRUE, FALSE);
    check(hr == D3DRM_OK, "SetTextureTopology");
    hr = IDirect3DRMFrame2_GetTextureTopology(frame2, &u, &v);
    check(hr == D3DRM_OK && u == TRUE && v == FALSE, "GetTextureTopology through the other interface");
    hr = IDirect3DRMFrame_GetTextureTopology(frame1, NULL, &v);
    check(hr == D3DRMERR_BADVALUE, "GetTextureTopology rejects NULL");

    hr = IDirect3DRMFrame3_GetMaterial(frame3, &got);
    check(hr == D3DRM_OK && !got, "no material to begin with");
    IDirect3DRM3_CreateMaterial(d3drm3, 5.0f, &mat);
    hr = IDirect3DRMFrame3_SetMaterial(frame3, mat);
    check(hr == D3DRM_OK, "SetMaterial");
    hr = IDirect3DRMFrame3_GetMaterial(frame3, &got);
    check(hr == D3DRM_OK && got == mat, "GetMaterial returns the material");
    if (got) IDirect3DRMMaterial2_Release(got);
    hr = IDirect3DRMFrame2_GetMaterial(frame2, &got1);
    check(hr == D3DRM_OK && got1, "the older frame interface sees the material");
    if (got1) IDirect3DRMMaterial_Release(got1);
    hr = IDirect3DRMFrame3_SetMaterial(frame3, NULL);
    check(hr == D3DRM_OK, "SetMaterial(NULL)");
    hr = IDirect3DRMFrame3_GetMaterial(frame3, &got);
    check(hr == D3DRM_OK && !got, "the material is cleared");

    memset(&ov, 0, sizeof(ov));
    ov.dwSize = sizeof(ov);
    ov.dwFlags = D3DRMMATERIALOVERRIDE_DIFFUSE | D3DRMMATERIALOVERRIDE_POWER;
    ov.dcDiffuse.r = 0.25f; ov.dcDiffuse.g = 0.5f; ov.dcDiffuse.b = 0.75f; ov.dcDiffuse.a = 1.0f;
    ov.dvPower = 12.0f;
    hr = IDirect3DRMFrame3_SetMaterialOverride(frame3, &ov);
    check(hr == D3DRM_OK, "SetMaterialOverride");
    memset(&ov2, 0, sizeof(ov2));
    hr = IDirect3DRMFrame3_GetMaterialOverride(frame3, &ov2);
    check(hr == D3DRM_OK && ov2.dwFlags == ov.dwFlags && ov2.dcDiffuse.g == 0.5f && ov2.dvPower == 12.0f, "GetMaterialOverride returns what was set");
    check(IDirect3DRMFrame3_SetMaterialOverride(frame3, NULL) == D3DRMERR_BADVALUE, "SetMaterialOverride rejects NULL");

    test_resource(d3drm3);

    IDirect3DRMMaterial2_Release(mat);
    IDirect3DRMFrame3_Release(frame3);
    IDirect3DRMFrame2_Release(frame2);
    IDirect3DRMFrame_Release(frame1);
    IDirect3DRM3_Release(d3drm3);
    IDirect3DRM_Release(d3drm);
    printf("%d failures\n", failures);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
