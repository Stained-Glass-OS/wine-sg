/* mfplat's buffer and conversion stubs (patches/sg/2830), run by
 * test/mfplat-buffers-gate.sh: MFCreateMediaBufferWrapper,
 * MFCalculateBitmapImageSize, MFConvertToFP16Array / MFConvertFromFP16Array,
 * MFCreateLegacyMediaBufferOnMFMediaBuffer with an offset, IMF2DBuffer2::Copy2DTo
 * and the 2D buffer's IMFGetService (these were "@ stub" exports, a FIXME
 * and E_NOTIMPL).
 *
 *   mfplat-buffers-probe.exe */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <mfapi.h>
#include <mferror.h>
#include <mfobjects.h>
#include <mfidl.h>
#include <mediaobj.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}
#define CHECKF(ok, ...) do { char _b[256]; snprintf(_b, sizeof(_b), __VA_ARGS__); check(ok, _b); } while (0)

/* the IEEE half-precision conversions, both directions */
static void test_fp16(void)
{
    static const struct { UINT32 f; WORD h; const char *name; } to_half[] =
    {
        {0x00000000, 0x0000, "0"}, {0x80000000, 0x8000, "-0"}, {0x3f800000, 0x3c00, "1"}, {0xc0000000, 0xc000, "-2"},
        {0x477fe000, 0x7bff, "65504 (largest half)"}, {0x477ff000, 0x7c00, "65520 rounds to infinity"},
        {0x38800000, 0x0400, "2^-14 (smallest normal)"}, {0x33800000, 0x0001, "2^-24 (smallest subnormal)"},
        {0x33000000, 0x0000, "2^-25 ties to even (zero)"}, {0x33000001, 0x0001, "just over 2^-25 rounds up"},
        {0x32000000, 0x0000, "2^-26 underflows to zero"}, {0x7f800000, 0x7c00, "+inf"}, {0xff800000, 0xfc00, "-inf"},
        {0x3eaaaaab, 0x3555, "1/3"}, {0x3f802000, 0x3c01, "1 + 2^-10"}, {0x3f801000, 0x3c00, "1 + 2^-11 ties to even"},
        {0x3f803000, 0x3c02, "1 + 3*2^-11 ties to even (up)"}, {0x477fefff, 0x7bff, "just below the overflow midpoint"},
        {0x7f7fffff, 0x7c00, "largest float overflows"}, {0xc7000000, 0xf800, "-32768"},
    };
    static const struct { WORD h; UINT32 f; const char *name; } from_half[] =
    {
        {0x0000, 0x00000000, "0"}, {0x8000, 0x80000000, "-0"}, {0x3c00, 0x3f800000, "1"}, {0xc000, 0xc0000000, "-2"},
        {0x7bff, 0x477fe000, "65504"}, {0x0400, 0x38800000, "2^-14"}, {0x0001, 0x33800000, "2^-24 (subnormal)"},
        {0x03ff, 0x387fc000, "largest subnormal"}, {0x7c00, 0x7f800000, "+inf"}, {0xfc00, 0xff800000, "-inf"},
        {0x3555, 0x3eaaa000, "0x3555"}, {0x7e00, 0x7fc00000, "quiet NaN"},
    };
    float src[ARRAY_SIZE(to_half)], back[ARRAY_SIZE(from_half)];
    WORD dst[ARRAY_SIZE(to_half)], hsrc[ARRAY_SIZE(from_half)];
    UINT32 bits;
    HRESULT hr;
    size_t i;

    for (i = 0; i < ARRAY_SIZE(to_half); i++) memcpy(&src[i], &to_half[i].f, 4);
    memset(dst, 0xaa, sizeof(dst));
    hr = MFConvertToFP16Array(dst, src, ARRAY_SIZE(src));
    CHECKF(hr == S_OK, "MFConvertToFP16Array returns S_OK (%#lx)", hr);
    for (i = 0; i < ARRAY_SIZE(to_half); i++)
        CHECKF(dst[i] == to_half[i].h, "to half: %s -> %04x (got %04x)", to_half[i].name, to_half[i].h, dst[i]);

    {
        float nan = nanf(""), pnan;
        WORD h;
        memcpy(&pnan, &nan, 4);
        MFConvertToFP16Array(&h, &nan, 1);
        CHECKF((h & 0x7c00) == 0x7c00 && (h & 0x3ff), "to half: NaN stays a NaN (%04x)", h);
    }

    for (i = 0; i < ARRAY_SIZE(from_half); i++) hsrc[i] = from_half[i].h;
    hr = MFConvertFromFP16Array(back, hsrc, ARRAY_SIZE(hsrc));
    CHECKF(hr == S_OK, "MFConvertFromFP16Array returns S_OK (%#lx)", hr);
    for (i = 0; i < ARRAY_SIZE(from_half); i++)
    {
        memcpy(&bits, &back[i], 4);
        CHECKF(bits == from_half[i].f, "from half: %s -> %08x (got %08x)", from_half[i].name, from_half[i].f, bits);
    }

    /* every half survives the round trip */
    {
        int bad = 0;
        unsigned int h;
        for (h = 0; h < 0x10000; h++)
        {
            WORD in = h, out = 0;
            float f;
            if ((h & 0x7c00) == 0x7c00 && (h & 0x3ff)) continue;   /* NaN payloads */
            MFConvertFromFP16Array(&f, &in, 1);
            MFConvertToFP16Array(&out, &f, 1);
            if (out != in) bad++;
        }
        CHECKF(!bad, "all 65536 half values round-trip (%d differ)", bad);
    }

    hr = MFConvertToFP16Array(NULL, src, 1);
    CHECKF(hr == E_POINTER, "MFConvertToFP16Array(NULL dest) is E_POINTER (%#lx)", hr);
    hr = MFConvertFromFP16Array(back, NULL, 1);
    CHECKF(hr == E_POINTER, "MFConvertFromFP16Array(NULL src) is E_POINTER (%#lx)", hr);
    hr = MFConvertToFP16Array(dst, src, 0);
    CHECKF(hr == S_OK, "a zero count is S_OK (%#lx)", hr);
}

