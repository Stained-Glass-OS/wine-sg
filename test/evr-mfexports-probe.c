/* evr's mfplat-equivalent exports (patches/sg/2945), run by test/evr-mfexports-gate.sh:
 * every export is looked up in evr.dll itself (GetProcAddress) and called, values checked:
 * MFConvertColorInfoToDXVA/FromDXVA, MFConvertToFP16Array/FromFP16Array,
 * MFCreateVideoMediaTypeFromBitMapInfoHeader/VideoInfoHeader2, MFGetUncompressedVideoFormat,
 * MFInitVideoFormat, MFInitVideoFormat_RGB.
 *
 *   evr-mfexports-probe.exe */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <mfapi.h>
#include <mferror.h>
#include <mfidl.h>
#include <dvdmedia.h>
#include <d3d9.h>
#include <stdio.h>
#include <string.h>

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#endif
static int failures;

static void get_size(IMFAttributes *a, UINT32 *w, UINT32 *h)
{
    UINT64 v = 0;
    IMFAttributes_GetUINT64(a, &MF_MT_FRAME_SIZE, &v);
    *w = v >> 32; *h = (UINT32)v;
}
#define MFGetAttributeSize(a, k, w, h) get_size(a, w, h)

static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}
#define CHECKF(ok, ...) do { char _b[320]; snprintf(_b, sizeof(_b), __VA_ARGS__); check(ok, _b); } while (0)

static HMODULE evr;
static void *get(const char *name)
{
    void *p = evr ? (void *)GetProcAddress(evr, name) : NULL;
    CHECKF(p != NULL, "evr exports %s", name);
    return p;
}

static HRESULT (WINAPI *pToDXVA)(DWORD *, const MFVIDEOFORMAT *);
static HRESULT (WINAPI *pFromDXVA)(MFVIDEOFORMAT *, DWORD);
static HRESULT (WINAPI *pToFP16)(WORD *, const float *, DWORD);
static HRESULT (WINAPI *pFromFP16)(float *, const WORD *, DWORD);
static HRESULT (WINAPI *pFromBIH)(const BITMAPINFOHEADER *, DWORD, DWORD, MFVideoInterlaceMode, QWORD, QWORD, QWORD, DWORD, IMFVideoMediaType **);
static HRESULT (WINAPI *pFromVIH2)(const VIDEOINFOHEADER2 *, DWORD, QWORD, const GUID *, IMFVideoMediaType **);
static DWORD (WINAPI *pGetUncompressed)(const MFVIDEOFORMAT *);
static HRESULT (WINAPI *pInitFormat)(MFVIDEOFORMAT *, MFStandardVideoFormat);
static HRESULT (WINAPI *pInitRGB)(MFVIDEOFORMAT *, DWORD, DWORD, DWORD);

