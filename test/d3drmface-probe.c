/* d3drm (patches/sg/2661): IDirect3DRMFace and IDirect3DRMFace2 hold vertices, texture coordinates, a normal,
 * a texture topology, a texture and a material. */
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
static const GUID my_IID_Face = { 0xeb16cb07, 0xd271, 0x11ce, { 0xac, 0x48, 0x00, 0x00, 0xc0, 0x38, 0x25, 0xa1 } };
static const GUID my_IID_Material = { 0xeb16cb0b, 0xd271, 0x11ce, { 0xac, 0x48, 0x00, 0x00, 0xc0, 0x38, 0x25, 0xa1 } };
#define BADVALUE ((HRESULT)0x88760316)

typedef HRESULT (STDMETHODCALLTYPE *f_getmat)(void *, void **);
#define FACE2_GetMaterial(f, out) (((f_getmat)(*(void ***)(f))[25])((f), (void **)(out)))
static int approx(float a, float b) { return fabsf(a - b) < 1e-4f; }

int main(void)
{
    HMODULE mod = LoadLibraryA("d3drm.dll");
    HRESULT (WINAPI *create)(IDirect3DRM **) = mod ? (void *)GetProcAddress(mod, "Direct3DRMCreate") : NULL;
    IDirect3DRM *drm1 = NULL;
    IDirect3DRM3 *drm = NULL;
    IDirect3DRMFace2 *face = NULL;
    IDirect3DRMFace *face1 = NULL;
    IDirect3DRMMaterial2 *mat = NULL, *mat_out = NULL;
    IDirect3DRMMaterial *mat1 = NULL;
    D3DVECTOR p[3], n[3], v, nv;
    DWORD count;
    float u, w;
    BOOL wu, wv;
    HRESULT hr;
    ULONG before;

    if (!create || FAILED(create(&drm1))) { printf("FAIL  no Direct3DRM\nRESULT: FAIL\n"); return 1; }
    IDirect3DRM_QueryInterface(drm1, &my_IID_Drm3, (void **)&drm);
    hr = IDirect3DRM3_CreateFace(drm, &face);
    check(hr == S_OK && face, "CreateFace (%#lx)", hr);
    if (!face) goto done;

    check(IDirect3DRMFace2_GetVertexCount(face) == 0, "no vertices at first");
    IDirect3DRMFace2_AddVertex(face, 0, 0, 0);
    IDirect3DRMFace2_AddVertex(face, 0, 1, 0);
    IDirect3DRMFace2_AddVertex(face, 1, 0, 0);
    check(IDirect3DRMFace2_GetVertexCount(face) == 3, "three vertices");

    hr = IDirect3DRMFace2_GetVertex(face, 1, &v, &nv);
    check(hr == S_OK && approx(v.x, 0) && approx(v.y, 1) && approx(v.z, 0), "GetVertex 1 (%g %g %g)", v.x, v.y, v.z);
    check(approx(nv.x, 0) && approx(nv.y, 0) && approx(nv.z, -1), "its normal is the face's (%g %g %g)", nv.x, nv.y, nv.z);
    check(IDirect3DRMFace2_GetVertex(face, 3, &v, &nv) == BADVALUE, "GetVertex 3 is D3DRMERR_BADVALUE");

    hr = IDirect3DRMFace2_GetNormal(face, &v);
    check(hr == S_OK && approx(v.x, 0) && approx(v.y, 0) && approx(v.z, -1), "GetNormal (%g %g %g)", v.x, v.y, v.z);

    count = 0;
    hr = IDirect3DRMFace2_GetVertices(face, &count, NULL, NULL);
    check(hr == S_OK && count == 3, "GetVertices asks the count (%#lx, %lu)", hr, count);
    count = 2;
    hr = IDirect3DRMFace2_GetVertices(face, &count, p, n);
    check(hr == BADVALUE && count == 3, "a short buffer is D3DRMERR_BADVALUE and gives the count (%#lx, %lu)", hr, count);
    count = 3;
    hr = IDirect3DRMFace2_GetVertices(face, &count, p, n);
    check(hr == S_OK && approx(p[2].x, 1) && approx(p[2].y, 0) && approx(n[0].z, -1), "GetVertices (%#lx)", hr);

    hr = IDirect3DRMFace2_SetTextureCoordinates(face, 2, 0.25f, 0.75f);
    check(hr == S_OK, "SetTextureCoordinates");
    hr = IDirect3DRMFace2_GetTextureCoordinates(face, 2, &u, &w);
    check(hr == S_OK && approx(u, 0.25f) && approx(w, 0.75f), "GetTextureCoordinates (%g %g)", u, w);
    check(IDirect3DRMFace2_SetTextureCoordinates(face, 3, 0, 0) == BADVALUE, "texture coordinates of vertex 3 are D3DRMERR_BADVALUE");
    check(IDirect3DRMFace2_GetTextureCoordinates(face, 9, &u, &w) == BADVALUE, "so are those read");

    IDirect3DRMFace2_SetTextureTopology(face, TRUE, FALSE);
    hr = IDirect3DRMFace2_GetTextureTopology(face, &wu, &wv);
    check(hr == S_OK && wu == TRUE && wv == FALSE, "texture topology");

    check(IDirect3DRMFace2_GetVertexIndex(face, 1) == 1 && IDirect3DRMFace2_GetVertexIndex(face, 7) == -1, "vertex indices");

    /* material, through both interfaces */
    hr = IDirect3DRM3_CreateMaterial(drm, 5.0f, &mat);
    check(hr == S_OK && mat, "CreateMaterial");
    before = IDirect3DRMMaterial2_AddRef(mat); IDirect3DRMMaterial2_Release(mat);
    hr = IDirect3DRMFace2_SetMaterial(face, mat);
    check(hr == S_OK, "SetMaterial");
    {
        ULONG after = IDirect3DRMMaterial2_AddRef(mat); IDirect3DRMMaterial2_Release(mat);
        check(after == before + 1, "the face holds the material (%lu -> %lu)", before, after);
    }
    hr = FACE2_GetMaterial(face, &mat_out);
    check(hr == S_OK && mat_out == mat, "GetMaterial returns it (%#lx %p %p)", hr, mat_out, mat);
    if (mat_out) IDirect3DRMMaterial2_Release(mat_out);
    IDirect3DRMFace2_SetMaterial(face, NULL);
    {
        ULONG after = IDirect3DRMMaterial2_AddRef(mat); IDirect3DRMMaterial2_Release(mat);
        check(after == before, "SetMaterial(NULL) lets it go (%lu)", after);
    }
    hr = IDirect3DRMFace2_QueryInterface(face, &my_IID_Face, (void **)&face1);
    check(hr == S_OK && face1, "IDirect3DRMFace");
    if (face1)
    {
        IDirect3DRMMaterial2_QueryInterface(mat, &my_IID_Material, (void **)&mat1);
        hr = IDirect3DRMFace_SetMaterial(face1, mat1);
        check(hr == S_OK, "IDirect3DRMFace::SetMaterial");
        mat_out = NULL;
        hr = FACE2_GetMaterial(face, &mat_out);
        check(hr == S_OK && mat_out, "it is the face's material for the second interface");
        if (mat_out) IDirect3DRMMaterial2_Release(mat_out);
        if (mat1) IDirect3DRMMaterial_Release(mat1);
        IDirect3DRMFace_Release(face1);
    }
    IDirect3DRMMaterial2_Release(mat);

done:
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
