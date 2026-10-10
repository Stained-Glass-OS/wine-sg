/* mfplat's video media type constructors (patches/sg/2838), run by
 * test/mfplat-mediatype-gate.sh: MFCreateVideoMediaTypeFromVideoInfoHeader
 * (was E_NOTIMPL; dlls/mfplat/tests/mfplat.c is the ground truth), ...Header2,
 * MFCreateVideoMediaTypeFromBitMapInfoHeader and ...Ex ("@ stub" exports).
 *
 *   mfplat-mediatype-probe.exe */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <ks.h>
#include <ksmedia.h>
#include <mfapi.h>
#include <mferror.h>
#include <mfobjects.h>
#include <dvdmedia.h>
#include <stdio.h>
#include <string.h>

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}
#define CHECKF(ok, ...) do { char _b[256]; snprintf(_b, sizeof(_b), __VA_ARGS__); check(ok, _b); } while (0)

#ifndef MFVideoFlag_AnalogProtected
#define MFVideoFlag_AnalogProtected 0x20
#endif

static UINT32 u32(IMFAttributes *a, const GUID *key, UINT32 missing)
{
    UINT32 v;
    return SUCCEEDED(IMFAttributes_GetUINT32(a, key, &v)) ? v : missing;
}
static UINT64 u64(IMFAttributes *a, const GUID *key)
{
    UINT64 v = 0xdeadbeef;
    return SUCCEEDED(IMFAttributes_GetUINT64(a, key, &v)) ? v : ~0ull;
}
static int has_guid(IMFAttributes *a, const GUID *key, const GUID *want)
{
    GUID g;
    return SUCCEEDED(IMFAttributes_GetGUID(a, key, &g)) && IsEqualGUID(&g, want);
}

static void test_header2(void)
{
    HRESULT (WINAPI *create2)(const VIDEOINFOHEADER2 *, DWORD, QWORD, const GUID *, IMFVideoMediaType **) =
        (void *)GetProcAddress(GetModuleHandleA("mfplat.dll"), "MFCreateVideoMediaTypeFromVideoInfoHeader2");
    VIDEOINFOHEADER2 vih;
    IMFVideoMediaType *type;
    IMFAttributes *a;
    HRESULT hr;

    check(create2 != NULL, "mfplat exports MFCreateVideoMediaTypeFromVideoInfoHeader2");
    if (!create2) return;

    memset(&vih, 0, sizeof(vih));
    hr = create2(NULL, sizeof(vih), 0, NULL, &type);
    CHECKF(hr == E_INVALIDARG, "Header2: NULL header is E_INVALIDARG (%#lx)", hr);
    hr = create2(&vih, sizeof(vih) - 1, 0, NULL, &type);
    CHECKF(hr == E_INVALIDARG, "Header2: a short size is E_INVALIDARG (%#lx)", hr);
    hr = create2(&vih, sizeof(vih), 0, NULL, &type);
    CHECKF(hr == E_INVALIDARG, "Header2: a zero biSize is E_INVALIDARG (%#lx)", hr);
    vih.bmiHeader.biSize = sizeof(vih.bmiHeader);
    hr = create2(&vih, sizeof(vih), 0, NULL, &type);
    CHECKF(hr == E_INVALIDARG, "Header2: no bit depth and no subtype is E_INVALIDARG (%#lx)", hr);

    vih.bmiHeader.biPlanes = 1;
    vih.bmiHeader.biWidth = 16;
    vih.bmiHeader.biHeight = 32;
    vih.bmiHeader.biBitCount = 32;
    hr = create2(&vih, sizeof(vih), MFVideoFlag_AnalogProtected, &GUID_NULL, &type);
    CHECKF(hr == S_OK, "Header2: a null subtype is taken from the bit depth (%#lx)", hr);
    if (hr != S_OK) return;
    a = (IMFAttributes *)type;
    check(has_guid(a, &MF_MT_MAJOR_TYPE, &MFMediaType_Video), "Header2: major type is video");
    check(has_guid(a, &MF_MT_SUBTYPE, &MFVideoFormat_RGB32), "Header2: subtype is RGB32");
    CHECKF(u64(a, &MF_MT_FRAME_SIZE) == ((UINT64)16 << 32 | 32), "Header2: frame size 16x32 (%#I64x)", u64(a, &MF_MT_FRAME_SIZE));
    CHECKF(u32(a, &MF_MT_DEFAULT_STRIDE, 0) == (UINT32)-64, "Header2: bottom-up stride -64 (%d)", (int)u32(a, &MF_MT_DEFAULT_STRIDE, 0));
    CHECKF(u32(a, &MF_MT_SAMPLE_SIZE, 0) == 2048, "Header2: sample size 2048 (%u)", u32(a, &MF_MT_SAMPLE_SIZE, 0));
    CHECKF(u32(a, &MF_MT_DRM_FLAGS, 0xff) == 1, "Header2: AnalogProtected is MF_MT_DRM_FLAGS 1 (%u)", u32(a, &MF_MT_DRM_FLAGS, 0xff));
    IMFVideoMediaType_Release(type);

    hr = create2(&vih, sizeof(vih), 0, &MFVideoFormat_RGB24, &type);
    CHECKF(hr == S_OK && has_guid((IMFAttributes *)type, &MF_MT_SUBTYPE, &MFVideoFormat_RGB24), "Header2: an explicit subtype is kept (%#lx)", hr);
    if (hr == S_OK) { CHECKF(u32((IMFAttributes *)type, &MF_MT_DRM_FLAGS, 0xff) == 0xff, "Header2: no flags, no DRM attribute"); IMFVideoMediaType_Release(type); }
}

