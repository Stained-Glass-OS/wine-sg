/* MFInitMediaTypeFromVideoInfoHeader / ...Header2 details (patches/sg/2964), run by
 * test/mfplat-vih-gate.sh: header size check, empty frames, bit rates, display aspect ratio to
 * pixel aspect ratio, palette versus user data, the packed 1/4 bit strides. */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <mfapi.h>
#include <mferror.h>
#include <mfobjects.h>
#include <dshow.h>
#include <dvdmedia.h>
#include <stdio.h>
#include <string.h>

static const GUID MFVideoFormat_RGB1 = {118, 0x0000, 0x0010, {0x80,0x00,0x00,0xaa,0x00,0x38,0x9b,0x71}}; /* D3DFMT_A1 */
static const GUID MFVideoFormat_RGB4 = {0x78785034, 0x0000, 0x0010, {0x80,0x00,0x00,0xaa,0x00,0x38,0x9b,0x71}}; /* '4Pxx' */

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}
#define CHECKF(ok, ...) do { char _b[256]; snprintf(_b, sizeof(_b), __VA_ARGS__); check(ok, _b); } while (0)

static IMFMediaType *mt;
static UINT32 u32(const GUID *key, UINT32 missing)
{
    UINT32 v;
    return SUCCEEDED(IMFMediaType_GetUINT32(mt, key, &v)) ? v : missing;
}
static UINT64 u64(const GUID *key)
{
    UINT64 v;
    return SUCCEEDED(IMFMediaType_GetUINT64(mt, key, &v)) ? v : ~0ull;
}
static int blob(const GUID *key, void *buf, UINT32 cap, UINT32 *size)
{
    return SUCCEEDED(IMFMediaType_GetBlob(mt, key, buf, cap, size));
}

static HRESULT init(int v2, void *vih, UINT32 size, const GUID *subtype)
{
    if (v2) return MFInitMediaTypeFromVideoInfoHeader2(mt, vih, size, subtype);
    return MFInitMediaTypeFromVideoInfoHeader(mt, vih, size, subtype);
}

