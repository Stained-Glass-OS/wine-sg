/* wmvcore's async reader operations (patches/sg/2855), run by
 * test/wmv-reader-gate.sh against test/wmv-sample.wmv: the per-output
 * settings of IWMReaderAdvanced2 (the table of what an audio and a video
 * output accept), the answers about the source (buffer and download
 * progress, protocol, URL, save-as, preroll), max output sample size,
 * languages, player hooks, StartAtMarker/StartAtPosition and the default
 * sample allocator.
 *
 *   wmv-reader-probe.exe C:/sample.wmv */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <wmsdk.h>
#include <nserror.h>
#include <stdio.h>
#include <string.h>

#ifndef ASF_E_BUFFERTOOSMALL
#define ASF_E_BUFFERTOOSMALL ((HRESULT)0xc00d07d1)
#endif
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}
#define CHECKF(ok, ...) do { char _b[320]; snprintf(_b, sizeof(_b), __VA_ARGS__); check(ok, _b); } while (0)

/* ---- callback ---- */

struct callback
{
    IWMReaderCallback IWMReaderCallback_iface;
    IWMReaderCallbackAdvanced IWMReaderCallbackAdvanced_iface;
    LONG ref;
    BOOL no_advanced;
    HANDLE opened, started;
    LONG started_count, sample_count, bad_buffers;
    void *started_context;
    BOOL check_buffers;
};

static struct callback *impl_cb(IWMReaderCallback *i) { return CONTAINING_RECORD(i, struct callback, IWMReaderCallback_iface); }
static struct callback *impl_cba(IWMReaderCallbackAdvanced *i) { return CONTAINING_RECORD(i, struct callback, IWMReaderCallbackAdvanced_iface); }
static HRESULT WINAPI cb_QI(IWMReaderCallback *i, REFIID riid, void **ppv)
{
    struct callback *c = impl_cb(i);
    if (IsEqualGUID(riid, &IID_IUnknown) || IsEqualGUID(riid, &IID_IWMStatusCallback) || IsEqualGUID(riid, &IID_IWMReaderCallback))
        *ppv = &c->IWMReaderCallback_iface;
    else if (IsEqualGUID(riid, &IID_IWMReaderCallbackAdvanced) && !c->no_advanced)
        *ppv = &c->IWMReaderCallbackAdvanced_iface;
    else { *ppv = NULL; return E_NOINTERFACE; }
    IUnknown_AddRef((IUnknown *)*ppv);
    return S_OK;
}
static ULONG WINAPI cb_AddRef(IWMReaderCallback *i) { return InterlockedIncrement(&impl_cb(i)->ref); }
static ULONG WINAPI cb_Release(IWMReaderCallback *i) { return InterlockedDecrement(&impl_cb(i)->ref); }
static HRESULT WINAPI cb_OnStatus(IWMReaderCallback *i, WMT_STATUS status, HRESULT hr, WMT_ATTR_DATATYPE type, BYTE *value, void *context)
{
    struct callback *c = impl_cb(i);
    if (status == WMT_OPENED) SetEvent(c->opened);
    if (status == WMT_STARTED)
    {
        c->started_context = context;
        InterlockedIncrement(&c->started_count);
        SetEvent(c->started);
    }
    return S_OK;
}
static HRESULT WINAPI cb_OnSample(IWMReaderCallback *i, DWORD output, QWORD time, QWORD duration, DWORD flags, INSSBuffer *sample, void *context)
{
    struct callback *c = impl_cb(i);
    if (c->check_buffers)
    {
        DWORD len = 0, max = 0, len2 = 0;
        BYTE *data = NULL, *data2 = NULL;
        INSSBuffer *other = NULL;
        BOOL ok = SUCCEEDED(INSSBuffer_GetLength(sample, &len)) && SUCCEEDED(INSSBuffer_GetMaxLength(sample, &max))
                && len <= max && len > 0 && SUCCEEDED(INSSBuffer_GetBufferAndLength(sample, &data, &len2)) && len2 == len && data
                && SUCCEEDED(INSSBuffer_GetBuffer(sample, &data2)) && data2 == data
                && SUCCEEDED(INSSBuffer_QueryInterface(sample, &IID_INSSBuffer, (void **)&other));
        if (other) INSSBuffer_Release(other);
        if (ok && max < 0xffffffff && INSSBuffer_SetLength(sample, max + 1) == S_OK) ok = FALSE; /* too long must fail */
        if (ok && INSSBuffer_SetLength(sample, len) != S_OK) ok = FALSE;
        if (!ok) InterlockedIncrement(&c->bad_buffers);
    }
    InterlockedIncrement(&c->sample_count);
    return S_OK;
}
static const IWMReaderCallbackVtbl cb_vtbl = {cb_QI, cb_AddRef, cb_Release, cb_OnStatus, cb_OnSample};

