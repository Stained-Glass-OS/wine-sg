/* d3dx10_43 / d3dx11_43, fast mode: texture and shader resource view loading from memory and files,
 * image info, shader preprocessing and compiling from files. */
#define COBJMACROS
#include <initguid.h>
#include <windows.h>
#include <d3d10.h>
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

struct image_info { UINT width, height, depth, array_size, mip_levels, misc_flags, format, dimension, file_format; };

/* a 2x1 24 bit BMP: one red, one blue pixel */
static const unsigned char bmp[] =
{
    'B','M', 62,0,0,0, 0,0,0,0, 54,0,0,0,
    40,0,0,0, 2,0,0,0, 1,0,0,0, 1,0, 24,0, 0,0,0,0, 8,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
    0,0,255, 255,0,0, 0,0
};

typedef HRESULT (WINAPI *info_mem_fn)(const void *, SIZE_T, void *, struct image_info *, HRESULT *);
typedef HRESULT (WINAPI *info_file_fn)(const char *, void *, struct image_info *, HRESULT *);
typedef HRESULT (WINAPI *tex_mem_fn)(void *, const void *, SIZE_T, void *, void *, void **, HRESULT *);
typedef HRESULT (WINAPI *tex_file_fn)(void *, const char *, void *, void *, void **, HRESULT *);
typedef HRESULT (WINAPI *prep_mem_fn)(const char *, SIZE_T, const char *, const void *, void *, void *, ID3DBlob **, ID3DBlob **, HRESULT *);
typedef HRESULT (WINAPI *prep_file_fn)(const char *, const void *, void *, void *, ID3DBlob **, ID3DBlob **, HRESULT *);
typedef HRESULT (WINAPI *comp_file_fn)(const char *, const void *, void *, const char *, const char *, UINT, UINT, void *, ID3DBlob **, ID3DBlob **, HRESULT *);

