/* d3dx10_43 / d3dx11_43 (patch 3680): loading, filtering, copying and saving 1D and 3D textures. */
#define COBJMACROS
#include <initguid.h>
#include <windows.h>
#include <d3d10.h>
#include <d3d11.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

struct image_info { UINT width, height, depth, array_size, mip_levels, misc_flags, format, dimension, file_format; };

/* a DDS with a DX10 header: dim 2 = 1D, 3 = 2D, 4 = 3D; RGBA8, no payload filled */
static void make_dds(BYTE *dds, UINT dim, UINT w, UINT h, UINT depth, UINT mips, UINT fmt)
{
    UINT hd[31] = {0}, dx10[5] = {fmt, dim, 0, 1, 0};

    *(UINT *)dds = 0x20534444;
    hd[0] = 124;
    hd[1] = 0x1007 | (dim == 4 ? 0x800000 : 0) | (mips > 1 ? 0x20000 : 0);
    hd[2] = h; hd[3] = w; hd[4] = w * 4; hd[5] = dim == 4 ? depth : 0; hd[6] = mips;
    hd[18] = 32; hd[19] = 4; hd[20] = 0x30315844;
    hd[26] = 0x1000; hd[27] = dim == 4 ? 0x200000 : 0;
    memcpy(dds + 4, hd, sizeof(hd));
    memcpy(dds + 128, dx10, sizeof(dx10));
}

static ID3D11DeviceContext *ctx11;
#define FILTER_ARGS10
#define CTX_ARG10
#define FILTER_ARGS11 ID3D11DeviceContext *,
#define CTX_ARG11 ctx11,

static ID3D10Device *make_device10(void)
{
    ID3D10Device *dev = NULL;
    HRESULT hr = D3D10CreateDevice(NULL, D3D10_DRIVER_TYPE_HARDWARE, NULL, 0, D3D10_SDK_VERSION, &dev);
    if (FAILED(hr)) hr = D3D10CreateDevice(NULL, D3D10_DRIVER_TYPE_WARP, NULL, 0, D3D10_SDK_VERSION, &dev);
    return FAILED(hr) ? NULL : dev;
}

static ID3D11Device *make_device11(void)
{
    HRESULT (WINAPI *create_device)(void *, int, HMODULE, UINT, const void *, UINT, UINT, ID3D11Device **, void *, ID3D11DeviceContext **);
    HMODULE m = LoadLibraryA("d3d11.dll");
    ID3D11Device *dev = NULL;
    HRESULT hr;

    if (!m) return NULL;
    create_device = (void *)GetProcAddress(m, "D3D11CreateDevice");
    hr = create_device(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, NULL, 0, D3D11_SDK_VERSION, &dev, NULL, &ctx11);
    if (FAILED(hr)) hr = create_device(NULL, D3D_DRIVER_TYPE_WARP, NULL, 0, NULL, 0, D3D11_SDK_VERSION, &dev, NULL, &ctx11);
    return FAILED(hr) ? NULL : dev;
}

#define COPY_SUB10(dev, st, res, mip) ID3D10Device_CopySubresourceRegion(dev, st, 0, 0, 0, 0, res, mip, NULL)
#define COPY_SUB11(dev, st, res, mip) ID3D11DeviceContext_CopySubresourceRegion(ctx11, st, 0, 0, 0, 0, res, mip, NULL)

static int MAP_READ10(ID3D10Device *dev, ID3D10Resource *st, const BYTE **p, UINT *rp, UINT *dp)
{
    D3D10_RESOURCE_DIMENSION dim;
    ID3D10Resource_GetType(st, &dim);
    if (dim == D3D10_RESOURCE_DIMENSION_TEXTURE1D)
    {
        void *data;
        if (FAILED(ID3D10Texture1D_Map((ID3D10Texture1D *)st, 0, D3D10_MAP_READ, 0, &data))) return 0;
        *p = data; *rp = *dp = 0;
        return 1;
    }
    else
    {
        D3D10_MAPPED_TEXTURE3D m;
        if (FAILED(ID3D10Texture3D_Map((ID3D10Texture3D *)st, 0, D3D10_MAP_READ, 0, &m))) return 0;
        *p = m.pData; *rp = m.RowPitch; *dp = m.DepthPitch;
        return 1;
    }
}
static void UNMAP10(ID3D10Device *dev, ID3D10Resource *st)
{
    D3D10_RESOURCE_DIMENSION dim;
    ID3D10Resource_GetType(st, &dim);
    if (dim == D3D10_RESOURCE_DIMENSION_TEXTURE1D) ID3D10Texture1D_Unmap((ID3D10Texture1D *)st, 0);
    else ID3D10Texture3D_Unmap((ID3D10Texture3D *)st, 0);
}
static int MAP_READ11(ID3D11Device *dev, ID3D11Resource *st, const BYTE **p, UINT *rp, UINT *dp)
{
    D3D11_MAPPED_SUBRESOURCE m;
    if (FAILED(ID3D11DeviceContext_Map(ctx11, st, 0, D3D11_MAP_READ, 0, &m))) return 0;
    *p = m.pData; *rp = m.RowPitch; *dp = m.DepthPitch;
    return 1;
}
static void UNMAP11(ID3D11Device *dev, ID3D11Resource *st) { ID3D11DeviceContext_Unmap(ctx11, st, 0); }

/* ---- D3D10 ---- */
static HMODULE x10;
struct ll10 { UINT Width, Height, Depth, FirstMipLevel, MipLevels, Usage, BindFlags, CpuAccessFlags, MiscFlags, Format, Filter, MipFilter; void *pSrcInfo; };
struct tl10 { D3D10_BOX *pSrcBox, *pDstBox; UINT SrcFirstMip, DstFirstMip, NumMips, SrcFirstElement, DstFirstElement, NumElements, Filter, MipFilter; };

static ID3D10Resource *make_tex10(ID3D10Device *dev, D3D10_RESOURCE_DIMENSION dim, UINT w, UINT h, UINT d, UINT mips, DXGI_FORMAT fmt, const BYTE *level0)
{
    D3D10_SUBRESOURCE_DATA sub[8];
    BYTE *zero[8];
    UINT i, bpp = 4;
    ID3D10Resource *res = NULL;
    HRESULT hr;

    for (i = 0; i < mips; ++i)
    {
        UINT lw = max(w >> i, 1), lh = max(h >> i, 1), ld = max(d >> i, 1);
        zero[i] = calloc(1, lw * lh * ld * bpp);
        if (!i && level0) memcpy(zero[0], level0, lw * lh * ld * bpp);
        sub[i].pSysMem = zero[i];
        sub[i].SysMemPitch = lw * bpp;
        sub[i].SysMemSlicePitch = lw * lh * bpp;
    }
    if (dim == D3D10_RESOURCE_DIMENSION_TEXTURE1D)
    {
        D3D10_TEXTURE1D_DESC desc = {0};
        ID3D10Texture1D *t;
        desc.Width = w; desc.MipLevels = mips; desc.ArraySize = 1; desc.Format = fmt; desc.Usage = D3D10_USAGE_DEFAULT; desc.BindFlags = D3D10_BIND_SHADER_RESOURCE;
        hr = ID3D10Device_CreateTexture1D(dev, &desc, sub, &t);
        if (SUCCEEDED(hr)) res = (ID3D10Resource *)t;
    }
    else if (dim == D3D10_RESOURCE_DIMENSION_TEXTURE3D)
    {
        D3D10_TEXTURE3D_DESC desc = {0};
        ID3D10Texture3D *t;
        desc.Width = w; desc.Height = h; desc.Depth = d; desc.MipLevels = mips; desc.Format = fmt; desc.Usage = D3D10_USAGE_DEFAULT; desc.BindFlags = D3D10_BIND_SHADER_RESOURCE;
        hr = ID3D10Device_CreateTexture3D(dev, &desc, sub, &t);
        if (SUCCEEDED(hr)) res = (ID3D10Resource *)t;
    }
    for (i = 0; i < mips; ++i) free(zero[i]);
    return res;
}

