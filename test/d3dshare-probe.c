/* d3d11: textures shared between devices (patches/sg/2627): legacy shared handles,
 * OpenSharedResource, the keyed mutex across devices (acquiring and releasing rules,
 * pixels handed over with the key, abandoning), invalid sharing flags, and ClearView. */
#define COBJMACROS
#include <windows.h>
#include <d3d11_1.h>
#include <dxgi.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

static const GUID my_iid_keyed = {0x9d8e1289, 0xd7b3, 0x465f, {0x81, 0x26, 0x25, 0x0e, 0x34, 0x9a, 0xf8, 0x5d}};
static const GUID my_iid_dxgiresource = {0x035f3ab4, 0x482e, 0x4e50, {0xb4, 0x1f, 0x8a, 0x7f, 0x8b, 0xd8, 0x96, 0x0b}};
static const GUID my_iid_context1 = {0xbb2c6faa, 0xb5fb, 0x4082, {0x8e, 0x6b, 0x38, 0x8b, 0x8c, 0xfa, 0x90, 0xe1}};

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

static ID3D11Device *make_device(ID3D11DeviceContext **context)
{
    static const D3D_FEATURE_LEVEL levels[] = {D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0};
    ID3D11Device *device = NULL;
    HRESULT hr = D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, levels, 3, D3D11_SDK_VERSION, &device, NULL, context);

    if (FAILED(hr))
        hr = D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_WARP, NULL, 0, levels, 3, D3D11_SDK_VERSION, &device, NULL, context);
    return SUCCEEDED(hr) ? device : NULL;
}

static ID3D11Texture2D *make_texture(ID3D11Device *device, UINT misc, const UINT *initial)
{
    D3D11_TEXTURE2D_DESC desc = {0};
    D3D11_SUBRESOURCE_DATA data;
    UINT pixels[4 * 4];
    ID3D11Texture2D *tex = NULL;
    int i;

    desc.Width = 4; desc.Height = 4; desc.MipLevels = 1; desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; desc.SampleDesc.Count = 1; desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
    desc.MiscFlags = misc;
    if (initial)
    {
        for (i = 0; i < 16; ++i) pixels[i] = *initial;
        data.pSysMem = pixels; data.SysMemPitch = 16; data.SysMemSlicePitch = 0;
    }
    ID3D11Device_CreateTexture2D(device, &desc, initial ? &data : NULL, &tex);
    return tex;
}

static HRESULT make_texture_hr(ID3D11Device *device, UINT misc)
{
    D3D11_TEXTURE2D_DESC desc = {0};
    ID3D11Texture2D *tex = NULL;
    HRESULT hr;

    desc.Width = 4; desc.Height = 4; desc.MipLevels = 1; desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; desc.SampleDesc.Count = 1; desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    desc.MiscFlags = misc;
    hr = ID3D11Device_CreateTexture2D(device, &desc, NULL, &tex);
    if (tex) ID3D11Texture2D_Release(tex);
    return hr;
}

static UINT read_pixel(ID3D11Device *device, ID3D11DeviceContext *context, ID3D11Texture2D *tex, UINT x, UINT y)
{
    D3D11_TEXTURE2D_DESC desc;
    ID3D11Texture2D *staging = NULL;
    D3D11_MAPPED_SUBRESOURCE mapped;
    UINT pixel = 0xdeadbeef;

    ID3D11Texture2D_GetDesc(tex, &desc);
    desc.Usage = D3D11_USAGE_STAGING; desc.BindFlags = 0; desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ; desc.MiscFlags = 0;
    ID3D11Device_CreateTexture2D(device, &desc, NULL, &staging);
    ID3D11DeviceContext_CopyResource(context, (ID3D11Resource *)staging, (ID3D11Resource *)tex);
    if (SUCCEEDED(ID3D11DeviceContext_Map(context, (ID3D11Resource *)staging, 0, D3D11_MAP_READ, 0, &mapped)))
    {
        pixel = *(UINT *)((BYTE *)mapped.pData + y * mapped.RowPitch + x * 4);
        ID3D11DeviceContext_Unmap(context, (ID3D11Resource *)staging, 0);
    }
    ID3D11Texture2D_Release(staging);
    return pixel;
}

