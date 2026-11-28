/* avirgb16-probe: the AVI splitter hands out 16-bit (and 1/4-bit) uncompressed video
 * (patches/sg/9413).
 *
 * GStreamer's AVI demuxer has no caps for a 16-bit BI_RGB or BI_BITFIELDS stream (it says
 * video/x-avi-unknown), so Wine's AVI splitter had no media type for it and RenderFile failed
 * with VFW_E_CANNOT_RENDER. Windows' AVI splitter gives such a stream the subtype of its bitmap
 * header (RGB555 for BI_RGB, RGB565 or RGB555 by the BI_BITFIELDS masks), a VIDEOINFOHEADER that
 * carries the stream's own header, and the frames as they are in the file.
 *
 * The probe writes two AVIs (RGB555 BI_RGB, RGB565 BI_BITFIELDS; 3 frames of a known pattern),
 * reads each through File Source (Async) -> AVI Splitter -> Sample Grabber -> Null Renderer and
 * checks the splitter's media type, the frames' bytes and times; then RenderFile on each.
 *
 * Prints name=value lines; see test/avirgb16-gate.sh.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#define COBJMACROS
#define CONST_VTABLE
#include <windows.h>
#include <initguid.h>
#include <dshow.h>
#include <qedit.h>
#include <stdio.h>
#include <string.h>

DEFINE_GUID(p_CLSID_SampleGrabber, 0xc1f400a0, 0x3f08, 0x11d3, 0x9f, 0x0b, 0x00, 0x60, 0x08, 0x03, 0x9e, 0x37);
DEFINE_GUID(p_CLSID_NullRenderer, 0xc1f400a4, 0x3f08, 0x11d3, 0x9f, 0x0b, 0x00, 0x60, 0x08, 0x03, 0x9e, 0x37);
DEFINE_GUID(p_IID_ISampleGrabber, 0x6b652fff, 0x11fe, 0x4fce, 0x92, 0xad, 0x02, 0x66, 0xb5, 0xd7, 0xc7, 0x8f);
DEFINE_GUID(p_IID_ISampleGrabberCB, 0x0579154a, 0x2b53, 0x4994, 0xb0, 0xd0, 0xe7, 0x73, 0x14, 0x8e, 0xff, 0x85);

#define W 32
#define H 8
#define FRAMES 3
#define RATE 10

static void say(const char *name, int ok, const char *fmt, ...)
{
    char buf[512];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    printf("%s=%d %s\n", name, ok ? 1 : 0, buf);
    fflush(stdout);
}

/* pixel at row r of the file's bottom-up rows */
static WORD pixel(int frame, int x, int r, BOOL is565)
{
    if (is565) return (WORD)(((x & 31) << 11) | ((r * 7 + frame) & 63) << 5 | ((x + r) & 31));
    return (WORD)(((x & 31) << 10) | ((r * 3 + frame) & 31) << 5 | ((x + r) & 31));
}

static void put32(BYTE **p, DWORD v) { memcpy(*p, &v, 4); *p += 4; }
static void put16(BYTE **p, WORD v) { memcpy(*p, &v, 2); *p += 2; }
static void putcc(BYTE **p, const char *cc) { memcpy(*p, cc, 4); *p += 4; }