static void test_dxva(void)
{
    static const struct { UINT32 interlace, chroma, range, matrix, lighting, primaries, transfer, sample_format; } t[] =
    {
        { MFVideoInterlace_Progressive, 0, MFNominalRange_0_255, MFVideoTransferMatrix_BT709, MFVideoLighting_dim,
          MFVideoPrimaries_BT709, MFVideoTransFunc_709, 2 },
        { MFVideoInterlace_FieldInterleavedUpperFirst, MFVideoChromaSubsampling_MPEG2, MFNominalRange_16_235,
          MFVideoTransferMatrix_BT601, MFVideoLighting_office, MFVideoPrimaries_SMPTE170M, MFVideoTransFunc_sRGB, 3 },
        { MFVideoInterlace_FieldInterleavedLowerFirst, MFVideoChromaSubsampling_DV_PAL, MFNominalRange_48_208,
          MFVideoTransferMatrix_SMPTE240M, MFVideoLighting_bright, MFVideoPrimaries_BT470_2_SysBG,
          MFVideoTransFunc_22, 4 },
        { MFVideoInterlace_MixedInterlaceOrProgressive, 0, MFNominalRange_Normal, MFVideoTransferMatrix_BT709,
          MFVideoLighting_dark, MFVideoPrimaries_BT709, MFVideoTransFunc_10, 3 },
    };
    unsigned int i;
    HRESULT hr;

    for (i = 0; i < ARRAY_SIZE(t); i++)
    {
        MFVIDEOFORMAT f = {0}, g = {0};
        DWORD dxva = 0, expect;

        f.dwSize = sizeof(f);
        f.videoInfo.InterlaceMode = t[i].interlace;
        f.videoInfo.SourceChromaSubsampling = t[i].chroma;
        f.videoInfo.NominalRange = t[i].range;
        f.videoInfo.TransferMatrix = t[i].matrix;
        f.videoInfo.SourceLighting = t[i].lighting;
        f.videoInfo.ColorPrimaries = t[i].primaries;
        f.videoInfo.TransferFunction = t[i].transfer;
        hr = pToDXVA(&dxva, &f);
        expect = t[i].sample_format | (t[i].chroma << 8) | (t[i].range << 12) | (t[i].matrix << 15)
                | (t[i].lighting << 18) | (t[i].primaries << 22) | (t[i].transfer << 27);
        CHECKF(hr == S_OK && dxva == expect, "dxva[%u]: ToDXVA %#lx (expected %#lx) hr %#lx", i, dxva, expect, hr);

        g.dwSize = sizeof(g);
        hr = pFromDXVA(&g, expect);
        CHECKF(hr == S_OK, "dxva[%u]: FromDXVA hr %#lx", i, hr);
        CHECKF(g.videoInfo.InterlaceMode == (t[i].interlace == MFVideoInterlace_MixedInterlaceOrProgressive
                ? t[i].sample_format : t[i].interlace), "dxva[%u]: interlace %u", i, g.videoInfo.InterlaceMode);
        CHECKF(g.videoInfo.SourceChromaSubsampling == t[i].chroma, "dxva[%u]: chroma %u", i, g.videoInfo.SourceChromaSubsampling);
        CHECKF(g.videoInfo.NominalRange == t[i].range, "dxva[%u]: range %u", i, g.videoInfo.NominalRange);
        CHECKF(g.videoInfo.TransferMatrix == t[i].matrix, "dxva[%u]: matrix %u", i, g.videoInfo.TransferMatrix);
        CHECKF(g.videoInfo.SourceLighting == t[i].lighting, "dxva[%u]: lighting %u", i, g.videoInfo.SourceLighting);
        CHECKF(g.videoInfo.ColorPrimaries == t[i].primaries, "dxva[%u]: primaries %u", i, g.videoInfo.ColorPrimaries);
        CHECKF(g.videoInfo.TransferFunction == t[i].transfer, "dxva[%u]: transfer %u", i, g.videoInfo.TransferFunction);
    }
    hr = pFromDXVA(NULL, 0);
    CHECKF(hr == E_INVALIDARG, "FromDXVA(NULL) hr %#lx", hr);
}

