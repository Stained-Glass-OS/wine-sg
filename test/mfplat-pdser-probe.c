/* MFSerializePresentationDescriptor / MFDeserializePresentationDescriptor (patches/sg/2968), run by
 * test/mfplat-pdser-gate.sh. */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
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

DEFINE_GUID(test_key, 0x9c0ffee0, 0x1, 0x2, 1,2,3,4,5,6,7,8);
DEFINE_GUID(test_key2, 0x9c0ffee0, 0x2, 0x2, 1,2,3,4,5,6,7,8);
HRESULT WINAPI MFDeserializePresentationDescriptor(DWORD, BYTE *, IMFPresentationDescriptor **);

static IMFMediaType *make_audio(UINT32 rate)
{
    IMFMediaType *t;
    MFCreateMediaType(&t);
    IMFMediaType_SetGUID(t, &MF_MT_MAJOR_TYPE, &MFMediaType_Audio);
    IMFMediaType_SetGUID(t, &MF_MT_SUBTYPE, &MFAudioFormat_PCM);
    IMFMediaType_SetUINT32(t, &MF_MT_AUDIO_SAMPLES_PER_SECOND, rate);
    IMFMediaType_SetUINT32(t, &MF_MT_AUDIO_NUM_CHANNELS, 2);
    return t;
}

static IMFMediaType *make_video(UINT32 w, UINT32 h)
{
    IMFMediaType *t;
    MFCreateMediaType(&t);
    IMFMediaType_SetGUID(t, &MF_MT_MAJOR_TYPE, &MFMediaType_Video);
    IMFMediaType_SetGUID(t, &MF_MT_SUBTYPE, &MFVideoFormat_RGB32);
    IMFMediaType_SetUINT64(t, &MF_MT_FRAME_SIZE, ((UINT64)w << 32) | h);
    return t;
}

static void check_stream(IMFPresentationDescriptor *pd, DWORD index, BOOL exp_selected, DWORD exp_id,
        const GUID *exp_major, DWORD exp_count, int exp_current, const char *name)
{
    IMFMediaTypeHandler *handler;
    IMFStreamDescriptor *sd;
    IMFMediaType *type;
    BOOL selected;
    DWORD id, count;
    GUID major;
    UINT32 value;
    HRESULT hr;

    hr = IMFPresentationDescriptor_GetStreamDescriptorByIndex(pd, index, &selected, &sd);
    CHECKF(hr == S_OK, "%s: get stream %lu hr %#lx", name, index, hr);
    if (hr != S_OK) return;
    CHECKF(selected == exp_selected, "%s: stream %lu selected %d (expected %d)", name, index, selected, exp_selected);
    IMFStreamDescriptor_GetStreamIdentifier(sd, &id);
    CHECKF(id == exp_id, "%s: stream %lu id %lu (expected %lu)", name, index, id, exp_id);
    IMFStreamDescriptor_GetMediaTypeHandler(sd, &handler);
    IMFMediaTypeHandler_GetMediaTypeCount(handler, &count);
    CHECKF(count == exp_count, "%s: stream %lu type count %lu", name, index, count);
    hr = IMFMediaTypeHandler_GetMajorType(handler, &major);
    CHECKF(hr == S_OK && IsEqualGUID(&major, exp_major), "%s: stream %lu major type (hr %#lx)", name, index, hr);
    type = NULL;
    hr = IMFMediaTypeHandler_GetCurrentMediaType(handler, &type);
    if (exp_current < 0)
        CHECKF(hr == MF_E_NOT_INITIALIZED, "%s: stream %lu no current type hr %#lx", name, index, hr);
    else
    {
        CHECKF(hr == S_OK, "%s: stream %lu current type hr %#lx", name, index, hr);
        if (hr == S_OK)
        {
            if (IsEqualGUID(exp_major, &MFMediaType_Audio))
                hr = IMFMediaType_GetUINT32(type, &MF_MT_AUDIO_SAMPLES_PER_SECOND, &value);
            else
            {
                UINT64 size;
                hr = IMFMediaType_GetUINT64(type, &MF_MT_FRAME_SIZE, &size);
                value = size & 0xffffffff;
            }
            CHECKF(hr == S_OK && value == (UINT32)exp_current, "%s: stream %lu current value %u (expected %d)", name, index, value, exp_current);
            IMFMediaType_Release(type);
        }
    }
    IMFMediaTypeHandler_Release(handler);
    IMFStreamDescriptor_Release(sd);
}

