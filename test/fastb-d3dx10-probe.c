/* d3dx10_43 / d3dx11_43, fast mode round 2 (patches 3592..): sprites, fonts, meshes, thread pump paths,
 * texture filtering and saving, compile helpers. Raw vtable slots: no d3dx10 headers in the toolchain. */
#define COBJMACROS
#include <initguid.h>
#include <windows.h>
#include <d3d10.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

#define VT(obj) (*(void ***)(obj))
#define CALL(obj, slot, type, ...) (((type)VT(obj)[slot])(obj, ##__VA_ARGS__))

static void unk_release(void *o) { if (o) CALL(o, 2, ULONG (WINAPI *)(void *)); }

static ID3D10Device *make_device(void)
{
    ID3D10Device *dev = NULL;
    HRESULT hr = D3D10CreateDevice(NULL, D3D10_DRIVER_TYPE_HARDWARE, NULL, 0, D3D10_SDK_VERSION, &dev);
    if (FAILED(hr)) hr = D3D10CreateDevice(NULL, D3D10_DRIVER_TYPE_WARP, NULL, 0, D3D10_SDK_VERSION, &dev);
    if (FAILED(hr)) hr = D3D10CreateDevice(NULL, D3D10_DRIVER_TYPE_REFERENCE, NULL, 0, D3D10_SDK_VERSION, &dev);
    return FAILED(hr) ? NULL : dev;
}

/* render target helper */
struct rt { ID3D10Texture2D *tex, *staging; ID3D10RenderTargetView *rtv; UINT w, h; };

static int rt_create(ID3D10Device *dev, struct rt *rt, UINT w, UINT h)
{
    D3D10_TEXTURE2D_DESC d = {0};
    D3D10_VIEWPORT vp = {0, 0, w, h, 0.0f, 1.0f};
    float clear[4] = {0, 0, 0, 1};

    d.Width = w; d.Height = h; d.MipLevels = 1; d.ArraySize = 1; d.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    d.SampleDesc.Count = 1; d.Usage = D3D10_USAGE_DEFAULT; d.BindFlags = D3D10_BIND_RENDER_TARGET;
    if (FAILED(ID3D10Device_CreateTexture2D(dev, &d, NULL, &rt->tex))) return 0;
    d.Usage = D3D10_USAGE_STAGING; d.BindFlags = 0; d.CPUAccessFlags = D3D10_CPU_ACCESS_READ;
    if (FAILED(ID3D10Device_CreateTexture2D(dev, &d, NULL, &rt->staging))) return 0;
    if (FAILED(ID3D10Device_CreateRenderTargetView(dev, (ID3D10Resource *)rt->tex, NULL, &rt->rtv))) return 0;
    ID3D10Device_OMSetRenderTargets(dev, 1, &rt->rtv, NULL);
    ID3D10Device_RSSetViewports(dev, 1, &vp);
    ID3D10Device_ClearRenderTargetView(dev, rt->rtv, clear);
    rt->w = w; rt->h = h;
    return 1;
}

static DWORD rt_pixel(ID3D10Device *dev, struct rt *rt, UINT x, UINT y)
{
    D3D10_MAPPED_TEXTURE2D m;
    DWORD v = 0xdeadbeef;
    ID3D10Device_CopyResource(dev, (ID3D10Resource *)rt->staging, (ID3D10Resource *)rt->tex);
    if (SUCCEEDED(ID3D10Texture2D_Map(rt->staging, 0, D3D10_MAP_READ, 0, &m)))
    {
        v = ((DWORD *)((BYTE *)m.pData + y * m.RowPitch))[x];
        ID3D10Texture2D_Unmap(rt->staging, 0);
    }
    return v;
}

static void rt_free(struct rt *rt)
{
    if (rt->rtv) ID3D10RenderTargetView_Release(rt->rtv);
    if (rt->staging) ID3D10Texture2D_Release(rt->staging);
    if (rt->tex) ID3D10Texture2D_Release(rt->tex);
}

static ID3D10ShaderResourceView *solid_srv(ID3D10Device *dev, DWORD color)
{
    D3D10_TEXTURE2D_DESC d = {0};
    D3D10_SUBRESOURCE_DATA sd;
    DWORD px[16];
    ID3D10Texture2D *t;
    ID3D10ShaderResourceView *v = NULL;
    int i;

    for (i = 0; i < 16; ++i) px[i] = color;
    d.Width = 4; d.Height = 4; d.MipLevels = 1; d.ArraySize = 1; d.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    d.SampleDesc.Count = 1; d.Usage = D3D10_USAGE_DEFAULT; d.BindFlags = D3D10_BIND_SHADER_RESOURCE;
    sd.pSysMem = px; sd.SysMemPitch = 16; sd.SysMemSlicePitch = 0;
    if (FAILED(ID3D10Device_CreateTexture2D(dev, &d, &sd, &t))) return NULL;
    ID3D10Device_CreateShaderResourceView(dev, (ID3D10Resource *)t, NULL, &v);
    ID3D10Texture2D_Release(t);
    return v;
}

typedef struct { float m[16]; } MAT;
typedef struct { MAT world; float tc[2], ts[2], color[4]; ID3D10ShaderResourceView *tex; UINT index; } SPRITE;

static MAT mat_id(void) { MAT m = {{1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1}}; return m; }

