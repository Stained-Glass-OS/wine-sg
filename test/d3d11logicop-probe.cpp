/* Direct3D 11.1 output-merger logic operations (patches/sg/9305): every D3D11_LOGIC_OP on an
 * R8G8B8A8_UINT render target, read back; CheckFeatureSupport(D3D11_OPTIONS) reports what is done. */
#include <windows.h>
#include <d3d11_1.h>
#include <d3dcompiler.h>
#include <stdio.h>

static const char vs_src[] =
    "float4 main(uint id : SV_VertexID) : SV_Position\n"
    "{ float2 p = float2((id << 1) & 2, id & 2); return float4(p * float2(2, -2) + float2(-1, 1), 0, 1); }\n";
static const char ps_src[] =
    "uint4 main() : SV_Target { return uint4(0xff, 0x0f, 0x3c, 0x00); }\n";

static const unsigned char src_px[4] = {0xff, 0x0f, 0x3c, 0x00};
static const unsigned char dst_px[4] = {0xf0, 0x55, 0xaa, 0x0f};

static unsigned char expect(int op, unsigned char s, unsigned char d)
{
    switch (op)
    {
        case D3D11_LOGIC_OP_CLEAR: return 0;
        case D3D11_LOGIC_OP_SET: return 0xff;
        case D3D11_LOGIC_OP_COPY: return s;
        case D3D11_LOGIC_OP_COPY_INVERTED: return ~s;
        case D3D11_LOGIC_OP_NOOP: return d;
        case D3D11_LOGIC_OP_INVERT: return ~d;
        case D3D11_LOGIC_OP_AND: return s & d;
        case D3D11_LOGIC_OP_NAND: return ~(s & d);
        case D3D11_LOGIC_OP_OR: return s | d;
        case D3D11_LOGIC_OP_NOR: return ~(s | d);
        case D3D11_LOGIC_OP_XOR: return s ^ d;
        case D3D11_LOGIC_OP_EQUIV: return ~(s ^ d);
        case D3D11_LOGIC_OP_AND_REVERSE: return s & ~d;
        case D3D11_LOGIC_OP_AND_INVERTED: return ~s & d;
        case D3D11_LOGIC_OP_OR_REVERSE: return s | ~d;
        case D3D11_LOGIC_OP_OR_INVERTED: return ~s | d;
    }
    return 0;
}

static ID3DBlob *compile(const char *src, const char *target)
{
    ID3DBlob *blob = NULL, *err = NULL;
    if (FAILED(D3DCompile(src, strlen(src), NULL, NULL, NULL, "main", target, 0, 0, &blob, &err)))
    {
        printf("compile_error=%s\n", err ? (const char *)err->GetBufferPointer() : "?");
        return NULL;
    }
    return blob;
}

