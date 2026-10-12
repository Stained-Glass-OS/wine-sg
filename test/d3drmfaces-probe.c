/* d3drm (patch 2680): the faces of a mesh builder: CreateFace adds a face, GetFaceCount counts the faces,
 * GetFaces and GetFace hand them out (IDirect3DRMFaceArray), faces loaded from X data come out as face objects
 * with their vertices and normals, and a face made before Load stays in front of the loaded ones. */
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

static int approx(float a, float b) { return fabsf(a - b) < 1e-4f; }

int main(void)
{
    HMODULE mod = LoadLibraryA("d3drm.dll");
    HRESULT (WINAPI *create)(IDirect3DRM **) = mod ? (void *)GetProcAddress(mod, "Direct3DRMCreate") : NULL;
    IDirect3DRM *drm1 = NULL;
    IDirect3DRM3 *drm = NULL;
    IDirect3DRMMeshBuilder3 *mb = NULL;
    IDirect3DRMFace2 *empty = NULL, *f2 = NULL;
    IDirect3DRMFace *f = NULL, *g = NULL;
    IDirect3DRMFaceArray *arr = NULL, *arr2 = NULL;
    D3DRMLOADMEMORY info;
    D3DVECTOR vs[4], ns[4], vv[4], nn[4];
    DWORD count;
    HRESULT hr;
    ULONG ref1, ref2;

    if (!create || FAILED(create(&drm1))) { printf("FAIL  no Direct3DRM\nRESULT: FAIL\n"); return 1; }
    IDirect3DRM_QueryInterface(drm1, &my_IID_Drm3, (void **)&drm);
    IDirect3DRM3_CreateMeshBuilder(drm, &mb);
    check(mb && IDirect3DRMMeshBuilder3_GetFaceCount(mb) == 0, "a new builder has no faces");
    hr = IDirect3DRMMeshBuilder3_GetFaces(mb, &arr);
    check(hr == S_OK && arr && IDirect3DRMFaceArray_GetSize(arr) == 0, "GetFaces of none: an empty array");
    if (arr) IDirect3DRMFaceArray_Release(arr);
    check(IDirect3DRMMeshBuilder3_GetFaces(mb, NULL) == BADVALUE, "GetFaces(NULL) is D3DRMERR_BADVALUE");
    check(IDirect3DRMMeshBuilder3_GetFace(mb, 0, &f2) == BADVALUE, "GetFace(0) of none is D3DRMERR_BADVALUE");

    hr = IDirect3DRMMeshBuilder3_CreateFace(mb, &empty);
    check(hr == S_OK && empty, "CreateFace");
    check(IDirect3DRMMeshBuilder3_GetFaceCount(mb) == 1, "the builder has one face");
    hr = IDirect3DRMMeshBuilder3_GetFace(mb, 0, &f2);
    check(hr == S_OK && f2 == empty, "GetFace(0) is the face made");
    if (f2) IDirect3DRMFace2_Release(f2);
    ref1 = IDirect3DRMFace2_AddRef(empty); IDirect3DRMFace2_Release(empty);
    hr = IDirect3DRMMeshBuilder3_GetFaces(mb, &arr);
    check(hr == S_OK && arr && IDirect3DRMFaceArray_GetSize(arr) == 1, "GetFaces: one face");
    ref2 = IDirect3DRMFace2_AddRef(empty); IDirect3DRMFace2_Release(empty);
    check(ref2 > ref1, "the array holds a reference to the face (%lu, %lu)", ref1, ref2);
    hr = IDirect3DRMFaceArray_GetElement(arr, 0, &f);
    check(hr == S_OK && f, "GetElement(0)");
    check(IDirect3DRMFaceArray_GetElement(arr, 1, &g) == BADVALUE && !g, "GetElement(1) is D3DRMERR_BADVALUE");
    if (f) IDirect3DRMFace_Release(f);
    IDirect3DRMFaceArray_Release(arr);
    ref2 = IDirect3DRMFace2_AddRef(empty); IDirect3DRMFace2_Release(empty);
    check(ref2 == ref1, "and gives it back");

    info.lpMemory = data;
    info.dSize = strlen(data);
    hr = IDirect3DRMMeshBuilder3_Load(mb, &info, NULL, D3DRMLOAD_FROMMEMORY, NULL, NULL);
    check(hr == S_OK, "Load");
    check(IDirect3DRMMeshBuilder3_GetFaceCount(mb) == 4, "the face made stays: 1 + 3 faces (%d)", IDirect3DRMMeshBuilder3_GetFaceCount(mb));
    count = 4;
    IDirect3DRMMeshBuilder3_GetVertices(mb, 0, &count, vs);
    count = 4;
    IDirect3DRMMeshBuilder3_GetNormals(mb, 0, &count, ns);
    check(IDirect3DRMMeshBuilder3_GetFace(mb, 0, &f2) == S_OK && f2 == empty, "face 0 is still the one made");
    if (f2) IDirect3DRMFace2_Release(f2);
    hr = IDirect3DRMMeshBuilder3_GetFaces(mb, &arr);
    check(hr == S_OK && arr && IDirect3DRMFaceArray_GetSize(arr) == 4, "GetFaces: four");
    hr = IDirect3DRMFaceArray_GetElement(arr, 2, &f);
    check(hr == S_OK && f, "element 2");
    if (f)
    {
        count = 3;
        hr = IDirect3DRMFace_GetVertices(f, &count, vv, nn);
        check(hr == S_OK && count == 3, "it has three vertices");
        /* the second loaded face: vertices 1, 2, 3 */
        check(approx(vv[0].x, vs[1].x) && approx(vv[0].y, vs[1].y) && approx(vv[1].z, vs[2].z) &&
              approx(vv[2].x, vs[3].x), "at the positions of the file's vertices 1, 2, 3");
        check(approx(nn[0].x, ns[1].x) && approx(nn[0].y, ns[1].y) && approx(nn[0].z, ns[1].z) &&
              approx(nn[2].x, ns[3].x) && approx(nn[2].z, ns[3].z), "and the builder's normals of them");
        IDirect3DRMFace_Release(f);
    }
    hr = IDirect3DRMMeshBuilder3_GetFace(mb, 3, &f2);
    check(hr == S_OK && f2, "GetFace(3)");
    if (f2) IDirect3DRMFace2_Release(f2);
    check(IDirect3DRMMeshBuilder3_GetFace(mb, 4, &f2) == BADVALUE, "GetFace(4) is D3DRMERR_BADVALUE");
    IDirect3DRMFaceArray_Release(arr);

    /* a face made after Load goes behind the loaded ones */
    IDirect3DRMMeshBuilder3_CreateFace(mb, &f2);
    check(IDirect3DRMMeshBuilder3_GetFaceCount(mb) == 5, "five faces");
    hr = IDirect3DRMMeshBuilder3_GetFaces(mb, &arr);
    hr = arr ? IDirect3DRMFaceArray_GetElement(arr, 4, &f) : E_FAIL;
    {
        IDirect3DRMFace *made = NULL;

        IDirect3DRMFace2_QueryInterface(f2, &IID_IDirect3DRMFace, (void **)&made);
        check(hr == S_OK && f == made, "and it is the last (face %p, made %p)", f, made);
        if (made) IDirect3DRMFace_Release(made);
    }
    if (f) IDirect3DRMFace_Release(f);
    if (arr) IDirect3DRMFaceArray_Release(arr);
    IDirect3DRMFace2_Release(f2);
    IDirect3DRMFace2_Release(empty);
    IDirect3DRMMeshBuilder3_Release(mb);
    IDirect3DRM3_Release(drm);
    IDirect3DRM_Release(drm1);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
