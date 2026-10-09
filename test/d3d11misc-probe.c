/* D3D11 device miscellany (patches/sg/2603): resource eviction priorities,
 * Set/GetExceptionMode, GetCreationFlags, the (empty) performance counter
 * set, D3D10 text filter size, CheckMultisampleQualityLevels1 and the
 * CreateDeferredContext2/3 entry points.  All of these were stubs that
 * logged a FIXME and returned E_NOTIMPL or zero. */
#define COBJMACROS
#include <windows.h>
#include <d3d11_4.h>
#include <d3d10.h>
#include <stdio.h>

/* libuuid in some mingw-w64 releases lacks these IIDs. */
static const GUID iid_d3d11_device2 = {0x9d06dffa, 0xd1e5, 0x4d07, {0x83, 0xa8, 0x1b, 0xb1, 0x23, 0xf2, 0xf8, 0x41}};
static const GUID iid_d3d11_device3 = {0xa05c8c37, 0xd2c6, 0x4732, {0xb3, 0xa0, 0x9c, 0xe0, 0xb0, 0xdc, 0x9a, 0xe6}};

#define PRIO_NORMAL 0x78000000u
#define PRIO_HIGH   0xa0000000u
#define PRIO_MIN    0x28000000u

static int failures;

static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) failures++;
}

static void checkv(int ok, const char *what, unsigned long got)
{
    printf("%s  %s (got %#lx)\n", ok ? "PASS" : "FAIL", what, got);
    if (!ok) failures++;
}

static void test_eviction(ID3D11Device *device)
{
    D3D11_BUFFER_DESC bd = {64, D3D11_USAGE_DEFAULT, D3D11_BIND_VERTEX_BUFFER, 0, 0, 0};
    D3D11_TEXTURE1D_DESC t1 = {8, 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_USAGE_DEFAULT, D3D11_BIND_SHADER_RESOURCE, 0, 0};
    D3D11_TEXTURE2D_DESC t2 = {8, 8, 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM, {1, 0}, D3D11_USAGE_DEFAULT,
            D3D11_BIND_SHADER_RESOURCE, 0, 0};
    D3D11_TEXTURE3D_DESC t3 = {4, 4, 4, 1, DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_USAGE_DEFAULT,
            D3D11_BIND_SHADER_RESOURCE, 0, 0};
    ID3D11Resource *res[4] = {0};
    static const char *names[] = {"buffer", "texture1d", "texture2d", "texture3d"};
    unsigned int i;
    HRESULT hr;

    hr = ID3D11Device_CreateBuffer(device, &bd, NULL, (ID3D11Buffer **)&res[0]);
    check(hr == S_OK, "create buffer");
    hr = ID3D11Device_CreateTexture1D(device, &t1, NULL, (ID3D11Texture1D **)&res[1]);
    check(hr == S_OK, "create texture1d");
    hr = ID3D11Device_CreateTexture2D(device, &t2, NULL, (ID3D11Texture2D **)&res[2]);
    check(hr == S_OK, "create texture2d");
    hr = ID3D11Device_CreateTexture3D(device, &t3, NULL, (ID3D11Texture3D **)&res[3]);
    check(hr == S_OK, "create texture3d");

    for (i = 0; i < 4; ++i)
    {
        char what[128];
        IDXGIResource *dxgi = NULL;
        UINT prio, p2;

        if (!res[i]) continue;
        prio = ID3D11Resource_GetEvictionPriority(res[i]);
        sprintf(what, "%s default eviction priority is normal", names[i]);
        checkv(prio == PRIO_NORMAL, what, prio);

        ID3D11Resource_SetEvictionPriority(res[i], PRIO_HIGH);
        prio = ID3D11Resource_GetEvictionPriority(res[i]);
        sprintf(what, "%s eviction priority reads back HIGH", names[i]);
        checkv(prio == PRIO_HIGH, what, prio);

        hr = ID3D11Resource_QueryInterface(res[i], &IID_IDXGIResource, (void **)&dxgi);
        if (hr == S_OK)
        {
            p2 = 0;
            hr = IDXGIResource_GetEvictionPriority(dxgi, &p2);
            sprintf(what, "%s IDXGIResource sees the same priority", names[i]);
            checkv(hr == S_OK && p2 == PRIO_HIGH, what, p2);
            hr = IDXGIResource_SetEvictionPriority(dxgi, PRIO_MIN);
            prio = ID3D11Resource_GetEvictionPriority(res[i]);
            sprintf(what, "%s priority set through DXGI is seen by D3D11", names[i]);
            checkv(hr == S_OK && prio == PRIO_MIN, what, prio);
            hr = IDXGIResource_GetEvictionPriority(dxgi, NULL);
            sprintf(what, "%s GetEvictionPriority(NULL) is E_INVALIDARG", names[i]);
            check(hr == E_INVALIDARG, what);
            IDXGIResource_Release(dxgi);
        }
        else check(0, "QI IDXGIResource");
    }
    for (i = 0; i < 4; ++i)
        if (res[i]) ID3D11Resource_Release(res[i]);
}

