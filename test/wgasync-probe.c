/* winegstreamer's sync reader stream selection (WMT_CLEANPOINT_ONLY) and
 * wmvcore's async reader transport (Pause/Resume, Start rate, StartAtPosition
 * in frames/timecode/playlist, SetProtectStreamSamples, IWMReaderAccelerator,
 * IWMReaderTypeNegotiation), run by test/wgasync-gate.sh on a copy of
 * test/wmv-sample.wmv.
 *
 *   wgasync-probe.exe C:/sample.wmv */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <wmsdk.h>
#include <nserror.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}
#define CHECKF(ok, ...) do { char _b[320]; snprintf(_b, sizeof(_b), __VA_ARGS__); check(ok, _b); } while (0)

struct callback
{
    IWMReaderCallback iface;
    LONG ref;
    HANDLE opened, started, eof;
    LONG samples[4];       /* per output */
    LONG started_count;
    HRESULT started_hr;
};
static struct callback *impl(IWMReaderCallback *i) { return CONTAINING_RECORD(i, struct callback, iface); }
static HRESULT WINAPI cb_QI(IWMReaderCallback *i, REFIID riid, void **ppv)
{
    if (IsEqualGUID(riid, &IID_IUnknown) || IsEqualGUID(riid, &IID_IWMStatusCallback) || IsEqualGUID(riid, &IID_IWMReaderCallback))
    { *ppv = i; IWMReaderCallback_AddRef(i); return S_OK; }
    *ppv = NULL; return E_NOINTERFACE;
}
static ULONG WINAPI cb_AddRef(IWMReaderCallback *i) { return InterlockedIncrement(&impl(i)->ref); }
static ULONG WINAPI cb_Release(IWMReaderCallback *i) { return InterlockedDecrement(&impl(i)->ref); }
static HRESULT WINAPI cb_OnStatus(IWMReaderCallback *i, WMT_STATUS s, HRESULT hr, WMT_ATTR_DATATYPE t, BYTE *v, void *ctx)
{
    struct callback *c = impl(i);
    if (s == WMT_OPENED) SetEvent(c->opened);
    if (s == WMT_STARTED) { c->started_hr = hr; InterlockedIncrement(&c->started_count); SetEvent(c->started); }
    if (s == WMT_EOF) SetEvent(c->eof);
    return S_OK;
}
static HRESULT WINAPI cb_OnSample(IWMReaderCallback *i, DWORD out, QWORD t, QWORD d, DWORD f, INSSBuffer *b, void *ctx)
{
    if (out < 4) InterlockedIncrement(&impl(i)->samples[out]);
    return S_OK;
}
static const IWMReaderCallbackVtbl cb_vtbl = {cb_QI, cb_AddRef, cb_Release, cb_OnStatus, cb_OnSample};

static void cb_init(struct callback *c)
{
    memset(c, 0, sizeof(*c));
    c->iface.lpVtbl = (IWMReaderCallbackVtbl *)&cb_vtbl;
    c->ref = 1;
    c->opened = CreateEventW(NULL, FALSE, FALSE, NULL);
    c->started = CreateEventW(NULL, FALSE, FALSE, NULL);
    c->eof = CreateEventW(NULL, FALSE, FALSE, NULL);
}
static LONG total(struct callback *c) { return c->samples[0] + c->samples[1] + c->samples[2] + c->samples[3]; }

static HRESULT open_retry(IWMReader *reader, const WCHAR *file, struct callback *cb)
{
    HRESULT hr = E_FAIL;
    int i;
    for (i = 0; i < 5; i++)
    {
        hr = IWMReader_Open(reader, file, &cb->iface, NULL);
        if (hr == S_OK && WaitForSingleObject(cb->opened, 5000) == WAIT_OBJECT_0) return S_OK;
        if (hr == S_OK) IWMReader_Close(reader);
        Sleep(1500);
    }
    return FAILED(hr) ? hr : E_FAIL;
}

static int output_of_type(IWMReader *reader, const GUID *major)
{
    DWORD i, count = 0;
    IWMReader_GetOutputCount(reader, &count);
    for (i = 0; i < count; i++)
    {
        IWMOutputMediaProps *props;
        GUID type;
        if (FAILED(IWMReader_GetOutputProps(reader, i, &props))) continue;
        IWMOutputMediaProps_GetType(props, &type);
        IWMOutputMediaProps_Release(props);
        if (IsEqualGUID(&type, major)) return i;
    }
    return -1;
}

