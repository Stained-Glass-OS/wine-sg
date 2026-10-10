/* d3d11: the Direct3D 11.3 resource, view, query and rasterizer state
 * interfaces made through ID3D11Device3 (patches/sg/2612). */
#define COBJMACROS
#include <windows.h>
#include <d3d11_4.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

static const GUID my_iid_device3 = {0xa05c8c37, 0xd2c6, 0x4732, {0xb3, 0xa0, 0x9c, 0xe0, 0xb0, 0xdc, 0x9a, 0xe6}};

#define IID_ID3D11Texture2D1 my_iid_tex2d1
#define IID_ID3D11ShaderResourceView1 my_iid_srv1
#define IID_ID3D11Query1 my_iid_query1
static const GUID my_iid_tex2d1 = {0x51218251, 0x1e33, 0x4617, {0x9c, 0xcb, 0x4d, 0x3a, 0x43, 0x67, 0xe7, 0xbb}};
static const GUID my_iid_srv1 = {0x91308b87, 0x9040, 0x411d, {0x8c, 0x67, 0xc3, 0x92, 0x53, 0xce, 0x38, 0x02}};
static const GUID my_iid_query1 = {0x631b4766, 0x36dc, 0x461d, {0x8d, 0xb6, 0xc4, 0x7e, 0x13, 0xe6, 0x09, 0x16}};

static int failures;
static ID3D11Device3 *dev3;
static ID3D11Device *dev;
static ID3D11DeviceContext *ctx;

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

