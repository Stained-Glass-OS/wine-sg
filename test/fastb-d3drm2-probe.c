/* d3drm, fast mode (patch 3536): mesh builder AddFaces / AddFace / AddMeshBuilder / GenerateNormals / vertex colours /
 * Empty, and a light's enable frame. */
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
/* Wine's header gives AddMeshBuilder a flags argument, mingw's does not: call through the table */
typedef HRESULT (STDMETHODCALLTYPE *add_builder_fn)(IDirect3DRMMeshBuilder3 *, IDirect3DRMMeshBuilder3 *, DWORD);
static HRESULT add_builder(IDirect3DRMMeshBuilder3 *to, IDirect3DRMMeshBuilder3 *from)
{
    return ((add_builder_fn)to->lpVtbl->AddMeshBuilder)(to, from, 0);
}

static int nearf(float a, float b) { return a - b < 1e-4f && b - a < 1e-4f; }

int main(void)
{
    HRESULT (WINAPI *create)(IDirect3DRM **);
    IDirect3DRM *d3drm = NULL;
    IDirect3DRM3 *d3drm3 = NULL;
    IDirect3DRMMeshBuilder3 *b = NULL, *b2 = NULL;
    IDirect3DRMFaceArray *faces = NULL;
    IDirect3DRMLight *light = NULL;
    IDirect3DRMFrame *frame = NULL, *got = NULL;
    D3DVECTOR verts[4], normals[2], out[16];
    DWORD data[] = { 3, 0, 0, 1, 0, 2, 0,  3, 0, 0, 3, 0, 2, 0,  0 };
    DWORD vcount, ncount, dsize, fdata[32];
    HRESULT hr;

    memset(verts, 0, sizeof(verts)); memset(normals, 0, sizeof(normals));
    verts[1].x = 1; verts[2].y = 1; verts[3].x = 1; verts[3].y = 1; normals[0].z = -1;
    create = (void *)GetProcAddress(LoadLibraryA("d3drm.dll"), "Direct3DRMCreate");
    if (!create || FAILED(create(&d3drm))) { printf("FAIL  no d3drm\nRESULT: FAIL\n"); return 1; }
    IDirect3DRM_QueryInterface(d3drm, &IID_IDirect3DRM3, (void **)&d3drm3);
    IDirect3DRM3_CreateMeshBuilder(d3drm3, &b);

    hr = IDirect3DRMMeshBuilder3_AddFaces(b, 4, verts, 1, normals, data, &faces);
    check(hr == D3DRM_OK, "AddFaces");
    check(faces && IDirect3DRMFaceArray_GetSize(faces) == 2, "AddFaces returns the two new faces");
    if (faces) IDirect3DRMFaceArray_Release(faces);
    vcount = 8; ncount = 2; dsize = 32;
    hr = IDirect3DRMMeshBuilder3_GetGeometry(b, &vcount, out, &ncount, normals, &dsize, fdata);
    check(hr == D3DRM_OK && vcount == 4 && ncount == 1 && dsize == 15, "geometry counts after AddFaces");

    data[0] = 3; data[1] = 9;   /* a vertex that is not there */
    hr = IDirect3DRMMeshBuilder3_AddFaces(b, 4, verts, 1, normals, data, NULL);
    check(hr == D3DRMERR_BADVALUE, "AddFaces rejects a bad index");
    check(IDirect3DRMMeshBuilder3_GetVertexCount(b) == 4, "nothing added by the failed call");

    hr = IDirect3DRMMeshBuilder3_GenerateNormals(b, 0.0f, 0);
    check(hr == D3DRM_OK, "GenerateNormals");
    vcount = 4; ncount = 8; dsize = 32;
    IDirect3DRMMeshBuilder3_GetGeometry(b, &vcount, out, &ncount, normals ? out + 4 : NULL, &dsize, fdata);
    check(ncount == 6, "a normal per face corner");
    check(nearf(out[4].z, 1.0f), "normals lie along z for a flat quad");

    check(IDirect3DRMMeshBuilder3_SetVertexColor(b, 1, 0xff102030) == D3DRM_OK, "SetVertexColor");
    check(IDirect3DRMMeshBuilder3_GetVertexColor(b, 1) == 0xff102030, "GetVertexColor");
    check(IDirect3DRMMeshBuilder3_SetVertexColor(b, 99, 0) == D3DRMERR_BADVALUE, "SetVertexColor out of range");
    IDirect3DRMMeshBuilder3_SetVertexColorRGB(b, 2, 1.0f, 0.0f, 0.0f);
    check((IDirect3DRMMeshBuilder3_GetVertexColor(b, 2) & 0xffffff) == 0xff0000, "SetVertexColorRGB");

    IDirect3DRM3_CreateMeshBuilder(d3drm3, &b2);
    hr = add_builder(b2, b);
    check(hr == D3DRM_OK && IDirect3DRMMeshBuilder3_GetVertexCount(b2) == 4, "AddMeshBuilder");
    hr = add_builder(b2, b);
    check(IDirect3DRMMeshBuilder3_GetVertexCount(b2) == 8, "AddMeshBuilder twice");
    {
        IDirect3DRMFaceArray *fa = NULL;
        IDirect3DRMMeshBuilder3_GetFaces(b2, &fa);
        check(fa && IDirect3DRMFaceArray_GetSize(fa) == 4, "faces of both copies");
        if (fa) IDirect3DRMFaceArray_Release(fa);
    }
    hr = IDirect3DRMMeshBuilder3_Empty(b2, 0);
    check(hr == D3DRM_OK && IDirect3DRMMeshBuilder3_GetVertexCount(b2) == 0, "Empty");

    hr = IDirect3DRM_CreateLightRGB(d3drm, D3DRMLIGHT_POINT, 1, 1, 1, &light);
    check(hr == D3DRM_OK, "CreateLightRGB");
    IDirect3DRM_CreateFrame(d3drm, NULL, &frame);
    hr = IDirect3DRMLight_SetEnableFrame(light, frame);
    check(hr == D3DRM_OK, "SetEnableFrame");
    hr = IDirect3DRMLight_GetEnableFrame(light, &got);
    check(hr == D3DRM_OK && got == frame, "GetEnableFrame returns it");
    if (got) IDirect3DRMFrame_Release(got);
    IDirect3DRMLight_SetEnableFrame(light, NULL);
    got = (void *)1;
    hr = IDirect3DRMLight_GetEnableFrame(light, &got);
    check(hr == D3DRM_OK && got == NULL, "cleared");

    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures ? 1 : 0;
}
