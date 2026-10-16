/* Video capture sources (patch 3299): MFEnumDeviceSources / MFCreateDeviceSource /
 * MFCreateDeviceSourceActivate and the camera's IMFMediaSource, against the fake
 * cameras of the WINE_SG_FAKE_CAMERAS test hook (a live videotestsrc; no /dev/video*
 * node is touched). Run by test/fastvidcap-gate.sh. */
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
#define CHECKF(ok, ...) do { char _b[300]; snprintf(_b, sizeof(_b), __VA_ARGS__); check(ok, _b); } while (0)

typedef HRESULT (WINAPI *create_source_t)(IMFAttributes *, IMFMediaSource **);
typedef HRESULT (WINAPI *create_activate_t)(IMFAttributes *, IMFActivate **);

DEFINE_GUID(cat_camera, 0xe5323777, 0xf976, 0x4f5b, 0x9b, 0x55, 0xb9, 0x46, 0x99, 0xc4, 0x6e, 0x44);

static HRESULT wait_event(IMFMediaEventGenerator *gen, MediaEventType want, IMFMediaEvent **out, DWORD ms)
{
    DWORD start = GetTickCount();

    for (;;)
    {
        IMFMediaEvent *event;
        MediaEventType type;
        HRESULT hr = IMFMediaEventGenerator_GetEvent(gen, MF_EVENT_FLAG_NO_WAIT, &event);

        if (hr == S_OK)
        {
            IMFMediaEvent_GetType(event, &type);
            if (type == want) { if (out) *out = event; else IMFMediaEvent_Release(event); return S_OK; }
            IMFMediaEvent_Release(event);
            continue;
        }
        if (hr != MF_E_NO_EVENTS_AVAILABLE) return hr;
        if (GetTickCount() - start > ms) return HRESULT_FROM_WIN32(ERROR_TIMEOUT);
        Sleep(5);
    }
}

static int thread_count(void)
{
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW(L"Z:\\proc\\self\\task\\*", &fd);
    int n = 0;

    if (h == INVALID_HANDLE_VALUE) return -1;
    do { if (fd.cFileName[0] != '.') n++; } while (FindNextFileW(h, &fd));
    FindClose(h);
    return n;
}

static HRESULT get_sample(IMFMediaSource *source, IMFMediaStream *stream, IMFSample **sample, DWORD ms)
{
    IMFMediaEvent *event;
    PROPVARIANT v;
    HRESULT hr;

    if (FAILED(hr = IMFMediaStream_RequestSample(stream, NULL))) return hr;
    if (FAILED(hr = wait_event((IMFMediaEventGenerator *)stream, MEMediaSample, &event, ms))) return hr;
    PropVariantInit(&v);
    IMFMediaEvent_GetValue(event, &v);
    hr = v.vt == VT_UNKNOWN && v.punkVal ? IUnknown_QueryInterface(v.punkVal, &IID_IMFSample, (void **)sample) : E_FAIL;
    PropVariantClear(&v);
    IMFMediaEvent_Release(event);
    return hr;
}

static void check_types(IMFMediaTypeHandler *handler, int expected_count, UINT w0, UINT h0, const char *tag)
{
    DWORD count = 0, i;
    int nv12 = 0, yuy2 = 0, first_ok = 0;

    IMFMediaTypeHandler_GetMediaTypeCount(handler, &count);
    CHECKF((int)count == expected_count, "%s: %d native types (%lu)", tag, expected_count, count);
    for (i = 0; i < count; ++i)
    {
        IMFMediaType *type;
        GUID sub;
        UINT64 size, rate;
        UINT32 stride, ssize;

        if (FAILED(IMFMediaTypeHandler_GetMediaTypeByIndex(handler, i, &type))) continue;
        IMFMediaType_GetGUID(type, &MF_MT_SUBTYPE, &sub);
        IMFMediaType_GetUINT64(type, &MF_MT_FRAME_SIZE, &size);
        IMFMediaType_GetUINT64(type, &MF_MT_FRAME_RATE, &rate);
        stride = ssize = 0;
        IMFMediaType_GetUINT32(type, &MF_MT_DEFAULT_STRIDE, &stride);
        IMFMediaType_GetUINT32(type, &MF_MT_SAMPLE_SIZE, &ssize);
        if (IsEqualGUID(&sub, &MFVideoFormat_NV12))
        {
            nv12++;
            if (ssize != (size >> 32) * (UINT32)size * 3 / 2 || stride != (size >> 32)) nv12 = 1000;
        }
        else if (IsEqualGUID(&sub, &MFVideoFormat_YUY2))
        {
            yuy2++;
            if (ssize != (size >> 32) * (UINT32)size * 2 || stride != (size >> 32) * 2) yuy2 = 1000;
        }
        if (i == 0 && (size >> 32) == w0 && (UINT32)size == h0 && (rate >> 32) > 0) first_ok = 1;
        IMFMediaType_Release(type);
    }
    CHECKF(nv12 * 2 == expected_count && yuy2 * 2 == expected_count, "%s: half NV12, half YUY2, sizes/strides consistent (%d/%d)", tag, nv12, yuy2);
    CHECKF(first_ok, "%s: first type is %ux%u", tag, w0, h0);
}