static void roundtrip(void)
{
    IMFMediaType *types_a[2], *types_v[1];
    IMFStreamDescriptor *sds[3];
    IMFPresentationDescriptor *pd, *pd2;
    IMFMediaTypeHandler *handler;
    DWORD size, i;
    BYTE *data;
    UINT64 u64;
    WCHAR *str;
    UINT32 len;
    HRESULT hr;

    types_a[0] = make_audio(44100);
    types_a[1] = make_audio(48000);
    types_v[0] = make_video(320, 240);
    MFCreateStreamDescriptor(3, 2, types_a, &sds[0]);
    MFCreateStreamDescriptor(9, 1, types_v, &sds[1]);
    MFCreateStreamDescriptor(5, 1, types_v, &sds[2]);
    IMFStreamDescriptor_SetUINT64(sds[0], &test_key, 0x123456789abcdef0ull);
    IMFStreamDescriptor_GetMediaTypeHandler(sds[0], &handler);
    IMFMediaTypeHandler_SetCurrentMediaType(handler, types_a[1]);
    IMFMediaTypeHandler_Release(handler);
    IMFStreamDescriptor_GetMediaTypeHandler(sds[1], &handler);
    IMFMediaTypeHandler_SetCurrentMediaType(handler, types_v[0]);
    IMFMediaTypeHandler_Release(handler);

    MFCreatePresentationDescriptor(3, sds, &pd);
    IMFPresentationDescriptor_SelectStream(pd, 0);
    IMFPresentationDescriptor_SelectStream(pd, 2);
    IMFPresentationDescriptor_SetUINT64(pd, &MF_PD_DURATION, 7654321);
    IMFPresentationDescriptor_SetString(pd, &test_key2, L"video/x-sg");

    size = 0; data = NULL;
    hr = MFSerializePresentationDescriptor(pd, &size, &data);
    CHECKF(hr == S_OK && size && data, "serialize hr %#lx size %lu", hr, size);
    if (hr != S_OK) return;

    pd2 = NULL;
    hr = MFDeserializePresentationDescriptor(size, data, &pd2);
    CHECKF(hr == S_OK && pd2, "deserialize hr %#lx", hr);
    if (hr == S_OK)
    {
        DWORD count;
        IMFPresentationDescriptor_GetStreamDescriptorCount(pd2, &count);
        CHECKF(count == 3, "stream count %lu", count);
        check_stream(pd2, 0, TRUE, 3, &MFMediaType_Audio, 2, 48000, "rt");
        check_stream(pd2, 1, FALSE, 9, &MFMediaType_Video, 1, 240, "rt");
        check_stream(pd2, 2, TRUE, 5, &MFMediaType_Video, 1, -1, "rt");
        u64 = 0;
        hr = IMFPresentationDescriptor_GetUINT64(pd2, &MF_PD_DURATION, &u64);
        CHECKF(hr == S_OK && u64 == 7654321, "duration attribute hr %#lx", hr);
        hr = IMFPresentationDescriptor_GetAllocatedString(pd2, &test_key2, &str, &len);
        CHECKF(hr == S_OK && !wcscmp(str, L"video/x-sg") && len == 10, "mime attribute hr %#lx", hr);
        if (hr == S_OK) CoTaskMemFree(str);
        {
            IMFStreamDescriptor *sd;
            BOOL sel;
            IMFPresentationDescriptor_GetStreamDescriptorByIndex(pd2, 0, &sel, &sd);
            u64 = 0;
            hr = IMFStreamDescriptor_GetUINT64(sd, &test_key, &u64);
            CHECKF(hr == S_OK && u64 == 0x123456789abcdef0ull, "stream attribute hr %#lx", hr);
            IMFStreamDescriptor_Release(sd);
        }
        IMFPresentationDescriptor_Release(pd2);
    }

    /* truncated and corrupt data */
    for (i = 0; i < 4; i++)
    {
        static const DWORD cuts[4] = {0, 3, 8, 40};
        DWORD n = cuts[i];
        pd2 = (void *)1;
        hr = MFDeserializePresentationDescriptor(n, data, &pd2);
        CHECKF(FAILED(hr) && !pd2, "deserialize truncated (%lu bytes) hr %#lx", n, hr);
    }
    data[0] ^= 0xff;
    hr = MFDeserializePresentationDescriptor(size, data, &pd2);
    CHECKF(hr == MF_E_INVALID_FORMAT, "deserialize bad magic hr %#lx", hr);

    CoTaskMemFree(data);
    hr = MFSerializePresentationDescriptor(NULL, &size, &data);
    CHECKF(hr == E_INVALIDARG, "serialize NULL pd hr %#lx", hr);
    hr = MFDeserializePresentationDescriptor(8, NULL, &pd2);
    CHECKF(hr == E_INVALIDARG, "deserialize NULL data hr %#lx", hr);

    IMFPresentationDescriptor_Release(pd);
    for (i = 0; i < 3; i++) IMFStreamDescriptor_Release(sds[i]);
    IMFMediaType_Release(types_a[0]); IMFMediaType_Release(types_a[1]); IMFMediaType_Release(types_v[0]);
}

int main(void)
{
    CoInitialize(NULL);
    MFStartup(MF_VERSION, MFSTARTUP_FULL);
    roundtrip();
    MFShutdown();
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
