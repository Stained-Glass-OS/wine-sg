/* evr's default mixer (patches/sg/2890), run by test/evr-mixer-gate.sh:
 * IMFVideoProcessor (mode, ProcAmp and filtering ranges and values),
 * IMFVideoMixerBitmap, IMFVideoPositionMapper, IMFQualityAdvise,
 * IMFClockStateSink and SetInputType for a substream. Table-driven where the
 * answers are a list. The parts that need a media type use a D3D9 device and
 * the device manager; without a device they print a note.
 *
 *   evr-mixer-probe.exe */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <d3d9.h>
#include <dxva2api.h>
#include <mfapi.h>
#include <mferror.h>
#include <mfidl.h>
#include <evr.h>
#include <evr9.h>
#include <stdio.h>
#include <string.h>

#include "evr-qa.h"

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}
#define CHECKF(ok, ...) do { char _b[320]; snprintf(_b, sizeof(_b), __VA_ARGS__); check(ok, _b); } while (0)

#define FIXED(f) ((LONG)((f) * 65536.0))

static LRESULT CALLBACK wndproc(HWND h, UINT m, WPARAM w, LPARAM l) { return DefWindowProcA(h, m, w, l); }

static IDirect3DDevice9 *create_device(HWND window)
{
    IDirect3D9 *(WINAPI *create9)(UINT) = (void *)GetProcAddress(LoadLibraryA("d3d9.dll"), "Direct3DCreate9");
    D3DPRESENT_PARAMETERS pp = {0};
    IDirect3DDevice9 *device = NULL;
    IDirect3D9 *d3d9;

    if (!create9 || !(d3d9 = create9(D3D_SDK_VERSION))) return NULL;
    pp.BackBufferWidth = 640;
    pp.BackBufferHeight = 480;
    pp.BackBufferFormat = D3DFMT_A8R8G8B8;
    pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    pp.hDeviceWindow = window;
    pp.Windowed = TRUE;
    IDirect3D9_CreateDevice(d3d9, D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, window,
            D3DCREATE_HARDWARE_VERTEXPROCESSING, &pp, &device);
    IDirect3D9_Release(d3d9);
    return device;
}

static IMFMediaType *rgb32_type(unsigned int w, unsigned int h)
{
    IMFVideoMediaType *vt;
    IMFMediaType *type;

    if (FAILED(MFCreateVideoMediaTypeFromSubtype(&MFVideoFormat_RGB32, &vt))) return NULL;
    IMFVideoMediaType_QueryInterface(vt, &IID_IMFMediaType, (void **)&type);
    IMFVideoMediaType_Release(vt);
    IMFMediaType_SetUINT64(type, &MF_MT_FRAME_SIZE, (UINT64)w << 32 | h);
    IMFMediaType_SetUINT32(type, &MF_MT_ALL_SAMPLES_INDEPENDENT, TRUE);
    return type;
}

