/* mixervol-probe: a program's session volume is its volume in the sound server's mixer
 * (patches/sg/9416).
 *
 * Windows' Volume Mixer shows each program's audio session: its slider is the session's volume
 * (ISimpleAudioVolume), a program that changes it moves the slider, and a slider moved there is
 * the program's volume, told to it (IAudioSessionEvents::OnSimpleVolumeChanged). Wine applied the
 * session volume to the samples and left the sound server's per-program volume at 100%: a mixer
 * showed nothing, and a volume set there was never the program's.
 *
 * Plays a tone in two streams of one session, then: SetMasterVolume(0.5) reaches both sound
 * server streams (-6 dB); the mixer (pactl) sets one stream to 30%: the session's volume becomes
 * that (0.3^3 = 0.027, the server's volume is cubic), the program is told, and the other stream
 * follows; the mixer mutes it: GetMute, told, the other follows. A fast fade (40 steps at once) ends
 * where the program left it, its own changes not taken for the mixer's (patches/sg/9421).
 * pactl runs through start /unix; MIXERVOL_OUT names a scratch file (a Unix path).
 *
 * Prints name=value lines; see test/mixervol-gate.sh.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#define COBJMACROS
#define CONST_VTABLE
#include <windows.h>
#include <initguid.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <audiopolicy.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

static char out_unix[512], out_dos[512];

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

static void unix_cmd(const char *cmd)
{
    char line[2048];
    STARTUPINFOA si = {sizeof(si)};
    PROCESS_INFORMATION pi;
    snprintf(line, sizeof(line), "C:\\windows\\system32\\start.exe /wait /unix /bin/sh -c \"%s\"", cmd);
    if (CreateProcessA(NULL, line, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi))
    {
        WaitForSingleObject(pi.hProcess, 15000);
        CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
    }
}

/* the sound server's streams of this program: index and raw volume (65536 = 100%), mute */
static int streams(unsigned int *index, unsigned int *volume, int *mute, int max)
{
    char cmd[1024], line[256];
    FILE *f;
    int n = 0;
    snprintf(cmd, sizeof(cmd), "pactl list sink-inputs | awk '/^Sink Input #/{i=substr($3,2)} /^[[:space:]]*Mute:/{m=($2==\\\"yes\\\")} "
            "/^[[:space:]]*Volume:/{v=$3} /application.name = .mixervol/{print i, v, m}' > %s.tmp 2>&1; echo END >> %s.tmp; mv %s.tmp %s", out_unix, out_unix, out_unix, out_unix);
    DeleteFileA(out_dos);
    unix_cmd(cmd);
    /* start /unix need not wait for the Unix program: wait for its last line */
    for (int tries = 0; tries < 100; tries++)
    {
        BOOL end = FALSE;
        n = 0;
        if ((f = fopen(out_dos, "r")))
        {
            while (fgets(line, sizeof(line), f))
            {
                if (!strncmp(line, "END", 3)) end = TRUE;
                else if (n < max && sscanf(line, "%u %u %d", &index[n], &volume[n], &mute[n]) == 3) n++;
            }
            fclose(f);
        }
        if (end) return n;
        Sleep(50);
    }
    return -1;
}

/* ---- session events ---- */
static volatile LONG events;
static volatile float event_level = -1;
static volatile BOOL event_mute;
static HRESULT WINAPI se_QueryInterface(IAudioSessionEvents *iface, REFIID iid, void **out) { *out = iface; return S_OK; }
static ULONG WINAPI se_AddRef(IAudioSessionEvents *iface) { return 2; }
static ULONG WINAPI se_Release(IAudioSessionEvents *iface) { return 1; }
static HRESULT WINAPI se_OnDisplayNameChanged(IAudioSessionEvents *iface, LPCWSTR n, LPCGUID c) { return S_OK; }
static HRESULT WINAPI se_OnIconPathChanged(IAudioSessionEvents *iface, LPCWSTR n, LPCGUID c) { return S_OK; }
static volatile LONG foreign_events;
static const GUID session_guid;
static HRESULT WINAPI se_OnSimpleVolumeChanged(IAudioSessionEvents *iface, float v, BOOL m, LPCGUID c)
{
    event_level = v; event_mute = m; InterlockedIncrement(&events);
    /* the probe's own changes carry its session GUID as their context */
    if (!c || !IsEqualGUID(c, &session_guid)) InterlockedIncrement(&foreign_events);
    return S_OK;
}
static HRESULT WINAPI se_OnChannelVolumeChanged(IAudioSessionEvents *iface, DWORD n, float *v, DWORD ch, LPCGUID c) { return S_OK; }
static HRESULT WINAPI se_OnGroupingParamChanged(IAudioSessionEvents *iface, LPCGUID g, LPCGUID c) { return S_OK; }
static HRESULT WINAPI se_OnStateChanged(IAudioSessionEvents *iface, AudioSessionState s) { return S_OK; }
static HRESULT WINAPI se_OnSessionDisconnected(IAudioSessionEvents *iface, AudioSessionDisconnectReason r) { return S_OK; }
static const IAudioSessionEventsVtbl se_vtbl = { se_QueryInterface, se_AddRef, se_Release, se_OnDisplayNameChanged,
    se_OnIconPathChanged, se_OnSimpleVolumeChanged, se_OnChannelVolumeChanged, se_OnGroupingParamChanged,
    se_OnStateChanged, se_OnSessionDisconnected };
