/* Native probe for IMediaDet bitmap grabbing (qedit.dll): EnterBitmapGrabMode,
 * GetBitmapBits, WriteBitmapBits, GetSampleGrabber. It writes a small
 * uncompressed 24-bit AVI (3 frames of 8x4 pixels) and grabs from it. */
#define __USE_MINGW_ANSI_STDIO 1
#define COBJMACROS
#include <windows.h>
#include <dshow.h>
#include <qedit.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures, checks;
#define CHECK(cond, ...) do { checks++; if (!(cond)) { failures++; printf("FAIL  line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)
#define CHECK_HR(got, want) CHECK((got) == (want), "%s: hr %#lx, expected %#lx", #got, (unsigned long)(got), (unsigned long)(want))

#define W 8
#define H 4
#define FRAMES 3
#define STRIDE (W * 3)
#define FRAME_SIZE (STRIDE * H)

/* colour of source pixel (x, y counted from the top) in frame f */
static void src_pixel(int f, int x, int y, BYTE *bgr)
{
    bgr[0] = x * 16;
    bgr[1] = y * 40;
    bgr[2] = 50 + f * 60;
}

static BYTE *buf;
static DWORD len;
static void put(const void *p, DWORD n) { memcpy(buf + len, p, n); len += n; }
static void put32(DWORD v) { put(&v, 4); }
static void put16(WORD v) { put(&v, 2); }

static void write_avi(const WCHAR *path)
{
    DWORD riff_size_pos, hdrl_size_pos, strl_size_pos, movi_size_pos, idx_pos, movi_start;
    DWORD offsets[FRAMES];
    BYTE frame[FRAME_SIZE];
    HANDLE file;
    DWORD written;
    int f, x, y;

    buf = calloc(1, 4096);
    put("RIFF", 4); riff_size_pos = len; put32(0); put("AVI ", 4);
    put("LIST", 4); hdrl_size_pos = len; put32(0); put("hdrl", 4);
    put("avih", 4); put32(56);
    put32(100000); put32(FRAME_SIZE * 10); put32(0); put32(0x10); put32(FRAMES); put32(0); put32(1);
    put32(FRAME_SIZE); put32(W); put32(H); put32(0); put32(0); put32(0); put32(0);
    put("LIST", 4); strl_size_pos = len; put32(0); put("strl", 4);
    put("strh", 4); put32(56);
    put("vids", 4); put("DIB ", 4); put32(0); put16(0); put16(0); put32(0); put32(1); put32(10); put32(0);
    put32(FRAMES); put32(FRAME_SIZE); put32(0xffffffff); put32(0); put16(0); put16(0); put16(W); put16(H);
    put("strf", 4); put32(40);
    put32(40); put32(W); put32(H); put16(1); put16(24); put32(BI_RGB); put32(FRAME_SIZE); put32(0); put32(0); put32(0); put32(0);
    *(DWORD *)(buf + strl_size_pos) = len - strl_size_pos - 4;
    *(DWORD *)(buf + hdrl_size_pos) = len - hdrl_size_pos - 4;
    put("LIST", 4); movi_size_pos = len; put32(0); movi_start = len; put("movi", 4);
    for (f = 0; f < FRAMES; f++)
    {
        for (y = 0; y < H; y++)
            for (x = 0; x < W; x++)
                src_pixel(f, x, H - 1 - y, frame + y * STRIDE + x * 3); /* bottom-up */
        offsets[f] = len - movi_start;
        put("00db", 4); put32(FRAME_SIZE); put(frame, FRAME_SIZE);
    }
    *(DWORD *)(buf + movi_size_pos) = len - movi_size_pos - 4;
    put("idx1", 4); idx_pos = len; put32(0);
    for (f = 0; f < FRAMES; f++)
    {
        put("00db", 4); put32(0x10); put32(offsets[f]); put32(FRAME_SIZE);
    }
    *(DWORD *)(buf + idx_pos) = len - idx_pos - 4;
    *(DWORD *)(buf + riff_size_pos) = len - 8;

    file = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    WriteFile(file, buf, len, &written, NULL);
    CloseHandle(file);
}

/* expected pixel of a w x h grab of frame f at (x, y) counted from the top */
static void expect_pixel(int f, int w, int h, int x, int y, BYTE *bgr)
{
    src_pixel(f, x * W / w, y * H / h, bgr);
}