static void test_fp16(void)
{
    static const struct { UINT32 f; WORD h; } t[] =
    {
        { 0x00000000, 0x0000 }, { 0x80000000, 0x8000 }, { 0x3f800000, 0x3c00 }, { 0xbf800000, 0xbc00 },
        { 0x40000000, 0x4000 }, { 0xc0000000, 0xc000 }, { 0x3f000000, 0x3800 }, { 0x477fe000, 0x7bff }, /* 65504 */
        { 0x47800000, 0x7c00 }, /* 65536 overflows to infinity */
        { 0x7f800000, 0x7c00 }, { 0xff800000, 0xfc00 },
        { 0x38800000, 0x0400 }, /* 2^-14, smallest normal */
        { 0x33800000, 0x0001 }, /* 2^-24, smallest subnormal */
        { 0x32000000, 0x0000 }, /* far too small */
        { 0x3a800000, 0x1400 }, /* 2^-10 */
        { 0x3f801000, 0x3c00 }, /* 1 + 2^-11: tie, rounds to even (down) */
        { 0x3f803000, 0x3c02 }, /* 1 + 3*2^-11: tie, rounds to even (up) */
        { 0x3f801001, 0x3c01 }, /* just above a tie: up */
        { 0x40490fdb, 0x4248 }, /* pi */
    };
    float in[ARRAY_SIZE(t)], out[ARRAY_SIZE(t)];
    WORD half[ARRAY_SIZE(t)];
    unsigned int i;
    HRESULT hr;

    for (i = 0; i < ARRAY_SIZE(t); i++) memcpy(&in[i], &t[i].f, 4);
    memset(half, 0xee, sizeof(half));
    hr = pToFP16(half, in, ARRAY_SIZE(t));
    CHECKF(hr == S_OK, "ToFP16 hr %#lx", hr);
    for (i = 0; i < ARRAY_SIZE(t); i++)
        CHECKF(half[i] == t[i].h, "ToFP16 %#x -> %#x (expected %#x)", t[i].f, half[i], t[i].h);

    memset(half, 0xee, sizeof(half));
    hr = pToFP16(half, in, 0);
    CHECKF(hr == S_OK && half[0] == 0xeeee, "ToFP16 count 0 hr %#lx touched %#x", hr, half[0]);

    /* NaN stays NaN */
    {
        UINT32 nanbits = 0x7fc00000; float nanf; WORD h;
        memcpy(&nanf, &nanbits, 4);
        pToFP16(&h, &nanf, 1);
        CHECKF((h & 0x7c00) == 0x7c00 && (h & 0x3ff), "ToFP16 NaN %#x", h);
    }

    for (i = 0; i < ARRAY_SIZE(t); i++) half[i] = t[i].h;
    memset(out, 0, sizeof(out));
    hr = pFromFP16(out, half, ARRAY_SIZE(t));
    CHECKF(hr == S_OK, "FromFP16 hr %#lx", hr);
    for (i = 0; i < ARRAY_SIZE(t); i++)
    {
        UINT32 bits;
        /* the exact float of a half: compare only the entries that are exactly representable */
        static const UINT32 exact[19] = {
            0x00000000, 0x80000000, 0x3f800000, 0xbf800000, 0x40000000, 0xc0000000, 0x3f000000, 0x477fe000,
            0x7f800000, 0x7f800000, 0xff800000, 0x38800000, 0x33800000, 0, 0x3a800000, 0x3f800000, 0x3f804000,
            0x3f802000, 0x40490000 };
        memcpy(&bits, &out[i], 4);
        CHECKF(bits == exact[i], "FromFP16 %#x -> %#x (expected %#x)", t[i].h, bits, exact[i]);
    }
    {
        WORD sub[3] = { 0x0200, 0x03ff, 0x8001 };
        UINT32 want[3] = { 0x38000000, 0x387fc000, 0xb3800000 }, bits;
        float o[3];
        pFromFP16(o, sub, 3);
        for (i = 0; i < 3; i++)
        {
            memcpy(&bits, &o[i], 4);
            CHECKF(bits == want[i], "FromFP16 subnormal %#x -> %#x (expected %#x)", sub[i], bits, want[i]);
        }
    }
    hr = pToFP16(NULL, in, 1);
    CHECKF(hr == E_POINTER, "ToFP16 NULL dest hr %#lx", hr);
    hr = pFromFP16(out, NULL, 1);
    CHECKF(hr == E_POINTER, "FromFP16 NULL src hr %#lx", hr);
}

static UINT32 u32(IMFAttributes *a, const GUID *key, UINT32 def)
{
    UINT32 v;
    return SUCCEEDED(IMFAttributes_GetUINT32(a, key, &v)) ? v : def;
}
static UINT64 u64(IMFAttributes *a, const GUID *key)
{
    UINT64 v;
    return SUCCEEDED(IMFAttributes_GetUINT64(a, key, &v)) ? v : ~0ull;
}