static void write_file(const char *name, const void *data, size_t size)
{
    HANDLE f = CreateFileA(name, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    DWORD n;
    WriteFile(f, data, size, &n, NULL);
    CloseHandle(f);
}

static void test_d3dx11(void)
{
    HMODULE x = LoadLibraryA("d3dx11_43.dll"), d3d11 = LoadLibraryA("d3d11.dll");
    HRESULT (WINAPI *create_device)(void *, int, HMODULE, UINT, const void *, UINT, UINT, ID3D11Device **, void *, void **);
    info_mem_fn info_mem; info_file_fn info_file; tex_mem_fn tex_mem; tex_file_fn tex_file;
    tex_mem_fn srv_mem; tex_file_fn srv_file; prep_mem_fn prep_mem; prep_file_fn prep_file;
    ID3D11Device *device = NULL;
    struct image_info info;
    void *obj;
    HRESULT hr, res = 123;

    if (!x || !d3d11) { check(0, "d3dx11 loaded"); return; }
    create_device = (void *)GetProcAddress(d3d11, "D3D11CreateDevice");
    info_mem = (void *)GetProcAddress(x, "D3DX11GetImageInfoFromMemory");
    info_file = (void *)GetProcAddress(x, "D3DX11GetImageInfoFromFileA");
    tex_mem = (void *)GetProcAddress(x, "D3DX11CreateTextureFromMemory");
    tex_file = (void *)GetProcAddress(x, "D3DX11CreateTextureFromFileA");
    srv_mem = (void *)GetProcAddress(x, "D3DX11CreateShaderResourceViewFromMemory");
    srv_file = (void *)GetProcAddress(x, "D3DX11CreateShaderResourceViewFromFileA");
    prep_mem = (void *)GetProcAddress(x, "D3DX11PreprocessShaderFromMemory");
    prep_file = (void *)GetProcAddress(x, "D3DX11PreprocessShaderFromFileA");

    memset(&info, 0xcc, sizeof(info));
    hr = info_mem(bmp, sizeof(bmp), NULL, &info, &res);
    check(hr == S_OK && res == S_OK && info.width == 2 && info.height == 1 && info.file_format == 0, "d3dx11 GetImageInfoFromMemory");
    write_file("probe11.bmp", bmp, sizeof(bmp));
    memset(&info, 0xcc, sizeof(info));
    hr = info_file("probe11.bmp", NULL, &info, NULL);
    check(hr == S_OK && info.width == 2 && info.height == 1, "d3dx11 GetImageInfoFromFile");
    check(info_file("nothere.bmp", NULL, &info, NULL) != S_OK, "missing file fails");

    {
        D3D_FEATURE_LEVEL level = D3D_FEATURE_LEVEL_10_0;
        HRESULT dh = create_device(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, &level, 1, D3D11_SDK_VERSION, &device, NULL, NULL);
        if (FAILED(dh)) dh = create_device(NULL, D3D_DRIVER_TYPE_WARP, NULL, 0, &level, 1, D3D11_SDK_VERSION, &device, NULL, NULL);
        if (FAILED(dh)) { printf("SKIP  no d3d11 device (%#lx)\n", dh); goto shaders; }
    }
    obj = NULL; res = 123;
    hr = tex_mem(device, bmp, sizeof(bmp), NULL, NULL, &obj, &res);
    check(hr == S_OK && obj && res == S_OK, "d3dx11 CreateTextureFromMemory");
    if (obj)
    {
        D3D11_TEXTURE2D_DESC desc;
        ID3D11Texture2D *tex2d = NULL;

        ID3D11Resource_QueryInterface((ID3D11Resource *)obj, &IID_ID3D11Texture2D, (void **)&tex2d);
        check(tex2d != NULL, "it is a 2D texture");
        if (tex2d)
        {
            ID3D11Texture2D_GetDesc(tex2d, &desc);
            check(desc.Width == 2 && desc.Height == 1, "2x1");
            ID3D11Texture2D_Release(tex2d);
        }
        ID3D11Resource_Release((ID3D11Resource *)obj);
    }
    obj = NULL;
    hr = tex_file(device, "probe11.bmp", NULL, NULL, &obj, NULL);
    check(hr == S_OK && obj, "d3dx11 CreateTextureFromFile");
    if (obj) ID3D11Resource_Release((ID3D11Resource *)obj);
    obj = NULL;
    hr = srv_mem(device, bmp, sizeof(bmp), NULL, NULL, &obj, NULL);
    check(hr == S_OK && obj, "d3dx11 CreateShaderResourceViewFromMemory");
    if (obj) ID3D11ShaderResourceView_Release((ID3D11ShaderResourceView *)obj);
    obj = NULL;
    hr = srv_file(device, "probe11.bmp", NULL, NULL, &obj, NULL);
    check(hr == S_OK && obj, "d3dx11 CreateShaderResourceViewFromFile");
    if (obj) ID3D11ShaderResourceView_Release((ID3D11ShaderResourceView *)obj);
    ID3D11Device_Release(device);

shaders:
    {
        static const char src[] = "#define X 1\nfloat4 main() : SV_Target { return X; }\n";
        ID3DBlob *text = NULL, *err = NULL;

        hr = prep_mem(src, strlen(src), "t.hlsl", NULL, NULL, NULL, &text, &err, NULL);
        check(hr == S_OK && text && strstr((char *)ID3D10Blob_GetBufferPointer(text), "return 1"), "d3dx11 PreprocessShaderFromMemory");
        if (text) ID3D10Blob_Release(text);
        if (err) ID3D10Blob_Release(err);
        write_file("probe11.hlsl", src, strlen(src));
        text = err = NULL;
        hr = prep_file("probe11.hlsl", NULL, NULL, NULL, &text, &err, NULL);
        check(hr == S_OK && text && strstr((char *)ID3D10Blob_GetBufferPointer(text), "return 1"), "d3dx11 PreprocessShaderFromFile");
        if (text) ID3D10Blob_Release(text);
        if (err) ID3D10Blob_Release(err);
    }
}

static void test_d3dx10(void)
{
    HMODULE x = LoadLibraryA("d3dx10_43.dll"), d3d10 = LoadLibraryA("d3d10.dll");
    HRESULT (WINAPI *create_device)(void *, int, HMODULE, UINT, UINT, ID3D10Device **);
    tex_file_fn srv_file; tex_mem_fn srv_mem; comp_file_fn comp_file; prep_file_fn prep_file;
    ID3D10Device *device = NULL;
    static const char src[] = "float4 main() : SV_Target { return float4(1, 0, 0, 1); }\n";
    ID3DBlob *code = NULL, *err = NULL;
    void *obj;
    HRESULT hr, res = 99;

    if (!x || !d3d10) { check(0, "d3dx10 loaded"); return; }
    create_device = (void *)GetProcAddress(d3d10, "D3D10CreateDevice");
    srv_file = (void *)GetProcAddress(x, "D3DX10CreateShaderResourceViewFromFileA");
    srv_mem = (void *)GetProcAddress(x, "D3DX10CreateShaderResourceViewFromMemory");
    comp_file = (void *)GetProcAddress(x, "D3DX10CompileFromFileA");
    prep_file = (void *)GetProcAddress(x, "D3DX10PreprocessShaderFromFileA");

    write_file("probe10.hlsl", src, strlen(src));
    hr = comp_file("probe10.hlsl", NULL, NULL, "main", "ps_4_0", 0, 0, NULL, &code, &err, &res);
    check(hr == S_OK && res == S_OK && code && ID3D10Blob_GetBufferSize(code) > 16, "d3dx10 CompileFromFile");
    if (code) ID3D10Blob_Release(code);
    if (err) ID3D10Blob_Release(err);
    code = err = NULL;
    hr = prep_file("probe10.hlsl", NULL, NULL, NULL, &code, &err, NULL);
    check(hr == S_OK && code && strstr((char *)ID3D10Blob_GetBufferPointer(code), "SV_Target"), "d3dx10 PreprocessShaderFromFile");
    if (code) ID3D10Blob_Release(code);
    if (err) ID3D10Blob_Release(err);
    check(comp_file("nothere.hlsl", NULL, NULL, "main", "ps_4_0", 0, 0, NULL, &code, &err, NULL) != S_OK, "missing shader file fails");

    if (FAILED(create_device(NULL, D3D10_DRIVER_TYPE_HARDWARE, NULL, 0, D3D10_SDK_VERSION, &device)) &&
        FAILED(create_device(NULL, D3D10_DRIVER_TYPE_WARP, NULL, 0, D3D10_SDK_VERSION, &device)))
    {
        printf("SKIP  no d3d10 device\n");
        return;
    }
    write_file("probe10.bmp", bmp, sizeof(bmp));
    obj = NULL;
    hr = srv_mem(device, bmp, sizeof(bmp), NULL, NULL, &obj, NULL);
    check(hr == S_OK && obj, "d3dx10 CreateShaderResourceViewFromMemory");
    if (obj) ID3D10ShaderResourceView_Release((ID3D10ShaderResourceView *)obj);
    obj = NULL;
    hr = srv_file(device, "probe10.bmp", NULL, NULL, &obj, NULL);
    check(hr == S_OK && obj, "d3dx10 CreateShaderResourceViewFromFile");
    if (obj) ID3D10ShaderResourceView_Release((ID3D10ShaderResourceView *)obj);
    ID3D10Device_Release(device);
}

int main(void)
{
    test_d3dx11();
    test_d3dx10();
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures ? 1 : 0;
}