static void test_info_header(void)
{
    KS_VIDEOINFOHEADER vih;
    IMFVideoMediaType *type;
    HRESULT hr;

    memset(&vih, 0, sizeof(vih));
    vih.bmiHeader.biSize = sizeof(vih.bmiHeader);
    vih.bmiHeader.biPlanes = 1;
    vih.bmiHeader.biWidth = 16;
    vih.bmiHeader.biHeight = -32;
    vih.bmiHeader.biBitCount = 32;
    hr = MFCreateVideoMediaTypeFromVideoInfoHeader(&vih, sizeof(vih), 3, 2, MFVideoInterlace_Progressive, MFVideoFlag_AnalogProtected,
            &GUID_NULL, &type);
    CHECKF(hr == S_OK, "VideoInfoHeader: a null subtype (%#lx)", hr);
    if (hr == S_OK)
    {
        IMFAttributes *a = (IMFAttributes *)type;
        CHECKF(u64(a, &MF_MT_PIXEL_ASPECT_RATIO) == ((UINT64)3 << 32 | 2), "VideoInfoHeader: pixel aspect ratio 3:2");
        CHECKF(u32(a, &MF_MT_INTERLACE_MODE, 0xff) == MFVideoInterlace_Progressive, "VideoInfoHeader: progressive");
        CHECKF(u32(a, &MF_MT_DEFAULT_STRIDE, 0) == 64, "VideoInfoHeader: top-down stride 64 (%d)", (int)u32(a, &MF_MT_DEFAULT_STRIDE, 0));
        CHECKF(u32(a, &MF_MT_DRM_FLAGS, 0xff) == 1, "VideoInfoHeader: AnalogProtected is MF_MT_DRM_FLAGS 1");
        IMFVideoMediaType_Release(type);
    }
    hr = MFCreateVideoMediaTypeFromVideoInfoHeader(NULL, sizeof(vih), 3, 2, MFVideoInterlace_Progressive, 0, NULL, &type);
    CHECKF(hr == E_INVALIDARG, "VideoInfoHeader: NULL header is E_INVALIDARG (%#lx)", hr);
    hr = MFCreateVideoMediaTypeFromVideoInfoHeader(&vih, 0, 3, 2, MFVideoInterlace_Progressive, 0, NULL, &type);
    CHECKF(hr == E_INVALIDARG, "VideoInfoHeader: size 0 is E_INVALIDARG (%#lx)", hr);
}