static void test_textures(void)
{
    D3D11_TEXTURE2D_DESC1 d1 = {0}, g1;
    D3D11_TEXTURE2D_DESC g0;
    D3D11_TEXTURE3D_DESC1 e1 = {0}, h1;
    D3D11_TEXTURE3D_DESC h0;
    ID3D11Texture2D1 *t2 = NULL;
    ID3D11Texture2D *plain = NULL, *back = NULL;
    ID3D11Texture3D1 *t3 = NULL;
    HRESULT hr;

    d1.Width = 64; d1.Height = 32; d1.MipLevels = 3; d1.ArraySize = 4;
    d1.Format = DXGI_FORMAT_R8G8B8A8_UNORM; d1.SampleDesc.Count = 1; d1.Usage = D3D11_USAGE_DEFAULT;
    d1.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
    hr = ID3D11Device3_CreateTexture2D1(dev3, &d1, NULL, &t2);
    check(hr == S_OK && t2, "CreateTexture2D1 = %#lx", hr);
    if (t2)
    {
        memset(&g1, 0xcc, sizeof(g1));
        ID3D11Texture2D1_GetDesc1(t2, &g1);
        check(g1.Width == 64 && g1.Height == 32 && g1.MipLevels == 3 && g1.ArraySize == 4 && g1.Format == d1.Format &&
              g1.BindFlags == d1.BindFlags && g1.SampleDesc.Count == 1 && g1.TextureLayout == D3D11_TEXTURE_LAYOUT_UNDEFINED,
              "GetDesc1 returns the description (%ux%u mips %u layout %d)", g1.Width, g1.Height, g1.MipLevels, g1.TextureLayout);
        ID3D11Texture2D1_GetDesc(t2, &g0);
        check(g0.Width == 64 && g0.Height == 32 && g0.ArraySize == 4, "GetDesc agrees");
        hr = ID3D11Texture2D1_QueryInterface(t2, &IID_ID3D11Texture2D, (void **)&back);
        check(hr == S_OK && back == (ID3D11Texture2D *)t2, "the texture is also an ID3D11Texture2D (%#lx)", hr);
        if (back) ID3D11Texture2D_Release(back);
        ID3D11Texture2D1_Release(t2);
    }

    /* the plain interface */
    hr = ID3D11Device_CreateTexture2D(dev, (D3D11_TEXTURE2D_DESC *)&d1, NULL, &plain);
    check(hr == S_OK, "plain CreateTexture2D = %#lx", hr);
    if (plain)
    {
        hr = ID3D11Texture2D_QueryInterface(plain, &IID_ID3D11Texture2D1, (void **)&t2);
        check(hr == S_OK && t2, "a plain texture answers ID3D11Texture2D1 (%#lx)", hr);
        if (t2)
        {
            memset(&g1, 0xcc, sizeof(g1));
            ID3D11Texture2D1_GetDesc1(t2, &g1);
            check(g1.Width == 64 && g1.TextureLayout == D3D11_TEXTURE_LAYOUT_UNDEFINED, "and has a description");
            ID3D11Texture2D1_Release(t2);
        }
        ID3D11Texture2D_Release(plain);
    }

    t2 = (ID3D11Texture2D1 *)0x1234;
    hr = ID3D11Device3_CreateTexture2D1(dev3, NULL, NULL, &t2);
    check(hr == E_INVALIDARG, "CreateTexture2D1(NULL) = %#lx", hr);
    d1.TextureLayout = D3D11_TEXTURE_LAYOUT_ROW_MAJOR;
    t2 = NULL;
    hr = ID3D11Device3_CreateTexture2D1(dev3, &d1, NULL, &t2);
    check(hr == E_INVALIDARG && !t2, "CreateTexture2D1 with a row major layout = %#lx", hr);
    d1.TextureLayout = D3D11_TEXTURE_LAYOUT_UNDEFINED;
    hr = ID3D11Device3_CreateTexture2D1(dev3, &d1, NULL, NULL);
    check(hr == S_FALSE, "CreateTexture2D1 with no result pointer only validates (%#lx)", hr);

    e1.Width = 16; e1.Height = 8; e1.Depth = 4; e1.MipLevels = 1; e1.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    e1.Usage = D3D11_USAGE_DEFAULT; e1.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
    hr = ID3D11Device3_CreateTexture3D1(dev3, &e1, NULL, &t3);
    check(hr == S_OK && t3, "CreateTexture3D1 = %#lx", hr);
    if (t3)
    {
        memset(&h1, 0xcc, sizeof(h1));
        ID3D11Texture3D1_GetDesc1(t3, &h1);
        check(h1.Width == 16 && h1.Height == 8 && h1.Depth == 4 && h1.BindFlags == e1.BindFlags && h1.TextureLayout == 0,
              "Texture3D GetDesc1 (%ux%ux%u)", h1.Width, h1.Height, h1.Depth);
        ID3D11Texture3D1_GetDesc(t3, &h0);
        check(h0.Depth == 4, "Texture3D GetDesc agrees");
        ID3D11Texture3D1_Release(t3);
    }
    e1.TextureLayout = D3D11_TEXTURE_LAYOUT_64K_STANDARD_SWIZZLE;
    hr = ID3D11Device3_CreateTexture3D1(dev3, &e1, NULL, &t3);
    check(hr == E_INVALIDARG, "CreateTexture3D1 with a swizzle layout = %#lx", hr);
}

static ID3D11Texture2D *make_tex(UINT w, UINT h, UINT mips, UINT array, UINT bind)
{
    D3D11_TEXTURE2D_DESC d = {0};
    ID3D11Texture2D *t = NULL;

    d.Width = w; d.Height = h; d.MipLevels = mips; d.ArraySize = array;
    d.Format = DXGI_FORMAT_R8G8B8A8_UNORM; d.SampleDesc.Count = 1; d.Usage = D3D11_USAGE_DEFAULT; d.BindFlags = bind;
    ID3D11Device_CreateTexture2D(dev, &d, NULL, &t);
    return t;
}