static HRESULT WINAPI cba_QI(IWMReaderCallbackAdvanced *i, REFIID riid, void **ppv) { return cb_QI(&impl_cba(i)->IWMReaderCallback_iface, riid, ppv); }
static ULONG WINAPI cba_AddRef(IWMReaderCallbackAdvanced *i) { return cb_AddRef(&impl_cba(i)->IWMReaderCallback_iface); }
static ULONG WINAPI cba_Release(IWMReaderCallbackAdvanced *i) { return cb_Release(&impl_cba(i)->IWMReaderCallback_iface); }
static HRESULT WINAPI cba_OnStreamSample(IWMReaderCallbackAdvanced *i, WORD s, QWORD t, QWORD d, DWORD f, INSSBuffer *b, void *c) { return S_OK; }
static HRESULT WINAPI cba_OnTime(IWMReaderCallbackAdvanced *i, QWORD time, void *context) { return S_OK; }
static HRESULT WINAPI cba_OnStreamSelection(IWMReaderCallbackAdvanced *i, WORD n, WORD *st, WMT_STREAM_SELECTION *s, void *c) { return S_OK; }
static HRESULT WINAPI cba_OnOutputPropsChanged(IWMReaderCallbackAdvanced *i, DWORD o, WM_MEDIA_TYPE *t, void *c) { return S_OK; }
static HRESULT WINAPI cba_AllocateForStream(IWMReaderCallbackAdvanced *i, WORD s, DWORD n, INSSBuffer **b, void *c) { return E_NOTIMPL; }
static HRESULT WINAPI cba_AllocateForOutput(IWMReaderCallbackAdvanced *i, DWORD o, DWORD n, INSSBuffer **b, void *c) { return E_NOTIMPL; }
static const IWMReaderCallbackAdvancedVtbl cba_vtbl = {cba_QI, cba_AddRef, cba_Release, cba_OnStreamSample, cba_OnTime,
    cba_OnStreamSelection, cba_OnOutputPropsChanged, cba_AllocateForStream, cba_AllocateForOutput};

static void callback_init(struct callback *cb, BOOL no_advanced)
{
    memset(cb, 0, sizeof(*cb));
    cb->IWMReaderCallback_iface.lpVtbl = (IWMReaderCallbackVtbl *)&cb_vtbl;
    cb->IWMReaderCallbackAdvanced_iface.lpVtbl = (IWMReaderCallbackAdvancedVtbl *)&cba_vtbl;
    cb->ref = 1;
    cb->no_advanced = no_advanced;
    cb->opened = CreateEventW(NULL, FALSE, FALSE, NULL);
    cb->started = CreateEventW(NULL, FALSE, FALSE, NULL);
}

/* The first open in a fresh prefix can fail while the media framework starts. */
static HRESULT open_retry(IWMReader *reader, const WCHAR *file, struct callback *cb)
{
    HRESULT hr = E_FAIL;
    int i;

    for (i = 0; i < 5; i++)
    {
        hr = IWMReader_Open(reader, file, &cb->IWMReaderCallback_iface, NULL);
        if (hr == S_OK && WaitForSingleObject(cb->opened, 5000) == WAIT_OBJECT_0)
            return S_OK;
        if (hr == S_OK)
            IWMReader_Close(reader);
        Sleep(1500);
    }
    return FAILED(hr) ? hr : E_FAIL;
}

/* ---- a player hook ---- */

struct hook { IWMPlayerHook iface; LONG ref; };
static struct hook *impl_hook(IWMPlayerHook *i) { return CONTAINING_RECORD(i, struct hook, iface); }
static HRESULT WINAPI hook_QI(IWMPlayerHook *i, REFIID riid, void **ppv)
{
    if (IsEqualGUID(riid, &IID_IUnknown) || IsEqualGUID(riid, &IID_IWMPlayerHook)) { *ppv = i; IWMPlayerHook_AddRef(i); return S_OK; }
    *ppv = NULL; return E_NOINTERFACE;
}
static ULONG WINAPI hook_AddRef(IWMPlayerHook *i) { return InterlockedIncrement(&impl_hook(i)->ref); }
static ULONG WINAPI hook_Release(IWMPlayerHook *i) { return InterlockedDecrement(&impl_hook(i)->ref); }
static HRESULT WINAPI hook_PreDecode(IWMPlayerHook *i) { return S_OK; }
static const IWMPlayerHookVtbl hook_vtbl = {hook_QI, hook_AddRef, hook_Release, hook_PreDecode};

/* ---- the table of output settings: what an audio and a video output answer ---- */

struct setting_case
{
    const WCHAR *name;
    WMT_ATTR_DATATYPE type;
    DWORD deflt[2];       /* value for [audio, video] when the get works */
    HRESULT get[2];
    HRESULT set[2];
    DWORD set_value;
};