/* describes any texture: dim, w, h, d, mips, format */
static int tex_desc10(ID3D10Resource *res, D3D10_RESOURCE_DIMENSION *dim, UINT *w, UINT *h, UINT *d, UINT *mips, DXGI_FORMAT *fmt)
{
    void *t;
    ID3D10Resource_GetType(res, dim);
    if (*dim == D3D10_RESOURCE_DIMENSION_TEXTURE1D)
    {
        D3D10_TEXTURE1D_DESC desc;
        if (FAILED(ID3D10Resource_QueryInterface(res, &IID_ID3D10Texture1D, &t))) return 0;
        ID3D10Texture1D_GetDesc((ID3D10Texture1D *)t, &desc); ID3D10Texture1D_Release((ID3D10Texture1D *)t);
        *w = desc.Width; *h = *d = 1; *mips = desc.MipLevels; *fmt = desc.Format;
    }
    else if (*dim == D3D10_RESOURCE_DIMENSION_TEXTURE3D)
    {
        D3D10_TEXTURE3D_DESC desc;
        if (FAILED(ID3D10Resource_QueryInterface(res, &IID_ID3D10Texture3D, &t))) return 0;
        ID3D10Texture3D_GetDesc((ID3D10Texture3D *)t, &desc); ID3D10Texture3D_Release((ID3D10Texture3D *)t);
        *w = desc.Width; *h = desc.Height; *d = desc.Depth; *mips = desc.MipLevels; *fmt = desc.Format;
    }
    else
    {
        D3D10_TEXTURE2D_DESC desc;
        if (FAILED(ID3D10Resource_QueryInterface(res, &IID_ID3D10Texture2D, &t))) return 0;
        ID3D10Texture2D_GetDesc((ID3D10Texture2D *)t, &desc); ID3D10Texture2D_Release((ID3D10Texture2D *)t);
        *w = desc.Width; *h = desc.Height; *d = 1; *mips = desc.MipLevels; *fmt = desc.Format;
    }
    return 1;
}

/* bytes of one texel at (x,y,z) in a mip, via a staging copy */
static int texel10(ID3D10Device *dev, ID3D10Resource *res, UINT mip, UINT x, UINT y, UINT z, BYTE out[4])
{
    D3D10_RESOURCE_DIMENSION dim;
    UINT w, h, d, mips, lw, lh, ld;
    DXGI_FORMAT fmt;
    ID3D10Resource *st = NULL;
    const BYTE *p = NULL;
    UINT rp = 0, dp = 0;
    int ok = 0;

    if (!tex_desc10(res, &dim, &w, &h, &d, &mips, &fmt)) return 0;
    lw = max(w >> mip, 1); lh = max(h >> mip, 1); ld = max(d >> mip, 1);
    if (dim == D3D10_RESOURCE_DIMENSION_TEXTURE1D)
    {
        D3D10_TEXTURE1D_DESC desc = {0}; ID3D10Texture1D *t;
        desc.Width = lw; desc.MipLevels = 1; desc.ArraySize = 1; desc.Format = fmt; desc.Usage = D3D10_USAGE_STAGING; desc.CPUAccessFlags = D3D10_CPU_ACCESS_READ;
        if (FAILED(ID3D10Device_CreateTexture1D(dev, &desc, NULL, &t))) return 0;
        st = (ID3D10Resource *)t;
    }
    else if (dim == D3D10_RESOURCE_DIMENSION_TEXTURE3D)
    {
        D3D10_TEXTURE3D_DESC desc = {0}; ID3D10Texture3D *t;
        desc.Width = lw; desc.Height = lh; desc.Depth = ld; desc.MipLevels = 1; desc.Format = fmt; desc.Usage = D3D10_USAGE_STAGING; desc.CPUAccessFlags = D3D10_CPU_ACCESS_READ;
        if (FAILED(ID3D10Device_CreateTexture3D(dev, &desc, NULL, &t))) return 0;
        st = (ID3D10Resource *)t;
    }
    else return 0;
    COPY_SUB10(dev, st, res, mip);
    ok = MAP_READ10(dev, st, &p, &rp, &dp);
    if (ok)
    {
        memcpy(out, p + z * dp + y * rp + x * 4, 4);
        UNMAP10(dev, st);
    }
    ID3D10Resource_Release(st);
    return ok;
}