static IAudioSessionEvents session_events = { &se_vtbl };

/* ---- a stream playing a quiet tone, in the probe's session ---- */
static const GUID session_guid = {0x5a1d0e57, 0x0b1c, 0x4e5d, {0x9a, 0x77, 0x6d, 0x69, 0x78, 0x65, 0x72, 0x76}};
struct player { IAudioClient *client; IAudioRenderClient *render; WAVEFORMATEX *fmt; UINT32 frames; double phase; HANDLE thread; volatile LONG stop; };

static void fill(struct player *p, UINT32 n)
{
    BYTE *data;
    UINT32 i, c;
    if (!n || FAILED(IAudioRenderClient_GetBuffer(p->render, n, &data))) return;
    for (i = 0; i < n; i++)
    {
        float v = (float)(0.2 * sin(p->phase));
        p->phase += 2 * 3.14159265 * 440 / p->fmt->nSamplesPerSec;
        for (c = 0; c < p->fmt->nChannels; c++)
        {
            if (p->fmt->wBitsPerSample == 32) ((float *)data)[i * p->fmt->nChannels + c] = v;
            else ((short *)data)[i * p->fmt->nChannels + c] = (short)(v * 32767);
        }
    }
    IAudioRenderClient_ReleaseBuffer(p->render, n, 0);
}

static DWORD WINAPI play_thread(void *arg)
{
    struct player *p = arg;
    while (!p->stop)
    {
        UINT32 pad = 0;
        if (FAILED(IAudioClient_GetCurrentPadding(p->client, &pad))) break;
        fill(p, p->frames - pad);
        Sleep(10);
    }
    return 0;
}

static HRESULT start_player(struct player *p, IMMDevice *dev)
{
    HRESULT hr;
    memset(p, 0, sizeof(*p));
    if (FAILED(hr = IMMDevice_Activate(dev, &IID_IAudioClient, CLSCTX_ALL, NULL, (void **)&p->client))) return hr;
    IAudioClient_GetMixFormat(p->client, &p->fmt);
    if (FAILED(hr = IAudioClient_Initialize(p->client, AUDCLNT_SHAREMODE_SHARED, 0, 2000000, 0, p->fmt, &session_guid))) return hr;
    IAudioClient_GetBufferSize(p->client, &p->frames);
    IAudioClient_GetService(p->client, &IID_IAudioRenderClient, (void **)&p->render);
    fill(p, p->frames);
    hr = IAudioClient_Start(p->client);
    p->thread = CreateThread(NULL, 0, play_thread, p, 0, NULL);
    return hr;
}

static int wait_events(LONG count, DWORD ms)
{
    DWORD t0 = GetTickCount();
    while (events < count && GetTickCount() - t0 < ms) Sleep(20);
    return events >= count;
}