static HRESULT set_type(IMFPresentationDescriptor *pd, REFGUID sub, UINT w, UINT h, UINT fn)
{
    IMFStreamDescriptor *sd;
    IMFMediaTypeHandler *handler;
    BOOL sel;
    DWORD count, i;
    HRESULT hr = E_FAIL;

    IMFPresentationDescriptor_GetStreamDescriptorByIndex(pd, 0, &sel, &sd);
    IMFStreamDescriptor_GetMediaTypeHandler(sd, &handler);
    IMFMediaTypeHandler_GetMediaTypeCount(handler, &count);
    for (i = 0; i < count; ++i)
    {
        IMFMediaType *type;
        GUID g;
        UINT64 size, rate;

        IMFMediaTypeHandler_GetMediaTypeByIndex(handler, i, &type);
        IMFMediaType_GetGUID(type, &MF_MT_SUBTYPE, &g);
        IMFMediaType_GetUINT64(type, &MF_MT_FRAME_SIZE, &size);
        IMFMediaType_GetUINT64(type, &MF_MT_FRAME_RATE, &rate);
        if (IsEqualGUID(&g, sub) && size == (((UINT64)w << 32) | h) && (rate >> 32) == fn)
            hr = IMFMediaTypeHandler_SetCurrentMediaType(handler, type);
        IMFMediaType_Release(type);
    }
    IMFMediaTypeHandler_Release(handler);
    IMFStreamDescriptor_Release(sd);
    return hr;
}

