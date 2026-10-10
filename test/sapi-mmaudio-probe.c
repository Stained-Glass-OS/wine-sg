/* SAPI's wave output device object, SpMMAudioOut (patch 2843), run by
 * test/sapi-mmaudio-gate.sh on Xvfb: the ISpEventSource / ISpEventSink side
 * (interest, queue, copies of string and object parameters, the notification
 * kinds), the IStream members of an output device, GetStatus, buffer info,
 * default format, volume, the MM handle, the pause/stop states and the device
 * id carried by the object token. The parts that need a wave device are
 * skipped when the machine has none.
 *
 *   sapi-mmaudio-probe.exe */
#define COBJMACROS
#include <windows.h>
#include <ole2.h>
#include <initguid.h>
#include <sapi.h>
#include <stdio.h>
#include <wchar.h>

DEFINE_GUID(IID_Bogus, 0x12345678, 0x1234, 0x1234, 0x12, 0x34, 0x12, 0x34, 0x12, 0x34, 0x12, 0x34);
DEFINE_GUID(PR_SPDFID_WaveFormatEx, 0xc31adbae, 0x527f, 0x4ff5, 0xa2, 0x30, 0xf6, 0x2b, 0xb6, 0x1f, 0xf7, 0x0c);

#define SPERR_UNINITIALIZED          ((HRESULT)0x80045001)
#define SPERR_ALREADY_INITIALIZED    ((HRESULT)0x80045002)
#define SPERR_INVALID_AUDIO_STATE    ((HRESULT)0x8004503b)
#define SPERR_GENERIC_MMSYS_ERROR    ((HRESULT)0x8004503c)
#define SP_AUDIO_STOPPED             ((HRESULT)0x00045065)

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}
#define CHECKF(ok, ...) do { char _b[300]; snprintf(_b, sizeof(_b), __VA_ARGS__); check(ok, _b); } while (0)

static ULONG refs(IUnknown *u) { IUnknown_AddRef(u); return IUnknown_Release(u); }

static LONG g_cb; static WPARAM g_cb_w; static LPARAM g_cb_l;
static void __stdcall callback_fn(WPARAM w, LPARAM l) { g_cb_w = w; g_cb_l = l; InterlockedIncrement(&g_cb); }
static LONG g_sink;
static HRESULT STDMETHODCALLTYPE sink_QI(ISpNotifySink *This, REFIID iid, void **obj)
{ if (IsEqualIID(iid, &IID_IUnknown) || IsEqualIID(iid, &IID_ISpNotifySink)) { *obj = This; return S_OK; } *obj = NULL; return E_NOINTERFACE; }
static ULONG STDMETHODCALLTYPE sink_AddRef(ISpNotifySink *This) { return 2; }
static ULONG STDMETHODCALLTYPE sink_Release(ISpNotifySink *This) { return 1; }
static HRESULT STDMETHODCALLTYPE sink_Notify(ISpNotifySink *This) { InterlockedIncrement(&g_sink); return S_OK; }
static const ISpNotifySinkVtbl sink_vtbl = { sink_QI, sink_AddRef, sink_Release, sink_Notify };
static ISpNotifySink sink_obj = { (ISpNotifySinkVtbl *)&sink_vtbl };
static LONG g_cbi; static WPARAM g_cbi_w; static LPARAM g_cbi_l;
struct cbi_vtbl { HRESULT (STDMETHODCALLTYPE *NotifyCallback)(void *, WPARAM, LPARAM); };
static HRESULT STDMETHODCALLTYPE cbi_Notify(void *This, WPARAM w, LPARAM l) { g_cbi_w = w; g_cbi_l = l; InterlockedIncrement(&g_cbi); return S_OK; }
static const struct cbi_vtbl cbi_vtbl_obj = { cbi_Notify };
static const struct cbi_vtbl *cbi_obj = &cbi_vtbl_obj;

static void new_audio(ISpMMSysAudio **audio, ISpEventSource **src, ISpEventSink **sink)
{
    HRESULT hr = CoCreateInstance(&CLSID_SpMMAudioOut, NULL, CLSCTX_INPROC_SERVER, &IID_ISpMMSysAudio, (void **)audio);
    if (hr != S_OK) { *audio = NULL; return; }
    ISpMMSysAudio_QueryInterface(*audio, &IID_ISpEventSource, (void **)src);
    ISpMMSysAudio_QueryInterface(*audio, &IID_ISpEventSink, (void **)sink);
}

