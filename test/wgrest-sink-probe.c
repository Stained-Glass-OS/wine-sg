/* winegstreamer media_sink.c: stream sink markers / Flush, sink characteristics,
 * presentation clock, stream sink removal, the stream sink media type handler
 * (patches/sg/2921), run by test/wgrest-sink-gate.sh. Uses MFCreateMPEG4MediaSink.
 *
 *   wgrest-sink-probe.exe */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mferror.h>
#include <propvarutil.h>
#include <stdio.h>

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}
#define CHECKF(ok, ...) do { char _b[300]; snprintf(_b, sizeof(_b), __VA_ARGS__); check(ok, _b); } while (0)
#define CHECKHR(hr, exp, what) CHECKF((hr) == (HRESULT)(exp), "%s: hr %#lx (want %#lx)", what, (unsigned long)(hr), (unsigned long)(HRESULT)(exp))

static const BYTE h264_header[] =
{
    0x00, 0x00, 0x00, 0x01, 0x67, 0x4d, 0x40, 0x0b, 0x96, 0x56, 0x31, 0xb4,
    0x20, 0x00, 0x00, 0x7d, 0x20, 0x00, 0x1d, 0x4c, 0x01, 0xb4, 0x11, 0x08,
    0xa7, 0x00, 0x00, 0x00, 0x01, 0x68, 0xce, 0x3c, 0x80,
};
static const BYTE aac_data[] = { 0x00, 0x00, 0x29, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x12, 0x08 };

static IMFMediaType *make_audio(void)
{
    IMFMediaType *t;
    MFCreateMediaType(&t);
    IMFMediaType_SetGUID(t, &MF_MT_MAJOR_TYPE, &MFMediaType_Audio);
    IMFMediaType_SetGUID(t, &MF_MT_SUBTYPE, &MFAudioFormat_AAC);
    IMFMediaType_SetUINT32(t, &MF_MT_AUDIO_NUM_CHANNELS, 1);
    IMFMediaType_SetUINT32(t, &MF_MT_AUDIO_BITS_PER_SAMPLE, 16);
    IMFMediaType_SetUINT32(t, &MF_MT_AUDIO_SAMPLES_PER_SECOND, 44100);
    IMFMediaType_SetUINT32(t, &MF_MT_AUDIO_AVG_BYTES_PER_SECOND, 12000);
    IMFMediaType_SetUINT32(t, &MF_MT_AAC_AUDIO_PROFILE_LEVEL_INDICATION, 41);
    IMFMediaType_SetUINT32(t, &MF_MT_AAC_PAYLOAD_TYPE, 0);
    IMFMediaType_SetBlob(t, &MF_MT_USER_DATA, aac_data, sizeof(aac_data));
    return t;
}

static IMFMediaType *make_video(void)
{
    IMFMediaType *t;
    MFCreateMediaType(&t);
    IMFMediaType_SetGUID(t, &MF_MT_MAJOR_TYPE, &MFMediaType_Video);
    IMFMediaType_SetGUID(t, &MF_MT_SUBTYPE, &MFVideoFormat_H264);
    IMFMediaType_SetUINT64(t, &MF_MT_FRAME_SIZE, ((UINT64)96 << 32) | 96);
    IMFMediaType_SetUINT64(t, &MF_MT_FRAME_RATE, ((UINT64)30000 << 32) | 1001);
    IMFMediaType_SetBlob(t, &MF_MT_MPEG_SEQUENCE_HEADER, h264_header, sizeof(h264_header));
    return t;
}

static IMFMediaSink *make_sink(IMFMediaType *video, IMFMediaType *audio)
{
    IMFByteStream *bs = NULL;
    IMFMediaSink *sink = NULL;
    HRESULT hr = MFCreateTempFile(MF_ACCESSMODE_WRITE, MF_OPENMODE_DELETE_IF_EXIST, 0, &bs);
    if (hr == S_OK) hr = MFCreateMPEG4MediaSink(bs, video, audio, &sink);
    if (hr != S_OK) { printf("SKIP  cannot create the MPEG4 sink: %#lx\n", (unsigned long)hr); exit(77); }
    IMFByteStream_Release(bs);
    return sink;
}