static void test_modes(ID3D11Device *device, UINT expect_flags)
{
    HRESULT hr;
    UINT flags;
    static const struct { UINT set; HRESULT hr; UINT expect; } modes[] =
    {
        {0, S_OK, 0},
        {1, S_OK, 1},
        {2, E_INVALIDARG, 1},
        {0x80, E_INVALIDARG, 1},
        {3, E_INVALIDARG, 1},
        {0, S_OK, 0},
    };
    unsigned int i;

    flags = ID3D11Device_GetCreationFlags(device);
    checkv(flags == expect_flags, "GetCreationFlags returns the creation flags", flags);

    for (i = 0; i < sizeof(modes) / sizeof(*modes); ++i)
    {
        char what[96];
        hr = ID3D11Device_SetExceptionMode(device, modes[i].set);
        flags = ID3D11Device_GetExceptionMode(device);
        sprintf(what, "SetExceptionMode(%#x) -> %#lx, mode %#x", modes[i].set, hr, flags);
        check(hr == modes[i].hr && flags == modes[i].expect, what);
    }

}

static void test_d3d10(ID3D10Device *d3d10)
{
    UINT w = 99, h = 99, active, nl = 0, ul = 0, dl = 0;
    D3D10_COUNTER_INFO info10;
    D3D10_COUNTER_DESC desc10;
    ID3D10Counter *counter10;
    D3D10_COUNTER_TYPE type10;
    HRESULT hr;
    UINT flags;

    flags = ID3D10Device_GetCreationFlags(d3d10);
    checkv(flags == 0, "d3d10 GetCreationFlags is 0 for a device created with no flags", flags);
    hr = ID3D10Device_SetExceptionMode(d3d10, 1);
    check(hr == S_OK && ID3D10Device_GetExceptionMode(d3d10) == 1, "d3d10 SetExceptionMode(1) is stored");
    hr = ID3D10Device_SetExceptionMode(d3d10, 4);
    check(hr == E_INVALIDARG && ID3D10Device_GetExceptionMode(d3d10) == 1, "d3d10 SetExceptionMode(4) rejected, mode kept");

    ID3D10Device_SetTextFilterSize(d3d10, 3, 5);
    ID3D10Device_GetTextFilterSize(d3d10, &w, &h);
    check(w == 3 && h == 5, "text filter size reads back 3x5");
    ID3D10Device_SetTextFilterSize(d3d10, 0, 7);
    ID3D10Device_GetTextFilterSize(d3d10, &w, NULL);
    ID3D10Device_GetTextFilterSize(d3d10, NULL, &h);
    check(w == 0 && h == 7, "text filter size 0x7, single NULL out pointer tolerated");

    memset(&info10, 0xff, sizeof(info10));
    ID3D10Device_CheckCounterInfo(d3d10, &info10);
    check(info10.LastDeviceDependentCounter == 0 && info10.NumSimultaneousCounters == 0
            && info10.NumDetectableParallelUnits == 0, "d3d10 CheckCounterInfo reports no counters");
    desc10.Counter = (D3D10_COUNTER)0x40000000; desc10.MiscFlags = 0;
    hr = ID3D10Device_CheckCounter(d3d10, &desc10, &type10, &active, NULL, &nl, NULL, &ul, NULL, &dl);
    checkv(hr == DXGI_ERROR_UNSUPPORTED, "d3d10 CheckCounter(device dependent 0) unsupported", hr);
    desc10.Counter = (D3D10_COUNTER)3;
    hr = ID3D10Device_CheckCounter(d3d10, &desc10, &type10, &active, NULL, &nl, NULL, &ul, NULL, &dl);
    checkv(hr == E_INVALIDARG, "d3d10 CheckCounter(3) invalid", hr);
    counter10 = NULL;
    desc10.Counter = (D3D10_COUNTER)0x40000000;
    hr = ID3D10Device_CreateCounter(d3d10, &desc10, &counter10);
    checkv(hr == DXGI_ERROR_UNSUPPORTED && !counter10, "d3d10 CreateCounter unsupported", hr);
}

