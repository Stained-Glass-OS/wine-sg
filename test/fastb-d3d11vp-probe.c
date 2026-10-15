/* d3d11 video processor: enumerator, processor, input/output views, ID3D11VideoContext state accessors,
 * video device decoder/content-protection queries, decoder creation parameters (patch 3730) */
#define COBJMACROS
#include <windows.h>
#include <d3d11_1.h>
#include <stdio.h>
#include <string.h>

static int f;
#define CHECK(c, ...) do { if (!(c)) { f++; printf("FAIL line %d: %s: ", __LINE__, #c); printf(__VA_ARGS__); printf("\n"); } } while (0)
#define HR(x) ((unsigned)(x))

static ID3D11Device *make_device(void)
{
    static const D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0 };
    ID3D11Device *d = NULL;
    HRESULT hr = D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, D3D11_CREATE_DEVICE_BGRA_SUPPORT, levels, 3, D3D11_SDK_VERSION, &d, NULL, NULL);
    if (FAILED(hr)) hr = D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_WARP, NULL, D3D11_CREATE_DEVICE_BGRA_SUPPORT, levels, 3, D3D11_SDK_VERSION, &d, NULL, NULL);
    return SUCCEEDED(hr) ? d : NULL;
}

static ID3D11Texture2D *make_tex(ID3D11Device *dev, UINT w, UINT h, UINT mips, UINT array)
{
    D3D11_TEXTURE2D_DESC td = {0};
    ID3D11Texture2D *t = NULL;
    td.Width = w; td.Height = h; td.MipLevels = mips; td.ArraySize = array;
    td.Format = DXGI_FORMAT_B8G8R8A8_UNORM; td.SampleDesc.Count = 1; td.Usage = D3D11_USAGE_DEFAULT;
    td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    ID3D11Device_CreateTexture2D(dev, &td, NULL, &t);
    return t;
}

static int rect_eq(const RECT *a, int l, int t, int r, int b) { return a->left == l && a->top == t && a->right == r && a->bottom == b; }