static void test_tex10(void)
{
    ID3D10Device *dev = make_device10();
    HRESULT (WINAPI *create)(ID3D10Device *, const void *, SIZE_T, void *, void *, ID3D10Resource **, HRESULT *);
    HRESULT (WINAPI *filter)(FILTER_ARGS10 ID3D10Resource *, UINT, UINT);
    HRESULT (WINAPI *load_tex)(FILTER_ARGS10 ID3D10Resource *, struct tl10 *, ID3D10Resource *);
    HRESULT (WINAPI *save)(FILTER_ARGS10 ID3D10Resource *, UINT, ID3D10Blob **, UINT);
    HRESULT (WINAPI *info_mem)(const void *, SIZE_T, void *, struct image_info *, HRESULT *);
    ID3D10Resource *r, *r2;
    BYTE buf[64 * 4], px[4];
    BYTE dds[148 + 4 * 4 * 4 * 4 + 64];
    struct ll10 li;
    struct tl10 tli;
    D3D10_BOX box;
    D3D10_RESOURCE_DIMENSION dim;
    UINT w, h, d, mips, i, x, y, z;
    DXGI_FORMAT fmt;
    ID3D10Blob *blob;
    HRESULT hr;
    const BYTE *bp;

    if (!x10 || !dev) { printf("SKIP  no d3d10 device\n"); return; }
    create = (void *)GetProcAddress(x10, "D3DX10CreateTextureFromMemory");
    filter = (void *)GetProcAddress(x10, "D3DX10FilterTexture");
    load_tex = (void *)GetProcAddress(x10, "D3DX10LoadTextureFromTexture");
    save = (void *)GetProcAddress(x10, "D3DX10SaveTextureToMemory");
    info_mem = (void *)GetProcAddress(x10, "D3DX10GetImageInfoFromMemory");

    /* 1D DDS, 8 texels, RGBA8: R = 32 * i */
    memset(dds, 0, sizeof(dds));
    make_dds(dds, 2, 8, 1, 1, 1, 28);
    for (i = 0; i < 8; ++i) { dds[148 + i * 4] = 32 * i; dds[148 + i * 4 + 1] = 7; dds[148 + i * 4 + 2] = 9; dds[148 + i * 4 + 3] = 255; }

    r = NULL;
    hr = create(dev, dds, 148 + 32, NULL, NULL, &r, NULL);
    check(hr == S_OK && r, "10 load 1D DDS");
    if (r)
    {
        check(tex_desc10(r, &dim, &w, &h, &d, &mips, &fmt) && dim == D3D10_RESOURCE_DIMENSION_TEXTURE1D && w == 8 && mips == 4, "10 1D load makes a Texture1D with the full chain");
        check(texel10(dev, r, 0, 5, 0, 0, px) && px[0] == 160 && px[1] == 7, "10 1D level 0 data");
        check(texel10(dev, r, 1, 0, 0, 0, px) && px[0] == 16 && px[1] == 7, "10 1D level 1 is the 2:1 average");
        check(texel10(dev, r, 1, 3, 0, 0, px) && px[0] == 208, "10 1D level 1 last texel");
        check(texel10(dev, r, 3, 0, 0, 0, px) && px[0] == 112, "10 1D level 3 is the mean");

        /* save it back as DDS */
        blob = NULL;
        hr = save(CTX_ARG10 r, 4 /* DDS */, &blob, 0);
        check(hr == S_OK && blob, "10 save 1D as DDS");
        if (blob)
        {
            bp = ID3D10Blob_GetBufferPointer(blob);
            check(ID3D10Blob_GetBufferSize(blob) == 148 + 60, "10 saved 1D DDS size covers four mips");
            check(((const UINT *)(bp + 4))[2] == 1 && ((const UINT *)(bp + 4))[3] == 8, "10 saved 1D DDS height 1, width 8");
            check(((const UINT *)(bp + 128))[0] == 28 && ((const UINT *)(bp + 128))[1] == 2, "10 saved 1D DDS DX10 header: R8G8B8A8, TEXTURE1D");
            check(bp[148 + 5 * 4] == 160, "10 saved 1D DDS data");
            ID3D10Blob_Release(blob);
        }
        blob = NULL;
        hr = save(CTX_ARG10 r, 0 /* BMP */, &blob, 0);
        check(hr == S_OK && blob, "10 save 1D as BMP");
        if (blob)
        {
            struct image_info ii;
            memset(&ii, 0, sizeof(ii));
            hr = info_mem(ID3D10Blob_GetBufferPointer(blob), ID3D10Blob_GetBufferSize(blob), NULL, &ii, NULL);
            check(hr == S_OK && ii.width == 8 && ii.height == 1, "10 the BMP is 8x1");
            ID3D10Blob_Release(blob);
        }
        ID3D10Resource_Release(r);
    }

    memset(&li, 0xff, sizeof(li));
    li.Width = 4; li.MipLevels = 1; li.Format = 0;
    li.Format = (UINT)-1;
    r = NULL;
    hr = create(dev, dds, 148 + 32, &li, NULL, &r, NULL);
    check(hr == S_OK && r && tex_desc10(r, &dim, &w, &h, &d, &mips, &fmt) && dim == D3D10_RESOURCE_DIMENSION_TEXTURE1D && w == 4 && mips == 1, "10 1D load resized to 4");
    check(r && texel10(dev, r, 0, 0, 0, 0, px) && px[0] == 16 && texel10(dev, r, 0, 3, 0, 0, px) && px[0] == 208, "10 1D resize averages");
    if (r) ID3D10Resource_Release(r);

    /* 3D DDS 4x4x4: R = 40 z, G = 40 x, B = 40 y */
    memset(dds, 0, sizeof(dds));
    make_dds(dds, 4, 4, 4, 4, 1, 28);
    for (z = 0; z < 4; ++z) for (y = 0; y < 4; ++y) for (x = 0; x < 4; ++x)
    {
        BYTE *q = dds + 148 + ((z * 4 + y) * 4 + x) * 4;
        q[0] = 40 * z; q[1] = 40 * x; q[2] = 40 * y; q[3] = 255;
    }
    r = NULL;
    hr = create(dev, dds, 148 + 256, NULL, NULL, &r, NULL);
    check(hr == S_OK && r, "10 load 3D DDS");
    if (r)
    {
        check(tex_desc10(r, &dim, &w, &h, &d, &mips, &fmt) && dim == D3D10_RESOURCE_DIMENSION_TEXTURE3D && w == 4 && h == 4 && d == 4 && mips == 3, "10 3D load generates the volume chain");
        check(texel10(dev, r, 0, 2, 3, 1, px) && px[0] == 40 && px[1] == 80 && px[2] == 120, "10 3D level 0 data");
        check(texel10(dev, r, 1, 0, 0, 0, px) && px[0] == 20 && px[1] == 20 && px[2] == 20, "10 3D level 1 averages 2x2x2");
        check(texel10(dev, r, 1, 1, 1, 1, px) && px[0] == 100 && px[1] == 100 && px[2] == 100, "10 3D level 1 far corner");
        check(texel10(dev, r, 2, 0, 0, 0, px) && px[0] == 60 && px[1] == 60 && px[2] == 60, "10 3D level 2 is the mean");
        blob = NULL;
        hr = save(CTX_ARG10 r, 4, &blob, 0);
        check(hr == S_OK && blob, "10 save 3D as DDS");
        if (blob)
        {
            bp = ID3D10Blob_GetBufferPointer(blob);
            check(ID3D10Blob_GetBufferSize(blob) == 148 + 256 + 32 + 4, "10 saved 3D DDS size covers three volume mips");
            check(((const UINT *)(bp + 4))[1] & 0x800000 && ((const UINT *)(bp + 4))[5] == 4, "10 saved 3D DDS has the depth flag and depth 4");
            check(((const UINT *)(bp + 4))[27] & 0x200000, "10 saved 3D DDS has DDSCAPS2_VOLUME");
            check(((const UINT *)(bp + 128))[1] == 4, "10 saved 3D DDS DX10 header: TEXTURE3D");
            check(bp[148 + (((1 * 4 + 3) * 4) + 2) * 4 + 1] == 80 && bp[148 + (((1 * 4 + 3) * 4) + 2) * 4 + 2] == 120, "10 saved 3D DDS data order");
            ID3D10Blob_Release(blob);
        }
        blob = NULL;
        check(FAILED(save(CTX_ARG10 r, 3 /* PNG */, &blob, 0)) && !blob, "10 a volume cannot be saved as PNG");
        ID3D10Resource_Release(r);
    }

    memset(&li, 0xff, sizeof(li));
    li.Width = 2; li.Height = 2; li.Depth = 2; li.MipLevels = 1;
    r = NULL;
    hr = create(dev, dds, 148 + 256, &li, NULL, &r, NULL);
    check(hr == S_OK && r && tex_desc10(r, &dim, &w, &h, &d, &mips, &fmt) && w == 2 && h == 2 && d == 2 && mips == 1, "10 3D load resized to 2x2x2");
    check(r && texel10(dev, r, 0, 0, 0, 0, px) && px[0] == 20 && px[1] == 20 && px[2] == 20, "10 3D resize averages in all three axes");
    check(r && texel10(dev, r, 0, 1, 1, 1, px) && px[0] == 100 && px[1] == 100, "10 3D resize far corner");
    if (r) ID3D10Resource_Release(r);

    /* a BC1 volume, 4x4x2: slice 0 red, slice 1 blue; converted to RGBA8 it gets a decoded chain */
    memset(dds, 0, sizeof(dds));
    make_dds(dds, 4, 4, 4, 2, 1, 71);
    dds[148] = 0x00; dds[149] = 0xf8; dds[150] = 0; dds[151] = 0;           /* color0 = 565 red, color1 = 0, indices 0 */
    dds[156] = 0x1f; dds[157] = 0x00; dds[158] = 0; dds[159] = 0;           /* color0 = 565 blue */
    memset(&li, 0xff, sizeof(li));
    li.Format = 28;
    r = NULL;
    hr = create(dev, dds, 148 + 16, &li, NULL, &r, NULL);
    check(hr == S_OK && r && tex_desc10(r, &dim, &w, &h, &d, &mips, &fmt) && dim == D3D10_RESOURCE_DIMENSION_TEXTURE3D && d == 2 && fmt == 28 && mips == 3, "10 BC1 volume converted to RGBA8 gets a chain");
    check(r && texel10(dev, r, 0, 1, 1, 0, px) && px[0] == 255 && px[2] == 0 && texel10(dev, r, 0, 1, 1, 1, px) && px[0] == 0 && px[2] == 255, "10 BC1 volume slices decode separately");
    check(r && texel10(dev, r, 1, 0, 0, 0, px) && px[0] >= 127 && px[0] <= 128 && px[2] >= 127 && px[2] <= 128, "10 BC1 volume level 1 averages the slices");
    if (r) ID3D10Resource_Release(r);
    /* the same volume kept as BC1 with a generated chain stays a compressed volume */
    memset(&li, 0xff, sizeof(li));
    r = NULL;
    hr = create(dev, dds, 148 + 16, &li, NULL, &r, NULL);
    check(hr == S_OK && r && tex_desc10(r, &dim, &w, &h, &d, &mips, &fmt) && d == 2 && fmt == 71 && mips == 3, "10 BC1 volume load with a regenerated BC1 chain");
    if (r) ID3D10Resource_Release(r);

    /* FilterTexture on a 1D texture */
    for (i = 0; i < 8; ++i) { buf[i * 4] = 32 * i; buf[i * 4 + 1] = 0; buf[i * 4 + 2] = 0; buf[i * 4 + 3] = 255; }
    r = make_tex10(dev, D3D10_RESOURCE_DIMENSION_TEXTURE1D, 8, 1, 1, 4, DXGI_FORMAT_R8G8B8A8_UNORM, buf);
    check(r != NULL, "10 make 1D texture");
    if (r)
    {
        hr = filter(CTX_ARG10 r, 0, (UINT)-1);
        check(hr == S_OK, "10 FilterTexture on a 1D texture");
        check(texel10(dev, r, 1, 2, 0, 0, px) && px[0] == 144 && px[3] == 255, "10 1D filter level 1");
        check(texel10(dev, r, 2, 1, 0, 0, px) && px[0] == 176, "10 1D filter level 2");
        check(FAILED(filter(CTX_ARG10 r, 9, (UINT)-1)), "10 FilterTexture rejects a source level past the chain");
        ID3D10Resource_Release(r);
    }
    /* FilterTexture on a 3D texture */
    for (z = 0; z < 4; ++z) for (y = 0; y < 4; ++y) for (x = 0; x < 4; ++x)
    { BYTE *q = buf + ((z * 4 + y) * 4 + x) * 4; q[0] = 40 * z; q[1] = 40 * x; q[2] = 40 * y; q[3] = 255; }
    r = make_tex10(dev, D3D10_RESOURCE_DIMENSION_TEXTURE3D, 4, 4, 4, 3, DXGI_FORMAT_R8G8B8A8_UNORM, buf);
    check(r != NULL, "10 make 3D texture");
    if (r)
    {
        hr = filter(CTX_ARG10 r, 0, (UINT)-1);
        check(hr == S_OK, "10 FilterTexture on a 3D texture");
        check(texel10(dev, r, 1, 1, 0, 1, px) && px[0] == 100 && px[1] == 100 && px[2] == 20, "10 3D filter level 1");
        check(texel10(dev, r, 2, 0, 0, 0, px) && px[0] == 60 && px[1] == 60, "10 3D filter level 2");
        ID3D10Resource_Release(r);
    }

    /* LoadTextureFromTexture */
    for (i = 0; i < 8; ++i) { buf[i * 4] = 32 * i; buf[i * 4 + 1] = 0; buf[i * 4 + 2] = 0; buf[i * 4 + 3] = 255; }
    r = make_tex10(dev, D3D10_RESOURCE_DIMENSION_TEXTURE1D, 8, 1, 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM, buf);
    r2 = make_tex10(dev, D3D10_RESOURCE_DIMENSION_TEXTURE1D, 4, 1, 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM, NULL);
    if (r && r2)
    {
        memset(&tli, 0, sizeof(tli)); tli.NumMips = tli.NumElements = tli.Filter = tli.MipFilter = (UINT)-1;
        hr = load_tex(CTX_ARG10 r, &tli, r2);
        check(hr == S_OK, "10 LoadTextureFromTexture 1D to a smaller 1D");
        check(texel10(dev, r2, 0, 1, 0, 0, px) && px[0] == 80 && px[3] == 255, "10 1D scaled copy");
        box.left = 2; box.right = 4; box.top = 0; box.bottom = 1; box.front = 0; box.back = 1;
        { D3D10_BOX sb = {4, 0, 0, 6, 1, 1};
          memset(&tli, 0, sizeof(tli)); tli.NumMips = tli.NumElements = tli.Filter = tli.MipFilter = (UINT)-1;
          tli.pSrcBox = &sb; tli.pDstBox = &box;
          hr = load_tex(CTX_ARG10 r, &tli, r2);
          check(hr == S_OK, "10 1D copy with boxes");
          check(texel10(dev, r2, 0, 2, 0, 0, px) && px[0] == 128 && texel10(dev, r2, 0, 3, 0, 0, px) && px[0] == 160 && texel10(dev, r2, 0, 0, 0, 0, px) && px[0] == 16, "10 1D box copy lands in the box only"); }
    }
    else check(0, "10 make 1D textures for LoadTextureFromTexture");
    if (r) ID3D10Resource_Release(r);
    if (r2) ID3D10Resource_Release(r2);

    for (i = 0; i < 8; ++i) { buf[i * 4] = 10 + i; buf[i * 4 + 1] = 20 + i; buf[i * 4 + 2] = 30 + i; buf[i * 4 + 3] = 255; }
    r = make_tex10(dev, D3D10_RESOURCE_DIMENSION_TEXTURE3D, 2, 2, 2, 1, DXGI_FORMAT_R8G8B8A8_UNORM, buf);
    r2 = make_tex10(dev, D3D10_RESOURCE_DIMENSION_TEXTURE3D, 4, 4, 4, 1, DXGI_FORMAT_R8G8B8A8_UNORM, NULL);
    if (r && r2)
    {
        D3D10_BOX db = {1, 1, 1, 3, 3, 3};
        memset(&tli, 0, sizeof(tli)); tli.NumMips = tli.NumElements = tli.Filter = tli.MipFilter = (UINT)-1;
        tli.pDstBox = &db;
        hr = load_tex(CTX_ARG10 r, &tli, r2);
        check(hr == S_OK, "10 LoadTextureFromTexture 3D into a box");
        check(texel10(dev, r2, 0, 1, 1, 1, px) && px[0] == 10 && px[1] == 20, "10 3D box copy first voxel");
        check(texel10(dev, r2, 0, 2, 2, 2, px) && px[0] == 17 && px[2] == 37, "10 3D box copy last voxel");
        check(texel10(dev, r2, 0, 0, 0, 0, px) && px[0] == 0 && texel10(dev, r2, 0, 3, 3, 3, px) && px[0] == 0, "10 3D box copy leaves the rest");
        /* a different format goes through the converting path */
        ID3D10Resource_Release(r2);
        r2 = make_tex10(dev, D3D10_RESOURCE_DIMENSION_TEXTURE3D, 2, 2, 2, 1, DXGI_FORMAT_B8G8R8A8_UNORM, NULL);
        memset(&tli, 0, sizeof(tli)); tli.NumMips = tli.NumElements = tli.Filter = tli.MipFilter = (UINT)-1;
        hr = r2 ? load_tex(CTX_ARG10 r, &tli, r2) : E_FAIL;
        check(hr == S_OK && texel10(dev, r2, 0, 1, 1, 1, px) && px[0] == 37 && px[2] == 17, "10 3D RGBA to BGRA converts");
        /* a 1D destination for a 3D source is refused */
        if (r2) ID3D10Resource_Release(r2);
        r2 = make_tex10(dev, D3D10_RESOURCE_DIMENSION_TEXTURE1D, 2, 1, 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM, NULL);
        memset(&tli, 0, sizeof(tli)); tli.NumMips = tli.NumElements = tli.Filter = tli.MipFilter = (UINT)-1;
        check(r2 && FAILED(load_tex(CTX_ARG10 r, &tli, r2)), "10 mixing dimensions is refused");
    }
    else check(0, "10 make 3D textures for LoadTextureFromTexture");
    if (r) ID3D10Resource_Release(r);
    if (r2) ID3D10Resource_Release(r2);
    ID3D10Device_Release(dev);
}
/* ---- D3D11 ---- */
static HMODULE x11;
struct ll11 { UINT Width, Height, Depth, FirstMipLevel, MipLevels, Usage, BindFlags, CpuAccessFlags, MiscFlags, Format, Filter, MipFilter; void *pSrcInfo; };
struct tl11 { D3D11_BOX *pSrcBox, *pDstBox; UINT SrcFirstMip, DstFirstMip, NumMips, SrcFirstElement, DstFirstElement, NumElements, Filter, MipFilter; };