static void test_version(int v2)
{
    static const BYTE pal[32] = {1,0,0,0, 2,0,0,0, 3,0,0,0, 4,0,0,0, 5,0,0,0, 6,0,0,0, 7,0,0,0, 8,0,0,0};
    static const BYTE user[6] = {6,5,4,3,2,1};
    union { VIDEOINFOHEADER2 h2; VIDEOINFOHEADER h1; BYTE raw[512]; } u;
    VIDEOINFOHEADER2 *h2 = &u.h2;
    VIDEOINFOHEADER *h1 = &u.h1;
    BITMAPINFOHEADER *bih;
    UINT32 hsize = v2 ? sizeof(VIDEOINFOHEADER2) : sizeof(VIDEOINFOHEADER), sz;
    BYTE buf[64];
    char tag[16];
    HRESULT hr;
    int i;

    sprintf(tag, "vih%d", v2 ? 2 : 1);
    memset(&u, 0, sizeof(u));
    bih = v2 ? &h2->bmiHeader : &h1->bmiHeader;

    /* header size */
    hr = init(v2, &u, hsize - 1, &GUID_NULL);
    CHECKF(hr == E_INVALIDARG, "%s: short size hr %#lx", tag, hr);
    hr = init(v2, &u, hsize, &GUID_NULL);
    CHECKF(hr == S_OK, "%s: exact size hr %#lx", tag, hr);

    /* empty frame: no frame size, aspect ratio or interlace mode */
    CHECKF(u64(&MF_MT_FRAME_SIZE) == ~0ull, "%s: frame size of an empty header", tag);
    bih->biWidth = 16;
    init(v2, &u, hsize, &GUID_NULL);
    CHECKF(u64(&MF_MT_FRAME_SIZE) == ~0ull, "%s: frame size without height", tag);
    CHECKF(u64(&MF_MT_PIXEL_ASPECT_RATIO) == ~0ull, "%s: PAR without height", tag);
    CHECKF(u32(&MF_MT_INTERLACE_MODE, 0xdead) == 0xdead, "%s: interlace without height", tag);
    bih->biHeight = 32;
    init(v2, &u, hsize, &GUID_NULL);
    CHECKF(u64(&MF_MT_FRAME_SIZE) == (16ull << 32 | 32), "%s: frame size %#I64x", tag, u64(&MF_MT_FRAME_SIZE));
    CHECKF(u32(&MF_MT_INTERLACE_MODE, 0xdead) == MFVideoInterlace_Progressive, "%s: interlace mode", tag);
    if (v2) CHECKF(u64(&MF_MT_PIXEL_ASPECT_RATIO) == ~0ull, "%s: PAR without DAR", tag);
    else CHECKF(u64(&MF_MT_PIXEL_ASPECT_RATIO) == (1ull << 32 | 1), "%s: PAR 1:1", tag);

    /* bit rates */
    CHECKF(u32(&MF_MT_AVG_BITRATE, 0xdead) == 0xdead, "%s: no bitrate when zero", tag);
    if (v2) h2->dwBitRate = 678910; else h1->dwBitRate = 678910;
    init(v2, &u, hsize, &GUID_NULL);
    CHECKF(u32(&MF_MT_AVG_BITRATE, 0xdead) == 678910, "%s: bitrate %u", tag, u32(&MF_MT_AVG_BITRATE, 0xdead));
    CHECKF(u32(&MF_MT_AVG_BIT_ERROR_RATE, 0xdead) == 0xdead, "%s: no error rate when zero", tag);
    if (v2) h2->dwBitErrorRate = 11121314; else h1->dwBitErrorRate = 11121314;
    init(v2, &u, hsize, &GUID_NULL);
    CHECKF(u32(&MF_MT_AVG_BIT_ERROR_RATE, 0xdead) == 11121314, "%s: error rate", tag);

    /* display aspect ratio -> pixel aspect ratio (frame 16x32) */
    if (v2)
    {
        h2->dwPictAspectRatioX = 123;
        init(v2, &u, hsize, &GUID_NULL);
        CHECKF(u64(&MF_MT_PIXEL_ASPECT_RATIO) == ~0ull, "%s: PAR with half a DAR", tag);
        h2->dwPictAspectRatioY = 456;
        init(v2, &u, hsize, &GUID_NULL);
        CHECKF(u64(&MF_MT_PIXEL_ASPECT_RATIO) == (41ull << 32 | 76), "%s: PAR %#I64x", tag, u64(&MF_MT_PIXEL_ASPECT_RATIO));
        h2->dwPictAspectRatioX = 4;
        h2->dwPictAspectRatioY = 3;
        bih->biWidth = 640;
        bih->biHeight = -480;
        init(v2, &u, hsize, &GUID_NULL);
        CHECKF(u64(&MF_MT_PIXEL_ASPECT_RATIO) == (1ull << 32 | 1), "%s: square pixels PAR", tag);
        h2->dwPictAspectRatioX = 16;
        h2->dwPictAspectRatioY = 9;
        init(v2, &u, hsize, &GUID_NULL);
        CHECKF(u64(&MF_MT_PIXEL_ASPECT_RATIO) == (4ull << 32 | 3), "%s: 16:9 on 640x480 PAR %#I64x", tag, u64(&MF_MT_PIXEL_ASPECT_RATIO));
        h2->dwPictAspectRatioX = h2->dwPictAspectRatioY = 0;
        bih->biWidth = 16;
        bih->biHeight = 32;
    }

    /* strides of the packed formats: DWORD padded, bottom-up gives a negative stride */
    for (i = 0; i < 2; i++)
    {
        const GUID *g = i ? &MFVideoFormat_RGB4 : &MFVideoFormat_RGB1;
        int want = i ? 8 : 4;
        init(v2, &u, hsize, g);
        CHECKF((int)u32(&MF_MT_DEFAULT_STRIDE, 0) == -want, "%s: rgb%d bottom-up stride %d", tag, i ? 4 : 1, (int)u32(&MF_MT_DEFAULT_STRIDE, 0));
        bih->biHeight = -32;
        init(v2, &u, hsize, g);
        CHECKF((int)u32(&MF_MT_DEFAULT_STRIDE, 0) == want, "%s: rgb%d top-down stride %d", tag, i ? 4 : 1, (int)u32(&MF_MT_DEFAULT_STRIDE, 0));
        bih->biHeight = 32;
    }
    init(v2, &u, hsize, &MFVideoFormat_RGB1);
    CHECKF(blob(&MF_MT_PALETTE, buf, sizeof(buf), &sz) && sz == 8, "%s: rgb1 default palette", tag);
    init(v2, &u, hsize, &MFVideoFormat_RGB4);
    CHECKF(!blob(&MF_MT_PALETTE, buf, sizeof(buf), &sz), "%s: rgb4 has no palette", tag);
    init(v2, &u, hsize, &MFVideoFormat_RGB8);
    CHECKF(!blob(&MF_MT_PALETTE, buf, sizeof(buf), &sz), "%s: rgb8 without colors has no palette", tag);

    /* palette versus user data (8 bit): the color table is behind biSize */
    bih->biBitCount = 8;
    bih->biClrUsed = 8;
    memcpy(u.raw + hsize, pal, sizeof(pal));
    bih->biSize = sizeof(*bih) + sizeof(pal);
    hr = init(v2, &u, hsize + sizeof(pal), &MFVideoFormat_RGB8);
    CHECKF(hr == MF_E_INVALIDMEDIATYPE, "%s: palette counted in biSize hr %#lx", tag, hr);
    bih->biSize = sizeof(*bih);
    hr = init(v2, &u, hsize + sizeof(pal), &MFVideoFormat_RGB8);
    CHECKF(hr == S_OK, "%s: palette hr %#lx", tag, hr);
    memset(buf, 0, sizeof(buf));
    CHECKF(blob(&MF_MT_PALETTE, buf, sizeof(buf), &sz) && sz == 32 && !memcmp(buf, pal, 32), "%s: palette contents", tag);
    CHECKF(!blob(&MF_MT_USER_DATA, buf, sizeof(buf), &sz), "%s: no user data next to a palette", tag);
    hr = init(v2, &u, hsize + sizeof(pal) - 4, &MFVideoFormat_RGB8);
    CHECKF(hr == MF_E_INVALIDMEDIATYPE, "%s: truncated palette hr %#lx", tag, hr);

    memcpy(u.raw + hsize, user, sizeof(user));
    memcpy(u.raw + hsize + sizeof(user), pal, sizeof(pal));
    bih->biSize = sizeof(*bih) + sizeof(user);
    hr = init(v2, &u, hsize + sizeof(user) + sizeof(pal), NULL);
    CHECKF(hr == S_OK, "%s: user data + palette hr %#lx", tag, hr);
    memset(buf, 0, sizeof(buf));
    CHECKF(blob(&MF_MT_PALETTE, buf, sizeof(buf), &sz) && sz == 32 && !memcmp(buf, pal, 32), "%s: palette behind user bytes", tag);
    CHECKF(!blob(&MF_MT_USER_DATA, buf, sizeof(buf), &sz), "%s: user data dropped with a palette", tag);

    /* other formats keep the trailing bytes as user data */
    bih->biBitCount = 24;
    bih->biSize = sizeof(*bih);
    hr = init(v2, &u, hsize + sizeof(pal), NULL);
    memset(buf, 0, sizeof(buf));
    CHECKF(hr == S_OK && blob(&MF_MT_USER_DATA, buf, sizeof(buf), &sz) && sz == 32 && !memcmp(buf, user, 6),
            "%s: rgb24 user data", tag);
    CHECKF(!blob(&MF_MT_PALETTE, buf, sizeof(buf), &sz), "%s: rgb24 no palette", tag);
}

int main(void)
{
    HRESULT hr;

    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    hr = MFStartup(MF_VERSION, MFSTARTUP_FULL);
    if (hr != S_OK) { printf("FAIL  MFStartup %#lx\n", hr); return 1; }
    hr = MFCreateMediaType(&mt);
    if (hr != S_OK) { printf("FAIL  MFCreateMediaType %#lx\n", hr); return 1; }
    test_version(0);
    test_version(1);
    IMFMediaType_Release(mt);
    MFShutdown();
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures ? 1 : 0;
}