static void test_events(void)
{
    ISpMMSysAudio *audio;
    ISpEventSource *src;
    ISpEventSink *sink;
    SPEVENTSOURCEINFO info;
    SPEVENT ev[4], got[4];
    IUnknown *obj;
    ULONGLONG interest;
    ULONG n, r0;
    HANDLE h;
    HRESULT hr;
    WCHAR str[] = L"payload";

    new_audio(&audio, &src, &sink);
    CHECKF(audio && src && sink, "SpMMAudioOut with its event source and sink");
    if (!audio) return;

    hr = ISpEventSource_GetInfo(src, &info);
    CHECKF(hr == S_OK && info.ullEventInterest == SPFEI_FLAGCHECK && info.ullQueuedInterest == SPFEI_FLAGCHECK && info.ulCount == 0,
           "GetInfo: no events of interest, empty queue (%#lx)", hr);
    hr = ISpEventSource_GetInfo(src, NULL);
    CHECKF(hr == E_POINTER, "GetInfo(NULL) is E_POINTER (%#lx)", hr);
    hr = ISpEventSink_GetEventInterest(sink, &interest);
    CHECKF(hr == S_OK && interest == SPFEI_FLAGCHECK, "sink GetEventInterest is the source's interest (%#lx)", hr);
    hr = ISpEventSink_GetEventInterest(sink, NULL);
    CHECKF(hr == E_POINTER, "sink GetEventInterest(NULL) is E_POINTER (%#lx)", hr);

    memset(ev, 0, sizeof(ev));
    ev[0].eEventId = SPEI_START_INPUT_STREAM;
    ev[0].ulStreamNum = 9;
    hr = ISpEventSink_AddEvents(sink, ev, 1);
    CHECKF(hr == S_OK, "AddEvents of an event nobody is interested in (%#lx)", hr);
    ISpEventSource_GetInfo(src, &info);
    CHECKF(info.ulCount == 0, "the uninteresting event is dropped (%lu)", info.ulCount);
    hr = ISpEventSink_AddEvents(sink, NULL, 1);
    CHECKF(hr == E_POINTER, "AddEvents(NULL, 1) is E_POINTER (%#lx)", hr);

    hr = ISpEventSource_SetInterest(src, 0, 0);
    CHECKF(hr == E_INVALIDARG, "SetInterest without the flag bits is E_INVALIDARG (%#lx)", hr);
    hr = ISpEventSource_SetInterest(src, SPFEI(SPEI_START_INPUT_STREAM) | SPFEI(SPEI_TTS_BOOKMARK), SPFEI(SPEI_START_INPUT_STREAM) | SPFEI(SPEI_TTS_BOOKMARK) | SPFEI(SPEI_PHONEME));
    CHECKF(hr == E_INVALIDARG, "SetInterest with queued beyond interest is E_INVALIDARG (%#lx)", hr);
    interest = SPFEI(SPEI_START_INPUT_STREAM) | SPFEI(SPEI_TTS_BOOKMARK) | SPFEI(SPEI_VOICE_CHANGE);
    hr = ISpEventSource_SetInterest(src, interest, interest);
    CHECKF(hr == S_OK, "SetInterest (%#lx)", hr);
    hr = ISpEventSink_GetEventInterest(sink, &interest);
    CHECKF(interest == (SPFEI(SPEI_START_INPUT_STREAM) | SPFEI(SPEI_TTS_BOOKMARK) | SPFEI(SPEI_VOICE_CHANGE)), "the sink reports the new interest");

    /* Win32 event */
    h = ISpEventSource_GetNotifyEventHandle(src);
    CHECKF(h == NULL, "GetNotifyEventHandle before choosing is NULL");
    hr = ISpEventSource_WaitForNotifyEvent(src, 0);
    CHECKF(hr == SPERR_UNINITIALIZED, "WaitForNotifyEvent before choosing (%#lx)", hr);
    hr = ISpEventSource_SetNotifyWin32Event(src);
    CHECKF(hr == S_OK, "SetNotifyWin32Event (%#lx)", hr);
    hr = ISpEventSource_SetNotifySink(src, &sink_obj);
    CHECKF(hr == SPERR_ALREADY_INITIALIZED, "a second kind of notification is SPERR_ALREADY_INITIALIZED (%#lx)", hr);
    hr = ISpEventSource_WaitForNotifyEvent(src, 0);
    CHECKF(hr == S_FALSE, "WaitForNotifyEvent with no events (%#lx)", hr);

    /* events with parameters: a string, an object, a pointer */
    CoCreateInstance(&CLSID_SpStream, NULL, CLSCTX_INPROC_SERVER, &IID_IUnknown, (void **)&obj);
    r0 = refs(obj);
    memset(ev, 0, sizeof(ev));
    ev[0].eEventId = SPEI_TTS_BOOKMARK; ev[0].elParamType = SPET_LPARAM_IS_STRING; ev[0].wParam = 3; ev[0].lParam = (LPARAM)str; ev[0].ullAudioStreamOffset = 100; ev[0].ulStreamNum = 4;
    ev[1].eEventId = SPEI_VOICE_CHANGE; ev[1].elParamType = SPET_LPARAM_IS_OBJECT; ev[1].lParam = (LPARAM)obj;
    ev[2].eEventId = SPEI_START_INPUT_STREAM; ev[2].elParamType = SPET_LPARAM_IS_POINTER; ev[2].wParam = 4; ev[2].lParam = (LPARAM)"abcd";
    ev[3].eEventId = SPEI_PHONEME; /* not of interest */
    hr = ISpEventSink_AddEvents(sink, ev, 4);
    CHECKF(hr == S_OK, "AddEvents of four events (%#lx)", hr);
    ISpEventSource_GetInfo(src, &info);
    CHECKF(info.ulCount == 3, "three of the four were queued (%lu)", info.ulCount);
    CHECKF(refs(obj) == r0 + 1, "the queue holds a reference on an object parameter (%lu)", refs(obj));
    hr = ISpEventSource_WaitForNotifyEvent(src, 0);
    CHECKF(hr == S_OK, "WaitForNotifyEvent after AddEvents (%#lx)", hr);
    str[0] = L'X'; /* the queue has its own copy */
    n = 0;
    memset(got, 0, sizeof(got));
    hr = ISpEventSource_GetEvents(src, 4, got, &n);
    CHECKF(hr == S_FALSE && n == 3, "GetEvents(4) returns three and S_FALSE (%#lx, %lu)", hr, n);
    CHECKF(got[0].eEventId == SPEI_TTS_BOOKMARK && got[0].wParam == 3 && got[0].ulStreamNum == 4 && got[0].ullAudioStreamOffset == 100 &&
           got[0].elParamType == SPET_LPARAM_IS_STRING && got[0].lParam && ((WCHAR *)got[0].lParam)[0] == L'p' && !wcscmp((WCHAR *)got[0].lParam, L"payload"),
           "the string event carries its fields and a copy of the string");
    CHECKF(got[1].eEventId == SPEI_VOICE_CHANGE && got[1].lParam == (LPARAM)obj, "the object event carries the object");
    CHECKF(got[2].eEventId == SPEI_START_INPUT_STREAM && got[2].wParam == 4 && got[2].lParam && got[2].lParam != (LPARAM)"abcd" &&
           !memcmp((void *)got[2].lParam, "abcd", 4), "the pointer event carries a copy of the data");
    CoTaskMemFree((void *)got[0].lParam);
    IUnknown_Release((IUnknown *)got[1].lParam);
    CoTaskMemFree((void *)got[2].lParam);
    CHECKF(refs(obj) == r0, "the caller owns the reference after GetEvents");
    CHECKF(WaitForSingleObject(ISpEventSource_GetNotifyEventHandle(src), 0) == WAIT_TIMEOUT, "the notify event is reset once the queue is empty");

    /* events still queued when the object goes away release their objects */
    ev[0].eEventId = SPEI_VOICE_CHANGE; ev[0].elParamType = SPET_LPARAM_IS_OBJECT; ev[0].lParam = (LPARAM)obj;
    ISpEventSink_AddEvents(sink, ev, 1);
    CHECKF(refs(obj) == r0 + 1, "a queued object is held");
    ISpEventSource_Release(src);
    ISpEventSink_Release(sink);
    ISpMMSysAudio_Release(audio);
    CHECKF(refs(obj) == r0, "releasing the device releases the queued object (%lu vs %lu)", refs(obj), r0);
    IUnknown_Release(obj);

    /* the other notification kinds */
    new_audio(&audio, &src, &sink);
    ISpEventSource_SetInterest(src, SPFEI(SPEI_START_INPUT_STREAM), SPFEI(SPEI_START_INPUT_STREAM));
    memset(ev, 0, sizeof(ev)); ev[0].eEventId = SPEI_START_INPUT_STREAM;
    hr = ISpEventSource_SetNotifyCallbackFunction(src, NULL, 1, 2);
    CHECKF(hr == E_INVALIDARG, "SetNotifyCallbackFunction(NULL) is E_INVALIDARG (%#lx)", hr);
    hr = ISpEventSource_SetNotifyCallbackFunction(src, callback_fn, 7, 8);
    CHECKF(hr == S_OK, "SetNotifyCallbackFunction (%#lx)", hr);
    ISpEventSink_AddEvents(sink, ev, 1);
    CHECKF(g_cb == 1 && g_cb_w == 7 && g_cb_l == 8, "callback function got its arguments (%ld)", g_cb);
    ISpEventSink_Release(sink); ISpEventSource_Release(src); ISpMMSysAudio_Release(audio);

    new_audio(&audio, &src, &sink);
    ISpEventSource_SetInterest(src, SPFEI(SPEI_START_INPUT_STREAM), SPFEI(SPEI_START_INPUT_STREAM));
    hr = ISpEventSource_SetNotifySink(src, NULL);
    CHECKF(hr == E_INVALIDARG, "SetNotifySink(NULL) is E_INVALIDARG (%#lx)", hr);
    hr = ISpEventSource_SetNotifySink(src, &sink_obj);
    CHECKF(hr == S_OK, "SetNotifySink (%#lx)", hr);
    ISpEventSink_AddEvents(sink, ev, 1);
    CHECKF(g_sink == 1, "the sink was notified (%ld)", g_sink);
    ISpEventSink_Release(sink); ISpEventSource_Release(src); ISpMMSysAudio_Release(audio);

    new_audio(&audio, &src, &sink);
    ISpEventSource_SetInterest(src, SPFEI(SPEI_START_INPUT_STREAM), SPFEI(SPEI_START_INPUT_STREAM));
    hr = ISpEventSource_SetNotifyCallbackInterface(src, (ISpNotifyCallback *)&cbi_obj, 5, 6);
    CHECKF(hr == S_OK, "SetNotifyCallbackInterface (%#lx)", hr);
    ISpEventSink_AddEvents(sink, ev, 1);
    CHECKF(g_cbi == 1 && g_cbi_w == 5 && g_cbi_l == 6, "callback interface got its arguments (%ld)", g_cbi);
    ISpEventSink_Release(sink); ISpEventSource_Release(src); ISpMMSysAudio_Release(audio);
}