/* wait for an event of the given type on a stream; returns the event or NULL */
static IMFMediaEvent *wait_event(IMFMediaEventGenerator *gen, MediaEventType want)
{
    IMFMediaEvent *ev;
    int i;
    for (i = 0; i < 300; i++)
    {
        HRESULT hr = IMFMediaEventGenerator_GetEvent(gen, MF_EVENT_FLAG_NO_WAIT, &ev);
        if (hr == S_OK)
        {
            MediaEventType t;
            IMFMediaEvent_GetType(ev, &t);
            if (t == want) return ev;
            IMFMediaEvent_Release(ev);
        }
        else Sleep(10);
    }
    return NULL;
}

/* Is the sink a clock state sink of the clock? Adding it a second time fails
 * when it is; if the add worked it was not, and the probe undoes it. */
static BOOL registered(IMFPresentationClock *clock, IMFClockStateSink *sink)
{
    HRESULT hr = IMFPresentationClock_AddClockStateSink(clock, sink);
    if (hr == S_OK) IMFPresentationClock_RemoveClockStateSink(clock, sink);
    return hr != S_OK;
}

/* ---- characteristics, count, shutdown results, clock ------------------- */
static void test_sink(void)
{
    IMFMediaType *a = make_audio(), *v = make_video();
    IMFMediaSink *sink = make_sink(v, a);
    IMFPresentationClock *clock, *clock2, *got;
    IMFClockStateSink *state_sink;
    IMFGetService *gs;
    IMFStreamSink *ss;
    DWORD flags, count;
    void *unk;
    HRESULT hr;

    flags = 0xdead;
    hr = IMFMediaSink_GetCharacteristics(sink, &flags);
    CHECKF(hr == S_OK && flags == MEDIASINK_RATELESS, "characteristics: rateless, not fixed streams (hr %#lx flags %#lx)", (unsigned long)hr, flags);
    hr = IMFMediaSink_GetCharacteristics(sink, NULL);
    CHECKHR(hr, E_POINTER, "GetCharacteristics(NULL)");

    hr = IMFMediaSink_QueryInterface(sink, &IID_IMFGetService, (void **)&gs);
    CHECKHR(hr, S_OK, "IMFGetService on the sink");
    if (hr == S_OK)
    {
        unk = (void *)1;
        hr = IMFGetService_GetService(gs, &MF_RATE_CONTROL_SERVICE, &IID_IUnknown, &unk);
        CHECKF(hr == MF_E_UNSUPPORTED_SERVICE && !unk, "GetService of a service the sink has none of (hr %#lx)", (unsigned long)hr);
        IMFGetService_Release(gs);
    }

    hr = IMFMediaSink_QueryInterface(sink, &IID_IMFClockStateSink, (void **)&state_sink);
    CHECKHR(hr, S_OK, "IMFClockStateSink on the sink");
    hr = IMFClockStateSink_OnClockSetRate(state_sink, 0, 2.0f);
    CHECKHR(hr, S_OK, "OnClockSetRate");
    hr = IMFClockStateSink_OnClockSetRate(state_sink, 0, 0.5f);
    CHECKHR(hr, S_OK, "OnClockSetRate(0.5)");

    /* presentation clock */
    MFCreatePresentationClock(&clock);
    MFCreatePresentationClock(&clock2);
    got = (void *)1;
    hr = IMFMediaSink_GetPresentationClock(sink, NULL);
    CHECKHR(hr, E_POINTER, "GetPresentationClock(NULL)");
    hr = IMFMediaSink_GetPresentationClock(sink, &got);
    CHECKHR(hr, MF_E_NO_CLOCK, "GetPresentationClock without a clock");
    hr = IMFMediaSink_SetPresentationClock(sink, NULL);
    CHECKHR(hr, S_OK, "SetPresentationClock(NULL)");
    hr = IMFMediaSink_SetPresentationClock(sink, clock);
    CHECKHR(hr, S_OK, "SetPresentationClock(clock)");
    got = NULL;
    hr = IMFMediaSink_GetPresentationClock(sink, &got);
    CHECKF(hr == S_OK && got == clock, "GetPresentationClock returns the clock (hr %#lx)", (unsigned long)hr);
    if (got) IMFPresentationClock_Release(got);
    /* the sink is registered as a state sink of the clock: adding it again is refused */
    CHECKF(registered(clock, state_sink), "the sink is registered with the clock");
    /* a new clock replaces the old */
    hr = IMFMediaSink_SetPresentationClock(sink, clock2);
    CHECKHR(hr, S_OK, "SetPresentationClock(clock2)");
    CHECKF(!registered(clock, state_sink), "the old clock lost the sink");
    CHECKF(registered(clock2, state_sink), "the new clock has the sink");
    got = NULL;
    IMFMediaSink_GetPresentationClock(sink, &got);
    CHECKF(got == clock2, "GetPresentationClock returns the new clock");
    if (got) IMFPresentationClock_Release(got);
    hr = IMFMediaSink_SetPresentationClock(sink, NULL);
    CHECKHR(hr, S_OK, "SetPresentationClock(NULL) detaches");
    CHECKF(!registered(clock2, state_sink), "the second clock lost the sink as well");
    hr = IMFMediaSink_GetPresentationClock(sink, &got);
    CHECKHR(hr, MF_E_NO_CLOCK, "no clock after detaching");
    IMFMediaSink_SetPresentationClock(sink, clock);

    /* stream count and shutdown */
    IMFMediaSink_GetStreamSinkCount(sink, &count);
    CHECKF(count == 2, "two stream sinks (%lu)", count);
    hr = IMFMediaSink_GetStreamSinkByIndex(sink, 0, &ss);
    CHECKHR(hr, S_OK, "stream sink 0");
    IMFStreamSink_Release(ss);

    hr = IMFMediaSink_Shutdown(sink);
    CHECKHR(hr, S_OK, "Shutdown");
    CHECKF(!registered(clock, state_sink), "Shutdown unregistered the sink from its clock");
    hr = IMFMediaSink_GetCharacteristics(sink, &flags);
    CHECKHR(hr, MF_E_SHUTDOWN, "GetCharacteristics after Shutdown");
    hr = IMFMediaSink_GetPresentationClock(sink, &got);
    CHECKHR(hr, MF_E_SHUTDOWN, "GetPresentationClock after Shutdown");
    hr = IMFMediaSink_SetPresentationClock(sink, clock);
    CHECKHR(hr, MF_E_SHUTDOWN, "SetPresentationClock after Shutdown");
    hr = IMFClockStateSink_OnClockSetRate(state_sink, 0, 1.0f);
    CHECKHR(hr, MF_E_SHUTDOWN, "OnClockSetRate after Shutdown");
    hr = IMFMediaSink_GetStreamSinkCount(sink, &count);
    CHECKHR(hr, MF_E_SHUTDOWN, "GetStreamSinkCount after Shutdown");
    hr = IMFMediaSink_GetStreamSinkByIndex(sink, 0, &ss);
    CHECKHR(hr, MF_E_SHUTDOWN, "GetStreamSinkByIndex after Shutdown");
    hr = IMFMediaSink_GetStreamSinkById(sink, 1, &ss);
    CHECKHR(hr, MF_E_SHUTDOWN, "GetStreamSinkById after Shutdown");
    hr = IMFMediaSink_AddStreamSink(sink, 5, a, &ss);
    CHECKHR(hr, MF_E_SHUTDOWN, "AddStreamSink after Shutdown");
    hr = IMFMediaSink_RemoveStreamSink(sink, 1);
    CHECKHR(hr, MF_E_SHUTDOWN, "RemoveStreamSink after Shutdown");

    IMFClockStateSink_Release(state_sink);
    IMFMediaSink_Release(sink);
    IMFPresentationClock_Release(clock);
    IMFPresentationClock_Release(clock2);
    IMFMediaType_Release(a);
    IMFMediaType_Release(v);
}

