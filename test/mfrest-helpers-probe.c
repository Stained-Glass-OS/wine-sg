/* mfplat helpers (patch 2862): MFInitVideoFormat, MFGetUncompressedVideoFormat, MFConvertColorInfoFromDXVA,
 * MFValidateMediaTypeSize, MFSerializeAttributesToStream / MFDeserializeAttributesFromStream.
 * Prints "FAIL ..." lines for every wrong value and "RESULT: PASS" at the end when there is none. */
#define COBJMACROS
#include <windows.h>
#include <stdio.h>
#include <initguid.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mferror.h>
#include <d3d9types.h>
#include <strmif.h>
#include <amvideo.h>
#include <dvdmedia.h>
#include <uuids.h>

static int failures;
#define CHECK(cond, ...) do { if (!(cond)) { failures++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static HRESULT (WINAPI *pMFInitVideoFormat)(MFVIDEOFORMAT *, int);
static DWORD (WINAPI *pMFGetUncompressedVideoFormat)(const MFVIDEOFORMAT *);
static HRESULT (WINAPI *pMFConvertColorInfoFromDXVA)(MFVIDEOFORMAT *, DWORD);
static HRESULT (WINAPI *pMFValidateMediaTypeSize)(GUID, UINT8 *, UINT32);
static HRESULT (WINAPI *pMFSerializeAttributesToStream)(IMFAttributes *, DWORD, IStream *);
static HRESULT (WINAPI *pMFDeserializeAttributesFromStream)(IMFAttributes *, DWORD, IStream *);