static void test_bih(void)
{
    static const struct { DWORD compression; WORD bits; LONG w, h; const GUID *subtype; UINT32 sample_size; LONG stride; } t[] =
    {
        { MAKEFOURCC('Y','U','Y','2'), 16, 320, 240, &MFVideoFormat_YUY2, 320 * 2 * 240, 640 },
        { MAKEFOURCC('N','V','1','2'), 12, 320, 240, &MFVideoFormat_NV12, 320 * 240 * 3 / 2, 320 },
        { BI_RGB, 32, 64, 32, &MFVideoFormat_RGB32, 64 * 4 * 32, -256 },
        { BI_RGB, 24, 64, -32, &MFVideoFormat_RGB24, 64 * 3 * 32, 192 },
    };
    unsigned int i;
    HRESULT hr;

    for (i = 0; i < ARRAY_SIZE(t); i++)
    {
        BITMAPINFOHEADER bih = {0};
        IMFVideoMediaType *vt = NULL;
        IMFAttributes *a;
        GUID g;
        UINT32 w, h;

        bih.biSize = sizeof(bih);
        bih.biWidth = t[i].w;
        bih.biHeight = t[i].h;
        bih.biPlanes = 1;
        bih.biBitCount = t[i].bits;
        bih.biCompression = t[i].compression;
        bih.biSizeImage = t[i].sample_size;
        hr = pFromBIH(&bih, 4, 3, MFVideoInterlace_Progressive, 0, 30000, 1001, 123456, &vt);
        CHECKF(hr == S_OK && vt, "bih[%u]: hr %#lx", i, hr);
        if (FAILED(hr) || !vt) continue;
        a = (IMFAttributes *)vt;
        hr = IMFAttributes_GetGUID(a, &MF_MT_MAJOR_TYPE, &g);
        CHECKF(hr == S_OK && IsEqualGUID(&g, &MFMediaType_Video), "bih[%u]: major type", i);
        hr = IMFAttributes_GetGUID(a, &MF_MT_SUBTYPE, &g);
        CHECKF(hr == S_OK && IsEqualGUID(&g, t[i].subtype), "bih[%u]: subtype %s", i, "?");
        MFGetAttributeSize(a, &MF_MT_FRAME_SIZE, &w, &h);
        CHECKF(w == t[i].w && h == abs(t[i].h), "bih[%u]: frame size %ux%u", i, w, h);
        CHECKF(u64(a, &MF_MT_PIXEL_ASPECT_RATIO) == ((4ull << 32) | 3), "bih[%u]: PAR %#I64x", i, u64(a, &MF_MT_PIXEL_ASPECT_RATIO));
        CHECKF(u64(a, &MF_MT_FRAME_RATE) == ((30000ull << 32) | 1001), "bih[%u]: frame rate %#I64x", i, u64(a, &MF_MT_FRAME_RATE));
        CHECKF(u32(a, &MF_MT_INTERLACE_MODE, 99) == MFVideoInterlace_Progressive, "bih[%u]: interlace %u", i, u32(a, &MF_MT_INTERLACE_MODE, 99));
        CHECKF(u32(a, &MF_MT_AVG_BITRATE, 0) == 123456, "bih[%u]: bitrate %u", i, u32(a, &MF_MT_AVG_BITRATE, 0));
        CHECKF(u32(a, &MF_MT_SAMPLE_SIZE, 0) == t[i].sample_size, "bih[%u]: sample size %u", i, u32(a, &MF_MT_SAMPLE_SIZE, 0));
        CHECKF((LONG)u32(a, &MF_MT_DEFAULT_STRIDE, 0) == t[i].stride, "bih[%u]: stride %d (expected %d)", i, (int)u32(a, &MF_MT_DEFAULT_STRIDE, 0), (int)t[i].stride);
        IMFVideoMediaType_Release(vt);
    }

    {
        BITMAPINFOHEADER bih = { sizeof(bih), 16, 16, 1, 32, BI_RGB };
        IMFVideoMediaType *vt = (void *)0x1;
        hr = pFromBIH(NULL, 1, 1, MFVideoInterlace_Progressive, 0, 30, 1, 0, &vt);
        CHECKF(hr == E_INVALIDARG, "bih NULL header hr %#lx", hr);
        hr = pFromBIH(&bih, 1, 1, MFVideoInterlace_Progressive, 0, 30, 1, 0, NULL);
        CHECKF(hr == E_INVALIDARG, "bih NULL output hr %#lx", hr);
        bih.biSize = 8;
        hr = pFromBIH(&bih, 1, 1, MFVideoInterlace_Progressive, 0, 30, 1, 0, &vt);
        CHECKF(hr == E_INVALIDARG, "bih short biSize hr %#lx", hr);
    }
}