static ID3D11Resource *make_tex11(ID3D11Device *dev, D3D11_RESOURCE_DIMENSION dim, UINT w, UINT h, UINT d, UINT mips, DXGI_FORMAT fmt, const BYTE *level0)
{
    D3D11_SUBRESOURCE_DATA sub[8];
    BYTE *zero[8];
    UINT i, bpp = 4;
    ID3D11Resource *res = NULL;
    HRESULT hr;

    for (i = 0; i < mips; ++i)
    {
        UINT lw = max(w >> i, 1), lh = max(h >> i, 1), ld = max(d >> i, 1);
        zero[i] = calloc(1, lw * lh * ld * bpp);
        if (!i && level0) memcpy(zero[0], level0, lw * lh * ld * bpp);
        sub[i].pSysMem = zero[i];
        sub[i].SysMemPitch = lw * bpp;
        sub[i].SysMemSlicePitch = lw * lh * bpp;
    }
    if (dim == D3D11_RESOURCE_DIMENSION_TEXTURE1D)
    {
        D3D11_TEXTURE1D_DESC desc = {0};
        ID3D11Texture1D *t;
        desc.Width = w; desc.MipLevels = mips; desc.ArraySize = 1; desc.Format = fmt; desc.Usage = D3D11_USAGE_DEFAULT; desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        hr = ID3D11Device_CreateTexture1D(dev, &desc, sub, &t);
        if (SUCCEEDED(hr)) res = (ID3D11Resource *)t;
    }
    else if (dim == D3D11_RESOURCE_DIMENSION_TEXTURE3D)
    {
        D3D11_TEXTURE3D_DESC desc = {0};
        ID3D11Texture3D *t;
        desc.Width = w; desc.Height = h; desc.Depth = d; desc.MipLevels = mips; desc.Format = fmt; desc.Usage = D3D11_USAGE_DEFAULT; desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        hr = ID3D11Device_CreateTexture3D(dev, &desc, sub, &t);
        if (SUCCEEDED(hr)) res = (ID3D11Resource *)t;
    }
    for (i = 0; i < mips; ++i) free(zero[i]);
    return res;
}

