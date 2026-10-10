/* dmsynth's FIXME stubs (patches/sg/2970), run by test/dmusic-synth-gate.sh:
 * IDirectMusicSynth(8) channel groups and priorities, running statistics, the
 * voice methods, download type validation and the wave unload callback.
 *
 *   dmusic-synth-probe.exe */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <dmusici.h>
#include <dmusics.h>
#include <dmerror.h>
#include <dmdls.h>
#include <stdio.h>
#include <stddef.h>
#include <string.h>

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}
#define CHECKF(ok, ...) do { char _b[256]; snprintf(_b, sizeof(_b), __VA_ARGS__); check(ok, _b); } while (0)

static int cb_called;
static HANDLE cb_handle, cb_data;
static HRESULT CALLBACK unload_cb(HANDLE handle, HANDLE data)
{
    cb_called++;
    cb_handle = handle;
    cb_data = data;
    return S_OK;
}

struct wave_download
{
    DMUS_DOWNLOADINFO info;
    ULONG offsets[2];
    DMUS_WAVE wave;
    ULONG size;
    BYTE samples[256];
};

struct instrument_download
{
    DMUS_DOWNLOADINFO info;
    ULONG offsets[4];
    DMUS_INSTRUMENT instrument;
    DMUS_REGION region;
    DMUS_ARTICULATION articulation;
    DMUS_ARTICPARAMS artic_params;
};

static void init_wave(struct wave_download *w, DWORD id)
{
    memset(w, 0, sizeof(*w));
    w->info.dwDLType = DMUS_DOWNLOADINFO_WAVE;
    w->info.dwDLId = id;
    w->info.dwNumOffsetTableEntries = 2;
    w->info.cbSize = sizeof(*w);
    w->offsets[0] = offsetof(struct wave_download, wave);
    w->offsets[1] = offsetof(struct wave_download, size);
    w->wave.ulWaveDataIdx = 1;
    w->wave.WaveformatEx.wFormatTag = WAVE_FORMAT_PCM;
    w->wave.WaveformatEx.nChannels = 1;
    w->wave.WaveformatEx.wBitsPerSample = 8;
    w->wave.WaveformatEx.nSamplesPerSec = 44100;
    w->wave.WaveformatEx.nAvgBytesPerSec = 44100;
    w->wave.WaveformatEx.nBlockAlign = 1;
    w->size = sizeof(w->samples);
}

static void init_instrument(struct instrument_download *i, DWORD id, DWORD wave_id)
{
    memset(i, 0, sizeof(*i));
    i->info.dwDLType = DMUS_DOWNLOADINFO_INSTRUMENT;
    i->info.dwDLId = id;
    i->info.dwNumOffsetTableEntries = 4;
    i->info.cbSize = sizeof(*i);
    i->offsets[0] = offsetof(struct instrument_download, instrument);
    i->offsets[1] = offsetof(struct instrument_download, region);
    i->offsets[2] = offsetof(struct instrument_download, articulation);
    i->offsets[3] = offsetof(struct instrument_download, artic_params);
    i->instrument.ulFirstRegionIdx = 1;
    i->instrument.ulGlobalArtIdx = 2;
    i->region.RangeKey.usHigh = 127;
    i->region.RangeVelocity.usHigh = 127;
    i->region.WaveLink.ulChannel = 1;
    i->region.WaveLink.ulTableIndex = wave_id;
    i->region.WSMP.cbSize = sizeof(WSMPL);
    i->region.WSMP.usUnityNote = 60;
    i->articulation.ulArt1Idx = 3;
    i->artic_params.VolEG.tcAttack = 32768u << 16;
    i->artic_params.VolEG.tcDecay = 32768u << 16;
    i->artic_params.VolEG.ptSustain = 10000 << 16;
    i->artic_params.VolEG.tcRelease = 32768u << 16;
}

#define DEFPRIO(n) DAUD_CHAN##n##_DEF_VOICE_PRIORITY

