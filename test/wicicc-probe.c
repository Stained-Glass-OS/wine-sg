/* windowscodecs: colour contexts and thumbnails on the encoder side
 * (patches/sg/2609). A profile set on a PNG, JPEG or TIFF frame has to come
 * back out of the decoder; the formats that cannot hold one, and every
 * thumbnail / preview / container metadata request, answer in the documented
 * way. Expected values are written out, not computed by the code under test. */
#define COBJMACROS
#include <windows.h>
#include <wincodec.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

static const GUID my_CLSID_WICImagingFactory = {0xcacaf262, 0x9370, 0x4615, {0xa1, 0x3b, 0x9f, 0x55, 0x39, 0xda, 0x4c, 0x0a}};
static const GUID my_IID_IWICImagingFactory = {0xec5ec8a9, 0xc395, 0x4314, {0x9c, 0x77, 0x54, 0xd7, 0xa9, 0x35, 0xff, 0x70}};
static const GUID my_fmt_24bppBGR = {0x6fddc324, 0x4e03, 0x4bfe, {0xb1, 0x85, 0x3d, 0x77, 0x76, 0x8d, 0xc9, 0x0c}};
static const GUID my_png = {0x1b7cfaf4, 0x713f, 0x473c, {0xbb, 0xcd, 0x61, 0x37, 0x42, 0x5f, 0xae, 0xaf}};
static const GUID my_jpeg = {0x19e4a5aa, 0x5662, 0x4fc5, {0xa0, 0xc0, 0x17, 0x58, 0x02, 0x8e, 0x10, 0x57}};
static const GUID my_tiff = {0x163bcc30, 0xe2e9, 0x4f0b, {0x96, 0x1d, 0xa3, 0xe9, 0xfd, 0xb7, 0x88, 0xa3}};
static const GUID my_bmp = {0x0af1d87e, 0xfcfe, 0x4188, {0xbd, 0xeb, 0xa7, 0x90, 0x64, 0x71, 0xcb, 0xe3}};
static const GUID my_gif = {0x1f8a5601, 0x7d4d, 0x4cbd, {0x9c, 0x82, 0x1b, 0xc8, 0xd4, 0xee, 0xb9, 0xa5}};

static int failures;
static IWICImagingFactory *factory;

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

/* A profile with a plausible header: its own length at the front, the
 * 'acsp' signature at offset 36, an empty tag table, then a byte pattern. */
static BYTE *make_profile(UINT size, BYTE seed)
{
    BYTE *p = calloc(1, size);
    UINT i;

    p[0] = size >> 24; p[1] = size >> 16; p[2] = size >> 8; p[3] = size;
    p[8] = 2; p[9] = 0x10;
    memcpy(p + 12, "mntr", 4);
    memcpy(p + 16, "RGB ", 4);
    memcpy(p + 20, "XYZ ", 4);
    memcpy(p + 36, "acsp", 4);
    /* PCS illuminant D50, as s15Fixed16 */
    p[70] = 0xf6; p[71] = 0xd6; p[73] = 1; p[78] = 0xd3; p[79] = 0x2d;
    for (i = 132; i < size; i++) p[i] = (BYTE)(seed + i * 7);
    return p;
}

static IWICColorContext *profile_context(const BYTE *p, UINT size)
{
    IWICColorContext *ctx = NULL;

    if (FAILED(IWICImagingFactory_CreateColorContext(factory, &ctx))) return NULL;
    if (FAILED(IWICColorContext_InitializeFromMemory(ctx, p, size)))
    {
        IWICColorContext_Release(ctx);
        return NULL;
    }
    return ctx;
}

struct fmt
{
    const char *name;
    const GUID *container;
    int icc;            /* the format carries an embedded profile */
};

static const struct fmt formats[] = {
    {"png", &my_png, 1},
    {"jpeg", &my_jpeg, 1},
    {"tiff", &my_tiff, 1},
    {"bmp", &my_bmp, 0},
    {"gif", &my_gif, 0},
};

struct enc
{
    IStream *stream;
    IWICBitmapEncoder *encoder;
    IWICBitmapFrameEncode *frame;
};

