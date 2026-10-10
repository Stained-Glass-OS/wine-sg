/* d3dx9_36 (patches/sg/2648): D3DXValidMesh finds the faces that use a vertex twice, indices that are out of
 * range and bow-tie vertices; ID3DXSkinInfo keeps and reports its influences; D3DXComputeTangentFrameEx refuses
 * a call with nothing to compute or nowhere to put it (the Wine tests record Windows for the last). */
#define COBJMACROS
#include <windows.h>
#include <d3d9.h>
#include <d3dx9.h>
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

static ID3DXMesh *make_mesh(IDirect3DDevice9 *device, UINT vertices, UINT faces, const DWORD *indices)
{
    ID3DXMesh *mesh = NULL;
    D3DXVECTOR3 *v;
    WORD *idx;
    UINT i;

    if (FAILED(D3DXCreateMeshFVF(faces, vertices, D3DXMESH_SYSTEMMEM, D3DFVF_XYZ, device, &mesh))) return NULL;
    mesh->lpVtbl->LockVertexBuffer(mesh, 0, (void **)&v);
    for (i = 0; i < vertices; i++) { v[i].x = (float)i; v[i].y = (float)(i & 1); v[i].z = 0.0f; }
    mesh->lpVtbl->UnlockVertexBuffer(mesh);
    mesh->lpVtbl->LockIndexBuffer(mesh, 0, (void **)&idx);
    for (i = 0; i < faces * 3; i++) idx[i] = (WORD)indices[i];
    mesh->lpVtbl->UnlockIndexBuffer(mesh);
    return mesh;
}