static void test_source(IMFActivate *activate, const char *tag, UINT first_w, UINT first_h, UINT def_w, UINT def_h, int ntypes, int jpeg)
{
    IMFMediaSource *source;
    IMFMediaSourceEx *ex;
    IMFPresentationDescriptor *pd;
    IMFStreamDescriptor *sd;
    IMFMediaTypeHandler *handler;
    IMFMediaStream *stream = NULL;
    IMFMediaEvent *event;
    IMFSample *sample;
    PROPVARIANT pos;
    DWORD chars, count;
    BOOL sel;
    HRESULT hr;
    char buf[200];

    hr = IMFActivate_ActivateObject(activate, &IID_IMFMediaSource, (void **)&source);
    CHECKF(hr == S_OK, "%s: ActivateObject (%#lx)", tag, hr);
    if (FAILED(hr)) return;

    hr = IMFMediaSource_QueryInterface(source, &IID_IMFMediaSourceEx, (void **)&ex);
    CHECKF(hr == S_OK, "%s: IMFMediaSourceEx", tag);
    if (ex) IMFMediaSourceEx_Release(ex);
    hr = IMFMediaSource_GetCharacteristics(source, &chars);
    CHECKF(hr == S_OK && (chars & MFMEDIASOURCE_IS_LIVE) && !(chars & (MFMEDIASOURCE_CAN_PAUSE | MFMEDIASOURCE_CAN_SEEK)),
            "%s: live, no pause or seek (%#lx %#lx)", tag, hr, chars);

    hr = IMFMediaSource_CreatePresentationDescriptor(source, &pd);
    CHECKF(hr == S_OK, "%s: presentation descriptor", tag);
    IMFPresentationDescriptor_GetStreamDescriptorCount(pd, &count);
    CHECKF(count == 1, "%s: one stream (%lu)", tag, count);
    IMFPresentationDescriptor_GetStreamDescriptorByIndex(pd, 0, &sel, &sd);
    CHECKF(sel, "%s: stream selected", tag);
    IMFStreamDescriptor_GetMediaTypeHandler(sd, &handler);
    check_types(handler, ntypes, first_w, first_h, tag);
    {
        IMFMediaType *cur = NULL;
        UINT64 size = 0;
        GUID sub = {0};

        hr = IMFMediaTypeHandler_GetCurrentMediaType(handler, &cur);
        if (cur)
        {
            IMFMediaType_GetUINT64(cur, &MF_MT_FRAME_SIZE, &size);
            IMFMediaType_GetGUID(cur, &MF_MT_SUBTYPE, &sub);
            IMFMediaType_Release(cur);
        }
        CHECKF(hr == S_OK && size == (((UINT64)def_w << 32) | def_h) && IsEqualGUID(&sub, &MFVideoFormat_YUY2),
                "%s: default current type is YUY2 %ux%u (%#lx %llx)", tag, def_w, def_h, hr, (unsigned long long)size);
    }
    IMFMediaTypeHandler_Release(handler);
    IMFStreamDescriptor_Release(sd);

    /* pause is not possible for a live camera */
    hr = IMFMediaSource_Pause(source);
    CHECKF(hr == MF_E_INVALID_STATE_TRANSITION, "%s: Pause before start (%#lx)", tag, hr);
    hr = IMFMediaSource_Stop(source);
    CHECKF(hr == MF_E_INVALID_STATE_TRANSITION, "%s: Stop before start (%#lx)", tag, hr);

    PropVariantInit(&pos);
    hr = IMFMediaSource_Start(source, pd, &GUID_NULL, &pos);
    CHECKF(hr == S_OK, "%s: Start (%#lx)", tag, hr);
    hr = wait_event((IMFMediaEventGenerator *)source, MENewStream, &event, 5000);
    CHECKF(hr == S_OK, "%s: MENewStream", tag);
    if (hr == S_OK)
    {
        PROPVARIANT v;
        PropVariantInit(&v);
        IMFMediaEvent_GetValue(event, &v);
        if (v.vt == VT_UNKNOWN) IUnknown_QueryInterface(v.punkVal, &IID_IMFMediaStream, (void **)&stream);
        PropVariantClear(&v);
        IMFMediaEvent_Release(event);
    }
    hr = wait_event((IMFMediaEventGenerator *)source, MESourceStarted, NULL, 5000);
    CHECKF(hr == S_OK, "%s: MESourceStarted", tag);
    CHECKF(stream != NULL, "%s: the new stream", tag);
    if (!stream) goto out;
    hr = wait_event((IMFMediaEventGenerator *)stream, MEStreamStarted, NULL, 5000);
    CHECKF(hr == S_OK, "%s: MEStreamStarted", tag);

    /* nobody asked for a frame: none is sent (a live camera does not queue them) */
    Sleep(400);
    hr = wait_event((IMFMediaEventGenerator *)stream, MEMediaSample, NULL, 0);
    CHECKF(hr != S_OK, "%s: no sample without a request", tag);

    {
        LONGLONG t0 = 0, t1 = 0, d = 0;
        DWORD len = 0;
        int i, nonzero = 0, got = 0;
        BYTE *data;
        IMFMediaBuffer *mb;

        for (i = 0; i < 10; ++i)
        {
            hr = get_sample(source, stream, &sample, 5000);
            if (FAILED(hr)) break;
            got++;
            if (i == 1) IMFSample_GetSampleTime(sample, &t0);
            if (i == 9) IMFSample_GetSampleTime(sample, &t1);
            IMFSample_GetSampleDuration(sample, &d);
            if (i == 0)
            {
                IMFSample_ConvertToContiguousBuffer(sample, &mb);
                IMFMediaBuffer_Lock(mb, &data, NULL, &len);
                for (DWORD k = 0; k < len; ++k) if (data[k] != data[0]) { nonzero = 1; break; }
                IMFMediaBuffer_Unlock(mb);
                IMFMediaBuffer_Release(mb);
            }
            IMFSample_Release(sample);
        }
        CHECKF(got == 10, "%s: ten samples (%d, %#lx)", tag, got, hr);
        CHECKF(len == def_w * def_h * 2, "%s: sample size is a YUY2 frame (%lu)", tag, len);
        CHECKF(nonzero, "%s: the picture is not flat", tag);
        {
            LONGLONG span = t1 - t0;
            LONGLONG per = 333333;
            CHECKF(got == 10 && span > 8 * per * 8 / 10 && span < 8 * per * 14 / 10,
                    "%s: timestamps follow the frame rate (8 frames span %lld, per frame %lld)", tag, span, per);
        }
        CHECKF(d > 0, "%s: sample duration set (%lld)", tag, d);
    }

    /* a new type: NV12 at the first mode, applied by Start */
    {
        IMFPresentationDescriptor *pd2;
        UINT w = def_w == 640 ? 320 : def_w, h = def_w == 640 ? 240 : def_h;

        IMFMediaSource_CreatePresentationDescriptor(source, &pd2);
        hr = set_type(pd2, &MFVideoFormat_NV12, w, h, 30);
        CHECKF(hr == S_OK, "%s: choose NV12 %ux%u (%#lx)", tag, w, h, hr);
        hr = IMFMediaSource_Start(source, pd2, &GUID_NULL, &pos);
        CHECKF(hr == S_OK, "%s: restart with the new type (%#lx)", tag, hr);
        hr = wait_event((IMFMediaEventGenerator *)source, MEUpdatedStream, NULL, 5000);
        CHECKF(hr == S_OK, "%s: MEUpdatedStream", tag);
        hr = wait_event((IMFMediaEventGenerator *)source, MESourceStarted, NULL, 5000);
        CHECKF(hr == S_OK, "%s: MESourceStarted again", tag);
        hr = get_sample(source, stream, &sample, 5000);
        CHECKF(hr == S_OK, "%s: NV12 sample (%#lx)", tag, hr);
        if (hr == S_OK)
        {
            DWORD len = 0;
            IMFSample_GetTotalLength(sample, &len);
            CHECKF(len == w * h * 3 / 2, "%s: NV12 frame size (%lu, want %u)", tag, len, w * h * 3 / 2);
            IMFSample_Release(sample);
        }
        IMFPresentationDescriptor_Release(pd2);
    }

    hr = IMFMediaSource_Stop(source);
    CHECKF(hr == S_OK, "%s: Stop (%#lx)", tag, hr);
    hr = wait_event((IMFMediaEventGenerator *)source, MESourceStopped, NULL, 5000);
    CHECKF(hr == S_OK, "%s: MESourceStopped", tag);
    hr = wait_event((IMFMediaEventGenerator *)stream, MEStreamStopped, NULL, 5000);
    CHECKF(hr == S_OK, "%s: MEStreamStopped", tag);
    hr = IMFMediaStream_RequestSample(stream, NULL);
    CHECKF(hr == MF_E_MEDIA_SOURCE_WRONGSTATE, "%s: RequestSample when stopped (%#lx)", tag, hr);

    /* idle: a stopped camera costs no CPU */
    {
        FILETIME c, e, k0, u0, k1, u1;
        ULONGLONG used;
        GetProcessTimes(GetCurrentProcess(), &c, &e, &k0, &u0);
        Sleep(1500);
        GetProcessTimes(GetCurrentProcess(), &c, &e, &k1, &u1);
        used = (((ULONGLONG)k1.dwHighDateTime << 32 | k1.dwLowDateTime) - ((ULONGLONG)k0.dwHighDateTime << 32 | k0.dwLowDateTime))
             + (((ULONGLONG)u1.dwHighDateTime << 32 | u1.dwLowDateTime) - ((ULONGLONG)u0.dwHighDateTime << 32 | u0.dwLowDateTime));
        CHECKF(used < 1000000, "%s: stopped camera is idle (%llu x100ns CPU in 1.5 s)", tag, (unsigned long long)used);
    }

    /* start again after stop */
    hr = IMFMediaSource_Start(source, pd, &GUID_NULL, &pos);
    CHECKF(hr == S_OK, "%s: Start after Stop (%#lx)", tag, hr);
    hr = get_sample(source, stream, &sample, 5000);
    CHECKF(hr == S_OK, "%s: sample after restart (%#lx)", tag, hr);
    if (hr == S_OK) IMFSample_Release(sample);
    hr = IMFMediaSource_Start(source, pd, (const GUID *)&IID_IUnknown, &pos);
    CHECKF(hr == MF_E_UNSUPPORTED_TIME_FORMAT, "%s: time format (%#lx)", tag, hr);

    hr = IMFActivate_ShutdownObject(activate);
    CHECKF(hr == S_OK, "%s: ShutdownObject (%#lx)", tag, hr);
out:
    hr = IMFMediaSource_Shutdown(source);
    CHECKF(hr == MF_E_SHUTDOWN, "%s: Shutdown again is MF_E_SHUTDOWN (%#lx)", tag, hr);
    hr = IMFMediaSource_GetCharacteristics(source, &chars);
    CHECKF(hr == MF_E_SHUTDOWN, "%s: shut down source (%#lx)", tag, hr);
    if (stream)
    {
        hr = IMFMediaStream_RequestSample(stream, NULL);
        CHECKF(hr == MF_E_SHUTDOWN, "%s: shut down stream (%#lx)", tag, hr);
        IMFMediaStream_Release(stream);
    }
    IMFPresentationDescriptor_Release(pd);
    IMFMediaSource_Release(source);
    (void)buf;
}