static int open_encoder(const struct fmt *f, struct enc *e, int with_frame)
{
    HRESULT hr;

    memset(e, 0, sizeof(*e));
    if (FAILED(CreateStreamOnHGlobal(NULL, TRUE, &e->stream))) return 0;
    hr = IWICImagingFactory_CreateEncoder(factory, f->container, NULL, &e->encoder);
    if (FAILED(hr)) { check(0, "%s: CreateEncoder %#lx", f->name, hr); return 0; }
    hr = IWICBitmapEncoder_Initialize(e->encoder, e->stream, WICBitmapEncoderNoCache);
    if (FAILED(hr)) { check(0, "%s: encoder Initialize %#lx", f->name, hr); return 0; }
    if (with_frame)
    {
        hr = IWICBitmapEncoder_CreateNewFrame(e->encoder, &e->frame, NULL);
        if (FAILED(hr)) { check(0, "%s: CreateNewFrame %#lx", f->name, hr); return 0; }
    }
    return 1;
}

static void close_encoder(struct enc *e)
{
    if (e->frame) IWICBitmapFrameEncode_Release(e->frame);
    if (e->encoder) IWICBitmapEncoder_Release(e->encoder);
    if (e->stream) IStream_Release(e->stream);
}

/* Fill an 8x8 24bpp image, commit frame and file. */
static HRESULT write_image(struct enc *e)
{
    BYTE pixels[8 * 24];
    GUID pf = my_fmt_24bppBGR;
    HRESULT hr;
    UINT i;

    for (i = 0; i < 64; i++) { pixels[i * 3] = 50; pixels[i * 3 + 1] = 100; pixels[i * 3 + 2] = 200; }
    hr = IWICBitmapFrameEncode_SetSize(e->frame, 8, 8);
    if (SUCCEEDED(hr)) hr = IWICBitmapFrameEncode_SetPixelFormat(e->frame, &pf);
    if (SUCCEEDED(hr)) hr = IWICBitmapFrameEncode_WritePixels(e->frame, 8, 24, sizeof(pixels), pixels);
    return hr;
}

static HRESULT finish_image(struct enc *e)
{
    HRESULT hr = IWICBitmapFrameEncode_Commit(e->frame);
    if (SUCCEEDED(hr)) hr = IWICBitmapEncoder_Commit(e->encoder);
    return hr;
}

/* Decode what the encoder wrote; returns the number of colour contexts and
 * fills *profile with the first one. */
static UINT read_back(IStream *stream, BYTE **profile, UINT *size, BYTE *pixel)
{
    IWICBitmapDecoder *dec = NULL;
    IWICBitmapFrameDecode *frame = NULL;
    IWICColorContext *ctx = NULL;
    LARGE_INTEGER zero = {{0}};
    UINT count = ~0u, actual = 0;
    HRESULT hr;

    *profile = NULL;
    *size = 0;
    IStream_Seek(stream, zero, STREAM_SEEK_SET, NULL);
    hr = IWICImagingFactory_CreateDecoderFromStream(factory, stream, NULL, WICDecodeMetadataCacheOnDemand, &dec);
    if (FAILED(hr)) { printf("note: CreateDecoderFromStream %#lx\n", hr); return ~0u; }
    hr = IWICBitmapDecoder_GetFrame(dec, 0, &frame);
    if (FAILED(hr)) { IWICBitmapDecoder_Release(dec); return ~0u; }

    if (pixel)
    {
        WICRect rc = {0, 0, 1, 1};
        BYTE px[4] = {0};
        GUID pf;

        IWICBitmapFrameDecode_GetPixelFormat(frame, &pf);
        if (IsEqualGUID(&pf, &my_fmt_24bppBGR) &&
            SUCCEEDED(IWICBitmapFrameDecode_CopyPixels(frame, &rc, 4, 4, px)))
            memcpy(pixel, px, 3);
    }

    hr = IWICBitmapFrameDecode_GetColorContexts(frame, 0, NULL, &count);
    if (FAILED(hr)) { printf("note: GetColorContexts %#lx\n", hr); count = ~0u; }
    else if (count)
    {
        IWICImagingFactory_CreateColorContext(factory, &ctx);
        actual = 0;
        hr = IWICBitmapFrameDecode_GetColorContexts(frame, 1, &ctx, &actual);
        if (SUCCEEDED(hr))
        {
            UINT len = 0;
            IWICColorContext_GetProfileBytes(ctx, 0, NULL, &len);
            *profile = malloc(len);
            IWICColorContext_GetProfileBytes(ctx, len, *profile, size);
        }
        IWICColorContext_Release(ctx);
    }
    IWICBitmapFrameDecode_Release(frame);
    IWICBitmapDecoder_Release(dec);
    return count;
}