static void test_sprite(ID3D10Device *dev, HMODULE x10)
{
    HRESULT (WINAPI *create)(ID3D10Device *, UINT, void **) = (void *)GetProcAddress(x10, "D3DX10CreateSprite");
    void *sprite = NULL;
    struct rt rt;
    ID3D10ShaderResourceView *red, *green;
    SPRITE s[2];
    MAT proj = mat_id();
    HRESULT hr;
    DWORD p;
    ULONG ref, ref2;

    if (!create || FAILED(create(dev, 0, &sprite))) { check(0, "sprite created"); return; }
    if (!rt_create(dev, &rt, 64, 64)) { check(0, "sprite render target"); return; }
    red = solid_srv(dev, 0xff0000ff);   /* ABGR: red */
    green = solid_srv(dev, 0xff00ff00);

    /* pixel space -> clip space, y down */
    proj.m[0] = 2.0f / 64; proj.m[5] = -2.0f / 64; proj.m[12] = -1.0f; proj.m[13] = 1.0f;
    hr = CALL(sprite, 11, HRESULT (WINAPI *)(void *, MAT *), &proj);
    check(hr == S_OK, "sprite SetProjectionTransform");

    memset(s, 0, sizeof(s));
    s[0].world = mat_id(); s[0].world.m[0] = 32; s[0].world.m[5] = 32; s[0].world.m[12] = 16; s[0].world.m[13] = 16;
    s[0].ts[0] = s[0].ts[1] = 1.0f; s[0].color[0] = s[0].color[1] = s[0].color[2] = s[0].color[3] = 1.0f;
    s[0].tex = red;
    s[1] = s[0];
    s[1].world.m[0] = 8; s[1].world.m[5] = 8; s[1].world.m[12] = 0; s[1].world.m[13] = 0;
    s[1].tex = green;

    hr = CALL(sprite, 3, HRESULT (WINAPI *)(void *, UINT), 0);
    check(hr == S_OK, "sprite Begin");
    hr = CALL(sprite, 3, HRESULT (WINAPI *)(void *, UINT), 0);
    check(hr == E_FAIL, "sprite Begin twice fails");
    hr = CALL(sprite, 4, HRESULT (WINAPI *)(void *, SPRITE *, UINT), s, 2);
    check(hr == S_OK, "sprite DrawSpritesBuffered");
    hr = CALL(sprite, 7, HRESULT (WINAPI *)(void *));
    check(hr == S_OK, "sprite End draws");
    p = rt_pixel(dev, &rt, 32, 32);
    check(p == 0xff0000ff, "sprite red quad centre");
    p = rt_pixel(dev, &rt, 4, 4);
    check(p == 0xff00ff00, "sprite green quad corner");
    p = rt_pixel(dev, &rt, 60, 60);
    check(p == 0xff000000, "sprite outside is background");
    p = rt_pixel(dev, &rt, 12, 20);
    check(p == 0xff000000, "sprite gap between quads");

    /* colour modulate and alpha */
    {
        float clear[4] = {0, 0, 0, 1};
        ID3D10Device_ClearRenderTargetView(dev, rt.rtv, clear);
        s[0].color[1] = 1.0f; s[0].color[0] = 0.5f; s[0].color[3] = 1.0f;
        CALL(sprite, 3, HRESULT (WINAPI *)(void *, UINT), 0);
        CALL(sprite, 4, HRESULT (WINAPI *)(void *, SPRITE *, UINT), s, 1);
        CALL(sprite, 7, HRESULT (WINAPI *)(void *));
        p = rt_pixel(dev, &rt, 32, 32);
        check((p & 0xff) >= 0x7e && (p & 0xff) <= 0x81, "sprite ColorModulate scales red");
    }

    /* depth sorting: back to front keeps the nearer sprite on top */
    {
        float clear[4] = {0, 0, 0, 1};
        SPRITE d[2] = { s[0], s[0] };
        ID3D10Device_ClearRenderTargetView(dev, rt.rtv, clear);
        d[0].color[0] = 1.0f; d[0].tex = red;   d[0].world.m[14] = 0.1f;
        d[1].color[0] = 1.0f; d[1].tex = green; d[1].world.m[14] = 0.9f;
        CALL(sprite, 3, HRESULT (WINAPI *)(void *, UINT), 0x02);
        CALL(sprite, 4, HRESULT (WINAPI *)(void *, SPRITE *, UINT), d, 2);
        CALL(sprite, 7, HRESULT (WINAPI *)(void *));
        p = rt_pixel(dev, &rt, 32, 32);
        check(p == 0xff0000ff, "sprite back-to-front sort draws the nearer sprite last");
        ID3D10Device_ClearRenderTargetView(dev, rt.rtv, clear);
        CALL(sprite, 3, HRESULT (WINAPI *)(void *, UINT), 0x04);
        CALL(sprite, 4, HRESULT (WINAPI *)(void *, SPRITE *, UINT), d, 2);
        CALL(sprite, 7, HRESULT (WINAPI *)(void *));
        p = rt_pixel(dev, &rt, 32, 32);
        check(p == 0xff00ff00, "sprite front-to-back sort draws the farther sprite last");
    }

    /* immediate drawing */
    {
        float clear[4] = {0, 0, 0, 1};
        ID3D10Device_ClearRenderTargetView(dev, rt.rtv, clear);
        hr = CALL(sprite, 6, HRESULT (WINAPI *)(void *, SPRITE *, UINT, UINT, UINT), &s[1], 1, sizeof(SPRITE), 0);
        check(hr == S_OK, "sprite DrawSpritesImmediate");
        p = rt_pixel(dev, &rt, 4, 4);
        check(p == 0xff00ff00, "sprite immediate drawn");
    }

    /* AddRef flag */
    ref = ID3D10ShaderResourceView_AddRef(red) - 1; ID3D10ShaderResourceView_Release(red);
    CALL(sprite, 3, HRESULT (WINAPI *)(void *, UINT), 0x10);
    CALL(sprite, 4, HRESULT (WINAPI *)(void *, SPRITE *, UINT), s, 1);
    ref2 = ID3D10ShaderResourceView_AddRef(red) - 1; ID3D10ShaderResourceView_Release(red);
    check(ref2 == ref + 1, "sprite holds a reference to buffered textures");
    CALL(sprite, 5, HRESULT (WINAPI *)(void *));
    ref2 = ID3D10ShaderResourceView_AddRef(red) - 1; ID3D10ShaderResourceView_Release(red);
    check(ref2 == ref, "sprite Flush releases the texture reference");
    CALL(sprite, 7, HRESULT (WINAPI *)(void *));

    /* save state */
    {
        ID3D10VertexShader *before = NULL, *after = NULL;
        ID3D10Device_VSGetShader(dev, &before);
        CALL(sprite, 3, HRESULT (WINAPI *)(void *, UINT), 0x08);
        CALL(sprite, 4, HRESULT (WINAPI *)(void *, SPRITE *, UINT), s, 1);
        CALL(sprite, 7, HRESULT (WINAPI *)(void *));
        ID3D10Device_VSGetShader(dev, &after);
        check(before == after, "sprite SAVE_STATE restores the vertex shader");
        if (before) ID3D10VertexShader_Release(before);
        if (after) ID3D10VertexShader_Release(after);
    }

    ID3D10ShaderResourceView_Release(red);
    ID3D10ShaderResourceView_Release(green);
    rt_free(&rt);
    unk_release(sprite);
}