/* ---- adding / removing stream sinks ------------------------------------ */
static void test_remove(void)
{
    IMFMediaType *a = make_audio(), *v = make_video();
    IMFMediaSink *sink = make_sink(v, a);
    IMFStreamSink *ss, *ss1, *removed;
    DWORD count, id;
    HRESULT hr;

    hr = IMFMediaSink_RemoveStreamSink(sink, 77);
    CHECKHR(hr, MF_E_INVALIDSTREAMNUMBER, "RemoveStreamSink of an unknown id");
    IMFMediaSink_GetStreamSinkById(sink, 1, &removed);
    hr = IMFMediaSink_RemoveStreamSink(sink, 1);
    CHECKHR(hr, S_OK, "RemoveStreamSink(1)");
    IMFMediaSink_GetStreamSinkCount(sink, &count);
    CHECKF(count == 1, "one stream sink left (%lu)", count);
    hr = IMFMediaSink_GetStreamSinkById(sink, 1, &ss);
    CHECKHR(hr, MF_E_INVALIDSTREAMNUMBER, "stream 1 is gone");
    IMFMediaSink_GetStreamSinkByIndex(sink, 0, &ss);
    IMFStreamSink_GetIdentifier(ss, &id);
    CHECKF(id == 2, "stream 2 remains (%lu)", id);
    IMFStreamSink_Release(ss);
    hr = IMFMediaSink_RemoveStreamSink(sink, 1);
    CHECKHR(hr, MF_E_INVALIDSTREAMNUMBER, "RemoveStreamSink(1) twice");

    /* the removed stream sink refuses work */
    hr = IMFStreamSink_Flush(removed);
    CHECKHR(hr, MF_E_STREAMSINK_REMOVED, "Flush on a removed stream sink");
    hr = IMFStreamSink_PlaceMarker(removed, MFSTREAMSINK_MARKER_DEFAULT, NULL, NULL);
    CHECKHR(hr, MF_E_STREAMSINK_REMOVED, "PlaceMarker on a removed stream sink");
    IMFStreamSink_Release(removed);

    /* the id can be used again, the stream sink goes to the end */
    hr = IMFMediaSink_AddStreamSink(sink, 1, v, &ss1);
    CHECKHR(hr, S_OK, "AddStreamSink(1) again");
    IMFMediaSink_GetStreamSinkByIndex(sink, 1, &ss);
    IMFStreamSink_GetIdentifier(ss, &id);
    CHECKF(id == 1 && ss == ss1, "the new stream sink 1 is last (%lu)", id);
    IMFStreamSink_Release(ss);
    IMFStreamSink_Release(ss1);
    hr = IMFMediaSink_AddStreamSink(sink, 1, v, &ss);
    CHECKF(hr == MF_E_STREAMSINK_EXISTS && !ss, "AddStreamSink of an existing id (hr %#lx)", (unsigned long)hr);
    hr = IMFMediaSink_AddStreamSink(sink, 9, NULL, &ss);
    CHECKHR(hr, E_POINTER, "AddStreamSink without a media type");

    IMFMediaSink_Shutdown(sink);
    IMFMediaSink_Release(sink);
    IMFMediaType_Release(a);
    IMFMediaType_Release(v);
}