static int same(const BYTE *a, UINT alen, const BYTE *b, UINT blen)
{
    return a && b && alen == blen && !memcmp(a, b, alen);
}

/* ---- argument and state errors on every encoder ----------------------- */

static void test_frame_errors(const struct fmt *f)
{
    struct enc e;
    IWICColorContext *ctx, *blank;
    IWICBitmap *thumb = NULL;
    BYTE data[4] = {0};
    GUID pf = my_fmt_24bppBGR;
    BYTE *profile = make_profile(200, 3);
    HRESULT hr;
    const char *n = f->name;

    if (!open_encoder(f, &e, 1)) return;
    ctx = profile_context(profile, 200);
    IWICImagingFactory_CreateColorContext(factory, &blank);
    IWICImagingFactory_CreateBitmapFromMemory(factory, 1, 1, &pf, 4, 4, data, &thumb);

    hr = IWICBitmapFrameEncode_SetColorContexts(e.frame, 1, &ctx);
    check(hr == WINCODEC_ERR_NOTINITIALIZED, "%s: frame SetColorContexts before Initialize = %#lx", n, hr);
    hr = IWICBitmapFrameEncode_SetThumbnail(e.frame, (IWICBitmapSource *)thumb);
    check(hr == WINCODEC_ERR_NOTINITIALIZED, "%s: frame SetThumbnail before Initialize = %#lx", n, hr);

    hr = IWICBitmapFrameEncode_Initialize(e.frame, NULL);
    check(hr == S_OK, "%s: frame Initialize = %#lx", n, hr);

    hr = IWICBitmapFrameEncode_SetColorContexts(e.frame, 1, NULL);
    check(hr == E_INVALIDARG, "%s: SetColorContexts(1, NULL) = %#lx", n, hr);
    {
        IWICColorContext *none = NULL;
        hr = IWICBitmapFrameEncode_SetColorContexts(e.frame, 1, &none);
        check(hr == E_INVALIDARG, "%s: SetColorContexts with a NULL entry = %#lx", n, hr);
    }
    hr = IWICBitmapFrameEncode_SetColorContexts(e.frame, 1, &blank);
    check(hr == WINCODEC_ERR_NOTINITIALIZED, "%s: SetColorContexts with an uninitialized context = %#lx", n, hr);

    hr = IWICBitmapFrameEncode_SetColorContexts(e.frame, 1, &ctx);
    if (f->icc)
        check(hr == S_OK, "%s: SetColorContexts(profile) = %#lx", n, hr);
    else
        check(hr == WINCODEC_ERR_UNSUPPORTEDOPERATION, "%s: SetColorContexts(profile) = %#lx", n, hr);
    hr = IWICBitmapFrameEncode_SetColorContexts(e.frame, 0, NULL);
    if (f->icc)
        check(hr == S_OK, "%s: SetColorContexts(0, NULL) clears = %#lx", n, hr);

    hr = IWICBitmapFrameEncode_SetThumbnail(e.frame, NULL);
    check(hr == E_INVALIDARG, "%s: SetThumbnail(NULL) = %#lx", n, hr);
    hr = IWICBitmapFrameEncode_SetThumbnail(e.frame, (IWICBitmapSource *)thumb);
    check(hr == WINCODEC_ERR_UNSUPPORTEDOPERATION, "%s: SetThumbnail = %#lx", n, hr);

    if (f->icc)
    {
        hr = write_image(&e);
        check(hr == S_OK, "%s: image written = %#lx", n, hr);
        hr = IWICBitmapFrameEncode_SetColorContexts(e.frame, 1, &ctx);
        check(hr == WINCODEC_ERR_WRONGSTATE, "%s: SetColorContexts after WritePixels = %#lx", n, hr);
        hr = IWICBitmapFrameEncode_SetThumbnail(e.frame, (IWICBitmapSource *)thumb);
        check(hr == WINCODEC_ERR_WRONGSTATE, "%s: SetThumbnail after WritePixels = %#lx", n, hr);
    }

    if (ctx) IWICColorContext_Release(ctx);
    IWICColorContext_Release(blank);
    if (thumb) IWICBitmap_Release(thumb);
    close_encoder(&e);
    free(profile);
}