static void test_views(void)
{
    ID3D11Texture2D *tex = make_tex(32, 32, 3, 4, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET | D3D11_BIND_UNORDERED_ACCESS);
    ID3D11Texture2D *single = make_tex(8, 8, 1, 1, D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE);
    D3D11_SHADER_RESOURCE_VIEW_DESC1 s1 = {0}, sg1;
    D3D11_SHADER_RESOURCE_VIEW_DESC sg0;
    D3D11_RENDER_TARGET_VIEW_DESC1 r1 = {0}, rg1;
    D3D11_RENDER_TARGET_VIEW_DESC rg0;
    D3D11_UNORDERED_ACCESS_VIEW_DESC1 u1 = {0}, ug1;
    D3D11_UNORDERED_ACCESS_VIEW_DESC ug0;
    ID3D11ShaderResourceView1 *srv = NULL;
    ID3D11RenderTargetView1 *rtv = NULL;
    ID3D11UnorderedAccessView1 *uav = NULL;
    ID3D11ShaderResourceView *plain_srv = NULL;
    ID3D11Buffer *buffer = NULL;
    D3D11_BUFFER_DESC bd = {0};
    HRESULT hr;

    if (!tex || !single) { check(0, "textures for the views"); return; }

    /* shader resource view of a 2D texture */
    s1.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    s1.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    s1.Texture2D.MostDetailedMip = 1;
    s1.Texture2D.MipLevels = 2;
    hr = ID3D11Device3_CreateShaderResourceView1(dev3, (ID3D11Resource *)tex, &s1, &srv);
    check(hr == S_OK && srv, "CreateShaderResourceView1(Texture2D) = %#lx", hr);
    if (srv)
    {
        memset(&sg1, 0xcc, sizeof(sg1));
        ID3D11ShaderResourceView1_GetDesc1(srv, &sg1);
        check(sg1.ViewDimension == D3D11_SRV_DIMENSION_TEXTURE2D && sg1.Texture2D.MostDetailedMip == 1 &&
              sg1.Texture2D.MipLevels == 2 && sg1.Texture2D.PlaneSlice == 0, "GetDesc1 (%u,%u,%u)",
              sg1.Texture2D.MostDetailedMip, sg1.Texture2D.MipLevels, sg1.Texture2D.PlaneSlice);
        ID3D11ShaderResourceView1_GetDesc(srv, &sg0);
        check(sg0.Texture2D.MostDetailedMip == 1 && sg0.Texture2D.MipLevels == 2, "GetDesc agrees (%u,%u)",
              sg0.Texture2D.MostDetailedMip, sg0.Texture2D.MipLevels);
        ID3D11ShaderResourceView1_Release(srv);
    }
    s1.Texture2D.PlaneSlice = 1;
    srv = NULL;
    hr = ID3D11Device3_CreateShaderResourceView1(dev3, (ID3D11Resource *)tex, &s1, &srv);
    check(hr == E_INVALIDARG && !srv, "a plane slice other than 0 = %#lx", hr);

    /* array */
    memset(&s1, 0, sizeof(s1));
    s1.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    s1.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
    s1.Texture2DArray.MostDetailedMip = 0;
    s1.Texture2DArray.MipLevels = 3;
    s1.Texture2DArray.FirstArraySlice = 1;
    s1.Texture2DArray.ArraySize = 2;
    hr = ID3D11Device3_CreateShaderResourceView1(dev3, (ID3D11Resource *)tex, &s1, &srv);
    check(hr == S_OK && srv, "CreateShaderResourceView1(Texture2DArray) = %#lx", hr);
    if (srv)
    {
        ID3D11ShaderResourceView1_GetDesc1(srv, &sg1);
        check(sg1.Texture2DArray.MipLevels == 3 && sg1.Texture2DArray.FirstArraySlice == 1 && sg1.Texture2DArray.ArraySize == 2 &&
              sg1.Texture2DArray.PlaneSlice == 0, "array GetDesc1 (%u,%u,%u)", sg1.Texture2DArray.MipLevels,
              sg1.Texture2DArray.FirstArraySlice, sg1.Texture2DArray.ArraySize);
        ID3D11ShaderResourceView1_GetDesc(srv, &sg0);
        check(sg0.Texture2DArray.FirstArraySlice == 1 && sg0.Texture2DArray.ArraySize == 2, "array GetDesc agrees (%u,%u)",
              sg0.Texture2DArray.FirstArraySlice, sg0.Texture2DArray.ArraySize);
        ID3D11ShaderResourceView1_Release(srv);
    }

    /* the default view, and a plain view asked for the new interface */
    hr = ID3D11Device3_CreateShaderResourceView1(dev3, (ID3D11Resource *)tex, NULL, &srv);
    check(hr == S_OK && srv, "CreateShaderResourceView1(NULL desc) = %#lx", hr);
    if (srv) ID3D11ShaderResourceView1_Release(srv);
    hr = ID3D11Device_CreateShaderResourceView(dev, (ID3D11Resource *)tex, NULL, &plain_srv);
    if (SUCCEEDED(hr))
    {
        hr = ID3D11ShaderResourceView_QueryInterface(plain_srv, &IID_ID3D11ShaderResourceView1, (void **)&srv);
        check(hr == S_OK && srv, "a plain view answers ID3D11ShaderResourceView1 (%#lx)", hr);
        if (srv)
        {
            ID3D11ShaderResourceView1_GetDesc1(srv, &sg1);
            check(sg1.ViewDimension == D3D11_SRV_DIMENSION_TEXTURE2DARRAY && sg1.Texture2DArray.ArraySize == 4,
                  "its default description is the whole array (%d, %u)", sg1.ViewDimension, sg1.Texture2DArray.ArraySize);
            ID3D11ShaderResourceView1_Release(srv);
        }
        ID3D11ShaderResourceView_Release(plain_srv);
    }

    /* a buffer view goes through the unchanged union */
    bd.ByteWidth = 256; bd.Usage = D3D11_USAGE_DEFAULT; bd.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    hr = ID3D11Device_CreateBuffer(dev, &bd, NULL, &buffer);
    if (SUCCEEDED(hr))
    {
        memset(&s1, 0, sizeof(s1));
        s1.Format = DXGI_FORMAT_R32_FLOAT;
        s1.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
        s1.Buffer.FirstElement = 2;
        s1.Buffer.NumElements = 8;
        hr = ID3D11Device3_CreateShaderResourceView1(dev3, (ID3D11Resource *)buffer, &s1, &srv);
        check(hr == S_OK && srv, "CreateShaderResourceView1(Buffer) = %#lx", hr);
        if (srv)
        {
            ID3D11ShaderResourceView1_GetDesc1(srv, &sg1);
            check(sg1.ViewDimension == D3D11_SRV_DIMENSION_BUFFER && sg1.Buffer.FirstElement == 2 && sg1.Buffer.NumElements == 8,
                  "buffer GetDesc1 (%u,%u)", sg1.Buffer.FirstElement, sg1.Buffer.NumElements);
            ID3D11ShaderResourceView1_Release(srv);
        }
        ID3D11Buffer_Release(buffer);
    }

    /* render target views */
    r1.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    r1.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
    r1.Texture2D.MipSlice = 2;
    hr = ID3D11Device3_CreateRenderTargetView1(dev3, (ID3D11Resource *)tex, &r1, &rtv);
    check(hr == S_OK && rtv, "CreateRenderTargetView1(Texture2D) = %#lx", hr);
    if (rtv)
    {
        ID3D11RenderTargetView1_GetDesc1(rtv, &rg1);
        check(rg1.ViewDimension == D3D11_RTV_DIMENSION_TEXTURE2D && rg1.Texture2D.MipSlice == 2 && rg1.Texture2D.PlaneSlice == 0,
              "RTV GetDesc1 (%u,%u)", rg1.Texture2D.MipSlice, rg1.Texture2D.PlaneSlice);
        ID3D11RenderTargetView1_GetDesc(rtv, &rg0);
        check(rg0.Texture2D.MipSlice == 2, "RTV GetDesc agrees");
        ID3D11RenderTargetView1_Release(rtv);
    }
    r1.Texture2D.PlaneSlice = 2;
    hr = ID3D11Device3_CreateRenderTargetView1(dev3, (ID3D11Resource *)tex, &r1, &rtv);
    check(hr == E_INVALIDARG, "RTV with a plane slice = %#lx", hr);
    memset(&r1, 0, sizeof(r1));
    r1.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    r1.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
    r1.Texture2DArray.MipSlice = 1;
    r1.Texture2DArray.FirstArraySlice = 2;
    r1.Texture2DArray.ArraySize = 2;
    hr = ID3D11Device3_CreateRenderTargetView1(dev3, (ID3D11Resource *)tex, &r1, &rtv);
    check(hr == S_OK && rtv, "CreateRenderTargetView1(Texture2DArray) = %#lx", hr);
    if (rtv)
    {
        ID3D11RenderTargetView1_GetDesc1(rtv, &rg1);
        check(rg1.Texture2DArray.MipSlice == 1 && rg1.Texture2DArray.FirstArraySlice == 2 && rg1.Texture2DArray.ArraySize == 2,
              "RTV array GetDesc1 (%u,%u,%u)", rg1.Texture2DArray.MipSlice, rg1.Texture2DArray.FirstArraySlice, rg1.Texture2DArray.ArraySize);
        ID3D11RenderTargetView1_Release(rtv);
    }

    /* the view works for drawing: clear and read back */
    hr = ID3D11Device3_CreateRenderTargetView1(dev3, (ID3D11Resource *)single, NULL, &rtv);
    check(hr == S_OK && rtv, "CreateRenderTargetView1(NULL desc) = %#lx", hr);
    if (rtv)
    {
        static const float colour[4] = {1.0f, 0.0f, 0.0f, 1.0f};
        D3D11_TEXTURE2D_DESC d;
        ID3D11Texture2D *staging = NULL;
        D3D11_MAPPED_SUBRESOURCE map;

        ID3D11DeviceContext_ClearRenderTargetView(ctx, (ID3D11RenderTargetView *)rtv, colour);
        ID3D11Texture2D_GetDesc(single, &d);
        d.Usage = D3D11_USAGE_STAGING; d.BindFlags = 0; d.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        ID3D11Device_CreateTexture2D(dev, &d, NULL, &staging);
        if (staging)
        {
            ID3D11DeviceContext_CopyResource(ctx, (ID3D11Resource *)staging, (ID3D11Resource *)single);
            if (SUCCEEDED(ID3D11DeviceContext_Map(ctx, (ID3D11Resource *)staging, 0, D3D11_MAP_READ, 0, &map)))
            {
                BYTE *p = map.pData;
                check(p[0] == 255 && p[1] == 0 && p[2] == 0 && p[3] == 255, "clearing through the new view works (%u,%u,%u,%u)", p[0], p[1], p[2], p[3]);
                ID3D11DeviceContext_Unmap(ctx, (ID3D11Resource *)staging, 0);
            }
            ID3D11Texture2D_Release(staging);
        }
        ID3D11RenderTargetView1_Release(rtv);
    }

    /* unordered access views */
    u1.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    u1.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
    u1.Texture2D.MipSlice = 1;
    hr = ID3D11Device3_CreateUnorderedAccessView1(dev3, (ID3D11Resource *)tex, &u1, &uav);
    check(hr == S_OK && uav, "CreateUnorderedAccessView1(Texture2D) = %#lx", hr);
    if (uav)
    {
        ID3D11UnorderedAccessView1_GetDesc1(uav, &ug1);
        check(ug1.ViewDimension == D3D11_UAV_DIMENSION_TEXTURE2D && ug1.Texture2D.MipSlice == 1 && ug1.Texture2D.PlaneSlice == 0,
              "UAV GetDesc1 (%u,%u)", ug1.Texture2D.MipSlice, ug1.Texture2D.PlaneSlice);
        ID3D11UnorderedAccessView1_GetDesc(uav, &ug0);
        check(ug0.Texture2D.MipSlice == 1, "UAV GetDesc agrees");
        ID3D11UnorderedAccessView1_Release(uav);
    }
    u1.Texture2D.PlaneSlice = 1;
    hr = ID3D11Device3_CreateUnorderedAccessView1(dev3, (ID3D11Resource *)tex, &u1, &uav);
    check(hr == E_INVALIDARG, "UAV with a plane slice = %#lx", hr);
    memset(&u1, 0, sizeof(u1));
    u1.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    u1.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2DARRAY;
    u1.Texture2DArray.MipSlice = 0;
    u1.Texture2DArray.FirstArraySlice = 1;
    u1.Texture2DArray.ArraySize = 3;
    hr = ID3D11Device3_CreateUnorderedAccessView1(dev3, (ID3D11Resource *)tex, &u1, &uav);
    check(hr == S_OK && uav, "CreateUnorderedAccessView1(Texture2DArray) = %#lx", hr);
    if (uav)
    {
        ID3D11UnorderedAccessView1_GetDesc1(uav, &ug1);
        check(ug1.Texture2DArray.FirstArraySlice == 1 && ug1.Texture2DArray.ArraySize == 3 && ug1.Texture2DArray.PlaneSlice == 0,
              "UAV array GetDesc1 (%u,%u)", ug1.Texture2DArray.FirstArraySlice, ug1.Texture2DArray.ArraySize);
        ID3D11UnorderedAccessView1_Release(uav);
    }

    ID3D11Texture2D_Release(tex);
    ID3D11Texture2D_Release(single);
}