typedef struct { float p[3]; } V3;
typedef struct { UINT face; float u, v, dist; } HIT;
typedef struct { UINT id, fs, fc, vs, vc; } RANGE;
typedef HRESULT (WINAPI *BUFMAP)(void *, void **, SIZE_T *);

static void test_mesh(ID3D10Device *dev, HMODULE x10)
{
    HRESULT (WINAPI *create)(ID3D10Device *, const D3D10_INPUT_ELEMENT_DESC *, UINT, const char *, UINT, UINT, UINT, void **) =
            (void *)GetProcAddress(x10, "D3DX10CreateMesh");
    D3D10_INPUT_ELEMENT_DESC decl[2] =
    {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D10_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 1, 0, D3D10_INPUT_PER_VERTEX_DATA, 0},
    };
    V3 pos[4] = {{{0,0,0}}, {{1,0,0}}, {{0,1,0}}, {{1,1,0}}};
    float uv[4][2] = {{0,0},{1,0},{0,1},{1,1}};
    WORD idx[6] = {0, 1, 2, 2, 1, 3};
    UINT attrs[2] = {1, 0};
    void *mesh = NULL, *buf = NULL, *clone = NULL;
    const D3D10_INPUT_ELEMENT_DESC *got;
    UINT count, i, adj[6], face_remap[2], hits, face;
    float u, v, dist;
    V3 ray_pos = {{0.75f, 0.5f, 1.0f}}, ray_dir = {{0, 0, -1}};
    void *data; SIZE_T size;
    ID3D10Blob *blob = NULL;
    HRESULT hr;

    if (!create) { check(0, "D3DX10CreateMesh exported"); return; }
    hr = create(dev, decl, 2, "POSITION", 4, 2, 0, &mesh);
    check(hr == S_OK && mesh, "mesh created");
    if (!mesh) return;
    check(CALL(mesh, 3, UINT (WINAPI *)(void *)) == 2, "mesh face count");
    check(CALL(mesh, 4, UINT (WINAPI *)(void *)) == 4, "mesh vertex count");
    check(CALL(mesh, 5, UINT (WINAPI *)(void *)) == 2, "mesh vertex buffer count (two slots)");
    check(CALL(mesh, 6, UINT (WINAPI *)(void *)) == 0, "mesh flags");
    hr = CALL(mesh, 7, HRESULT (WINAPI *)(void *, const D3D10_INPUT_ELEMENT_DESC **, UINT *), &got, &count);
    check(hr == S_OK && count == 2 && !strcmp(got[1].SemanticName, "TEXCOORD") && got[1].InputSlot == 1, "mesh vertex description");

    check(create(NULL, decl, 2, "POSITION", 4, 2, 0, &clone) == E_INVALIDARG, "mesh create rejects NULL device");
    check(create(dev, decl, 2, "POSITION", 70000, 2, 0, &clone) == E_INVALIDARG, "mesh create rejects 16 bit with 70000 vertices");

    hr = CALL(mesh, 8, HRESULT (WINAPI *)(void *, UINT, const void *), 0, pos);
    check(hr == S_OK, "mesh SetVertexData 0");
    hr = CALL(mesh, 8, HRESULT (WINAPI *)(void *, UINT, const void *), 1, uv);
    check(hr == S_OK, "mesh SetVertexData 1");
    check(CALL(mesh, 8, HRESULT (WINAPI *)(void *, UINT, const void *), 2, uv) == E_FAIL, "mesh SetVertexData bad slot");
    hr = CALL(mesh, 10, HRESULT (WINAPI *)(void *, const void *, UINT), idx, 6);
    check(hr == S_OK, "mesh SetIndexData");

    hr = CALL(mesh, 9, HRESULT (WINAPI *)(void *, UINT, void **), 1, &buf);
    check(hr == S_OK && buf, "mesh GetVertexBuffer");
    if (buf)
    {
        hr = CALL(buf, 3, BUFMAP, &data, &size);
        check(hr == S_OK && size == sizeof(uv) && !memcmp(data, uv, sizeof(uv)), "mesh buffer Map returns the vertex data");
        check(CALL(buf, 5, SIZE_T (WINAPI *)(void *)) == sizeof(uv), "mesh buffer GetSize");
        CALL(buf, 4, HRESULT (WINAPI *)(void *));
        unk_release(buf);
    }
    hr = CALL(mesh, 11, HRESULT (WINAPI *)(void *, void **), &buf);
    if (hr == S_OK) { CALL(buf, 3, BUFMAP, &data, &size); check(size == sizeof(idx) && !memcmp(data, idx, sizeof(idx)), "mesh index buffer"); unk_release(buf); }
    else check(0, "mesh GetIndexBuffer");

    /* adjacency and point reps */
    hr = CALL(mesh, 19, HRESULT (WINAPI *)(void *, void **), &buf);
    check(hr == E_FAIL, "mesh adjacency buffer absent before generation");
    hr = CALL(mesh, 16, HRESULT (WINAPI *)(void *, float), 0.0f);
    check(hr == S_OK, "mesh GenerateAdjacencyAndPointReps");
    hr = CALL(mesh, 19, HRESULT (WINAPI *)(void *, void **), &buf);
    if (hr == S_OK)
    {
        CALL(buf, 3, BUFMAP, &data, &size);
        memcpy(adj, data, sizeof(adj));
        /* face 0 (0,1,2): edge 1-2 shared with face 1; the other edges are open */
        check(adj[0] == 0xffffffff && adj[1] == 1 && adj[2] == 0xffffffff, "mesh adjacency of face 0");
        check(adj[3 + 0] == 0 && adj[3 + 1] == 0xffffffff && adj[3 + 2] == 0xffffffff, "mesh adjacency of face 1");
        unk_release(buf);
    }
    else check(0, "mesh GetAdjacencyBuffer");

    /* intersection */
    hits = 99;
    hr = CALL(mesh, 26, HRESULT (WINAPI *)(void *, V3 *, V3 *, UINT *, UINT *, float *, float *, float *, ID3D10Blob **),
            &ray_pos, &ray_dir, &hits, &face, &u, &v, &dist, &blob);
    check(hr == S_OK && hits == 1 && face == 1 && fabsf(dist - 1.0f) < 1e-4f, "mesh Intersect finds face 1");
    check(blob && ID3D10Blob_GetBufferSize(blob) == sizeof(HIT), "mesh Intersect returns the hit blob");
    if (blob) ID3D10Blob_Release(blob);
    ray_pos.p[0] = 5.0f;
    hr = CALL(mesh, 26, HRESULT (WINAPI *)(void *, V3 *, V3 *, UINT *, UINT *, float *, float *, float *, ID3D10Blob **),
            &ray_pos, &ray_dir, &hits, &face, &u, &v, &dist, NULL);
    check(hr == S_OK && hits == 0, "mesh Intersect misses");

    /* attribute sorting */
    hr = CALL(mesh, 12, HRESULT (WINAPI *)(void *, const UINT *), attrs);
    check(hr == S_OK, "mesh SetAttributeData");
    hr = CALL(mesh, 24, HRESULT (WINAPI *)(void *, UINT, UINT *, ID3D10Blob **), 0x02000000, face_remap, NULL);
    check(hr == S_OK && face_remap[0] == 1 && face_remap[1] == 0, "mesh Optimize ATTR_SORT reorders faces");
    {
        RANGE tbl[4]; UINT n = 4;
        hr = CALL(mesh, 15, HRESULT (WINAPI *)(void *, RANGE *, UINT *), tbl, &n);
        check(hr == S_OK && n == 2 && tbl[0].id == 0 && tbl[0].fs == 0 && tbl[0].fc == 1
                && tbl[1].id == 1 && tbl[1].fs == 1 && tbl[1].fc == 1, "mesh Optimize builds the attribute table");
        n = 0;
        hr = CALL(mesh, 15, HRESULT (WINAPI *)(void *, RANGE *, UINT *), NULL, &n);
        check(hr == S_OK && n == 2, "mesh GetAttributeTable count query");
    }
    hr = CALL(mesh, 11, HRESULT (WINAPI *)(void *, void **), &buf);
    if (hr == S_OK)
    {
        WORD *w;
        CALL(buf, 3, BUFMAP, &data, &size);
        w = data;
        check(w[0] == 2 && w[1] == 1 && w[2] == 3 && w[3] == 0 && w[4] == 1 && w[5] == 2, "mesh indices follow the face remap");
        unk_release(buf);
    }
    /* compaction drops unused vertices */
    {
        void *m2 = NULL;
        ID3D10Blob *vr = NULL;
        WORD idx2[3] = {1, 3, 2};
        hr = create(dev, decl, 2, "POSITION", 4, 1, 0, &m2);
        CALL(m2, 8, HRESULT (WINAPI *)(void *, UINT, const void *), 0, pos);
        CALL(m2, 8, HRESULT (WINAPI *)(void *, UINT, const void *), 1, uv);
        CALL(m2, 10, HRESULT (WINAPI *)(void *, const void *, UINT), idx2, 3);
        hr = CALL(m2, 24, HRESULT (WINAPI *)(void *, UINT, UINT *, ID3D10Blob **), 0x01000000, face_remap, &vr);
        check(hr == S_OK && CALL(m2, 4, UINT (WINAPI *)(void *)) == 3, "mesh Optimize COMPACT removes the unused vertex");
        if (vr)
        {
            UINT *r = ID3D10Blob_GetBufferPointer(vr);
            check(ID3D10Blob_GetBufferSize(vr) == 12 && r[0] == 1 && r[1] == 3 && r[2] == 2, "mesh vertex remap lists old indices");
            ID3D10Blob_Release(vr);
        }
        else check(0, "mesh vertex remap blob");
        unk_release(m2);
    }

    /* clone with a different layout */
    {
        D3D10_INPUT_ELEMENT_DESC decl2[2] =
        {
            {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D10_INPUT_PER_VERTEX_DATA, 0},
            {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D10_INPUT_PER_VERTEX_DATA, 0},
        };
        hr = CALL(mesh, 23, HRESULT (WINAPI *)(void *, UINT, const char *, const D3D10_INPUT_ELEMENT_DESC *, UINT, void **),
                1, "POSITION", decl2, 2, &clone);
        check(hr == S_OK && clone, "mesh CloneMesh to a 32 bit single slot layout");
        if (clone)
        {
            check(CALL(clone, 6, UINT (WINAPI *)(void *)) == 1, "clone has the 32 bit flag");
            check(CALL(clone, 5, UINT (WINAPI *)(void *)) == 1, "clone has one vertex buffer");
            hr = CALL(clone, 9, HRESULT (WINAPI *)(void *, UINT, void **), 0, &buf);
            if (hr == S_OK)
            {
                float *f; UINT *ix;
                CALL(buf, 3, BUFMAP, &data, &size);
                f = data;
                check(size == 4 * 24 && f[0] == 0 && f[6] == 1 && f[9] == 0 && f[10] == 0, "clone copies positions and zeroes the new normal");
                unk_release(buf);
                CALL(clone, 11, HRESULT (WINAPI *)(void *, void **), &buf);
                CALL(buf, 3, BUFMAP, &data, &size);
                ix = data;
                check(size == 24 && ix[0] == 2 && ix[1] == 1 && ix[2] == 3, "clone widens the indices");
                unk_release(buf);
            }
            unk_release(clone);
        }
    }

    /* device buffers */
    hr = CALL(mesh, 28, HRESULT (WINAPI *)(void *));
    check(hr == S_OK, "mesh CommitToDevice");
    {
        ID3D10Buffer *vb = NULL, *ib = NULL;
        D3D10_BUFFER_DESC bd;
        hr = CALL(mesh, 31, HRESULT (WINAPI *)(void *, UINT, ID3D10Buffer **), 0, &vb);
        check(hr == S_OK && vb, "mesh GetDeviceVertexBuffer");
        if (vb) { ID3D10Buffer_GetDesc(vb, &bd); check(bd.ByteWidth == 48 && (bd.BindFlags & D3D10_BIND_VERTEX_BUFFER), "device vertex buffer size"); ID3D10Buffer_Release(vb); }
        hr = CALL(mesh, 32, HRESULT (WINAPI *)(void *, ID3D10Buffer **), &ib);
        check(hr == S_OK && ib, "mesh GetDeviceIndexBuffer");
        if (ib) { ID3D10Buffer_GetDesc(ib, &bd); check(bd.ByteWidth == 12, "device index buffer size"); ID3D10Buffer_Release(ib); }
    }
    hr = CALL(mesh, 29, HRESULT (WINAPI *)(void *, UINT), 1);
    check(hr == S_OK, "mesh DrawSubset existing subset");
    hr = CALL(mesh, 29, HRESULT (WINAPI *)(void *, UINT), 7);
    check(hr == E_FAIL, "mesh DrawSubset missing subset");

    /* discard */
    hr = CALL(mesh, 22, HRESULT (WINAPI *)(void *, UINT), 0x1f);
    check(hr == S_OK, "mesh Discard");
    hr = CALL(mesh, 19, HRESULT (WINAPI *)(void *, void **), &buf);
    check(hr == E_FAIL, "mesh adjacency gone after Discard");
    hr = CALL(mesh, 32, HRESULT (WINAPI *)(void *, ID3D10Buffer **), (ID3D10Buffer **)&buf);
    check(hr == E_FAIL, "mesh device buffers gone after Discard");

    /* geometry shader adjacency */
    {
        void *m3 = NULL;
        hr = create(dev, decl, 2, "POSITION", 4, 2, 4, &m3);
        CALL(m3, 8, HRESULT (WINAPI *)(void *, UINT, const void *), 0, pos);
        CALL(m3, 10, HRESULT (WINAPI *)(void *, const void *, UINT), idx, 6);
        hr = CALL(m3, 17, HRESULT (WINAPI *)(void *));
        check(hr == E_FAIL, "mesh GenerateGSAdjacency needs adjacency first");
        CALL(m3, 16, HRESULT (WINAPI *)(void *, float), 0.0f);
        hr = CALL(m3, 17, HRESULT (WINAPI *)(void *));
        check(hr == S_OK, "mesh GenerateGSAdjacency");
        hr = CALL(m3, 11, HRESULT (WINAPI *)(void *, void **), &buf);
        if (hr == S_OK)
        {
            WORD *w;
            CALL(buf, 3, BUFMAP, &data, &size);
            w = data;
            check(size == 24 && w[0] == 0 && w[2] == 1 && w[4] == 2 && w[3] == 3, "GS adjacency index list: vertices and the vertex across the shared edge");
            unk_release(buf);
        }
        unk_release(m3);
        hr = create(dev, decl, 2, "POSITION", 4, 2, 0, &m3);
        hr = CALL(m3, 17, HRESULT (WINAPI *)(void *));
        check(hr == E_FAIL, "mesh GenerateGSAdjacency needs the GS flag");
        unk_release(m3);
    }
    unk_release(mesh);
}