static void fill(ID3D11DeviceContext *context, ID3D11Texture2D *tex, UINT colour)
{
    UINT pixels[16];
    int i;

    for (i = 0; i < 16; ++i) pixels[i] = colour;
    ID3D11DeviceContext_UpdateSubresource(context, (ID3D11Resource *)tex, 0, NULL, pixels, 16, 0);
}

static void test_flags(ID3D11Device *device)
{
    static const struct { UINT misc; HRESULT hr; const char *what; } tests[] =
    {
        {0, S_OK, "no sharing"},
        {D3D11_RESOURCE_MISC_SHARED, S_OK, "shared"},
        {D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX, S_OK, "keyed mutex"},
        {D3D11_RESOURCE_MISC_SHARED | D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX, E_INVALIDARG, "shared and keyed mutex"},
        {0x800 /* NTHANDLE */, E_INVALIDARG, "NT handle alone"},
    };
    unsigned int i;

    for (i = 0; i < sizeof(tests) / sizeof(tests[0]); ++i)
    {
        HRESULT hr = make_texture_hr(device, tests[i].misc);

        check(hr == tests[i].hr, "%s: %#lx", tests[i].what, hr);
    }
}

static void test_handles(ID3D11Device *device, ID3D11DeviceContext *context)
{
    ID3D11Texture2D *plain = make_texture(device, 0, NULL), *shared = make_texture(device, D3D11_RESOURCE_MISC_SHARED, NULL);
    IDXGIResource *res = NULL;
    HANDLE h, h2;
    HRESULT hr;

    ID3D11Texture2D_QueryInterface(plain, &my_iid_dxgiresource, (void **)&res);
    h = (HANDLE)0xdeadbeef;
    hr = IDXGIResource_GetSharedHandle(res, &h);
    check(hr == S_OK && !h, "a plain texture has no handle (%#lx, %p)", hr, h);
    IDXGIResource_Release(res);

    ID3D11Texture2D_QueryInterface(shared, &my_iid_dxgiresource, (void **)&res);
    h = NULL;
    hr = IDXGIResource_GetSharedHandle(res, &h);
    check(hr == S_OK && h && ((ULONG_PTR)h & 0xc0000000), "a shared texture has a global handle (%#lx, %p)", hr, h);
    h2 = NULL;
    IDXGIResource_GetSharedHandle(res, &h2);
    check(h2 == h, "and it is always the same");
    check(!DuplicateHandle(GetCurrentProcess(), h, GetCurrentProcess(), &h2, 0, FALSE, DUPLICATE_SAME_ACCESS)
            && GetLastError() == ERROR_INVALID_HANDLE, "it is not a handle to duplicate");
    check(!CloseHandle(h) && GetLastError() == ERROR_INVALID_HANDLE, "nor one to close");
    IDXGIResource_Release(res);

    {
        ID3D11Device *device2;
        ID3D11DeviceContext *context2;
        ID3D11Texture2D *opened = (void *)0xdeadbeef;
        IDXGIResource *res2 = NULL;
        HANDLE h3 = NULL;
        UINT colour = 0xff336699;
        UINT p;

        fill(context, shared, colour);
        ID3D11DeviceContext_Flush(context);
        device2 = make_device(&context2);
        hr = ID3D11Device_OpenSharedResource(device2, h, &IID_ID3D11Texture2D, (void **)&opened);
        check(hr == S_OK && opened, "OpenSharedResource on another device (%#lx)", hr);
        if (opened)
        {
            ID3D11Texture2D_QueryInterface(opened, &my_iid_dxgiresource, (void **)&res2);
            IDXGIResource_GetSharedHandle(res2, &h3);
            check(h3 == h, "the opened texture has the same handle");
            IDXGIResource_Release(res2);
            p = read_pixel(device2, context2, opened, 2, 2);
            check(p == colour, "and shows what the first device drew (%#x)", p);
            ID3D11Texture2D_Release(opened);
        }
        opened = (void *)0xdeadbeef;
        hr = ID3D11Device_OpenSharedResource(device2, (HANDLE)0xc0123450, &IID_ID3D11Texture2D, (void **)&opened);
        check(hr == E_INVALIDARG, "a handle nothing was made for = %#lx", hr);
        ID3D11DeviceContext_Release(context2);
        ID3D11Device_Release(device2);
    }
    ID3D11Texture2D_Release(plain);
    ID3D11Texture2D_Release(shared);
}