int main(void)
{
    create_source_t create_source;
    create_activate_t create_activate;
    HMODULE mf;
    IMFAttributes *attrs;
    IMFActivate **sources = NULL;
    UINT32 count = 0, i;
    HRESULT hr;
    WCHAR str[200];
    GUID g;
    int base, after;

    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    MFStartup(MF_VERSION, 0);
    mf = LoadLibraryA("mf.dll");
    create_source = (create_source_t)GetProcAddress(mf, "MFCreateDeviceSource");
    create_activate = (create_activate_t)GetProcAddress(mf, "MFCreateDeviceSourceActivate");
    CHECKF(create_source && create_activate, "mf exports MFCreateDeviceSource and MFCreateDeviceSourceActivate");
    if (!create_source || !create_activate) return 1;

    MFCreateAttributes(&attrs, 2);
    IMFAttributes_SetGUID(attrs, &MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, &MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID);
    hr = MFEnumDeviceSources(attrs, &sources, &count);
    CHECKF(hr == S_OK && count == 2 && sources, "two fake cameras enumerate (%#lx, %u)", hr, count);
    if (count != 2) return 1;

    for (i = 0; i < 2; ++i)
    {
        hr = IMFActivate_GetGUID(sources[i], &MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, &g);
        CHECKF(hr == S_OK && IsEqualGUID(&g, &MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID), "camera %u: source type", i);
        hr = IMFActivate_GetGUID(sources[i], &MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_CATEGORY, &g);
        CHECKF(hr == S_OK && IsEqualGUID(&g, &cat_camera), "camera %u: category is the video camera class", i);
        hr = IMFActivate_GetString(sources[i], &MF_DEVSOURCE_ATTRIBUTE_FRIENDLY_NAME, str, 200, NULL);
        CHECKF(hr == S_OK && !wcsncmp(str, L"Fake ", 5), "camera %u: friendly name %ls", i, str);
        hr = IMFActivate_GetString(sources[i], &MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_SYMBOLIC_LINK, str, 200, NULL);
        CHECKF(hr == S_OK && !wcsncmp(str, L"fake:", 5), "camera %u: symbolic link %ls", i, str);
    }

    /* category filter */
    {
        IMFActivate **none = (IMFActivate **)0x1234;
        UINT32 n = 99;
        IMFAttributes_SetGUID(attrs, &MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_CATEGORY, &MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID);
        hr = MFEnumDeviceSources(attrs, &none, &n);
        CHECKF(hr == S_OK && n == 0, "a category that is not a camera: no devices (%#lx, %u)", hr, n);
        IMFAttributes_SetGUID(attrs, &MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_CATEGORY, &cat_camera);
        hr = MFEnumDeviceSources(attrs, &none, &n);
        CHECKF(hr == S_OK && n == 2, "the camera category: both (%#lx, %u)", hr, n);
        for (i = 0; i < n; ++i) IMFActivate_Release(none[i]);
        CoTaskMemFree(none);
        IMFAttributes_DeleteItem(attrs, &MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_CATEGORY);
    }

    test_source(sources[0], "raw", 320, 240, 640, 480, 6, 0);
    base = thread_count();
    for (i = 0; i < 3; ++i)
    {
        IMFMediaSource *s;
        IMFMediaStream *st;
        IMFPresentationDescriptor *pd;
        PROPVARIANT pos;
        IMFActivate_ActivateObject(sources[0], &IID_IMFMediaSource, (void **)&s);
        IMFMediaSource_CreatePresentationDescriptor(s, &pd);
        PropVariantInit(&pos);
        IMFMediaSource_Start(s, pd, &GUID_NULL, &pos);
        st = NULL;
        {
            IMFMediaEvent *ev;
            if (wait_event((IMFMediaEventGenerator *)s, MENewStream, &ev, 5000) == S_OK)
            {
                PROPVARIANT v; PropVariantInit(&v); IMFMediaEvent_GetValue(ev, &v);
                IUnknown_QueryInterface(v.punkVal, &IID_IMFMediaStream, (void **)&st);
                PropVariantClear(&v); IMFMediaEvent_Release(ev);
            }
        }
        if (st) { IMFSample *sm; if (get_sample(s, st, &sm, 5000) == S_OK) IMFSample_Release(sm); IMFMediaStream_Release(st); }
        IMFActivate_ShutdownObject(sources[0]);
        IMFPresentationDescriptor_Release(pd);
        IMFMediaSource_Release(s);
    }
    after = thread_count();
    CHECKF(base > 0 && after <= base, "start/shutdown cycles do not leave threads (%d -> %d)", base, after);

    test_source(sources[1], "jpeg", 1280, 720, 1280, 720, 4, 1);

    /* MFCreateDeviceSource / MFCreateDeviceSourceActivate */
    {
        IMFMediaSource *s = NULL;
        IMFActivate *act = NULL;
        IMFAttributes *a2;
        MFCreateAttributes(&a2, 2);

        hr = create_source(a2, &s);
        CHECKF(hr == MF_E_ATTRIBUTENOTFOUND, "MFCreateDeviceSource without a type (%#lx)", hr);
        IMFAttributes_SetGUID(a2, &MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, &MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID);
        hr = create_source(a2, &s);
        CHECKF(hr == E_INVALIDARG, "MFCreateDeviceSource without a symbolic link (%#lx)", hr);
        IMFAttributes_SetString(a2, &MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_SYMBOLIC_LINK, L"fake:none9");
        hr = create_source(a2, &s);
        CHECKF(hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND) && !s, "MFCreateDeviceSource of an unknown camera (%#lx)", hr);
        IMFActivate_GetString(sources[1], &MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_SYMBOLIC_LINK, str, 200, NULL);
        IMFAttributes_SetString(a2, &MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_SYMBOLIC_LINK, str);
        hr = create_source(a2, &s);
        CHECKF(hr == S_OK && s, "MFCreateDeviceSource of the JPEG camera (%#lx)", hr);
        if (s)
        {
            IMFAttributes *sa;
            WCHAR name[100];
            hr = IMFMediaSourceEx_GetSourceAttributes((IMFMediaSourceEx *)s, &sa);
            if (SUCCEEDED(hr))
            {
                hr = IMFAttributes_GetString(sa, &MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_SYMBOLIC_LINK, name, 100, NULL);
                CHECKF(hr == S_OK && !wcscmp(name, str), "source attributes carry the symbolic link");
                IMFAttributes_Release(sa);
            }
            IMFMediaSource_Shutdown(s);
            IMFMediaSource_Release(s);
        }
        hr = create_activate(a2, &act);
        CHECKF(hr == S_OK && act, "MFCreateDeviceSourceActivate (%#lx)", hr);
        if (act)
        {
            WCHAR link2[100];
            hr = IMFActivate_GetString(act, &MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_SYMBOLIC_LINK, link2, 100, NULL);
            CHECKF(hr == S_OK && !wcscmp(link2, str), "the activate carries the attributes it was made from");
            IMFActivate_Release(act);
        }
        IMFAttributes_Release(a2);
        MFCreateAttributes(&a2, 1);
        hr = create_activate(a2, &act);
        CHECKF(hr == MF_E_ATTRIBUTENOTFOUND, "MFCreateDeviceSourceActivate without a type (%#lx)", hr);
        IMFAttributes_Release(a2);
    }

    for (i = 0; i < 2; ++i) IMFActivate_Release(sources[i]);
    CoTaskMemFree(sources);
    IMFAttributes_Release(attrs);
    MFShutdown();
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
