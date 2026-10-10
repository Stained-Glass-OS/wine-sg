/* mfplat media type conversions (patches/sg/2965), run by test/mfplat-mtconv-gate.sh:
 * MFInitMediaTypeFromMFVideoFormat (overflow of the frame size, chroma-block strides) and
 * MFInitMediaTypeFromAMMediaType / MFCreateMediaTypeFromRepresentation (empty type, MFVIDEOFORMAT
 * format blocks, the audio subtype). */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <mfapi.h>
#include <mferror.h>
#include <mfobjects.h>
#include <dshow.h>
#include <stdio.h>
#include <string.h>

static const GUID MY_FORMAT_MFVideoFormat = {0xaed4ab2d, 0x7326, 0x43cb, {0x94,0x64,0xc8,0x79,0xca,0xb9,0xc4,0x3d}};
static const GUID MY_AM_REPR = {0xe2e42ad2, 0x132c, 0x491e, {0xa2,0x68,0x3c,0x7c,0x2d,0xca,0x18,0x1f}};
#define FCC_GUID(name, fcc) static const GUID name = {fcc, 0x0000, 0x0010, {0x80,0x00,0x00,0xaa,0x00,0x38,0x9b,0x71}}
FCC_GUID(G_NV12, MAKEFOURCC('N','V','1','2'));
FCC_GUID(G_I420, MAKEFOURCC('I','4','2','0'));
FCC_GUID(G_YV12, MAKEFOURCC('Y','V','1','2'));
FCC_GUID(G_NV11, MAKEFOURCC('N','V','1','1'));
FCC_GUID(G_YUY2, MAKEFOURCC('Y','U','Y','2'));
FCC_GUID(G_PCM, 1);
FCC_GUID(G_FLOAT, 3);
FCC_GUID(G_H264, MAKEFOURCC('H','2','6','4'));
static const GUID G_RGB32 = {22, 0x0000, 0x0010, {0x80,0x00,0x00,0xaa,0x00,0x38,0x9b,0x71}};
static const GUID G_MT_VIDEO = {0x73646976, 0x0000, 0x0010, {0x80,0x00,0x00,0xaa,0x00,0x38,0x9b,0x71}};
static const GUID G_MT_AUDIO = {0x73647561, 0x0000, 0x0010, {0x80,0x00,0x00,0xaa,0x00,0x38,0x9b,0x71}};
static const GUID G_FMT_WAVE = {0x05589f81, 0xc356, 0x11ce, {0xbf,0x01,0x00,0xaa,0x00,0x55,0x59,0x5a}};

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
static GUID guid(const GUID *key)
{
    GUID g;
    if (FAILED(IMFMediaType_GetGUID(mt, key, &g))) memset(&g, 0xcd, sizeof(g));
    return g;
}

static void test_mfvideoformat(void)
{
    static const struct { const GUID *sub; unsigned int width, stride; const char *name; } strides[] =
    {
        { &G_NV12, 123, 124, "NV12" }, { &G_NV12, 124, 124, "NV12 even" }, { &G_I420, 123, 124, "I420" },
        { &G_YV12, 5, 6, "YV12" }, { &G_NV11, 123, 124, "NV11" }, { &G_NV11, 121, 124, "NV11 b" },
        { &G_YUY2, 123, 246, "YUY2" }, { &G_RGB32, 123, 492, "RGB32" },
    };
    MFVIDEOFORMAT f;
    unsigned int i;
    HRESULT hr;

    memset(&f, 0, sizeof(f));
    f.dwSize = sizeof(f);
    f.guidFormat = G_NV12;
    f.videoInfo.dwWidth = -123;
    f.videoInfo.dwHeight = 456;
    hr = MFInitMediaTypeFromMFVideoFormat(mt, &f, sizeof(f));
    CHECKF(hr == HRESULT_FROM_WIN32(ERROR_ARITHMETIC_OVERFLOW), "negative width hr %#lx", hr);
    f.videoInfo.dwWidth = 123;
    f.videoInfo.dwHeight = -456;
    hr = MFInitMediaTypeFromMFVideoFormat(mt, &f, sizeof(f));
    CHECKF(hr == HRESULT_FROM_WIN32(ERROR_ARITHMETIC_OVERFLOW), "negative height hr %#lx", hr);
    f.videoInfo.dwHeight = 0x7fffffff;
    hr = MFInitMediaTypeFromMFVideoFormat(mt, &f, sizeof(f));
    CHECKF(hr == S_OK, "INT_MAX height hr %#lx", hr);
    f.videoInfo.dwHeight = 456;

    for (i = 0; i < sizeof(strides) / sizeof(*strides); i++)
    {
        IMFMediaType_DeleteAllItems(mt);
        f.guidFormat = *strides[i].sub;
        f.videoInfo.dwWidth = strides[i].width;
        hr = MFInitMediaTypeFromMFVideoFormat(mt, &f, sizeof(f));
        CHECKF(hr == S_OK && u32(&MF_MT_DEFAULT_STRIDE, 0) == strides[i].stride, "%s width %u stride %u (want %u)",
                strides[i].name, strides[i].width, u32(&MF_MT_DEFAULT_STRIDE, 0), strides[i].stride);
    }
    IMFMediaType_DeleteAllItems(mt);
    f.guidFormat = G_H264;
    MFInitMediaTypeFromMFVideoFormat(mt, &f, sizeof(f));
    CHECKF(u32(&MF_MT_DEFAULT_STRIDE, 0xdead) == 0xdead, "H264 has no stride");
}