static void test_streams_and_info(void)
{
    ISpMMSysAudio *audio;
    ISpEventSource *src;
    ISpEventSink *sink;
    LARGE_INTEGER mv;
    ULARGE_INTEGER pos, sz, rd, wr;
    STATSTG st;
    IStream *clone;
    SPAUDIOBUFFERINFO bi;
    WAVEFORMATEX *wfx;
    GUID fmt;
    ULONG n, notify;
    char buf[16];
    HRESULT hr;

    new_audio(&audio, &src, &sink);
    ISpEventSource_Release(src); ISpEventSink_Release(sink);

    hr = ISpMMSysAudio_Read(audio, buf, 4, &n);
    CHECKF(hr == STG_E_ACCESSDENIED, "Read from an output device is STG_E_ACCESSDENIED (%#lx)", hr);
    mv.QuadPart = 0;
    pos.QuadPart = 99;
    hr = ISpMMSysAudio_Seek(audio, mv, STREAM_SEEK_CUR, &pos);
    CHECKF(hr == S_OK && pos.QuadPart == 0, "Seek(0, current) reports position 0 (%#lx)", hr);
    hr = ISpMMSysAudio_Seek(audio, mv, STREAM_SEEK_SET, &pos);
    CHECKF(hr == STG_E_INVALIDFUNCTION, "Seek to the start is STG_E_INVALIDFUNCTION (%#lx)", hr);
    mv.QuadPart = 5;
    hr = ISpMMSysAudio_Seek(audio, mv, STREAM_SEEK_CUR, &pos);
    CHECKF(hr == STG_E_INVALIDFUNCTION, "Seek(5, current) is STG_E_INVALIDFUNCTION (%#lx)", hr);
    sz.QuadPart = 100;
    hr = ISpMMSysAudio_SetSize(audio, sz);
    CHECKF(hr == STG_E_INVALIDFUNCTION, "SetSize is STG_E_INVALIDFUNCTION (%#lx)", hr);
    rd.QuadPart = wr.QuadPart = 7;
    hr = ISpMMSysAudio_CopyTo(audio, NULL, sz, &rd, &wr);
    CHECKF(hr == STG_E_ACCESSDENIED && rd.QuadPart == 0 && wr.QuadPart == 0, "CopyTo is STG_E_ACCESSDENIED with 0 and 0 (%#lx)", hr);
    hr = ISpMMSysAudio_Commit(audio, STGC_DEFAULT);
    CHECKF(hr == S_OK, "Commit is S_OK (%#lx)", hr);
    hr = ISpMMSysAudio_Revert(audio);
    CHECKF(hr == S_OK, "Revert is S_OK (%#lx)", hr);
    hr = ISpMMSysAudio_LockRegion(audio, sz, sz, LOCK_WRITE);
    CHECKF(hr == STG_E_INVALIDFUNCTION, "LockRegion is STG_E_INVALIDFUNCTION (%#lx)", hr);
    hr = ISpMMSysAudio_UnlockRegion(audio, sz, sz, LOCK_WRITE);
    CHECKF(hr == STG_E_INVALIDFUNCTION, "UnlockRegion is STG_E_INVALIDFUNCTION (%#lx)", hr);
    memset(&st, 0xcc, sizeof(st));
    hr = ISpMMSysAudio_Stat(audio, &st, STATFLAG_NONAME);
    CHECKF(hr == S_OK && st.type == STGTY_STREAM && st.cbSize.QuadPart == 0 && st.grfMode == STGM_WRITE && st.pwcsName == NULL, "Stat: a write-only stream of size 0 (%#lx)", hr);
    hr = ISpMMSysAudio_Stat(audio, NULL, 0);
    CHECKF(hr == STG_E_INVALIDPOINTER, "Stat(NULL) is STG_E_INVALIDPOINTER (%#lx)", hr);
    clone = (IStream *)1;
    hr = ISpMMSysAudio_Clone(audio, &clone);
    CHECKF(hr == STG_E_INVALIDFUNCTION && clone == NULL, "Clone is STG_E_INVALIDFUNCTION and NULL (%#lx)", hr);

    /* buffer info and default format */
    memset(&bi, 0xcc, sizeof(bi));
    hr = ISpMMSysAudio_GetBufferInfo(audio, &bi);
    CHECKF(hr == S_OK && bi.ulMsMinNotification == 50 && bi.ulMsBufferSize == 500 && bi.ulMsEventBias == 0, "GetBufferInfo defaults: 50, 500, 0 (%#lx)", hr);
    hr = ISpMMSysAudio_GetBufferInfo(audio, NULL);
    CHECKF(hr == E_POINTER, "GetBufferInfo(NULL) is E_POINTER (%#lx)", hr);
    hr = ISpMMSysAudio_SetBufferInfo(audio, NULL);
    CHECKF(hr == E_INVALIDARG, "SetBufferInfo(NULL) is E_INVALIDARG (%#lx)", hr);
    bi.ulMsMinNotification = 0; bi.ulMsBufferSize = 300; bi.ulMsEventBias = 0;
    hr = ISpMMSysAudio_SetBufferInfo(audio, &bi);
    CHECKF(hr == E_INVALIDARG, "SetBufferInfo with a zero notification is E_INVALIDARG (%#lx)", hr);
    bi.ulMsMinNotification = 20; bi.ulMsBufferSize = 300; bi.ulMsEventBias = 5;
    hr = ISpMMSysAudio_SetBufferInfo(audio, &bi);
    CHECKF(hr == S_OK, "SetBufferInfo (%#lx)", hr);
    memset(&bi, 0, sizeof(bi));
    ISpMMSysAudio_GetBufferInfo(audio, &bi);
    CHECKF(bi.ulMsMinNotification == 20 && bi.ulMsBufferSize == 300 && bi.ulMsEventBias == 5, "GetBufferInfo reads it back");
    hr = ISpMMSysAudio_GetBufferNotifySize(audio, &notify);
    CHECKF(hr == S_OK && notify == 2205, "GetBufferNotifySize default is 50 ms of 22 kHz 16 bit mono, 2205 (%#lx, %lu)", hr, notify);
    hr = ISpMMSysAudio_SetBufferNotifySize(audio, 4000);
    ISpMMSysAudio_GetBufferNotifySize(audio, &notify);
    CHECKF(hr == S_OK && notify == 4000, "SetBufferNotifySize is kept (%#lx)", hr);
    hr = ISpMMSysAudio_GetBufferNotifySize(audio, NULL);
    CHECKF(hr == E_POINTER, "GetBufferNotifySize(NULL) is E_POINTER (%#lx)", hr);

    wfx = NULL;
    hr = ISpMMSysAudio_GetDefaultFormat(audio, &fmt, &wfx);
    CHECKF(hr == S_OK && IsEqualGUID(&fmt, &PR_SPDFID_WaveFormatEx) && wfx && wfx->wFormatTag == WAVE_FORMAT_PCM && wfx->nChannels == 1 &&
           wfx->nSamplesPerSec == 22050 && wfx->wBitsPerSample == 16 && wfx->nBlockAlign == 2 && wfx->nAvgBytesPerSec == 44100 && wfx->cbSize == 0,
           "GetDefaultFormat: 22050 Hz 16 bit mono (%#lx)", hr);
    CoTaskMemFree(wfx);
    hr = ISpMMSysAudio_GetDefaultFormat(audio, NULL, &wfx);
    CHECKF(hr == E_POINTER, "GetDefaultFormat(NULL GUID) is E_POINTER (%#lx)", hr);

    /* closed device */
    {
        SPAUDIOSTATUS status;
        void *handle = (void *)1;
        memset(&status, 0xcc, sizeof(status));
        hr = ISpMMSysAudio_GetStatus(audio, &status);
        CHECKF(hr == S_OK && status.State == SPAS_CLOSED && status.CurSeekPos == 0 && status.CurDevicePos == 0 &&
               status.cbFreeBuffSpace == 300 * 44100 / 1000 && status.cbNonBlockingIO == status.cbFreeBuffSpace,
               "GetStatus of a closed device: free space is the 300 ms buffer, 13230 bytes (%#lx, %lu)", hr, status.cbFreeBuffSpace);
        hr = ISpMMSysAudio_GetStatus(audio, NULL);
        CHECKF(hr == E_POINTER, "GetStatus(NULL) is E_POINTER (%#lx)", hr);
        hr = ISpMMSysAudio_GetMMHandle(audio, &handle);
        CHECKF(hr == SPERR_INVALID_AUDIO_STATE, "GetMMHandle of a closed device is SPERR_INVALID_AUDIO_STATE (%#lx)", hr);
        hr = ISpMMSysAudio_GetMMHandle(audio, NULL);
        CHECKF(hr == E_POINTER, "GetMMHandle(NULL) is E_POINTER (%#lx)", hr);
    }
    hr = ISpMMSysAudio_SetState(audio, (SPAUDIOSTATE)7, 0);
    CHECKF(hr == E_INVALIDARG, "SetState(7) is E_INVALIDARG (%#lx)", hr);
    hr = ISpMMSysAudio_SetVolumeLevel(audio, 10001);
    CHECKF(hr == E_INVALIDARG, "SetVolumeLevel(10001) is E_INVALIDARG (%#lx)", hr);
    hr = ISpMMSysAudio_GetVolumeLevel(audio, NULL);
    CHECKF(hr == E_POINTER, "GetVolumeLevel(NULL) is E_POINTER (%#lx)", hr);

    if (waveOutGetNumDevs() == 0)
    {
        printf("note  no wave out device: open-device checks skipped\n");
        ISpMMSysAudio_Release(audio);
        return;
    }

    /* an open device: write, status, pause, stop */
    {
        SPAUDIOSTATUS status;
        void *handle = NULL;
        ULONG level = 0;
        ULONG written = 0;
        char *data = calloc(1, 4410);

        hr = ISpMMSysAudio_SetState(audio, SPAS_PAUSE, 0);
        CHECKF(hr == S_OK, "SetState(PAUSE) opens the device paused (%#lx)", hr);
        ISpMMSysAudio_GetStatus(audio, &status);
        CHECKF(status.State == SPAS_PAUSE, "GetStatus: paused (%d)", status.State);
        hr = ISpMMSysAudio_GetMMHandle(audio, &handle);
        CHECKF(hr == S_OK && handle != NULL, "GetMMHandle of an open device is a handle (%#lx)", hr);
        hr = ISpMMSysAudio_Write(audio, data, 4410, &written);
        CHECKF(hr == S_OK && written == 4410, "Write while paused queues the data (%#lx, %lu)", hr, written);
        ISpMMSysAudio_GetStatus(audio, &status);
        CHECKF(status.CurSeekPos == 4410, "GetStatus: CurSeekPos counts the bytes written (%s)", status.CurSeekPos == 4410 ? "4410" : "other");
        CHECKF(status.cbFreeBuffSpace <= 300 * 44100 / 1000 - 4410, "GetStatus: the pending bytes are taken off the free space (%lu)", status.cbFreeBuffSpace);
        mv.QuadPart = 0;
        ISpMMSysAudio_Seek(audio, mv, STREAM_SEEK_CUR, &pos);
        CHECKF(pos.QuadPart == 4410, "Seek(0, current) is the bytes written (%s)", pos.QuadPart == 4410 ? "4410" : "other");
        ISpMMSysAudio_Stat(audio, &st, STATFLAG_NONAME);
        CHECKF(st.cbSize.QuadPart == 4410, "Stat: size is the bytes written");
        hr = ISpMMSysAudio_SetState(audio, SPAS_RUN, 0);
        CHECKF(hr == S_OK, "SetState(RUN) from paused (%#lx)", hr);
        ISpMMSysAudio_GetStatus(audio, &status);
        CHECKF(status.State == SPAS_RUN, "GetStatus: running");
        hr = ISpMMSysAudio_SetState(audio, SPAS_STOP, 0);
        CHECKF(hr == S_OK, "SetState(STOP) (%#lx)", hr);
        ISpMMSysAudio_GetStatus(audio, &status);
        CHECKF(status.State == SPAS_STOP, "GetStatus: stopped");
        hr = ISpMMSysAudio_Write(audio, data, 100, NULL);
        CHECKF(hr == SP_AUDIO_STOPPED, "Write while stopped is SP_AUDIO_STOPPED (%#lx)", hr);
        hr = ISpMMSysAudio_GetMMHandle(audio, &handle);
        CHECKF(hr == S_OK, "the device stays open when stopped (%#lx)", hr);
        hr = ISpMMSysAudio_SetState(audio, SPAS_CLOSED, 0);
        CHECKF(hr == S_OK, "SetState(CLOSED) (%#lx)", hr);
        hr = ISpMMSysAudio_GetMMHandle(audio, &handle);
        CHECKF(hr == SPERR_INVALID_AUDIO_STATE, "no handle once closed (%#lx)", hr);

        hr = ISpMMSysAudio_SetVolumeLevel(audio, 5000);
        if (hr == S_OK)
        {
            hr = ISpMMSysAudio_GetVolumeLevel(audio, &level);
            CHECKF(hr == S_OK && level >= 4999 && level <= 5001, "volume 5000 reads back as 5000 (%#lx, %lu)", hr, level);
            ISpMMSysAudio_SetVolumeLevel(audio, 10000);
            ISpMMSysAudio_GetVolumeLevel(audio, &level);
            CHECKF(level == 10000, "volume 10000 reads back (%lu)", level);
        }
        else
            CHECKF(hr == SPERR_GENERIC_MMSYS_ERROR, "SetVolumeLevel on a device without volume control is SPERR_GENERIC_MMSYS_ERROR (%#lx)", hr);
        free(data);
    }
    ISpMMSysAudio_Release(audio);
}