static void test_encoder_errors(const struct fmt *f)
{
    struct enc e;
    IWICColorContext *ctx;
    IWICBitmap *thumb = NULL;
    IWICMetadataQueryWriter *qw = (IWICMetadataQueryWriter *)0x1234;
    BYTE data[4] = {0};
    GUID pf = my_fmt_24bppBGR;
    BYTE *profile = make_profile(200, 3);
    HRESULT hr;
    const char *n = f->name;
    IWICBitmapEncoder *enc = NULL;
    IStream *stream;

    /* before Initialize */
    CreateStreamOnHGlobal(NULL, TRUE, &stream);
    hr = IWICImagingFactory_CreateEncoder(factory, f->container, NULL, &enc);
    ctx = profile_context(profile, 200);
    IWICImagingFactory_CreateBitmapFromMemory(factory, 1, 1, &pf, 4, 4, data, &thumb);

    hr = IWICBitmapEncoder_SetColorContexts(enc, 1, &ctx);
    check(hr == WINCODEC_ERR_NOTINITIALIZED, "%s: encoder SetColorContexts before Initialize = %#lx", n, hr);
    hr = IWICBitmapEncoder_SetThumbnail(enc, (IWICBitmapSource *)thumb);
    check(hr == WINCODEC_ERR_NOTINITIALIZED, "%s: encoder SetThumbnail before Initialize = %#lx", n, hr);
    hr = IWICBitmapEncoder_SetPreview(enc, (IWICBitmapSource *)thumb);
    check(hr == WINCODEC_ERR_NOTINITIALIZED, "%s: encoder SetPreview before Initialize = %#lx", n, hr);
    hr = IWICBitmapEncoder_GetMetadataQueryWriter(enc, &qw);
    check(hr == WINCODEC_ERR_NOTINITIALIZED && !qw, "%s: encoder GetMetadataQueryWriter before Initialize = %#lx", n, hr);
    IWICBitmapEncoder_Release(enc);
    IStream_Release(stream);

    if (!open_encoder(f, &e, 0)) return;

    hr = IWICBitmapEncoder_SetColorContexts(e.encoder, 1, NULL);
    check(hr == E_INVALIDARG, "%s: encoder SetColorContexts(1, NULL) = %#lx", n, hr);
    hr = IWICBitmapEncoder_SetColorContexts(e.encoder, 1, &ctx);
    check(hr == WINCODEC_ERR_UNSUPPORTEDOPERATION, "%s: encoder SetColorContexts = %#lx", n, hr);
    hr = IWICBitmapEncoder_SetThumbnail(e.encoder, NULL);
    check(hr == E_INVALIDARG, "%s: encoder SetThumbnail(NULL) = %#lx", n, hr);
    hr = IWICBitmapEncoder_SetThumbnail(e.encoder, (IWICBitmapSource *)thumb);
    check(hr == WINCODEC_ERR_UNSUPPORTEDOPERATION, "%s: encoder SetThumbnail = %#lx", n, hr);
    hr = IWICBitmapEncoder_SetPreview(e.encoder, NULL);
    check(hr == E_INVALIDARG, "%s: encoder SetPreview(NULL) = %#lx", n, hr);
    hr = IWICBitmapEncoder_SetPreview(e.encoder, (IWICBitmapSource *)thumb);
    check(hr == WINCODEC_ERR_UNSUPPORTEDOPERATION, "%s: encoder SetPreview = %#lx", n, hr);
    hr = IWICBitmapEncoder_GetMetadataQueryWriter(e.encoder, NULL);
    check(hr == E_INVALIDARG, "%s: encoder GetMetadataQueryWriter(NULL) = %#lx", n, hr);
    qw = (IWICMetadataQueryWriter *)0x1234;
    hr = IWICBitmapEncoder_GetMetadataQueryWriter(e.encoder, &qw);
    check(hr == WINCODEC_ERR_UNSUPPORTEDOPERATION && !qw, "%s: encoder GetMetadataQueryWriter = %#lx", n, hr);

    if (ctx) IWICColorContext_Release(ctx);
    if (thumb) IWICBitmap_Release(thumb);
    close_encoder(&e);
    free(profile);
}