static void test_query_and_state(void)
{
    D3D11_QUERY_DESC1 q1 = {D3D11_QUERY_EVENT, 0, D3D11_CONTEXT_TYPE_ALL}, g1;
    D3D11_QUERY_DESC g0;
    ID3D11Query1 *query = NULL;
    ID3D11Query *plain = NULL;
    D3D11_RASTERIZER_DESC2 r2 = {0}, rg2;
    D3D11_RASTERIZER_DESC1 rg1;
    ID3D11RasterizerState2 *state = NULL;
    ID3D11RasterizerState1 *st1 = NULL;
    HRESULT hr;

    hr = ID3D11Device3_CreateQuery1(dev3, &q1, &query);
    check(hr == S_OK && query, "CreateQuery1 = %#lx", hr);
    if (query)
    {
        memset(&g1, 0xcc, sizeof(g1));
        ID3D11Query1_GetDesc1(query, &g1);
        check(g1.Query == D3D11_QUERY_EVENT && g1.MiscFlags == 0 && g1.ContextType == D3D11_CONTEXT_TYPE_ALL, "Query GetDesc1 (%d,%u,%d)", g1.Query, g1.MiscFlags, g1.ContextType);
        ID3D11Query1_GetDesc(query, &g0);
        check(g0.Query == D3D11_QUERY_EVENT, "Query GetDesc agrees");
        ID3D11Query1_Release(query);
    }
    q1.Query = D3D11_QUERY_TIMESTAMP;
    q1.ContextType = D3D11_CONTEXT_TYPE_3D;
    hr = ID3D11Device3_CreateQuery1(dev3, &q1, &query);
    check(hr == S_OK && query, "CreateQuery1 with a 3D context type = %#lx", hr);
    if (query)
    {
        ID3D11Query1_GetDesc1(query, &g1);
        check(g1.Query == D3D11_QUERY_TIMESTAMP && g1.ContextType == D3D11_CONTEXT_TYPE_ALL, "the immediate context serves all types (%d)", g1.ContextType);
        ID3D11Query1_Release(query);
    }
    hr = ID3D11Device3_CreateQuery1(dev3, NULL, &query);
    check(hr == E_INVALIDARG, "CreateQuery1(NULL) = %#lx", hr);
    {
        D3D11_QUERY_DESC d = {D3D11_QUERY_EVENT, 0};
        ID3D11Device_CreateQuery(dev, &d, &plain);
        if (plain)
        {
            hr = ID3D11Query_QueryInterface(plain, &IID_ID3D11Query1, (void **)&query);
            check(hr == S_OK && query, "a plain query answers ID3D11Query1 (%#lx)", hr);
            if (query) ID3D11Query1_Release(query);
            ID3D11Query_Release(plain);
        }
    }

    r2.FillMode = D3D11_FILL_WIREFRAME;
    r2.CullMode = D3D11_CULL_FRONT;
    r2.FrontCounterClockwise = TRUE;
    r2.DepthBias = 7;
    r2.DepthClipEnable = TRUE;
    r2.ScissorEnable = TRUE;
    r2.ForcedSampleCount = 0;
    r2.ConservativeRaster = D3D11_CONSERVATIVE_RASTERIZATION_MODE_OFF;
    hr = ID3D11Device3_CreateRasterizerState2(dev3, &r2, &state);
    check(hr == S_OK && state, "CreateRasterizerState2 = %#lx", hr);
    if (state)
    {
        memset(&rg2, 0xcc, sizeof(rg2));
        ID3D11RasterizerState2_GetDesc2(state, &rg2);
        check(rg2.FillMode == D3D11_FILL_WIREFRAME && rg2.CullMode == D3D11_CULL_FRONT && rg2.FrontCounterClockwise && rg2.DepthBias == 7 &&
              rg2.ScissorEnable && rg2.ConservativeRaster == D3D11_CONSERVATIVE_RASTERIZATION_MODE_OFF, "GetDesc2 returns the state");
        ID3D11RasterizerState2_GetDesc1(state, &rg1);
        check(rg1.DepthBias == 7 && rg1.CullMode == D3D11_CULL_FRONT, "GetDesc1 agrees");
        hr = ID3D11RasterizerState2_QueryInterface(state, &IID_ID3D11RasterizerState1, (void **)&st1);
        check(hr == S_OK, "it is also an ID3D11RasterizerState1");
        if (st1) ID3D11RasterizerState1_Release(st1);
        ID3D11RasterizerState2_Release(state);
    }
    r2.ConservativeRaster = D3D11_CONSERVATIVE_RASTERIZATION_MODE_ON;
    state = NULL;
    hr = ID3D11Device3_CreateRasterizerState2(dev3, &r2, &state);
    check(hr == E_INVALIDARG && !state, "conservative rasterization is refused = %#lx", hr);
    hr = ID3D11Device3_CreateRasterizerState2(dev3, NULL, &state);
    check(hr == E_INVALIDARG, "CreateRasterizerState2(NULL) = %#lx", hr);
}

int main(void)
{
    static const D3D_FEATURE_LEVEL levels[] = {D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0};
    HRESULT hr;

    hr = D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, levels, 3, D3D11_SDK_VERSION, &dev, NULL, &ctx);
    if (FAILED(hr))
        hr = D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_WARP, NULL, 0, levels, 3, D3D11_SDK_VERSION, &dev, NULL, &ctx);
    check(SUCCEEDED(hr), "D3D11CreateDevice");
    if (FAILED(hr)) goto done;
    hr = ID3D11Device_QueryInterface(dev, &my_iid_device3, (void **)&dev3);
    check(hr == S_OK, "ID3D11Device3 (%#lx)", hr);
    if (FAILED(hr)) goto done;

    test_textures();
    test_views();
    test_query_and_state();

done:
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