static const struct setting_case settings[] =
{
    {L"AllowInterlacedOutput", WMT_TYPE_BOOL, {0, 0}, {E_INVALIDARG, S_OK}, {E_INVALIDARG, S_OK}, 1},
    {L"DedicatedDeliveryThread", WMT_TYPE_BOOL, {0, 0}, {E_INVALIDARG, E_INVALIDARG}, {S_OK, S_OK}, 1},
    {L"DeliverOnReceive", WMT_TYPE_BOOL, {0, 0}, {S_OK, S_OK}, {S_OK, S_OK}, 1},
    {L"EnableDiscreteOutput", WMT_TYPE_BOOL, {0, 0}, {S_OK, E_INVALIDARG}, {S_OK, E_INVALIDARG}, 1},
    {L"EnableFrameInterpolation", WMT_TYPE_BOOL, {0, 0}, {E_INVALIDARG, S_OK}, {E_INVALIDARG, S_OK}, 1},
    {L"JustInTimeDecode", WMT_TYPE_BOOL, {0, 0}, {S_OK, S_OK}, {S_OK, S_OK}, 1},
    {L"NeedsPreviousSample", WMT_TYPE_BOOL, {0, 0}, {E_INVALIDARG, NS_E_INVALID_REQUEST}, {E_INVALIDARG, E_INVALIDARG}, 0},
    {L"ScrambledAudio", WMT_TYPE_BOOL, {0, 0}, {E_INVALIDARG, E_INVALIDARG}, {E_INVALIDARG, E_INVALIDARG}, 0},
    {L"SingleOutputBuffer", WMT_TYPE_BOOL, {0, 0}, {S_OK, S_OK}, {S_OK, S_OK}, 1},
    {L"SoftwareScaling", WMT_TYPE_BOOL, {0, 1}, {E_INVALIDARG, S_OK}, {E_INVALIDARG, S_OK}, 0},
    {L"VideoSampleDurations", WMT_TYPE_BOOL, {0, 0}, {E_INVALIDARG, S_OK}, {E_INVALIDARG, S_OK}, 1},
    {L"EnableWMAProSPDIFOutput", WMT_TYPE_BOOL, {0, 0}, {E_INVALIDARG, E_INVALIDARG}, {S_OK, E_INVALIDARG}, 1},
    {L"StreamLanguage", WMT_TYPE_WORD, {0, 0}, {NS_E_INVALID_REQUEST, NS_E_INVALID_REQUEST}, {S_OK, S_OK}, 0},
    {L"DynamicRangeControl", WMT_TYPE_DWORD, {~0u, 0}, {S_OK, E_INVALIDARG}, {S_OK, E_INVALIDARG}, 1},
    {L"EarlyDataDelivery", WMT_TYPE_DWORD, {0, 0}, {S_OK, S_OK}, {S_OK, S_OK}, 1500},
    {L"SpeakerConfig", WMT_TYPE_DWORD, {~0u, 0}, {S_OK, E_INVALIDARG}, {S_OK, E_INVALIDARG}, 2},
};

static DWORD type_size(WMT_ATTR_DATATYPE t) { return t == WMT_TYPE_WORD ? 2 : 4; }