static void test_token(void)
{
    ISpMMSysAudio *audio;
    ISpObjectWithToken *owt;
    ISpObjectToken *tok, *tok2, *got;
    HKEY k;
    UINT id;
    HRESULT hr;

    RegDeleteTreeW(HKEY_CURRENT_USER, L"Software\\SGProbeMM");
    RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\SGProbeMM\\Tokens\\Dev0", 0, NULL, 0, KEY_ALL_ACCESS, NULL, &k, NULL);
    RegSetValueExW(k, L"DeviceId", 0, REG_SZ, (const BYTE *)L"0", 4);
    RegCloseKey(k);
    RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\SGProbeMM\\Tokens\\Junk", 0, NULL, 0, KEY_ALL_ACCESS, NULL, &k, NULL);
    RegSetValueExW(k, L"DeviceId", 0, REG_SZ, (const BYTE *)L"{0.0.0.00000000}.{guid}", 48);
    RegCloseKey(k);
    CoCreateInstance(&CLSID_SpObjectToken, NULL, CLSCTX_INPROC_SERVER, &IID_ISpObjectToken, (void **)&tok);
    ISpObjectToken_SetId(tok, NULL, L"HKEY_CURRENT_USER\\Software\\SGProbeMM\\Tokens\\Dev0", FALSE);
    CoCreateInstance(&CLSID_SpObjectToken, NULL, CLSCTX_INPROC_SERVER, &IID_ISpObjectToken, (void **)&tok2);
    ISpObjectToken_SetId(tok2, NULL, L"HKEY_CURRENT_USER\\Software\\SGProbeMM\\Tokens\\Junk", FALSE);

    CoCreateInstance(&CLSID_SpMMAudioOut, NULL, CLSCTX_INPROC_SERVER, &IID_ISpObjectWithToken, (void **)&owt);
    got = (ISpObjectToken *)1;
    hr = ISpObjectWithToken_GetObjectToken(owt, &got);
    CHECKF(hr == S_FALSE && got == NULL, "GetObjectToken before SetObjectToken is S_FALSE (%#lx)", hr);
    hr = ISpObjectWithToken_SetObjectToken(owt, NULL);
    CHECKF(hr == E_INVALIDARG, "SetObjectToken(NULL) is E_INVALIDARG (%#lx)", hr);
    hr = ISpObjectWithToken_SetObjectToken(owt, tok2);
    CHECKF(hr == S_OK, "SetObjectToken with a device id that is not a number (%#lx)", hr);
    ISpObjectWithToken_QueryInterface(owt, &IID_ISpMMSysAudio, (void **)&audio);
    id = 0;
    ISpMMSysAudio_GetDeviceId(audio, &id);
    CHECKF(id == WAVE_MAPPER, "such a token leaves the wave mapper (%#x)", id);
    hr = ISpObjectWithToken_SetObjectToken(owt, tok);
    CHECKF(hr == SPERR_ALREADY_INITIALIZED, "SetObjectToken twice (%#lx)", hr);
    hr = ISpObjectWithToken_GetObjectToken(owt, &got);
    CHECKF(hr == S_OK && got == tok2, "GetObjectToken returns the token given");
    if (hr == S_OK) ISpObjectToken_Release(got);
    ISpMMSysAudio_Release(audio);
    ISpObjectWithToken_Release(owt);

    if (waveOutGetNumDevs() > 0)
    {
        CoCreateInstance(&CLSID_SpMMAudioOut, NULL, CLSCTX_INPROC_SERVER, &IID_ISpObjectWithToken, (void **)&owt);
        hr = ISpObjectWithToken_SetObjectToken(owt, tok);
        CHECKF(hr == S_OK, "SetObjectToken with DeviceId 0 (%#lx)", hr);
        ISpObjectWithToken_QueryInterface(owt, &IID_ISpMMSysAudio, (void **)&audio);
        id = 99;
        ISpMMSysAudio_GetDeviceId(audio, &id);
        CHECKF(id == 0, "the token's DeviceId selects device 0 (%#x)", id);
        ISpMMSysAudio_Release(audio);
        ISpObjectWithToken_Release(owt);
    }
    ISpObjectToken_Release(tok);
    ISpObjectToken_Release(tok2);
    RegDeleteTreeW(HKEY_CURRENT_USER, L"Software\\SGProbeMM");
}

int main(void)
{
    CoInitialize(NULL);
    test_events();
    test_streams_and_info();
    test_token();
    CoUninitialize();
    printf("%s\n", failures ? "RESULT: FAIL" : "RESULT: PASS");
    return failures ? 1 : 0;
}