/* describes any texture: dim, w, h, d, mips, format */
static int tex_desc11(ID3D11Resource *res, D3D11_RESOURCE_DIMENSION *dim, UINT *w, UINT *h, UINT *d, UINT *mips, DXGI_FORMAT *fmt)
{
    void *t;
    ID3D11Resource_GetType(res, dim);
    if (*dim == D3D11_RESOURCE_DIMENSION_TEXTURE1D)
    {
        D3D11_TEXTURE1D_DESC desc;
        if (FAILED(ID3D11Resource_QueryInterface(res, &IID_ID3D11Texture1D, &t))) return 0;
        ID3D11Texture1D_GetDesc((ID3D11Texture1D *)t, &desc); ID3D11Texture1D_Release((ID3D11Texture1D *)t);
        *w = desc.Width; *h = *d = 1; *mips = desc.MipLevels; *fmt = desc.Format;
    }
    else if (*dim == D3D11_RESOURCE_DIMENSION_TEXTURE3D)
    {
        D3D11_TEXTURE3D_DESC desc;
        if (FAILED(ID3D11Resource_QueryInterface(res, &IID_ID3D11Texture3D, &t))) return 0;
        ID3D11Texture3D_GetDesc((ID3D11Texture3D *)t, &desc); ID3D11Texture3D_Release((ID3D11Texture3D *)t);
        *w = desc.Width; *h = desc.Height; *d = desc.Depth; *mips = desc.MipLevels; *fmt = desc.Format;
    }
    else
    {
        D3D11_TEXTURE2D_DESC desc;
        if (FAILED(ID3D11Resource_QueryInterface(res, &IID_ID3D11Texture2D, &t))) return 0;
        ID3D11Texture2D_GetDesc((ID3D11Texture2D *)t, &desc); ID3D11Texture2D_Release((ID3D11Texture2D *)t);
        *w = desc.Width; *h = desc.Height; *d = 1; *mips = desc.MipLevels; *fmt = desc.Format;
    }
    return 1;
}