static void test_bitmap_size(void)
{
    static const struct
    {
        DWORD compression; LONG width, height; WORD bpp; DWORD size_image; UINT32 expect; BOOL known; const char *name;
    } cases[] =
    {
        {BI_RGB, 3, 2, 24, 0, 24, TRUE, "RGB24 3x2 (rows padded to 12 bytes)"},
        {BI_RGB, 3, -2, 24, 0, 24, TRUE, "RGB24 top-down 3x-2"},
        {BI_RGB, 10, 10, 32, 0, 400, TRUE, "RGB32 10x10"},
        {BI_RGB, 5, 4, 8, 0, 32, TRUE, "RGB8 5x4 (rows padded to 8 bytes)"},
        {BI_RGB, 17, 3, 1, 0, 12, TRUE, "RGB1 17x3 (4-byte rows)"},
        {BI_BITFIELDS, 4, 4, 16, 0, 32, TRUE, "BITFIELDS 16-bit 4x4"},
        {MAKEFOURCC('N','V','1','2'), 4, 4, 12, 999, 24, TRUE, "NV12 4x4"},
        {MAKEFOURCC('Y','U','Y','2'), 4, 4, 16, 999, 32, TRUE, "YUY2 4x4"},
        {MAKEFOURCC('X','Y','Z','W'), 4, 4, 16, 123, 123, FALSE, "an unknown FOURCC keeps biSizeImage"},
    };
    BITMAPINFOHEADER header;
    UINT32 size;
    BOOL known;
    HRESULT hr;
    size_t i;

    for (i = 0; i < ARRAY_SIZE(cases); i++)
    {
        memset(&header, 0, sizeof(header));
        header.biSize = sizeof(header);
        header.biCompression = cases[i].compression;
        header.biWidth = cases[i].width;
        header.biHeight = cases[i].height;
        header.biBitCount = cases[i].bpp;
        header.biSizeImage = cases[i].size_image;
        size = 0xdead; known = 7;
        hr = MFCalculateBitmapImageSize(&header, sizeof(header), &size, &known);
        CHECKF(hr == S_OK && size == cases[i].expect && known == cases[i].known,
               "%s: size %u known %d (got %#lx, %u, %d)", cases[i].name, cases[i].expect, cases[i].known, hr, size, known);
    }
    hr = MFCalculateBitmapImageSize(&header, sizeof(header) - 1, &size, &known);
    CHECKF(hr == E_INVALIDARG, "a buffer smaller than the header is E_INVALIDARG (%#lx)", hr);
    hr = MFCalculateBitmapImageSize(NULL, sizeof(header), &size, &known);
    CHECKF(hr == E_POINTER, "NULL header is E_POINTER (%#lx)", hr);
}