/* ---- the profile makes the round trip ---------------------------------- */

static void roundtrip(const struct fmt *f, UINT size, const char *what)
{
    struct enc e;
    IWICColorContext *ctx;
    BYTE *profile = make_profile(size, 11), *back = NULL, pixel[3] = {0};
    UINT backlen, count;
    HRESULT hr;

    if (!open_encoder(f, &e, 1)) return;
    IWICBitmapFrameEncode_Initialize(e.frame, NULL);
    ctx = profile_context(profile, size);
    hr = IWICBitmapFrameEncode_SetColorContexts(e.frame, 1, &ctx);
    check(hr == S_OK, "%s %s: SetColorContexts = %#lx", f->name, what, hr);
    hr = write_image(&e);
    if (SUCCEEDED(hr)) hr = finish_image(&e);
    check(hr == S_OK, "%s %s: file written = %#lx", f->name, what, hr);

    count = read_back(e.stream, &back, &backlen, pixel);
    check(count == 1, "%s %s: one colour context read back (%u)", f->name, what, count);
    check(same(back, backlen, profile, size), "%s %s: the profile is byte for byte what was set (%u bytes back)",
          f->name, what, backlen);
    check(abs(pixel[0] - 50) < 12 && abs(pixel[1] - 100) < 12 && abs(pixel[2] - 200) < 12,
          "%s %s: the pixels survived (%u,%u,%u)", f->name, what, pixel[0], pixel[1], pixel[2]);

    free(back);
    IWICColorContext_Release(ctx);
    close_encoder(&e);
    free(profile);
}