static void test_init_video_format(void)
{
    static const struct
    {
        unsigned w, h, par_n, par_d, fps_n, fps_d, interlace, primaries, matrix;
        const GUID *guid;
    } t[] =
    {
        {0},
        { 720, 480, 10, 11, 30000, 1001, MFVideoInterlace_FieldInterleavedLowerFirst, MFVideoPrimaries_SMPTE170M, MFVideoTransferMatrix_BT601, &MFVideoFormat_Base },
        { 720, 576, 12, 11, 25, 1, MFVideoInterlace_FieldInterleavedUpperFirst, MFVideoPrimaries_BT470_2_SysBG, MFVideoTransferMatrix_BT601, &MFVideoFormat_Base },
        { 720, 480, 10, 11, 30000, 1001, MFVideoInterlace_FieldInterleavedUpperFirst, MFVideoPrimaries_SMPTE170M, MFVideoTransferMatrix_BT601, &MFVideoFormat_MPEG2 },
        { 720, 576, 12, 11, 25, 1, MFVideoInterlace_FieldInterleavedUpperFirst, MFVideoPrimaries_BT470_2_SysBG, MFVideoTransferMatrix_BT601, &MFVideoFormat_MPEG2 },
        { 720, 576, 12, 11, 25, 1, MFVideoInterlace_FieldInterleavedLowerFirst, MFVideoPrimaries_BT470_2_SysBG, MFVideoTransferMatrix_BT601, &MFVideoFormat_DVSD },
        { 720, 480, 10, 11, 30000, 1001, MFVideoInterlace_FieldInterleavedLowerFirst, MFVideoPrimaries_SMPTE170M, MFVideoTransferMatrix_BT601, &MFVideoFormat_DVSD },
        { 704, 480, 10, 11, 30000, 1001, MFVideoInterlace_FieldInterleavedUpperFirst, MFVideoPrimaries_SMPTE170M, MFVideoTransferMatrix_BT601, &MFVideoFormat_MPEG2 },
        { 1920, 1080, 1, 1, 30000, 1001, MFVideoInterlace_FieldInterleavedUpperFirst, MFVideoPrimaries_BT709, MFVideoTransferMatrix_BT709, &MFVideoFormat_MPEG2 },
        { 1280, 720, 1, 1, 60000, 1001, MFVideoInterlace_Progressive, MFVideoPrimaries_BT709, MFVideoTransferMatrix_BT709, &MFVideoFormat_MPEG2 },
    };
    MFVIDEOFORMAT f;
    HRESULT hr;
    int i;

    hr = pMFInitVideoFormat(NULL, 1);
    CHECK(hr == E_INVALIDARG, "NULL format: %#lx", hr);
    memset(&f, 0xcc, sizeof(f));
    hr = pMFInitVideoFormat(&f, 0);
    CHECK(hr == E_INVALIDARG, "type 0: %#lx", hr);
    hr = pMFInitVideoFormat(&f, 10);
    CHECK(hr == E_INVALIDARG, "type 10: %#lx", hr);
    hr = pMFInitVideoFormat(&f, -1);
    CHECK(hr == E_INVALIDARG, "type -1: %#lx", hr);

    for (i = 1; i <= 9; ++i)
    {
        memset(&f, 0xcc, sizeof(f));
        hr = pMFInitVideoFormat(&f, i);
        CHECK(hr == S_OK, "type %d: %#lx", i, hr);
        CHECK(f.dwSize == sizeof(f), "type %d: size %lu", i, f.dwSize);
        CHECK(f.videoInfo.dwWidth == t[i].w && f.videoInfo.dwHeight == t[i].h, "type %d: %lux%lu", i, f.videoInfo.dwWidth, f.videoInfo.dwHeight);
        CHECK(f.videoInfo.PixelAspectRatio.Numerator == t[i].par_n && f.videoInfo.PixelAspectRatio.Denominator == t[i].par_d,
                "type %d: par %lu/%lu", i, f.videoInfo.PixelAspectRatio.Numerator, f.videoInfo.PixelAspectRatio.Denominator);
        CHECK(f.videoInfo.FramesPerSecond.Numerator == t[i].fps_n && f.videoInfo.FramesPerSecond.Denominator == t[i].fps_d,
                "type %d: fps %lu/%lu", i, f.videoInfo.FramesPerSecond.Numerator, f.videoInfo.FramesPerSecond.Denominator);
        CHECK(f.videoInfo.InterlaceMode == t[i].interlace, "type %d: interlace %lu", i, f.videoInfo.InterlaceMode);
        CHECK(f.videoInfo.ColorPrimaries == t[i].primaries, "type %d: primaries %lu", i, f.videoInfo.ColorPrimaries);
        CHECK(f.videoInfo.TransferMatrix == t[i].matrix, "type %d: matrix %lu", i, f.videoInfo.TransferMatrix);
        CHECK(f.videoInfo.TransferFunction == MFVideoTransFunc_709, "type %d: transfer %lu", i, f.videoInfo.TransferFunction);
        CHECK(f.videoInfo.NominalRange == MFNominalRange_16_235, "type %d: range %lu", i, f.videoInfo.NominalRange);
        CHECK(f.videoInfo.GeometricAperture.Area.cx == (LONG)t[i].w && f.videoInfo.GeometricAperture.Area.cy == (LONG)t[i].h,
                "type %d: aperture", i);
        CHECK(!memcmp(&f.videoInfo.MinimumDisplayAperture, &f.videoInfo.GeometricAperture, sizeof(MFVideoArea)), "type %d: min aperture", i);
        CHECK(IsEqualGUID(&f.guidFormat, t[i].guid), "type %d: subtype %08lx", i, f.guidFormat.Data1);
        CHECK(f.compressedInfo.AvgBitrate == 0 && f.surfaceInfo.Format == 0, "type %d: leftovers", i);
    }
}

static void test_uncompressed_format(void)
{
    MFVIDEOFORMAT f;
    GUID g;

    memset(&f, 0, sizeof(f));
    f.guidFormat = MFVideoFormat_RGB32;
    CHECK(pMFGetUncompressedVideoFormat(&f) == D3DFMT_X8R8G8B8, "rgb32 %lu", pMFGetUncompressedVideoFormat(&f));
    f.guidFormat = MFVideoFormat_NV12;
    CHECK(pMFGetUncompressedVideoFormat(&f) == MAKEFOURCC('N','V','1','2'), "nv12");
    f.guidFormat = MFVideoFormat_YUY2;
    CHECK(pMFGetUncompressedVideoFormat(&f) == MAKEFOURCC('Y','U','Y','2'), "yuy2");
    f.guidFormat = MFVideoFormat_ARGB32;
    CHECK(pMFGetUncompressedVideoFormat(&f) == D3DFMT_A8R8G8B8, "argb32");
    f.guidFormat = MFVideoFormat_H264;
    CHECK(pMFGetUncompressedVideoFormat(&f) == 0, "h264 is compressed");
    f.guidFormat = MFVideoFormat_MPEG2;
    CHECK(pMFGetUncompressedVideoFormat(&f) == 0, "mpeg2 is compressed");
    g = MFVideoFormat_Base;
    g.Data1 = MAKEFOURCC('A','B','C','D');
    f.guidFormat = g;
    CHECK(pMFGetUncompressedVideoFormat(&f) == 0, "unknown fourcc");
    f.guidFormat = GUID_NULL;
    CHECK(pMFGetUncompressedVideoFormat(&f) == 0, "null guid");
    CHECK(pMFGetUncompressedVideoFormat(NULL) == 0, "NULL format");
}