static void test_vih2(void)
{
    VIDEOINFOHEADER2 vih = {{0}};
    IMFVideoMediaType *vt = NULL;
    IMFAttributes *a;
    GUID g;
    UINT32 w, h;
    HRESULT hr;

    vih.dwPictAspectRatioX = 16;
    vih.dwPictAspectRatioY = 9;
    vih.dwInterlaceFlags = 0;
    vih.AvgTimePerFrame = 400000;
    vih.dwBitRate = 5000000;
    vih.bmiHeader.biSize = sizeof(vih.bmiHeader);
    vih.bmiHeader.biWidth = 640;
    vih.bmiHeader.biHeight = 360;
    vih.bmiHeader.biPlanes = 1;
    vih.bmiHeader.biBitCount = 12;
    vih.bmiHeader.biCompression = MAKEFOURCC('N','V','1','2');
    vih.bmiHeader.biSizeImage = 640 * 360 * 3 / 2;
    hr = pFromVIH2(&vih, sizeof(vih), 0, &MFVideoFormat_NV12, &vt);
    CHECKF(hr == S_OK && vt, "vih2: hr %#lx", hr);
    if (SUCCEEDED(hr) && vt)
    {
        a = (IMFAttributes *)vt;
        hr = IMFAttributes_GetGUID(a, &MF_MT_MAJOR_TYPE, &g);
        CHECKF(hr == S_OK && IsEqualGUID(&g, &MFMediaType_Video), "vih2: major type");
        hr = IMFAttributes_GetGUID(a, &MF_MT_SUBTYPE, &g);
        CHECKF(hr == S_OK && IsEqualGUID(&g, &MFVideoFormat_NV12), "vih2: subtype");
        MFGetAttributeSize(a, &MF_MT_FRAME_SIZE, &w, &h);
        CHECKF(w == 640 && h == 360, "vih2: frame size %ux%u", w, h);
        CHECKF(u64(a, &MF_MT_FRAME_RATE) == ((25ull << 32) | 1), "vih2: frame rate %#I64x", u64(a, &MF_MT_FRAME_RATE));
        CHECKF(u32(a, &MF_MT_AVG_BITRATE, 0) == 5000000, "vih2: bitrate %u", u32(a, &MF_MT_AVG_BITRATE, 0));
        CHECKF(u32(a, &MF_MT_INTERLACE_MODE, 99) == MFVideoInterlace_Progressive, "vih2: interlace %u", u32(a, &MF_MT_INTERLACE_MODE, 99));
        CHECKF(u32(a, &MF_MT_SAMPLE_SIZE, 0) == 640 * 360 * 3 / 2, "vih2: sample size %u", u32(a, &MF_MT_SAMPLE_SIZE, 0));
        IMFVideoMediaType_Release(vt);
    }

    vt = (void *)0x1;
    hr = pFromVIH2(NULL, sizeof(vih), 0, NULL, &vt);
    CHECKF(hr == E_INVALIDARG, "vih2 NULL header hr %#lx", hr);
    hr = pFromVIH2(&vih, sizeof(vih) - 1, 0, NULL, &vt);
    CHECKF(hr == E_INVALIDARG, "vih2 short size hr %#lx", hr);
    hr = pFromVIH2(&vih, sizeof(vih), 0, NULL, NULL);
    CHECKF(hr == E_INVALIDARG, "vih2 NULL output hr %#lx", hr);
}