static void test_no_profile(const struct fmt *f)
{
    struct enc e;
    IWICColorContext *ctx, *exif = NULL, *list[2];
    BYTE *profile = make_profile(300, 5), *back = NULL;
    UINT backlen, count;
    HRESULT hr;

    /* nothing set */
    if (!open_encoder(f, &e, 1)) return;
    IWICBitmapFrameEncode_Initialize(e.frame, NULL);
    write_image(&e);
    finish_image(&e);
    count = read_back(e.stream, &back, &backlen, NULL);
    check(count == 0, "%s: no profile set, none read back (%u)", f->name, count);
    close_encoder(&e);

    /* set, then cleared by an empty list */
    if (!open_encoder(f, &e, 1)) return;
    IWICBitmapFrameEncode_Initialize(e.frame, NULL);
    ctx = profile_context(profile, 300);
    IWICBitmapFrameEncode_SetColorContexts(e.frame, 1, &ctx);
    hr = IWICBitmapFrameEncode_SetColorContexts(e.frame, 0, NULL);
    check(hr == S_OK, "%s: empty list = %#lx", f->name, hr);
    write_image(&e);
    finish_image(&e);
    count = read_back(e.stream, &back, &backlen, NULL);
    check(count == 0, "%s: cleared profile is not written (%u)", f->name, count);
    close_encoder(&e);

    /* an Exif colour space context has no bytes to embed; the second entry is the profile */
    if (!open_encoder(f, &e, 1)) return;
    IWICBitmapFrameEncode_Initialize(e.frame, NULL);
    IWICImagingFactory_CreateColorContext(factory, &exif);
    IWICColorContext_InitializeFromExifColorSpace(exif, 1);
    list[0] = exif;
    list[1] = ctx;
    hr = IWICBitmapFrameEncode_SetColorContexts(e.frame, 2, list);
    check(hr == S_OK, "%s: Exif context plus profile = %#lx", f->name, hr);
    write_image(&e);
    finish_image(&e);
    count = read_back(e.stream, &back, &backlen, NULL);
    check(count == 1 && same(back, backlen, profile, 300), "%s: the profile of the list is found (%u)", f->name, count);
    free(back);
    close_encoder(&e);

    /* the second SetColorContexts replaces the first as a whole */
    {
        BYTE *other = make_profile(260, 77);
        IWICColorContext *ctx2 = profile_context(other, 260);

        if (!open_encoder(f, &e, 1)) return;
        IWICBitmapFrameEncode_Initialize(e.frame, NULL);
        IWICBitmapFrameEncode_SetColorContexts(e.frame, 1, &ctx);
        IWICBitmapFrameEncode_SetColorContexts(e.frame, 1, &ctx2);
        write_image(&e);
        finish_image(&e);
        back = NULL;
        count = read_back(e.stream, &back, &backlen, NULL);
        check(count == 1 && same(back, backlen, other, 260), "%s: a second list replaces the first (%u)", f->name, count);
        free(back);
        close_encoder(&e);
        IWICColorContext_Release(ctx2);
        free(other);
    }

    if (f->container == &my_png)
    {
        /* a profile libpng rejects (no signature) is left out, the image still gets written */
        BYTE *junk = make_profile(200, 1);
        IWICColorContext *jctx;

        memcpy(junk + 36, "xxxx", 4);
        jctx = profile_context(junk, 200);
        if (!open_encoder(f, &e, 1)) return;
        IWICBitmapFrameEncode_Initialize(e.frame, NULL);
        hr = IWICBitmapFrameEncode_SetColorContexts(e.frame, 1, &jctx);
        check(hr == S_OK, "png: malformed profile accepted by SetColorContexts = %#lx", hr);
        hr = write_image(&e);
        if (SUCCEEDED(hr)) hr = finish_image(&e);
        check(hr == S_OK, "png: image with a malformed profile still written = %#lx", hr);
        back = NULL;
        count = read_back(e.stream, &back, &backlen, NULL);
        check(count == 0, "png: the malformed profile is not embedded (%u)", count);
        free(back);
        close_encoder(&e);
        IWICColorContext_Release(jctx);
        free(junk);
    }

    IWICColorContext_Release(exif);
    IWICColorContext_Release(ctx);
    free(profile);
}

/* Decode a copy of the file bytes. */
static UINT read_back_bytes(const BYTE *file, UINT len, BYTE **profile, UINT *size)
{
    HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, len);
    IStream *copy = NULL;
    UINT count;

    memcpy(GlobalLock(h), file, len);
    GlobalUnlock(h);
    CreateStreamOnHGlobal(h, TRUE, &copy);
    *profile = NULL;
    count = read_back(copy, profile, size, NULL);
    IStream_Release(copy);
    return count;
}

/* A big profile: JPEG needs several APP2 segments. The segments are then
 * renumbered in the file and the decoder has to put them together by their
 * sequence numbers, and to refuse an inconsistent set. */