/* ---- the sync reader: stream selection ---- */

struct sel_case { WMT_STREAM_SELECTION sel; HRESULT hr; };

static void sync_count(IWMSyncReader *r, WORD stream, LONG *all, LONG *clean)
{
    INSSBuffer *buf;
    QWORD pts, dur;
    DWORD flags, out;
    WORD num;
    HRESULT hr;

    *all = *clean = 0;
    while ((hr = IWMSyncReader_GetNextSample(r, stream, &buf, &pts, &dur, &flags, &out, &num)) == S_OK)
    {
        (*all)++;
        if (getenv("DUMP") && *all < 8) printf("  sample pts %I64u dur %I64u flags %lx out %lu\n", pts, dur, flags, out);
        if (flags & WM_SF_CLEANPOINT) (*clean)++;
        INSSBuffer_Release(buf);
    }
}

static void test_sync(const WCHAR *file)
{
    static const struct sel_case cases[] = {{WMT_OFF, S_OK}, {WMT_CLEANPOINT_ONLY, S_OK}, {WMT_ON, S_OK},
            {(WMT_STREAM_SELECTION)3, E_INVALIDARG}, {(WMT_STREAM_SELECTION)0x100, E_INVALIDARG}};
    HRESULT (WINAPI *create)(IUnknown *, DWORD, IWMSyncReader **) = (void *)GetProcAddress(GetModuleHandleW(L"wmvcore.dll"), "WMCreateSyncReader");
    IWMSyncReader *r = NULL;
    WMT_STREAM_SELECTION got;
    WORD vnum, one = 0;
    DWORD i, vout = ~0u, count = 0;
    LONG all_on, clean_on, all_co, clean_co;
    HRESULT hr;

    if (!create || FAILED(create(NULL, 0, &r))) { check(0, "WMCreateSyncReader"); return; }
    hr = IWMSyncReader_Open(r, file);
    if (hr != S_OK) { printf("SKIP  sync reader: sample not opened (%08lx)\n", hr); IWMSyncReader_Release(r); return; }
    IWMSyncReader_GetOutputCount(r, &count);
    for (i = 0; i < count; i++)
    {
        IWMOutputMediaProps *props;
        GUID type;
        IWMSyncReader_GetOutputProps(r, i, &props);
        IWMOutputMediaProps_GetType(props, &type);
        IWMOutputMediaProps_Release(props);
        if (IsEqualGUID(&type, &WMMEDIATYPE_Video)) vout = i;
    }
    CHECKF(vout != ~0u, "video output found (%lu)", vout);
    if (vout == ~0u) { IWMSyncReader_Release(r); return; }
    IWMSyncReader_GetStreamNumberForOutput(r, vout, &vnum);

    for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
    {
        WMT_STREAM_SELECTION before = WMT_ON;
        IWMSyncReader_GetStreamSelected(r, vnum, &before);
        hr = IWMSyncReader_SetStreamsSelected(r, 1, &vnum, (WMT_STREAM_SELECTION *)&cases[i].sel);
        CHECKF(hr == cases[i].hr, "SetStreamsSelected(%#x) = %08lx (%08lx)", cases[i].sel, cases[i].hr, hr);
        got = 0xdead;
        IWMSyncReader_GetStreamSelected(r, vnum, &got);
        CHECKF(got == (cases[i].hr == S_OK ? cases[i].sel : before), "selection after %#x is %#x", cases[i].sel, got);
    }

    /* the video stream alone, all samples, then key samples only; compressed
     * samples keep the key/delta distinction the decoder drops */
    got = WMT_ON;
    IWMSyncReader_SetStreamsSelected(r, 1, &vnum, &got);
    IWMSyncReader_SetRange(r, 0, 0);
    sync_count(r, vnum, &all_on, &clean_on);
    {
        LONG a2, c2;
        hr = IWMSyncReader2_SetRangeByFrame((IWMSyncReader2 *)r, vnum, 0, 3);
        sync_count(r, vnum, &a2, &c2);
        CHECKF(hr == S_OK && a2 >= 2 && a2 <= 3, "SetRangeByFrame(0, 3) (%08lx) delivers 2 or 3 samples (%ld)", hr, a2);
        IWMSyncReader_SetRange(r, 0, 0);
        sync_count(r, vnum, &a2, &c2);
        CHECKF(a2 == all_on, "SetRange(0, 0) after a frame range delivers everything again (%ld of %ld)", a2, all_on);
    }
    hr = IWMSyncReader_SetReadStreamSamples(r, vnum, TRUE);
    CHECKF(hr == S_OK, "SetReadStreamSamples(compressed) (%08lx)", hr);
    IWMSyncReader_SetRange(r, 0, 0);
    sync_count(r, vnum, &all_on, &clean_on);
    got = WMT_CLEANPOINT_ONLY;
    hr = IWMSyncReader_SetStreamsSelected(r, 1, &vnum, &got);
    IWMSyncReader_SetRange(r, 0, 0);
    sync_count(r, vnum, &all_co, &clean_co);
    CHECKF(all_on > 3, "video stream delivers samples (%ld)", all_on);
    CHECKF(clean_on >= 1, "the video has key samples (%ld of %ld)", clean_on, all_on);
    CHECKF(all_co == clean_on && clean_co == all_co, "CLEANPOINT_ONLY delivers %ld key samples only (%ld, %ld key)", clean_on, all_co, clean_co);
    (void)one;
    IWMSyncReader_Close(r);
    IWMSyncReader_Release(r);
}