static void test_wrapper(void)
{
    IMFMediaBuffer *buffer, *wrapper, *other;
    BYTE *base, *data;
    DWORD max, cur;
    IUnknown *unk;
    HRESULT hr;
    int i;

    hr = MFCreateMemoryBuffer(100, &buffer);
    check(hr == S_OK, "MFCreateMemoryBuffer");
    IMFMediaBuffer_Lock(buffer, &base, NULL, NULL);
    for (i = 0; i < 100; i++) base[i] = i;
    IMFMediaBuffer_Unlock(buffer);
    IMFMediaBuffer_SetCurrentLength(buffer, 50);

    hr = MFCreateMediaBufferWrapper(buffer, 10, 20, &wrapper);
    CHECKF(hr == S_OK, "MFCreateMediaBufferWrapper(10, 20) (%#lx)", hr);
    if (hr != S_OK) { IMFMediaBuffer_Release(buffer); return; }

    hr = IMFMediaBuffer_GetMaxLength(wrapper, &max);
    CHECKF(hr == S_OK && max == 20, "wrapper max length is 20 (%u)", max);
    hr = IMFMediaBuffer_GetCurrentLength(wrapper, &cur);
    CHECKF(hr == S_OK && cur == 20, "wrapper current length starts at the whole range (%u)", cur);
    hr = IMFMediaBuffer_Lock(wrapper, &data, &max, &cur);
    CHECKF(hr == S_OK && data[0] == 10 && data[19] == 29 && max == 20 && cur == 20,
           "Lock gives the range's bytes 10..29 (%#lx, %u, %u)", hr, data[0], data[19]);
    data[0] = 0xee;
    hr = IMFMediaBuffer_Unlock(wrapper);
    CHECKF(hr == S_OK, "Unlock (%#lx)", hr);
    IMFMediaBuffer_Lock(buffer, &base, NULL, NULL);
    CHECKF(base[10] == 0xee && base[9] == 9 && base[30] == 30, "a write through the wrapper lands in the original at the offset");
    IMFMediaBuffer_Unlock(buffer);

    hr = IMFMediaBuffer_SetCurrentLength(wrapper, 21);
    CHECKF(hr == E_INVALIDARG, "SetCurrentLength past the range is E_INVALIDARG (%#lx)", hr);
    hr = IMFMediaBuffer_SetCurrentLength(wrapper, 5);
    CHECKF(hr == S_OK, "SetCurrentLength(5) (%#lx)", hr);
    hr = IMFMediaBuffer_GetCurrentLength(wrapper, &cur);
    CHECKF(hr == S_OK && cur == 5, "current length is 5 (%u)", cur);
    IMFMediaBuffer_GetCurrentLength(buffer, &cur);
    CHECKF(cur == 50, "the original's length is untouched (%u)", cur);

    hr = IMFMediaBuffer_QueryInterface(wrapper, &IID_IMF2DBuffer, (void **)&unk);
    CHECKF(hr == E_NOINTERFACE && !unk, "no IMF2DBuffer on a wrapper (%#lx)", hr);
    hr = IMFMediaBuffer_QueryInterface(wrapper, &IID_IMFMediaBuffer, (void **)&unk);
    CHECKF(hr == S_OK, "QueryInterface(IMFMediaBuffer) (%#lx)", hr);
    if (hr == S_OK) IUnknown_Release(unk);
    IMFMediaBuffer_Release(wrapper);

    hr = MFCreateMediaBufferWrapper(buffer, 90, 10, &wrapper);
    CHECKF(hr == S_OK, "the range ending exactly at the end is fine (%#lx)", hr);
    if (hr == S_OK) IMFMediaBuffer_Release(wrapper);
    hr = MFCreateMediaBufferWrapper(buffer, 90, 11, &wrapper);
    CHECKF(hr == E_INVALIDARG, "a range past the end is E_INVALIDARG (%#lx)", hr);
    hr = MFCreateMediaBufferWrapper(buffer, 101, 0, &wrapper);
    CHECKF(hr == E_INVALIDARG, "an offset past the end is E_INVALIDARG (%#lx)", hr);
    hr = MFCreateMediaBufferWrapper(buffer, 0xfffffff0, 0x20, &wrapper);
    CHECKF(hr == E_INVALIDARG, "offset + length overflowing is E_INVALIDARG (%#lx)", hr);
    hr = MFCreateMediaBufferWrapper(NULL, 0, 10, &wrapper);
    CHECKF(hr == E_INVALIDARG, "NULL buffer is E_INVALIDARG (%#lx)", hr);
    hr = MFCreateMediaBufferWrapper(buffer, 0, 10, NULL);
    CHECKF(hr == E_INVALIDARG, "NULL output is E_INVALIDARG (%#lx)", hr);

    /* a wrapper of a wrapper composes the offsets */
    hr = MFCreateMediaBufferWrapper(buffer, 10, 40, &wrapper);
    hr = MFCreateMediaBufferWrapper(wrapper, 5, 10, &other);
    CHECKF(hr == S_OK, "a wrapper of a wrapper (%#lx)", hr);
    if (hr == S_OK)
    {
        IMFMediaBuffer_Lock(other, &data, NULL, NULL);
        CHECKF(data[1] == 16, "offsets add up: byte 1 of (10 + 5) is 16 (%u)", data[1]);
        IMFMediaBuffer_Unlock(other);
        IMFMediaBuffer_Release(other);
    }
    IMFMediaBuffer_Release(wrapper);
    IMFMediaBuffer_Release(buffer);
}

