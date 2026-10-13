/* Probe for patches/sg/4590: the synthesizer applies the DLS level 1
 * articulation of a DMUS_DOWNLOADINFO_INSTRUMENT download (DMUS_ARTICPARAMS):
 * a note of an instrument with a two second volume attack starts far quieter
 * than the same note with an instant attack. The sink and download layout
 * follow Wine's dmsynth conformance test. Lines "ok"/"FAIL", last
 * "RESULT: PASS|FAIL". */
#define COBJMACROS
#define INITGUID
#include <windows.h>
#include <initguid.h>
#include <dmusicc.h>
#include <dmusics.h>
#include <dmdls.h>
#include <math.h>
#include <stdio.h>
#include <stddef.h>

static int failures;
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

struct test_sink
{
    IDirectMusicSynthSink IDirectMusicSynthSink_iface;
    IReferenceClock IReferenceClock_iface;
    LONG refcount;
    IReferenceClock *clock;
    IDirectMusicSynth *synth;
    REFERENCE_TIME activate_time, latency_time;
    DWORD written;
};
#define SINK(i) CONTAINING_RECORD(i, struct test_sink, IDirectMusicSynthSink_iface)
#define CLOCK(i) CONTAINING_RECORD(i, struct test_sink, IReferenceClock_iface)

static HRESULT WINAPI sink_qi(IDirectMusicSynthSink *iface, REFIID iid, void **out) { *out = NULL; return E_NOINTERFACE; }
static ULONG WINAPI sink_addref(IDirectMusicSynthSink *iface) { return InterlockedIncrement(&SINK(iface)->refcount); }
static ULONG WINAPI sink_release(IDirectMusicSynthSink *iface) { return InterlockedDecrement(&SINK(iface)->refcount); }
static HRESULT WINAPI sink_init(IDirectMusicSynthSink *iface, IDirectMusicSynth *synth) { SINK(iface)->synth = synth; return S_OK; }
static HRESULT WINAPI sink_setclock(IDirectMusicSynthSink *iface, IReferenceClock *clock)
{
    struct test_sink *sink = SINK(iface);
    if (sink->clock) IReferenceClock_Release(sink->clock);
    if ((sink->clock = clock)) IReferenceClock_AddRef(clock);
    return S_OK;
}
static HRESULT WINAPI sink_latency(IDirectMusicSynthSink *iface, IReferenceClock **clock)
{
    *clock = &SINK(iface)->IReferenceClock_iface;
    IReferenceClock_AddRef(*clock);
    return S_OK;
}
static HRESULT WINAPI sink_activate(IDirectMusicSynthSink *iface, BOOL enable)
{
    struct test_sink *sink = SINK(iface);
    if (!sink->clock) return DMUS_E_NO_MASTER_CLOCK;
    IReferenceClock_GetTime(sink->clock, &sink->activate_time);
    sink->latency_time = sink->activate_time;
    return S_OK;
}
static HRESULT WINAPI sink_s2t(IDirectMusicSynthSink *iface, LONGLONG sample, REFERENCE_TIME *time)
{
    *time = SINK(iface)->activate_time + ((sample * 10000) / 44100) * 1000;
    return S_OK;
}
static HRESULT WINAPI sink_t2s(IDirectMusicSynthSink *iface, REFERENCE_TIME time, LONGLONG *sample)
{
    *sample = (((time - SINK(iface)->activate_time) / 1000) * 44100) / 10000;
    return S_OK;
}
static HRESULT WINAPI sink_setds(IDirectMusicSynthSink *iface, IDirectSound *ds, IDirectSoundBuffer *b) { return E_NOTIMPL; }
static HRESULT WINAPI sink_bufsize(IDirectMusicSynthSink *iface, DWORD *size) { return E_NOTIMPL; }
static IDirectMusicSynthSinkVtbl sink_vtbl = {sink_qi, sink_addref, sink_release, sink_init, sink_setclock, sink_latency,
                                              sink_activate, sink_s2t, sink_t2s, sink_setds, sink_bufsize};