static void test_mutex(ID3D11Device *device, ID3D11DeviceContext *context)
{
    ID3D11Texture2D *tex = make_texture(device, D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX, NULL), *tex2 = NULL;
    IDXGIKeyedMutex *km = NULL, *km2 = NULL;
    IDXGIResource *res = NULL;
    ID3D11Device *device2;
    ID3D11DeviceContext *context2;
    HANDLE handle = NULL;
    HRESULT hr;
    UINT p;

    hr = ID3D11Texture2D_QueryInterface(tex, &my_iid_keyed, (void **)&km);
    check(hr == S_OK && km, "keyed mutex (%#lx)", hr);
    if (!km) return;

    hr = IDXGIKeyedMutex_ReleaseSync(km, 0);
    check(hr == DXGI_ERROR_INVALID_CALL, "releasing what was not acquired = %#lx", hr);
    hr = IDXGIKeyedMutex_AcquireSync(km, 1, 0);
    check(hr == WAIT_TIMEOUT, "key 1 is not the first key = %#lx", hr);
    hr = IDXGIKeyedMutex_AcquireSync(km, 0, 0);
    check(hr == S_OK, "key 0 is = %#lx", hr);
    hr = IDXGIKeyedMutex_AcquireSync(km, 0, 0);
    check(hr == DXGI_ERROR_INVALID_CALL, "acquiring it again on the same device = %#lx", hr);

    fill(context, tex, 0xff0000ff);
    hr = IDXGIKeyedMutex_ReleaseSync(km, 1);
    check(hr == S_OK, "release with key 1 = %#lx", hr);

    ID3D11Texture2D_QueryInterface(tex, &my_iid_dxgiresource, (void **)&res);
    IDXGIResource_GetSharedHandle(res, &handle);
    IDXGIResource_Release(res);
    device2 = make_device(&context2);
    hr = ID3D11Device_OpenSharedResource(device2, handle, &IID_ID3D11Texture2D, (void **)&tex2);
    check(hr == S_OK && tex2, "opened on the other device (%#lx)", hr);
    ID3D11Texture2D_QueryInterface(tex2, &my_iid_keyed, (void **)&km2);

    hr = IDXGIKeyedMutex_AcquireSync(km2, 0, 0);
    check(hr == WAIT_TIMEOUT, "the other device needs key 1 = %#lx", hr);
    hr = IDXGIKeyedMutex_ReleaseSync(km2, 0);
    check(hr == DXGI_ERROR_INVALID_CALL, "and cannot release what it has not acquired = %#lx", hr);
    hr = IDXGIKeyedMutex_AcquireSync(km2, 1, 0);
    check(hr == S_OK, "key 1 = %#lx", hr);
    p = read_pixel(device2, context2, tex2, 1, 1);
    check(p == 0xff0000ff, "the pixels came with the key (%#x)", p);

    hr = IDXGIKeyedMutex_AcquireSync(km, 1, 0);
    check(hr == WAIT_TIMEOUT, "the first device waits (%#lx)", hr);
    hr = IDXGIKeyedMutex_ReleaseSync(km, 5);
    check(hr == DXGI_ERROR_INVALID_CALL, "and cannot release what the other device holds = %#lx", hr);

    fill(context2, tex2, 0xff00ff00);
    hr = IDXGIKeyedMutex_ReleaseSync(km2, 2);
    check(hr == S_OK, "the other device releases with key 2 = %#lx", hr);
    hr = IDXGIKeyedMutex_AcquireSync(km, 2, 0);
    check(hr == S_OK, "the first device acquires it = %#lx", hr);
    p = read_pixel(device, context, tex, 3, 3);
    check(p == 0xff00ff00, "and sees what the other drew (%#x)", p);

    /* the device holding the mutex goes away: nothing can acquire it again */
    IDXGIKeyedMutex_Release(km);
    ID3D11Texture2D_Release(tex);
    hr = IDXGIKeyedMutex_AcquireSync(km2, 2, 0);
    check(hr == WAIT_ABANDONED, "abandoned = %#lx", hr);
    hr = IDXGIKeyedMutex_AcquireSync(km2, 0, 0);
    check(hr == WAIT_ABANDONED, "abandoned again = %#lx", hr);
    hr = IDXGIKeyedMutex_ReleaseSync(km2, 0);
    check(hr == DXGI_ERROR_INVALID_CALL, "nothing to release = %#lx", hr);

    IDXGIKeyedMutex_Release(km2);
    ID3D11Texture2D_Release(tex2);
    ID3D11DeviceContext_Release(context2);
    ID3D11Device_Release(device2);
}