static void check_dib(const char *what, const BYTE *data, LONG size, int f, int w, int h)
{
    const BITMAPINFOHEADER *bih = (const BITMAPINFOHEADER *)data;
    LONG stride = ((w * 24 + 31) >> 3) & ~3;
    int x, y, bad = 0;

    CHECK(size == (LONG)(sizeof(*bih) + stride * h), "%s: size %ld", what, size);
    CHECK(bih->biSize == sizeof(*bih) && bih->biWidth == w && bih->biHeight == h && bih->biPlanes == 1
            && bih->biBitCount == 24 && bih->biCompression == BI_RGB && bih->biSizeImage == (DWORD)(stride * h),
            "%s: header %ld %ld %ld %d %d %ld %ld", what, bih->biSize, bih->biWidth, bih->biHeight,
            bih->biPlanes, bih->biBitCount, bih->biCompression, bih->biSizeImage);
    for (y = 0; y < h; y++)
        for (x = 0; x < w; x++)
        {
            const BYTE *got = data + sizeof(*bih) + (h - 1 - y) * stride + x * 3; /* bottom-up */
            BYTE want[3];
            expect_pixel(f, w, h, x, y, want);
            if (memcmp(got, want, 3) && bad++ < 3)
                CHECK(0, "%s: pixel (%d,%d) is %02x%02x%02x, expected %02x%02x%02x", what, x, y,
                        got[2], got[1], got[0], want[2], want[1], want[0]);
        }
}