/* things that need no media type */
static void test_untyped(IMFTransform *mixer)
{
    IMFVideoProcessor *proc;
    IMFVideoMixerBitmap *bitmap;
    IMFQualityAdvise *qa;
    IMFClockStateSink *sink;
    MFVideoAlphaBitmapParams params;
    MFVideoAlphaBitmap bm;
    DXVA2_ValueRange range;
    DXVA2_ProcAmpValues values;
    DXVA2_Fixed32 fixed;
    GUID mode;
    HRESULT hr;
    unsigned int i;

    IMFTransform_QueryInterface(mixer, &IID_IMFVideoProcessor, (void **)&proc);
    hr = IMFVideoProcessor_GetProcAmpRange(proc, DXVA2_ProcAmp_Brightness, &range);
    CHECKF(hr == MF_E_TRANSFORM_TYPE_NOT_SET, "GetProcAmpRange without types is MF_E_TRANSFORM_TYPE_NOT_SET (%#lx)", hr);
    hr = IMFVideoProcessor_GetProcAmpRange(proc, DXVA2_ProcAmp_Brightness, NULL);
    CHECKF(hr == E_POINTER, "GetProcAmpRange(NULL) is E_POINTER (%#lx)", hr);
    hr = IMFVideoProcessor_GetProcAmpValues(proc, DXVA2_ProcAmp_Mask, &values);
    CHECKF(hr == MF_E_TRANSFORM_TYPE_NOT_SET, "GetProcAmpValues without types (%#lx)", hr);
    hr = IMFVideoProcessor_SetProcAmpValues(proc, DXVA2_ProcAmp_Mask, &values);
    CHECKF(hr == MF_E_TRANSFORM_TYPE_NOT_SET, "SetProcAmpValues without types (%#lx)", hr);
    hr = IMFVideoProcessor_GetProcAmpValues(proc, DXVA2_ProcAmp_Mask, NULL);
    CHECKF(hr == E_POINTER, "GetProcAmpValues(NULL) is E_POINTER (%#lx)", hr);
    hr = IMFVideoProcessor_GetFilteringRange(proc, DXVA2_DetailFilterLumaLevel, &range);
    CHECKF(hr == MF_E_TRANSFORM_TYPE_NOT_SET, "GetFilteringRange without types (%#lx)", hr);
    hr = IMFVideoProcessor_GetFilteringValue(proc, DXVA2_DetailFilterLumaLevel, &fixed);
    CHECKF(hr == MF_E_TRANSFORM_TYPE_NOT_SET, "GetFilteringValue without types (%#lx)", hr);
    fixed.ll = 0;
    hr = IMFVideoProcessor_SetFilteringValue(proc, DXVA2_DetailFilterLumaLevel, &fixed);
    CHECKF(hr == MF_E_TRANSFORM_TYPE_NOT_SET, "SetFilteringValue without types (%#lx)", hr);
    hr = IMFVideoProcessor_GetVideoProcessorMode(proc, &mode);
    CHECKF(hr == MF_E_TRANSFORM_TYPE_NOT_SET, "GetVideoProcessorMode without types (%#lx)", hr);
    hr = IMFVideoProcessor_GetVideoProcessorMode(proc, NULL);
    CHECKF(hr == E_POINTER, "GetVideoProcessorMode(NULL) is E_POINTER (%#lx)", hr);
    hr = IMFVideoProcessor_SetVideoProcessorMode(proc, &mode);
    CHECKF(hr == MF_E_TRANSFORM_TYPE_NOT_SET, "SetVideoProcessorMode without types (%#lx)", hr);
    IMFVideoProcessor_Release(proc);

    /* alpha bitmap: nothing set yet */
    IMFTransform_QueryInterface(mixer, &IID_IMFVideoMixerBitmap, (void **)&bitmap);
    hr = IMFVideoMixerBitmap_GetAlphaBitmapParameters(bitmap, &params);
    CHECKF(hr == MF_E_NOT_INITIALIZED, "GetAlphaBitmapParameters before a bitmap is MF_E_NOT_INITIALIZED (%#lx)", hr);
    hr = IMFVideoMixerBitmap_GetAlphaBitmapParameters(bitmap, NULL);
    CHECKF(hr == E_POINTER, "GetAlphaBitmapParameters(NULL) is E_POINTER (%#lx)", hr);
    memset(&params, 0, sizeof(params));
    params.dwFlags = MFVideoAlphaBitmap_Alpha;
    params.fAlpha = 0.5f;
    hr = IMFVideoMixerBitmap_UpdateAlphaBitmapParameters(bitmap, &params);
    CHECKF(hr == MF_E_NOT_INITIALIZED, "UpdateAlphaBitmapParameters before a bitmap is MF_E_NOT_INITIALIZED (%#lx)", hr);
    hr = IMFVideoMixerBitmap_UpdateAlphaBitmapParameters(bitmap, NULL);
    CHECKF(hr == E_POINTER, "UpdateAlphaBitmapParameters(NULL) is E_POINTER (%#lx)", hr);
    hr = IMFVideoMixerBitmap_SetAlphaBitmap(bitmap, NULL);
    CHECKF(hr == E_POINTER, "SetAlphaBitmap(NULL) is E_POINTER (%#lx)", hr);
    memset(&bm, 0, sizeof(bm));
    hr = IMFVideoMixerBitmap_SetAlphaBitmap(bitmap, &bm);
    CHECKF(hr == E_INVALIDARG, "SetAlphaBitmap without a surface is E_INVALIDARG (%#lx)", hr);
    hr = IMFVideoMixerBitmap_ClearAlphaBitmap(bitmap);
    CHECKF(hr == S_OK, "ClearAlphaBitmap with none set is S_OK (%#lx)", hr);
    IMFVideoMixerBitmap_Release(bitmap);

    /* quality advise: the mixer cannot drop or degrade */
    IMFTransform_QueryInterface(mixer, &IID_IMFQualityAdvise_sg, (void **)&qa);
    {
        static const struct { int mode; HRESULT hr; } drop[] =
        {
            { MF_DROP_MODE_NONE, S_OK }, { MF_DROP_MODE_1, MF_E_NO_MORE_DROP_MODES },
            { MF_DROP_MODE_5, MF_E_NO_MORE_DROP_MODES }, { MF_NUM_DROP_MODES, E_INVALIDARG }, { 99, E_INVALIDARG },
        };
        static const struct { int level; HRESULT hr; } qual[] =
        {
            { MF_QUALITY_NORMAL, S_OK }, { MF_QUALITY_NORMAL_MINUS_1, MF_E_NO_MORE_QUALITY_LEVELS },
            { MF_QUALITY_NORMAL_MINUS_5, MF_E_NO_MORE_QUALITY_LEVELS }, { MF_NUM_QUALITY_LEVELS, E_INVALIDARG },
        };
        MF_QUALITY_DROP_MODE dm = MF_DROP_MODE_5;
        MF_QUALITY_LEVEL lv = MF_QUALITY_NORMAL_MINUS_5;

        for (i = 0; i < ARRAY_SIZE(drop); ++i)
        {
            hr = IMFQualityAdvise_SetDropMode(qa, drop[i].mode);
            CHECKF(hr == drop[i].hr, "SetDropMode(%d) is %#lx (%#lx)", drop[i].mode, (long)drop[i].hr, hr);
        }
        for (i = 0; i < ARRAY_SIZE(qual); ++i)
        {
            hr = IMFQualityAdvise_SetQualityLevel(qa, qual[i].level);
            CHECKF(hr == qual[i].hr, "SetQualityLevel(%d) is %#lx (%#lx)", qual[i].level, (long)qual[i].hr, hr);
        }
        hr = IMFQualityAdvise_GetDropMode(qa, &dm);
        CHECKF(hr == S_OK && dm == MF_DROP_MODE_NONE, "GetDropMode is S_OK, none (%#lx, %d)", hr, dm);
        hr = IMFQualityAdvise_GetQualityLevel(qa, &lv);
        CHECKF(hr == S_OK && lv == MF_QUALITY_NORMAL, "GetQualityLevel is S_OK, normal (%#lx, %d)", hr, lv);
        hr = IMFQualityAdvise_GetDropMode(qa, NULL);
        CHECKF(hr == E_POINTER, "GetDropMode(NULL) is E_POINTER (%#lx)", hr);
        hr = IMFQualityAdvise_GetQualityLevel(qa, NULL);
        CHECKF(hr == E_POINTER, "GetQualityLevel(NULL) is E_POINTER (%#lx)", hr);
        hr = IMFQualityAdvise_DropTime(qa, 10000000);
        CHECKF(hr == MF_E_DROPTIME_NOT_SUPPORTED, "DropTime is MF_E_DROPTIME_NOT_SUPPORTED (%#lx)", hr);
    }
    IMFQualityAdvise_Release(qa);

    /* the clock sink takes every notification */
    IMFTransform_QueryInterface(mixer, &IID_IMFClockStateSink, (void **)&sink);
    hr = IMFClockStateSink_OnClockStart(sink, 0, 0);
    CHECKF(hr == S_OK, "OnClockStart is S_OK (%#lx)", hr);
    hr = IMFClockStateSink_OnClockPause(sink, 100);
    CHECKF(hr == S_OK, "OnClockPause is S_OK (%#lx)", hr);
    hr = IMFClockStateSink_OnClockRestart(sink, 200);
    CHECKF(hr == S_OK, "OnClockRestart is S_OK (%#lx)", hr);
    hr = IMFClockStateSink_OnClockSetRate(sink, 300, 2.0f);
    CHECKF(hr == S_OK, "OnClockSetRate is S_OK (%#lx)", hr);
    hr = IMFClockStateSink_OnClockStop(sink, 400);
    CHECKF(hr == S_OK, "OnClockStop is S_OK (%#lx)", hr);
    IMFClockStateSink_Release(sink);

    /* the documented E_NOTIMPL answers stay */
    {
        IMFAttributes *attrs = NULL;
        IMFMediaType *type = NULL;
        hr = IMFTransform_GetOutputStreamAttributes(mixer, 0, &attrs);
        CHECKF(hr == E_NOTIMPL, "GetOutputStreamAttributes is E_NOTIMPL (%#lx)", hr);
        hr = IMFTransform_GetInputAvailableType(mixer, 0, 0, &type);
        CHECKF(hr == E_NOTIMPL, "GetInputAvailableType is E_NOTIMPL (%#lx)", hr);
        hr = IMFTransform_ProcessEvent(mixer, 0, NULL);
        CHECKF(hr == E_NOTIMPL, "ProcessEvent is E_NOTIMPL (%#lx)", hr);
    }
}