int main(void)
{
    D3D_FEATURE_LEVEL levels[] = {D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0}, fl;
    ID3D11Device *dev0; ID3D11DeviceContext *ctx0;
    ID3D11Device1 *dev; ID3D11DeviceContext1 *ctx;
    D3D11_FEATURE_DATA_D3D11_OPTIONS opt;
    HRESULT hr;
    int op, ok_ops = 0, i;

    setvbuf(stdout, NULL, _IONBF, 0);
    hr = D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, levels, 2, D3D11_SDK_VERSION, &dev0, &fl, &ctx0);
    if (FAILED(hr)) { printf("no_device=%#lx\n", hr); return 0; }
    printf("feature_level=%#x\n", fl);
    dev0->QueryInterface(__uuidof(ID3D11Device1), (void **)&dev);
    ctx0->QueryInterface(__uuidof(ID3D11DeviceContext1), (void **)&ctx);

    memset(&opt, 0xcc, sizeof(opt));
    hr = dev->CheckFeatureSupport(D3D11_FEATURE_D3D11_OPTIONS, &opt, sizeof(opt));
    printf("options_hr=%#lx logicop_cap=%d clearview_cap=%d cboffset=%d cbpartial=%d\n", hr,
            opt.OutputMergerLogicOp, opt.ClearView, opt.ConstantBufferOffsetting, opt.ConstantBufferPartialUpdate);
    printf("options_logicop=%d\n", hr == S_OK && opt.OutputMergerLogicOp == TRUE);
    printf("options_clearview=%d\n", hr == S_OK && opt.ClearView == TRUE);

    /* Direct3D refuses a logic op with independent blending (it is one op for all render targets). */
    {
        D3D11_BLEND_DESC1 bd = {};
        ID3D11BlendState1 *bs = NULL;
        bd.IndependentBlendEnable = TRUE;
        for (i = 0; i < 2; ++i)
        {
            bd.RenderTarget[i].LogicOpEnable = TRUE;
            bd.RenderTarget[i].SrcBlend = bd.RenderTarget[i].SrcBlendAlpha = D3D11_BLEND_ONE;
            bd.RenderTarget[i].DestBlend = bd.RenderTarget[i].DestBlendAlpha = D3D11_BLEND_ZERO;
            bd.RenderTarget[i].BlendOp = bd.RenderTarget[i].BlendOpAlpha = D3D11_BLEND_OP_ADD;
            bd.RenderTarget[i].LogicOp = D3D11_LOGIC_OP_CLEAR;
            bd.RenderTarget[i].RenderTargetWriteMask = 0xf;
        }
        hr = dev->CreateBlendState1(&bd, &bs);
        printf("independent_hr=%#lx\n", hr);
        printf("rejects_independent=%d\n", hr == E_INVALIDARG);
        if (bs) bs->Release();
        bd.IndependentBlendEnable = FALSE;
        hr = dev->CreateBlendState1(&bd, &bs);
        printf("shared_hr=%#lx\n", hr);
        if (bs) bs->Release();
    }

    ID3DBlob *vsb = compile(vs_src, "vs_5_0"), *psb = compile(ps_src, "ps_5_0");
    if (!vsb || !psb) { printf("no_compiler=1\n"); return 0; }
    ID3D11VertexShader *vs; ID3D11PixelShader *ps;
    dev->CreateVertexShader(vsb->GetBufferPointer(), vsb->GetBufferSize(), NULL, &vs);
    dev->CreatePixelShader(psb->GetBufferPointer(), psb->GetBufferSize(), NULL, &ps);

    D3D11_TEXTURE2D_DESC td = {};
    td.Width = td.Height = 4; td.MipLevels = td.ArraySize = 1; td.Format = DXGI_FORMAT_R8G8B8A8_UINT;
    td.SampleDesc.Count = 1; td.Usage = D3D11_USAGE_DEFAULT; td.BindFlags = D3D11_BIND_RENDER_TARGET;
    ID3D11Texture2D *rt, *staging;
    dev->CreateTexture2D(&td, NULL, &rt);
    td.Usage = D3D11_USAGE_STAGING; td.BindFlags = 0; td.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    dev->CreateTexture2D(&td, NULL, &staging);
    ID3D11RenderTargetView *rtv;
    dev->CreateRenderTargetView(rt, NULL, &rtv);

    unsigned char init[16 * 4];
    for (i = 0; i < 16; ++i) memcpy(init + i * 4, dst_px, 4);
    D3D11_VIEWPORT vp = {0, 0, 4, 4, 0, 1};
    ctx->RSSetViewports(1, &vp);
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ctx->VSSetShader(vs, NULL, 0);
    ctx->PSSetShader(ps, NULL, 0);
    ctx->OMSetRenderTargets(1, &rtv, NULL);

    for (op = D3D11_LOGIC_OP_CLEAR; op <= D3D11_LOGIC_OP_OR_INVERTED; ++op)
    {
        D3D11_BLEND_DESC1 bd = {};
        ID3D11BlendState1 *bs = NULL;
        D3D11_MAPPED_SUBRESOURCE map;
        const unsigned char *px;
        int good = 1, c;

        bd.RenderTarget[0].LogicOpEnable = TRUE;
        bd.RenderTarget[0].SrcBlend = bd.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
        bd.RenderTarget[0].DestBlend = bd.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
        bd.RenderTarget[0].BlendOp = bd.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
        bd.RenderTarget[0].LogicOp = (D3D11_LOGIC_OP)op;
        bd.RenderTarget[0].RenderTargetWriteMask = 0xf;
        if (FAILED(hr = dev->CreateBlendState1(&bd, &bs)))
        {
            printf("op%d create_hr=%#lx\n", op, hr);
            continue;
        }
        ctx->UpdateSubresource(rt, 0, NULL, init, 16, 0);
        ctx->OMSetBlendState(bs, NULL, 0xffffffff);
        ctx->Draw(3, 0);
        ctx->CopyResource(staging, rt);
        ctx->Map(staging, 0, D3D11_MAP_READ, 0, &map);
        px = (const unsigned char *)map.pData + map.RowPitch + 4; /* pixel (1,1) */
        for (c = 0; c < 4; ++c)
            if (px[c] != expect(op, src_px[c], dst_px[c])) good = 0;
        printf("op%d got=%02x%02x%02x%02x want=%02x%02x%02x%02x %s\n", op, px[0], px[1], px[2], px[3],
                expect(op, src_px[0], dst_px[0]), expect(op, src_px[1], dst_px[1]),
                expect(op, src_px[2], dst_px[2]), expect(op, src_px[3], dst_px[3]), good ? "ok" : "WRONG");
        ctx->Unmap(staging, 0);
        ok_ops += good;
        bs->Release();
    }
    printf("ops_right=%d\n", ok_ops);
    printf("all_ops=%d\n", ok_ops == 16);

    /* logic op off again: plain writes (state is not left enabled) */
    {
        D3D11_MAPPED_SUBRESOURCE map;
        const unsigned char *px;
        ctx->UpdateSubresource(rt, 0, NULL, init, 16, 0);
        ctx->OMSetBlendState(NULL, NULL, 0xffffffff);
        ctx->Draw(3, 0);
        ctx->CopyResource(staging, rt);
        ctx->Map(staging, 0, D3D11_MAP_READ, 0, &map);
        px = (const unsigned char *)map.pData + map.RowPitch + 4;
        printf("default_state=%d\n", !memcmp(px, src_px, 4));
        ctx->Unmap(staging, 0);
    }
    printf("done=1\n");
    return 0;
}
