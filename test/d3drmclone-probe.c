/* d3drm (patch 2681): Clone of materials, lights, faces and mesh builders makes an independent copy. */
#define COBJMACROS
#include <windows.h>
#include <d3drm.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>

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

static const GUID my_IID_Drm3 = { 0x4516ec83, 0x8f20, 0x11d0, { 0x9b, 0x6d, 0x00, 0x00, 0xc0, 0x78, 0x1b, 0xc3 } };
#define BADVALUE ((HRESULT)0x88760316)
static char data[] =
"xof 0302txt 0064\nHeader Object\n{\n1; 0; 1;\n}\nMesh Object\n{\n4;\n1.0; 0.0; 0.0;,\n0.0; 1.0; 0.0;,\n0.0; 0.0; 1.0;,\n1.0; 1.0; 1.0;;\n"
"3;\n3; 0, 1, 2;,\n3; 1, 2, 3;,\n3; 3, 1, 2;;\n}\n";

int main(void)
{
    HMODULE mod = LoadLibraryA("d3drm.dll");
    HRESULT (WINAPI *create)(IDirect3DRM **) = mod ? (void *)GetProcAddress(mod, "Direct3DRMCreate") : NULL;
    IDirect3DRM *drm1 = NULL;
    IDirect3DRM3 *drm = NULL;
    IDirect3DRMMaterial2 *mat = NULL, *mat2 = NULL;
    IDirect3DRMLight *light = NULL, *light2 = NULL;
    IDirect3DRMFace2 *face = NULL, *face2 = NULL;
    IDirect3DRMMeshBuilder3 *mb = NULL, *mb2 = NULL;
    D3DRMLOADMEMORY info;
    D3DVECTOR v, n;
    IUnknown *unk;
    DWORD count;
    HRESULT hr;
    float r, g, b;

    if (!create || FAILED(create(&drm1))) { printf("FAIL  no Direct3DRM\nRESULT: FAIL\n"); return 1; }
    IDirect3DRM_QueryInterface(drm1, &my_IID_Drm3, (void **)&drm);

    IDirect3DRM3_CreateMaterial(drm, 7.5f, &mat);
    IDirect3DRMMaterial2_SetEmissive(mat, 0.1f, 0.2f, 0.3f);
    IDirect3DRMMaterial2_SetSpecular(mat, 0.4f, 0.5f, 0.6f);
    hr = IDirect3DRMMaterial2_Clone(mat, NULL, &IID_IDirect3DRMMaterial2, (void **)&mat2);
    check(hr == S_OK && mat2 && mat2 != mat, "material Clone");
    check(IDirect3DRMMaterial2_Clone(mat, (IUnknown *)mat, &IID_IDirect3DRMMaterial2, (void **)&unk) == CLASS_E_NOAGGREGATION, "aggregation is CLASS_E_NOAGGREGATION");
    check(IDirect3DRMMaterial2_Clone(mat, NULL, &IID_IDirect3DRMMaterial2, NULL) == BADVALUE, "no output is D3DRMERR_BADVALUE");
    if (mat2)
    {
        check(fabsf(IDirect3DRMMaterial2_GetPower(mat2) - 7.5f) < 1e-5f, "the power is copied");
        IDirect3DRMMaterial2_GetEmissive(mat2, &r, &g, &b);
        check(fabsf(r - 0.1f) < 1e-5f && fabsf(b - 0.3f) < 1e-5f, "and the emissive color");
        IDirect3DRMMaterial2_GetSpecular(mat2, &r, &g, &b);
        check(fabsf(g - 0.5f) < 1e-5f, "and the specular color");
        IDirect3DRMMaterial2_SetPower(mat2, 1.0f);
        check(fabsf(IDirect3DRMMaterial2_GetPower(mat) - 7.5f) < 1e-5f, "the copy is independent");
        IDirect3DRMMaterial2_Release(mat2);
    }
    hr = IDirect3DRMMaterial2_Clone(mat, NULL, &IID_IDirect3DRMObject, (void **)&unk);
    check(hr == S_OK, "Clone(IDirect3DRMObject)");
    if (hr == S_OK) IUnknown_Release(unk);
    IDirect3DRMMaterial2_Release(mat);

    IDirect3DRM3_CreateLightRGB(drm, D3DRMLIGHT_POINT, 0.2f, 0.4f, 0.6f, &light);
    IDirect3DRMLight_SetRange(light, 33.0f);
    hr = IDirect3DRMLight_Clone(light, NULL, &IID_IDirect3DRMLight, (void **)&light2);
    check(hr == S_OK && light2 && light2 != light, "light Clone");
    if (light2)
    {
        check(IDirect3DRMLight_GetType(light2) == D3DRMLIGHT_POINT && fabsf(IDirect3DRMLight_GetRange(light2) - 33.0f) < 1e-5f
              && IDirect3DRMLight_GetColor(light2) == IDirect3DRMLight_GetColor(light), "type, range and color are copied");
        IDirect3DRMLight_SetRange(light2, 1.0f);
        check(fabsf(IDirect3DRMLight_GetRange(light) - 33.0f) < 1e-5f, "the copy is independent");
        IDirect3DRMLight_Release(light2);
    }
    IDirect3DRMLight_Release(light);

    IDirect3DRM3_CreateFace(drm, &face);
    IDirect3DRMFace2_AddVertex(face, 0, 0, 0);
    IDirect3DRMFace2_AddVertex(face, 1, 0, 0);
    IDirect3DRMFace2_AddVertex(face, 0, 1, 0);
    IDirect3DRMFace2_SetColor(face, 0xff112233);
    hr = IDirect3DRMFace2_Clone(face, NULL, &IID_IDirect3DRMFace2, (void **)&face2);
    check(hr == S_OK && face2 && face2 != face, "face Clone");
    if (face2)
    {
        check(IDirect3DRMFace2_GetVertexCount(face2) == 3 && IDirect3DRMFace2_GetColor(face2) == 0xff112233, "vertices and color are copied");
        IDirect3DRMFace2_AddVertex(face2, 5, 5, 5);
        check(IDirect3DRMFace2_GetVertexCount(face) == 3, "the copy is independent");
        IDirect3DRMFace2_Release(face2);
    }
    IDirect3DRMFace2_Release(face);

    IDirect3DRM3_CreateMeshBuilder(drm, &mb);
    info.lpMemory = data; info.dSize = strlen(data);
    IDirect3DRMMeshBuilder3_CreateFace(mb, &face);
    IDirect3DRMMeshBuilder3_Load(mb, &info, NULL, D3DRMLOAD_FROMMEMORY, NULL, NULL);
    IDirect3DRMMeshBuilder3_SetColorRGB(mb, 0.5f, 0.25f, 1.0f);
    hr = IDirect3DRMMeshBuilder3_Clone(mb, NULL, &IID_IDirect3DRMMeshBuilder3, (void **)&mb2);
    check(hr == S_OK && mb2 && mb2 != mb, "mesh builder Clone");
    if (mb2)
    {
        check(IDirect3DRMMeshBuilder3_GetVertexCount(mb2) == 4 && IDirect3DRMMeshBuilder3_GetNormalCount(mb2) == 4, "vertices and normals are copied");
        check(IDirect3DRMMeshBuilder3_GetFaceCount(mb2) == IDirect3DRMMeshBuilder3_GetFaceCount(mb) &&
              IDirect3DRMMeshBuilder3_GetFaceCount(mb2) == 4, "and the faces (%d)", IDirect3DRMMeshBuilder3_GetFaceCount(mb2));
        IDirect3DRMMeshBuilder3_GetVertex(mb2, 2, &v);
        IDirect3DRMMeshBuilder3_GetNormal(mb2, 2, &n);
        check(v.z == 1.0f && IDirect3DRMMeshBuilder3_GetVertexColor(mb2, 0) == IDirect3DRMMeshBuilder3_GetVertexColor(mb, 0), "vertex data and color");
        IDirect3DRMMeshBuilder3_AddVertex(mb2, 9, 9, 9);
        check(IDirect3DRMMeshBuilder3_GetVertexCount(mb) == 4, "the copy is independent");
        count = IDirect3DRMMeshBuilder3_GetFace(mb2, 0, &face2);
        check(count == S_OK && face2 && face2 != face, "the face made is a copy of the face made");
        check(face2 && IDirect3DRMFace2_GetVertexCount(face2) == 0, "which still comes first, in front of the loaded faces");
        if (face2) IDirect3DRMFace2_Release(face2);
        IDirect3DRMMeshBuilder3_Release(mb2);
    }
    IDirect3DRMFace2_Release(face);
    IDirect3DRMMeshBuilder3_Release(mb);
    IDirect3DRM3_Release(drm);
    IDirect3DRM_Release(drm1);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
