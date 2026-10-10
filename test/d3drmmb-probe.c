/* d3drm (patches/sg/2670): the vertex and normal accessors, Translate, GetBox, ReserveSpace, the perspective flag
 * and the color source of a mesh builder (versions 2 and 3). */
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

static int approx(float a, float b) { return fabsf(a - b) < 1e-4f; }
static int vec_is(const D3DVECTOR *v, float x, float y, float z) { return approx(v->x, x) && approx(v->y, y) && approx(v->z, z); }

int main(void)
{
    HMODULE mod = LoadLibraryA("d3drm.dll");
    HRESULT (WINAPI *create)(IDirect3DRM **) = mod ? (void *)GetProcAddress(mod, "Direct3DRMCreate") : NULL;
    IDirect3DRM *drm1 = NULL;
    IDirect3DRM3 *drm = NULL;
    IDirect3DRMMeshBuilder3 *mb = NULL;
    D3DVECTOR v, vs[2], vo[8], no[8];
    D3DRMBOX box;
    DWORD vc, nc, fc;
    HRESULT hr;

    if (!create || FAILED(create(&drm1))) { printf("FAIL  no Direct3DRM\nRESULT: FAIL\n"); return 1; }
    IDirect3DRM_QueryInterface(drm1, &my_IID_Drm3, (void **)&drm);
    hr = IDirect3DRM3_CreateMeshBuilder(drm, &mb);
    check(hr == S_OK && mb, "CreateMeshBuilder (%#lx)", hr);
    if (!mb) goto done;

    IDirect3DRMMeshBuilder3_AddVertex(mb, 0, 0, 0);
    IDirect3DRMMeshBuilder3_AddVertex(mb, 1, 2, 3);
    IDirect3DRMMeshBuilder3_AddVertex(mb, -1, 5, 0);
    IDirect3DRMMeshBuilder3_AddNormal(mb, 0, 0, 1);
    check(IDirect3DRMMeshBuilder3_GetVertexCount(mb) == 3, "three vertices");

    hr = IDirect3DRMMeshBuilder3_GetVertex(mb, 1, &v);
    check(hr == S_OK && vec_is(&v, 1, 2, 3), "GetVertex");
    check(IDirect3DRMMeshBuilder3_GetVertex(mb, 3, &v) == BADVALUE, "GetVertex 3 is D3DRMERR_BADVALUE");
    check(IDirect3DRMMeshBuilder3_SetVertex(mb, 1, 7, 8, 9) == S_OK, "SetVertex");
    IDirect3DRMMeshBuilder3_GetVertex(mb, 1, &v);
    check(vec_is(&v, 7, 8, 9), "the vertex changed");
    check(IDirect3DRMMeshBuilder3_SetVertex(mb, 5, 0, 0, 0) == BADVALUE, "SetVertex 5 is D3DRMERR_BADVALUE");

    vs[0].x = 10; vs[0].y = 0; vs[0].z = 0;
    vs[1].x = 20; vs[1].y = 1; vs[1].z = 1;
    check(IDirect3DRMMeshBuilder3_SetVertices(mb, 1, 2, vs) == S_OK, "SetVertices");
    IDirect3DRMMeshBuilder3_GetVertex(mb, 2, &v);
    check(vec_is(&v, 20, 1, 1), "the second one changed");
    check(IDirect3DRMMeshBuilder3_SetVertices(mb, 2, 2, vs) == BADVALUE, "SetVertices past the end is D3DRMERR_BADVALUE");

    hr = IDirect3DRMMeshBuilder3_GetNormal(mb, 0, &v);
    check(hr == S_OK && vec_is(&v, 0, 0, 1), "GetNormal");
    check(IDirect3DRMMeshBuilder3_SetNormal(mb, 0, 1, 0, 0) == S_OK, "SetNormal");
    IDirect3DRMMeshBuilder3_GetNormal(mb, 0, &v);
    check(vec_is(&v, 1, 0, 0), "the normal changed");
    check(IDirect3DRMMeshBuilder3_SetNormal(mb, 1, 0, 0, 0) == BADVALUE, "SetNormal 1 is D3DRMERR_BADVALUE");

    hr = mb->lpVtbl->Translate(mb, 1, 1, 1);
    check(hr == S_OK, "Translate");
    IDirect3DRMMeshBuilder3_GetVertex(mb, 0, &v);
    check(vec_is(&v, 1, 1, 1), "vertex 0 moved (%g %g %g)", v.x, v.y, v.z);
    hr = IDirect3DRMMeshBuilder3_GetBox(mb, &box);
    check(hr == S_OK && vec_is(&box.min, 1, 1, 1) && vec_is(&box.max, 21, 2, 2), "GetBox (%g %g %g | %g %g %g)",
          box.min.x, box.min.y, box.min.z, box.max.x, box.max.y, box.max.z);

    check(IDirect3DRMMeshBuilder3_ReserveSpace(mb, 100, 50, 10) == S_OK && IDirect3DRMMeshBuilder3_GetVertexCount(mb) == 3, "ReserveSpace keeps the contents");
    IDirect3DRMMeshBuilder3_GetVertex(mb, 2, &v);
    check(vec_is(&v, 21, 2, 2), "and the vertices");

    vc = nc = fc = 8;
    hr = IDirect3DRMMeshBuilder3_GetGeometry(mb, &vc, vo, &nc, no, &fc, NULL);
    check(hr == S_OK && vc == 3 && nc == 1 && vec_is(&vo[2], 21, 2, 2), "GetGeometry (%#lx, %lu %lu)", hr, vc, nc);
    vc = 1;
    hr = IDirect3DRMMeshBuilder3_GetGeometry(mb, &vc, vo, &nc, no, &fc, NULL);
    check(hr == BADVALUE, "a short vertex buffer is D3DRMERR_BADVALUE (%#lx)", hr);

    check(IDirect3DRMMeshBuilder3_GetPerspective(mb) == FALSE, "no perspective at first");
    IDirect3DRMMeshBuilder3_SetPerspective(mb, TRUE);
    check(IDirect3DRMMeshBuilder3_GetPerspective(mb) == TRUE, "perspective");
    check(IDirect3DRMMeshBuilder3_GetColorSource(mb) == D3DRMCOLOR_FROMFACE, "colors from the faces at first");
    check(mb->lpVtbl->SetColorSource(mb, D3DRMCOLOR_FROMVERTEX) == S_OK
          && IDirect3DRMMeshBuilder3_GetColorSource(mb) == D3DRMCOLOR_FROMVERTEX, "color source");
    check(mb->lpVtbl->SetColorSource(mb, 5) == BADVALUE, "an unknown color source is D3DRMERR_BADVALUE");
    check(IDirect3DRMMeshBuilder3_SetTextureTopology(mb, TRUE, TRUE) == S_OK, "SetTextureTopology");

done:
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