static void test_counters(ID3D11Device *device)
{
    D3D11_COUNTER_INFO info;
    D3D11_COUNTER_DESC desc;
    ID3D11Counter *counter;
    D3D11_COUNTER_TYPE type;
    UINT active, nl = 0, ul = 0, dl = 0;
    ID3D10Device *d3d10;
    HRESULT hr;

    memset(&info, 0xff, sizeof(info));
    ID3D11Device_CheckCounterInfo(device, &info);
    check(info.LastDeviceDependentCounter == 0 && info.NumSimultaneousCounters == 0
            && info.NumDetectableParallelUnits == 0, "CheckCounterInfo reports no counters");

    desc.Counter = (D3D11_COUNTER)0x40000000; desc.MiscFlags = 0;
    hr = ID3D11Device_CheckCounter(device, &desc, &type, &active, NULL, &nl, NULL, &ul, NULL, &dl);
    checkv(hr == DXGI_ERROR_UNSUPPORTED, "CheckCounter(device dependent 0) is unsupported", hr);
    desc.Counter = (D3D11_COUNTER)1;
    hr = ID3D11Device_CheckCounter(device, &desc, &type, &active, NULL, &nl, NULL, &ul, NULL, &dl);
    checkv(hr == E_INVALIDARG, "CheckCounter(1) is invalid", hr);
    hr = ID3D11Device_CheckCounter(device, NULL, &type, &active, NULL, &nl, NULL, &ul, NULL, &dl);
    checkv(hr == E_INVALIDARG, "CheckCounter(NULL desc) is invalid", hr);

    desc.Counter = (D3D11_COUNTER)0x40000000; desc.MiscFlags = 0;
    counter = (ID3D11Counter *)(UINT_PTR)0x1234;
    hr = ID3D11Device_CreateCounter(device, &desc, &counter);
    check(hr == DXGI_ERROR_UNSUPPORTED && !counter, "CreateCounter(device dependent 0) unsupported, out cleared");
    desc.Counter = (D3D11_COUNTER)2;
    hr = ID3D11Device_CreateCounter(device, &desc, &counter);
    checkv(hr == E_INVALIDARG, "CreateCounter(2) is invalid", hr);
    hr = ID3D11Device_CreateCounter(device, NULL, &counter);
    checkv(hr == E_INVALIDARG, "CreateCounter(NULL) is invalid", hr);

}