static void test_stream_state(ID3D11VideoContext *ctx, ID3D11VideoProcessor *vp, ID3D11VideoProcessor *vp2)
{
    D3D11_VIDEO_PROCESSOR_COLOR_SPACE cs = {0}, cs2;
    D3D11_VIDEO_FRAME_FORMAT ff;
    D3D11_VIDEO_PROCESSOR_OUTPUT_RATE rate;
    DXGI_RATIONAL custom = {30, 1}, c2, sa = {1, 1}, da = {4, 3};
    BOOL en, rep, en2, lh, bh;
    RECT r = {1, 2, 30, 40}, r2;
    float a, lo, hi;
    UINT pal[4] = {0x11111111, 0x22222222, 0x33333333, 0x44444444}, pal2[6];
    int lvl, mono;
    D3D11_VIDEO_PROCESSOR_STEREO_FORMAT sf;
    D3D11_VIDEO_PROCESSOR_STEREO_FLIP_MODE fm;
    D3D11_VIDEO_PROCESSOR_ROTATION rot;
    GUID g = {1, 2, 3, {4}};
    char buf[4] = {0};
    HRESULT hr;
    int i;

    /* documented / chosen defaults on a fresh processor */
    ID3D11VideoContext_VideoProcessorGetStreamFrameFormat(ctx, vp, 0, &ff);
    CHECK(ff == D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE, "default frame format %d", ff);
    ID3D11VideoContext_VideoProcessorGetStreamOutputRate(ctx, vp, 0, &rate, &rep, &c2);
    CHECK(rate == D3D11_VIDEO_PROCESSOR_OUTPUT_RATE_NORMAL, "default rate %d", rate);
    ID3D11VideoContext_VideoProcessorGetStreamSourceRect(ctx, vp, 0, &en, &r2);
    CHECK(!en, "source rect enabled by default");
    ID3D11VideoContext_VideoProcessorGetStreamDestRect(ctx, vp, 0, &en, &r2);
    CHECK(!en, "dest rect enabled by default");
    ID3D11VideoContext_VideoProcessorGetStreamAlpha(ctx, vp, 0, &en, &a);
    CHECK(!en, "alpha enabled by default");
    ID3D11VideoContext_VideoProcessorGetStreamAutoProcessingMode(ctx, vp, 0, &en);
    CHECK(en, "auto processing off by default");
    ID3D11VideoContext_VideoProcessorGetStreamLumaKey(ctx, vp, 0, &en, &lo, &hi);
    CHECK(!en, "luma key on by default");
    ID3D11VideoContext_VideoProcessorGetStreamPixelAspectRatio(ctx, vp, 0, &en, &c2, &c2);
    CHECK(!en, "aspect ratio on by default");
    ID3D11VideoContext_VideoProcessorGetStreamStereoFormat(ctx, vp, 0, &en, &sf, &lh, &bh, &fm, &mono);
    CHECK(!en, "stereo on by default");
    ID3D11VideoContext_VideoProcessorGetStreamRotation(ctx, vp, 0, &en, &rot);
    CHECK(!en, "rotation on by default");
    ID3D11VideoContext_VideoProcessorGetStreamFilter(ctx, vp, 0, D3D11_VIDEO_PROCESSOR_FILTER_BRIGHTNESS, &en, &lvl);
    CHECK(!en, "filter on by default");

    /* round trips on stream 0 */
    ID3D11VideoContext_VideoProcessorSetStreamFrameFormat(ctx, vp, 0, D3D11_VIDEO_FRAME_FORMAT_INTERLACED_BOTTOM_FIELD_FIRST);
    ID3D11VideoContext_VideoProcessorGetStreamFrameFormat(ctx, vp, 0, &ff);
    CHECK(ff == D3D11_VIDEO_FRAME_FORMAT_INTERLACED_BOTTOM_FIELD_FIRST, "frame format %d", ff);

    cs.Usage = 1; cs.RGB_Range = 1; cs.YCbCr_Matrix = 1; cs.YCbCr_xvYCC = 1; cs.Nominal_Range = 2;
    ID3D11VideoContext_VideoProcessorSetStreamColorSpace(ctx, vp, 0, &cs);
    memset(&cs2, 0xff, sizeof(cs2));
    ID3D11VideoContext_VideoProcessorGetStreamColorSpace(ctx, vp, 0, &cs2);
    CHECK(cs2.Usage == 1 && cs2.RGB_Range == 1 && cs2.YCbCr_Matrix == 1 && cs2.YCbCr_xvYCC == 1 && cs2.Nominal_Range == 2, "colour space");

    ID3D11VideoContext_VideoProcessorSetStreamOutputRate(ctx, vp, 0, D3D11_VIDEO_PROCESSOR_OUTPUT_RATE_CUSTOM, FALSE, &custom);
    ID3D11VideoContext_VideoProcessorGetStreamOutputRate(ctx, vp, 0, &rate, &rep, &c2);
    CHECK(rate == D3D11_VIDEO_PROCESSOR_OUTPUT_RATE_CUSTOM && !rep && c2.Numerator == 30 && c2.Denominator == 1, "output rate %d %d %u/%u", rate, rep, c2.Numerator, c2.Denominator);
    ID3D11VideoContext_VideoProcessorSetStreamOutputRate(ctx, vp, 0, D3D11_VIDEO_PROCESSOR_OUTPUT_RATE_HALF, TRUE, NULL);
    ID3D11VideoContext_VideoProcessorGetStreamOutputRate(ctx, vp, 0, &rate, &rep, &c2);
    CHECK(rate == D3D11_VIDEO_PROCESSOR_OUTPUT_RATE_HALF && rep, "half rate %d %d", rate, rep);

    ID3D11VideoContext_VideoProcessorSetStreamSourceRect(ctx, vp, 0, TRUE, &r);
    memset(&r2, 0, sizeof(r2)); en = FALSE;
    ID3D11VideoContext_VideoProcessorGetStreamSourceRect(ctx, vp, 0, &en, &r2);
    CHECK(en && rect_eq(&r2, 1, 2, 30, 40), "source rect %d %s", en, r2.left);
    ID3D11VideoContext_VideoProcessorSetStreamSourceRect(ctx, vp, 0, FALSE, NULL);
    ID3D11VideoContext_VideoProcessorGetStreamSourceRect(ctx, vp, 0, &en, &r2);
    CHECK(!en, "source rect still enabled");

    ID3D11VideoContext_VideoProcessorSetStreamDestRect(ctx, vp, 0, TRUE, &r);
    ID3D11VideoContext_VideoProcessorGetStreamDestRect(ctx, vp, 0, &en, &r2);
    CHECK(en && rect_eq(&r2, 1, 2, 30, 40), "dest rect");

    ID3D11VideoContext_VideoProcessorSetStreamAlpha(ctx, vp, 0, TRUE, 0.25f);
    ID3D11VideoContext_VideoProcessorGetStreamAlpha(ctx, vp, 0, &en, &a);
    CHECK(en && a == 0.25f, "alpha %d %f", en, a);

    ID3D11VideoContext_VideoProcessorSetStreamPalette(ctx, vp, 0, 4, pal);
    memset(pal2, 0xab, sizeof(pal2));
    ID3D11VideoContext_VideoProcessorGetStreamPalette(ctx, vp, 0, 6, pal2);
    CHECK(!memcmp(pal2, pal, sizeof(pal)) && pal2[4] == 0 && pal2[5] == 0, "palette %08x %08x %08x", pal2[0], pal2[3], pal2[4]);

    ID3D11VideoContext_VideoProcessorSetStreamPixelAspectRatio(ctx, vp, 0, TRUE, &sa, &da);
    ID3D11VideoContext_VideoProcessorGetStreamPixelAspectRatio(ctx, vp, 0, &en, &c2, &custom);
    CHECK(en && c2.Numerator == 1 && c2.Denominator == 1 && custom.Numerator == 4 && custom.Denominator == 3, "pixel aspect");

    ID3D11VideoContext_VideoProcessorSetStreamLumaKey(ctx, vp, 0, TRUE, 0.125f, 0.5f);
    ID3D11VideoContext_VideoProcessorGetStreamLumaKey(ctx, vp, 0, &en, &lo, &hi);
    CHECK(en && lo == 0.125f && hi == 0.5f, "luma key");

    ID3D11VideoContext_VideoProcessorSetStreamStereoFormat(ctx, vp, 0, TRUE, D3D11_VIDEO_PROCESSOR_STEREO_FORMAT_SEPARATE, TRUE, FALSE, D3D11_VIDEO_PROCESSOR_STEREO_FLIP_FRAME1, -3);
    ID3D11VideoContext_VideoProcessorGetStreamStereoFormat(ctx, vp, 0, &en, &sf, &lh, &bh, &fm, &mono);
    CHECK(en && sf == D3D11_VIDEO_PROCESSOR_STEREO_FORMAT_SEPARATE && lh && !bh && fm == D3D11_VIDEO_PROCESSOR_STEREO_FLIP_FRAME1 && mono == -3, "stereo %d %d %d %d %d %d", en, sf, lh, bh, fm, mono);

    ID3D11VideoContext_VideoProcessorSetStreamAutoProcessingMode(ctx, vp, 0, FALSE);
    ID3D11VideoContext_VideoProcessorGetStreamAutoProcessingMode(ctx, vp, 0, &en);
    CHECK(!en, "auto processing");

    ID3D11VideoContext_VideoProcessorSetStreamFilter(ctx, vp, 0, D3D11_VIDEO_PROCESSOR_FILTER_HUE, TRUE, -7);
    ID3D11VideoContext_VideoProcessorGetStreamFilter(ctx, vp, 0, D3D11_VIDEO_PROCESSOR_FILTER_HUE, &en, &lvl);
    CHECK(en && lvl == -7, "filter hue");
    ID3D11VideoContext_VideoProcessorGetStreamFilter(ctx, vp, 0, D3D11_VIDEO_PROCESSOR_FILTER_BRIGHTNESS, &en, &lvl);
    CHECK(!en, "filter brightness leaked from hue");

    ID3D11VideoContext_VideoProcessorSetStreamRotation(ctx, vp, 0, TRUE, D3D11_VIDEO_PROCESSOR_ROTATION_180);
    ID3D11VideoContext_VideoProcessorGetStreamRotation(ctx, vp, 0, &en, &rot);
    CHECK(en && rot == D3D11_VIDEO_PROCESSOR_ROTATION_180, "rotation");

    /* state is per stream and per processor */
    ID3D11VideoContext_VideoProcessorGetStreamFrameFormat(ctx, vp, 1, &ff);
    CHECK(ff == D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE, "stream 1 sees stream 0 frame format");
    ID3D11VideoContext_VideoProcessorGetStreamDestRect(ctx, vp, 1, &en, &r2);
    CHECK(!en, "stream 1 sees stream 0 dest rect");
    ID3D11VideoContext_VideoProcessorGetStreamAlpha(ctx, vp, 1, &en, &a);
    CHECK(!en, "stream 1 sees stream 0 alpha");
    ID3D11VideoContext_VideoProcessorGetStreamFrameFormat(ctx, vp2, 0, &ff);
    CHECK(ff == D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE, "second processor shares state");
    ID3D11VideoContext_VideoProcessorGetStreamAutoProcessingMode(ctx, vp2, 0, &en);
    CHECK(en, "second processor shares auto mode");

    /* last stream state works, one past it is ignored and leaves outputs alone */
    ID3D11VideoContext_VideoProcessorSetStreamAlpha(ctx, vp, 15, TRUE, 0.5f);
    ID3D11VideoContext_VideoProcessorGetStreamAlpha(ctx, vp, 15, &en, &a);
    CHECK(en && a == 0.5f, "stream 15");
    en = 0x55; a = 7.0f;
    ID3D11VideoContext_VideoProcessorSetStreamAlpha(ctx, vp, 16, TRUE, 0.75f);
    ID3D11VideoContext_VideoProcessorGetStreamAlpha(ctx, vp, 16, &en, &a);
    CHECK(en == 0x55 && a == 7.0f, "stream 16 answered: %d %f", en, a);
    ID3D11VideoContext_VideoProcessorGetStreamAlpha(ctx, vp, 0, &en, &a);
    CHECK(en && a == 0.25f, "stream 16 write changed stream 0");

    /* ClearState of the device context does not touch the processor */
    {
        ID3D11DeviceContext *dc = NULL;
        ID3D11VideoContext_QueryInterface(ctx, &IID_ID3D11DeviceContext, (void **)&dc);
        if (dc) { ID3D11DeviceContext_ClearState(dc); ID3D11DeviceContext_Release(dc); }
        ID3D11VideoContext_VideoProcessorGetStreamAlpha(ctx, vp, 0, &en, &a);
        CHECK(en && a == 0.25f, "ClearState reset the processor");
    }

    /* driver extensions: this implementation has none */
    hr = ID3D11VideoContext_VideoProcessorSetStreamExtension(ctx, vp, 0, &g, sizeof(buf), buf);
    CHECK(FAILED(hr), "stream extension %#x", HR(hr));
    hr = ID3D11VideoContext_VideoProcessorGetStreamExtension(ctx, vp, 0, &g, sizeof(buf), buf);
    CHECK(FAILED(hr), "get stream extension %#x", HR(hr));
    hr = ID3D11VideoContext_VideoProcessorSetOutputExtension(ctx, vp, &g, sizeof(buf), buf);
    CHECK(FAILED(hr), "output extension %#x", HR(hr));
    hr = ID3D11VideoContext_VideoProcessorGetOutputExtension(ctx, vp, &g, sizeof(buf), buf);
    CHECK(FAILED(hr), "get output extension %#x", HR(hr));
    (void)i;
}