int main(void)
{
    IMMDeviceEnumerator *mmde;
    IMMDevice *dev;
    struct player a, b;
    ISimpleAudioVolume *sav;
    IAudioSessionControl *ctl;
    unsigned int index[8], volume[8];
    int mute[8], n, i, ok;
    char cmd[256];
    float level = -1;
    BOOL muted = FALSE;
    LONG before;
    HRESULT hr;

    if (!GetEnvironmentVariableA("MIXERVOL_OUT", out_unix, sizeof(out_unix))) strcpy(out_unix, "/tmp/mixervol.out");
    snprintf(out_dos, sizeof(out_dos), "Z:%s", out_unix);
    for (char *p = out_dos; *p; p++) if (*p == '/') *p = '\\';

    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    CoCreateInstance(&CLSID_MMDeviceEnumerator, NULL, CLSCTX_INPROC_SERVER, &IID_IMMDeviceEnumerator, (void **)&mmde);
    IMMDeviceEnumerator_GetDefaultAudioEndpoint(mmde, eRender, eConsole, &dev);
    hr = start_player(&a, dev);
    if (SUCCEEDED(hr)) hr = start_player(&b, dev);
    say("play", hr == S_OK, "two streams of one session playing (%#lx)", hr);
    if (FAILED(hr)) goto done;
    IAudioClient_GetService(a.client, &IID_IAudioSessionControl, (void **)&ctl);
    IAudioSessionControl_RegisterAudioSessionNotification(ctl, &session_events);
    IAudioClient_GetService(a.client, &IID_ISimpleAudioVolume, (void **)&sav);
    Sleep(500);

    n = streams(index, volume, mute, 8);
    say("listed", n == 2, "%d sound server stream(s) of the program", n);

    /* the program sets its volume: the mixer shows it */
    ISimpleAudioVolume_SetMasterVolume(sav, 0.5f, NULL);
    Sleep(500);
    n = streams(index, volume, mute, 8);
    for (i = 0, ok = n == 2; i < n; i++) if (abs((int)volume[i] - 52016) > 700) ok = 0;   /* cbrt(0.5) * 65536 */
    say("app_to_mixer", ok, "SetMasterVolume(0.5): the streams at %u and %u (want 52016, -6 dB)", n > 0 ? volume[0] : 0, n > 1 ? volume[1] : 0);
    if (n != 2) goto done;

    /* the mixer sets the first stream to 30%: the session's volume, told, the other stream follows */
    before = events;
    snprintf(cmd, sizeof(cmd), "pactl set-sink-input-volume %u 30%%", index[0]);
    unix_cmd(cmd);
    wait_events(before + 1, 3000);
    Sleep(300);
    ISimpleAudioVolume_GetMasterVolume(sav, &level);
    say("mixer_to_app", fabsf(level - 0.027f) < 0.003f, "the mixer at 30%%: GetMasterVolume %.4f (want 0.027)", level);
    say("told", events > before && fabsf(event_level - 0.027f) < 0.003f, "OnSimpleVolumeChanged %ld time(s), last %.4f",
            events - before, event_level);
    n = streams(index, volume, mute, 8);
    ok = n == 2 && abs((int)volume[0] - (int)volume[1]) < 700 && abs((int)volume[1] - 19661) < 700;   /* 30% */
    say("follows", ok, "the session's other stream follows: %u and %u (want 19661)", n > 0 ? volume[0] : 0, n > 1 ? volume[1] : 0);

    /* the mixer mutes it */
    before = events;
    snprintf(cmd, sizeof(cmd), "pactl set-sink-input-mute %u 1", index[0]);
    unix_cmd(cmd);
    wait_events(before + 1, 3000);
    Sleep(300);
    ISimpleAudioVolume_GetMute(sav, &muted);
    n = streams(index, volume, mute, 8);
    say("mute", muted && events > before && event_mute && n == 2 && mute[0] && mute[1],
            "the mixer muted it: GetMute %d, told %ld (mute %d), streams muted %d %d", muted, events - before, event_mute,
            n > 0 ? mute[0] : -1, n > 1 ? mute[1] : -1);

    /* the program unmutes and sets 100%: the mixer shows it */
    ISimpleAudioVolume_SetMute(sav, FALSE, NULL);
    ISimpleAudioVolume_SetMasterVolume(sav, 1.0f, NULL);
    Sleep(500);
    n = streams(index, volume, mute, 8);
    say("restore", n == 2 && !mute[0] && !mute[1] && abs((int)volume[0] - 65536) < 700 && abs((int)volume[1] - 65536) < 700,
            "unmuted at 100%%: %u/%d %u/%d", n > 0 ? volume[0] : 0, n > 0 ? mute[0] : -1, n > 1 ? volume[1] : 0, n > 1 ? mute[1] : -1);

    /* a fade, as players and games do it: 40 steps from 100% to 20% at once; five times. The
     * server's answers to the older steps come back after the newer ones were sent: they are the
     * program's own, not the mixer's (wine-sg 9421) */
    {
        int k, round, good = 0;
        for (round = 0; round < 5; round++)
        {
            foreign_events = 0;
            for (k = 1; k <= 40; k++)
            {
                ISimpleAudioVolume_SetMasterVolume(sav, 1.0f - 0.02f * k, &session_guid);
                if (k % 8 == 0) Sleep(1);
            }
            Sleep(800);
            ISimpleAudioVolume_GetMasterVolume(sav, &level);
            n = streams(index, volume, mute, 8);
            if (fabsf(level - 0.2f) < 0.001f && !foreign_events && n == 2
                    && abs((int)volume[0] - 38320) < 700 && abs((int)volume[1] - 38320) < 700) good++;
            else printf("  fade %d: GetMasterVolume %.4f, streams %u %u, %ld event(s) not the program's\n", round, level,
                    n > 0 ? volume[0] : 0, n > 1 ? volume[1] : 0, foreign_events);
            ISimpleAudioVolume_SetMasterVolume(sav, 1.0f, &session_guid);
            Sleep(300);
        }
        say("fade", good == 5, "a fast fade ends where the program left it, 5 of 5 times (%d)", good);
    }
done:
    fflush(stdout);
    TerminateProcess(GetCurrentProcess(), 0);
    return 0;
}