static void test_contexts(ID3D11Device *device)
{
    ID3D11Device2 *dev2 = NULL;
    ID3D11Device3 *dev3 = NULL;
    ID3D11DeviceContext2 *ctx2 = NULL;
    ID3D11DeviceContext3 *ctx3 = NULL;
    ID3D11DeviceContext *ctx;
    UINT levels, ref;
    HRESULT hr;
    ID3D11Buffer *buf;
    D3D11_BUFFER_DESC bd = {64, D3D11_USAGE_DEFAULT, D3D11_BIND_VERTEX_BUFFER, 0, 0, 0};

    hr = ID3D11Device_QueryInterface(device, &iid_d3d11_device2, (void **)&dev2);
    if (hr == S_OK)
    {
        hr = ID3D11Device2_CreateDeferredContext2(dev2, 0, &ctx2);
        check(hr == S_OK && ctx2, "CreateDeferredContext2 succeeds");
        if (ctx2)
        {
            check(ID3D11DeviceContext2_GetType(ctx2) == D3D11_DEVICE_CONTEXT_DEFERRED, "context 2 is deferred");
            hr = ID3D11DeviceContext2_QueryInterface(ctx2, &IID_ID3D11DeviceContext, (void **)&ctx);
            check(hr == S_OK, "context 2 supports the base interface");
            if (hr == S_OK) ID3D11DeviceContext_Release(ctx);
            ref = ID3D11DeviceContext2_Release(ctx2);
            check(ref == 0, "deferred context 2 released");
        }
        levels = 77;
        hr = ID3D11Device2_CheckMultisampleQualityLevels1(dev2, DXGI_FORMAT_R8G8B8A8_UNORM, 1, 0, &levels);
        check(hr == S_OK && levels == 1, "quality levels1 (1 sample) == 1");
        levels = 77;
        hr = ID3D11Device2_CheckMultisampleQualityLevels1(dev2, DXGI_FORMAT_R8G8B8A8_UNORM, 1,
                D3D11_CHECK_MULTISAMPLE_QUALITY_LEVELS_TILED_RESOURCE, &levels);
        check(hr == S_OK && levels == 0, "quality levels1 tiled -> 0 levels");
        levels = 77;
        hr = ID3D11Device2_CheckMultisampleQualityLevels1(dev2, DXGI_FORMAT_R8G8B8A8_UNORM, 1, 0x2, &levels);
        checkv(hr == E_INVALIDARG, "quality levels1 unknown flag invalid", hr);
        ID3D11Device2_Release(dev2);
    }
    else check(0, "QI ID3D11Device2");

    hr = ID3D11Device_QueryInterface(device, &iid_d3d11_device3, (void **)&dev3);
    if (hr == S_OK)
    {
        hr = ID3D11Device3_CreateDeferredContext3(dev3, 0, &ctx3);
        check(hr == S_OK && ctx3, "CreateDeferredContext3 succeeds");
        if (ctx3)
        {
            D3D11_RECT rect = {0, 0, 1, 1};
            ID3D11DeviceContext1 *ctx1 = NULL;
            ID3D11Resource *r = NULL;

            check(ID3D11DeviceContext3_GetType(ctx3) == D3D11_DEVICE_CONTEXT_DEFERRED, "context 3 is deferred");
            hr = ID3D11Device_CreateBuffer(device, &bd, NULL, &buf);
            if (hr == S_OK)
            {
                r = (ID3D11Resource *)buf;
                ID3D11DeviceContext3_QueryInterface(ctx3, &IID_ID3D11DeviceContext1, (void **)&ctx1);
                if (ctx1)
                {
                    ID3D11DeviceContext1_DiscardResource(ctx1, r);
                    ID3D11DeviceContext1_DiscardView1(ctx1, NULL, &rect, 1);
                    check(1, "Discard* on a deferred context are accepted");
                    ID3D11DeviceContext1_Release(ctx1);
                }
                else check(0, "context 3 supports ID3D11DeviceContext1");
                ID3D11Buffer_Release(buf);
            }
            ID3D11DeviceContext3_Release(ctx3);
        }
        ID3D11Device3_Release(dev3);
    }
    else check(0, "QI ID3D11Device3");
}

int main(void)
{
    ID3D11Device *device;
    ID3D10Device *d3d10;
    D3D_FEATURE_LEVEL fl;
    HRESULT hr;
    UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;

    hr = D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, flags, NULL, 0, D3D11_SDK_VERSION, &device, &fl, NULL);
    if (FAILED(hr))
    {
        printf("FAIL  no D3D11 device (%#lx)\nRESULT: FAIL\n", hr);
        return 1;
    }
    printf("feature level %#x\n", fl);

    test_eviction(device);
    test_modes(device, flags);
    test_counters(device);
    test_contexts(device);

    ID3D11Device_Release(device);

    hr = D3D10CreateDevice(NULL, D3D10_DRIVER_TYPE_HARDWARE, NULL, 0, D3D10_SDK_VERSION, &d3d10);
    if (FAILED(hr))
    {
        printf("FAIL  no D3D10 device (%#lx)\n", hr);
        failures++;
    }
    else
    {
        test_d3d10(d3d10);
        ID3D10Device_Release(d3d10);
    }
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