int main(void)
{
    static const DWORD prio_default[16] =
    {
        DAUD_CHAN1_DEF_VOICE_PRIORITY, DAUD_CHAN2_DEF_VOICE_PRIORITY, DAUD_CHAN3_DEF_VOICE_PRIORITY,
        DAUD_CHAN4_DEF_VOICE_PRIORITY, DAUD_CHAN5_DEF_VOICE_PRIORITY, DAUD_CHAN6_DEF_VOICE_PRIORITY,
        DAUD_CHAN7_DEF_VOICE_PRIORITY, DAUD_CHAN8_DEF_VOICE_PRIORITY, DAUD_CHAN9_DEF_VOICE_PRIORITY,
        DAUD_CHAN10_DEF_VOICE_PRIORITY, DAUD_CHAN11_DEF_VOICE_PRIORITY, DAUD_CHAN12_DEF_VOICE_PRIORITY,
        DAUD_CHAN13_DEF_VOICE_PRIORITY, DAUD_CHAN14_DEF_VOICE_PRIORITY, DAUD_CHAN15_DEF_VOICE_PRIORITY,
        DAUD_CHAN16_DEF_VOICE_PRIORITY,
    };
    struct wave_download wave, wave2;
    struct instrument_download inst;
    IDirectMusicSynth *synth;
    IDirectMusicSynth8 *synth8;
    DMUS_SYNTHSTATS stats;
    DMUS_VOICE_STATE states[2];
    DWORD voices[2] = {1, 2};
    DWORD buses[2] = {0, 1};
    DWORD prio, i;
    BOOL can_free;
    HANDLE wh, wh2, ih;
    HRESULT hr;

    CoInitialize(NULL);
    hr = CoCreateInstance(&CLSID_DirectMusicSynth, NULL, CLSCTX_INPROC_SERVER, &IID_IDirectMusicSynth, (void **)&synth);
    CHECKF(hr == S_OK, "create the synth (%#lx)", hr);
    if (hr != S_OK) { printf("RESULT: FAIL\n"); return 1; }
    hr = IDirectMusicSynth_QueryInterface(synth, &IID_IDirectMusicSynth8, (void **)&synth8);
    CHECKF(hr == S_OK, "IDirectMusicSynth8 (%#lx)", hr);

    /* closed synth */
    hr = IDirectMusicSynth_SetNumChannelGroups(synth, 1);
    CHECKF(hr == DMUS_E_SYNTHNOTCONFIGURED, "SetNumChannelGroups needs Open (%#lx)", hr);
    hr = IDirectMusicSynth8_PlayVoice(synth8, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0);
    CHECKF(hr == DMUS_E_SYNTHNOTCONFIGURED, "PlayVoice needs Open (%#lx)", hr);
    hr = IDirectMusicSynth_Unload(synth, (HANDLE)0, NULL, NULL);
    CHECKF(hr == DMUS_E_SYNTHNOTCONFIGURED, "Unload needs Open (%#lx)", hr);

    hr = IDirectMusicSynth_Open(synth, NULL);
    CHECKF(hr == S_OK, "Open (%#lx)", hr);

    /* channel groups */
    hr = IDirectMusicSynth_SetNumChannelGroups(synth, 0);
    CHECKF(hr == E_INVALIDARG, "SetNumChannelGroups(0) (%#lx)", hr);
    hr = IDirectMusicSynth_SetNumChannelGroups(synth, 1001);
    CHECKF(hr == E_INVALIDARG, "SetNumChannelGroups(1001) (%#lx)", hr);
    hr = IDirectMusicSynth_SetNumChannelGroups(synth, 3);
    CHECKF(hr == S_OK, "SetNumChannelGroups(3) (%#lx)", hr);

    /* channel priorities */
    for (i = 0; i < 16; i++)
    {
        prio = 0xdeadbeef;
        hr = IDirectMusicSynth_GetChannelPriority(synth, 1, i, &prio);
        CHECKF(hr == S_OK && prio == prio_default[i], "default priority of channel %lu is %#lx (%#lx)", i, prio, hr);
    }
    hr = IDirectMusicSynth_SetChannelPriority(synth, 1, 3, 1234);
    CHECKF(hr == S_OK, "SetChannelPriority (%#lx)", hr);
    prio = 0;
    hr = IDirectMusicSynth_GetChannelPriority(synth, 1, 3, &prio);
    CHECKF(hr == S_OK && prio == 1234, "priority read back is %lu (%#lx)", prio, hr);
    prio = 0;
    hr = IDirectMusicSynth_GetChannelPriority(synth, 2, 3, &prio);
    CHECKF(hr == S_OK && prio == prio_default[3], "group 2 keeps its default (%#lx)", prio);
    hr = IDirectMusicSynth_SetChannelPriority(synth, 0, 0, 1);
    CHECKF(hr == E_INVALIDARG, "group 0 (%#lx)", hr);
    hr = IDirectMusicSynth_SetChannelPriority(synth, 1001, 0, 1);
    CHECKF(hr == E_INVALIDARG, "group 1001 (%#lx)", hr);
    hr = IDirectMusicSynth_SetChannelPriority(synth, 1, 16, 1);
    CHECKF(hr == E_INVALIDARG, "channel 16 (%#lx)", hr);
    hr = IDirectMusicSynth_GetChannelPriority(synth, 1, 16, &prio);
    CHECKF(hr == E_INVALIDARG, "Get channel 16 (%#lx)", hr);
    hr = IDirectMusicSynth_GetChannelPriority(synth, 1, 0, NULL);
    CHECKF(hr == E_POINTER, "Get NULL (%#lx)", hr);

    /* running statistics */
    hr = IDirectMusicSynth_GetRunningStats(synth, NULL);
    CHECKF(hr == E_INVALIDARG, "GetRunningStats(NULL) (%#lx)", hr);
    memset(&stats, 0xcc, sizeof(stats));
    stats.dwSize = 0;
    hr = IDirectMusicSynth_GetRunningStats(synth, &stats);
    CHECKF(hr == E_INVALIDARG, "GetRunningStats with dwSize 0 (%#lx)", hr);
    memset(&stats, 0xcc, sizeof(stats));
    stats.dwSize = sizeof(stats);
    hr = IDirectMusicSynth_GetRunningStats(synth, &stats);
    CHECKF(hr == S_OK, "GetRunningStats (%#lx)", hr);
    CHECKF(stats.dwSize == sizeof(stats), "dwSize kept (%lu)", stats.dwSize);
    CHECKF((stats.dwValidStats & DMUS_SYNTHSTATS_VOICES) && stats.dwVoices == 0, "idle synth has no voices (valid %#lx, voices %lu)",
            stats.dwValidStats, stats.dwVoices);
    CHECKF(stats.dwLostNotes == 0 && stats.dwCPUPerVoice == 0, "no lost notes / per-voice load (%lu %lu)", stats.dwLostNotes, stats.dwCPUPerVoice);

    /* voices */
    hr = IDirectMusicSynth8_PlayVoice(synth8, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0);
    CHECKF(hr == S_OK, "PlayVoice (%#lx)", hr);
    hr = IDirectMusicSynth8_PlayVoice(synth8, 0, 1, 1, 16, 0, 0, 0, 0, 0, 0);
    CHECKF(hr == E_INVALIDARG, "PlayVoice channel 16 (%#lx)", hr);
    hr = IDirectMusicSynth8_StopVoice(synth8, 0, 1);
    CHECKF(hr == S_OK, "StopVoice (%#lx)", hr);
    states[0].bExists = states[1].bExists = TRUE;
    states[0].spPosition = states[1].spPosition = 77;
    hr = IDirectMusicSynth8_GetVoiceState(synth8, voices, 2, states);
    CHECKF(hr == S_OK && !states[0].bExists && !states[1].bExists && !states[0].spPosition, "GetVoiceState reports no live voice (%#lx %d %d)",
            hr, states[0].bExists, states[1].bExists);
    hr = IDirectMusicSynth8_GetVoiceState(synth8, NULL, 2, states);
    CHECKF(hr == E_POINTER, "GetVoiceState NULL ids (%#lx)", hr);
    hr = IDirectMusicSynth8_Refresh(synth8, 5, 0);
    CHECKF(hr == S_OK, "Refresh (%#lx)", hr);
    hr = IDirectMusicSynth8_AssignChannelToBuses(synth8, 1, 0, buses, 2);
    CHECKF(hr == S_OK, "AssignChannelToBuses (%#lx)", hr);
    hr = IDirectMusicSynth8_AssignChannelToBuses(synth8, 1, 0, NULL, 2);
    CHECKF(hr == E_POINTER, "AssignChannelToBuses NULL buses (%#lx)", hr);
    hr = IDirectMusicSynth8_AssignChannelToBuses(synth8, 0, 0, buses, 2);
    CHECKF(hr == E_INVALIDARG, "AssignChannelToBuses group 0 (%#lx)", hr);

    /* downloads */
    init_wave(&wave, 1);
    wave.info.dwDLType = 99;
    wave.info.dwNumOffsetTableEntries = 0;
    hr = IDirectMusicSynth_Download(synth, &wh, &wave, &can_free);
    CHECKF(hr == DMUS_E_UNKNOWNDOWNLOAD, "unknown download type with an empty offset table (%#lx)", hr);
    wave.info.dwDLType = 0;
    hr = IDirectMusicSynth_Download(synth, &wh, &wave, &can_free);
    CHECKF(hr == DMUS_E_UNKNOWNDOWNLOAD, "download type 0 (%#lx)", hr);
    init_wave(&wave, 1);
    wave.info.dwNumOffsetTableEntries = 0;
    hr = IDirectMusicSynth_Download(synth, &wh, &wave, &can_free);
    CHECKF(hr == DMUS_E_BADOFFSETTABLE, "known type with an empty offset table (%#lx)", hr);

    /* wave not used by anything: the callback fires on Unload */
    init_wave(&wave, 1);
    can_free = TRUE;
    hr = IDirectMusicSynth_Download(synth, &wh, &wave, &can_free);
    CHECKF(hr == S_OK && wh && can_free == FALSE, "wave download keeps the buffer (%#lx, can_free %d)", hr, can_free);
    cb_called = 0;
    hr = IDirectMusicSynth_Unload(synth, wh, unload_cb, (HANDLE)0x1234);
    CHECKF(hr == S_OK && cb_called == 1 && cb_handle == wh && cb_data == (HANDLE)0x1234,
            "unload callback of an unused wave (%#lx, called %d, handle %p, data %p)", hr, cb_called, cb_handle, cb_data);

    /* wave used by an instrument: the callback waits for the instrument */
    init_wave(&wave2, 7);
    hr = IDirectMusicSynth_Download(synth, &wh2, &wave2, &can_free);
    CHECKF(hr == S_OK && wh2, "second wave (%#lx)", hr);
    init_instrument(&inst, 8, 7);
    can_free = FALSE;
    hr = IDirectMusicSynth_Download(synth, &ih, &inst, &can_free);
    CHECKF(hr == S_OK && ih && can_free == TRUE, "instrument download (%#lx)", hr);
    cb_called = 0;
    hr = IDirectMusicSynth_Unload(synth, wh2, unload_cb, (HANDLE)0x4321);
    CHECKF(hr == S_OK && cb_called == 0, "no callback while the instrument uses the wave (%#lx, called %d)", hr, cb_called);
    hr = IDirectMusicSynth_Unload(synth, ih, NULL, NULL);
    CHECKF(hr == S_OK && cb_called == 1 && cb_handle == wh2 && cb_data == (HANDLE)0x4321,
            "callback once the instrument is gone (%#lx, called %d)", hr, cb_called);
    hr = IDirectMusicSynth_Unload(synth, (HANDLE)0xdeadbeef, NULL, NULL);
    CHECKF(hr == E_FAIL, "unknown handle (%#lx)", hr);

    hr = IDirectMusicSynth_Close(synth);
    CHECKF(hr == S_OK, "Close (%#lx)", hr);

    IDirectMusicSynth8_Release(synth8);
    IDirectMusicSynth_Release(synth);
    CoUninitialize();
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