int main(void)
{
    static const DWORD good[] = { 0, 1, 2 };
    static const DWORD good_adj[] = { ~0u, ~0u, ~0u };
    static const DWORD bowtie[] = { 0, 1, 2, 1, 3, 4 };
    static const DWORD bowtie_adj[] = { ~0u, ~0u, ~0u, ~0u, ~0u, ~0u };
    static const DWORD fan[] = { 0, 1, 2, 2, 1, 3 };
    static const DWORD fan_adj[] = { ~0u, 1, ~0u, 0, ~0u, ~0u };
    static const DWORD twice[] = { 0, 1, 1 };
    static const DWORD range[] = { 0, 1, 9 };
    D3DPRESENT_PARAMETERS pp = {0};
    IDirect3DDevice9 *device = NULL;
    IDirect3D9 *d3d;
    HWND window;
    HRESULT hr;
    ID3DXMesh *mesh;
    ID3DXBuffer *errors;
    ID3DXSkinInfo *skin = NULL;

    window = CreateWindowA("static", "mesh", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 0, 0, 200, 200, 0, 0, 0, 0);
    d3d = Direct3DCreate9(D3D_SDK_VERSION);
    check(d3d != NULL, "Direct3D 9");
    if (!d3d) goto done;
    pp.BackBufferWidth = 64; pp.BackBufferHeight = 64; pp.BackBufferFormat = D3DFMT_X8R8G8B8;
    pp.SwapEffect = D3DSWAPEFFECT_DISCARD; pp.Windowed = TRUE; pp.hDeviceWindow = window;
    hr = IDirect3D9_CreateDevice(d3d, 0, D3DDEVTYPE_HAL, window, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &pp, &device);
    check(hr == D3D_OK, "device (%#lx)", hr);
    if (FAILED(hr)) goto done;

    {
        static const struct { const DWORD *idx; const DWORD *adj; UINT verts, faces; HRESULT want; const char *name; } tests[] =
        {
            { good, good_adj, 3, 1, D3D_OK, "one triangle" },
            { fan, fan_adj, 4, 2, D3D_OK, "two triangles that share an edge" },
            { bowtie, bowtie_adj, 5, 2, D3DXERR_INVALIDMESH, "bow-tie" },
            { twice, good_adj, 3, 1, D3DXERR_INVALIDMESH, "a vertex twice in a face" },
            { range, good_adj, 3, 1, D3DXERR_INVALIDMESH, "an index out of range" },
        };
        unsigned int i;

        for (i = 0; i < sizeof(tests) / sizeof(tests[0]); i++)
        {
            mesh = make_mesh(device, tests[i].verts, tests[i].faces, tests[i].idx);
            errors = NULL;
            hr = D3DXValidMesh(mesh, tests[i].adj, &errors);
            check(hr == tests[i].want, "%s: %#lx (want %#lx)", tests[i].name, hr, tests[i].want);
            if (errors) errors->lpVtbl->Release(errors);
            mesh->lpVtbl->Release(mesh);
        }
    }

    /* the tangent frame arguments */
    mesh = NULL;
    D3DXCreateBox(device, 1.0f, 1.0f, 1.0f, &mesh, NULL);
    if (mesh)
    {
        hr = D3DXComputeTangentFrameEx(mesh, D3DX_DEFAULT, 0, D3DX_DEFAULT, 0, D3DX_DEFAULT, 0, D3DDECLUSAGE_NORMAL, 0,
                D3DXTANGENT_GENERATE_IN_PLACE, NULL, -1.01f, -0.01f, -1.01f, NULL, NULL);
        check(hr == D3DERR_INVALIDCALL, "nothing to compute (%#lx)", hr);
        hr = D3DXComputeTangentFrameEx(mesh, D3DX_DEFAULT, 0, D3DX_DEFAULT, 0, D3DX_DEFAULT, 0, D3DDECLUSAGE_NORMAL, 0,
                0, NULL, -1.01f, -0.01f, -1.01f, NULL, NULL);
        check(hr == D3DERR_INVALIDCALL, "no flags (%#lx)", hr);
        hr = D3DXComputeTangentFrameEx(mesh, D3DX_DEFAULT, 0, D3DX_DEFAULT, 0, D3DX_DEFAULT, 0, D3DDECLUSAGE_NORMAL, 0,
                D3DXTANGENT_CALCULATE_NORMALS, NULL, -1.01f, -0.01f, -1.01f, NULL, NULL);
        check(hr == D3DERR_INVALIDCALL, "no output mesh and not in place (%#lx)", hr);
        mesh->lpVtbl->Release(mesh);
    }

    /* skin info */
    hr = D3DXCreateSkinInfoFVF(6, D3DFVF_XYZ, 3, &skin);
    check(hr == D3D_OK && skin, "skin info (%#lx)", hr);
    if (skin)
    {
        static const DWORD v0[] = { 0, 1, 2 }, v1[] = { 1, 2 }, v2[] = { 4 };
        static const float w0[] = { 1.0f, 0.5f, 0.25f }, w1[] = { 0.5f, 0.75f }, w2[] = { 1.0f };
        DWORD max = 99, idx = 99, vertex = 99, face_max = 99;
        float weight = 0;
        IDirect3DIndexBuffer9 *ib;
        WORD *indices;

        skin->lpVtbl->SetBoneInfluence(skin, 0, 3, v0, w0);
        skin->lpVtbl->SetBoneInfluence(skin, 1, 2, v1, w1);
        skin->lpVtbl->SetBoneInfluence(skin, 2, 1, v2, w2);

        hr = skin->lpVtbl->GetMaxVertexInfluences(skin, &max);
        check(hr == D3D_OK && max == 2, "most influences of a vertex = %lu (%#lx)", max, hr);
        hr = skin->lpVtbl->GetBoneVertexInfluence(skin, 1, 1, &weight, &vertex);
        check(hr == D3D_OK && weight == 0.75f && vertex == 2, "influence 1 of bone 1: vertex %lu weight %g (%#lx)", vertex, weight, hr);
        check(skin->lpVtbl->GetBoneVertexInfluence(skin, 1, 2, &weight, &vertex) == D3DERR_INVALIDCALL, "influence out of range");
        check(skin->lpVtbl->SetBoneVertexInfluence(skin, 1, 1, 0.1f) == D3D_OK, "SetBoneVertexInfluence");
        skin->lpVtbl->GetBoneVertexInfluence(skin, 1, 1, &weight, &vertex);
        check(weight == 0.1f, "and it is kept (%g)", weight);
        check(skin->lpVtbl->SetBoneVertexInfluence(skin, 1, 2, 0.1f) == D3DERR_INVALIDCALL, "set out of range");
        hr = skin->lpVtbl->FindBoneVertexInfluenceIndex(skin, 0, 2, &idx);
        check(hr == D3D_OK && idx == 2, "vertex 2 is influence %lu of bone 0 (%#lx)", idx, hr);
        check(skin->lpVtbl->FindBoneVertexInfluenceIndex(skin, 0, 5, &idx) == D3DERR_INVALIDCALL, "vertex 5 is not influenced by bone 0");

        check(skin->lpVtbl->SetMinBoneInfluence(skin, 0.3f) == D3D_OK && skin->lpVtbl->GetMinBoneInfluence(skin) == 0.3f, "min influence is kept");

        /* two faces: (0,1,2) is influenced by bones 0 and 1, (3,4,5) by bone 2 only */
        IDirect3DDevice9_CreateIndexBuffer(device, 6 * sizeof(WORD), 0, D3DFMT_INDEX16, D3DPOOL_MANAGED, &ib, NULL);
        IDirect3DIndexBuffer9_Lock(ib, 0, 0, (void **)&indices, 0);
        indices[0] = 0; indices[1] = 1; indices[2] = 2; indices[3] = 3; indices[4] = 4; indices[5] = 5;
        IDirect3DIndexBuffer9_Unlock(ib);
        hr = skin->lpVtbl->GetMaxFaceInfluences(skin, ib, 2, &face_max);
        check(hr == D3D_OK && face_max == 2, "most bones of a face = %lu (%#lx)", face_max, hr);
        IDirect3DIndexBuffer9_Release(ib);
        skin->lpVtbl->Release(skin);
    }

done:
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
