/* The audio renderer's last Release after Shutdown (patches/sg/3763): the wait item that the renderer
 * queues for the audio client's buffer event must not keep the renderer alive after Shutdown, and a
 * render callback that runs at the time of Shutdown ends before Shutdown returns.
 *
 *   probe.exe */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <mfapi.h>
#include <mferror.h>
#include <mfidl.h>
#include <stdio.h>

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}
#define CHECKF(ok, ...) do { char _b[256]; snprintf(_b, sizeof(_b), __VA_ARGS__); check(ok, _b); } while (0)

static IMFMediaType *make_type(void)
{
    IMFMediaType *type;
    if (FAILED(MFCreateMediaType(&type))) return NULL;
    IMFMediaType_SetGUID(type, &MF_MT_MAJOR_TYPE, &MFMediaType_Audio);
    IMFMediaType_SetGUID(type, &MF_MT_SUBTYPE, &MFAudioFormat_Float);
    IMFMediaType_SetUINT32(type, &MF_MT_AUDIO_NUM_CHANNELS, 2);
    IMFMediaType_SetUINT32(type, &MF_MT_AUDIO_BITS_PER_SAMPLE, 32);
    IMFMediaType_SetUINT32(type, &MF_MT_AUDIO_SAMPLES_PER_SECOND, 48000);
    IMFMediaType_SetUINT32(type, &MF_MT_AUDIO_BLOCK_ALIGNMENT, 8);
    IMFMediaType_SetUINT32(type, &MF_MT_AUDIO_AVG_BYTES_PER_SECOND, 8 * 48000);
    return type;
}

/* mode 0: type set, shut down at once; 1: clock started first, then a short wait; 2: started and stopped */
static int run_once(int mode, int delay_ms, int *skipped)
{
    IMFStreamSink *stream;
    IMFMediaTypeHandler *handler;
    IMFClockStateSink *state;
    IMFMediaSink *sink;
    IMFMediaType *type;
    HRESULT hr;
    ULONG ref;

    hr = MFCreateAudioRenderer(NULL, &sink);
    if (hr == MF_E_NO_AUDIO_PLAYBACK_DEVICE) { *skipped = 1; return 1; }
    if (FAILED(hr)) { printf("create failed %#lx\n", hr); return 0; }

    IMFMediaSink_GetStreamSinkByIndex(sink, 0, &stream);
    IMFStreamSink_GetMediaTypeHandler(stream, &handler);
    IMFStreamSink_Release(stream);
    type = make_type();
    hr = IMFMediaTypeHandler_SetCurrentMediaType(handler, type);
    IMFMediaType_Release(type);
    IMFMediaTypeHandler_Release(handler);
    if (FAILED(hr)) { printf("SetCurrentMediaType failed %#lx\n", hr); IMFMediaSink_Release(sink); return 0; }

    if (mode)
    {
        IMFMediaSink_QueryInterface(sink, &IID_IMFClockStateSink, (void **)&state);
        IMFClockStateSink_OnClockStart(state, 0, 0);
        if (delay_ms) Sleep(delay_ms);
        if (mode == 2) IMFClockStateSink_OnClockStop(state, 0);
        IMFClockStateSink_Release(state);
    }

    hr = IMFMediaSink_Shutdown(sink);
    if (FAILED(hr)) { printf("Shutdown failed %#lx\n", hr); IMFMediaSink_Release(sink); return 0; }
    ref = IMFMediaSink_Release(sink);
    if (ref) printf("mode %d delay %d: Release returned %lu\n", mode, delay_ms, ref);
    return ref == 0;
}

int main(void)
{
    int mode, i, bad, skipped = 0;
    HRESULT hr;

    CoInitialize(NULL);
    hr = MFStartup(MF_VERSION, MFSTARTUP_FULL);
    CHECKF(hr == S_OK, "MFStartup %#lx", hr);

    for (mode = 0; mode < 3; mode++)
    {
        bad = 0;
        for (i = 0; i < 40 && !skipped; i++)
            if (!run_once(mode, mode ? (i % 4) * 5 : 0, &skipped)) bad++;
        if (skipped) { printf("note  no audio render device\n"); break; }
        CHECKF(!bad, "mode %d: Release after Shutdown returns 0 (%d of 40 failed)", mode, bad);
    }

    MFShutdown();
    CoUninitialize();
    printf(failures ? "RESULT: FAIL\n" : "RESULT: PASS\n");
    return failures != 0;
}