static BOOL write_avi(const char *path, BOOL is565)
{
    static BYTE file[16384];
    DWORD frame_size = W * H * 2, strf_size = is565 ? 52 : 40;
    BYTE *p = file, *riff_size, *hdrl_size, *strl_size, *movi_size, *movi, *frames[FRAMES];
    DWORD written;
    HANDLE h;
    int f, x, r;

    putcc(&p, "RIFF"); riff_size = p; put32(&p, 0); putcc(&p, "AVI ");
    putcc(&p, "LIST"); hdrl_size = p; put32(&p, 0); putcc(&p, "hdrl");
    putcc(&p, "avih"); put32(&p, 56);
    put32(&p, 1000000 / RATE); put32(&p, frame_size * RATE); put32(&p, 0); put32(&p, 0x10 /* HASINDEX */);
    put32(&p, FRAMES); put32(&p, 0); put32(&p, 1); put32(&p, frame_size);
    put32(&p, W); put32(&p, H); put32(&p, 0); put32(&p, 0); put32(&p, 0); put32(&p, 0);
    putcc(&p, "LIST"); strl_size = p; put32(&p, 0); putcc(&p, "strl");
    putcc(&p, "strh"); put32(&p, 56);
    putcc(&p, "vids"); put32(&p, 0); put32(&p, 0); put16(&p, 0); put16(&p, 0); put32(&p, 0);
    put32(&p, 1); put32(&p, RATE); put32(&p, 0); put32(&p, FRAMES); put32(&p, frame_size);
    put32(&p, 0xffffffff); put32(&p, 0);
    put16(&p, 0); put16(&p, 0); put16(&p, W); put16(&p, H);
    putcc(&p, "strf"); put32(&p, strf_size);
    put32(&p, 40); put32(&p, W); put32(&p, H); put16(&p, 1); put16(&p, 16);
    put32(&p, is565 ? BI_BITFIELDS : BI_RGB); put32(&p, frame_size); put32(&p, 0); put32(&p, 0);
    put32(&p, 0); put32(&p, 0);
    if (is565) { put32(&p, 0xf800); put32(&p, 0x07e0); put32(&p, 0x001f); }
    *(DWORD *)strl_size = p - strl_size - 4;
    *(DWORD *)hdrl_size = p - hdrl_size - 4;
    putcc(&p, "LIST"); movi_size = p; put32(&p, 0); movi = p; putcc(&p, "movi");
    for (f = 0; f < FRAMES; f++)
    {
        frames[f] = p;
        putcc(&p, "00db"); put32(&p, frame_size);
        for (r = 0; r < H; r++)
            for (x = 0; x < W; x++)
                put16(&p, pixel(f, x, r, is565));
    }
    *(DWORD *)movi_size = p - movi_size - 4;
    putcc(&p, "idx1"); put32(&p, FRAMES * 16);
    for (f = 0; f < FRAMES; f++)
    {
        putcc(&p, "00db"); put32(&p, 0x10); put32(&p, frames[f] - movi); put32(&p, frame_size);
    }
    *(DWORD *)riff_size = p - riff_size - 4;

    h = CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) return FALSE;
    WriteFile(h, file, p - file, &written, NULL);
    CloseHandle(h);
    return written == (DWORD)(p - file);
}

/* ---- a sample grabber callback that keeps every frame ---- */
static BYTE got[FRAMES + 2][W * H * 2];
static LONG got_len[FRAMES + 2];
static REFERENCE_TIME got_start[FRAMES + 2], got_stop[FRAMES + 2];
static int got_count;

static HRESULT WINAPI cb_QueryInterface(ISampleGrabberCB *iface, REFIID iid, void **out)
{
    if (IsEqualGUID(iid, &IID_IUnknown) || IsEqualGUID(iid, &p_IID_ISampleGrabberCB)) { *out = iface; return S_OK; }
    *out = NULL; return E_NOINTERFACE;
}
static ULONG WINAPI cb_AddRef(ISampleGrabberCB *iface) { return 2; }
static ULONG WINAPI cb_Release(ISampleGrabberCB *iface) { return 1; }
static HRESULT WINAPI cb_SampleCB(ISampleGrabberCB *iface, double time, IMediaSample *sample)
{
    BYTE *data;
    LONG len;
    if (got_count >= FRAMES + 2) return S_OK;
    IMediaSample_GetPointer(sample, &data);
    len = IMediaSample_GetActualDataLength(sample);
    got_len[got_count] = len;
    memcpy(got[got_count], data, min(len, (LONG)sizeof(got[0])));
    got_start[got_count] = got_stop[got_count] = -1;
    IMediaSample_GetTime(sample, &got_start[got_count], &got_stop[got_count]);
    got_count++;
    return S_OK;
}
static HRESULT WINAPI cb_BufferCB(ISampleGrabberCB *iface, double time, BYTE *buf, LONG len) { return S_OK; }
static const ISampleGrabberCBVtbl cb_vtbl = { cb_QueryInterface, cb_AddRef, cb_Release, cb_SampleCB, cb_BufferCB };
static ISampleGrabberCB grab_cb = { &cb_vtbl };

static IPin *get_pin(IBaseFilter *filter, PIN_DIRECTION want)
{
    IEnumPins *e;
    IPin *pin;
    PIN_DIRECTION dir;
    if (FAILED(IBaseFilter_EnumPins(filter, &e))) return NULL;
    while (IEnumPins_Next(e, 1, &pin, NULL) == S_OK)
    {
        IPin_QueryDirection(pin, &dir);
        if (dir == want) { IEnumPins_Release(e); return pin; }
        IPin_Release(pin);
    }
    IEnumPins_Release(e);
    return NULL;
}