static void test_output_settings(IWMReader *reader, IWMReaderAdvanced2 *adv)
{
    DWORD out[2] = {99, 99}, count, i, k;
    HRESULT hr;

    hr = IWMReader_GetOutputCount(reader, &count);
    CHECKF(hr == S_OK && count == 2, "two outputs (%08lx %lu)", hr, count);
    for (i = 0; i < count; i++)
    {
        IWMOutputMediaProps *props;
        GUID type;
        IWMReader_GetOutputProps(reader, i, &props);
        IWMOutputMediaProps_GetType(props, &type);
        IWMOutputMediaProps_Release(props);
        if (IsEqualGUID(&type, &WMMEDIATYPE_Audio)) out[0] = i;
        if (IsEqualGUID(&type, &WMMEDIATYPE_Video)) out[1] = i;
    }
    CHECKF(out[0] != 99 && out[1] != 99 && out[0] != out[1], "the file has an audio and a video output (%lu, %lu)", out[0], out[1]);
    if (out[0] == 99 || out[1] == 99) return;

    /* the defaults and what is readable */
    for (k = 0; k < 2; k++)
    {
        const char *kind = k ? "video" : "audio";
        for (i = 0; i < ARRAY_SIZE(settings); i++)
        {
            const struct setting_case *c = &settings[i];
            WMT_ATTR_DATATYPE type = 99;
            DWORD value = 0x55555555;
            WORD size = type_size(c->type);

            hr = IWMReaderAdvanced2_GetOutputSetting(adv, out[k], c->name, &type, (BYTE *)&value, &size);
            CHECKF(hr == c->get[k], "%s output: get %ls = %08lx (%08lx)", kind, c->name, c->get[k], hr);
            if (hr == S_OK)
            {
                DWORD mask = size == 2 ? 0xffff : 0xffffffff;
                CHECKF(type == c->type && size == type_size(c->type) && (value & mask) == (c->deflt[k] & mask),
                       "%s output: %ls default is %lx of type %d size %u (got %lx type %d size %u)", kind, c->name, c->deflt[k], c->type,
                       type_size(c->type), value, type, size);
            }
        }
    }

    /* setting */
    for (k = 0; k < 2; k++)
    {
        const char *kind = k ? "video" : "audio";
        for (i = 0; i < ARRAY_SIZE(settings); i++)
        {
            const struct setting_case *c = &settings[i];
            DWORD value = c->set_value;

            hr = IWMReaderAdvanced2_SetOutputSetting(adv, out[k], c->name, c->type, (BYTE *)&value, type_size(c->type));
            CHECKF(hr == c->set[k], "%s output: set %ls = %08lx (%08lx)", kind, c->name, c->set[k], hr);
        }
    }

    /* values that were set can be read back, per output */
    {
        WMT_ATTR_DATATYPE type;
        DWORD value;
        WORD size;
        BOOL b;

        type = WMT_TYPE_DWORD; size = 4; value = 0;
        hr = IWMReaderAdvanced2_GetOutputSetting(adv, out[0], L"EarlyDataDelivery", &type, (BYTE *)&value, &size);
        CHECKF(hr == S_OK && value == 1500, "audio EarlyDataDelivery reads back what was set (%lu)", value);
        hr = IWMReaderAdvanced2_GetOutputSetting(adv, out[1], L"EarlyDataDelivery", &type, (BYTE *)&value, &size);
        CHECKF(hr == S_OK && value == 1500, "video EarlyDataDelivery reads back what was set (%lu)", value);
        value = 77;
        IWMReaderAdvanced2_SetOutputSetting(adv, out[1], L"EarlyDataDelivery", WMT_TYPE_DWORD, (BYTE *)&value, 4);
        IWMReaderAdvanced2_GetOutputSetting(adv, out[0], L"EarlyDataDelivery", &type, (BYTE *)&value, &size);
        CHECKF(value == 1500, "outputs have their own settings (audio still %lu)", value);
        IWMReaderAdvanced2_GetOutputSetting(adv, out[1], L"EarlyDataDelivery", &type, (BYTE *)&value, &size);
        CHECKF(value == 77, "outputs have their own settings (video %lu)", value);

        type = WMT_TYPE_DWORD; size = 4; value = 0;
        hr = IWMReaderAdvanced2_GetOutputSetting(adv, out[0], L"SpeakerConfig", &type, (BYTE *)&value, &size);
        CHECKF(hr == S_OK && value == 2, "SpeakerConfig reads back (%lu)", value);
        hr = IWMReaderAdvanced2_GetOutputSetting(adv, out[0], L"dynamicrangecontrol", &type, (BYTE *)&value, &size);
        CHECKF(hr == S_OK && value == 1, "setting names are case-insensitive (%08lx %lu)", hr, value);

        /* a BOOL is stored as 0 or 1 */
        b = 7;
        IWMReaderAdvanced2_SetOutputSetting(adv, out[0], L"DeliverOnReceive", WMT_TYPE_BOOL, (BYTE *)&b, 4);
        b = 55; size = 4; type = 0;
        IWMReaderAdvanced2_GetOutputSetting(adv, out[0], L"DeliverOnReceive", &type, (BYTE *)&b, &size);
        CHECKF(b == 1, "a BOOL setting stores 1 for a non-zero value (%d)", b);
        b = 0;
        IWMReaderAdvanced2_SetOutputSetting(adv, out[0], L"DeliverOnReceive", WMT_TYPE_BOOL, (BYTE *)&b, 4);
        IWMReaderAdvanced2_GetOutputSetting(adv, out[0], L"DeliverOnReceive", &type, (BYTE *)&b, &size);
        CHECKF(b == 0, "and 0 again");

        /* argument checks */
        size = 4; type = 0;
        hr = IWMReaderAdvanced2_GetOutputSetting(adv, out[0], L"NoSuchSetting", &type, (BYTE *)&value, &size);
        CHECKF(hr == E_INVALIDARG, "get of an unknown setting = E_INVALIDARG (%08lx)", hr);
        hr = IWMReaderAdvanced2_GetOutputSetting(adv, 7, L"EarlyDataDelivery", &type, (BYTE *)&value, &size);
        CHECKF(hr == E_INVALIDARG, "get on output 7 = E_INVALIDARG (%08lx)", hr);
        hr = IWMReaderAdvanced2_GetOutputSetting(adv, out[0], NULL, &type, (BYTE *)&value, &size);
        CHECKF(hr == E_INVALIDARG, "get with a NULL name = E_INVALIDARG (%08lx)", hr);
        hr = IWMReaderAdvanced2_GetOutputSetting(adv, out[0], L"EarlyDataDelivery", NULL, (BYTE *)&value, &size);
        CHECKF(hr == E_INVALIDARG, "get with a NULL type = E_INVALIDARG (%08lx)", hr);
        hr = IWMReaderAdvanced2_GetOutputSetting(adv, out[0], L"EarlyDataDelivery", &type, (BYTE *)&value, NULL);
        CHECKF(hr == E_INVALIDARG, "get with a NULL length = E_INVALIDARG (%08lx)", hr);
        size = 0;
        hr = IWMReaderAdvanced2_GetOutputSetting(adv, out[0], L"EarlyDataDelivery", &type, NULL, &size);
        CHECKF(hr == S_OK && size == 4 && type == WMT_TYPE_DWORD, "get without a buffer returns the size (%08lx %u)", hr, size);
        size = 2; value = 0x55555555;
        hr = IWMReaderAdvanced2_GetOutputSetting(adv, out[0], L"EarlyDataDelivery", &type, (BYTE *)&value, &size);
        CHECKF(hr == ASF_E_BUFFERTOOSMALL && size == 4 && value == 0x55555555, "get into a short buffer = ASF_E_BUFFERTOOSMALL with the size (%08lx %u)", hr, size);
        size = 40; value = 0;
        hr = IWMReaderAdvanced2_GetOutputSetting(adv, out[0], L"EarlyDataDelivery", &type, (BYTE *)&value, &size);
        CHECKF(hr == S_OK && size == 4, "get into a long buffer returns the used size (%08lx %u)", hr, size);

        value = 5;
        hr = IWMReaderAdvanced2_SetOutputSetting(adv, out[0], L"EarlyDataDelivery", WMT_TYPE_BOOL, (BYTE *)&value, 4);
        CHECKF(hr == E_INVALIDARG, "set with the wrong type = E_INVALIDARG (%08lx)", hr);
        hr = IWMReaderAdvanced2_SetOutputSetting(adv, out[0], L"EarlyDataDelivery", WMT_TYPE_DWORD, (BYTE *)&value, 2);
        CHECKF(hr == E_INVALIDARG, "set with the wrong size = E_INVALIDARG (%08lx)", hr);
        hr = IWMReaderAdvanced2_SetOutputSetting(adv, out[0], L"EarlyDataDelivery", WMT_TYPE_DWORD, NULL, 4);
        CHECKF(hr == E_INVALIDARG, "set without a value = E_INVALIDARG (%08lx)", hr);
        hr = IWMReaderAdvanced2_SetOutputSetting(adv, out[0], NULL, WMT_TYPE_DWORD, (BYTE *)&value, 4);
        CHECKF(hr == E_INVALIDARG, "set with a NULL name = E_INVALIDARG (%08lx)", hr);
        hr = IWMReaderAdvanced2_SetOutputSetting(adv, 9, L"EarlyDataDelivery", WMT_TYPE_DWORD, (BYTE *)&value, 4);
        CHECKF(hr == E_INVALIDARG, "set on output 9 = E_INVALIDARG (%08lx)", hr);
        hr = IWMReaderAdvanced2_SetOutputSetting(adv, out[0], L"NoSuchSetting", WMT_TYPE_DWORD, (BYTE *)&value, 4);
        CHECKF(hr == E_INVALIDARG, "set of an unknown setting = E_INVALIDARG (%08lx)", hr);
        size = 4; type = 0;
        IWMReaderAdvanced2_GetOutputSetting(adv, out[0], L"EarlyDataDelivery", &type, (BYTE *)&value, &size);
        CHECKF(value == 1500, "refused sets change nothing (%lu)", value);
    }
}