static HRESULT WINAPI lc_qi(IReferenceClock *iface, REFIID iid, void **out) { *out = NULL; return E_NOINTERFACE; }
static ULONG WINAPI lc_addref(IReferenceClock *iface) { return InterlockedIncrement(&CLOCK(iface)->refcount); }
static ULONG WINAPI lc_release(IReferenceClock *iface) { return InterlockedDecrement(&CLOCK(iface)->refcount); }
static HRESULT WINAPI lc_gettime(IReferenceClock *iface, REFERENCE_TIME *time) { *time = CLOCK(iface)->latency_time; return S_OK; }
static HRESULT WINAPI lc_advise(IReferenceClock *iface, REFERENCE_TIME b, REFERENCE_TIME o, HEVENT e, DWORD_PTR *c) { return E_NOTIMPL; }
static HRESULT WINAPI lc_periodic(IReferenceClock *iface, REFERENCE_TIME s, REFERENCE_TIME p, HSEMAPHORE h, DWORD_PTR *c) { return E_NOTIMPL; }
static HRESULT WINAPI lc_unadvise(IReferenceClock *iface, DWORD_PTR c) { return E_NOTIMPL; }
static IReferenceClockVtbl lc_vtbl = {lc_qi, lc_addref, lc_release, lc_gettime, lc_advise, lc_periodic, lc_unadvise};

static void sink_render(struct test_sink *sink, short *buffer, DWORD size)
{
    memset(buffer, 0, size);
    IDirectMusicSynth_Render(sink->synth, buffer, size / 4, sink->written / 4);
    sink->written += size;
    sink_s2t(&sink->IDirectMusicSynthSink_iface, sink->written / 4, &sink->latency_time);
}