/* bytes of one texel at (x,y,z) in a mip, via a staging copy */
static int texel11(ID3D11Device *dev, ID3D11Resource *res, UINT mip, UINT x, UINT y, UINT z, BYTE out[4])
{
    D3D11_RESOURCE_DIMENSION dim;
    UINT w, h, d, mips, lw, lh, ld;
    DXGI_FORMAT fmt;
    ID3D11Resource *st = NULL;
    const BYTE *p = NULL;
    UINT rp = 0, dp = 0;
    int ok = 0;

    if (!tex_desc11(res, &dim, &w, &h, &d, &mips, &fmt)) return 0;
    lw = max(w >> mip, 1); lh = max(h >> mip, 1); ld = max(d >> mip, 1);
    if (dim == D3D11_RESOURCE_DIMENSION_TEXTURE1D)
    {
        D3D11_TEXTURE1D_DESC desc = {0}; ID3D11Texture1D *t;
        desc.Width = lw; desc.MipLevels = 1; desc.ArraySize = 1; desc.Format = fmt; desc.Usage = D3D11_USAGE_STAGING; desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        if (FAILED(ID3D11Device_CreateTexture1D(dev, &desc, NULL, &t))) return 0;
        st = (ID3D11Resource *)t;
    }
    else if (dim == D3D11_RESOURCE_DIMENSION_TEXTURE3D)
    {
        D3D11_TEXTURE3D_DESC desc = {0}; ID3D11Texture3D *t;
        desc.Width = lw; desc.Height = lh; desc.Depth = ld; desc.MipLevels = 1; desc.Format = fmt; desc.Usage = D3D11_USAGE_STAGING; desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        if (FAILED(ID3D11Device_CreateTexture3D(dev, &desc, NULL, &t))) return 0;
        st = (ID3D11Resource *)t;
    }
    else return 0;
    COPY_SUB11(dev, st, res, mip);
    ok = MAP_READ11(dev, st, &p, &rp, &dp);
    if (ok)
    {
        memcpy(out, p + z * dp + y * rp + x * 4, 4);
        UNMAP11(dev, st);
    }
    ID3D11Resource_Release(st);
    return ok;
}