static void test_bitmap_header(void)
{
    HRESULT (WINAPI *create)(const BITMAPINFOHEADER *, DWORD, DWORD, MFVideoInterlaceMode, QWORD, QWORD, QWORD, DWORD, IMFVideoMediaType **) =
        (void *)GetProcAddress(GetModuleHandleA("mfplat.dll"), "MFCreateVideoMediaTypeFromBitMapInfoHeader");
    HRESULT (WINAPI *create_ex)(const BITMAPINFOHEADER *, UINT32, DWORD, DWORD, MFVideoInterlaceMode, QWORD, DWORD, DWORD, DWORD, IMFVideoMediaType **) =
        (void *)GetProcAddress(GetModuleHandleA("mfplat.dll"), "MFCreateVideoMediaTypeFromBitMapInfoHeaderEx");
    BITMAPINFOHEADER bih;
    IMFVideoMediaType *type;
    IMFAttributes *a;
    HRESULT hr;

    check(create != NULL && create_ex != NULL, "mfplat exports both MFCreateVideoMediaTypeFromBitMapInfoHeader variants");
    if (!create || !create_ex) return;

    memset(&bih, 0, sizeof(bih));
    bih.biSize = sizeof(bih);
    bih.biPlanes = 1;
    bih.biWidth = 16;
    bih.biHeight = 8;
    bih.biBitCount = 24;

    hr = create_ex(NULL, sizeof(bih), 1, 1, MFVideoInterlace_Progressive, 0, 30, 1, 0, &type);
    CHECKF(hr == E_INVALIDARG, "BitMapInfoHeaderEx: NULL header is E_INVALIDARG (%#lx)", hr);
    hr = create_ex(&bih, sizeof(bih) - 1, 1, 1, MFVideoInterlace_Progressive, 0, 30, 1, 0, &type);
    CHECKF(hr == E_INVALIDARG, "BitMapInfoHeaderEx: a short size is E_INVALIDARG (%#lx)", hr);
    hr = create_ex(&bih, sizeof(bih), 1, 1, MFVideoInterlace_Progressive, 0, 30, 1, 0, NULL);
    CHECKF(hr == E_INVALIDARG, "BitMapInfoHeaderEx: a NULL output is E_INVALIDARG (%#lx)", hr);

    hr = create_ex(&bih, sizeof(bih), 4, 3, MFVideoInterlace_Progressive, MFVideoFlag_AnalogProtected, 30, 1, 5000, &type);
    CHECKF(hr == S_OK, "BitMapInfoHeaderEx: RGB24 (%#lx)", hr);
    if (hr == S_OK)
    {
        a = (IMFAttributes *)type;
        check(has_guid(a, &MF_MT_SUBTYPE, &MFVideoFormat_RGB24), "BitMapInfoHeaderEx: subtype RGB24");
        CHECKF(u64(a, &MF_MT_FRAME_SIZE) == ((UINT64)16 << 32 | 8), "BitMapInfoHeaderEx: frame size 16x8");
        CHECKF(u64(a, &MF_MT_PIXEL_ASPECT_RATIO) == ((UINT64)4 << 32 | 3), "BitMapInfoHeaderEx: pixel aspect ratio 4:3");
        CHECKF(u64(a, &MF_MT_FRAME_RATE) == ((UINT64)30 << 32 | 1), "BitMapInfoHeaderEx: frame rate 30/1 (%#I64x)", u64(a, &MF_MT_FRAME_RATE));
        CHECKF(u32(a, &MF_MT_AVG_BITRATE, 0) == 5000, "BitMapInfoHeaderEx: bit rate 5000 (%u)", u32(a, &MF_MT_AVG_BITRATE, 0));
        CHECKF(u32(a, &MF_MT_INTERLACE_MODE, 0xff) == MFVideoInterlace_Progressive, "BitMapInfoHeaderEx: progressive");
        CHECKF(u32(a, &MF_MT_DRM_FLAGS, 0xff) == 1, "BitMapInfoHeaderEx: AnalogProtected is MF_MT_DRM_FLAGS 1");
        IMFVideoMediaType_Release(type);
    }
    hr = create_ex(&bih, sizeof(bih), 0, 0, MFVideoInterlace_Unknown, 0, 0, 0, 0, &type);
    if (hr == S_OK)
    {
        a = (IMFAttributes *)type;
        CHECKF(u64(a, &MF_MT_FRAME_RATE) == ~0ull, "BitMapInfoHeaderEx: no frame rate when none is given");
        CHECKF(u32(a, &MF_MT_AVG_BITRATE, 0xff) == 0xff, "BitMapInfoHeaderEx: no bit rate when none is given");
        IMFVideoMediaType_Release(type);
    }
    else check(0, "BitMapInfoHeaderEx: no optional values");

    bih.biCompression = MAKEFOURCC('N','V','1','2');
    bih.biBitCount = 12;
    hr = create_ex(&bih, sizeof(bih), 1, 1, MFVideoInterlace_Progressive, 0, 30, 1, 0, &type);
    CHECKF(hr == S_OK, "BitMapInfoHeaderEx: NV12 (%#lx)", hr);
    if (hr == S_OK)
    {
        check(has_guid((IMFAttributes *)type, &MF_MT_SUBTYPE, &MFVideoFormat_NV12), "BitMapInfoHeaderEx: subtype NV12 from the FOURCC");
        IMFVideoMediaType_Release(type);
    }

    bih.biCompression = 0;
    bih.biBitCount = 32;
    hr = create(&bih, 1, 1, MFVideoInterlace_MixedInterlaceOrProgressive, 0, 25, 1, 100, &type);
    CHECKF(hr == S_OK, "BitMapInfoHeader (no size): RGB32 (%#lx)", hr);
    if (hr == S_OK)
    {
        a = (IMFAttributes *)type;
        check(has_guid(a, &MF_MT_SUBTYPE, &MFVideoFormat_RGB32), "BitMapInfoHeader: subtype RGB32");
        CHECKF(u64(a, &MF_MT_FRAME_RATE) == ((UINT64)25 << 32 | 1), "BitMapInfoHeader: frame rate 25/1");
        CHECKF(u32(a, &MF_MT_AVG_BITRATE, 0) == 100, "BitMapInfoHeader: bit rate 100");
        CHECKF(u32(a, &MF_MT_INTERLACE_MODE, 0xff) == MFVideoInterlace_MixedInterlaceOrProgressive, "BitMapInfoHeader: mixed interlace");
        IMFVideoMediaType_Release(type);
    }
    hr = create(NULL, 1, 1, MFVideoInterlace_Progressive, 0, 25, 1, 100, &type);
    CHECKF(hr == E_INVALIDARG, "BitMapInfoHeader: NULL header is E_INVALIDARG (%#lx)", hr);
}

int main(void)
{
    HRESULT hr = MFStartup(MF_VERSION, MFSTARTUP_FULL);
    check(hr == S_OK, "MFStartup");
    test_info_header();
    test_header2();
    test_bitmap_header();
    MFShutdown();
    printf("%s\n", failures ? "RESULT: FAIL" : "RESULT: PASS");
    return failures != 0;
}
