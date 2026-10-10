/* d3drm (patches/sg/2660): the state of frames -- colour, modes, fog -- positions, orientations, velocities and
 * rotations in the space of a reference frame, Move and its callbacks, Transform and InverseTransform. */
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
#define NOTFOUND ((HRESULT)0x88760311)

static int approx(float a, float b) { return fabsf(a - b) < 1e-4f; }
static int vec_is(const D3DVECTOR *v, float x, float y, float z) { return approx(v->x, x) && approx(v->y, y) && approx(v->z, z); }

static int calls;
static void *call_log[8];
static void __cdecl move_cb(IDirect3DRMFrame3 *frame, void *ctx, D3DVALUE delta)
{
    if (calls < 8) call_log[calls] = frame;
    calls++;
}

int main(void)
{
    HMODULE mod = LoadLibraryA("d3drm.dll");
    HRESULT (WINAPI *create)(IDirect3DRM **) = mod ? (void *)GetProcAddress(mod, "Direct3DRMCreate") : NULL;
    IDirect3DRM *drm1 = NULL;
    IDirect3DRM3 *drm = NULL;
    IDirect3DRMFrame3 *root = NULL, *child = NULL, *sibling = NULL;
    D3DVECTOR v, v2;
    D3DVALUE a, b, c;
    HRESULT hr;
    D3DRMMATRIX4D m;

    if (!create || FAILED(create(&drm1))) { printf("FAIL  no Direct3DRM\nRESULT: FAIL\n"); return 1; }
    IDirect3DRM_QueryInterface(drm1, &my_IID_Drm3, (void **)&drm);
    IDirect3DRM3_CreateFrame(drm, NULL, &root);
    IDirect3DRM3_CreateFrame(drm, root, &child);
    IDirect3DRM3_CreateFrame(drm, root, &sibling);
    check(root && child && sibling, "frames");
    if (!root || !child || !sibling) goto done;

    /* colour */
    IDirect3DRMFrame3_SetColor(child, 0x12345678);
    check(IDirect3DRMFrame3_GetColor(child) == 0x12345678, "colour round trip (%08lx)", IDirect3DRMFrame3_GetColor(child));
    IDirect3DRMFrame3_SetColorRGB(child, 1.0f, 0.0f, 0.0f);
    check(IDirect3DRMFrame3_GetColor(child) == 0xffff0000, "SetColorRGB (%08lx)", IDirect3DRMFrame3_GetColor(child));

    /* modes */
    check(IDirect3DRMFrame3_GetSortMode(child) == D3DRMSORT_FROMPARENT && IDirect3DRMFrame3_GetZbufferMode(child) == D3DRMZBUFFER_FROMPARENT,
          "default sort and z buffer modes come from the parent");
    check(IDirect3DRMFrame3_SetSortMode(child, D3DRMSORT_BACKTOFRONT) == S_OK && IDirect3DRMFrame3_GetSortMode(child) == D3DRMSORT_BACKTOFRONT, "sort mode");
    check(IDirect3DRMFrame3_SetSortMode(child, 77) == BADVALUE, "an unknown sort mode is D3DRMERR_BADVALUE");
    check(IDirect3DRMFrame3_SetZbufferMode(child, D3DRMZBUFFER_DISABLE) == S_OK && IDirect3DRMFrame3_GetZbufferMode(child) == D3DRMZBUFFER_DISABLE, "z buffer mode");
    check(IDirect3DRMFrame3_SetZbufferMode(child, 9) == BADVALUE, "an unknown z buffer mode is D3DRMERR_BADVALUE");
    check(IDirect3DRMFrame3_SetMaterialMode(child, D3DRMMATERIAL_FROMFRAME) == S_OK && IDirect3DRMFrame3_GetMaterialMode(child) == D3DRMMATERIAL_FROMFRAME, "material mode");
    check(IDirect3DRMFrame3_SetMaterialMode(child, 9) == BADVALUE, "an unknown material mode is D3DRMERR_BADVALUE");

    /* fog */
    IDirect3DRMFrame3_SetSceneFogEnable(root, TRUE);
    IDirect3DRMFrame3_SetSceneFogColor(root, 0xff102030);
    IDirect3DRMFrame3_SetSceneFogMode(root, D3DRMFOG_EXPONENTIAL);
    IDirect3DRMFrame3_SetSceneFogParams(root, 1.5f, 20.0f, 0.25f);
    check(IDirect3DRMFrame3_GetSceneFogEnable(root) == TRUE && IDirect3DRMFrame3_GetSceneFogColor(root) == 0xff102030
          && IDirect3DRMFrame3_GetSceneFogMode(root) == D3DRMFOG_EXPONENTIAL, "fog enable, colour and mode");
    hr = IDirect3DRMFrame3_GetSceneFogParams(root, &a, &b, &c);
    check(hr == S_OK && approx(a, 1.5f) && approx(b, 20.0f) && approx(c, 0.25f), "fog parameters");
    check(IDirect3DRMFrame3_SetSceneFogMode(root, 7) == BADVALUE, "an unknown fog mode is D3DRMERR_BADVALUE");

    /* positions: root at (10,0,0); sibling at (100,0,0) and child at (1,2,3) in the root's space */
    IDirect3DRMFrame3_SetPosition(root, NULL, 10.0f, 0.0f, 0.0f);
    IDirect3DRMFrame3_SetPosition(sibling, NULL, 100.0f, 0.0f, 0.0f);
    IDirect3DRMFrame3_SetPosition(child, NULL, 1.0f, 2.0f, 3.0f);
    hr = IDirect3DRMFrame3_GetPosition(child, NULL, &v);
    check(hr == S_OK && vec_is(&v, 1, 2, 3), "position in the parent's space (%g %g %g)", v.x, v.y, v.z);
    hr = IDirect3DRMFrame3_GetPosition(child, root, &v);
    check(hr == S_OK && vec_is(&v, 1, 2, 3), "position seen from the parent (%g %g %g)", v.x, v.y, v.z);
    hr = IDirect3DRMFrame3_GetPosition(child, sibling, &v);
#ifdef SG_DUMMY
#endif
    check(hr == S_OK && vec_is(&v, -99, 2, 3), "position seen from a sibling (%g %g %g)", v.x, v.y, v.z);
    IDirect3DRMFrame3_SetPosition(child, sibling, 0.0f, 0.0f, 0.0f);
    IDirect3DRMFrame3_GetPosition(child, NULL, &v);
    check(vec_is(&v, 100, 0, 0), "position set in a sibling's space (%g %g %g)", v.x, v.y, v.z);
    IDirect3DRMFrame3_SetPosition(child, NULL, 1.0f, 2.0f, 3.0f);

    /* Transform and InverseTransform */
    v.x = v.y = v.z = 0.0f;
    IDirect3DRMFrame3_Transform(child, &v2, &v);
    check(vec_is(&v2, 11, 2, 3), "Transform (%g %g %g)", v2.x, v2.y, v2.z);
    v.x = 11.0f; v.y = 2.0f; v.z = 3.0f;
    hr = IDirect3DRMFrame3_InverseTransform(child, &v2, &v);
    check(hr == S_OK && vec_is(&v2, 0, 0, 0), "InverseTransform (%g %g %g)", v2.x, v2.y, v2.z);
    {
        D3DVECTOR src[2] = { { 0, 0, 0 }, { 1, 1, 1 } }, dst[2];

        hr = IDirect3DRMFrame3_TransformVectors(child, NULL, 2, dst, src);
        check(hr == S_OK && vec_is(&dst[0], 11, 2, 3) && vec_is(&dst[1], 12, 3, 4), "TransformVectors (%g %g %g)", dst[1].x, dst[1].y, dst[1].z);
        hr = IDirect3DRMFrame3_InverseTransformVectors(child, NULL, 2, src, dst);
        check(hr == S_OK && vec_is(&src[0], 0, 0, 0) && vec_is(&src[1], 1, 1, 1), "InverseTransformVectors");
    }

    /* orientation */
    hr = IDirect3DRMFrame3_SetOrientation(child, NULL, 1, 0, 0, 0, 1, 0);
    check(hr == S_OK, "SetOrientation (%#lx)", hr);
    hr = IDirect3DRMFrame3_GetOrientation(child, NULL, &v, &v2);
    check(hr == S_OK && vec_is(&v, 1, 0, 0) && vec_is(&v2, 0, 1, 0), "GetOrientation (%g %g %g | %g %g %g)", v.x, v.y, v.z, v2.x, v2.y, v2.z);
    IDirect3DRMFrame3_GetTransform(child, NULL, m);
    check(approx(m[2][0], 1.0f) && approx(m[3][0], 1.0f) && approx(m[3][1], 2.0f), "the transform has the new axis and keeps the position (%g, pos %g %g)", m[2][0], m[3][0], m[3][1]);
    check(IDirect3DRMFrame3_SetOrientation(child, NULL, 0, 1, 0, 0, 1, 0) == BADVALUE, "parallel direction and up are D3DRMERR_BADVALUE");
    check(IDirect3DRMFrame3_SetOrientation(child, NULL, 0, 0, 0, 0, 1, 0) == BADVALUE, "a zero direction is D3DRMERR_BADVALUE");
    IDirect3DRMFrame3_SetOrientation(child, NULL, 0, 0, 1, 0, 1, 0);

    /* velocity, rotation and Move, children included */
    IDirect3DRMFrame3_SetVelocity(child, NULL, 1.0f, 0.0f, 0.0f, FALSE);
    IDirect3DRMFrame3_SetVelocity(root, NULL, 0.0f, 5.0f, 0.0f, FALSE);
    hr = IDirect3DRMFrame3_GetVelocity(child, NULL, &v, FALSE);
    check(hr == S_OK && vec_is(&v, 1, 0, 0), "GetVelocity");
    hr = IDirect3DRMFrame3_Move(root, 2.0f);
    check(hr == S_OK, "Move (%#lx)", hr);
    IDirect3DRMFrame3_GetPosition(root, NULL, &v);
    IDirect3DRMFrame3_GetPosition(child, NULL, &v2);
    check(vec_is(&v, 10, 10, 0), "the root moved by its velocity (%g %g %g)", v.x, v.y, v.z);
    check(vec_is(&v2, 3, 2, 3), "the child moved too (%g %g %g)", v2.x, v2.y, v2.z);
    IDirect3DRMFrame3_SetVelocity(child, NULL, 0, 0, 0, FALSE);
    IDirect3DRMFrame3_SetVelocity(root, NULL, 0, 0, 0, FALSE);

    IDirect3DRMFrame3_SetRotation(child, NULL, 0.0f, 0.0f, 1.0f, 1.5707964f);
    hr = IDirect3DRMFrame3_GetRotation(child, NULL, &v, &a);
    check(hr == S_OK && vec_is(&v, 0, 0, 1) && approx(a, 1.5707964f), "GetRotation");
    IDirect3DRMFrame3_Move(child, 1.0f);
    IDirect3DRMFrame3_GetOrientation(child, NULL, &v, &v2);
    check(vec_is(&v, 0, 0, 1) && approx(fabsf(v2.x), 1.0f) && approx(v2.y, 0.0f), "a turn about Z by a quarter keeps Z and turns up (%g %g %g | %g %g %g)", v.x, v.y, v.z, v2.x, v2.y, v2.z);
    IDirect3DRMFrame3_GetPosition(child, NULL, &v);
    check(vec_is(&v, 3, 2, 3), "turning in place keeps the position (%g %g %g)", v.x, v.y, v.z);
    IDirect3DRMFrame3_SetRotation(child, NULL, 0, 0, 1, 0);

    /* callbacks */
    calls = 0;
    check(IDirect3DRMFrame3_AddMoveCallback(root, move_cb, NULL, D3DRMCALLBACK_PREORDER) == S_OK, "AddMoveCallback");
    IDirect3DRMFrame3_AddMoveCallback(child, move_cb, NULL, D3DRMCALLBACK_POSTORDER);
    IDirect3DRMFrame3_Move(root, 0.5f);
    check(calls == 2 && call_log[0] == (void *)root && call_log[1] == (void *)child, "callbacks run on every frame moved, the before-order ones first (%d)", calls);
    check(IDirect3DRMFrame3_DeleteMoveCallback(root, move_cb, NULL) == S_OK, "DeleteMoveCallback");
    check(IDirect3DRMFrame3_DeleteMoveCallback(root, move_cb, NULL) == NOTFOUND, "deleting it twice is D3DRMERR_NOTFOUND");
    IDirect3DRMFrame3_DeleteMoveCallback(child, move_cb, NULL);

    /* a post-order callback of the root runs after the child's */
    calls = 0;
    memset(call_log, 0, sizeof(call_log));
    IDirect3DRMFrame3_AddMoveCallback(root, move_cb, NULL, D3DRMCALLBACK_POSTORDER);
    IDirect3DRMFrame3_AddMoveCallback(child, move_cb, NULL, D3DRMCALLBACK_PREORDER);
    IDirect3DRMFrame3_Move(root, 0.5f);
    check(calls == 2 && call_log[0] == (void *)child && call_log[1] == (void *)root, "a post-order callback of the parent runs after the child's");
    IDirect3DRMFrame3_DeleteMoveCallback(root, move_cb, NULL);
    IDirect3DRMFrame3_DeleteMoveCallback(child, move_cb, NULL);

    /* the rest of the state */
    IDirect3DRMFrame3_SetInheritAxes(child, TRUE);
    check(IDirect3DRMFrame3_GetInheritAxes(child) == TRUE, "inherit axes");
    IDirect3DRMFrame3_SetAxes(child, 1, 2, 3, 4, 5, 6);
    IDirect3DRMFrame3_GetAxes(child, &v, &v2);
    check(vec_is(&v, 1, 2, 3) && vec_is(&v2, 4, 5, 6), "axes");
    IDirect3DRMFrame3_SetBoxEnable(child, TRUE);
    check(IDirect3DRMFrame3_GetBoxEnable(child) == TRUE, "box enable");

done:
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