static void test_clearview(ID3D11Device *device, ID3D11DeviceContext *context)
{
    ID3D11DeviceContext1 *context1 = NULL;
    ID3D11Texture2D *tex = make_texture(device, 0, NULL);
    ID3D11RenderTargetView *rtv = NULL;
    static const float white[4] = {1, 1, 1, 1}, blue[4] = {0, 0, 1, 1};
    D3D11_RECT rect = {1, 1, 3, 2};
    UINT p;

    if (FAILED(ID3D11DeviceContext_QueryInterface(context, &my_iid_context1, (void **)&context1)))
    {
        printf("note: no ID3D11DeviceContext1\n");
        ID3D11Texture2D_Release(tex);
        return;
    }
    ID3D11Device_CreateRenderTargetView(device, (ID3D11Resource *)tex, NULL, &rtv);
    ID3D11DeviceContext_ClearRenderTargetView(context, rtv, white);
    ID3D11DeviceContext1_ClearView(context1, (ID3D11View *)rtv, blue, &rect, 1);
    p = read_pixel(device, context, tex, 1, 1);
    check(p == 0xffff0000, "inside the rectangle: blue (%#x)", p);
    p = read_pixel(device, context, tex, 2, 1);
    check(p == 0xffff0000, "inside the rectangle, next pixel (%#x)", p);
    p = read_pixel(device, context, tex, 0, 0);
    check(p == 0xffffffff, "outside: untouched (%#x)", p);
    p = read_pixel(device, context, tex, 1, 2);
    check(p == 0xffffffff, "below the rectangle: untouched (%#x)", p);
    ID3D11DeviceContext1_ClearView(context1, (ID3D11View *)rtv, blue, NULL, 0);
    p = read_pixel(device, context, tex, 3, 3);
    check(p == 0xffff0000, "no rectangles: the whole view (%#x)", p);
    ID3D11RenderTargetView_Release(rtv);
    ID3D11DeviceContext1_Release(context1);
    ID3D11Texture2D_Release(tex);
}

int main(void)
{
    ID3D11DeviceContext *context = NULL;
    ID3D11Device *device = make_device(&context);

    check(device != NULL, "device");
    if (device)
    {
        test_flags(device);
        test_handles(device, context);
        test_mutex(device, context);
        test_clearview(device, context);
        ID3D11DeviceContext_Release(context);
        ID3D11Device_Release(device);
    }
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