static void test_source_state(IWMReaderAdvanced6 *adv, const WCHAR *file, QWORD file_size)
{
    DWORD percent = 99, d;
    QWORD buffering = 99, got = 99, total = 99;
    WCHAR buf[MAX_PATH];
    BOOL b;
    double f;
    HRESULT hr;

    hr = IWMReaderAdvanced6_GetBufferProgress(adv, &percent, &buffering);
    CHECKF(hr == S_OK && percent == 100 && buffering == 0, "a file is fully buffered (%08lx %lu %I64u)", hr, percent, buffering);
    CHECKF(IWMReaderAdvanced6_GetBufferProgress(adv, NULL, &buffering) == E_INVALIDARG, "GetBufferProgress(NULL) = E_INVALIDARG");
    hr = IWMReaderAdvanced6_GetDownloadProgress(adv, &percent, &got, &total);
    CHECKF(hr == S_OK && percent == 100 && got == file_size && total == file_size,
           "the download is complete, with the size of the file (%08lx %lu %I64u %I64u vs %I64u)", hr, percent, got, total, file_size);
    CHECKF(IWMReaderAdvanced6_GetDownloadProgress(adv, &percent, NULL, &total) == E_INVALIDARG, "GetDownloadProgress(NULL) = E_INVALIDARG");

    d = 0; hr = IWMReaderAdvanced6_GetProtocolName(adv, NULL, &d);
    CHECKF(hr == S_OK && d == 5, "protocol name length (%08lx %lu)", hr, d);
    d = 64; memset(buf, 0, sizeof(buf));
    hr = IWMReaderAdvanced6_GetProtocolName(adv, buf, &d);
    CHECKF(hr == S_OK && !wcscmp(buf, L"file"), "protocol of a file is file (%08lx %ls)", hr, buf);
    d = 2; hr = IWMReaderAdvanced6_GetProtocolName(adv, buf, &d);
    CHECKF(hr == ASF_E_BUFFERTOOSMALL && d == 5, "protocol name in a short buffer (%08lx %lu)", hr, d);
    d = 0; hr = IWMReaderAdvanced6_GetURL(adv, NULL, &d);
    CHECKF(hr == S_OK && d == wcslen(file) + 1, "URL length (%08lx %lu)", hr, d);
    d = MAX_PATH; hr = IWMReaderAdvanced6_GetURL(adv, buf, &d);
    CHECKF(hr == S_OK && !wcscmp(buf, file), "URL is what was opened (%08lx %ls)", hr, buf);
    CHECKF(IWMReaderAdvanced6_GetURL(adv, buf, NULL) == E_INVALIDARG, "GetURL(NULL length) = E_INVALIDARG");

    b = 5; hr = IWMReaderAdvanced6_CanSaveFileAs(adv, &b);
    CHECKF(hr == S_OK && b == FALSE, "a file cannot be saved as (%08lx %d)", hr, b);
    CHECKF(IWMReaderAdvanced6_CanSaveFileAs(adv, NULL) == E_INVALIDARG, "CanSaveFileAs(NULL) = E_INVALIDARG");
    hr = IWMReaderAdvanced6_SaveFileAs(adv, L"C:\\copy.wmv");
    CHECKF(hr == NS_E_INVALID_REQUEST, "SaveFileAs of a file = NS_E_INVALID_REQUEST (%08lx)", hr);
    CHECKF(IWMReaderAdvanced6_SaveFileAs(adv, NULL) == E_INVALIDARG, "SaveFileAs(NULL) = E_INVALIDARG");
    hr = IWMReaderAdvanced6_GetSaveAsProgress(adv, &percent);
    CHECKF(hr == NS_E_INVALID_REQUEST, "no save-as progress (%08lx)", hr);
    CHECKF(IWMReaderAdvanced6_GetSaveAsProgress(adv, NULL) == E_INVALIDARG, "GetSaveAsProgress(NULL) = E_INVALIDARG");
    CHECKF(IWMReaderAdvanced6_CancelSaveFileAs(adv) == NS_E_INVALID_REQUEST, "CancelSaveFileAs with nothing to cancel = NS_E_INVALID_REQUEST");
    CHECKF(IWMReaderAdvanced6_Preroll(adv, 0, 0, 1.0f) == S_OK, "Preroll");
    CHECKF(IWMReaderAdvanced6_StopBuffering(adv) == S_OK, "StopBuffering");
    CHECKF(IWMReaderAdvanced6_StopNetStreaming(adv) == S_OK, "StopNetStreaming");
    b = 5; hr = IWMReaderAdvanced6_IsUsingFastCache(adv, &b);
    CHECKF(hr == S_OK && b == FALSE, "no fast cache (%08lx %d)", hr, b);
    CHECKF(IWMReaderAdvanced6_IsUsingFastCache(adv, NULL) == E_INVALIDARG, "IsUsingFastCache(NULL) = E_INVALIDARG");
    f = 0; hr = IWMReaderAdvanced6_GetMaxSpeedFactor(adv, &f);
    CHECKF(hr == S_OK && f == 1.0, "max speed factor is 1 (%08lx %f)", hr, f);
    CHECKF(IWMReaderAdvanced6_GetMaxSpeedFactor(adv, NULL) == E_INVALIDARG, "GetMaxSpeedFactor(NULL) = E_INVALIDARG");

    /* max sample sizes of the outputs */
    for (d = 0; d < 2; d++)
    {
        DWORD max = 0;
        hr = IWMReaderAdvanced6_GetMaxOutputSampleSize(adv, d, &max);
        CHECKF(hr == S_OK && max > 0, "max output sample size of output %lu (%08lx %lu)", d, hr, max);
    }
    hr = IWMReaderAdvanced6_GetMaxOutputSampleSize(adv, 5, &d);
    CHECKF(FAILED(hr), "max output sample size of output 5 fails (%08lx)", hr);
    CHECKF(IWMReaderAdvanced6_GetMaxOutputSampleSize(adv, 0, NULL) == E_INVALIDARG, "GetMaxOutputSampleSize(NULL) = E_INVALIDARG");

    /* languages */
    {
        WORD count = 99, len = 64;
        hr = IWMReaderAdvanced6_GetLanguageCount(adv, 0, &count);
        CHECKF(hr == S_OK, "GetLanguageCount (%08lx %u)", hr, count);
        hr = IWMReaderAdvanced6_GetLanguage(adv, 0, count, buf, &len);
        CHECKF(hr == E_INVALIDARG, "GetLanguage past the count = E_INVALIDARG (%08lx)", hr);
        hr = IWMReaderAdvanced6_GetLanguageCount(adv, 9, &count);
        CHECKF(hr == E_INVALIDARG, "GetLanguageCount(output 9) = E_INVALIDARG (%08lx)", hr);
        CHECKF(IWMReaderAdvanced6_GetLanguageCount(adv, 0, NULL) == E_INVALIDARG, "GetLanguageCount(NULL) = E_INVALIDARG");
    }

    /* markers */
    hr = IWMReaderAdvanced6_StartAtMarker(adv, 0, 0, 1.0f, NULL);
    CHECKF(hr == E_INVALIDARG, "StartAtMarker(0) in a file without markers = E_INVALIDARG (%08lx)", hr);
}