#define DXVA(sample, chroma, range, matrix, lighting, primaries, transfer) \
    ((sample) | (chroma) << 8 | (range) << 12 | (matrix) << 15 | (lighting) << 18 | (primaries) << 22 | (transfer) << 27)

static void test_color_info(void)
{
    MFVIDEOFORMAT f, g;
    DWORD dxva = 0;
    HRESULT hr;

    memset(&f, 0, sizeof(f));
    f.videoInfo.InterlaceMode = MFVideoInterlace_Progressive;
    f.videoInfo.SourceChromaSubsampling = MFVideoChromaSubsampling_MPEG2;
    f.videoInfo.NominalRange = MFNominalRange_16_235;
    f.videoInfo.TransferMatrix = MFVideoTransferMatrix_BT709;
    f.videoInfo.SourceLighting = MFVideoLighting_dim;
    f.videoInfo.ColorPrimaries = MFVideoPrimaries_SMPTE170M;
    f.videoInfo.TransferFunction = MFVideoTransFunc_709;
    hr = MFConvertColorInfoToDXVA(&dxva, &f);
    CHECK(hr == S_OK, "to dxva %#lx", hr);
    CHECK(dxva == DXVA(MFVideoInterlace_Progressive, MFVideoChromaSubsampling_MPEG2, MFNominalRange_16_235,
            MFVideoTransferMatrix_BT709, MFVideoLighting_dim, MFVideoPrimaries_SMPTE170M, MFVideoTransFunc_709), "dxva %#lx", dxva);

    memset(&g, 0, sizeof(g));
    hr = pMFConvertColorInfoFromDXVA(&g, dxva);
    CHECK(hr == S_OK, "from dxva %#lx", hr);
    CHECK(!memcmp(&g.videoInfo.InterlaceMode, &f.videoInfo.InterlaceMode, 0) && g.videoInfo.InterlaceMode == MFVideoInterlace_Progressive, "interlace %lu", g.videoInfo.InterlaceMode);
    CHECK(g.videoInfo.SourceChromaSubsampling == MFVideoChromaSubsampling_MPEG2, "chroma %lu", g.videoInfo.SourceChromaSubsampling);
    CHECK(g.videoInfo.NominalRange == MFNominalRange_16_235, "range %lu", g.videoInfo.NominalRange);
    CHECK(g.videoInfo.TransferMatrix == MFVideoTransferMatrix_BT709, "matrix %lu", g.videoInfo.TransferMatrix);
    CHECK(g.videoInfo.SourceLighting == MFVideoLighting_dim, "lighting %lu", g.videoInfo.SourceLighting);
    CHECK(g.videoInfo.ColorPrimaries == MFVideoPrimaries_SMPTE170M, "primaries %lu", g.videoInfo.ColorPrimaries);
    CHECK(g.videoInfo.TransferFunction == MFVideoTransFunc_709, "transfer %lu", g.videoInfo.TransferFunction);
    CHECK(g.dwSize == 0 && g.videoInfo.dwWidth == 0, "other fields untouched");

    hr = pMFConvertColorInfoFromDXVA(&g, DXVA(MFVideoInterlace_FieldInterleavedLowerFirst, 0, MFNominalRange_0_255,
            MFVideoTransferMatrix_BT601, MFVideoLighting_bright, MFVideoPrimaries_BT709, MFVideoTransFunc_sRGB));
    CHECK(hr == S_OK && g.videoInfo.InterlaceMode == MFVideoInterlace_FieldInterleavedLowerFirst
            && g.videoInfo.NominalRange == MFNominalRange_0_255 && g.videoInfo.TransferMatrix == MFVideoTransferMatrix_BT601
            && g.videoInfo.SourceLighting == MFVideoLighting_bright && g.videoInfo.ColorPrimaries == MFVideoPrimaries_BT709
            && g.videoInfo.TransferFunction == MFVideoTransFunc_sRGB && g.videoInfo.SourceChromaSubsampling == 0, "second value");

    hr = pMFConvertColorInfoFromDXVA(NULL, 0);
    CHECK(hr == E_INVALIDARG, "NULL: %#lx", hr);
}

