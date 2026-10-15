/* d3dx11_43 round 2 (patch 3593): thread pump paths, load info and mip chains, FilterTexture, LoadTextureFromTexture,
 * SaveTextureToMemory. Raw function pointers: no d3dx11 headers in the toolchain. */
#define COBJMACROS
#include <initguid.h>
#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <stdio.h>
#include <string.h>

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

struct load_info { UINT width, height, depth, first_mip, mips, usage, bind, cpu, misc, format, filter, mip_filter; void *src_info; };
struct image_info { UINT width, height, depth, array_size, mip_levels, misc_flags, format, dimension, file_format; };

static const unsigned char bmp2x2[] =
{
    'B','M', 70,0,0,0, 0,0,0,0, 54,0,0,0,
    40,0,0,0, 2,0,0,0, 2,0,0,0, 1,0, 24,0, 0,0,0,0, 16,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
    0,0,255, 0,0,255, 0,0,  0,0,255, 0,0,255, 0,0
};

static DWORD read_pixel(ID3D11Device *dev, ID3D11DeviceContext *ctx, ID3D11Texture2D *tex, UINT sub, UINT x, UINT y)
{
    D3D11_TEXTURE2D_DESC d;
    ID3D11Texture2D *st;
    D3D11_MAPPED_SUBRESOURCE m;
    DWORD v = 0xdeadbeef;
    UINT level = sub % (UINT)(d.MipLevels = 1);

    (void)level;
    ID3D11Texture2D_GetDesc(tex, &d);
    {
        D3D11_TEXTURE2D_DESC sd = d;
        UINT mip = sub % d.MipLevels;
        sd.Width = max(d.Width >> mip, 1); sd.Height = max(d.Height >> mip, 1); sd.MipLevels = 1; sd.ArraySize = 1;
        sd.Usage = D3D11_USAGE_STAGING; sd.BindFlags = 0; sd.CPUAccessFlags = D3D11_CPU_ACCESS_READ; sd.MiscFlags = 0;
        if (FAILED(ID3D11Device_CreateTexture2D(dev, &sd, NULL, &st))) return v;
        ID3D11DeviceContext_CopySubresourceRegion(ctx, (ID3D11Resource *)st, 0, 0, 0, 0, (ID3D11Resource *)tex, sub, NULL);
    }
    if (SUCCEEDED(ID3D11DeviceContext_Map(ctx, (ID3D11Resource *)st, 0, D3D11_MAP_READ, 0, &m)))
    {
        v = ((DWORD *)((BYTE *)m.pData + y * m.RowPitch))[x];
        ID3D11DeviceContext_Unmap(ctx, (ID3D11Resource *)st, 0);
    }
    ID3D11Texture2D_Release(st);
    return v;
}