/* ---- the media type handler ------------------------------------------- */
static void test_type_handler(void)
{
    IMFMediaType *a = make_audio(), *v = make_video(), *t, *t2, *other;
    IMFMediaSink *sink = make_sink(v, a);
    IMFMediaTypeHandler *h, *hv;
    IMFStreamSink *ss, *sv;
    DWORD count;
    UINT32 ch;
    GUID guid;
    HRESULT hr;

    IMFMediaSink_GetStreamSinkById(sink, 2, &ss);
    IMFMediaSink_GetStreamSinkById(sink, 1, &sv);
    IMFStreamSink_GetMediaTypeHandler(ss, &h);
    IMFStreamSink_GetMediaTypeHandler(sv, &hv);

    hr = IMFMediaTypeHandler_GetMajorType(h, NULL);
    CHECKHR(hr, E_POINTER, "GetMajorType(NULL)");
    hr = IMFMediaTypeHandler_GetMajorType(h, &guid);
    CHECKF(hr == S_OK && IsEqualGUID(&guid, &MFMediaType_Audio), "audio stream: major type audio (hr %#lx)", (unsigned long)hr);
    hr = IMFMediaTypeHandler_GetMajorType(hv, &guid);
    CHECKF(hr == S_OK && IsEqualGUID(&guid, &MFMediaType_Video), "video stream: major type video (hr %#lx)", (unsigned long)hr);

    hr = IMFMediaTypeHandler_GetMediaTypeCount(h, NULL);
    CHECKHR(hr, E_POINTER, "GetMediaTypeCount(NULL)");
    count = 0xdead;
    hr = IMFMediaTypeHandler_GetMediaTypeCount(h, &count);
    CHECKF(hr == S_OK && count == 1, "one media type (hr %#lx count %lu)", (unsigned long)hr, count);
    hr = IMFMediaTypeHandler_GetMediaTypeByIndex(h, 0, NULL);
    CHECKHR(hr, E_POINTER, "GetMediaTypeByIndex(NULL)");
    t = NULL;
    hr = IMFMediaTypeHandler_GetMediaTypeByIndex(h, 0, &t);
    CHECKHR(hr, S_OK, "GetMediaTypeByIndex(0)");
    if (t)
    {
        BOOL eq = FALSE;
        IMFMediaType_Compare(t, (IMFAttributes *)a, MF_ATTRIBUTES_MATCH_ALL_ITEMS, &eq);
        CHECKF(eq, "the media type at index 0 is the one the stream was added with");
        CHECKF(t != a, "and it is a copy");
        IMFMediaType_Release(t);
    }
    t = (void *)1;
    hr = IMFMediaTypeHandler_GetMediaTypeByIndex(h, 1, &t);
    CHECKHR(hr, MF_E_NO_MORE_TYPES, "GetMediaTypeByIndex(1)");

    /* IsMediaTypeSupported */
    hr = IMFMediaTypeHandler_IsMediaTypeSupported(h, NULL, NULL);
    CHECKHR(hr, E_POINTER, "IsMediaTypeSupported(NULL)");
    t = (void *)1;
    hr = IMFMediaTypeHandler_IsMediaTypeSupported(h, a, &t);
    CHECKF(hr == S_OK && !t, "the stream's own type is supported, no closest type (hr %#lx)", (unsigned long)hr);
    hr = IMFMediaTypeHandler_IsMediaTypeSupported(h, a, NULL);
    CHECKHR(hr, S_OK, "IsMediaTypeSupported without out type");
    hr = IMFMediaTypeHandler_IsMediaTypeSupported(h, v, NULL);
    CHECKHR(hr, MF_E_INVALIDMEDIATYPE, "a video type on the audio stream");
    MFCreateMediaType(&other);
    hr = IMFMediaTypeHandler_IsMediaTypeSupported(h, other, NULL);
    CHECKHR(hr, MF_E_INVALIDMEDIATYPE, "an empty media type");
    IMFMediaType_SetGUID(other, &MF_MT_MAJOR_TYPE, &MFMediaType_Audio);
    IMFMediaType_SetGUID(other, &MF_MT_SUBTYPE, &MFAudioFormat_PCM);
    hr = IMFMediaTypeHandler_IsMediaTypeSupported(h, other, NULL);
    CHECKHR(hr, MF_E_INVALIDMEDIATYPE, "another audio subtype");

    /* SetCurrentMediaType */
    hr = IMFMediaTypeHandler_SetCurrentMediaType(h, NULL);
    CHECKHR(hr, E_POINTER, "SetCurrentMediaType(NULL)");
    hr = IMFMediaTypeHandler_SetCurrentMediaType(h, other);
    CHECKHR(hr, MF_E_INVALIDMEDIATYPE, "SetCurrentMediaType with another subtype");
    hr = IMFMediaTypeHandler_SetCurrentMediaType(h, v);
    CHECKHR(hr, MF_E_INVALIDMEDIATYPE, "SetCurrentMediaType with a video type");
    MFCreateMediaType(&t2);
    IMFMediaType_CopyAllItems(a, (IMFAttributes *)t2);
    IMFMediaType_SetUINT32(t2, &MF_MT_AUDIO_NUM_CHANNELS, 2);
    hr = IMFMediaTypeHandler_SetCurrentMediaType(h, t2);
    CHECKHR(hr, S_OK, "SetCurrentMediaType with 2 channels");
    t = NULL;
    hr = IMFMediaTypeHandler_GetCurrentMediaType(h, &t);
    ch = 0;
    if (t) IMFMediaType_GetUINT32(t, &MF_MT_AUDIO_NUM_CHANNELS, &ch);
    CHECKF(hr == S_OK && t == t2 && ch == 2, "the current media type is the new one (channels %u)", ch);
    if (t) IMFMediaType_Release(t);
    hr = IMFMediaTypeHandler_GetCurrentMediaType(hv, &t);
    CHECKF(hr == S_OK && t == v, "the other stream keeps its type");
    if (t) IMFMediaType_Release(t);
    IMFMediaType_Release(t2);
    IMFMediaType_Release(other);

    IMFMediaTypeHandler_Release(h);
    IMFMediaTypeHandler_Release(hv);
    IMFMediaSink_Shutdown(sink);

    /* the stream sink objects outlive the shutdown */
    IMFStreamSink_GetMediaTypeHandler(ss, &h);
    hr = IMFMediaTypeHandler_GetMajorType(h, &guid);
    CHECKF(hr == S_OK && IsEqualGUID(&guid, &MFMediaType_Audio), "GetMajorType after Shutdown");
    hr = IMFMediaTypeHandler_SetCurrentMediaType(h, a);
    CHECKHR(hr, MF_E_STREAMSINK_REMOVED, "SetCurrentMediaType after Shutdown");
    IMFMediaTypeHandler_Release(h);
    IMFStreamSink_Release(ss);
    IMFStreamSink_Release(sv);
    IMFMediaSink_Release(sink);
    IMFMediaType_Release(a);
    IMFMediaType_Release(v);
}