typedef struct { float r, g, b, a; } COLORF;

static void test_font(ID3D10Device *dev, HMODULE x10)
{
    HRESULT (WINAPI *create)(ID3D10Device *, INT, UINT, UINT, UINT, BOOL, UINT, UINT, UINT, UINT, const char *, void **) =
            (void *)GetProcAddress(x10, "D3DX10CreateFontA");
    HRESULT (WINAPI *create_sprite)(ID3D10Device *, UINT, void **) = (void *)GetProcAddress(x10, "D3DX10CreateSprite");
    void *font = NULL, *sprite = NULL;
    ID3D10ShaderResourceView *srv = NULL;
    RECT bb, r = {0, 0, 64, 32};
    POINT inc;
    struct rt rt;
    MAT proj = mat_id();
    COLORF white = {1, 1, 1, 1};
    INT h;
    int x, y, lit = 0;
    HRESULT hr;

    if (!create || FAILED(create(dev, 16, 0, 400, 0, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, DEFAULT_QUALITY,
            DEFAULT_PITCH, "Tahoma", &font)))
    { check(0, "font created"); return; }

    hr = CALL(font, 9, HRESULT (WINAPI *)(void *, UINT, ID3D10ShaderResourceView **, RECT *, POINT *), 0x23, &srv, &bb, &inc);
    check(hr == S_OK && srv, "font GetGlyphData returns a view");
    check(bb.right > bb.left && bb.bottom > bb.top, "font glyph black box has an area");
    if (srv)
    {
        ID3D10Resource *res; D3D10_TEXTURE2D_DESC d;
        ID3D10ShaderResourceView_GetResource(srv, &res);
        ID3D10Texture2D_GetDesc((ID3D10Texture2D *)res, &d);
        check(d.Format == DXGI_FORMAT_R8G8B8A8_UNORM && d.Width == 256 && d.MipLevels == 5, "font atlas format, size and mip levels");
        ID3D10Resource_Release(res);
        ID3D10ShaderResourceView_Release(srv);
    }
    hr = CALL(font, 9, HRESULT (WINAPI *)(void *, UINT, ID3D10ShaderResourceView **, RECT *, POINT *), 1, &srv, &bb, &inc);
    check(hr == S_OK && !srv, "font glyph without outline has no view");
    hr = CALL(font, 11, HRESULT (WINAPI *)(void *, UINT, UINT), 0, 1000);
    check(hr == S_OK, "font PreloadGlyphs spans several atlas textures");

    /* DrawText through a sprite renders pixels */
    if (!rt_create(dev, &rt, 64, 32)) { check(0, "font render target"); return; }
    proj.m[0] = 2.0f / 64; proj.m[5] = -2.0f / 32; proj.m[12] = -1.0f; proj.m[13] = 1.0f;
    create_sprite(dev, 0, &sprite);
    CALL(sprite, 11, HRESULT (WINAPI *)(void *, MAT *), &proj);
    CALL(sprite, 3, HRESULT (WINAPI *)(void *, UINT), 0);
    h = CALL(font, 14, INT (WINAPI *)(void *, void *, const char *, INT, RECT *, UINT, COLORF), sprite, "HI", -1, &r, DT_TOP, white);
    check(h == 16, "font DrawTextA returns the line height");
    CALL(sprite, 7, HRESULT (WINAPI *)(void *));
    for (y = 0; y < 32; ++y)
        for (x = 0; x < 64; ++x)
            if ((rt_pixel(dev, &rt, x, y) & 0xffffff) > 0x808080) lit++;
    check(lit > 10, "font text drew white pixels into the target");
    check((rt_pixel(dev, &rt, 60, 30) & 0xffffff) == 0, "font text left the far corner alone");

    r.left = r.top = 0; r.right = r.bottom = 0;
    h = CALL(font, 14, INT (WINAPI *)(void *, void *, const char *, INT, RECT *, UINT, COLORF), sprite, "HI\nHI", -1, &r, DT_CALCRECT, white);
    check(h == 32 && r.right > 0 && r.bottom == 32, "font DT_CALCRECT measures two lines");
    h = CALL(font, 14, INT (WINAPI *)(void *, void *, const char *, INT, RECT *, UINT, COLORF), NULL, "HI", -1, &r, DT_TOP, white);
    check(h == 16, "font DrawTextA without a sprite");

    unk_release(sprite);
    rt_free(&rt);
    unk_release(font);
}