static void test_mapper(IMFTransform *mixer)
{
    IMFVideoPositionMapper *mapper;
    IMFVideoMixerControl *control;
    MFVideoNormalizedRect rect = { 0.5f, 0.0f, 1.0f, 0.5f };
    float x = -1, y = -1;
    HRESULT hr;
    DWORD id = 3;

    IMFTransform_QueryInterface(mixer, &IID_IMFVideoPositionMapper, (void **)&mapper);
    IMFTransform_QueryInterface(mixer, &IID_IMFVideoMixerControl, (void **)&control);

    hr = IMFVideoPositionMapper_MapOutputCoordinateToInputStream(mapper, 0.25f, 0.75f, 0, 0, &x, &y);
    CHECKF(hr == S_OK && x == 0.25f && y == 0.75f, "a full-size stream maps coordinates as they are (%#lx, %f, %f)", hr, x, y);
    hr = IMFVideoPositionMapper_MapOutputCoordinateToInputStream(mapper, 0.25f, 0.75f, 0, 0, NULL, &y);
    CHECKF(hr == E_POINTER, "NULL x is E_POINTER (%#lx)", hr);
    hr = IMFVideoPositionMapper_MapOutputCoordinateToInputStream(mapper, 0.25f, 0.75f, 0, 0, &x, NULL);
    CHECKF(hr == E_POINTER, "NULL y is E_POINTER (%#lx)", hr);
    hr = IMFVideoPositionMapper_MapOutputCoordinateToInputStream(mapper, 0.25f, 0.75f, 1, 0, &x, &y);
    CHECKF(hr == MF_E_INVALIDINDEX, "output stream 1 is MF_E_INVALIDINDEX (%#lx)", hr);
    hr = IMFVideoPositionMapper_MapOutputCoordinateToInputStream(mapper, 0.25f, 0.75f, 0, 7, &x, &y);
    CHECKF(hr == MF_E_INVALIDINDEX, "an unknown input stream is MF_E_INVALIDINDEX (%#lx)", hr);

    /* a stream composed into the top right quarter */
    hr = IMFVideoMixerControl_SetStreamOutputRect(control, 0, &rect);
    CHECKF(hr == S_OK, "SetStreamOutputRect (%#lx)", hr);
    hr = IMFVideoPositionMapper_MapOutputCoordinateToInputStream(mapper, 0.75f, 0.25f, 0, 0, &x, &y);
    CHECKF(hr == S_OK && x == 0.5f && y == 0.5f, "the output rectangle is mapped back to the stream (%#lx, %f, %f)", hr, x, y);
    hr = IMFVideoPositionMapper_MapOutputCoordinateToInputStream(mapper, 1.0f, 0.5f, 0, 0, &x, &y);
    CHECKF(hr == S_OK && x == 1.0f && y == 1.0f, "its far corner is the stream's far corner (%#lx, %f, %f)", hr, x, y);
    hr = IMFVideoPositionMapper_MapOutputCoordinateToInputStream(mapper, 0.5f, 0.0f, 0, 0, &x, &y);
    CHECKF(hr == S_OK && x == 0.0f && y == 0.0f, "its near corner is the stream's origin (%#lx, %f, %f)", hr, x, y);

    hr = IMFTransform_AddInputStreams(mixer, 1, &id);
    CHECKF(hr == S_OK, "AddInputStreams (%#lx)", hr);
    hr = IMFVideoPositionMapper_MapOutputCoordinateToInputStream(mapper, 0.5f, 0.5f, 0, id, &x, &y);
    CHECKF(hr == S_OK && x == 0.5f && y == 0.5f, "a substream starts with the full output rectangle (%#lx, %f, %f)", hr, x, y);
    IMFTransform_DeleteInputStream(mixer, id);

    IMFVideoMixerControl_Release(control);
    IMFVideoPositionMapper_Release(mapper);
}