static void test_jpeg_segments(void)
{
    const struct fmt *f = &formats[1];
    struct enc e;
    IWICColorContext *ctx;
    BYTE *profile = make_profile(150000, 9), *back = NULL, *file, *seqs[3];
    UINT backlen, count, i, nseg = 0, filelen;
    UINT sz1 = 65519, sz2 = 65519, sz3 = 150000 - 2 * 65519;
    STATSTG st;
    HGLOBAL hg;

    if (!open_encoder(f, &e, 1)) return;
    IWICBitmapFrameEncode_Initialize(e.frame, NULL);
    ctx = profile_context(profile, 150000);
    IWICBitmapFrameEncode_SetColorContexts(e.frame, 1, &ctx);
    write_image(&e);
    finish_image(&e);

    count = read_back(e.stream, &back, &backlen, NULL);
    check(count == 1 && same(back, backlen, profile, 150000), "jpeg: a 150000 byte profile in several segments (%u, %u bytes)", count, backlen);
    free(back);

    IStream_Stat(e.stream, &st, STATFLAG_NONAME);
    filelen = st.cbSize.LowPart;
    GetHGlobalFromStream(e.stream, &hg);
    file = GlobalLock(hg);
    for (i = 0; i + 16 < filelen; i++)
    {
        if (file[i] == 0xff && file[i + 1] == 0xe2 && !memcmp(file + i + 4, "ICC_PROFILE", 12))
        {
            if (nseg < 3) seqs[nseg] = file + i + 4 + 12;
            nseg++;
        }
    }
    check(nseg == 3, "jpeg: the profile is split into %u APP2 segments", nseg);
    if (nseg == 3)
    {
        BYTE *expect = malloc(150000);

        check(seqs[0][0] == 1 && seqs[1][0] == 2 && seqs[2][0] == 3 && seqs[0][1] == 3 && seqs[1][1] == 3 && seqs[2][1] == 3,
              "jpeg: segments are numbered 1..3 of 3");

        seqs[1][0] = 3; seqs[2][0] = 2;
        count = read_back_bytes(file, filelen, &back, &backlen);
        memcpy(expect, profile, sz1);
        memcpy(expect + sz1, profile + sz1 + sz2, sz3);
        memcpy(expect + sz1 + sz3, profile + sz1, sz2);
        check(count == 1 && same(back, backlen, expect, 150000), "jpeg: renumbered segments are joined by their numbers (%u, %u bytes)", count, backlen);
        free(back);
        free(expect);

        seqs[1][0] = 2; seqs[2][0] = 2;
        count = read_back_bytes(file, filelen, &back, &backlen);
        check(count == 0, "jpeg: a duplicated segment number gives no profile (%u)", count);
        free(back);

        seqs[2][0] = 3; seqs[2][1] = 4;
        count = read_back_bytes(file, filelen, &back, &backlen);
        check(count == 0, "jpeg: disagreeing segment counts give no profile (%u)", count);
        free(back);

        seqs[2][1] = 3;
        seqs[0][1] = seqs[1][1] = seqs[2][1] = 4; /* all agree on four, only three exist */
        count = read_back_bytes(file, filelen, &back, &backlen);
        check(count == 0, "jpeg: a segment missing from the set gives no profile (%u)", count);
        free(back);
        seqs[0][1] = seqs[1][1] = seqs[2][1] = 3;

        seqs[1][0] = 7;
        count = read_back_bytes(file, filelen, &back, &backlen);
        check(count == 0, "jpeg: a segment number above the count gives no profile (%u)", count);
        free(back);

        seqs[1][0] = 2;
        count = read_back_bytes(file, filelen, &back, &backlen);
        check(count == 1 && same(back, backlen, profile, 150000), "jpeg: the restored file reads again (%u)", count);
        free(back);
    }
    GlobalUnlock(hg);

    IWICColorContext_Release(ctx);
    close_encoder(&e);
    free(profile);
}

/* a JPEG written with no profile has none (the decoder must not invent one) */

int main(void)
{
    UINT i;
    HRESULT hr;

    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    hr = CoCreateInstance(&my_CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, &my_IID_IWICImagingFactory, (void **)&factory);
    if (FAILED(hr)) { printf("FAIL  no factory %#lx\nRESULT: FAIL\n", hr); return 1; }

    for (i = 0; i < sizeof(formats) / sizeof(formats[0]); i++)
    {
        test_frame_errors(&formats[i]);
        test_encoder_errors(&formats[i]);
        if (formats[i].icc)
        {
            roundtrip(&formats[i], 200, "small profile");
            roundtrip(&formats[i], 4000, "4000 byte profile");
            test_no_profile(&formats[i]);
        }
    }
    roundtrip(&formats[0], 150000, "big profile");
    roundtrip(&formats[2], 150000, "big profile");
    test_jpeg_segments();

    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