struct load_info { UINT width, height, depth, first_mip, mips, usage, bind, cpu, misc, format, filter, mip_filter; void *src_info; };
struct image_info { UINT width, height, depth, array_size, mip_levels, misc_flags, format, dimension, file_format; };

static const unsigned char bmp2x2[] =
{
    'B','M', 70,0,0,0, 0,0,0,0, 54,0,0,0,
    40,0,0,0, 2,0,0,0, 2,0,0,0, 1,0, 24,0, 0,0,0,0, 16,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
    0,0,255, 0,0,255, 0,0,  0,0,255, 0,0,255, 0,0
};

static void test_misc(ID3D10Device *dev, HMODULE x10)
{
    HRESULT (WINAPI *create_pump)(UINT, UINT, void **) = (void *)GetProcAddress(x10, "D3DX10CreateThreadPump");
    HRESULT (WINAPI *compile)(const char *, SIZE_T, const char *, const void *, void *, const char *, const char *, UINT, UINT,
            void *, ID3D10Blob **, ID3D10Blob **, HRESULT *) = (void *)GetProcAddress(x10, "D3DX10CompileFromMemory");
    HRESULT (WINAPI *preprocess)(const char *, SIZE_T, const char *, const void *, void *, void *, ID3D10Blob **, ID3D10Blob **,
            HRESULT *) = (void *)GetProcAddress(x10, "D3DX10PreprocessShaderFromMemory");
    HRESULT (WINAPI *srv_mem)(ID3D10Device *, const void *, SIZE_T, struct load_info *, void *, ID3D10ShaderResourceView **,
            HRESULT *) = (void *)GetProcAddress(x10, "D3DX10CreateShaderResourceViewFromMemory");
    HRESULT (WINAPI *tex_mem)(ID3D10Device *, const void *, SIZE_T, struct load_info *, void *, ID3D10Resource **,
            HRESULT *) = (void *)GetProcAddress(x10, "D3DX10CreateTextureFromMemory");
    HRESULT (WINAPI *filter_tex)(ID3D10Resource *, UINT, UINT) = (void *)GetProcAddress(x10, "D3DX10FilterTexture");
    HRESULT (WINAPI *load_tex)(ID3D10Resource *, void *, ID3D10Resource *) = (void *)GetProcAddress(x10, "D3DX10LoadTextureFromTexture");
    HRESULT (WINAPI *save_mem)(ID3D10Resource *, int, ID3D10Blob **, UINT) = (void *)GetProcAddress(x10, "D3DX10SaveTextureToMemory");
    HRESULT (WINAPI *info_mem)(const void *, SIZE_T, void *, struct image_info *, HRESULT *) = (void *)GetProcAddress(x10, "D3DX10GetImageInfoFromMemory");
    HRESULT (WINAPI *effect_mem)(const void *, SIZE_T, const char *, const void *, void *, const char *, UINT, UINT, ID3D10Device *,
            void *, void *, void **, ID3D10Blob **, HRESULT *) = (void *)GetProcAddress(x10, "D3DX10CreateEffectFromMemory");
    static const char hlsl[] = "float4 main(float4 p : SV_POSITION) : SV_TARGET { return float4(1, 0, 0, 1); }";
    static const char macro_src[] = "#define VALUE 42\nint x = VALUE;\n";
    void *pump = NULL;
    ID3D10Blob *shader = NULL, *errors = NULL, *text = NULL;
    HRESULT hr, res = 0x12345678;

    /* thread pump driven work: the answer arrives after the pump ran the device stage */
    check(create_pump && SUCCEEDED(create_pump(0, 0, &pump)), "thread pump created");
    if (!pump) return;

    hr = compile(hlsl, sizeof(hlsl) - 1, "pump.hlsl", NULL, NULL, "main", "ps_4_0", 0, 0, pump, &shader, &errors, &res);
    check(hr == S_OK, "CompileFromMemory with a pump queues the work");
    hr = CALL(pump, 5, HRESULT (WINAPI *)(void *));
    check(hr == S_OK && shader && res == S_OK && ID3D10Blob_GetBufferSize(shader) > 0, "pumped compile delivers the byte code");
    if (shader) ID3D10Blob_Release(shader);
    shader = NULL;

    res = 0x12345678;
    hr = compile("float4 main( : SV_TARGET { }", 29, "bad.hlsl", NULL, NULL, "main", "ps_4_0", 0, 0, pump, &shader, &errors, &res);
    CALL(pump, 5, HRESULT (WINAPI *)(void *));
    check(hr == S_OK && !shader && FAILED(res) && res != 0x12345678, "pumped compile of broken source reports the error through hresult");
    if (errors) { check(ID3D10Blob_GetBufferSize(errors) > 1, "pumped compile errors blob"); ID3D10Blob_Release(errors); errors = NULL; }
    else check(0, "pumped compile errors blob");

    res = 0x12345678;
    hr = preprocess(macro_src, sizeof(macro_src) - 1, "m.hlsl", NULL, NULL, pump, &text, &errors, &res);
    CALL(pump, 5, HRESULT (WINAPI *)(void *));
    check(hr == S_OK && text && res == S_OK && strstr(ID3D10Blob_GetBufferPointer(text), "int x = 42"), "pumped preprocess expands the macro");
    if (text) ID3D10Blob_Release(text);

    {
        ID3D10ShaderResourceView *view = NULL;
        res = 0x12345678;
        hr = srv_mem(dev, bmp2x2, sizeof(bmp2x2), NULL, pump, &view, &res);
        check(hr == S_OK, "CreateShaderResourceViewFromMemory with a pump queues the work");
        CALL(pump, 5, HRESULT (WINAPI *)(void *));
        check(view && res == S_OK, "pumped shader resource view is created");
        if (view) ID3D10ShaderResourceView_Release(view);
    }
    {
        ID3D10Effect *effect = NULL;
        static const char fx[] = "cbuffer cb { float f; }\n";
        res = 0x12345678;
        hr = effect_mem(fx, sizeof(fx) - 1, "e.fx", NULL, NULL, "fx_4_0", 0, 0, dev, NULL, pump, (void **)&effect, &errors, &res);
        CALL(pump, 5, HRESULT (WINAPI *)(void *));
        check(hr == S_OK && effect && res == S_OK, "pumped effect creation");
        if (effect) unk_release(effect);
        if (errors) { ID3D10Blob_Release(errors); errors = NULL; }
    }
    unk_release(pump);

    /* load info and mip chains */
    {
        ID3D10Resource *res_tex = NULL;
        ID3D10Texture2D *t2;
        D3D10_TEXTURE2D_DESC d;
        struct load_info li;
        struct image_info info;

        hr = tex_mem(dev, bmp2x2, sizeof(bmp2x2), NULL, NULL, &res_tex, NULL);
        check(hr == S_OK && res_tex, "texture from a BMP");
        if (res_tex)
        {
            ID3D10Resource_QueryInterface(res_tex, &IID_ID3D10Texture2D, (void **)&t2);
            ID3D10Texture2D_GetDesc(t2, &d);
            check(d.Width == 2 && d.Height == 2 && d.MipLevels == 2, "default load builds the full mip chain");
            ID3D10Texture2D_Release(t2);
            ID3D10Resource_Release(res_tex);
        }
        memset(&li, 0xff, sizeof(li));
        li.src_info = NULL;
        li.width = 8; li.height = 4; li.mips = 1; li.usage = D3D10_USAGE_DEFAULT; li.bind = D3D10_BIND_SHADER_RESOURCE | D3D10_BIND_RENDER_TARGET;
        li.format = DXGI_FORMAT_B8G8R8A8_UNORM;
        hr = tex_mem(dev, bmp2x2, sizeof(bmp2x2), &li, NULL, &res_tex, NULL);
        check(hr == S_OK && res_tex, "texture with explicit load info");
        if (res_tex)
        {
            D3D10_MAPPED_TEXTURE2D m;
            struct rt tmp;
            ID3D10Resource_QueryInterface(res_tex, &IID_ID3D10Texture2D, (void **)&t2);
            ID3D10Texture2D_GetDesc(t2, &d);
            check(d.Width == 8 && d.Height == 4 && d.MipLevels == 1 && d.Format == DXGI_FORMAT_B8G8R8A8_UNORM
                    && (d.BindFlags & D3D10_BIND_RENDER_TARGET), "load info size, mips, format and bind flags are honoured");
            memset(&tmp, 0, sizeof(tmp));
            /* read back the stretched image: all red (BGRA 0xffff0000) */
            {
                D3D10_TEXTURE2D_DESC sd = d;
                ID3D10Texture2D *st;
                sd.Usage = D3D10_USAGE_STAGING; sd.BindFlags = 0; sd.CPUAccessFlags = D3D10_CPU_ACCESS_READ;
                ID3D10Device_CreateTexture2D(dev, &sd, NULL, &st);
                ID3D10Device_CopyResource(dev, (ID3D10Resource *)st, res_tex);
                if (SUCCEEDED(ID3D10Texture2D_Map(st, 0, D3D10_MAP_READ, 0, &m)))
                {
                    check(((DWORD *)m.pData)[0] == 0xffff0000 && ((DWORD *)((BYTE *)m.pData + m.RowPitch))[7] == 0xffff0000,
                            "stretched image keeps its colour");
                    ID3D10Texture2D_Unmap(st, 0);
                }
                else check(0, "map stretched image");
                ID3D10Texture2D_Release(st);
            }
            ID3D10Texture2D_Release(t2);
            ID3D10Resource_Release(res_tex);
        }

        /* save and reload */
        {
            ID3D10ShaderResourceView *v = solid_srv(dev, 0xff00ff00);
            ID3D10Resource *r;
            ID3D10Blob *png = NULL, *dds = NULL;
            ID3D10ShaderResourceView_GetResource(v, &r);
            hr = save_mem(r, 3, &png, 0);
            check(hr == S_OK && png && !memcmp(ID3D10Blob_GetBufferPointer(png), "\x89PNG", 4), "SaveTextureToMemory writes a PNG");
            if (png && info_mem(ID3D10Blob_GetBufferPointer(png), ID3D10Blob_GetBufferSize(png), NULL, &info, NULL) == S_OK)
                check(info.width == 4 && info.height == 4, "the saved PNG has the texture size");
            else check(0, "saved PNG info");
            if (png) ID3D10Blob_Release(png);
            hr = save_mem(r, 4, &dds, 0);
            check(hr == S_OK && dds && !memcmp(ID3D10Blob_GetBufferPointer(dds), "DDS ", 4), "SaveTextureToMemory writes a DDS");
            if (dds && info_mem(ID3D10Blob_GetBufferPointer(dds), ID3D10Blob_GetBufferSize(dds), NULL, &info, NULL) == S_OK)
                check(info.width == 4 && info.format == DXGI_FORMAT_R8G8B8A8_UNORM && info.file_format == 4, "the saved DDS loads back");
            else check(0, "saved DDS info");
            if (dds) ID3D10Blob_Release(dds);
            ID3D10Resource_Release(r);
            ID3D10ShaderResourceView_Release(v);
        }

        /* FilterTexture and LoadTextureFromTexture */
        {
            D3D10_TEXTURE2D_DESC td = {0};
            ID3D10Texture2D *a, *b, *rb;
            DWORD px[16], blue[16];
            D3D10_SUBRESOURCE_DATA sd[3];
            D3D10_MAPPED_TEXTURE2D m;
            DWORD zero4[4] = {0}, one = 0;
            int i;
            for (i = 0; i < 16; ++i) { px[i] = 0xff0000ff; blue[i] = 0xffff0000; }
            td.Width = 4; td.Height = 4; td.MipLevels = 3; td.ArraySize = 1; td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            td.SampleDesc.Count = 1; td.Usage = D3D10_USAGE_DEFAULT; td.BindFlags = D3D10_BIND_SHADER_RESOURCE;
            sd[0].pSysMem = px; sd[0].SysMemPitch = 16; sd[0].SysMemSlicePitch = 0;
            sd[1].pSysMem = zero4; sd[1].SysMemPitch = 8; sd[1].SysMemSlicePitch = 0;
            sd[2].pSysMem = &one; sd[2].SysMemPitch = 4; sd[2].SysMemSlicePitch = 0;
            ID3D10Device_CreateTexture2D(dev, &td, sd, &a);
            hr = filter_tex((ID3D10Resource *)a, 0, 4);
            check(hr == S_OK, "D3DX10FilterTexture");
            td.Usage = D3D10_USAGE_STAGING; td.BindFlags = 0; td.CPUAccessFlags = D3D10_CPU_ACCESS_READ; td.MipLevels = 3;
            ID3D10Device_CreateTexture2D(dev, &td, NULL, &rb);
            ID3D10Device_CopyResource(dev, (ID3D10Resource *)rb, (ID3D10Resource *)a);
            if (SUCCEEDED(ID3D10Texture2D_Map(rb, 2, D3D10_MAP_READ, 0, &m)))
            {
                check(*(DWORD *)m.pData == 0xff0000ff, "the filtered 1x1 level is red");
                ID3D10Texture2D_Unmap(rb, 2);
            }
            else check(0, "map filtered level");
            ID3D10Texture2D_Release(rb);

            td.Width = 2; td.Height = 2; td.MipLevels = 1; td.Usage = D3D10_USAGE_DEFAULT; td.BindFlags = D3D10_BIND_SHADER_RESOURCE; td.CPUAccessFlags = 0;
            sd[0].pSysMem = blue; sd[0].SysMemPitch = 8;
            ID3D10Device_CreateTexture2D(dev, &td, sd, &b);
            hr = load_tex((ID3D10Resource *)a, NULL, (ID3D10Resource *)b);
            check(hr == S_OK, "D3DX10LoadTextureFromTexture");
            td.Usage = D3D10_USAGE_STAGING; td.BindFlags = 0; td.CPUAccessFlags = D3D10_CPU_ACCESS_READ;
            ID3D10Device_CreateTexture2D(dev, &td, NULL, &rb);
            ID3D10Device_CopyResource(dev, (ID3D10Resource *)rb, (ID3D10Resource *)b);
            if (SUCCEEDED(ID3D10Texture2D_Map(rb, 0, D3D10_MAP_READ, 0, &m)))
            {
                check(*(DWORD *)m.pData == 0xff0000ff, "the 4x4 red level was shrunk into the 2x2 target");
                ID3D10Texture2D_Unmap(rb, 0);
            }
            ID3D10Texture2D_Release(rb);
            ID3D10Texture2D_Release(a);
            ID3D10Texture2D_Release(b);
        }
    }
}

int main(void)
{
    HMODULE x10 = LoadLibraryA("d3dx10_43.dll");
    ID3D10Device *dev = make_device();

    if (!x10 || !dev) { check(0, "device and d3dx10_43"); printf("RESULT: FAIL\n"); return 1; }
    test_sprite(dev, x10);
    test_mesh(dev, x10);
    test_font(dev, x10);
    test_misc(dev, x10);

    ID3D10Device_Release(dev);
    printf("%d failures\n", failures);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