static BOOL fixed_in(DXVA2_Fixed32 v, DXVA2_ValueRange *r)
{
    return v.ll >= r->MinValue.ll && v.ll <= r->MaxValue.ll;
}

static void test_typed(IMFTransform *mixer, IDirect3DDevice9 *device)
{
    static const DWORD procamp[] = { DXVA2_ProcAmp_Brightness, DXVA2_ProcAmp_Contrast, DXVA2_ProcAmp_Hue, DXVA2_ProcAmp_Saturation };
    static const char *procamp_names[] = { "brightness", "contrast", "hue", "saturation" };
    IMFVideoProcessor *proc;
    IMFVideoMixerBitmap *bitmap;
    IDirect3DDeviceManager9 *manager;
    HRESULT (WINAPI *create_manager)(UINT *, IDirect3DDeviceManager9 **);
    IMFMediaType *type, *sub_type, *out_type, *cur;
    DXVA2_ValueRange range;
    DXVA2_ProcAmpValues values;
    DXVA2_Fixed32 fixed;
    MFVideoAlphaBitmap bm;
    MFVideoAlphaBitmapParams params;
    IDirect3DSurface9 *surface = NULL;
    GUID mode, *modes = NULL;
    UINT token, count = 0;
    HRESULT hr;
    DWORD id = 1;
    unsigned int i;

    create_manager = (void *)GetProcAddress(LoadLibraryA("dxva2.dll"), "DXVA2CreateDirect3DDeviceManager9");
    if (!create_manager || FAILED(create_manager(&token, &manager)))
    {
        puts("NOTE: no device manager, typed tests skipped");
        return;
    }
    IDirect3DDeviceManager9_ResetDevice(manager, device, token);
    hr = IMFTransform_ProcessMessage(mixer, MFT_MESSAGE_SET_D3D_MANAGER, (ULONG_PTR)manager);
    CHECKF(hr == S_OK, "SET_D3D_MANAGER (%#lx)", hr);

    type = rgb32_type(64, 64);
    sub_type = rgb32_type(32, 32);

    /* substreams: unknown ids, and their types are independent of the reference stream */
    hr = IMFTransform_SetInputType(mixer, 5, type, 0);
    CHECKF(hr == MF_E_INVALIDSTREAMNUMBER, "SetInputType on an unknown stream is MF_E_INVALIDSTREAMNUMBER (%#lx)", hr);
    hr = IMFTransform_SetInputType(mixer, 5, NULL, 0);
    CHECKF(hr == MF_E_INVALIDSTREAMNUMBER, "clearing an unknown stream is MF_E_INVALIDSTREAMNUMBER (%#lx)", hr);

    hr = IMFTransform_SetInputType(mixer, 0, type, 0);
    CHECKF(hr == S_OK, "SetInputType(0) (%#lx)", hr);
    hr = IMFTransform_GetOutputAvailableType(mixer, 0, 0, &out_type);
    CHECKF(hr == S_OK, "GetOutputAvailableType (%#lx)", hr);
    hr = IMFTransform_SetOutputType(mixer, 0, out_type, 0);
    CHECKF(hr == S_OK, "SetOutputType (%#lx)", hr);
    IMFMediaType_Release(out_type);

    hr = IMFTransform_AddInputStreams(mixer, 1, &id);
    CHECKF(hr == S_OK, "AddInputStreams(1) (%#lx)", hr);
    hr = IMFTransform_SetInputType(mixer, 1, sub_type, MFT_SET_TYPE_TEST_ONLY);
    CHECKF(hr == S_OK, "SetInputType(1, test only) (%#lx)", hr);
    hr = IMFTransform_GetInputCurrentType(mixer, 1, &cur);
    CHECKF(hr == MF_E_TRANSFORM_TYPE_NOT_SET, "a test-only type is not kept (%#lx)", hr);
    hr = IMFTransform_SetInputType(mixer, 1, sub_type, 0);
    CHECKF(hr == S_OK, "SetInputType(1) for a substream is S_OK (%#lx)", hr);
    cur = NULL;
    hr = IMFTransform_GetInputCurrentType(mixer, 1, &cur);
    CHECKF(hr == S_OK && cur == sub_type, "the substream keeps its type (%#lx)", hr);
    if (cur) IMFMediaType_Release(cur);
    cur = NULL;
    hr = IMFTransform_GetInputCurrentType(mixer, 0, &cur);
    CHECKF(hr == S_OK && cur == type, "the reference stream type is still there (%#lx)", hr);
    if (cur) IMFMediaType_Release(cur);
    cur = NULL;
    hr = IMFTransform_GetOutputCurrentType(mixer, 0, &cur);
    CHECKF(hr == S_OK, "the output type is still there (%#lx)", hr);
    if (cur) IMFMediaType_Release(cur);
    hr = IMFTransform_SetInputType(mixer, 1, NULL, 0);
    CHECKF(hr == S_OK, "clearing the substream type (%#lx)", hr);
    hr = IMFTransform_GetInputCurrentType(mixer, 1, &cur);
    CHECKF(hr == MF_E_TRANSFORM_TYPE_NOT_SET, "the substream type is gone (%#lx)", hr);
    cur = NULL;
    hr = IMFTransform_GetInputCurrentType(mixer, 0, &cur);
    CHECKF(hr == S_OK && cur == type, "clearing the substream keeps the reference type (%#lx)", hr);
    if (cur) IMFMediaType_Release(cur);
    hr = IMFTransform_GetOutputCurrentType(mixer, 0, &cur);
    CHECKF(hr == S_OK, "clearing the substream keeps the output type (%#lx)", hr);
    if (cur) IMFMediaType_Release(cur);
    IMFTransform_DeleteInputStream(mixer, 1);

    IMFTransform_QueryInterface(mixer, &IID_IMFVideoProcessor, (void **)&proc);

    /* processor mode */
    memset(&mode, 0xcc, sizeof(mode));
    hr = IMFVideoProcessor_GetVideoProcessorMode(proc, &mode);
    CHECKF(hr == S_FALSE && IsEqualGUID(&mode, &GUID_NULL), "no mode chosen: S_FALSE and a null GUID (%#lx)", hr);
    hr = IMFVideoProcessor_GetAvailableVideoProcessorModes(proc, &count, &modes);
    CHECKF(hr == S_OK && count > 0, "GetAvailableVideoProcessorModes (%#lx, %u)", hr, count);
    {
        GUID bogus = { 0x5eed0001, 0x1234, 0x4321, { 1, 2, 3, 4, 5, 6, 7, 8 } };
        hr = IMFVideoProcessor_SetVideoProcessorMode(proc, &bogus);
        CHECKF(hr == E_INVALIDARG, "SetVideoProcessorMode with an unknown GUID is E_INVALIDARG (%#lx)", hr);
    }
    hr = IMFVideoProcessor_SetVideoProcessorMode(proc, NULL);
    CHECKF(hr == E_POINTER, "SetVideoProcessorMode(NULL) is E_POINTER (%#lx)", hr);
    if (count)
    {
        hr = IMFVideoProcessor_SetVideoProcessorMode(proc, &modes[0]);
        CHECKF(hr == S_OK, "SetVideoProcessorMode with an offered mode (%#lx)", hr);
        memset(&mode, 0, sizeof(mode));
        hr = IMFVideoProcessor_GetVideoProcessorMode(proc, &mode);
        CHECKF(hr == S_OK && IsEqualGUID(&mode, &modes[0]), "GetVideoProcessorMode returns the chosen mode (%#lx)", hr);
    }
    CoTaskMemFree(modes);

    /* ProcAmp ranges and defaults */
    for (i = 0; i < ARRAY_SIZE(procamp); ++i)
    {
        memset(&range, 0, sizeof(range));
        hr = IMFVideoProcessor_GetProcAmpRange(proc, procamp[i], &range);
        CHECKF(hr == S_OK && range.MinValue.ll < range.MaxValue.ll && fixed_in(range.DefaultValue, &range)
                && range.StepSize.ll > 0, "%s has a sane range (%#lx)", procamp_names[i], hr);
    }
    hr = IMFVideoProcessor_GetProcAmpRange(proc, 0x3, &range);
    CHECKF(hr == E_INVALIDARG, "two properties at once is E_INVALIDARG (%#lx)", hr);
    hr = IMFVideoProcessor_GetProcAmpRange(proc, 0x10, &range);
    CHECKF(hr == E_INVALIDARG, "an unknown ProcAmp property is E_INVALIDARG (%#lx)", hr);
    memset(&values, 0x55, sizeof(values));
    hr = IMFVideoProcessor_GetProcAmpValues(proc, DXVA2_ProcAmp_Mask, &values);
    CHECKF(hr == S_OK && values.Brightness.ll == 0 && values.Contrast.ll == FIXED(1.0)
            && values.Hue.ll == 0 && values.Saturation.ll == FIXED(1.0), "the ProcAmp defaults (%#lx, %lx %lx %lx %lx)", hr,
            (long)values.Brightness.ll, (long)values.Contrast.ll, (long)values.Hue.ll, (long)values.Saturation.ll);
    hr = IMFVideoProcessor_GetProcAmpValues(proc, 0x10, &values);
    CHECKF(hr == E_INVALIDARG, "GetProcAmpValues with an unknown flag is E_INVALIDARG (%#lx)", hr);

    memset(&values, 0, sizeof(values));
    values.Brightness.ll = FIXED(10.0);
    hr = IMFVideoProcessor_SetProcAmpValues(proc, DXVA2_ProcAmp_Brightness, &values);
    CHECKF(hr == S_OK, "SetProcAmpValues(brightness 10) (%#lx)", hr);
    memset(&values, 0x55, sizeof(values));
    hr = IMFVideoProcessor_GetProcAmpValues(proc, DXVA2_ProcAmp_Brightness, &values);
    CHECKF(hr == S_OK && values.Brightness.ll == FIXED(10.0), "brightness reads back 10 (%#lx, %lx)", hr, (long)values.Brightness.ll);
    values.Brightness.ll = FIXED(5000.0);
    hr = IMFVideoProcessor_SetProcAmpValues(proc, DXVA2_ProcAmp_Brightness, &values);
    CHECKF(hr == E_INVALIDARG, "brightness 5000 is E_INVALIDARG (%#lx)", hr);
    /* all or nothing: contrast is valid, brightness is not */
    values.Contrast.ll = FIXED(1.5);
    hr = IMFVideoProcessor_SetProcAmpValues(proc, DXVA2_ProcAmp_Brightness | DXVA2_ProcAmp_Contrast, &values);
    CHECKF(hr == E_INVALIDARG, "one bad value fails the call (%#lx)", hr);
    memset(&values, 0x55, sizeof(values));
    hr = IMFVideoProcessor_GetProcAmpValues(proc, DXVA2_ProcAmp_Mask, &values);
    CHECKF(hr == S_OK && values.Brightness.ll == FIXED(10.0) && values.Contrast.ll == FIXED(1.0),
            "a failed call changed nothing (%#lx, %lx, %lx)", hr, (long)values.Brightness.ll, (long)values.Contrast.ll);
    values.Brightness.ll = FIXED(-20.0);
    values.Contrast.ll = FIXED(1.5);
    values.Hue.ll = FIXED(90.0);
    values.Saturation.ll = FIXED(0.5);
    hr = IMFVideoProcessor_SetProcAmpValues(proc, DXVA2_ProcAmp_Mask, &values);
    CHECKF(hr == S_OK, "all four values (%#lx)", hr);
    memset(&values, 0x55, sizeof(values));
    hr = IMFVideoProcessor_GetProcAmpValues(proc, DXVA2_ProcAmp_Mask, &values);
    CHECKF(hr == S_OK && values.Brightness.ll == FIXED(-20.0) && values.Contrast.ll == FIXED(1.5)
            && values.Hue.ll == FIXED(90.0) && values.Saturation.ll == FIXED(0.5), "all four read back (%#lx)", hr);
    hr = IMFVideoProcessor_SetProcAmpValues(proc, 0x20, &values);
    CHECKF(hr == E_INVALIDARG, "SetProcAmpValues with an unknown flag is E_INVALIDARG (%#lx)", hr);
    hr = IMFVideoProcessor_SetProcAmpValues(proc, DXVA2_ProcAmp_Mask, NULL);
    CHECKF(hr == E_POINTER, "SetProcAmpValues(NULL) is E_POINTER (%#lx)", hr);

    /* filtering: properties 1..12 */
    for (i = 1; i <= 12; ++i)
    {
        memset(&range, 0, sizeof(range));
        hr = IMFVideoProcessor_GetFilteringRange(proc, i, &range);
        if (!(hr == S_OK && range.MinValue.ll < range.MaxValue.ll && fixed_in(range.DefaultValue, &range)))
            CHECKF(0, "filter %u has a sane range (%#lx)", i, hr);
        fixed.ll = 0x1234;
        hr = IMFVideoProcessor_GetFilteringValue(proc, i, &fixed);
        if (!(hr == S_OK && fixed.ll == range.DefaultValue.ll))
            CHECKF(0, "filter %u starts at its default (%#lx, %lx)", i, hr, (long)fixed.ll);
    }
    check(1, "all twelve filter properties have a range and a default");
    hr = IMFVideoProcessor_GetFilteringRange(proc, 0, &range);
    CHECKF(hr == E_INVALIDARG, "filter property 0 is E_INVALIDARG (%#lx)", hr);
    hr = IMFVideoProcessor_GetFilteringRange(proc, 13, &range);
    CHECKF(hr == E_INVALIDARG, "filter property 13 is E_INVALIDARG (%#lx)", hr);
    hr = IMFVideoProcessor_GetFilteringValue(proc, 13, &fixed);
    CHECKF(hr == E_INVALIDARG, "GetFilteringValue(13) is E_INVALIDARG (%#lx)", hr);
    IMFVideoProcessor_GetFilteringRange(proc, DXVA2_DetailFilterLumaLevel, &range);
    fixed.ll = FIXED(50.0);
    hr = IMFVideoProcessor_SetFilteringValue(proc, DXVA2_DetailFilterLumaLevel, &fixed);
    CHECKF(hr == S_OK, "SetFilteringValue(50) (%#lx)", hr);
    fixed.ll = 0;
    hr = IMFVideoProcessor_GetFilteringValue(proc, DXVA2_DetailFilterLumaLevel, &fixed);
    CHECKF(hr == S_OK && fixed.ll == FIXED(50.0), "the filtering value reads back (%#lx, %lx)", hr, (long)fixed.ll);
    hr = IMFVideoProcessor_GetFilteringValue(proc, DXVA2_DetailFilterLumaThreshold, &fixed);
    CHECKF(hr == S_OK && fixed.ll == 0, "another filter is untouched (%#lx, %lx)", hr, (long)fixed.ll);
    fixed.ll = FIXED(1000.0);
    hr = IMFVideoProcessor_SetFilteringValue(proc, DXVA2_DetailFilterLumaLevel, &fixed);
    CHECKF(hr == E_INVALIDARG, "an out of range filtering value is E_INVALIDARG (%#lx)", hr);
    hr = IMFVideoProcessor_SetFilteringValue(proc, DXVA2_DetailFilterLumaLevel, NULL);
    CHECKF(hr == E_POINTER, "SetFilteringValue(NULL) is E_POINTER (%#lx)", hr);

    /* the processor mode goes with the types */
    IMFTransform_SetInputType(mixer, 0, NULL, 0);
    hr = IMFVideoProcessor_GetVideoProcessorMode(proc, &mode);
    CHECKF(hr == MF_E_TRANSFORM_TYPE_NOT_SET, "after clearing the types the mode is MF_E_TRANSFORM_TYPE_NOT_SET (%#lx)", hr);
    IMFVideoProcessor_Release(proc);

    /* alpha bitmap with a surface */
    IMFTransform_QueryInterface(mixer, &IID_IMFVideoMixerBitmap, (void **)&bitmap);
    hr = IDirect3DDevice9_CreateOffscreenPlainSurface(device, 32, 32, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &surface, NULL);
    if (FAILED(hr))
        puts("NOTE: no offscreen surface, alpha bitmap tests skipped");
    else
    {
        memset(&bm, 0, sizeof(bm));
        bm.bitmap.pDDS = surface;
        bm.params.dwFlags = MFVideoAlphaBitmap_EntireDDS | MFVideoAlphaBitmap_Alpha | MFVideoAlphaBitmap_DestRect;
        bm.params.fAlpha = 2.0f;
        bm.params.nrcDest.left = 0.1f; bm.params.nrcDest.top = 0.1f;
        bm.params.nrcDest.right = 0.5f; bm.params.nrcDest.bottom = 0.5f;
        hr = IMFVideoMixerBitmap_SetAlphaBitmap(bitmap, &bm);
        CHECKF(hr == E_INVALIDARG, "alpha 2.0 is E_INVALIDARG (%#lx)", hr);
        bm.params.fAlpha = 0.25f;
        bm.params.nrcDest.left = 0.8f;
        hr = IMFVideoMixerBitmap_SetAlphaBitmap(bitmap, &bm);
        CHECKF(hr == E_INVALIDARG, "an inverted destination rectangle is E_INVALIDARG (%#lx)", hr);
        bm.params.nrcDest.left = 0.1f;
        bm.params.dwFlags |= 0x100;
        hr = IMFVideoMixerBitmap_SetAlphaBitmap(bitmap, &bm);
        CHECKF(hr == E_INVALIDARG, "an unknown flag is E_INVALIDARG (%#lx)", hr);
        bm.params.dwFlags &= ~0x100;
        hr = IMFVideoMixerBitmap_SetAlphaBitmap(bitmap, &bm);
        CHECKF(hr == S_OK, "SetAlphaBitmap (%#lx)", hr);
        memset(&params, 0x55, sizeof(params));
        hr = IMFVideoMixerBitmap_GetAlphaBitmapParameters(bitmap, &params);
        CHECKF(hr == S_OK && params.fAlpha == 0.25f && params.nrcDest.right == 0.5f
                && params.dwFlags == bm.params.dwFlags, "the parameters read back (%#lx, %f)", hr, params.fAlpha);
        memset(&params, 0, sizeof(params));
        params.dwFlags = MFVideoAlphaBitmap_Alpha;
        params.fAlpha = 0.75f;
        hr = IMFVideoMixerBitmap_UpdateAlphaBitmapParameters(bitmap, &params);
        CHECKF(hr == S_OK, "UpdateAlphaBitmapParameters (%#lx)", hr);
        memset(&params, 0x55, sizeof(params));
        IMFVideoMixerBitmap_GetAlphaBitmapParameters(bitmap, &params);
        CHECKF(params.fAlpha == 0.75f && params.nrcDest.right == 0.5f && params.nrcDest.left == 0.1f,
                "an update changes only the flagged member (%f, %f)", params.fAlpha, params.nrcDest.right);
        memset(&params, 0, sizeof(params));
        params.dwFlags = MFVideoAlphaBitmap_Alpha;
        params.fAlpha = -1.0f;
        hr = IMFVideoMixerBitmap_UpdateAlphaBitmapParameters(bitmap, &params);
        CHECKF(hr == E_INVALIDARG, "a negative alpha update is E_INVALIDARG (%#lx)", hr);
        hr = IMFVideoMixerBitmap_ClearAlphaBitmap(bitmap);
        CHECKF(hr == S_OK, "ClearAlphaBitmap (%#lx)", hr);
        hr = IMFVideoMixerBitmap_GetAlphaBitmapParameters(bitmap, &params);
        CHECKF(hr == MF_E_NOT_INITIALIZED, "after Clear the parameters are MF_E_NOT_INITIALIZED (%#lx)", hr);
        /* the mixer holds the surface only while the bitmap is set */
        IMFVideoMixerBitmap_SetAlphaBitmap(bitmap, &bm);
        IDirect3DSurface9_AddRef(surface);
        {
            ULONG with = IDirect3DSurface9_Release(surface);
            IMFVideoMixerBitmap_ClearAlphaBitmap(bitmap);
            IDirect3DSurface9_AddRef(surface);
            CHECKF(IDirect3DSurface9_Release(surface) == with - 1, "Clear releases the surface (%lu)", with);
        }
        IDirect3DSurface9_Release(surface);
    }
    IMFVideoMixerBitmap_Release(bitmap);

    IMFMediaType_Release(type);
    IMFMediaType_Release(sub_type);
    IDirect3DDeviceManager9_Release(manager);
}