static void run_file(const char *tag, const WCHAR *wpath, BOOL is565)
{
    IGraphBuilder *graph;
    IBaseFilter *source, *splitter, *grabber, *nullr;
    IPin *src_out, *split_in, *split_out, *grab_in, *grab_out, *null_in;
    ISampleGrabber *sg;
    IMediaControl *control;
    IMediaEvent *event;
    AM_MEDIA_TYPE mt, grab_mt;
    char name[64];
    HRESULT hr;
    LONG code;
    int f, ok, x, r;

    hr = CoCreateInstance(&CLSID_FilterGraph, NULL, CLSCTX_INPROC_SERVER, &IID_IGraphBuilder, (void **)&graph);
    if (FAILED(hr)) { say(tag, 0, "no filter graph %#lx", hr); return; }
    hr = IGraphBuilder_AddSourceFilter(graph, wpath, L"source", &source);
    if (FAILED(hr)) { say(tag, 0, "AddSourceFilter %#lx", hr); return; }
    CoCreateInstance(&CLSID_AviSplitter, NULL, CLSCTX_INPROC_SERVER, &IID_IBaseFilter, (void **)&splitter);
    IGraphBuilder_AddFilter(graph, splitter, L"splitter");
    src_out = get_pin(source, PINDIR_OUTPUT);
    split_in = get_pin(splitter, PINDIR_INPUT);
    hr = IGraphBuilder_ConnectDirect(graph, src_out, split_in, NULL);
    snprintf(name, sizeof(name), "%s_splitter", tag);
    say(name, hr == S_OK, "source -> AVI splitter %#lx", hr);
    split_out = get_pin(splitter, PINDIR_OUTPUT);
    snprintf(name, sizeof(name), "%s_pin", tag);
    say(name, split_out != NULL, "the splitter has an output pin");
    if (!split_out) return;

    /* the splitter's media type */
    {
        IEnumMediaTypes *e;
        AM_MEDIA_TYPE *pmt = NULL;
        IPin_EnumMediaTypes(split_out, &e);
        hr = IEnumMediaTypes_Next(e, 1, &pmt, NULL);
        IEnumMediaTypes_Release(e);
        snprintf(name, sizeof(name), "%s_type", tag);
        if (hr != S_OK || !pmt) { say(name, 0, "no media type %#lx", hr); return; }
        {
            const VIDEOINFOHEADER *vih = (const VIDEOINFOHEADER *)pmt->pbFormat;
            const GUID *want = is565 ? &MEDIASUBTYPE_RGB565 : &MEDIASUBTYPE_RGB555;
            ok = IsEqualGUID(&pmt->majortype, &MEDIATYPE_Video) && IsEqualGUID(&pmt->subtype, want)
                    && IsEqualGUID(&pmt->formattype, &FORMAT_VideoInfo)
                    && pmt->cbFormat >= sizeof(VIDEOINFOHEADER) + (is565 ? 12 : 0);
            say(name, ok, "major %s sub %s format %s cb %lu", IsEqualGUID(&pmt->majortype, &MEDIATYPE_Video) ? "video" : "?",
                    IsEqualGUID(&pmt->subtype, &MEDIASUBTYPE_RGB565) ? "RGB565" :
                    IsEqualGUID(&pmt->subtype, &MEDIASUBTYPE_RGB555) ? "RGB555" : "other",
                    IsEqualGUID(&pmt->formattype, &FORMAT_VideoInfo) ? "VideoInfo" : "other", pmt->cbFormat);
            if (ok)
            {
                const DWORD *masks = (const DWORD *)(vih + 1);
                snprintf(name, sizeof(name), "%s_header", tag);
                ok = vih->bmiHeader.biWidth == W && vih->bmiHeader.biHeight == H && vih->bmiHeader.biBitCount == 16
                        && vih->bmiHeader.biCompression == (is565 ? BI_BITFIELDS : BI_RGB)
                        && vih->bmiHeader.biSizeImage == W * H * 2 && vih->AvgTimePerFrame == 10000000 / RATE
                        && (!is565 || (masks[0] == 0xf800 && masks[1] == 0x07e0 && masks[2] == 0x001f));
                say(name, ok, "%ldx%ld bpp %u comp %lu size %lu frame %I64d", vih->bmiHeader.biWidth, vih->bmiHeader.biHeight,
                        vih->bmiHeader.biBitCount, vih->bmiHeader.biCompression, vih->bmiHeader.biSizeImage,
                        vih->AvgTimePerFrame);
                snprintf(name, sizeof(name), "%s_fixed", tag);
                say(name, pmt->bFixedSizeSamples && pmt->lSampleSize == W * H * 2 && !pmt->bTemporalCompression,
                        "fixed %d size %lu temporal %d", pmt->bFixedSizeSamples, pmt->lSampleSize, pmt->bTemporalCompression);
            }
        }
        mt = *pmt;
    }

    /* the frames */
    CoCreateInstance(&p_CLSID_SampleGrabber, NULL, CLSCTX_INPROC_SERVER, &IID_IBaseFilter, (void **)&grabber);
    CoCreateInstance(&p_CLSID_NullRenderer, NULL, CLSCTX_INPROC_SERVER, &IID_IBaseFilter, (void **)&nullr);
    IGraphBuilder_AddFilter(graph, grabber, L"grabber");
    IGraphBuilder_AddFilter(graph, nullr, L"null");
    IBaseFilter_QueryInterface(grabber, &p_IID_ISampleGrabber, (void **)&sg);
    memset(&grab_mt, 0, sizeof(grab_mt));
    grab_mt.majortype = MEDIATYPE_Video;
    grab_mt.subtype = is565 ? MEDIASUBTYPE_RGB565 : MEDIASUBTYPE_RGB555;
    ISampleGrabber_SetMediaType(sg, &grab_mt);
    ISampleGrabber_SetCallback(sg, &grab_cb, 0);
    grab_in = get_pin(grabber, PINDIR_INPUT);
    grab_out = get_pin(grabber, PINDIR_OUTPUT);
    null_in = get_pin(nullr, PINDIR_INPUT);
    hr = IGraphBuilder_ConnectDirect(graph, split_out, grab_in, &mt);
    snprintf(name, sizeof(name), "%s_connect", tag);
    say(name, hr == S_OK, "splitter -> sample grabber %#lx", hr);
    if (hr != S_OK) return;
    IGraphBuilder_ConnectDirect(graph, grab_out, null_in, NULL);
    IGraphBuilder_QueryInterface(graph, &IID_IMediaControl, (void **)&control);
    IGraphBuilder_QueryInterface(graph, &IID_IMediaEvent, (void **)&event);
    got_count = 0;
    IFilterGraph_SetDefaultSyncSource((IFilterGraph *)graph);
    {
        IMediaFilter *mf;
        IGraphBuilder_QueryInterface(graph, &IID_IMediaFilter, (void **)&mf);
        IMediaFilter_SetSyncSource(mf, NULL);   /* as fast as it can */
        IMediaFilter_Release(mf);
    }
    hr = IMediaControl_Run(control);
    code = 0;
    hr = IMediaEvent_WaitForCompletion(event, 10000, &code);
    IMediaControl_Stop(control);
    snprintf(name, sizeof(name), "%s_frames", tag);
    say(name, got_count == FRAMES && code == EC_COMPLETE, "%d frames, completion %#lx code %ld", got_count, hr, code);
    for (f = 0, ok = 1; f < got_count && f < FRAMES; f++)
    {
        if (got_len[f] != W * H * 2) { ok = 0; break; }
        for (r = 0; r < H && ok; r++)
            for (x = 0; x < W && ok; x++)
            {
                WORD v;
                memcpy(&v, &got[f][(r * W + x) * 2], 2);
                if (v != pixel(f, x, r, is565))
                {
                    printf("  frame %d (%d,%d): %04x, want %04x\n", f, x, r, v, pixel(f, x, r, is565));
                    ok = 0;
                }
            }
    }
    snprintf(name, sizeof(name), "%s_bytes", tag);
    say(name, ok && got_count == FRAMES, "frames as written in the file (len %ld)", got_count ? got_len[0] : 0);
    snprintf(name, sizeof(name), "%s_times", tag);
    ok = got_count == FRAMES;
    for (f = 0; f < got_count && f < FRAMES; f++)
        if (got_start[f] != f * (10000000 / RATE) || got_stop[f] != (f + 1) * (10000000 / RATE)) ok = 0;
    say(name, ok, "%I64d-%I64d %I64d-%I64d", got_start[0], got_stop[0], got_count > 1 ? got_start[1] : -1,
            got_count > 1 ? got_stop[1] : -1);
    IMediaControl_Release(control);
    IMediaEvent_Release(event);
    IGraphBuilder_Release(graph);
}

static void render_file(const char *tag, const WCHAR *wpath)
{
    IGraphBuilder *graph;
    char name[64];
    HRESULT hr;

    CoCreateInstance(&CLSID_FilterGraph, NULL, CLSCTX_INPROC_SERVER, &IID_IGraphBuilder, (void **)&graph);
    hr = IGraphBuilder_RenderFile(graph, wpath, NULL);
    snprintf(name, sizeof(name), "%s_render", tag);
    say(name, hr == S_OK, "RenderFile %#lx", hr);
    IGraphBuilder_Release(graph);
}

int main(void)
{
    CoInitialize(NULL);
    say("write", write_avi("C:\\rgb555.avi", FALSE) && write_avi("C:\\rgb565.avi", TRUE), "two AVIs written");
    run_file("rgb555", L"C:\\rgb555.avi", FALSE);
    run_file("rgb565", L"C:\\rgb565.avi", TRUE);
    render_file("rgb555", L"C:\\rgb555.avi");
    render_file("rgb565", L"C:\\rgb565.avi");
    CoUninitialize();
    return 0;
}