static void test_validate(void)
{
    BYTE buf[512];
    VIDEOINFOHEADER *vih = (VIDEOINFOHEADER *)buf;
    VIDEOINFOHEADER2 *vih2 = (VIDEOINFOHEADER2 *)buf;
    WAVEFORMATEX *wfx = (WAVEFORMATEX *)buf;
    MFVIDEOFORMAT *mfv = (MFVIDEOFORMAT *)buf;
    HRESULT hr;

    memset(buf, 0, sizeof(buf));
    hr = pMFValidateMediaTypeSize(GUID_NULL, NULL, 0);
    CHECK(hr == S_OK, "no block: %#lx", hr);
    hr = pMFValidateMediaTypeSize(FORMAT_VideoInfo, NULL, 0);
    CHECK(hr == E_INVALIDARG, "NULL VideoInfo: %#lx", hr);

    vih->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    hr = pMFValidateMediaTypeSize(FORMAT_VideoInfo, buf, sizeof(VIDEOINFOHEADER));
    CHECK(hr == S_OK, "VideoInfo exact: %#lx", hr);
    hr = pMFValidateMediaTypeSize(FORMAT_VideoInfo, buf, sizeof(VIDEOINFOHEADER) - 1);
    CHECK(hr == MF_E_INVALIDMEDIATYPE, "VideoInfo short: %#lx", hr);
    hr = pMFValidateMediaTypeSize(FORMAT_VideoInfo, buf, 8);
    CHECK(hr == MF_E_INVALIDMEDIATYPE, "VideoInfo tiny: %#lx", hr);
    vih->bmiHeader.biSize = sizeof(BITMAPINFOHEADER) + 100;
    hr = pMFValidateMediaTypeSize(FORMAT_VideoInfo, buf, sizeof(VIDEOINFOHEADER));
    CHECK(hr == MF_E_INVALIDMEDIATYPE, "VideoInfo header bigger than block: %#lx", hr);
    hr = pMFValidateMediaTypeSize(FORMAT_VideoInfo, buf, sizeof(VIDEOINFOHEADER) + 100);
    CHECK(hr == S_OK, "VideoInfo with tail: %#lx", hr);
    vih->bmiHeader.biSize = 4;
    hr = pMFValidateMediaTypeSize(FORMAT_VideoInfo, buf, sizeof(buf));
    CHECK(hr == MF_E_INVALIDMEDIATYPE, "VideoInfo bad biSize: %#lx", hr);

    memset(buf, 0, sizeof(buf));
    vih2->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    hr = pMFValidateMediaTypeSize(FORMAT_VideoInfo2, buf, sizeof(VIDEOINFOHEADER2));
    CHECK(hr == S_OK, "VideoInfo2 exact: %#lx", hr);
    hr = pMFValidateMediaTypeSize(FORMAT_VideoInfo2, buf, sizeof(VIDEOINFOHEADER2) - 1);
    CHECK(hr == MF_E_INVALIDMEDIATYPE, "VideoInfo2 short: %#lx", hr);

    memset(buf, 0, sizeof(buf));
    wfx->wFormatTag = WAVE_FORMAT_PCM;
    hr = pMFValidateMediaTypeSize(FORMAT_WaveFormatEx, buf, sizeof(PCMWAVEFORMAT));
    CHECK(hr == S_OK, "PCM waveformat: %#lx", hr);
    hr = pMFValidateMediaTypeSize(FORMAT_WaveFormatEx, buf, sizeof(PCMWAVEFORMAT) - 1);
    CHECK(hr == MF_E_INVALIDMEDIATYPE, "PCM short: %#lx", hr);
    wfx->wFormatTag = WAVE_FORMAT_IEEE_FLOAT;
    wfx->cbSize = 22;
    hr = pMFValidateMediaTypeSize(FORMAT_WaveFormatEx, buf, sizeof(WAVEFORMATEX) + 22);
    CHECK(hr == S_OK, "float with extra: %#lx", hr);
    hr = pMFValidateMediaTypeSize(FORMAT_WaveFormatEx, buf, sizeof(WAVEFORMATEX) + 21);
    CHECK(hr == MF_E_INVALIDMEDIATYPE, "float cbSize beyond block: %#lx", hr);

    memset(buf, 0, sizeof(buf));
    mfv->dwSize = sizeof(MFVIDEOFORMAT);
    hr = pMFValidateMediaTypeSize(FORMAT_MFVideoFormat, buf, sizeof(MFVIDEOFORMAT));
    CHECK(hr == S_OK, "MFVideoFormat: %#lx", hr);
    hr = pMFValidateMediaTypeSize(FORMAT_MFVideoFormat, buf, sizeof(MFVIDEOFORMAT) - 4);
    CHECK(hr == MF_E_INVALIDMEDIATYPE, "MFVideoFormat short: %#lx", hr);
    mfv->dwSize = sizeof(MFVIDEOFORMAT) + 16;
    hr = pMFValidateMediaTypeSize(FORMAT_MFVideoFormat, buf, sizeof(MFVIDEOFORMAT));
    CHECK(hr == MF_E_INVALIDMEDIATYPE, "MFVideoFormat dwSize beyond: %#lx", hr);

    hr = pMFValidateMediaTypeSize(FORMAT_DvInfo, buf, 4);
    CHECK(hr == S_OK, "unknown format: %#lx", hr);
}