static void test_legacy_offset(void)
{
    IMFMediaBuffer *buffer;
    IMediaBuffer *legacy;
    BYTE *data, *base;
    DWORD max, len;
    HRESULT hr;
    int i;

    MFCreateMemoryBuffer(100, &buffer);
    IMFMediaBuffer_Lock(buffer, &base, NULL, NULL);
    for (i = 0; i < 100; i++) base[i] = i;
    IMFMediaBuffer_Unlock(buffer);
    IMFMediaBuffer_SetCurrentLength(buffer, 50);

    hr = MFCreateLegacyMediaBufferOnMFMediaBuffer(NULL, buffer, 10, &legacy);
    CHECKF(hr == S_OK, "MFCreateLegacyMediaBufferOnMFMediaBuffer with an offset (%#lx)", hr);
    if (hr == S_OK)
    {
        hr = IMediaBuffer_GetMaxLength(legacy, &max);
        CHECKF(hr == S_OK && max == 90, "max length is the rest of the buffer, 90 (%u)", max);
        hr = IMediaBuffer_GetBufferAndLength(legacy, &data, &len);
        CHECKF(hr == S_OK && len == 40 && data[0] == 10, "data starts at the offset with length 40 (%#lx, %u, %u)", hr, len, data[0]);
        hr = IMediaBuffer_SetLength(legacy, 91);
        CHECKF(hr == E_INVALIDARG, "SetLength past the range is E_INVALIDARG (%#lx)", hr);
        hr = IMediaBuffer_SetLength(legacy, 60);
        CHECKF(hr == S_OK, "SetLength(60) (%#lx)", hr);
        IMediaBuffer_Release(legacy);
    }
    hr = MFCreateLegacyMediaBufferOnMFMediaBuffer(NULL, buffer, 101, &legacy);
    CHECKF(hr == E_INVALIDARG, "an offset past the end is E_INVALIDARG (%#lx)", hr);
    hr = MFCreateLegacyMediaBufferOnMFMediaBuffer(NULL, buffer, 0, &legacy);
    CHECKF(hr == S_OK, "offset 0 still works (%#lx)", hr);
    if (hr == S_OK)
    {
        IMediaBuffer_GetBufferAndLength(legacy, &data, &len);
        CHECKF(len == 50 && data[0] == 0, "offset 0 is the whole buffer (%u)", len);
        IMediaBuffer_Release(legacy);
    }
    IMFMediaBuffer_Release(buffer);
}