static void test_output_state(ID3D11VideoContext *ctx, ID3D11VideoProcessor *vp, ID3D11VideoProcessor *vp2)
{
    D3D11_VIDEO_PROCESSOR_COLOR_SPACE cs = {0}, cs2;
    D3D11_VIDEO_PROCESSOR_ALPHA_FILL_MODE mode;
    D3D11_VIDEO_COLOR col, col2;
    RECT r = {5, 6, 70, 80}, r2;
    SIZE sz = {320, 200}, sz2;
    BOOL en, yuv;
    UINT idx;

    ID3D11VideoContext_VideoProcessorGetOutputTargetRect(ctx, vp, &en, &r2);
    CHECK(!en, "target rect enabled by default");
    ID3D11VideoContext_VideoProcessorGetOutputAlphaFillMode(ctx, vp, &mode, &idx);
    CHECK(mode == D3D11_VIDEO_PROCESSOR_ALPHA_FILL_MODE_OPAQUE, "default alpha fill %d", mode);
    ID3D11VideoContext_VideoProcessorGetOutputConstriction(ctx, vp, &en, &sz2);
    CHECK(!en, "constriction on by default");
    ID3D11VideoContext_VideoProcessorGetOutputStereoMode(ctx, vp, &en);
    CHECK(!en, "output stereo on by default");

    ID3D11VideoContext_VideoProcessorSetOutputTargetRect(ctx, vp, TRUE, &r);
    ID3D11VideoContext_VideoProcessorGetOutputTargetRect(ctx, vp, &en, &r2);
    CHECK(en && rect_eq(&r2, 5, 6, 70, 80), "target rect");

    col.RGBA.R = 0.1f; col.RGBA.G = 0.2f; col.RGBA.B = 0.3f; col.RGBA.A = 0.4f;
    ID3D11VideoContext_VideoProcessorSetOutputBackgroundColor(ctx, vp, TRUE, &col);
    memset(&col2, 0, sizeof(col2));
    ID3D11VideoContext_VideoProcessorGetOutputBackgroundColor(ctx, vp, &yuv, &col2);
    CHECK(yuv && col2.RGBA.R == 0.1f && col2.RGBA.G == 0.2f && col2.RGBA.B == 0.3f && col2.RGBA.A == 0.4f, "background");

    cs.Usage = 1; cs.RGB_Range = 1; cs.Nominal_Range = 1;
    ID3D11VideoContext_VideoProcessorSetOutputColorSpace(ctx, vp, &cs);
    ID3D11VideoContext_VideoProcessorGetOutputColorSpace(ctx, vp, &cs2);
    CHECK(cs2.Usage == 1 && cs2.RGB_Range == 1 && cs2.Nominal_Range == 1, "output colour space");

    ID3D11VideoContext_VideoProcessorSetOutputAlphaFillMode(ctx, vp, D3D11_VIDEO_PROCESSOR_ALPHA_FILL_MODE_SOURCE_STREAM, 3);
    ID3D11VideoContext_VideoProcessorGetOutputAlphaFillMode(ctx, vp, &mode, &idx);
    CHECK(mode == D3D11_VIDEO_PROCESSOR_ALPHA_FILL_MODE_SOURCE_STREAM && idx == 3, "alpha fill %d %u", mode, idx);
    /* the stream index only counts for SOURCE_STREAM */
    ID3D11VideoContext_VideoProcessorSetOutputAlphaFillMode(ctx, vp, D3D11_VIDEO_PROCESSOR_ALPHA_FILL_MODE_BACKGROUND, 9);
    ID3D11VideoContext_VideoProcessorGetOutputAlphaFillMode(ctx, vp, &mode, &idx);
    CHECK(mode == D3D11_VIDEO_PROCESSOR_ALPHA_FILL_MODE_BACKGROUND && idx == 3, "alpha fill index taken for another mode: %d %u", mode, idx);

    ID3D11VideoContext_VideoProcessorSetOutputConstriction(ctx, vp, TRUE, sz);
    ID3D11VideoContext_VideoProcessorGetOutputConstriction(ctx, vp, &en, &sz2);
    CHECK(en && sz2.cx == 320 && sz2.cy == 200, "constriction");

    ID3D11VideoContext_VideoProcessorSetOutputStereoMode(ctx, vp, TRUE);
    ID3D11VideoContext_VideoProcessorGetOutputStereoMode(ctx, vp, &en);
    CHECK(en, "output stereo");

    ID3D11VideoContext_VideoProcessorGetOutputTargetRect(ctx, vp2, &en, &r2);
    CHECK(!en, "second processor shares output state");
    ID3D11VideoContext_VideoProcessorGetOutputStereoMode(ctx, vp2, &en);
    CHECK(!en, "second processor shares stereo mode");
}