static void test_serialize(void)
{
    static const GUID key_u32 = {1}, key_u64 = {2}, key_dbl = {3}, key_guid = {4}, key_str = {5}, key_blob = {6},
            key_empty_str = {7}, key_empty_blob = {8}, key_unk = {9}, key_extra = {10};
    static const WCHAR str[] = L"hello \x00e9\x4e16 world";
    static const BYTE blob[] = {0, 1, 2, 0xff, 0, 0x55};
    IMFAttributes *src, *dst, *unk_obj;
    LARGE_INTEGER zero = {0};
    ULARGE_INTEGER pos;
    UINT32 u32, size, count;
    IStream *stream;
    STATSTG stat;
    PROPVARIANT pv;
    UINT64 u64;
    HRESULT hr;
    double dbl;
    WCHAR *wstr;
    BYTE *bptr;
    GUID guid;
    IUnknown *unk;
    BOOL equal;

    MFCreateAttributes(&src, 8);
    MFCreateAttributes(&unk_obj, 0);
    IMFAttributes_SetUINT32(src, &key_u32, 0xdeadbeef);
    IMFAttributes_SetUINT64(src, &key_u64, 0x1122334455667788ull);
    IMFAttributes_SetDouble(src, &key_dbl, -2.5e-7);
    IMFAttributes_SetGUID(src, &key_guid, &MFMediaType_Video);
    IMFAttributes_SetString(src, &key_str, str);
    IMFAttributes_SetBlob(src, &key_blob, blob, sizeof(blob));
    IMFAttributes_SetString(src, &key_empty_str, L"");
    IMFAttributes_SetBlob(src, &key_empty_blob, blob, 0);

    CreateStreamOnHGlobal(NULL, TRUE, &stream);

    /* invalid arguments */
    hr = pMFSerializeAttributesToStream(NULL, 0, stream);
    CHECK(hr == E_INVALIDARG, "NULL attributes: %#lx", hr);
    hr = pMFSerializeAttributesToStream(src, 0, NULL);
    CHECK(hr == E_INVALIDARG, "NULL stream: %#lx", hr);
    hr = pMFSerializeAttributesToStream(src, 2, stream);
    CHECK(hr == E_INVALIDARG, "bad flag: %#lx", hr);
    hr = pMFDeserializeAttributesFromStream(NULL, 0, stream);
    CHECK(hr == E_INVALIDARG, "deserialize NULL attributes: %#lx", hr);

    /* plain round trip */
    hr = pMFSerializeAttributesToStream(src, 0, stream);
    CHECK(hr == S_OK, "serialize: %#lx", hr);
    IStream_Stat(stream, &stat, STATFLAG_NONAME);
    CHECK(stat.cbSize.QuadPart > 100, "stream size %I64u", stat.cbSize.QuadPart);
    IStream_Seek(stream, zero, STREAM_SEEK_CUR, &pos);
    CHECK(pos.QuadPart == stat.cbSize.QuadPart, "position %I64u", pos.QuadPart);
    IStream_Seek(stream, zero, STREAM_SEEK_SET, NULL);

    MFCreateAttributes(&dst, 0);
    IMFAttributes_SetUINT32(dst, &key_extra, 1); /* must not survive */
    hr = pMFDeserializeAttributesFromStream(dst, 0, stream);
    CHECK(hr == S_OK, "deserialize: %#lx", hr);
    IMFAttributes_GetCount(dst, &count);
    CHECK(count == 8, "count %u", count);
    CHECK(IMFAttributes_GetItemType(dst, &key_extra, (MF_ATTRIBUTE_TYPE *)&u32) == MF_E_ATTRIBUTENOTFOUND, "extra item survived");
    hr = IMFAttributes_GetUINT32(dst, &key_u32, &u32);
    CHECK(hr == S_OK && u32 == 0xdeadbeef, "u32 %#lx %#x", hr, u32);
    hr = IMFAttributes_GetUINT64(dst, &key_u64, &u64);
    CHECK(hr == S_OK && u64 == 0x1122334455667788ull, "u64 %#lx", hr);
    hr = IMFAttributes_GetDouble(dst, &key_dbl, &dbl);
    CHECK(hr == S_OK && dbl == -2.5e-7, "double %#lx %g", hr, dbl);
    hr = IMFAttributes_GetGUID(dst, &key_guid, &guid);
    CHECK(hr == S_OK && IsEqualGUID(&guid, &MFMediaType_Video), "guid %#lx", hr);
    hr = IMFAttributes_GetAllocatedString(dst, &key_str, &wstr, &size);
    CHECK(hr == S_OK && size == lstrlenW(str) && !lstrcmpW(wstr, str), "string %#lx %u", hr, size);
    CoTaskMemFree(wstr);
    hr = IMFAttributes_GetAllocatedString(dst, &key_empty_str, &wstr, &size);
    CHECK(hr == S_OK && size == 0 && !wstr[0], "empty string %#lx %u", hr, size);
    CoTaskMemFree(wstr);
    hr = IMFAttributes_GetAllocatedBlob(dst, &key_blob, &bptr, &size);
    CHECK(hr == S_OK && size == sizeof(blob) && !memcmp(bptr, blob, sizeof(blob)), "blob %#lx %u", hr, size);
    CoTaskMemFree(bptr);
    hr = IMFAttributes_GetBlobSize(dst, &key_empty_blob, &size);
    CHECK(hr == S_OK && size == 0, "empty blob %#lx %u", hr, size);
    equal = FALSE;
    hr = IMFAttributes_Compare(src, dst, MF_ATTRIBUTES_MATCH_ALL_ITEMS, &equal);
    CHECK(hr == S_OK && equal, "Compare ALL: %#lx %d", hr, equal);

    /* IUnknown items need the by-reference flag */
    IMFAttributes_SetUnknown(src, &key_unk, (IUnknown *)unk_obj);
    IStream_Release(stream);
    CreateStreamOnHGlobal(NULL, TRUE, &stream);
    hr = pMFSerializeAttributesToStream(src, 0, stream);
    CHECK(hr == MF_E_INVALIDTYPE, "unknown without flag: %#lx", hr);
    IStream_Stat(stream, &stat, STATFLAG_NONAME);
    CHECK(stat.cbSize.QuadPart == 0, "stream written on failure: %I64u", stat.cbSize.QuadPart);

    hr = pMFSerializeAttributesToStream(src, MF_ATTRIBUTE_SERIALIZE_UNKNOWN_BYREF, stream);
    CHECK(hr == S_OK, "unknown byref: %#lx", hr);
    IStream_Seek(stream, zero, STREAM_SEEK_SET, NULL);
    IMFAttributes_DeleteAllItems(dst);
    hr = pMFDeserializeAttributesFromStream(dst, 0, stream);
    CHECK(hr == MF_E_INVALIDTYPE, "deserialize unknown without flag: %#lx", hr);
    IMFAttributes_GetCount(dst, &count);
    CHECK(count == 0, "destination changed on failure: %u", count);
    IStream_Seek(stream, zero, STREAM_SEEK_SET, NULL);
    hr = pMFDeserializeAttributesFromStream(dst, MF_ATTRIBUTE_SERIALIZE_UNKNOWN_BYREF, stream);
    CHECK(hr == S_OK, "deserialize byref: %#lx", hr);
    unk = NULL;
    hr = IMFAttributes_GetUnknown(dst, &key_unk, &IID_IUnknown, (void **)&unk);
    CHECK(hr == S_OK && unk == (IUnknown *)unk_obj, "same object back: %#lx %p vs %p", hr, unk, unk_obj);
    if (unk) IUnknown_Release(unk);
    IMFAttributes_GetCount(dst, &count);
    CHECK(count == 9, "byref count %u", count);
    hr = IMFAttributes_GetUINT32(dst, &key_u32, &u32);
    CHECK(hr == S_OK && u32 == 0xdeadbeef, "byref u32 %#lx", hr);

    /* empty store */
    IStream_Release(stream);
    CreateStreamOnHGlobal(NULL, TRUE, &stream);
    IMFAttributes_DeleteAllItems(src);
    hr = pMFSerializeAttributesToStream(src, 0, stream);
    CHECK(hr == S_OK, "empty serialize: %#lx", hr);
    IStream_Seek(stream, zero, STREAM_SEEK_SET, NULL);
    IMFAttributes_SetUINT32(dst, &key_extra, 3);
    hr = pMFDeserializeAttributesFromStream(dst, 0, stream);
    IMFAttributes_GetCount(dst, &count);
    CHECK(hr == S_OK && count == 0, "empty round trip %#lx %u", hr, count);

    /* corrupt streams leave the destination alone */
    IMFAttributes_SetUINT32(dst, &key_extra, 3);
    IStream_Release(stream);
    CreateStreamOnHGlobal(NULL, TRUE, &stream);
    hr = pMFDeserializeAttributesFromStream(dst, 0, stream);
    CHECK(hr == MF_E_INVALID_FORMAT, "empty stream: %#lx", hr);
    IMFAttributes_SetString(src, &key_str, str);
    IMFAttributes_SetBlob(src, &key_blob, blob, sizeof(blob));
    hr = pMFSerializeAttributesToStream(src, 0, stream);
    IStream_Stat(stream, &stat, STATFLAG_NONAME);
    {
        ULARGE_INTEGER newsize;
        newsize.QuadPart = stat.cbSize.QuadPart - 3;
        IStream_SetSize(stream, newsize);
    }
    IStream_Seek(stream, zero, STREAM_SEEK_SET, NULL);
    hr = pMFDeserializeAttributesFromStream(dst, 0, stream);
    CHECK(hr == MF_E_INVALID_FORMAT, "truncated stream: %#lx", hr);
    hr = IMFAttributes_GetUINT32(dst, &key_extra, &u32);
    CHECK(hr == S_OK && u32 == 3, "destination kept on failure: %#lx", hr);
    {
        BYTE bad[16] = {'x', 'x', 'x', 'x', 0, 0, 0, 0};
        IStream_Release(stream);
        CreateStreamOnHGlobal(NULL, TRUE, &stream);
        IStream_Write(stream, bad, sizeof(bad), NULL);
        IStream_Seek(stream, zero, STREAM_SEEK_SET, NULL);
        hr = pMFDeserializeAttributesFromStream(dst, 0, stream);
        CHECK(hr == MF_E_INVALID_FORMAT, "bad magic: %#lx", hr);
    }

    PropVariantInit(&pv);
    IStream_Release(stream);
    IMFAttributes_Release(dst);
    IMFAttributes_Release(src);
    IMFAttributes_Release(unk_obj);
}

int main(void)
{
    HMODULE mfplat;

    CoInitialize(NULL);
    MFStartup(MF_VERSION, MFSTARTUP_FULL);
    mfplat = GetModuleHandleA("mfplat.dll");
#define X(f) p##f = (void *)GetProcAddress(mfplat, #f); if (!p##f) { printf("FAIL no %s\n", #f); return 1; }
    X(MFInitVideoFormat) X(MFGetUncompressedVideoFormat) X(MFConvertColorInfoFromDXVA) X(MFValidateMediaTypeSize)
    X(MFSerializeAttributesToStream) X(MFDeserializeAttributesFromStream)
#undef X
    test_init_video_format();
    test_uncompressed_format();
    test_color_info();
    test_validate();
    test_serialize();
    MFShutdown();
    printf(failures ? "RESULT: FAIL (%d)\n" : "RESULT: PASS\n", failures);
    return failures != 0;
}