static void test_hook(IWMReaderAdvanced6 *adv, IWMReader *reader)
{
    struct hook h = {{(IWMPlayerHookVtbl *)&hook_vtbl}, 1};
    HRESULT hr;

    hr = IWMReaderAdvanced6_SetPlayerHook(adv, 0, &h.iface);
    CHECKF(hr == S_OK, "SetPlayerHook (%08lx)", hr);
    CHECKF(h.ref == 2, "the reader holds the hook (%ld)", h.ref);
    hr = IWMReaderAdvanced6_SetPlayerHook(adv, 0, &h.iface);
    CHECKF(hr == S_OK && h.ref == 2, "setting the same hook again keeps one reference (%ld)", h.ref);
    hr = IWMReaderAdvanced6_SetPlayerHook(adv, 0, NULL);
    CHECKF(hr == S_OK && h.ref == 1, "a NULL hook releases it (%ld)", h.ref);
    hr = IWMReaderAdvanced6_SetPlayerHook(adv, 1, &h.iface);
    CHECKF(hr == S_OK && h.ref == 2, "a hook on the other output (%ld)", h.ref);
    hr = IWMReaderAdvanced6_SetPlayerHook(adv, 9, &h.iface);
    CHECKF(hr == E_INVALIDARG && h.ref == 2, "a hook on output 9 = E_INVALIDARG (%08lx)", hr);
    hr = IWMReader_Close(reader);
    CHECKF(hr == S_OK, "Close (%08lx)", hr);
    CHECKF(h.ref == 1, "Close releases the hooks (%ld)", h.ref);
}