/* ---- the async reader ---- */

struct rate_case { float rate; HRESULT hr; };

static void test_async(const WCHAR *file)
{
    static const struct rate_case rates[] = {{0.0f, E_INVALIDARG}, {-1.0f, NS_E_INVALID_REQUEST},
            {1.0f / 0.0f, E_INVALIDARG}, {0.0f / 0.0f, E_INVALIDARG}};
    HRESULT (WINAPI *create)(IUnknown *, DWORD, IWMReader **) = (void *)GetProcAddress(GetModuleHandleW(L"wmvcore.dll"), "WMCreateReader");
    IWMReader *reader = NULL;
    IWMReaderAdvanced6 *adv = NULL;
    IWMReaderAccelerator *accel = NULL;
    IWMReaderTypeNegotiation *neg = NULL;
    struct callback cb;
    HRESULT hr;
    DWORD i, t0, t1, t8;
    LONG c1, c2, c3;
    int vout, aout;

    create(NULL, 0, &reader);
    IWMReader_QueryInterface(reader, &IID_IWMReaderAdvanced6, (void **)&adv);
    IWMReader_QueryInterface(reader, &IID_IWMReaderAccelerator, (void **)&accel);
    IWMReader_QueryInterface(reader, &IID_IWMReaderTypeNegotiation, (void **)&neg);
    cb_init(&cb);

    hr = IWMReader_Pause(reader);
    CHECKF(hr == E_UNEXPECTED, "Pause before Open = E_UNEXPECTED (%08lx)", hr);
    hr = IWMReader_Resume(reader);
    CHECKF(hr == E_UNEXPECTED, "Resume before Open = E_UNEXPECTED (%08lx)", hr);

    if (open_retry(reader, file, &cb) != S_OK) { printf("SKIP  async reader: sample not opened\n"); goto done; }
    vout = output_of_type(reader, &WMMEDIATYPE_Video);
    aout = output_of_type(reader, &WMMEDIATYPE_Audio);
    CHECKF(vout >= 0 && aout >= 0, "outputs found (%d %d)", vout, aout);

    hr = IWMReader_Pause(reader);
    CHECKF(hr == NS_E_INVALID_REQUEST, "Pause before Start = NS_E_INVALID_REQUEST (%08lx)", hr);
    hr = IWMReader_Resume(reader);
    CHECKF(hr == NS_E_INVALID_REQUEST, "Resume before Start = NS_E_INVALID_REQUEST (%08lx)", hr);
    for (i = 0; i < sizeof(rates) / sizeof(rates[0]); i++)
    {
        hr = IWMReader_Start(reader, 0, 0, rates[i].rate, NULL);
        CHECKF(hr == rates[i].hr, "Start(rate %g) = %08lx (%08lx)", rates[i].rate, rates[i].hr, hr);
    }

    /* the TypeNegotiation and Accelerator objects */
    CHECKF(accel && neg, "IWMReaderAccelerator and IWMReaderTypeNegotiation exist");
    if (accel)
    {
        void *codec = (void *)1;
        hr = IWMReaderAccelerator_GetCodecInterface(accel, 0, &IID_IUnknown, &codec);
        CHECKF(hr == E_NOINTERFACE && !codec, "GetCodecInterface(0) = E_NOINTERFACE, NULL out (%08lx %p)", hr, codec);
        codec = (void *)1;
        hr = IWMReaderAccelerator_GetCodecInterface(accel, 99, &IID_IUnknown, &codec);
        CHECKF(hr == E_INVALIDARG && !codec, "GetCodecInterface(99) = E_INVALIDARG (%08lx)", hr);
        hr = IWMReaderAccelerator_GetCodecInterface(accel, 0, &IID_IUnknown, NULL);
        CHECKF(hr == E_POINTER, "GetCodecInterface(NULL) = E_POINTER (%08lx)", hr);
        hr = IWMReaderAccelerator_Notify(accel, 0, NULL);
        CHECKF(hr == E_POINTER, "Notify(NULL type) = E_POINTER (%08lx)", hr);
        {
            WM_MEDIA_TYPE mt = {0};
            hr = IWMReaderAccelerator_Notify(accel, 0, &mt);
            CHECKF(hr == S_OK, "Notify(0) = S_OK (%08lx)", hr);
            hr = IWMReaderAccelerator_Notify(accel, 99, &mt);
            CHECKF(hr == E_INVALIDARG, "Notify(99) = E_INVALIDARG (%08lx)", hr);
        }
    }
    if (neg && vout >= 0)
    {
        IWMOutputMediaProps *props = NULL, *now = NULL;
        GUID sub0, sub1;
        DWORD idx = 0, n = 0;
        IWMReader_GetOutputFormatCount(reader, vout, &n);
        CHECKF(n >= 2, "video output has formats (%lu)", n);
        hr = IWMReaderTypeNegotiation_TryOutputProps(neg, vout, NULL);
        CHECKF(hr == E_POINTER, "TryOutputProps(NULL) = E_POINTER (%08lx)", hr);
        IWMReader_GetOutputProps(reader, vout, &now);
        IWMOutputMediaProps_GetMediaType(now, NULL, &i);
        IWMOutputMediaProps_Release(now);
        for (idx = 0; idx < n; idx++)
        {
            IWMReader_GetOutputFormat(reader, vout, idx, &props);
            hr = IWMReaderTypeNegotiation_TryOutputProps(neg, vout, props);
            IWMReader_GetOutputProps(reader, vout, &now);
            IWMOutputMediaProps_GetMediaType(props, NULL, &i);
            {
                WM_MEDIA_TYPE *a = malloc(i), *b;
                DWORD j = i;
                IWMOutputMediaProps_GetMediaType(props, a, &j);
                IWMOutputMediaProps_GetMediaType(now, NULL, &j);
                b = malloc(j);
                IWMOutputMediaProps_GetMediaType(now, b, &j);
                sub0 = a->subtype; sub1 = b->subtype;
                CHECKF(hr == S_OK && IsEqualGUID(&sub0, &sub1), "TryOutputProps(format %lu) = S_OK and is the output type (%08lx)", idx, hr);
                free(a); free(b);
            }
            IWMOutputMediaProps_Release(now);
            IWMOutputMediaProps_Release(props);
        }
        IWMReader_GetOutputFormat(reader, vout, 0, &props);
        hr = IWMReaderTypeNegotiation_TryOutputProps(neg, 99, props);
        IWMOutputMediaProps_Release(props);
        CHECKF(hr == E_INVALIDARG, "TryOutputProps(output 99) = E_INVALIDARG (%08lx)", hr);
    }

    /* DRM */
    {
        BYTE cert[4] = {1, 2, 3, 4}, iv[16];
        DWORD ivsize = sizeof(iv);
        hr = IWMReaderAdvanced6_SetProtectStreamSamples(adv, NULL, 0, 0, 0, iv, &ivsize);
        CHECKF(hr == E_INVALIDARG, "SetProtectStreamSamples(NULL cert) = E_INVALIDARG (%08lx)", hr);
        hr = IWMReaderAdvanced6_SetProtectStreamSamples(adv, cert, 4, 0, 0, iv, &ivsize);
        CHECKF(hr == E_NOTIMPL, "SetProtectStreamSamples without a DRM engine = E_NOTIMPL (%08lx)", hr);
    }

    /* rate: the whole file at 1x against 8x */
    ResetEvent(cb.eof); ResetEvent(cb.started);
    t0 = GetTickCount();
    hr = IWMReader_Start(reader, 0, 0, 1.0f, NULL);
    WaitForSingleObject(cb.eof, 30000);
    t1 = GetTickCount() - t0;
    if (getenv("DUMP")) printf("  1x: %lu ms, samples %ld %ld, started_hr %08lx\n", t1, cb.samples[0], cb.samples[1], cb.started_hr);
    IWMReader_Stop(reader);
    Sleep(200);
    ResetEvent(cb.eof); ResetEvent(cb.started);
    t0 = GetTickCount();
    hr = IWMReader_Start(reader, 0, 0, 8.0f, NULL);
    CHECKF(hr == S_OK, "Start(8x) (%08lx)", hr);
    WaitForSingleObject(cb.eof, 30000);
    t8 = GetTickCount() - t0;
    IWMReader_Stop(reader);
    CHECKF(t1 > 800 && t8 * 3 < t1 * 2, "8x finishes much sooner than 1x (%lu ms, %lu ms)", t1, t8);

    /* Pause / Resume */
    Sleep(200);
    ResetEvent(cb.eof); ResetEvent(cb.started);
    memset(cb.samples, 0, sizeof(cb.samples));
    hr = IWMReader_Start(reader, 0, 0, 1.0f, NULL);
    WaitForSingleObject(cb.started, 5000);
    Sleep(100);
    hr = IWMReader_Pause(reader);
    CHECKF(hr == S_OK, "Pause (%08lx)", hr);
    hr = IWMReader_Pause(reader);
    CHECKF(hr == S_OK, "Pause again (%08lx)", hr);
    Sleep(300);
    c1 = total(&cb);
    Sleep(1200);
    c2 = total(&cb);
    CHECKF(c1 == c2 && WaitForSingleObject(cb.eof, 0) == WAIT_TIMEOUT, "no samples while paused (%ld, %ld)", c1, c2);
    t0 = GetTickCount();
    hr = IWMReader_Resume(reader);
    CHECKF(hr == S_OK, "Resume (%08lx)", hr);
    hr = IWMReader_Resume(reader);
    CHECKF(hr == S_OK, "Resume again (%08lx)", hr);
    CHECKF(WaitForSingleObject(cb.eof, 20000) == WAIT_OBJECT_0, "the stream ends after Resume");
    t1 = GetTickCount() - t0;
    /* the 1.5 s spent paused must not count: the rest of the 2.4 s file is still paced */
    CHECKF(t1 > 1500, "the paused time is not counted against the timestamps (%lu ms after Resume)", t1);
    c3 = total(&cb);
    CHECKF(c3 > c2, "samples after Resume (%ld > %ld)", c3, c2);
    /* Stop ends a pause */
    ResetEvent(cb.eof); ResetEvent(cb.started);
    IWMReader_Start(reader, 0, 0, 1.0f, NULL);
    WaitForSingleObject(cb.started, 5000);
    IWMReader_Pause(reader);
    hr = IWMReader_Stop(reader);
    CHECKF(hr == S_OK, "Stop while paused (%08lx)", hr);
    hr = IWMReader_Pause(reader);
    CHECKF(hr == NS_E_INVALID_REQUEST, "Pause after Stop = NS_E_INVALID_REQUEST (%08lx)", hr);
    /* StartAtPosition: playlist and timecode formats, frames on audio/video/bad streams */
    {
        QWORD start = 0, len = 3;
        WMT_TIMECODE_EXTENSION_DATA tc[2] = {{0}};
        hr = IWMReaderAdvanced6_StartAtPosition(adv, 0, &start, &len, WMT_OFFSET_FORMAT_PLAYLIST_OFFSET, 1.0f, NULL);
        CHECKF(hr == NS_E_INVALID_REQUEST, "StartAtPosition(playlist) = NS_E_INVALID_REQUEST (%08lx)", hr);
        hr = IWMReaderAdvanced6_StartAtPosition(adv, 0, &tc[0], NULL, WMT_OFFSET_FORMAT_TIMECODE, 1.0f, NULL);
        CHECKF(hr == E_INVALIDARG, "StartAtPosition(timecode, no end) = E_INVALIDARG (%08lx)", hr);
        ResetEvent(cb.started);
        hr = IWMReaderAdvanced6_StartAtPosition(adv, vout + 1, &tc[0], &tc[1], WMT_OFFSET_FORMAT_TIMECODE, 1.0f, NULL);
        CHECKF(hr == S_OK && WaitForSingleObject(cb.started, 5000) == WAIT_OBJECT_0 && cb.started_hr == NS_E_INVALID_REQUEST,
               "StartAtPosition(timecode) on a stream without timecodes starts with NS_E_INVALID_REQUEST (%08lx %08lx)", hr, cb.started_hr);
        IWMReader_Stop(reader);
        ResetEvent(cb.started);
        hr = IWMReaderAdvanced6_StartAtPosition(adv, aout + 1, &start, &len, WMT_OFFSET_FORMAT_FRAME_NUMBERS, 1.0f, NULL);
        CHECKF(hr == S_OK && WaitForSingleObject(cb.started, 5000) == WAIT_OBJECT_0 && cb.started_hr == NS_E_INVALID_REQUEST,
               "frames on the audio stream: started with NS_E_INVALID_REQUEST (%08lx %08lx)", hr, cb.started_hr);
        IWMReader_Stop(reader);
        ResetEvent(cb.started);
        hr = IWMReaderAdvanced6_StartAtPosition(adv, 77, &start, &len, WMT_OFFSET_FORMAT_FRAME_NUMBERS, 1.0f, NULL);
        CHECKF(hr == S_OK && WaitForSingleObject(cb.started, 5000) == WAIT_OBJECT_0 && cb.started_hr == E_INVALIDARG,
               "frames on stream 77: started with E_INVALIDARG (%08lx %08lx)", hr, cb.started_hr);
        IWMReader_Stop(reader);
        Sleep(200);
        memset(cb.samples, 0, sizeof(cb.samples));
        ResetEvent(cb.started); ResetEvent(cb.eof);
        hr = IWMReaderAdvanced6_StartAtPosition(adv, vout + 1, &start, &len, WMT_OFFSET_FORMAT_FRAME_NUMBERS, 1.0f, NULL);
        CHECKF(hr == S_OK && WaitForSingleObject(cb.started, 5000) == WAIT_OBJECT_0 && cb.started_hr == S_OK,
               "frames on the video stream start (%08lx %08lx)", hr, cb.started_hr);
        CHECKF(WaitForSingleObject(cb.eof, 20000) == WAIT_OBJECT_0, "the frame range ends");
        if (getenv("DUMP")) printf("  frames: v %ld a %ld\n", cb.samples[0], cb.samples[1]);
        /* frames 0..2 span 120 ms; the first video sample sits at 46 ms */
        CHECKF(cb.samples[vout] >= 2 && cb.samples[vout] <= 3, "video frames 0..3 deliver 2 or 3 samples (%ld)", cb.samples[vout]);
        CHECKF(cb.samples[aout] > 0 && cb.samples[aout] < 20, "the audio of the range is cut too (%ld)", cb.samples[aout]);
        IWMReader_Stop(reader);
        WaitForSingleObject(cb.started, 0);
    }

    IWMReader_Close(reader);
done:
    if (accel) IWMReaderAccelerator_Release(accel);
    if (neg) IWMReaderTypeNegotiation_Release(neg);
    if (adv) IWMReaderAdvanced6_Release(adv);
    IWMReader_Release(reader);
}

int main(int argc, char **argv)
{
    WCHAR file[MAX_PATH];

    CoInitialize(NULL);
    LoadLibraryW(L"wmvcore.dll");
    if (argc < 2 || !MultiByteToWideChar(CP_ACP, 0, argv[1], -1, file, MAX_PATH)) { printf("SKIP  no sample file\n"); goto out; }
    test_sync(file);
    test_async(file);
out:
    CoUninitialize();
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures ? 1 : 0;
}