static void test_formats(void)
{
    static const struct
    {
        MFStandardVideoFormat type;
        UINT w, h, par_x, par_y, fps_n, fps_d, interlace, primaries, matrix;
        const GUID *subtype;
    } t[] =
    {
        { MFStdVideoFormat_NTSC, 720, 480, 10, 11, 30000, 1001, MFVideoInterlace_FieldInterleavedLowerFirst, MFVideoPrimaries_SMPTE170M, MFVideoTransferMatrix_BT601, &MFVideoFormat_Base },
        { MFStdVideoFormat_PAL, 720, 576, 12, 11, 25, 1, MFVideoInterlace_FieldInterleavedUpperFirst, MFVideoPrimaries_BT470_2_SysBG, MFVideoTransferMatrix_BT601, &MFVideoFormat_Base },
        { MFStdVideoFormat_DVD_NTSC, 720, 480, 10, 11, 30000, 1001, MFVideoInterlace_FieldInterleavedUpperFirst, MFVideoPrimaries_SMPTE170M, MFVideoTransferMatrix_BT601, &MFVideoFormat_MPEG2 },
        { MFStdVideoFormat_DVD_PAL, 720, 576, 12, 11, 25, 1, MFVideoInterlace_FieldInterleavedUpperFirst, MFVideoPrimaries_BT470_2_SysBG, MFVideoTransferMatrix_BT601, &MFVideoFormat_MPEG2 },
        { MFStdVideoFormat_DV_PAL, 720, 576, 12, 11, 25, 1, MFVideoInterlace_FieldInterleavedLowerFirst, MFVideoPrimaries_BT470_2_SysBG, MFVideoTransferMatrix_BT601, &MFVideoFormat_DVSD },
        { MFStdVideoFormat_DV_NTSC, 720, 480, 10, 11, 30000, 1001, MFVideoInterlace_FieldInterleavedLowerFirst, MFVideoPrimaries_SMPTE170M, MFVideoTransferMatrix_BT601, &MFVideoFormat_DVSD },
        { MFStdVideoFormat_ATSC_SD480i, 704, 480, 10, 11, 30000, 1001, MFVideoInterlace_FieldInterleavedUpperFirst, MFVideoPrimaries_SMPTE170M, MFVideoTransferMatrix_BT601, &MFVideoFormat_MPEG2 },
        { MFStdVideoFormat_ATSC_HD1080i, 1920, 1080, 1, 1, 30000, 1001, MFVideoInterlace_FieldInterleavedUpperFirst, MFVideoPrimaries_BT709, MFVideoTransferMatrix_BT709, &MFVideoFormat_MPEG2 },
        { MFStdVideoFormat_ATSC_HD720p, 1280, 720, 1, 1, 60000, 1001, MFVideoInterlace_Progressive, MFVideoPrimaries_BT709, MFVideoTransferMatrix_BT709, &MFVideoFormat_MPEG2 },
    };
    unsigned int i;
    MFVIDEOFORMAT f;
    HRESULT hr;

    for (i = 0; i < ARRAY_SIZE(t); i++)
    {
        memset(&f, 0xcc, sizeof(f));
        hr = pInitFormat(&f, t[i].type);
        CHECKF(hr == S_OK, "InitVideoFormat(%d) hr %#lx", t[i].type, hr);
        CHECKF(f.dwSize == sizeof(f), "InitVideoFormat(%d): dwSize %lu", t[i].type, f.dwSize);
        CHECKF(f.videoInfo.dwWidth == t[i].w && f.videoInfo.dwHeight == t[i].h, "InitVideoFormat(%d): size %lux%lu", t[i].type, f.videoInfo.dwWidth, f.videoInfo.dwHeight);
        CHECKF(f.videoInfo.PixelAspectRatio.Numerator == t[i].par_x && f.videoInfo.PixelAspectRatio.Denominator == t[i].par_y, "InitVideoFormat(%d): PAR %lu/%lu", t[i].type, f.videoInfo.PixelAspectRatio.Numerator, f.videoInfo.PixelAspectRatio.Denominator);
        CHECKF(f.videoInfo.FramesPerSecond.Numerator == t[i].fps_n && f.videoInfo.FramesPerSecond.Denominator == t[i].fps_d, "InitVideoFormat(%d): fps %lu/%lu", t[i].type, f.videoInfo.FramesPerSecond.Numerator, f.videoInfo.FramesPerSecond.Denominator);
        CHECKF(f.videoInfo.InterlaceMode == t[i].interlace, "InitVideoFormat(%d): interlace %u", t[i].type, f.videoInfo.InterlaceMode);
        CHECKF(f.videoInfo.ColorPrimaries == t[i].primaries, "InitVideoFormat(%d): primaries %u", t[i].type, f.videoInfo.ColorPrimaries);
        CHECKF(f.videoInfo.TransferMatrix == t[i].matrix, "InitVideoFormat(%d): matrix %u", t[i].type, f.videoInfo.TransferMatrix);
        CHECKF(f.videoInfo.TransferFunction == MFVideoTransFunc_709 && f.videoInfo.NominalRange == MFNominalRange_16_235 && f.videoInfo.SourceLighting == MFVideoLighting_dim, "InitVideoFormat(%d): transfer/range/lighting", t[i].type);
        CHECKF(f.videoInfo.GeometricAperture.Area.cx == (LONG)t[i].w && f.videoInfo.MinimumDisplayAperture.Area.cy == (LONG)t[i].h, "InitVideoFormat(%d): apertures", t[i].type);
        CHECKF(IsEqualGUID(&f.guidFormat, t[i].subtype), "InitVideoFormat(%d): guidFormat", t[i].type);
    }
    hr = pInitFormat(&f, 0);
    CHECKF(hr == E_INVALIDARG, "InitVideoFormat(0) hr %#lx", hr);
    hr = pInitFormat(&f, MFStdVideoFormat_ATSC_HD720p + 1);
    CHECKF(hr == E_INVALIDARG, "InitVideoFormat(past last) hr %#lx", hr);
    hr = pInitFormat(NULL, MFStdVideoFormat_NTSC);
    CHECKF(hr == E_INVALIDARG, "InitVideoFormat(NULL) hr %#lx", hr);

    {
        static const struct { DWORD d3d, expect; UINT transfer; } r[] =
        {
            { 0, D3DFMT_X8R8G8B8, MFVideoTransFunc_sRGB }, { D3DFMT_R8G8B8, D3DFMT_R8G8B8, MFVideoTransFunc_sRGB },
            { D3DFMT_A8R8G8B8, D3DFMT_A8R8G8B8, MFVideoTransFunc_sRGB }, { D3DFMT_R5G6B5, D3DFMT_R5G6B5, MFVideoTransFunc_sRGB },
            { D3DFMT_A16B16G16R16F, D3DFMT_A16B16G16R16F, MFVideoTransFunc_10 },
        };
        for (i = 0; i < ARRAY_SIZE(r); i++)
        {
            memset(&f, 0xcc, sizeof(f));
            hr = pInitRGB(&f, 800, 600, r[i].d3d);
            CHECKF(hr == S_OK, "InitVideoFormat_RGB(%#lx) hr %#lx", r[i].d3d, hr);
            CHECKF(f.dwSize == sizeof(f) && f.videoInfo.dwWidth == 800 && f.videoInfo.dwHeight == 600, "RGB(%#lx): size", r[i].d3d);
            CHECKF(f.guidFormat.Data1 == r[i].expect && f.surfaceInfo.Format == r[i].expect, "RGB(%#lx): format %#lx", r[i].d3d, f.guidFormat.Data1);
            CHECKF(f.videoInfo.TransferFunction == r[i].transfer, "RGB(%#lx): transfer %u", r[i].d3d, f.videoInfo.TransferFunction);
            CHECKF(f.videoInfo.InterlaceMode == MFVideoInterlace_Progressive && f.videoInfo.FramesPerSecond.Numerator == 60 && f.videoInfo.PixelAspectRatio.Numerator == 1, "RGB(%#lx): interlace/fps/PAR", r[i].d3d);
        }
        hr = pInitRGB(NULL, 1, 1, 0);
        CHECKF(hr == E_INVALIDARG, "InitVideoFormat_RGB(NULL) hr %#lx", hr);
    }

    /* MFGetUncompressedVideoFormat */
    {
        static const struct { const GUID *g; DWORD expect; const char *name; } u[] =
        {
            { &MFVideoFormat_NV12, MAKEFOURCC('N','V','1','2'), "NV12" }, { &MFVideoFormat_YUY2, MAKEFOURCC('Y','U','Y','2'), "YUY2" },
            { &MFVideoFormat_RGB32, 22, "RGB32" }, { &MFVideoFormat_ARGB32, 21, "ARGB32" }, { &MFVideoFormat_RGB24, 20, "RGB24" },
            { &MFVideoFormat_H264, 0, "H264 (compressed)" }, { &MFVideoFormat_MPEG2, 0, "MPEG2 (compressed)" },
            { &MFMediaType_Video, 0, "non-base GUID" },
        };
        for (i = 0; i < ARRAY_SIZE(u); i++)
        {
            DWORD d;
            memset(&f, 0, sizeof(f));
            f.dwSize = sizeof(f);
            f.guidFormat = *u[i].g;
            d = pGetUncompressed(&f);
            CHECKF(d == u[i].expect, "GetUncompressedVideoFormat %s = %#lx (expected %#lx)", u[i].name, d, u[i].expect);
        }
    }
}

int main(void)
{
    evr = LoadLibraryA("evr.dll");
    check(evr != NULL, "evr.dll loads");
    pToDXVA = get("MFConvertColorInfoToDXVA");
    pFromDXVA = get("MFConvertColorInfoFromDXVA");
    pToFP16 = get("MFConvertToFP16Array");
    pFromFP16 = get("MFConvertFromFP16Array");
    pFromBIH = get("MFCreateVideoMediaTypeFromBitMapInfoHeader");
    pFromVIH2 = get("MFCreateVideoMediaTypeFromVideoInfoHeader2");
    pGetUncompressed = get("MFGetUncompressedVideoFormat");
    pInitFormat = get("MFInitVideoFormat");
    pInitRGB = get("MFInitVideoFormat_RGB");
    if (failures) { printf("RESULT: FAIL\n"); return 1; }

    MFStartup(MF_VERSION, MFSTARTUP_FULL);
    test_dxva();
    test_fp16();
    test_bih();
    test_vih2();
    test_formats();
    MFShutdown();
    printf(failures ? "RESULT: FAIL (%d)\n" : "RESULT: PASS\n", failures);
    return failures != 0;
}