static void test_start_at(IWMReader *reader, IWMReaderAdvanced6 *adv, struct callback *cb)
{
    QWORD start = 0, duration = 5000000;
    HRESULT hr;

    hr = IWMReaderAdvanced6_StartAtPosition(adv, 0, &start, &duration, WMT_OFFSET_FORMAT_FRAME_NUMBERS, 1.0f, NULL);
    CHECKF(hr == E_NOTIMPL, "StartAtPosition in frame numbers is not supported (%08lx)", hr);
    hr = IWMReaderAdvanced6_StartAtPosition(adv, 0, NULL, NULL, WMT_OFFSET_FORMAT_100NS, 1.0f, NULL);
    CHECKF(hr == E_INVALIDARG, "StartAtPosition(NULL start) = E_INVALIDARG (%08lx)", hr);
    hr = IWMReaderAdvanced6_StartAtPosition(adv, 0, &start, &duration, (WMT_OFFSET_FORMAT)77, 1.0f, NULL);
    CHECKF(hr == E_INVALIDARG, "StartAtPosition(format 77) = E_INVALIDARG (%08lx)", hr);

    ResetEvent(cb->started);
    cb->started_count = 0;
    hr = IWMReaderAdvanced6_StartAtPosition(adv, 0, &start, &duration, WMT_OFFSET_FORMAT_100NS, 1.0f, (void *)0xf00d);
    CHECKF(hr == S_OK, "StartAtPosition(100 ns) (%08lx)", hr);
    CHECKF(WaitForSingleObject(cb->started, 5000) == WAIT_OBJECT_0 && cb->started_context == (void *)0xf00d,
           "the reader started with the context (%p)", cb->started_context);
    hr = IWMReader_Stop(reader);
    CHECKF(hr == S_OK, "Stop (%08lx)", hr);
}

static void test_allocator(const WCHAR *file)
{
    IWMReader *reader = NULL;
    IWMReaderAdvanced6 *adv = NULL;
    struct callback cb;
    HRESULT hr;
    DWORD r;
    int tries;

    callback_init(&cb, TRUE);
    cb.check_buffers = TRUE;
    {
        HRESULT (WINAPI *create)(IUnknown *, DWORD, IWMReader **) = (void *)GetProcAddress(GetModuleHandleW(L"wmvcore.dll"), "WMCreateReader");
        hr = create(NULL, 0, &reader);
    }
    IWMReader_QueryInterface(reader, &IID_IWMReaderAdvanced6, (void **)&adv);
    hr = open_retry(reader, file, &cb);
    r = hr == S_OK ? WAIT_OBJECT_0 : WAIT_TIMEOUT;
    if (r != WAIT_OBJECT_0)
    {
        printf("SKIP  allocator test: the sample could not be opened (%08lx)\n", hr);
        goto done;
    }

    /* the callback has no allocator of its own: the reader hands out its own buffers */
    hr = IWMReaderAdvanced6_SetAllocateForOutput(adv, 0, TRUE);
    CHECKF(hr == S_OK, "SetAllocateForOutput(0) (%08lx)", hr);
    hr = IWMReaderAdvanced6_SetAllocateForOutput(adv, 1, TRUE);
    CHECKF(hr == S_OK, "SetAllocateForOutput(1) (%08lx)", hr);
    hr = IWMReader_Start(reader, 0, 0, 1.0f, NULL);
    CHECKF(hr == S_OK, "Start (%08lx)", hr);
    for (tries = 0; tries < 100 && cb.sample_count < 4; tries++)
        Sleep(50);
    CHECKF(cb.sample_count >= 4, "samples arrive in the reader's buffers (%ld)", cb.sample_count);
    CHECKF(cb.bad_buffers == 0, "the buffers behave (%ld bad)", cb.bad_buffers);
    IWMReader_Stop(reader);
    IWMReader_Close(reader);
done:
    IWMReaderAdvanced6_Release(adv);
    IWMReader_Release(reader);
}

