/* d3dx9_36, fast mode (patch 3501): D3DXSHMultiply5/6 (against D3DXSHMultiply4
 * and the identity), the frame helpers, and the new resource loaders'
 * argument checks. */
#include <windows.h>
#define COBJMACROS
#include <d3dx9.h>
#include <stdio.h>
#include <math.h>

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

typedef float *(WINAPI *shmul_t)(float *, const float *, const float *);

static void test_sh(void)
{
    HMODULE mod = LoadLibraryA("d3dx9_36.dll");
    shmul_t D3DXSHMultiply5 = (shmul_t)GetProcAddress(mod, "D3DXSHMultiply5");
    shmul_t D3DXSHMultiply6 = (shmul_t)GetProcAddress(mod, "D3DXSHMultiply6");

    float a[36], b[36], o4[16], o5[25], o6[36], c[36];
    int i, ok;

    for (i = 0; i < 36; i++)
    {
        a[i] = i < 16 ? 0.1f * (i + 1) - 0.7f : 0.0f;
        b[i] = i < 16 ? 0.9f - 0.07f * i : 0.0f;
    }
    check(D3DXSHMultiply5 && D3DXSHMultiply6, "SHMultiply5 and 6 are exported");
    if (!D3DXSHMultiply5 || !D3DXSHMultiply6) return;
    D3DXSHMultiply4(o4, a, b);
    D3DXSHMultiply5(o5, a, b);
    D3DXSHMultiply6(o6, a, b);
    ok = 1;
    for (i = 0; i < 16; i++) if (fabsf(o5[i] - o4[i]) > 1e-4f) ok = 0;
    check(ok, "SHMultiply5 matches SHMultiply4 on order-4 inputs");
    ok = 1;
    for (i = 0; i < 16; i++) if (fabsf(o6[i] - o4[i]) > 1e-4f) ok = 0;
    check(ok, "SHMultiply6 matches SHMultiply4 on order-4 inputs");

    /* the constant function 1 is 1/0.28209479 in the first coefficient: multiplying leaves b alone */
    memset(c, 0, sizeof(c));
    c[0] = 1.0f / 0.28209479f;
    for (i = 0; i < 36; i++) b[i] = 0.01f * (i + 1);
    D3DXSHMultiply6(o6, c, b);
    ok = 1;
    for (i = 0; i < 36; i++) if (fabsf(o6[i] - b[i]) > 1e-4f) ok = 0;
    check(ok, "multiplying by the constant 1 keeps the function (order 6)");
    D3DXSHMultiply5(o5, c, b);
    ok = 1;
    for (i = 0; i < 25; i++) if (fabsf(o5[i] - b[i]) > 1e-4f) ok = 0;
    check(ok, "multiplying by the constant 1 keeps the function (order 5)");
}

static void test_frames(void)
{
    D3DXFRAME root, a, b, c, d;
    D3DXVECTOR3 center;
    float radius;
    HRESULT hr;

    memset(&root, 0, sizeof(root)); memset(&a, 0, sizeof(a)); memset(&b, 0, sizeof(b));
    memset(&c, 0, sizeof(c)); memset(&d, 0, sizeof(d));
    root.Name = (char *)"root"; a.Name = (char *)"a"; c.Name = (char *)"c";

    hr = D3DXFrameAppendChild(&root, &a);
    check(hr == D3D_OK && root.pFrameFirstChild == &a, "first child");
    hr = D3DXFrameAppendChild(&root, &b);
    check(hr == D3D_OK && a.pFrameSibling == &b, "second child is a sibling of the first");
    hr = D3DXFrameAppendChild(&a, &c);
    check(hr == D3D_OK && a.pFrameFirstChild == &c, "grandchild");
    hr = D3DXFrameAppendChild(&root, &d);
    check(hr == D3D_OK && b.pFrameSibling == &d, "third child at the end");
    hr = D3DXFrameAppendChild(NULL, &a);
    check(hr == D3DERR_INVALIDCALL, "null parent");
    check(D3DXFrameNumNamedMatrices(&root) == 3, "named matrices: root, a, c");
    check(D3DXFrameNumNamedMatrices(NULL) == 0, "no frame, no matrices");

    hr = D3DXFrameCalculateBoundingSphere(&root, &center, &radius);
    check(hr == D3D_OK && radius == 0.0f, "empty hierarchy has an empty sphere");
    hr = D3DXFrameCalculateBoundingSphere(NULL, &center, &radius);
    check(hr == D3DERR_INVALIDCALL, "sphere argument check");
}

