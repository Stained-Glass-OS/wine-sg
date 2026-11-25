/* Direct3D 11 stream output: appending (SOSetTargets offset -1), the buffer's filled size kept across
 * unbinding, an explicit offset, DrawAuto, and a second stream (patches/sg/9306). */
#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <stdio.h>
#include <math.h>

static ID3D11Device *dev;
static ID3D11DeviceContext *ctx;

static ID3DBlob *compile(const char *src, const char *target)
{
    ID3DBlob *blob = NULL, *err = NULL;
    if (FAILED(D3DCompile(src, strlen(src), NULL, NULL, NULL, "main", target, 0, 0, &blob, &err)))
    {
        printf("compile_error_%s=%s\n", target, err ? (const char *)err->GetBufferPointer() : "?");
        return NULL;
    }
    return blob;
}

static const char vs_src[] =
    "struct V { float4 p : POS; };\n"
    "V main(float4 p : POS) { V o; o.p = p; return o; }\n";
static const char gs_src[] =
    "struct V { float4 p : POS; };\n"
    "[maxvertexcount(1)] void main(point V i[1], inout PointStream<V> s) { s.Append(i[0]); }\n";
static const char gs2_src[] =
    "struct V { float4 p : POS; };\n"
    "struct W { float4 q : QOS; };\n"
    "[maxvertexcount(3)] void main(point V i[1], inout PointStream<V> s0, inout PointStream<W> s1)\n"
    "{ W w; s0.Append(i[0]); s0.RestartStrip(); w.q = i[0].p + 100; s1.Append(w); w.q = i[0].p + 200; s1.Append(w); }\n";

static ID3D11Buffer *make_buffer(UINT bind, UINT size, const void *data)
{
    D3D11_BUFFER_DESC bd = {size, D3D11_USAGE_DEFAULT, bind, 0, 0, 0};
    D3D11_SUBRESOURCE_DATA sd = {data, 0, 0};
    ID3D11Buffer *b = NULL;
    dev->CreateBuffer(&bd, data ? &sd : NULL, &b);
    return b;
}

static void read_buffer(ID3D11Buffer *b, float *out, UINT size)
{
    D3D11_BUFFER_DESC bd = {size, D3D11_USAGE_STAGING, 0, D3D11_CPU_ACCESS_READ, 0, 0};
    D3D11_MAPPED_SUBRESOURCE map;
    ID3D11Buffer *s;
    dev->CreateBuffer(&bd, NULL, &s);
    { D3D11_BOX box = {0, 0, 0, size, 1, 1}; ctx->CopySubresourceRegion(s, 0, 0, 0, 0, b, 0, &box); }
    ctx->Map(s, 0, D3D11_MAP_READ, 0, &map);
    memcpy(out, map.pData, size);
    ctx->Unmap(s, 0);
    s->Release();
}

/* entries [first, first+count) of buf hold x = base + k (y = x + 0.5) */
static int check_run(const float *f, int first, int count, float base)
{
    int k;
    for (k = 0; k < count; ++k)
        if (fabsf(f[(first + k) * 4] - (base + k)) > 0.001f || fabsf(f[(first + k) * 4 + 1] - (base + k + 0.5f)) > 0.001f)
            return 0;
    return 1;
}

static void dump(const char *name, const float *f, int n)
{
    int k;
    printf("%s:", name);
    for (k = 0; k < n; ++k) printf(" %g", f[k * 4]);
    printf("\n");
}

static void set_vb(ID3D11Buffer *vb)
{
    UINT stride = 16, offset = 0;
    ctx->IASetVertexBuffers(0, 1, &vb, &stride, &offset);
}

static ID3D11Buffer *points(float base, int n)
{
    float v[64 * 4];
    int k;
    for (k = 0; k < n; ++k) { v[k * 4] = base + k; v[k * 4 + 1] = base + k + 0.5f; v[k * 4 + 2] = 0; v[k * 4 + 3] = 1; }
    return make_buffer(D3D11_BIND_VERTEX_BUFFER, n * 16, v);
}