int main(void)
{
    HMODULE x11 = LoadLibraryA("d3dx11_43.dll");
    ID3D11Device *dev = NULL;
    ID3D11DeviceContext *ctx = NULL;
    HRESULT (WINAPI *create_pump)(UINT, UINT, void **) = x11 ? (void *)GetProcAddress(x11, "D3DX11CreateThreadPump") : NULL;
    HRESULT (WINAPI *compile)(const char *, SIZE_T, const char *, const void *, void *, const char *, const char *, UINT, UINT,
            void *, ID3D10Blob **, ID3D10Blob **, HRESULT *) = (void *)GetProcAddress(x11, "D3DX11CompileFromMemory");
    HRESULT (WINAPI *preprocess)(const char *, SIZE_T, const char *, const void *, void *, void *, ID3D10Blob **, ID3D10Blob **,
            HRESULT *) = (void *)GetProcAddress(x11, "D3DX11PreprocessShaderFromMemory");
    HRESULT (WINAPI *srv_mem)(ID3D11Device *, const void *, SIZE_T, struct load_info *, void *, ID3D11ShaderResourceView **,
            HRESULT *) = (void *)GetProcAddress(x11, "D3DX11CreateShaderResourceViewFromMemory");
    HRESULT (WINAPI *tex_mem)(ID3D11Device *, const void *, SIZE_T, struct load_info *, void *, ID3D11Resource **,
            HRESULT *) = (void *)GetProcAddress(x11, "D3DX11CreateTextureFromMemory");
    HRESULT (WINAPI *filter_tex)(ID3D11DeviceContext *, ID3D11Resource *, UINT, UINT) = (void *)GetProcAddress(x11, "D3DX11FilterTexture");
    HRESULT (WINAPI *load_tex)(ID3D11DeviceContext *, ID3D11Resource *, void *, ID3D11Resource *) = (void *)GetProcAddress(x11, "D3DX11LoadTextureFromTexture");
    HRESULT (WINAPI *save_mem)(ID3D11DeviceContext *, ID3D11Resource *, int, ID3D10Blob **, UINT) = (void *)GetProcAddress(x11, "D3DX11SaveTextureToMemory");
    HRESULT (WINAPI *info_mem)(const void *, SIZE_T, void *, struct image_info *, HRESULT *) = (void *)GetProcAddress(x11, "D3DX11GetImageInfoFromMemory");
    static const char hlsl[] = "float4 main(float4 p : SV_POSITION) : SV_TARGET { return float4(1, 0, 0, 1); }";
    static const char macro_src[] = "#define VALUE 42\nint x = VALUE;\n";
    D3D_FEATURE_LEVEL fl;
    void *pump = NULL;
    ID3D10Blob *shader = NULL, *errors = NULL, *text = NULL;
    HRESULT hr, res = 0x12345678;

    hr = D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, NULL, 0, D3D11_SDK_VERSION, &dev, &fl, &ctx);
    if (FAILED(hr)) hr = D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_WARP, NULL, 0, NULL, 0, D3D11_SDK_VERSION, &dev, &fl, &ctx);
    if (!x11 || !dev) { check(0, "device and d3dx11_43"); printf("RESULT: FAIL\n"); return 1; }

    check(create_pump && SUCCEEDED(create_pump(0, 0, &pump)), "thread pump created");
    if (pump)
    {
        hr = compile(hlsl, sizeof(hlsl) - 1, "pump.hlsl", NULL, NULL, "main", "ps_4_0", 0, 0, pump, &shader, &errors, &res);
        check(hr == S_OK, "CompileFromMemory with a pump queues the work");
        hr = CALL(pump, 5, HRESULT (WINAPI *)(void *));
        check(hr == S_OK && shader && res == S_OK && ID3D10Blob_GetBufferSize(shader) > 0, "pumped compile delivers the byte code");
        if (shader) ID3D10Blob_Release(shader);
        shader = NULL;
        res = 0x12345678;
        hr = compile("float4 main( : SV_TARGET { }", 29, "bad.hlsl", NULL, NULL, "main", "ps_4_0", 0, 0, pump, &shader, &errors, &res);
        CALL(pump, 5, HRESULT (WINAPI *)(void *));
        check(hr == S_OK && !shader && FAILED(res) && res != 0x12345678, "pumped compile of broken source reports through hresult");
        if (errors) { ID3D10Blob_Release(errors); errors = NULL; }
        res = 0x12345678;
        hr = preprocess(macro_src, sizeof(macro_src) - 1, "m.hlsl", NULL, NULL, pump, &text, &errors, &res);
        CALL(pump, 5, HRESULT (WINAPI *)(void *));
        check(hr == S_OK && text && res == S_OK && strstr(ID3D10Blob_GetBufferPointer(text), "int x = 42"), "pumped preprocess expands the macro");
        if (text) ID3D10Blob_Release(text);
        {
            ID3D11ShaderResourceView *view = NULL;
            struct image_info info;
            res = 0x12345678;
            hr = srv_mem(dev, bmp2x2, sizeof(bmp2x2), NULL, pump, &view, &res);
            CALL(pump, 5, HRESULT (WINAPI *)(void *));
            check(hr == S_OK && view && res == S_OK, "pumped shader resource view");
            if (view) ID3D11ShaderResourceView_Release(view);
            res = 0x12345678;
            memset(&info, 0, sizeof(info));
            hr = info_mem(bmp2x2, sizeof(bmp2x2), pump, &info, &res);
            CALL(pump, 5, HRESULT (WINAPI *)(void *));
            check(hr == S_OK && res == S_OK && info.width == 2, "pumped image info");
        }
        unk_release(pump);
    }

    {
        ID3D11Resource *tex = NULL;
        ID3D11Texture2D *t2;
        D3D11_TEXTURE2D_DESC d;
        struct load_info li;
        struct image_info info;

        hr = tex_mem(dev, bmp2x2, sizeof(bmp2x2), NULL, NULL, &tex, NULL);
        check(hr == S_OK && tex, "texture from a BMP");
        if (tex)
        {
            ID3D11Resource_QueryInterface(tex, &IID_ID3D11Texture2D, (void **)&t2);
            ID3D11Texture2D_GetDesc(t2, &d);
            check(d.Width == 2 && d.Height == 2 && d.MipLevels == 2, "default load builds the full mip chain");
            ID3D11Texture2D_Release(t2);
            ID3D11Resource_Release(tex);
        }
        memset(&li, 0xff, sizeof(li));
        li.src_info = NULL;
        li.width = 8; li.height = 4; li.mips = 1; li.usage = D3D11_USAGE_DEFAULT; li.bind = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
        li.format = DXGI_FORMAT_B8G8R8A8_UNORM;
        hr = tex_mem(dev, bmp2x2, sizeof(bmp2x2), &li, NULL, &tex, NULL);
        check(hr == S_OK && tex, "texture with explicit load info");
        if (tex)
        {
            ID3D11Resource_QueryInterface(tex, &IID_ID3D11Texture2D, (void **)&t2);
            ID3D11Texture2D_GetDesc(t2, &d);
            check(d.Width == 8 && d.Height == 4 && d.MipLevels == 1 && d.Format == DXGI_FORMAT_B8G8R8A8_UNORM
                    && (d.BindFlags & D3D11_BIND_RENDER_TARGET), "load info size, mips, format and bind flags are honoured");
            check(read_pixel(dev, ctx, t2, 0, 7, 3) == 0xffff0000, "stretched image keeps its colour");
            ID3D11Texture2D_Release(t2);
            ID3D11Resource_Release(tex);
        }

        {
            D3D11_TEXTURE2D_DESC td = {0};
            ID3D11Texture2D *a, *b;
            ID3D11Resource *r;
            ID3D10Blob *png = NULL, *dds = NULL;
            DWORD px[16], blue[16], zero4[4] = {0}, one = 0;
            D3D11_SUBRESOURCE_DATA sd[3];
            int i;

            for (i = 0; i < 16; ++i) { px[i] = 0xff0000ff; blue[i] = 0xffff0000; }
            td.Width = 4; td.Height = 4; td.MipLevels = 3; td.ArraySize = 1; td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            td.SampleDesc.Count = 1; td.Usage = D3D11_USAGE_DEFAULT; td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
            sd[0].pSysMem = px; sd[0].SysMemPitch = 16; sd[0].SysMemSlicePitch = 0;
            sd[1].pSysMem = zero4; sd[1].SysMemPitch = 8; sd[1].SysMemSlicePitch = 0;
            sd[2].pSysMem = &one; sd[2].SysMemPitch = 4; sd[2].SysMemSlicePitch = 0;
            ID3D11Device_CreateTexture2D(dev, &td, sd, &a);
            hr = filter_tex(ctx, (ID3D11Resource *)a, 0, 4);
            check(hr == S_OK, "D3DX11FilterTexture");
            check(read_pixel(dev, ctx, a, 2, 0, 0) == 0xff0000ff, "the filtered 1x1 level is red");

            td.Width = 2; td.Height = 2; td.MipLevels = 1;
            sd[0].pSysMem = blue; sd[0].SysMemPitch = 8;
            ID3D11Device_CreateTexture2D(dev, &td, sd, &b);
            hr = load_tex(ctx, (ID3D11Resource *)a, NULL, (ID3D11Resource *)b);
            check(hr == S_OK, "D3DX11LoadTextureFromTexture");
            check(read_pixel(dev, ctx, b, 0, 0, 0) == 0xff0000ff, "the 4x4 red level was shrunk into the 2x2 target");

            hr = save_mem(ctx, (ID3D11Resource *)a, 3, &png, 0);
            check(hr == S_OK && png && !memcmp(ID3D10Blob_GetBufferPointer(png), "\x89PNG", 4), "SaveTextureToMemory writes a PNG");
            if (png && info_mem(ID3D10Blob_GetBufferPointer(png), ID3D10Blob_GetBufferSize(png), NULL, &info, NULL) == S_OK)
                check(info.width == 4 && info.height == 4, "the saved PNG has the texture size");
            else check(0, "saved PNG info");
            if (png) ID3D10Blob_Release(png);
            hr = save_mem(ctx, (ID3D11Resource *)a, 4, &dds, 0);
            check(hr == S_OK && dds && !memcmp(ID3D10Blob_GetBufferPointer(dds), "DDS ", 4), "SaveTextureToMemory writes a DDS");
            if (dds && info_mem(ID3D10Blob_GetBufferPointer(dds), ID3D10Blob_GetBufferSize(dds), NULL, &info, NULL) == S_OK)
                check(info.width == 4 && info.mip_levels == 3 && info.file_format == 4, "the saved DDS loads back with its mips");
            else check(0, "saved DDS info");
            if (dds) ID3D10Blob_Release(dds);
            (void)r;
            ID3D11Texture2D_Release(a);
            ID3D11Texture2D_Release(b);
        }
    }
    ID3D11DeviceContext_Release(ctx);
    ID3D11Device_Release(dev);
    printf("%d failures\n", failures);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