/* ---- markers and Flush ------------------------------------------------ */
static void test_markers(void)
{
    IMFMediaType *a = make_audio(), *v = make_video();
    IMFMediaSink *sink = make_sink(v, a);
    IMFStreamSink *ss, *sv;
    IMFMediaEventGenerator *gen;
    IMFMediaEvent *ev;
    PROPVARIANT ctx, val;
    HRESULT hr;
    int i;

    IMFMediaSink_GetStreamSinkById(sink, 2, &ss);
    IMFMediaSink_GetStreamSinkById(sink, 1, &sv);
    IMFStreamSink_QueryInterface(ss, &IID_IMFMediaEventGenerator, (void **)&gen);

    PropVariantInit(&ctx);
    ctx.vt = VT_I4;
    ctx.lVal = 4711;
    hr = IMFStreamSink_PlaceMarker(ss, MFSTREAMSINK_MARKER_DEFAULT, NULL, &ctx);
    CHECKHR(hr, S_OK, "PlaceMarker(DEFAULT, context 4711)");
    ev = wait_event(gen, MEStreamSinkMarker);
    CHECKF(ev != NULL, "MEStreamSinkMarker is raised");
    if (ev)
    {
        PropVariantInit(&val);
        hr = IMFMediaEvent_GetValue(ev, &val);
        CHECKF(hr == S_OK && val.vt == VT_I4 && val.lVal == 4711, "the marker event carries the context (hr %#lx vt %d)", (unsigned long)hr, val.vt);
        PropVariantClear(&val);
        IMFMediaEvent_Release(ev);
    }
    hr = IMFStreamSink_PlaceMarker(ss, MFSTREAMSINK_MARKER_ENDOFSEGMENT, NULL, NULL);
    CHECKHR(hr, S_OK, "PlaceMarker(ENDOFSEGMENT, no context)");
    ev = wait_event(gen, MEStreamSinkMarker);
    CHECKF(ev != NULL, "its MEStreamSinkMarker is raised");
    if (ev)
    {
        PropVariantInit(&val);
        IMFMediaEvent_GetValue(ev, &val);
        CHECKF(val.vt == VT_EMPTY, "with no value (vt %d)", val.vt);
        IMFMediaEvent_Release(ev);
    }

    /* markers come out in order, and survive a Flush */
    for (i = 0; i < 3; i++)
    {
        ctx.lVal = 100 + i;
        PropVariantClear(&ctx);
        ctx.vt = VT_I4;
        ctx.lVal = 100 + i;
        hr = IMFStreamSink_PlaceMarker(ss, MFSTREAMSINK_MARKER_TICK, NULL, &ctx);
        CHECKHR(hr, S_OK, "PlaceMarker(TICK)");
        if (i == 1)
        {
            hr = IMFStreamSink_Flush(ss);
            CHECKHR(hr, S_OK, "Flush with markers queued");
        }
    }
    for (i = 0; i < 3; i++)
    {
        ev = wait_event(gen, MEStreamSinkMarker);
        CHECKF(ev != NULL, "marker %d is signalled although a Flush came in between", i);
        if (ev)
        {
            PropVariantInit(&val);
            IMFMediaEvent_GetValue(ev, &val);
            CHECKF(val.vt == VT_I4 && val.lVal == 100 + i, "marker %d arrives in order (%ld)", i, val.lVal);
            IMFMediaEvent_Release(ev);
        }
    }
    hr = IMFStreamSink_Flush(ss);
    CHECKHR(hr, S_OK, "Flush again");
    hr = IMFStreamSink_Flush(sv);
    CHECKHR(hr, S_OK, "Flush of the video stream sink");

    /* a marker of one stream is not seen on the other */
    {
        IMFMediaEventGenerator *genv;
        IMFStreamSink_QueryInterface(sv, &IID_IMFMediaEventGenerator, (void **)&genv);
        ev = wait_event(genv, MEStreamSinkMarker);
        CHECKF(ev == NULL, "the video stream sink saw none of the audio markers");
        if (ev) IMFMediaEvent_Release(ev);
        IMFMediaEventGenerator_Release(genv);
    }

    IMFMediaSink_Shutdown(sink);
    hr = IMFStreamSink_PlaceMarker(ss, MFSTREAMSINK_MARKER_DEFAULT, NULL, NULL);
    CHECKHR(hr, MF_E_STREAMSINK_REMOVED, "PlaceMarker after Shutdown");
    hr = IMFStreamSink_Flush(ss);
    CHECKHR(hr, MF_E_STREAMSINK_REMOVED, "Flush after Shutdown");

    IMFMediaEventGenerator_Release(gen);
    IMFStreamSink_Release(ss);
    IMFStreamSink_Release(sv);
    IMFMediaSink_Release(sink);
    IMFMediaType_Release(a);
    IMFMediaType_Release(v);
}

int main(void)
{
    HRESULT hr;

    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    hr = MFStartup(MF_VERSION, MFSTARTUP_FULL);
    if (hr != S_OK) { printf("SKIP  MFStartup %#lx\n", (unsigned long)hr); return 77; }

    test_sink();
    test_remove();
    test_type_handler();
    test_markers();

    MFShutdown();
    printf("%s\nRESULT: %s\n", failures ? "FAILURES" : "all checks passed", failures ? "FAIL" : "PASS");
    return failures ? 1 : 0;
}