static void test_resources(void)
{
    IDirect3DCubeTexture9 *cube = NULL;
    IDirect3DVolumeTexture9 *vol = NULL;
    HRESULT hr;

    hr = D3DXCreateCubeTextureFromResourceA(NULL, NULL, "none", &cube);
    check(hr == D3DERR_INVALIDCALL, "cube from resource, no device");
    hr = D3DXCreateVolumeTextureFromResourceW(NULL, NULL, L"none", &vol);
    check(hr == D3DERR_INVALIDCALL, "volume from resource, no device");
    hr = D3DXLoadVolumeFromResourceA(NULL, NULL, NULL, NULL, "none", NULL, D3DX_DEFAULT, 0, NULL);
    check(hr == D3DERR_INVALIDCALL, "load volume from resource, no volume");
}

HRESULT WINAPI D3DXComputeTangentFrame(ID3DXMesh *mesh, DWORD options);

static void test_normal_map(void)
{
    IDirect3D9 *d3d = Direct3DCreate9(D3D_SDK_VERSION);
    IDirect3DDevice9 *device = NULL;
    IDirect3DTexture9 *src = NULL, *dst = NULL;
    D3DPRESENT_PARAMETERS pp = {0};
    D3DLOCKED_RECT lr;
    HWND hwnd;
    HRESULT hr;
    UINT x, y;

    if (!d3d) { printf("SKIP  no d3d9\n"); return; }
    hwnd = CreateWindowA("static", "nm", WS_OVERLAPPED, 0, 0, 64, 64, NULL, NULL, NULL, NULL);
    pp.Windowed = TRUE;
    pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    pp.BackBufferFormat = D3DFMT_UNKNOWN;
    hr = IDirect3D9_CreateDevice(d3d, 0, D3DDEVTYPE_HAL, hwnd, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &pp, &device);
    if (FAILED(hr)) { printf("SKIP  no device %lx\n", hr); IDirect3D9_Release(d3d); DestroyWindow(hwnd); return; }

    hr = IDirect3DDevice9_CreateTexture(device, 4, 4, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &src, NULL);
    check(hr == S_OK, "create source");
    hr = IDirect3DDevice9_CreateTexture(device, 4, 4, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &dst, NULL);
    check(hr == S_OK, "create normal map");
    if (src && dst)
    {
        DWORD *px;

        IDirect3DTexture9_LockRect(src, 0, &lr, NULL, 0);
        for (y = 0; y < 4; y++)
        {
            px = (DWORD *)((BYTE *)lr.pBits + y * lr.Pitch);
            for (x = 0; x < 4; x++) px[x] = 0xff000000 | (x * 0x40) << 16;  /* a ramp in red */
        }
        IDirect3DTexture9_UnlockRect(src, 0);
        hr = D3DXComputeNormalMap(dst, src, NULL, D3DX_NORMALMAP_MIRROR, D3DX_CHANNEL_RED, 1.0f);
        check(hr == S_OK, "D3DXComputeNormalMap");
        IDirect3DTexture9_LockRect(dst, 0, &lr, NULL, D3DLOCK_READONLY);
        px = (DWORD *)((BYTE *)lr.pBits + 1 * lr.Pitch) + 1; /* in the ramp */
        printf("  pixel %08lx\n", *px);
        check(((*px >> 16) & 0xff) < 0x70 && ((*px >> 8) & 0xff) >= 0x78 && ((*px >> 8) & 0xff) <= 0x88
                && (*px & 0xff) > 0xb0, "a rising ramp tilts the normal toward -x");
        IDirect3DTexture9_UnlockRect(dst, 0);
        hr = D3DXComputeNormalMap(dst, src, NULL, D3DX_NORMALMAP_MIRROR | D3DX_NORMALMAP_INVERTSIGN,
                D3DX_CHANNEL_RED, 1.0f);
        IDirect3DTexture9_LockRect(dst, 0, &lr, NULL, D3DLOCK_READONLY);
        px = (DWORD *)((BYTE *)lr.pBits + 1 * lr.Pitch) + 1;
        check(((*px >> 16) & 0xff) > 0x90, "INVERTSIGN flips it");
        IDirect3DTexture9_UnlockRect(dst, 0);
        hr = D3DXComputeNormalMap(dst, src, NULL, 0, 99, 1.0f);
        check(hr == D3DERR_INVALIDCALL, "bad channel");
    }

    {
        static const D3DVERTEXELEMENT9 decl[] = {
            {0, 0,  D3DDECLTYPE_FLOAT3, 0, D3DDECLUSAGE_POSITION, 0},
            {0, 12, D3DDECLTYPE_FLOAT2, 0, D3DDECLUSAGE_TEXCOORD, 0},
            {0, 20, D3DDECLTYPE_FLOAT3, 0, D3DDECLUSAGE_NORMAL, 0},
            {0, 32, D3DDECLTYPE_FLOAT3, 0, D3DDECLUSAGE_TANGENT, 0},
            {0, 44, D3DDECLTYPE_FLOAT3, 0, D3DDECLUSAGE_BINORMAL, 0},
            D3DDECL_END() };
        static const D3DVERTEXELEMENT9 decl_plain[] = {
            {0, 0,  D3DDECLTYPE_FLOAT3, 0, D3DDECLUSAGE_POSITION, 0},
            {0, 12, D3DDECLTYPE_FLOAT2, 0, D3DDECLUSAGE_TEXCOORD, 0},
            D3DDECL_END() };
        ID3DXMesh *quad = NULL, *out = NULL, *joined = NULL;
        ID3DXBuffer *map = NULL;
        float *v = NULL;
        WORD *idx = NULL;
        /* a unit quad in the XY plane; u grows along x, v along -y */
        static const float pos[4][5] = { {0,0,0, 0,1}, {1,0,0, 1,1}, {1,1,0, 1,0}, {0,1,0, 0,0} };
        UINT k;

        hr = D3DXCreateMesh(2, 4, D3DXMESH_SYSTEMMEM, decl, device, &quad);
        check(hr == S_OK, "create a quad");
        if (quad)
        {
            quad->lpVtbl->LockVertexBuffer(quad, 0, (void **)&v);
            memset(v, 0, 4 * 56);
            for (k = 0; k < 4; k++)
            {
                memcpy(v + k * 14, pos[k], 5 * sizeof(float));
                v[k * 14 + 7] = 1.0f;   /* normal z */
            }
            quad->lpVtbl->UnlockVertexBuffer(quad);
            quad->lpVtbl->LockIndexBuffer(quad, 0, (void **)&idx);
            idx[0] = 0; idx[1] = 3; idx[2] = 2; idx[3] = 0; idx[4] = 2; idx[5] = 1;
            quad->lpVtbl->UnlockIndexBuffer(quad);

            hr = D3DXComputeTangentFrameEx(quad, D3DDECLUSAGE_TEXCOORD, 0, D3DDECLUSAGE_TANGENT, 0,
                    D3DDECLUSAGE_BINORMAL, 0, D3DX_DEFAULT, 0, D3DXTANGENT_GENERATE_IN_PLACE, NULL,
                    -1.01f, -0.01f, -1.01f, NULL, NULL);
            check(hr == S_OK, "tangent frame in place");
            quad->lpVtbl->LockVertexBuffer(quad, D3DLOCK_READONLY, (void **)&v);
            printf("  tangent %g %g %g binormal %g %g %g\n", v[8], v[9], v[10], v[11], v[12], v[13]);
            check(fabsf(v[8] - 1) < 1e-4 && fabsf(v[9]) < 1e-4 && fabsf(v[10]) < 1e-4, "tangent follows +u (x)");
            check(fabsf(v[11]) < 1e-4 && fabsf(v[12] + 1) < 1e-4 && fabsf(v[13]) < 1e-4, "binormal follows +v (-y)");
            quad->lpVtbl->UnlockVertexBuffer(quad);

            hr = D3DXComputeTangent(quad, 0, 0, 0, 0, NULL);
            check(hr == S_OK, "D3DXComputeTangent");
            hr = D3DXComputeTangentFrame(quad, 0);
            check(hr == S_OK, "D3DXComputeTangentFrame");

            hr = D3DXConcatenateMeshes(&quad, 1, D3DXMESH_SYSTEMMEM, NULL, NULL, NULL, device, &joined);
            check(hr == S_OK && joined, "D3DXConcatenateMeshes of one mesh");
            if (joined)
            {
                ID3DXMesh *both[2] = { quad, quad }, *two = NULL;
                D3DXMATRIX mv[2];
                float *tv = NULL;

                D3DXMatrixIdentity(&mv[0]);
                D3DXMatrixTranslation(&mv[1], 10.0f, 0, 0);
                hr = D3DXConcatenateMeshes(both, 2, D3DXMESH_SYSTEMMEM, mv, NULL, NULL, NULL, &two);
                check(hr == S_OK && two && two->lpVtbl->GetNumVertices(two) == 8 && two->lpVtbl->GetNumFaces(two) == 4,
                        "two meshes: 8 vertices, 4 faces");
                if (two)
                {
                    WORD *ti = NULL;

                    two->lpVtbl->LockVertexBuffer(two, D3DLOCK_READONLY, (void **)&tv);
                    check(tv && fabsf(tv[4 * 14] - 10.0f) < 1e-4, "second mesh moved by its matrix");
                    two->lpVtbl->UnlockVertexBuffer(two);
                    two->lpVtbl->LockIndexBuffer(two, D3DLOCK_READONLY, (void **)&ti);
                    check(ti && ti[6] == 4, "second mesh indices are offset");
                    two->lpVtbl->UnlockIndexBuffer(two);
                    two->lpVtbl->Release(two);
                }
                joined->lpVtbl->Release(joined);
            }


            /* save to an .x file and load it back */
            {
                D3DXMATERIAL mat;
                ID3DXMesh *loaded = NULL;
                ID3DXBuffer *mats = NULL;
                DWORD nmat = 0;
                char path[MAX_PATH];

                memset(&mat, 0, sizeof(mat));
                mat.MatD3D.Diffuse.r = 0.25f; mat.MatD3D.Diffuse.g = 0.5f; mat.MatD3D.Diffuse.b = 0.75f; mat.MatD3D.Diffuse.a = 1.0f;
                mat.pTextureFilename = (char *)"wall.bmp";
                GetTempPathA(sizeof(path), path);
                lstrcatA(path, "fastb-quad.x");
                hr = D3DXSaveMeshToXA(path, quad, NULL, &mat, NULL, 1, D3DXF_FILEFORMAT_TEXT);
                check(hr == S_OK, "D3DXSaveMeshToXA");
                hr = D3DXLoadMeshFromXA(path, D3DXMESH_SYSTEMMEM, device, NULL, &mats, NULL, &nmat, &loaded);
                check(hr == S_OK && loaded, "the saved file loads");
                if (loaded)
                {
                    float *lv = NULL;
                    D3DVERTEXELEMENT9 ld[MAX_FVF_DECL_SIZE];
                    int has_normal = 0, has_tex = 0, e;

                    check(loaded->lpVtbl->GetNumFaces(loaded) == 2, "two faces");
                    check(loaded->lpVtbl->GetNumVertices(loaded) == 4, "four vertices");
                    loaded->lpVtbl->GetDeclaration(loaded, ld);
                    for (e = 0; ld[e].Stream != 0xff; e++)
                    {
                        if (ld[e].Usage == D3DDECLUSAGE_NORMAL) has_normal = 1;
                        if (ld[e].Usage == D3DDECLUSAGE_TEXCOORD) has_tex = 1;
                    }
                    check(has_normal && has_tex, "normals and texture coordinates came back");
                    loaded->lpVtbl->LockVertexBuffer(loaded, D3DLOCK_READONLY, (void **)&lv);
                    if (lv)
                    {
                        UINT st = loaded->lpVtbl->GetNumBytesPerVertex(loaded) / 4, found = 0;
                        for (k = 0; k < 4; k++)
                            if (fabsf(lv[k * st] - 1.0f) < 1e-4 && fabsf(lv[k * st + 1] - 1.0f) < 1e-4) found = 1;
                        check(found, "the vertex (1,1,0) is there");
                    }
                    loaded->lpVtbl->UnlockVertexBuffer(loaded);
                    loaded->lpVtbl->Release(loaded);
                }
                check(nmat == 1 && mats, "one material");
                if (mats && nmat == 1)
                {
                    D3DXMATERIAL *lm = ID3DXBuffer_GetBufferPointer(mats);
                    check(lm->pTextureFilename && !strcmp(lm->pTextureFilename, "wall.bmp"), "the texture name");
                    check(fabsf(lm->MatD3D.Diffuse.g - 0.5f) < 1e-3, "the diffuse colour");
                }
                if (mats) ID3DXBuffer_Release(mats);
                DeleteFileA(path);
                hr = D3DXSaveMeshToXA(path, quad, NULL, NULL, NULL, 0, 9);
                check(hr == D3DERR_INVALIDCALL, "bad format");
            }

            /* the copy path: ask for tangent and binormal on a mesh that has none */
            {
                ID3DXMesh *plain = NULL;

                hr = quad->lpVtbl->CloneMesh(quad, D3DXMESH_SYSTEMMEM, decl_plain, device, &plain);
                check(hr == S_OK, "clone without tangents");
                if (plain)
                {
                    hr = D3DXComputeTangentFrameEx(plain, D3DDECLUSAGE_TEXCOORD, 0, D3DDECLUSAGE_TANGENT, 0,
                            D3DDECLUSAGE_BINORMAL, 0, D3DX_DEFAULT, 0, 0, NULL, -1.01f, -0.01f, -1.01f, &out, &map);
                    check(hr == S_OK && out && map, "tangent frame into a new mesh");
                    if (out)
                    {
                        D3DVERTEXELEMENT9 d2[MAX_FVF_DECL_SIZE];

                        out->lpVtbl->GetDeclaration(out, d2);
                        check(D3DXGetDeclVertexSize(d2, 0) == 20 + 24, "the new mesh has the two new elements");
                        out->lpVtbl->Release(out);
                    }
                    if (map) ID3DXBuffer_Release(map);
                    plain->lpVtbl->Release(plain);
                }
            }
            quad->lpVtbl->Release(quad);
        }
    }
    if (src) IDirect3DTexture9_Release(src);
    if (dst) IDirect3DTexture9_Release(dst);
    IDirect3DDevice9_Release(device);
    IDirect3D9_Release(d3d);
    DestroyWindow(hwnd);
}

int main(void)
{
    test_normal_map();
    test_sh();
    test_frames();
    test_resources();
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