int main(int argc, char **argv)
{
    IWMReader *reader = NULL;
    IWMReaderAdvanced2 *adv2 = NULL;
    IWMReaderAdvanced6 *adv6 = NULL;
    HRESULT (WINAPI *create)(IUnknown *, DWORD, IWMReader **);
    struct callback cb;
    WCHAR file[MAX_PATH];
    WIN32_FILE_ATTRIBUTE_DATA data;
    QWORD size = 0;
    HRESULT hr;
    DWORD r, d;
    WCHAR buf[8];
    QWORD q;
    BOOL b;

    CoInitialize(NULL);
    {
        HMODULE m = LoadLibraryW(L"wmvcore.dll");
        create = m ? (void *)GetProcAddress(m, "WMCreateReader") : NULL;
    }
    if (!create) { check(0, "WMCreateReader"); goto out; }
    if (argc < 2 || !MultiByteToWideChar(CP_ACP, 0, argv[1], -1, file, MAX_PATH)) { printf("SKIP  no sample file\n"); goto out; }
    if (GetFileAttributesExW(file, GetFileExInfoStandard, &data))
        size = ((QWORD)data.nFileSizeHigh << 32) | data.nFileSizeLow;

    hr = create(NULL, 0, &reader);
    CHECKF(hr == S_OK, "WMCreateReader (%08lx)", hr);
    IWMReader_QueryInterface(reader, &IID_IWMReaderAdvanced2, (void **)&adv2);
    IWMReader_QueryInterface(reader, &IID_IWMReaderAdvanced6, (void **)&adv6);

    /* before Open */
    {
        WMT_ATTR_DATATYPE type = WMT_TYPE_BOOL;
        DWORD value = 0;
        WORD len = 4;
        hr = IWMReaderAdvanced2_GetOutputSetting(adv2, 0, L"AllowInterlacedOutput", &type, (BYTE *)&value, &len);
        CHECKF(hr == E_UNEXPECTED, "GetOutputSetting before Open = E_UNEXPECTED (%08lx)", hr);
        value = 1;
        hr = IWMReaderAdvanced2_SetOutputSetting(adv2, 0, L"DeliverOnReceive", WMT_TYPE_BOOL, (BYTE *)&value, 4);
        CHECKF(hr == E_UNEXPECTED, "SetOutputSetting before Open = E_UNEXPECTED (%08lx)", hr);
    }
    hr = IWMReaderAdvanced6_GetBufferProgress(adv6, &d, &q);
    CHECKF(hr == E_UNEXPECTED, "GetBufferProgress before Open = E_UNEXPECTED (%08lx)", hr);
    {
        QWORD q1, q2;
        hr = IWMReaderAdvanced6_GetDownloadProgress(adv6, &d, &q1, &q2);
        CHECKF(hr == E_UNEXPECTED, "GetDownloadProgress before Open = E_UNEXPECTED (%08lx)", hr);
    }
    d = 4;
    CHECKF(IWMReaderAdvanced6_GetProtocolName(adv6, buf, &d) == E_UNEXPECTED, "GetProtocolName before Open = E_UNEXPECTED");
    CHECKF(IWMReaderAdvanced6_GetURL(adv6, buf, &d) == E_UNEXPECTED, "GetURL before Open = E_UNEXPECTED");
    b = 5; CHECKF(IWMReaderAdvanced6_CanSaveFileAs(adv6, &b) == E_UNEXPECTED, "CanSaveFileAs before Open = E_UNEXPECTED");
    CHECKF(IWMReaderAdvanced6_SaveFileAs(adv6, L"C:\\x.wmv") == E_UNEXPECTED, "SaveFileAs before Open = E_UNEXPECTED");
    CHECKF(IWMReaderAdvanced6_StopBuffering(adv6) == E_UNEXPECTED, "StopBuffering before Open = E_UNEXPECTED");
    CHECKF(IWMReaderAdvanced6_StopNetStreaming(adv6) == E_UNEXPECTED, "StopNetStreaming before Open = E_UNEXPECTED");
    CHECKF(IWMReaderAdvanced6_Preroll(adv6, 0, 0, 1.0f) == E_UNEXPECTED, "Preroll before Open = E_UNEXPECTED");
    {
        struct hook h = {{(IWMPlayerHookVtbl *)&hook_vtbl}, 1};
        CHECKF(IWMReaderAdvanced6_SetPlayerHook(adv6, 0, &h.iface) == E_UNEXPECTED && h.ref == 1, "SetPlayerHook before Open = E_UNEXPECTED");
    }
    CHECKF(IWMReaderAdvanced6_GetSaveAsProgress(adv6, &d) == NS_E_INVALID_REQUEST, "GetSaveAsProgress = NS_E_INVALID_REQUEST in any state");

    callback_init(&cb, FALSE);
    hr = open_retry(reader, file, &cb);
    r = hr == S_OK ? WAIT_OBJECT_0 : WAIT_TIMEOUT;
    if (r != WAIT_OBJECT_0)
    {
        printf("SKIP  the sample file could not be opened here (%08lx)\n", hr);
        goto done;
    }
    CHECKF(hr == S_OK, "Open (%08lx)", hr);

    test_output_settings(reader, adv2);
    test_source_state(adv6, file, size);
    test_start_at(reader, adv6, &cb);
    test_hook(adv6, reader);
    IWMReaderAdvanced6_Release(adv6);
    IWMReaderAdvanced2_Release(adv2);
    IWMReader_Release(reader);
    reader = NULL;
    test_allocator(file);
    goto out;
done:
    IWMReaderAdvanced6_Release(adv6);
    IWMReaderAdvanced2_Release(adv2);
    IWMReader_Release(reader);
out:
    CoUninitialize();
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures ? 1 : 0;
}