/* the RMS of the first rendered 40 ms of a note with this volume attack, or -1 */
static double note_rms(LONG attack)
{
    struct wave_download
    {
        DMUS_DOWNLOADINFO info;
        ULONG offsets[2];
        DMUS_WAVE wave;
        union { DMUS_WAVEDATA wave_data; struct { ULONG size; BYTE samples[256]; }; };
    } wave = {0};
    struct instrument_download
    {
        DMUS_DOWNLOADINFO info;
        ULONG offsets[4];
        DMUS_INSTRUMENT instrument;
        DMUS_REGION region;
        DMUS_ARTICULATION articulation;
        DMUS_ARTICPARAMS params;
    } instrument = {0};
    DMUS_PORTPARAMS port_params = {sizeof(port_params)};
    DMUS_BUFFERDESC buffer_desc = {sizeof(buffer_desc)};
    struct test_sink sink = {{&sink_vtbl}, {&lc_vtbl}, 1};
    IDirectMusicSynth *synth;
    IDirectMusicBuffer *buffer;
    IReferenceClock *clock;
    IDirectMusic *music;
    HANDLE wave_handle, instrument_handle;
    REFERENCE_TIME time;
    BOOL can_free;
    static short samples[1024];
    double sum = 0;
    DWORD len, i, n = 0;
    BYTE *raw;

    if (FAILED(CoCreateInstance(&CLSID_DirectMusic, NULL, CLSCTX_INPROC_SERVER, &IID_IDirectMusic, (void **)&music)))
        return -1;
    IDirectMusic_GetMasterClock(music, NULL, &clock);
    if (FAILED(CoCreateInstance(&CLSID_DirectMusicSynth, NULL, CLSCTX_INPROC_SERVER, &IID_IDirectMusicSynth, (void **)&synth)))
        return -1;
    port_params.dwValidParams = DMUS_PORTPARAMS_AUDIOCHANNELS | DMUS_PORTPARAMS_SAMPLERATE;
    port_params.dwAudioChannels = 2;
    port_params.dwSampleRate = 44100;
    IDirectMusicSynth_Open(synth, &port_params);
    IDirectMusicSynth_SetSynthSink(synth, &sink.IDirectMusicSynthSink_iface);
    sink_setclock(&sink.IDirectMusicSynthSink_iface, clock);
    IDirectMusicSynth_Activate(synth, TRUE);
    sink_render(&sink, samples, sizeof(samples));

    wave.info.dwDLType = DMUS_DOWNLOADINFO_WAVE; wave.info.dwDLId = 1; wave.info.dwNumOffsetTableEntries = 2;
    wave.info.cbSize = sizeof(wave);
    wave.offsets[0] = offsetof(struct wave_download, wave); wave.offsets[1] = offsetof(struct wave_download, wave_data);
    wave.wave.ulWaveDataIdx = 1;
    wave.wave.WaveformatEx.wFormatTag = WAVE_FORMAT_PCM; wave.wave.WaveformatEx.nChannels = 1;
    wave.wave.WaveformatEx.wBitsPerSample = 8; wave.wave.WaveformatEx.nSamplesPerSec = 44100;
    wave.wave.WaveformatEx.nAvgBytesPerSec = 44100; wave.wave.WaveformatEx.nBlockAlign = 1;
    wave.size = sizeof(wave.samples);
    for (i = 0; i < sizeof(wave.samples); i++) wave.samples[i] = (i / 16) & 1 ? 0xe0 : 0x20;  /* a square wave */
    if (FAILED(IDirectMusicSynth_Download(synth, &wave_handle, &wave, &can_free))) return -1;

    instrument.info.dwDLType = DMUS_DOWNLOADINFO_INSTRUMENT; instrument.info.dwDLId = 2;
    instrument.info.dwNumOffsetTableEntries = 4; instrument.info.cbSize = sizeof(instrument);
    instrument.offsets[0] = offsetof(struct instrument_download, instrument);
    instrument.offsets[1] = offsetof(struct instrument_download, region);
    instrument.offsets[2] = offsetof(struct instrument_download, articulation);
    instrument.offsets[3] = offsetof(struct instrument_download, params);
    instrument.instrument.ulFirstRegionIdx = 1; instrument.instrument.ulGlobalArtIdx = 2;
    instrument.region.RangeKey.usHigh = 127; instrument.region.RangeVelocity.usHigh = 127;
    instrument.region.fusOptions = F_RGN_OPTION_SELFNONEXCLUSIVE;
    instrument.region.WaveLink.ulChannel = 1; instrument.region.WaveLink.ulTableIndex = 1;
    instrument.region.WSMP.cbSize = sizeof(WSMPL); instrument.region.WSMP.usUnityNote = 60;
    instrument.region.WSMP.fulOptions = F_WSMP_NO_TRUNCATION; instrument.region.WSMP.cSampleLoops = 1;
    instrument.region.WLOOP[0].cbSize = sizeof(WLOOP); instrument.region.WLOOP[0].ulType = WLOOP_TYPE_FORWARD;
    instrument.region.WLOOP[0].ulStart = 0; instrument.region.WLOOP[0].ulLength = 256;
    instrument.articulation.ulArt1Idx = 3;
    instrument.params.VolEG.tcAttack = attack;
    instrument.params.VolEG.tcDecay = 0x7fffffff;
    instrument.params.VolEG.ptSustain = 1000 << 16;  /* 100.0 % */
    instrument.params.VolEG.tcRelease = 0x80000000;
    instrument.params.PitchEG.tcAttack = instrument.params.PitchEG.tcDecay = instrument.params.PitchEG.tcRelease = 0x80000000;
    instrument.params.LFO.tcDelay = 0x80000000;
    if (FAILED(IDirectMusicSynth_Download(synth, &instrument_handle, &instrument, &can_free))) return -1;

    buffer_desc.cbBuffer = 4096;
    IDirectMusic_CreateMusicBuffer(music, &buffer_desc, &buffer, NULL);
    IDirectMusicBuffer_PackStructured(buffer, 0, 1, 0x7f3c90);  /* note on, key 60, velocity 127 */
    lc_gettime(&sink.IReferenceClock_iface, &time);
    IDirectMusicBuffer_GetRawBufferPtr(buffer, &raw);
    IDirectMusicBuffer_GetUsedBytes(buffer, &len);
    IDirectMusicSynth_PlayBuffer(synth, time, raw, len);
    IDirectMusicBuffer_Release(buffer);

    for (i = 0; i < 7; i++)  /* 7 x 256 frames, about 40 ms */
    {
        DWORD j;
        sink_render(&sink, samples, sizeof(samples));
        for (j = 0; j < ARRAYSIZE(samples); j++) { sum += (double)samples[j] * samples[j]; n++; }
    }
    IDirectMusicSynth_Activate(synth, FALSE);
    IDirectMusicSynth_SetSynthSink(synth, NULL);
    IDirectMusicSynth_Close(synth);
    IDirectMusicSynth_Release(synth);
    IReferenceClock_Release(clock);
    IDirectMusic_Release(music);
    return sqrt(sum / n);
}

int main(int argc, char **argv)
{
    double instant, slow;

    CoInitialize(NULL);
    instant = note_rms(0x80000000);             /* no attack */
    slow = note_rms(1200 * 65536);              /* 2^(1200/1200) = 2 seconds */
    check(instant > 100, "a note with an instant attack sounds at once: RMS %.1f", instant);
    check(slow >= 0 && slow < instant * 0.3, "a note with a 2 s attack starts quieter: RMS %.1f (instant %.1f)", slow, instant);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