int main(void)
{
    HRESULT (WINAPI *create_mixer)(IUnknown *, REFIID, REFIID, void **);
    IDirect3DDevice9 *device;
    IMFTransform *mixer = NULL;
    WNDCLASSA wc = {0};
    HRESULT hr;
    HWND window;

    CoInitialize(NULL);
    MFStartup(MF_VERSION, MFSTARTUP_FULL);
    wc.lpfnWndProc = wndproc;
    wc.lpszClassName = "sg_evr_mixer";
    RegisterClassA(&wc);
    window = CreateWindowA("sg_evr_mixer", "evr", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 0, 0, 640, 480, NULL, NULL, NULL, NULL);

    create_mixer = (void *)GetProcAddress(LoadLibraryA("evr.dll"), "MFCreateVideoMixer");
    hr = create_mixer ? create_mixer(NULL, &IID_IDirect3DDevice9, &IID_IMFTransform, (void **)&mixer) : E_FAIL;
    CHECKF(hr == S_OK, "MFCreateVideoMixer (%#lx)", hr);
    if (!mixer) { puts("RESULT: FAIL"); return 1; }

    test_untyped(mixer);
    test_mapper(mixer);
    if ((device = create_device(window)))
    {
        test_typed(mixer, device);
        IDirect3DDevice9_Release(device);
    }
    else
        puts("NOTE: no D3D device, the typed tests are skipped");

    IMFTransform_Release(mixer);
    MFShutdown();
    printf("%s\n", failures ? "RESULT: FAIL" : "RESULT: PASS");
    return failures != 0;
}