int main(void)
{
    ID3D11Device *dev = make_device();
    ID3D11VideoDevice *vdev = NULL;
    ID3D11VideoContext *ctx = NULL;
    ID3D11DeviceContext *dc = NULL;
    ID3D11VideoProcessorEnumerator *en = NULL;
    ID3D11VideoProcessorEnumerator1 *en1 = NULL;
    ID3D11VideoProcessor *vp = NULL, *vp2 = NULL, *vpx = NULL;
    D3D11_VIDEO_PROCESSOR_CONTENT_DESC cd = {0}, cd2;
    D3D11_VIDEO_PROCESSOR_CAPS caps;
    D3D11_VIDEO_PROCESSOR_RATE_CONVERSION_CAPS rc;
    D3D11_VIDEO_PROCESSOR_CUSTOM_RATE cr;
    D3D11_VIDEO_PROCESSOR_FILTER_RANGE fr;
    D3D11_VIDEO_PROCESSOR_INPUT_VIEW_DESC ivd = {0}, ivd2;
    D3D11_VIDEO_PROCESSOR_OUTPUT_VIEW_DESC ovd = {0}, ovd2;
    D3D11_VIDEO_CONTENT_PROTECTION_CAPS cp;
    D3D11_VIDEO_DECODER_DESC dd = {0}, dd2;
    D3D11_VIDEO_DECODER_CONFIG dcfg = {{0}}, dcfg2;
    D3D11_VIDEO_DECODER_OUTPUT_VIEW_DESC dov = {{0}}, dov2;
    ID3D11VideoProcessorInputView *iv = NULL, *iv2 = NULL;
    ID3D11VideoProcessorOutputView *ov2x = NULL;
    ID3D11VideoProcessorOutputView *ov = NULL;
    ID3D11VideoDecoder *dec = NULL;
    ID3D11VideoDecoderOutputView *dview = NULL;
    ID3D11Texture2D *tex, *tex_ar;
    ID3D11Buffer *buf = NULL;
    ID3D11Resource *res;
    D3D11_BUFFER_DESC bd = {0};
    ID3D11VideoDevice1 *vdev1 = NULL;
    UINT flags, n, dcaps, priv;
    BOOL b, rt, supported;
    GUID g = {0x12345678, 1, 2, {3, 4, 5, 6, 7, 8, 9, 10}}, out_g;
    HRESULT hr;
    ULONG ref;
    char pd[8];

    if (!dev) { printf("SKIP no d3d11 device\nRESULT: PASS\n"); return 0; }
    hr = ID3D11Device_QueryInterface(dev, &IID_ID3D11VideoDevice, (void **)&vdev);
    CHECK(SUCCEEDED(hr), "no ID3D11VideoDevice: %#x", HR(hr));
    ID3D11Device_GetImmediateContext(dev, &dc);
    hr = ID3D11DeviceContext_QueryInterface(dc, &IID_ID3D11VideoContext, (void **)&ctx);
    CHECK(SUCCEEDED(hr), "no ID3D11VideoContext: %#x", HR(hr));
    if (!vdev || !ctx) { printf("RESULT: FAIL\n"); return 1; }

    /* ---- enumerator */
    cd.InputFrameFormat = D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE;
    cd.InputFrameRate.Numerator = 30; cd.InputFrameRate.Denominator = 1;
    cd.InputWidth = 640; cd.InputHeight = 480;
    cd.OutputFrameRate.Numerator = 60; cd.OutputFrameRate.Denominator = 1;
    cd.OutputWidth = 1280; cd.OutputHeight = 720;
    cd.Usage = D3D11_VIDEO_USAGE_PLAYBACK_NORMAL;
    hr = ID3D11VideoDevice_CreateVideoProcessorEnumerator(vdev, NULL, &en);
    CHECK(hr == E_INVALIDARG, "NULL desc: %#x", HR(hr));
    hr = ID3D11VideoDevice_CreateVideoProcessorEnumerator(vdev, &cd, NULL);
    CHECK(hr == E_INVALIDARG, "NULL out: %#x", HR(hr));
    hr = ID3D11VideoDevice_CreateVideoProcessorEnumerator(vdev, &cd, &en);
    CHECK(hr == S_OK && en, "enumerator: %#x", HR(hr));
    if (!en) { printf("RESULT: FAIL\n"); return 1; }
    hr = ID3D11VideoProcessorEnumerator_GetVideoProcessorContentDesc(en, &cd2);
    CHECK(hr == S_OK && !memcmp(&cd, &cd2, sizeof(cd)), "content desc round trip");
    CHECK(ID3D11VideoProcessorEnumerator_GetVideoProcessorContentDesc(en, NULL) == E_INVALIDARG, "content desc NULL");
    hr = ID3D11VideoProcessorEnumerator_QueryInterface(en, &IID_ID3D11VideoProcessorEnumerator1, (void **)&en1);
    CHECK(SUCCEEDED(hr), "enumerator1 %#x", HR(hr));

    memset(&caps, 0xff, sizeof(caps));
    hr = ID3D11VideoProcessorEnumerator_GetVideoProcessorCaps(en, &caps);
    CHECK(hr == S_OK && !caps.DeviceCaps && !caps.FeatureCaps && !caps.FilterCaps && !caps.InputFormatCaps
            && !caps.AutoStreamCaps && !caps.StereoCaps, "caps not zero: %x %x %x %x %x %x",
            caps.DeviceCaps, caps.FeatureCaps, caps.FilterCaps, caps.InputFormatCaps, caps.AutoStreamCaps, caps.StereoCaps);
    CHECK(caps.RateConversionCapsCount == 1 && caps.MaxStreamStates == 16 && caps.MaxInputStreams == 0,
            "counts %u %u %u", caps.RateConversionCapsCount, caps.MaxInputStreams, caps.MaxStreamStates);
    CHECK(ID3D11VideoProcessorEnumerator_GetVideoProcessorCaps(en, NULL) == E_INVALIDARG, "caps NULL");

    flags = 0x1234;
    hr = ID3D11VideoProcessorEnumerator_CheckVideoProcessorFormat(en, DXGI_FORMAT_NV12, &flags);
    CHECK(hr == S_OK && flags == 0, "NV12 flags %#x hr %#x", flags, HR(hr));
    flags = 0x1234;
    hr = ID3D11VideoProcessorEnumerator_CheckVideoProcessorFormat(en, DXGI_FORMAT_B8G8R8A8_UNORM, &flags);
    CHECK(hr == S_OK && flags == 0, "BGRA flags %#x hr %#x", flags, HR(hr));
    CHECK(ID3D11VideoProcessorEnumerator_CheckVideoProcessorFormat(en, DXGI_FORMAT_NV12, NULL) == E_INVALIDARG, "format NULL");

    memset(&rc, 0xff, sizeof(rc));
    hr = ID3D11VideoProcessorEnumerator_GetVideoProcessorRateConversionCaps(en, 0, &rc);
    CHECK(hr == S_OK && !rc.PastFrames && !rc.FutureFrames && !rc.ProcessorCaps && !rc.ITelecineCaps && !rc.CustomRateCount, "rate caps");
    CHECK(ID3D11VideoProcessorEnumerator_GetVideoProcessorRateConversionCaps(en, 1, &rc) == E_INVALIDARG, "rate caps idx 1");
    CHECK(ID3D11VideoProcessorEnumerator_GetVideoProcessorCustomRate(en, 0, 0, &cr) == E_INVALIDARG, "custom rate");
    CHECK(ID3D11VideoProcessorEnumerator_GetVideoProcessorFilterRange(en, D3D11_VIDEO_PROCESSOR_FILTER_BRIGHTNESS, &fr) == E_INVALIDARG, "filter range");
    if (en1)
    {
        supported = TRUE;
        hr = ID3D11VideoProcessorEnumerator1_CheckVideoProcessorFormatConversion(en1, DXGI_FORMAT_NV12, DXGI_COLOR_SPACE_YCBCR_STUDIO_G22_LEFT_P709,
                DXGI_FORMAT_B8G8R8A8_UNORM, DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709, &supported);
        CHECK(hr == S_OK && !supported, "format conversion %#x %d", HR(hr), supported);
        CHECK(ID3D11VideoProcessorEnumerator1_CheckVideoProcessorFormatConversion(en1, DXGI_FORMAT_NV12, 0, DXGI_FORMAT_NV12, 0, NULL) == E_INVALIDARG, "conversion NULL");
    }

    /* private data and device child */
    {
        ID3D11Device *d2 = NULL;
        UINT sz = sizeof(pd);
        memcpy(pd, "abcdefg", 8);
        hr = ID3D11VideoProcessorEnumerator_SetPrivateData(en, &g, 8, pd);
        CHECK(hr == S_OK, "enum SetPrivateData %#x", HR(hr));
        memset(pd, 0, 8);
        hr = ID3D11VideoProcessorEnumerator_GetPrivateData(en, &g, &sz, pd);
        CHECK(hr == S_OK && sz == 8 && !memcmp(pd, "abcdefg", 8), "enum GetPrivateData");
        ID3D11VideoProcessorEnumerator_GetDevice(en, &d2);
        CHECK(d2 == dev, "enum device %p vs %p", d2, dev);
        if (d2) ID3D11Device_Release(d2);
    }

    /* ---- processor */
    hr = ID3D11VideoDevice_CreateVideoProcessor(vdev, en, 1, &vpx);
    CHECK(hr == E_INVALIDARG && !vpx, "rate index 1: %#x", HR(hr));
    hr = ID3D11VideoDevice_CreateVideoProcessor(vdev, NULL, 0, &vpx);
    CHECK(hr == E_INVALIDARG, "NULL enumerator: %#x", HR(hr));
    hr = ID3D11VideoDevice_CreateVideoProcessor(vdev, en, 0, &vp);
    CHECK(hr == S_OK && vp, "processor: %#x", HR(hr));
    hr = ID3D11VideoDevice_CreateVideoProcessor(vdev, en, 0, &vp2);
    CHECK(hr == S_OK && vp2 && vp2 != vp, "processor 2: %#x", HR(hr));
    if (!vp || !vp2) { printf("RESULT: FAIL\n"); return 1; }
    memset(&cd2, 0, sizeof(cd2));
    ID3D11VideoProcessor_GetContentDesc(vp, &cd2);
    CHECK(!memcmp(&cd, &cd2, sizeof(cd)), "processor content desc");
    memset(&rc, 0xff, sizeof(rc));
    ID3D11VideoProcessor_GetRateConversionCaps(vp, &rc);
    CHECK(!rc.PastFrames && !rc.FutureFrames && !rc.ProcessorCaps && !rc.CustomRateCount, "processor rate caps");
    /* the processor keeps the enumerator alive */
    ref = ID3D11VideoProcessorEnumerator_AddRef(en);
    ID3D11VideoProcessorEnumerator_Release(en);
    {
        ULONG r1 = ref;
        ID3D11VideoProcessor *vp3 = NULL;
        ID3D11VideoDevice_CreateVideoProcessor(vdev, en, 0, &vp3);
        ref = ID3D11VideoProcessorEnumerator_AddRef(en);
        ID3D11VideoProcessorEnumerator_Release(en);
        CHECK(ref == r1 + 1, "processor does not hold the enumerator: %lu -> %lu", r1, ref);
        if (vp3) ID3D11VideoProcessor_Release(vp3);
    }

    test_output_state(ctx, vp, vp2);
    test_stream_state(ctx, vp, vp2);

    /* ---- views */
    tex = make_tex(dev, 64, 64, 3, 1);
    tex_ar = make_tex(dev, 32, 32, 1, 4);
    bd.ByteWidth = 256; bd.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    ID3D11Device_CreateBuffer(dev, &bd, NULL, &buf);
    CHECK(tex && tex_ar && buf, "resources");
    if (tex && tex_ar && buf)
    {
        ivd.FourCC = 0; ivd.ViewDimension = D3D11_VPIV_DIMENSION_TEXTURE2D;
        ivd.Texture2D.MipSlice = 2; ivd.Texture2D.ArraySlice = 0;
        hr = ID3D11VideoDevice_CreateVideoProcessorInputView(vdev, (ID3D11Resource *)tex, en, &ivd, &iv);
        CHECK(hr == S_OK && iv, "input view %#x", HR(hr));
        if (iv)
        {
            memset(&ivd2, 0, sizeof(ivd2));
            ID3D11VideoProcessorInputView_GetDesc(iv, &ivd2);
            CHECK(ivd2.ViewDimension == D3D11_VPIV_DIMENSION_TEXTURE2D && ivd2.Texture2D.MipSlice == 2 && ivd2.Texture2D.ArraySlice == 0, "input view desc");
            res = NULL;
            ID3D11VideoProcessorInputView_GetResource(iv, &res);
            CHECK(res == (ID3D11Resource *)tex, "input view resource");
            if (res) ID3D11Resource_Release(res);
            hr = ID3D11VideoProcessorInputView_QueryInterface(iv, &IID_ID3D11View, (void **)&res);
            CHECK(hr == S_OK, "input view is not an ID3D11View");
            if (hr == S_OK) ID3D11Resource_Release(res);
        }
        ivd.Texture2D.MipSlice = 3;
        hr = ID3D11VideoDevice_CreateVideoProcessorInputView(vdev, (ID3D11Resource *)tex, en, &ivd, &iv2);
        CHECK(hr == E_INVALIDARG && !iv2, "mip out of range: %#x", HR(hr));
        ivd.Texture2D.MipSlice = 0; ivd.Texture2D.ArraySlice = 1;
        hr = ID3D11VideoDevice_CreateVideoProcessorInputView(vdev, (ID3D11Resource *)tex, en, &ivd, &iv2);
        CHECK(hr == E_INVALIDARG, "slice out of range: %#x", HR(hr));
        ivd.Texture2D.ArraySlice = 0; ivd.ViewDimension = D3D11_VPIV_DIMENSION_UNKNOWN;
        hr = ID3D11VideoDevice_CreateVideoProcessorInputView(vdev, (ID3D11Resource *)tex, en, &ivd, &iv2);
        CHECK(hr == E_INVALIDARG, "unknown dimension: %#x", HR(hr));
        ivd.ViewDimension = D3D11_VPIV_DIMENSION_TEXTURE2D;
        hr = ID3D11VideoDevice_CreateVideoProcessorInputView(vdev, (ID3D11Resource *)buf, en, &ivd, &iv2);
        CHECK(hr == E_INVALIDARG, "buffer resource: %#x", HR(hr));
        hr = ID3D11VideoDevice_CreateVideoProcessorInputView(vdev, (ID3D11Resource *)tex, en, NULL, &iv2);
        CHECK(hr == E_INVALIDARG, "NULL desc: %#x", HR(hr));
        hr = ID3D11VideoDevice_CreateVideoProcessorInputView(vdev, NULL, en, &ivd, &iv2);
        CHECK(hr == E_INVALIDARG, "NULL resource: %#x", HR(hr));
        hr = ID3D11VideoDevice_CreateVideoProcessorInputView(vdev, (ID3D11Resource *)tex, NULL, &ivd, &iv2);
        CHECK(hr == E_INVALIDARG, "NULL enumerator: %#x", HR(hr));

        ovd.ViewDimension = D3D11_VPOV_DIMENSION_TEXTURE2DARRAY;
        ovd.Texture2DArray.MipSlice = 0; ovd.Texture2DArray.FirstArraySlice = 1; ovd.Texture2DArray.ArraySize = 3;
        hr = ID3D11VideoDevice_CreateVideoProcessorOutputView(vdev, (ID3D11Resource *)tex_ar, en, &ovd, &ov);
        CHECK(hr == S_OK && ov, "output view %#x", HR(hr));
        if (ov)
        {
            memset(&ovd2, 0, sizeof(ovd2));
            ID3D11VideoProcessorOutputView_GetDesc(ov, &ovd2);
            CHECK(ovd2.ViewDimension == D3D11_VPOV_DIMENSION_TEXTURE2DARRAY && ovd2.Texture2DArray.FirstArraySlice == 1 && ovd2.Texture2DArray.ArraySize == 3, "output view desc");
        }
        ovd.Texture2DArray.ArraySize = 4;
        hr = ID3D11VideoDevice_CreateVideoProcessorOutputView(vdev, (ID3D11Resource *)tex_ar, en, &ovd, &ov2x);
        CHECK(hr == E_INVALIDARG, "array range: %#x", HR(hr));
        ovd.ViewDimension = D3D11_VPOV_DIMENSION_UNKNOWN;
        {
            ID3D11VideoProcessorOutputView *ov2 = NULL;
            hr = ID3D11VideoDevice_CreateVideoProcessorOutputView(vdev, (ID3D11Resource *)tex_ar, en, &ovd, &ov2);
            CHECK(hr == E_INVALIDARG, "unknown output dimension: %#x", HR(hr));
        }

        /* the view keeps its resource alive */
        if (iv)
        {
            ULONG before, after;
            ID3D11Texture2D_AddRef(tex); before = ID3D11Texture2D_Release(tex);
            ID3D11Texture2D_Release(tex); tex = NULL;
            ID3D11VideoProcessorInputView_GetResource(iv, &res);
            after = ID3D11Resource_Release(res);
            CHECK(after >= 1, "resource died with the view alive (%lu %lu)", before, after);
        }
    }

    /* ---- video device queries */
    CHECK(ID3D11VideoDevice_GetVideoDecoderProfileCount(vdev) == 0, "profile count");
    hr = ID3D11VideoDevice_GetVideoDecoderProfile(vdev, 0, &out_g);
    CHECK(FAILED(hr), "profile 0: %#x", HR(hr));
    b = TRUE;
    hr = ID3D11VideoDevice_CheckVideoDecoderFormat(vdev, &g, DXGI_FORMAT_NV12, &b);
    CHECK(hr == E_INVALIDARG && !b, "decoder format: %#x %d", HR(hr), b);
    dd.Guid = g; dd.SampleWidth = 640; dd.SampleHeight = 480; dd.OutputFormat = DXGI_FORMAT_NV12;
    n = 77;
    hr = ID3D11VideoDevice_GetVideoDecoderConfigCount(vdev, &dd, &n);
    CHECK(FAILED(hr) && n == 0, "config count: %#x %u", HR(hr), n);
    hr = ID3D11VideoDevice_GetVideoDecoderConfig(vdev, &dd, 0, &dcfg);
    CHECK(FAILED(hr), "config 0: %#x", HR(hr));
    memset(&cp, 0xff, sizeof(cp));
    hr = ID3D11VideoDevice_GetContentProtectionCaps(vdev, NULL, NULL, &cp);
    CHECK(hr == S_OK && !cp.Caps && !cp.KeyExchangeTypeCount && !cp.BlockAlignmentSize && !cp.ProtectedMemorySize, "protection caps");
    CHECK(ID3D11VideoDevice_GetContentProtectionCaps(vdev, NULL, NULL, NULL) == E_INVALIDARG, "protection caps NULL");
    hr = ID3D11VideoDevice_CheckCryptoKeyExchange(vdev, &g, NULL, 0, &out_g);
    CHECK(FAILED(hr), "key exchange: %#x", HR(hr));

    hr = ID3D11VideoDevice_QueryInterface(vdev, &IID_ID3D11VideoDevice1, (void **)&vdev1);
    if (SUCCEEDED(hr))
    {
        DXGI_RATIONAL fr30 = {30, 1};
        D3D11_VIDEO_SAMPLE_DESC sd = {0};
        dcaps = 0;
        hr = ID3D11VideoDevice1_GetVideoDecoderCaps(vdev1, &g, 640, 480, &fr30, 0, NULL, &dcaps);
        CHECK(hr == S_OK && (dcaps & D3D11_VIDEO_DECODER_CAPS_UNSUPPORTED), "decoder caps %#x hr %#x", dcaps, HR(hr));
        supported = TRUE; rt = TRUE;
        hr = ID3D11VideoDevice1_CheckVideoDecoderDownsampling(vdev1, &dd, DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709, &dcfg, &fr30, &sd, &supported, &rt);
        CHECK(hr == S_OK && !supported && !rt, "downsampling %#x %d %d", HR(hr), supported, rt);
        hr = ID3D11VideoDevice1_RecommendVideoDecoderDownsampleParameters(vdev1, &dd, DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709, &dcfg, &fr30, &sd);
        CHECK(FAILED(hr), "recommend downsample %#x", HR(hr));
        ID3D11VideoDevice1_Release(vdev1);
    }

    /* private data set through the video device is the device's */
    priv = 8;
    memcpy(pd, "vdevdat", 8);
    {
        GUID g2 = {0x9abcdef0, 1, 2, {3, 4, 5, 6, 7, 8, 9, 11}};
        hr = ID3D11VideoDevice_SetPrivateData(vdev, &g2, 8, pd);
        CHECK(hr == S_OK, "video device SetPrivateData %#x", HR(hr));
        memset(pd, 0, 8);
        hr = ID3D11Device_GetPrivateData(dev, &g2, &priv, pd);
        CHECK(hr == S_OK && priv == 8 && !memcmp(pd, "vdevdat", 8), "device does not see video device private data: %#x", HR(hr));
    }

    /* ---- decoder creation parameters and output view */
    dd.Guid = g; dd.SampleWidth = 320; dd.SampleHeight = 240; dd.OutputFormat = DXGI_FORMAT_NV12;
    dcfg.guidConfigBitstreamEncryption = g; dcfg.ConfigMinRenderTargetBuffCount = 5; dcfg.ConfigDecoderSpecific = 9;
    hr = ID3D11VideoDevice_CreateVideoDecoder(vdev, &dd, &dcfg, &dec);
    if (SUCCEEDED(hr) && dec)
    {
        memset(&dd2, 0, sizeof(dd2)); memset(&dcfg2, 0, sizeof(dcfg2));
        hr = ID3D11VideoDecoder_GetCreationParameters(dec, &dd2, &dcfg2);
        CHECK(hr == S_OK && !memcmp(&dd2, &dd, sizeof(dd)) && dcfg2.ConfigMinRenderTargetBuffCount == 5
                && dcfg2.ConfigDecoderSpecific == 9 && IsEqualGUID(&dcfg2.guidConfigBitstreamEncryption, &g), "decoder creation parameters %#x", HR(hr));
        CHECK(ID3D11VideoDecoder_GetCreationParameters(dec, NULL, &dcfg2) == E_INVALIDARG, "creation parameters NULL");
        ID3D11VideoDecoder_Release(dec);
    }
    else printf("INFO CreateVideoDecoder %#x\n", HR(hr));

    {
        ID3D11Texture2D *dtex = make_tex(dev, 16, 16, 1, 2);
        dov.ViewDimension = D3D11_VDOV_DIMENSION_TEXTURE2D;
        dov.DecodeProfile = g; dov.Texture2D.ArraySlice = 1;
        hr = ID3D11VideoDevice_CreateVideoDecoderOutputView(vdev, (ID3D11Resource *)dtex, &dov, &dview);
        if (SUCCEEDED(hr) && dview)
        {
            memset(&dov2, 0, sizeof(dov2));
            ID3D11VideoDecoderOutputView_GetDesc(dview, &dov2);
            CHECK(dov2.ViewDimension == D3D11_VDOV_DIMENSION_TEXTURE2D && dov2.Texture2D.ArraySlice == 1 && IsEqualGUID(&dov2.DecodeProfile, &g), "decoder output view desc");
            ID3D11Texture2D_Release(dtex);
            res = NULL;
            ID3D11VideoDecoderOutputView_GetResource(dview, &res);
            CHECK(res != NULL, "decoder output view lost its resource");
            if (res) ID3D11Resource_Release(res);
            ID3D11VideoDecoderOutputView_Release(dview);
        }
        else { printf("INFO decoder output view %#x\n", HR(hr)); if (dtex) ID3D11Texture2D_Release(dtex); }
        CHECK(ID3D11VideoDevice_CreateVideoDecoderOutputView(vdev, NULL, &dov, &dview) == E_INVALIDARG, "NULL resource decoder output view");
    }

    /* ---- teardown */
    if (iv) ID3D11VideoProcessorInputView_Release(iv);
    if (ov) ID3D11VideoProcessorOutputView_Release(ov);
    if (tex_ar) ID3D11Texture2D_Release(tex_ar);
    if (tex) ID3D11Texture2D_Release(tex);
    if (buf) ID3D11Buffer_Release(buf);
    ID3D11VideoProcessor_Release(vp);
    ID3D11VideoProcessor_Release(vp2);
    if (en1) ID3D11VideoProcessorEnumerator1_Release(en1);
    ref = ID3D11VideoProcessorEnumerator_Release(en);
    CHECK(ref == 0, "enumerator leaked %lu refs", ref);
    ID3D11VideoContext_Release(ctx);
    ID3D11DeviceContext_Release(dc);
    ID3D11VideoDevice_Release(vdev);
    ID3D11Device_Release(dev);

    printf("RESULT: %s\n", f ? "FAIL" : "PASS");
    return f ? 1 : 0;
}