int main(void)
{
    D3D_FEATURE_LEVEL want = D3D_FEATURE_LEVEL_11_0, fl;
    HRESULT hr;
    float f[64 * 4];
    UINT off;

    setvbuf(stdout, NULL, _IONBF, 0);
    hr = D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, &want, 1, D3D11_SDK_VERSION, &dev, &fl, &ctx);
    if (FAILED(hr)) { printf("no_device=%#lx\n", hr); return 0; }

    ID3DBlob *vsb = compile(vs_src, "vs_5_0"), *gsb = compile(gs_src, "gs_4_0");
    if (!vsb || !gsb) { printf("no_compiler=1\n"); return 0; }
    ID3D11VertexShader *vs; ID3D11GeometryShader *gs; ID3D11InputLayout *il;
    D3D11_INPUT_ELEMENT_DESC ie = {"POS", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0};
    D3D11_SO_DECLARATION_ENTRY so = {0, "POS", 0, 0, 4, 0};
    UINT stride = 16;
    dev->CreateVertexShader(vsb->GetBufferPointer(), vsb->GetBufferSize(), NULL, &vs);
    dev->CreateInputLayout(&ie, 1, vsb->GetBufferPointer(), vsb->GetBufferSize(), &il);
    hr = dev->CreateGeometryShaderWithStreamOutput(gsb->GetBufferPointer(), gsb->GetBufferSize(), &so, 1, &stride, 1,
            D3D11_SO_NO_RASTERIZED_STREAM, NULL, &gs);
    if (FAILED(hr)) { printf("no_so_gs=%#lx\n", hr); return 0; }
    {
        D3D11_TEXTURE2D_DESC td = {};
        ID3D11Texture2D *rt; ID3D11RenderTargetView *rtv;
        D3D11_VIEWPORT vp = {0, 0, 4, 4, 0, 1};
        td.Width = td.Height = 4; td.MipLevels = td.ArraySize = 1; td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        td.SampleDesc.Count = 1; td.BindFlags = D3D11_BIND_RENDER_TARGET;
        dev->CreateTexture2D(&td, NULL, &rt);
        dev->CreateRenderTargetView(rt, NULL, &rtv);
        ctx->OMSetRenderTargets(1, &rtv, NULL);
        ctx->RSSetViewports(1, &vp);
    }
    ctx->IASetInputLayout(il);
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
    ctx->VSSetShader(vs, NULL, 0);
    ctx->GSSetShader(gs, NULL, 0);

    ID3D11Buffer *sobind[1], *nul = NULL;
    ID3D11Buffer *a = make_buffer(D3D11_BIND_STREAM_OUTPUT | D3D11_BIND_VERTEX_BUFFER, 1024, NULL);
    ID3D11Buffer *b = make_buffer(D3D11_BIND_STREAM_OUTPUT | D3D11_BIND_VERTEX_BUFFER, 1024, NULL);
    ID3D11Buffer *c = make_buffer(D3D11_BIND_STREAM_OUTPUT | D3D11_BIND_VERTEX_BUFFER, 1024, NULL);
    ID3D11Buffer *p10 = points(10, 4), *p20 = points(20, 3), *p30 = points(30, 2), *p40 = points(40, 1);

    /* 1. 4 points at offset 0, then 3 appended (offset -1): 7 in a row */
    sobind[0] = a; off = 0;
    ctx->SOSetTargets(1, sobind, &off);
    set_vb(p10); ctx->Draw(4, 0);
    off = ~0u;
    ctx->SOSetTargets(1, sobind, &off);
    set_vb(p20); ctx->Draw(3, 0);
    /* 2. unbound, bound again appending: the filled size stays the buffer's */
    ctx->SOSetTargets(1, &nul, &off);
    off = ~0u;
    ctx->SOSetTargets(1, sobind, &off);
    set_vb(p30); ctx->Draw(2, 0);
    ctx->SOSetTargets(1, &nul, &off);
    read_buffer(a, f, 16 * 16);
    dump("a", f, 10);
    printf("first_draw=%d\n", check_run(f, 0, 4, 10));
    printf("append=%d\n", check_run(f, 4, 3, 20));
    printf("append_after_rebind=%d\n", check_run(f, 7, 2, 30));

    /* 3. explicit offset 32 bytes (2 entries), then one appended: entries 2..4 */
    sobind[0] = c; off = 32;
    ctx->SOSetTargets(1, sobind, &off);
    set_vb(p30); ctx->Draw(2, 0);
    off = ~0u;
    ctx->SOSetTargets(1, sobind, &off);
    set_vb(p40); ctx->Draw(1, 0);
    ctx->SOSetTargets(1, &nul, &off);
    read_buffer(c, f, 16 * 16);
    dump("c", f, 6);
    printf("explicit_offset=%d\n", check_run(f, 2, 2, 30) && check_run(f, 4, 1, 40));

    /* 4. DrawAuto from a (9 vertices filled) into b: b gets a's 9 entries; a SO statistics query sees 9 */
    {
        D3D11_QUERY_DESC qd = {D3D11_QUERY_SO_STATISTICS, 0};
        D3D11_QUERY_DATA_SO_STATISTICS st = {};
        ID3D11Query *q;
        int tries = 0;
        dev->CreateQuery(&qd, &q);
        sobind[0] = b; off = 0;
        ctx->SOSetTargets(1, sobind, &off);
        set_vb(a);
        ctx->Begin(q);
        ctx->DrawAuto();
        ctx->End(q);
        ctx->SOSetTargets(1, &nul, &off);
        while ((hr = ctx->GetData(q, &st, sizeof(st), 0)) == S_FALSE && tries++ < 1000) Sleep(1);
        read_buffer(b, f, 16 * 16);
        dump("b", f, 10);
        printf("drawauto=%d\n", check_run(f, 0, 4, 10) && check_run(f, 4, 3, 20) && check_run(f, 7, 2, 30));
        printf("drawauto_written=%llu\n", (unsigned long long)st.NumPrimitivesWritten);
        printf("drawauto_count=%d\n", hr == S_OK && st.NumPrimitivesWritten == 9);
        /* DrawAuto from c: filled 80 bytes -> 5 vertices (the 2 skipped ones are zeros) */
        sobind[0] = b; off = 0;
        ctx->SOSetTargets(1, sobind, &off);
        set_vb(c);
        ctx->Begin(q);
        ctx->DrawAuto();
        ctx->End(q);
        ctx->SOSetTargets(1, &nul, &off);
        tries = 0;
        while ((hr = ctx->GetData(q, &st, sizeof(st), 0)) == S_FALSE && tries++ < 1000) Sleep(1);
        printf("drawauto_offset_written=%llu\n", (unsigned long long)st.NumPrimitivesWritten);
        printf("drawauto_offset=%d\n", hr == S_OK && st.NumPrimitivesWritten == 5);
        q->Release();
    }

    /* 5. two streams: stream 0 -> slot 0 (1 per point), stream 1 -> slot 1 (2 per point); append on both */
    {
        ID3DBlob *g2 = compile(gs2_src, "gs_5_0");
        D3D11_SO_DECLARATION_ENTRY so2[] = {{0, "POS", 0, 0, 4, 0}, {1, "QOS", 0, 0, 4, 1}};
        UINT strides[2] = {16, 16}, offs[2];
        ID3D11GeometryShader *gs2 = NULL;
        ID3D11Buffer *s0 = make_buffer(D3D11_BIND_STREAM_OUTPUT | D3D11_BIND_VERTEX_BUFFER, 1024, NULL);
        ID3D11Buffer *s1 = make_buffer(D3D11_BIND_STREAM_OUTPUT | D3D11_BIND_VERTEX_BUFFER, 1024, NULL);
        ID3D11Buffer *both[2] = {s0, s1}, *nuls[2] = {NULL, NULL};
        if (g2)
            hr = dev->CreateGeometryShaderWithStreamOutput(g2->GetBufferPointer(), g2->GetBufferSize(), so2, 2,
                    strides, 2, D3D11_SO_NO_RASTERIZED_STREAM, NULL, &gs2);
        if (!g2 || FAILED(hr))
            printf("multistream_unsupported=%#lx\n", g2 ? hr : 0);
        else
        {
            ctx->GSSetShader(gs2, NULL, 0);
            offs[0] = offs[1] = 0;
            ctx->SOSetTargets(2, both, offs);
            set_vb(p10); ctx->Draw(2, 0);
            offs[0] = offs[1] = ~0u;
            ctx->SOSetTargets(2, both, offs);
            set_vb(p20); ctx->Draw(1, 0);
            ctx->SOSetTargets(2, nuls, offs);
            read_buffer(s0, f, 8 * 16);
            dump("s0", f, 4);
            printf("stream0_append=%d\n", check_run(f, 0, 2, 10) && check_run(f, 2, 1, 20));
            read_buffer(s1, f, 8 * 16);
            dump("s1", f, 7);
            printf("stream1_append=%d\n", fabsf(f[0] - 110) < 0.01f && fabsf(f[4] - 210) < 0.01f
                    && fabsf(f[8] - 111) < 0.01f && fabsf(f[12] - 211) < 0.01f
                    && fabsf(f[16] - 120) < 0.01f && fabsf(f[20] - 220) < 0.01f);
            /* DrawAuto from s1 (6 vertices) through the one-stream shader into b */
            ctx->GSSetShader(gs, NULL, 0);
            sobind[0] = b; off = 0;
            ctx->SOSetTargets(1, sobind, &off);
            set_vb(s1);
            ctx->DrawAuto();
            ctx->SOSetTargets(1, &nul, &off);
            read_buffer(b, f, 16 * 16);
            dump("b2", f, 8);
            printf("stream1_drawauto=%d\n", fabsf(f[20] - 220) < 0.01f && f[24] == 0.0f);
        }
    }
    printf("done=1\n");
    return 0;
}
