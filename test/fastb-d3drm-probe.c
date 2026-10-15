/* d3drm, fast mode (patch 3507): mesh builder geometry (Translate, GetBox,
 * SetVertex/SetNormal(s), GetVertex/GetNormal, colour source, perspective),
 * and a mesh's Scale, Translate, GetBox and group mapping/quality. */
#define COBJMACROS
#include <initguid.h>
#include <windows.h>
#include <d3drm.h>
#include <stdio.h>

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

static int nearf(float a, float b) { return a - b < 1e-4f && b - a < 1e-4f; }

int main(void)
{
    HRESULT (WINAPI *create)(IDirect3DRM **);
    IDirect3DRM *d3drm = NULL;
    IDirect3DRM3 *d3drm3 = NULL;
    IDirect3DRMMeshBuilder3 *builder = NULL;
    IDirect3DRMMesh *mesh = NULL;
    D3DRMBOX box;
    D3DVECTOR v, verts[3], normals[1];
    unsigned int mesh_face[] = { 0, 1, 2 };
    D3DRMGROUPINDEX group;
    HRESULT hr;

    memset(verts, 0, sizeof(verts));
    memset(normals, 0, sizeof(normals));
    verts[1].x = 2; verts[2].y = 4; normals[0].z = 1;
    create = (void *)GetProcAddress(LoadLibraryA("d3drm.dll"), "Direct3DRMCreate");
    if (!create || FAILED(create(&d3drm))) { printf("FAIL  no d3drm\n"); printf("RESULT: FAIL\n"); return 1; }
    IDirect3DRM_QueryInterface(d3drm, &IID_IDirect3DRM3, (void **)&d3drm3);
    hr = IDirect3DRM3_CreateMeshBuilder(d3drm3, &builder);
    check(hr == D3DRM_OK, "CreateMeshBuilder");

    IDirect3DRMMeshBuilder3_AddVertex(builder, verts[0].x, verts[0].y, verts[0].z);
    IDirect3DRMMeshBuilder3_AddVertex(builder, verts[1].x, verts[1].y, verts[1].z);
    hr = IDirect3DRMMeshBuilder3_AddVertex(builder, verts[2].x, verts[2].y, verts[2].z);
    check(hr == 2, "AddVertex returns the index");
    hr = IDirect3DRMMeshBuilder3_AddNormal(builder, normals[0].x, normals[0].y, normals[0].z);
    check(hr == 0, "AddNormal returns the index");
    IDirect3DRMMeshBuilder3_GetBox(builder, &box);
    check(nearf(box.min.x, 0) && nearf(box.max.x, 2) && nearf(box.max.y, 4), "GetBox");
    hr = builder->lpVtbl->Translate(builder, 1, 2, 3);
    check(hr == D3DRM_OK, "Translate");
    IDirect3DRMMeshBuilder3_GetVertex(builder, 1, &v);
    check(nearf(v.x, 3) && nearf(v.y, 2) && nearf(v.z, 3), "vertex moved");
    IDirect3DRMMeshBuilder3_GetBox(builder, &box);
    check(nearf(box.min.x, 1) && nearf(box.max.y, 6) && nearf(box.min.z, 3), "box follows");
    hr = IDirect3DRMMeshBuilder3_SetVertex(builder, 0, 9, 8, 7);
    IDirect3DRMMeshBuilder3_GetVertex(builder, 0, &v);
    check(hr == D3DRM_OK && nearf(v.x, 9) && nearf(v.y, 8) && nearf(v.z, 7), "SetVertex");
    hr = IDirect3DRMMeshBuilder3_SetVertex(builder, 3, 0, 0, 0);
    check(hr == D3DRMERR_BADVALUE, "SetVertex out of range");
    IDirect3DRMMeshBuilder3_SetNormal(builder, 0, 0, 1, 0);
    IDirect3DRMMeshBuilder3_GetNormal(builder, 0, &v);
    check(nearf(v.y, 1), "SetNormal / GetNormal");
    verts[0].x = 5; verts[1].x = 6; verts[2].x = 7;
    hr = IDirect3DRMMeshBuilder3_SetVertices(builder, 0, 3, verts);
    IDirect3DRMMeshBuilder3_GetVertex(builder, 2, &v);
    check(hr == D3DRM_OK && nearf(v.x, 7), "SetVertices");
    hr = IDirect3DRMMeshBuilder3_SetVertices(builder, 2, 3, verts);
    check(hr == D3DRMERR_BADVALUE, "SetVertices out of range");
    check(IDirect3DRMMeshBuilder3_GetColorSource(builder) == D3DRMCOLOR_FROMFACE, "colour from the face");
    hr = builder->lpVtbl->SetColorSource(builder, D3DRMCOLOR_FROMVERTEX);
    check(hr == D3DRM_OK && IDirect3DRMMeshBuilder3_GetColorSource(builder) == D3DRMCOLOR_FROMVERTEX, "colour from the vertex");
    IDirect3DRMMeshBuilder3_SetPerspective(builder, TRUE);
    check(IDirect3DRMMeshBuilder3_GetPerspective(builder), "perspective on");

    hr = IDirect3DRM3_CreateMesh(d3drm3, &mesh);
    check(hr == D3DRM_OK, "CreateMesh");
    hr = IDirect3DRMMesh_AddGroup(mesh, 3, 1, 3, mesh_face, &group);
    check(hr == D3DRM_OK, "AddGroup");
    {
        D3DRMVERTEX mv[3];
        memset(mv, 0, sizeof(mv));
        mv[0].position.x = 1; mv[0].position.y = 1; mv[0].position.z = 1;
        mv[1].position.x = 3; mv[1].position.y = 1; mv[1].position.z = 1;
        mv[2].position.x = 1; mv[2].position.y = 5; mv[2].position.z = 1;
        IDirect3DRMMesh_SetVertices(mesh, group, 0, 3, mv);
    }
    IDirect3DRMMesh_Scale(mesh, 2, 2, 2);
    IDirect3DRMMesh_Translate(mesh, 1, 0, 0);
    IDirect3DRMMesh_GetBox(mesh, &box);
    check(nearf(box.min.x, 3) && nearf(box.max.x, 7) && nearf(box.max.y, 10) && nearf(box.min.z, 2), "mesh box after Scale and Translate");
    hr = IDirect3DRMMesh_SetGroupMapping(mesh, group, D3DRMMAP_WRAPU);
    check(hr == D3DRM_OK && IDirect3DRMMesh_GetGroupMapping(mesh, group) == D3DRMMAP_WRAPU, "group mapping");
    hr = IDirect3DRMMesh_SetGroupQuality(mesh, group, D3DRMRENDER_FLAT);
    check(hr == D3DRM_OK && IDirect3DRMMesh_GetGroupQuality(mesh, group) == D3DRMRENDER_FLAT, "group quality");
    hr = IDirect3DRMMesh_SetGroupMapping(mesh, 5, 0);
    check(hr == D3DRMERR_BADVALUE, "group mapping index check");

    IDirect3DRMMesh_Release(mesh);
    IDirect3DRMMeshBuilder3_Release(builder);
    IDirect3DRM3_Release(d3drm3);
    IDirect3DRM_Release(d3drm);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