static void test_tex11(void)
{
    ID3D11Device *dev = make_device11();
    HRESULT (WINAPI *create)(ID3D11Device *, const void *, SIZE_T, void *, void *, ID3D11Resource **, HRESULT *);
    HRESULT (WINAPI *filter)(FILTER_ARGS11 ID3D11Resource *, UINT, UINT);
    HRESULT (WINAPI *load_tex)(FILTER_ARGS11 ID3D11Resource *, struct tl11 *, ID3D11Resource *);
    HRESULT (WINAPI *save)(FILTER_ARGS11 ID3D11Resource *, UINT, ID3D10Blob **, UINT);
    HRESULT (WINAPI *info_mem)(const void *, SIZE_T, void *, struct image_info *, HRESULT *);
    ID3D11Resource *r, *r2;
    BYTE buf[64 * 4], px[4];
    BYTE dds[148 + 4 * 4 * 4 * 4 + 64];
    struct ll11 li;
    struct tl11 tli;
    D3D11_BOX box;
    D3D11_RESOURCE_DIMENSION dim;
    UINT w, h, d, mips, i, x, y, z;
    DXGI_FORMAT fmt;
    ID3D10Blob *blob;
    HRESULT hr;
    const BYTE *bp;

    if (!x11 || !dev) { printf("SKIP  no d3d11 device\n"); return; }
    create = (void *)GetProcAddress(x11, "D3DX11CreateTextureFromMemory");
    filter = (void *)GetProcAddress(x11, "D3DX11FilterTexture");
    load_tex = (void *)GetProcAddress(x11, "D3DX11LoadTextureFromTexture");
    save = (void *)GetProcAddress(x11, "D3DX11SaveTextureToMemory");
    info_mem = (void *)GetProcAddress(x11, "D3DX11GetImageInfoFromMemory");

    /* 1D DDS, 8 texels, RGBA8: R = 32 * i */
    memset(dds, 0, sizeof(dds));
    make_dds(dds, 2, 8, 1, 1, 1, 28);
    for (i = 0; i < 8; ++i) { dds[148 + i * 4] = 32 * i; dds[148 + i * 4 + 1] = 7; dds[148 + i * 4 + 2] = 9; dds[148 + i * 4 + 3] = 255; }

    r = NULL;
    hr = create(dev, dds, 148 + 32, NULL, NULL, &r, NULL);
    check(hr == S_OK && r, "11 load 1D DDS");
    if (r)
    {
        check(tex_desc11(r, &dim, &w, &h, &d, &mips, &fmt) && dim == D3D11_RESOURCE_DIMENSION_TEXTURE1D && w == 8 && mips == 4, "11 1D load makes a Texture1D with the full chain");
        check(texel11(dev, r, 0, 5, 0, 0, px) && px[0] == 160 && px[1] == 7, "11 1D level 0 data");
        check(texel11(dev, r, 1, 0, 0, 0, px) && px[0] == 16 && px[1] == 7, "11 1D level 1 is the 2:1 average");
        check(texel11(dev, r, 1, 3, 0, 0, px) && px[0] == 208, "11 1D level 1 last texel");
        check(texel11(dev, r, 3, 0, 0, 0, px) && px[0] == 112, "11 1D level 3 is the mean");

        /* save it back as DDS */
        blob = NULL;
        hr = save(CTX_ARG11 r, 4 /* DDS */, &blob, 0);
        check(hr == S_OK && blob, "11 save 1D as DDS");
        if (blob)
        {
            bp = ID3D10Blob_GetBufferPointer(blob);
            check(ID3D10Blob_GetBufferSize(blob) == 148 + 60, "11 saved 1D DDS size covers four mips");
            check(((const UINT *)(bp + 4))[2] == 1 && ((const UINT *)(bp + 4))[3] == 8, "11 saved 1D DDS height 1, width 8");
            check(((const UINT *)(bp + 128))[0] == 28 && ((const UINT *)(bp + 128))[1] == 2, "11 saved 1D DDS DX10 header: R8G8B8A8, TEXTURE1D");
            check(bp[148 + 5 * 4] == 160, "11 saved 1D DDS data");
            ID3D10Blob_Release(blob);
        }
        blob = NULL;
        hr = save(CTX_ARG11 r, 0 /* BMP */, &blob, 0);
        check(hr == S_OK && blob, "11 save 1D as BMP");
        if (blob)
        {
            struct image_info ii;
            memset(&ii, 0, sizeof(ii));
            hr = info_mem(ID3D10Blob_GetBufferPointer(blob), ID3D10Blob_GetBufferSize(blob), NULL, &ii, NULL);
            check(hr == S_OK && ii.width == 8 && ii.height == 1, "11 the BMP is 8x1");
            ID3D10Blob_Release(blob);
        }
        ID3D11Resource_Release(r);
    }

    memset(&li, 0xff, sizeof(li));
    li.Width = 4; li.MipLevels = 1; li.Format = 0;
    li.Format = (UINT)-1;
    r = NULL;
    hr = create(dev, dds, 148 + 32, &li, NULL, &r, NULL);
    check(hr == S_OK && r && tex_desc11(r, &dim, &w, &h, &d, &mips, &fmt) && dim == D3D11_RESOURCE_DIMENSION_TEXTURE1D && w == 4 && mips == 1, "11 1D load resized to 4");
    check(r && texel11(dev, r, 0, 0, 0, 0, px) && px[0] == 16 && texel11(dev, r, 0, 3, 0, 0, px) && px[0] == 208, "11 1D resize averages");
    if (r) ID3D11Resource_Release(r);

    /* 3D DDS 4x4x4: R = 40 z, G = 40 x, B = 40 y */
    memset(dds, 0, sizeof(dds));
    make_dds(dds, 4, 4, 4, 4, 1, 28);
    for (z = 0; z < 4; ++z) for (y = 0; y < 4; ++y) for (x = 0; x < 4; ++x)
    {
        BYTE *q = dds + 148 + ((z * 4 + y) * 4 + x) * 4;
        q[0] = 40 * z; q[1] = 40 * x; q[2] = 40 * y; q[3] = 255;
    }
    r = NULL;
    hr = create(dev, dds, 148 + 256, NULL, NULL, &r, NULL);
    check(hr == S_OK && r, "11 load 3D DDS");
    if (r)
    {
        check(tex_desc11(r, &dim, &w, &h, &d, &mips, &fmt) && dim == D3D11_RESOURCE_DIMENSION_TEXTURE3D && w == 4 && h == 4 && d == 4 && mips == 3, "11 3D load generates the volume chain");
        check(texel11(dev, r, 0, 2, 3, 1, px) && px[0] == 40 && px[1] == 80 && px[2] == 120, "11 3D level 0 data");
        check(texel11(dev, r, 1, 0, 0, 0, px) && px[0] == 20 && px[1] == 20 && px[2] == 20, "11 3D level 1 averages 2x2x2");
        check(texel11(dev, r, 1, 1, 1, 1, px) && px[0] == 100 && px[1] == 100 && px[2] == 100, "11 3D level 1 far corner");
        check(texel11(dev, r, 2, 0, 0, 0, px) && px[0] == 60 && px[1] == 60 && px[2] == 60, "11 3D level 2 is the mean");
        blob = NULL;
        hr = save(CTX_ARG11 r, 4, &blob, 0);
        check(hr == S_OK && blob, "11 save 3D as DDS");
        if (blob)
        {
            bp = ID3D10Blob_GetBufferPointer(blob);
            check(ID3D10Blob_GetBufferSize(blob) == 148 + 256 + 32 + 4, "11 saved 3D DDS size covers three volume mips");
            check(((const UINT *)(bp + 4))[1] & 0x800000 && ((const UINT *)(bp + 4))[5] == 4, "11 saved 3D DDS has the depth flag and depth 4");
            check(((const UINT *)(bp + 4))[27] & 0x200000, "11 saved 3D DDS has DDSCAPS2_VOLUME");
            check(((const UINT *)(bp + 128))[1] == 4, "11 saved 3D DDS DX10 header: TEXTURE3D");
            check(bp[148 + (((1 * 4 + 3) * 4) + 2) * 4 + 1] == 80 && bp[148 + (((1 * 4 + 3) * 4) + 2) * 4 + 2] == 120, "11 saved 3D DDS data order");
            ID3D10Blob_Release(blob);
        }
        blob = NULL;
        check(FAILED(save(CTX_ARG11 r, 3 /* PNG */, &blob, 0)) && !blob, "11 a volume cannot be saved as PNG");
        ID3D11Resource_Release(r);
    }

    memset(&li, 0xff, sizeof(li));
    li.Width = 2; li.Height = 2; li.Depth = 2; li.MipLevels = 1;
    r = NULL;
    hr = create(dev, dds, 148 + 256, &li, NULL, &r, NULL);
    check(hr == S_OK && r && tex_desc11(r, &dim, &w, &h, &d, &mips, &fmt) && w == 2 && h == 2 && d == 2 && mips == 1, "11 3D load resized to 2x2x2");
    check(r && texel11(dev, r, 0, 0, 0, 0, px) && px[0] == 20 && px[1] == 20 && px[2] == 20, "11 3D resize averages in all three axes");
    check(r && texel11(dev, r, 0, 1, 1, 1, px) && px[0] == 100 && px[1] == 100, "11 3D resize far corner");
    if (r) ID3D11Resource_Release(r);

    /* a BC1 volume, 4x4x2: slice 0 red, slice 1 blue; converted to RGBA8 it gets a decoded chain */
    memset(dds, 0, sizeof(dds));
    make_dds(dds, 4, 4, 4, 2, 1, 71);
    dds[148] = 0x00; dds[149] = 0xf8; dds[150] = 0; dds[151] = 0;           /* color0 = 565 red, color1 = 0, indices 0 */
    dds[156] = 0x1f; dds[157] = 0x00; dds[158] = 0; dds[159] = 0;           /* color0 = 565 blue */
    memset(&li, 0xff, sizeof(li));
    li.Format = 28;
    r = NULL;
    hr = create(dev, dds, 148 + 16, &li, NULL, &r, NULL);
    check(hr == S_OK && r && tex_desc11(r, &dim, &w, &h, &d, &mips, &fmt) && dim == D3D11_RESOURCE_DIMENSION_TEXTURE3D && d == 2 && fmt == 28 && mips == 3, "11 BC1 volume converted to RGBA8 gets a chain");
    check(r && texel11(dev, r, 0, 1, 1, 0, px) && px[0] == 255 && px[2] == 0 && texel11(dev, r, 0, 1, 1, 1, px) && px[0] == 0 && px[2] == 255, "11 BC1 volume slices decode separately");
    check(r && texel11(dev, r, 1, 0, 0, 0, px) && px[0] >= 127 && px[0] <= 128 && px[2] >= 127 && px[2] <= 128, "11 BC1 volume level 1 averages the slices");
    if (r) ID3D11Resource_Release(r);
    /* the same volume kept as BC1 with a generated chain stays a compressed volume */
    memset(&li, 0xff, sizeof(li));
    r = NULL;
    hr = create(dev, dds, 148 + 16, &li, NULL, &r, NULL);
    check(hr == S_OK && r && tex_desc11(r, &dim, &w, &h, &d, &mips, &fmt) && d == 2 && fmt == 71 && mips == 3, "11 BC1 volume load with a regenerated BC1 chain");
    if (r) ID3D11Resource_Release(r);

    /* FilterTexture on a 1D texture */
    for (i = 0; i < 8; ++i) { buf[i * 4] = 32 * i; buf[i * 4 + 1] = 0; buf[i * 4 + 2] = 0; buf[i * 4 + 3] = 255; }
    r = make_tex11(dev, D3D11_RESOURCE_DIMENSION_TEXTURE1D, 8, 1, 1, 4, DXGI_FORMAT_R8G8B8A8_UNORM, buf);
    check(r != NULL, "11 make 1D texture");
    if (r)
    {
        hr = filter(CTX_ARG11 r, 0, (UINT)-1);
        check(hr == S_OK, "11 FilterTexture on a 1D texture");
        check(texel11(dev, r, 1, 2, 0, 0, px) && px[0] == 144 && px[3] == 255, "11 1D filter level 1");
        check(texel11(dev, r, 2, 1, 0, 0, px) && px[0] == 176, "11 1D filter level 2");
        check(FAILED(filter(CTX_ARG11 r, 9, (UINT)-1)), "11 FilterTexture rejects a source level past the chain");
        ID3D11Resource_Release(r);
    }
    /* FilterTexture on a 3D texture */
    for (z = 0; z < 4; ++z) for (y = 0; y < 4; ++y) for (x = 0; x < 4; ++x)
    { BYTE *q = buf + ((z * 4 + y) * 4 + x) * 4; q[0] = 40 * z; q[1] = 40 * x; q[2] = 40 * y; q[3] = 255; }
    r = make_tex11(dev, D3D11_RESOURCE_DIMENSION_TEXTURE3D, 4, 4, 4, 3, DXGI_FORMAT_R8G8B8A8_UNORM, buf);
    check(r != NULL, "11 make 3D texture");
    if (r)
    {
        hr = filter(CTX_ARG11 r, 0, (UINT)-1);
        check(hr == S_OK, "11 FilterTexture on a 3D texture");
        check(texel11(dev, r, 1, 1, 0, 1, px) && px[0] == 100 && px[1] == 100 && px[2] == 20, "11 3D filter level 1");
        check(texel11(dev, r, 2, 0, 0, 0, px) && px[0] == 60 && px[1] == 60, "11 3D filter level 2");
        ID3D11Resource_Release(r);
    }

    /* LoadTextureFromTexture */
    for (i = 0; i < 8; ++i) { buf[i * 4] = 32 * i; buf[i * 4 + 1] = 0; buf[i * 4 + 2] = 0; buf[i * 4 + 3] = 255; }
    r = make_tex11(dev, D3D11_RESOURCE_DIMENSION_TEXTURE1D, 8, 1, 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM, buf);
    r2 = make_tex11(dev, D3D11_RESOURCE_DIMENSION_TEXTURE1D, 4, 1, 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM, NULL);
    if (r && r2)
    {
        memset(&tli, 0, sizeof(tli)); tli.NumMips = tli.NumElements = tli.Filter = tli.MipFilter = (UINT)-1;
        hr = load_tex(CTX_ARG11 r, &tli, r2);
        check(hr == S_OK, "11 LoadTextureFromTexture 1D to a smaller 1D");
        check(texel11(dev, r2, 0, 1, 0, 0, px) && px[0] == 80 && px[3] == 255, "11 1D scaled copy");
        box.left = 2; box.right = 4; box.top = 0; box.bottom = 1; box.front = 0; box.back = 1;
        { D3D11_BOX sb = {4, 0, 0, 6, 1, 1};
          memset(&tli, 0, sizeof(tli)); tli.NumMips = tli.NumElements = tli.Filter = tli.MipFilter = (UINT)-1;
          tli.pSrcBox = &sb; tli.pDstBox = &box;
          hr = load_tex(CTX_ARG11 r, &tli, r2);
          check(hr == S_OK, "11 1D copy with boxes");
          check(texel11(dev, r2, 0, 2, 0, 0, px) && px[0] == 128 && texel11(dev, r2, 0, 3, 0, 0, px) && px[0] == 160 && texel11(dev, r2, 0, 0, 0, 0, px) && px[0] == 16, "11 1D box copy lands in the box only"); }
    }
    else check(0, "11 make 1D textures for LoadTextureFromTexture");
    if (r) ID3D11Resource_Release(r);
    if (r2) ID3D11Resource_Release(r2);

    for (i = 0; i < 8; ++i) { buf[i * 4] = 10 + i; buf[i * 4 + 1] = 20 + i; buf[i * 4 + 2] = 30 + i; buf[i * 4 + 3] = 255; }
    r = make_tex11(dev, D3D11_RESOURCE_DIMENSION_TEXTURE3D, 2, 2, 2, 1, DXGI_FORMAT_R8G8B8A8_UNORM, buf);
    r2 = make_tex11(dev, D3D11_RESOURCE_DIMENSION_TEXTURE3D, 4, 4, 4, 1, DXGI_FORMAT_R8G8B8A8_UNORM, NULL);
    if (r && r2)
    {
        D3D11_BOX db = {1, 1, 1, 3, 3, 3};
        memset(&tli, 0, sizeof(tli)); tli.NumMips = tli.NumElements = tli.Filter = tli.MipFilter = (UINT)-1;
        tli.pDstBox = &db;
        hr = load_tex(CTX_ARG11 r, &tli, r2);
        check(hr == S_OK, "11 LoadTextureFromTexture 3D into a box");
        check(texel11(dev, r2, 0, 1, 1, 1, px) && px[0] == 10 && px[1] == 20, "11 3D box copy first voxel");
        check(texel11(dev, r2, 0, 2, 2, 2, px) && px[0] == 17 && px[2] == 37, "11 3D box copy last voxel");
        check(texel11(dev, r2, 0, 0, 0, 0, px) && px[0] == 0 && texel11(dev, r2, 0, 3, 3, 3, px) && px[0] == 0, "11 3D box copy leaves the rest");
        /* a different format goes through the converting path */
        ID3D11Resource_Release(r2);
        r2 = make_tex11(dev, D3D11_RESOURCE_DIMENSION_TEXTURE3D, 2, 2, 2, 1, DXGI_FORMAT_B8G8R8A8_UNORM, NULL);
        memset(&tli, 0, sizeof(tli)); tli.NumMips = tli.NumElements = tli.Filter = tli.MipFilter = (UINT)-1;
        hr = r2 ? load_tex(CTX_ARG11 r, &tli, r2) : E_FAIL;
        check(hr == S_OK && texel11(dev, r2, 0, 1, 1, 1, px) && px[0] == 37 && px[2] == 17, "11 3D RGBA to BGRA converts");
        /* a 1D destination for a 3D source is refused */
        if (r2) ID3D11Resource_Release(r2);
        r2 = make_tex11(dev, D3D11_RESOURCE_DIMENSION_TEXTURE1D, 2, 1, 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM, NULL);
        memset(&tli, 0, sizeof(tli)); tli.NumMips = tli.NumElements = tli.Filter = tli.MipFilter = (UINT)-1;
        check(r2 && FAILED(load_tex(CTX_ARG11 r, &tli, r2)), "11 mixing dimensions is refused");
    }
    else check(0, "11 make 3D textures for LoadTextureFromTexture");
    if (r) ID3D11Resource_Release(r);
    if (r2) ID3D11Resource_Release(r2);
    ID3D11Device_Release(dev);
}

int main(void)
{
    x10 = LoadLibraryA("d3dx10_43.dll");
    x11 = LoadLibraryA("d3dx11_43.dll");
    test_tex10();
    test_tex11();
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures ? 1 : 0;
}