int main(void)
{
    WCHAR path[MAX_PATH], bmp[MAX_PATH];
    BYTE *bits = malloc(4096);
    IMediaDet *det;
    ISampleGrabber *sg, *sg2;
    AM_MEDIA_TYPE mt;
    GUID type;
    BSTR name;
    LONG size, count;
    HRESULT hr;

    CoInitialize(NULL);
    GetTempPathW(MAX_PATH, path);
    wcscpy(bmp, path);
    wcscat(path, L"sgprobe.avi");
    wcscat(bmp, L"sgprobe.bmp");
    write_avi(path);

    hr = CoCreateInstance(&CLSID_MediaDet, NULL, CLSCTX_INPROC_SERVER, &IID_IMediaDet, (void **)&det);
    CHECK_HR(hr, S_OK);

    /* nothing loaded yet */
    CHECK_HR(IMediaDet_EnterBitmapGrabMode(det, 0.0), E_INVALIDARG);
    size = 1000;
    CHECK_HR(IMediaDet_GetBitmapBits(det, 0.0, &size, NULL, 0, 0), E_INVALIDARG);
    CHECK_HR(IMediaDet_GetSampleGrabber(det, &sg), VFW_E_WRONG_STATE);
    CHECK(sg == NULL, "grabber output %p", sg);
    CHECK_HR(IMediaDet_GetSampleGrabber(det, NULL), E_POINTER);

    name = SysAllocString(path);
    hr = IMediaDet_put_Filename(det, name);
    SysFreeString(name);
    CHECK_HR(hr, S_OK);
    CHECK_HR(IMediaDet_get_OutputStreams(det, &count), S_OK);
    CHECK(count == 1, "streams %ld", count);
    CHECK_HR(IMediaDet_get_StreamType(det, &type), S_OK);
    CHECK(IsEqualGUID(&type, &MEDIATYPE_Video), "stream type");

    /* argument checks */
    CHECK_HR(IMediaDet_GetBitmapBits(det, 0.0, NULL, NULL, 0, 0), E_POINTER);
    size = 1000;
    CHECK_HR(IMediaDet_GetBitmapBits(det, 0.0, &size, NULL, -1, 4), E_INVALIDARG);
    CHECK_HR(IMediaDet_GetBitmapBits(det, 0.0, &size, NULL, 4, -1), E_INVALIDARG);
    CHECK_HR(IMediaDet_WriteBitmapBits(det, 0.0, 8, 4, NULL), E_POINTER);
    CHECK_HR(IMediaDet_EnterBitmapGrabMode(det, -1.0), E_INVALIDARG);

    /* size queries need no grab */
    size = 0;
    CHECK_HR(IMediaDet_GetBitmapBits(det, 0.0, &size, NULL, 0, 0), S_OK);
    CHECK(size == 40 + STRIDE * H, "native size %ld", size);
    size = 0;
    CHECK_HR(IMediaDet_GetBitmapBits(det, 0.0, &size, NULL, 16, 8), S_OK);
    CHECK(size == 40 + 48 * 8, "16x8 size %ld", size);
    size = 0;
    CHECK_HR(IMediaDet_GetBitmapBits(det, 0.0, &size, NULL, 5, 3), S_OK);
    CHECK(size == 40 + 16 * 3, "5x3 size %ld (rows are 4-byte aligned)", size);
    CHECK_HR(IMediaDet_GetSampleGrabber(det, &sg), VFW_E_WRONG_STATE);

    /* grabs */
    size = 40 + STRIDE * H;
    CHECK_HR(IMediaDet_GetBitmapBits(det, 0.0, &size, (char *)bits, W, H), S_OK);
    check_dib("frame 0 native", bits, size, 0, W, H);

    size = 40 + 48 * 8;
    CHECK_HR(IMediaDet_GetBitmapBits(det, 0.0, &size, (char *)bits, 16, 8), S_OK);
    check_dib("frame 0 doubled", bits, size, 0, 16, 8);

    size = 40 + STRIDE * H;
    CHECK_HR(IMediaDet_GetBitmapBits(det, 0.2, &size, (char *)bits, 0, 0), S_OK);
    check_dib("frame 2", bits, size, 2, W, H);

    size = 40 + STRIDE * H;
    CHECK_HR(IMediaDet_GetBitmapBits(det, 0.1, &size, (char *)bits, W, H), S_OK);
    check_dib("frame 1", bits, size, 1, W, H);

    size = 40 + 16 * 3;
    CHECK_HR(IMediaDet_GetBitmapBits(det, 0.0, &size, (char *)bits, 5, 3), S_OK);
    check_dib("frame 0 5x3", bits, size, 0, 5, 3);

    size = 10;
    CHECK_HR(IMediaDet_GetBitmapBits(det, 0.0, &size, (char *)bits, W, H), E_INVALIDARG);
    CHECK(size == 40 + STRIDE * H, "required size reported: %ld", size);

    /* grab mode is now active */
    sg = NULL;
    CHECK_HR(IMediaDet_GetSampleGrabber(det, &sg), S_OK);
    CHECK(sg != NULL, "no grabber");
    if (sg)
    {
        memset(&mt, 0, sizeof(mt));
        CHECK_HR(ISampleGrabber_GetConnectedMediaType(sg, &mt), S_OK);
        CHECK(IsEqualGUID(&mt.majortype, &MEDIATYPE_Video) && IsEqualGUID(&mt.subtype, &MEDIASUBTYPE_RGB24),
                "grabber media type");
        CoTaskMemFree(mt.pbFormat);
        CHECK_HR(IMediaDet_GetSampleGrabber(det, &sg2), S_OK);
        CHECK(sg2 == sg, "same grabber each time");
        ISampleGrabber_Release(sg2);
        ISampleGrabber_Release(sg);
    }
    CHECK_HR(IMediaDet_EnterBitmapGrabMode(det, 0.1), S_OK);

    /* file output */
    name = SysAllocString(bmp);
    CHECK_HR(IMediaDet_WriteBitmapBits(det, 0.1, W, H, name), S_OK);
    SysFreeString(name);
    {
        HANDLE file = CreateFileW(bmp, GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);
        DWORD read = 0;
        BYTE *data = malloc(4096);
        BITMAPFILEHEADER *fh = (BITMAPFILEHEADER *)data;

        CHECK(file != INVALID_HANDLE_VALUE, "bitmap file not written");
        if (file != INVALID_HANDLE_VALUE)
        {
            ReadFile(file, data, 4096, &read, NULL);
            CloseHandle(file);
            CHECK(read == sizeof(*fh) + 40 + STRIDE * H, "file size %lu", read);
            CHECK(fh->bfType == 0x4d42 && fh->bfSize == read && fh->bfOffBits == sizeof(*fh) + 40,
                    "file header %04x %lu %lu", fh->bfType, fh->bfSize, fh->bfOffBits);
            check_dib("bitmap file", data + sizeof(*fh), read - sizeof(*fh), 1, W, H);
        }
        free(data);
    }

    /* switching streams (here: the same one) leaves grab mode */
    CHECK_HR(IMediaDet_put_CurrentStream(det, 0), S_OK);
    CHECK_HR(IMediaDet_GetSampleGrabber(det, &sg), VFW_E_WRONG_STATE);

    IMediaDet_Release(det);
    DeleteFileW(path);
    DeleteFileW(bmp);
    CoUninitialize();
    printf("%d checks, %d failures\n", checks, failures);
    printf(failures ? "RESULT: FAIL\n" : "RESULT: PASS\n");
    return failures != 0;
}