static void test_ammediatype(void)
{
    static const MFVideoArea aperture = {.OffsetX = {.value = 3}, .OffsetY = {.value = 4}, .Area = {.cx = 100, .cy = 200}};
    AM_MEDIA_TYPE am;
    MFVIDEOFORMAT f;
    WAVEFORMATEX wfx;
    IMFMediaType *created = NULL;
    GUID g;
    BYTE buf[64];
    UINT32 size;
    HRESULT hr;

    memset(&am, 0, sizeof(am));
    hr = MFInitMediaTypeFromAMMediaType(mt, &am);
    g = guid(&MF_MT_MAJOR_TYPE);
    CHECKF(hr == S_OK && IsEqualGUID(&g, &GUID_NULL), "empty AM type major hr %#lx", hr);
    g = guid(&MF_MT_SUBTYPE);
    CHECKF(IsEqualGUID(&g, &GUID_NULL), "empty AM type subtype");
    hr = MFCreateMediaTypeFromRepresentation(MY_AM_REPR, &am, &created);
    CHECKF(hr == S_OK && created, "MFCreateMediaTypeFromRepresentation of an empty type hr %#lx", hr);
    if (created) IMFMediaType_Release(created);

    /* an MFVIDEOFORMAT block keeps the geometry, aperture included */
    memset(&f, 0, sizeof(f));
    f.dwSize = sizeof(f);
    f.guidFormat = G_NV12;
    f.videoInfo.dwWidth = 1920;
    f.videoInfo.dwHeight = 1088;
    f.videoInfo.MinimumDisplayAperture = aperture;
    memset(&am, 0, sizeof(am));
    am.majortype = G_MT_VIDEO;
    am.formattype = MY_FORMAT_MFVideoFormat;
    am.pbFormat = (BYTE *)&f;
    am.cbFormat = sizeof(f);
    hr = MFInitMediaTypeFromAMMediaType(mt, &am);
    CHECKF(hr == S_OK, "MFVIDEOFORMAT AM type hr %#lx", hr);
    g = guid(&MF_MT_SUBTYPE);
    CHECKF(IsEqualGUID(&g, &G_NV12), "MFVIDEOFORMAT subtype");
    CHECKF(u64(&MF_MT_FRAME_SIZE) == (1920ull << 32 | 1088), "MFVIDEOFORMAT frame size %#I64x", u64(&MF_MT_FRAME_SIZE));
    memset(buf, 0, sizeof(buf));
    hr = IMFMediaType_GetBlob(mt, &MF_MT_MINIMUM_DISPLAY_APERTURE, buf, sizeof(buf), &size);
    CHECKF(hr == S_OK && size == sizeof(aperture) && !memcmp(buf, &aperture, size), "MFVIDEOFORMAT aperture hr %#lx", hr);

    /* the AM subtype wins over the wave format tag; without one the tag decides */
    memset(&wfx, 0, sizeof(wfx));
    memset(&am, 0, sizeof(am));
    am.majortype = G_MT_AUDIO;
    am.formattype = G_FMT_WAVE;
    am.pbFormat = (BYTE *)&wfx;
    am.cbFormat = sizeof(wfx);
    am.subtype = G_PCM;
    hr = MFInitMediaTypeFromAMMediaType(mt, &am);
    g = guid(&MF_MT_SUBTYPE);
    CHECKF(hr == S_OK && IsEqualGUID(&g, &G_PCM), "audio AM subtype PCM over tag 0, hr %#lx", hr);
    am.subtype = G_FLOAT;
    MFInitMediaTypeFromAMMediaType(mt, &am);
    g = guid(&MF_MT_SUBTYPE);
    CHECKF(IsEqualGUID(&g, &G_FLOAT), "audio AM subtype float");
    wfx.wFormatTag = WAVE_FORMAT_PCM;
    am.subtype = GUID_NULL;
    MFInitMediaTypeFromAMMediaType(mt, &am);
    g = guid(&MF_MT_SUBTYPE);
    CHECKF(IsEqualGUID(&g, &G_PCM), "audio tag decides without AM subtype");
}

int main(void)
{
    HRESULT hr;

    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    hr = MFStartup(MF_VERSION, MFSTARTUP_FULL);
    if (hr != S_OK) { printf("FAIL  MFStartup %#lx\n", hr); return 1; }
    hr = MFCreateMediaType(&mt);
    if (hr != S_OK) { printf("FAIL  MFCreateMediaType %#lx\n", hr); return 1; }
    test_mfvideoformat();
    test_ammediatype();
    IMFMediaType_Release(mt);
    MFShutdown();
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures ? 1 : 0;
}
