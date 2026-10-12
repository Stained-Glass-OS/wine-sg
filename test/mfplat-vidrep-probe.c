/* IMFVideoMediaType::GetVideoRepresentation and the AM_MEDIA_TYPE interlace mapping (patches/sg/2969),
 * run by test/mfplat-vidrep-gate.sh. */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <dshow.h>
#include <dvdmedia.h>
#include <mfapi.h>
#include <mferror.h>
#include <mfidl.h>
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

DEFINE_GUID(sg_AM_REP, 0xe2e42ad2, 0x132c, 0x491e, 0xa2, 0x68, 0x3c, 0x7c, 0x2d, 0xca, 0x18, 0x1f);

static void test_representation(void)
{
    IMFVideoMediaType *vtype;
    IMFMediaType *type;
    AM_MEDIA_TYPE *am;
    VIDEOINFOHEADER *vih;
    GUID other = {1,2,3,{4,5,6,7,8,9,10,11}};
    void *data;
    HRESULT hr;

    MFCreateMediaType(&type);
    IMFMediaType_SetGUID(type, &MF_MT_MAJOR_TYPE, &MFMediaType_Video);
    IMFMediaType_SetGUID(type, &MF_MT_SUBTYPE, &MFVideoFormat_RGB32);
    IMFMediaType_SetUINT64(type, &MF_MT_FRAME_SIZE, ((UINT64)64 << 32) | 32);
    hr = IMFMediaType_QueryInterface(type, &IID_IMFVideoMediaType, (void **)&vtype);
    if (hr != S_OK)
    {
        IMFMediaType_Release(type);
        MFCreateVideoMediaTypeFromSubtype(&MFVideoFormat_RGB32, &vtype);
        hr = vtype ? S_OK : E_FAIL;
        CHECKF(hr == S_OK, "video media type object");
        if (hr != S_OK) return;
        type = (IMFMediaType *)vtype;
        IMFMediaType_SetUINT64(type, &MF_MT_FRAME_SIZE, ((UINT64)64 << 32) | 32);
    }
    else
        IMFVideoMediaType_Release(vtype);
    hr = IMFMediaType_QueryInterface(type, &IID_IMFVideoMediaType, (void **)&vtype);
    CHECKF(hr == S_OK, "QI IMFVideoMediaType hr %#lx", hr);
    if (hr != S_OK) return;

    data = (void *)1;
    hr = IMFVideoMediaType_GetVideoRepresentation(vtype, other, &data, 0);
    CHECKF(hr == MF_E_UNSUPPORTED_REPRESENTATION && !data, "unknown representation hr %#lx", hr);
    hr = IMFVideoMediaType_GetVideoRepresentation(vtype, sg_AM_REP, NULL, 0);
    CHECKF(hr == E_POINTER, "NULL out hr %#lx", hr);

    am = NULL;
    hr = IMFVideoMediaType_GetVideoRepresentation(vtype, sg_AM_REP, (void **)&am, 0);
    CHECKF(hr == S_OK && am, "AM_MEDIA_TYPE representation hr %#lx", hr);
    if (hr == S_OK && am)
    {
        CHECKF(IsEqualGUID(&am->majortype, &MEDIATYPE_Video), "AM major type");
        CHECKF(IsEqualGUID(&am->subtype, &MEDIASUBTYPE_RGB32), "AM subtype");
        CHECKF(IsEqualGUID(&am->formattype, &FORMAT_VideoInfo) && am->cbFormat >= sizeof(VIDEOINFOHEADER),
                "AM format type %lu bytes", am->cbFormat);
        if (am->cbFormat >= sizeof(VIDEOINFOHEADER))
        {
            vih = (VIDEOINFOHEADER *)am->pbFormat;
            CHECKF(vih->bmiHeader.biWidth == 64 && abs(vih->bmiHeader.biHeight) == 32, "AM frame size %ldx%ld",
                    vih->bmiHeader.biWidth, vih->bmiHeader.biHeight);
        }
        CoTaskMemFree(am->pbFormat);
        CoTaskMemFree(am);
    }
    IMFVideoMediaType_Release(vtype);
}

static const struct { UINT32 mf; DWORD flags; } interlace_table[] = {
    { MFVideoInterlace_Progressive, 0 },
    { MFVideoInterlace_MixedInterlaceOrProgressive, AMINTERLACE_IsInterlaced | AMINTERLACE_DisplayModeBobOrWeave },
    { MFVideoInterlace_FieldInterleavedUpperFirst, AMINTERLACE_IsInterlaced | AMINTERLACE_Field1First },
    { MFVideoInterlace_FieldInterleavedLowerFirst, AMINTERLACE_IsInterlaced },
};

static void test_interlace(void)
{
    unsigned int i;

    for (i = 0; i < ARRAYSIZE(interlace_table); i++)
    {
        IMFMediaType *type, *back;
        AM_MEDIA_TYPE *am;
        VIDEOINFOHEADER2 vih2;
        UINT32 value;
        HRESULT hr;

        /* media type -> VIDEOINFOHEADER2 */
        MFCreateMediaType(&type);
        IMFMediaType_SetGUID(type, &MF_MT_MAJOR_TYPE, &MFMediaType_Video);
        IMFMediaType_SetGUID(type, &MF_MT_SUBTYPE, &MFVideoFormat_RGB32);
        IMFMediaType_SetUINT64(type, &MF_MT_FRAME_SIZE, ((UINT64)64 << 32) | 32);
        IMFMediaType_SetUINT32(type, &MF_MT_INTERLACE_MODE, interlace_table[i].mf);
        am = NULL;
        hr = MFCreateAMMediaTypeFromMFMediaType(type, FORMAT_VideoInfo2, &am);
        CHECKF(hr == S_OK && am, "interlace %u: AM type hr %#lx", interlace_table[i].mf, hr);
        if (hr == S_OK && am)
        {
            VIDEOINFOHEADER2 *p = (VIDEOINFOHEADER2 *)am->pbFormat;
            CHECKF(am->cbFormat >= sizeof(*p) && p->dwInterlaceFlags == interlace_table[i].flags,
                    "interlace %u: flags %#lx (expected %#lx)", interlace_table[i].mf,
                    am->cbFormat >= sizeof(*p) ? p->dwInterlaceFlags : 0, interlace_table[i].flags);
            vih2 = *p;
            CoTaskMemFree(am->pbFormat);
            CoTaskMemFree(am);

            /* VIDEOINFOHEADER2 -> media type */
            MFCreateMediaType(&back);
            hr = MFInitMediaTypeFromVideoInfoHeader2(back, &vih2, sizeof(vih2), &MFVideoFormat_RGB32);
            value = 0xdead;
            IMFMediaType_GetUINT32(back, &MF_MT_INTERLACE_MODE, &value);
            CHECKF(hr == S_OK && value == interlace_table[i].mf, "interlace %u: back to MF mode %u hr %#lx",
                    interlace_table[i].mf, value, hr);
            IMFMediaType_Release(back);
        }
        IMFMediaType_Release(type);
    }
}

int main(void)
{
    CoInitialize(NULL);
    MFStartup(MF_VERSION, MFSTARTUP_FULL);
    test_representation();
    test_interlace();
    MFShutdown();
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