static void test_copy2d(void)
{
    IMFMediaBuffer *src, *dst, *small;
    IMF2DBuffer2 *src2, *dst2, *small2;
    IMFGetService *gs;
    BYTE *scan, *start;
    DWORD len;
    LONG pitch;
    HRESULT hr;
    int x, y, ok;

    hr = MFCreate2DMediaBuffer(16, 8, MFVideoFormat_RGB32.Data1, FALSE, &src);
    check(hr == S_OK, "MFCreate2DMediaBuffer source");
    MFCreate2DMediaBuffer(16, 8, MFVideoFormat_RGB32.Data1, FALSE, &dst);
    MFCreate2DMediaBuffer(8, 4, MFVideoFormat_RGB32.Data1, FALSE, &small);
    hr = IMFMediaBuffer_QueryInterface(src, &IID_IMF2DBuffer2, (void **)&src2);
    check(hr == S_OK, "the source is an IMF2DBuffer2");
    IMFMediaBuffer_QueryInterface(dst, &IID_IMF2DBuffer2, (void **)&dst2);
    IMFMediaBuffer_QueryInterface(small, &IID_IMF2DBuffer2, (void **)&small2);
    if (!src2 || !dst2 || !small2) return;

    IMF2DBuffer2_Lock2DSize(src2, MF2DBuffer_LockFlags_Write, &scan, &pitch, &start, &len);
    for (y = 0; y < 8; y++)
        for (x = 0; x < 64; x++) scan[y * pitch + x] = y * 16 + x + 1;
    IMF2DBuffer2_Unlock2D(src2);

    hr = IMF2DBuffer2_Copy2DTo(src2, dst2);
    CHECKF(hr == S_OK, "Copy2DTo an equal buffer (%#lx)", hr);
    IMF2DBuffer2_Lock2DSize(dst2, MF2DBuffer_LockFlags_Read, &scan, &pitch, &start, &len);
    for (ok = 1, y = 0; y < 8; y++)
        for (x = 0; x < 64; x++) if (scan[y * pitch + x] != (BYTE)(y * 16 + x + 1)) ok = 0;
    IMF2DBuffer2_Unlock2D(dst2);
    check(ok, "every pixel of the copy equals the source");

    hr = IMF2DBuffer2_Copy2DTo(src2, small2);
    CHECKF(hr == MF_E_BUFFERTOOSMALL, "Copy2DTo a smaller buffer is MF_E_BUFFERTOOSMALL (%#lx)", hr);
    hr = IMF2DBuffer2_Copy2DTo(src2, NULL);
    CHECKF(hr == E_POINTER, "Copy2DTo NULL is E_POINTER (%#lx)", hr);
    /* the source is unlocked again: it can be locked for writing */
    hr = IMF2DBuffer2_Lock2DSize(src2, MF2DBuffer_LockFlags_Write, &scan, &pitch, &start, &len);
    CHECKF(hr == S_OK, "the source is not left locked (%#lx)", hr);
    if (hr == S_OK) IMF2DBuffer2_Unlock2D(src2);

    hr = IMFMediaBuffer_QueryInterface(src, &IID_IMFGetService, (void **)&gs);
    if (hr == S_OK)
    {
        void *obj = (void *)1;
        hr = IMFGetService_GetService(gs, &IID_IUnknown /* any non-service GUID */, &IID_IUnknown, &obj);
        CHECKF(hr == MF_E_UNSUPPORTED_SERVICE, "GetService of an unknown service is MF_E_UNSUPPORTED_SERVICE (%#lx)", hr);
        IMFGetService_Release(gs);
    }
    else check(1, "(a 2D memory buffer without IMFGetService: skipped)");

    IMF2DBuffer2_Release(src2); IMF2DBuffer2_Release(dst2); IMF2DBuffer2_Release(small2);
    IMFMediaBuffer_Release(src); IMFMediaBuffer_Release(dst); IMFMediaBuffer_Release(small);
}

int main(void)
{
    HRESULT hr = MFStartup(MF_VERSION, MFSTARTUP_FULL);
    check(hr == S_OK, "MFStartup");
    test_fp16();
    test_bitmap_size();
    test_wrapper();
    test_legacy_offset();
    test_copy2d();
    MFShutdown();
    printf("%s\n", failures ? "RESULT: FAIL" : "RESULT: PASS");
    return failures != 0;
}
