/* test/avclock-gate.sh probe. "mock": a WAV file's graph, with the stand-in
 * dsound.dll beside the probe whose sound plays 1% off real time: the clock
 * the DirectSound renderer gives the graph follows what the sound has played
 * (so video follows the sound, as on Windows), not the system clock; it never
 * goes back and its advises fire on its own time. "real": the play cursor of
 * a real DirectSound buffer is what the device has played, never ahead of
 * real time (SKIP without a sound device). */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <dshow.h>
#include <dsound.h>
#include <stdio.h>
#include <stdlib.h>

DEFINE_GUID(CLSID_DSoundRender_, 0x79376820, 0x07d0, 0x11cf, 0xa2, 0x4d, 0x00, 0x20, 0xaf, 0xd7, 0x97, 0x67);

static int failures;
static void check(int ok, const char *what)
{
    printf("%s %s\n", ok ? "ok  " : "FAIL", what);
    if (!ok) failures++;
}

static LONGLONG now_us(void)
{
    LARGE_INTEGER t, f;
    QueryPerformanceCounter(&t); QueryPerformanceFrequency(&f);
    return t.QuadPart * 1000000 / f.QuadPart;
}

static void write_wav(const WCHAR *name, DWORD seconds)
{
    DWORD rate = 22050, data = rate * 2 * seconds, n, i;
    BYTE hdr[44];
    HANDLE f = CreateFileW(name, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    short *pcm = calloc(rate, 2);
    memcpy(hdr, "RIFF", 4); *(DWORD *)(hdr + 4) = 36 + data; memcpy(hdr + 8, "WAVEfmt ", 8);
    *(DWORD *)(hdr + 16) = 16; *(WORD *)(hdr + 20) = 1; *(WORD *)(hdr + 22) = 1;
    *(DWORD *)(hdr + 24) = rate; *(DWORD *)(hdr + 28) = rate * 2; *(WORD *)(hdr + 32) = 2; *(WORD *)(hdr + 34) = 16;
    memcpy(hdr + 36, "data", 4); *(DWORD *)(hdr + 40) = data;
    WriteFile(f, hdr, 44, &n, NULL);
    for (i = 0; i < rate; i++) pcm[i] = (short)((i % 50) * 40 - 1000);
    for (i = 0; i < seconds; i++) WriteFile(f, pcm, rate * 2, &n, NULL);
    CloseHandle(f);
    free(pcm);
}

static int mock(int permille, int seconds)
{
    LONGLONG (WINAPI *played)(void);
    IGraphBuilder *graph;
    IMediaFilter *mf;
    IMediaControl *control;
    IMediaSeeking *seeking;
    IReferenceClock *clock = NULL;
    IBaseFilter *owner;
    OAFilterState state;
    REFERENCE_TIME c0, c1, s0, s1, t, last = 0, target;
    LONGLONG p0, p1, q0, q1, end;
    HANDLE event;
    DWORD_PTR cookie;
    CLSID clsid;
    char msg[200];
    HRESULT hr;
    int back = 0;
    double dc, dp, ds, dq;

    sprintf(msg, "%d", permille);
    SetEnvironmentVariableA("SGMOCKDS_PERMILLE", msg);
    write_wav(L"avclock.wav", seconds + 6);
    hr = CoCreateInstance(&CLSID_FilterGraph, NULL, CLSCTX_INPROC_SERVER, &IID_IGraphBuilder, (void **)&graph);
    check(hr == S_OK, "filter graph");
    if (hr != S_OK) return 1;
    hr = IGraphBuilder_RenderFile(graph, L"avclock.wav", NULL);
    sprintf(msg, "RenderFile(avclock.wav) %#lx", hr); check(hr == S_OK, msg);
    played = (void *)GetProcAddress(GetModuleHandleA("dsound.dll"), "sgmock_played_us");
    check(played != NULL, "the stand-in dsound.dll is the one loaded");
    if (hr != S_OK || !played) return 1;
    IGraphBuilder_QueryInterface(graph, &IID_IMediaFilter, (void **)&mf);
    IGraphBuilder_QueryInterface(graph, &IID_IMediaControl, (void **)&control);
    IGraphBuilder_QueryInterface(graph, &IID_IMediaSeeking, (void **)&seeking);
    hr = IMediaControl_Pause(control);
    IMediaControl_GetState(control, 5000, &state);
    IMediaFilter_GetSyncSource(mf, &clock);
    check(clock != NULL, "the graph has a clock");
    if (!clock) return 1;
    hr = IReferenceClock_QueryInterface(clock, &IID_IBaseFilter, (void **)&owner);
    check(hr == S_OK, "the graph's clock is a filter's");
    if (hr == S_OK)
    {
        IBaseFilter_GetClassID(owner, &clsid);
        check(IsEqualGUID(&clsid, &CLSID_DSoundRender_), "the graph's clock is the DirectSound renderer's");
        IBaseFilter_Release(owner);
    }

    IMediaControl_Run(control);
    hr = IMediaControl_GetState(control, 5000, &state);
    check(state == State_Running, "the graph runs");
    Sleep(500);
    IReferenceClock_GetTime(clock, &c0); p0 = played(); q0 = now_us();
    IMediaSeeking_GetCurrentPosition(seeking, &s0);
    end = q0 + seconds * 1000000LL;
    while (now_us() < end)
    {
        IReferenceClock_GetTime(clock, &t);
        if (t < last) back++;
        last = t;
        Sleep(5);
    }
    IReferenceClock_GetTime(clock, &c1); p1 = played(); q1 = now_us();
    IMediaSeeking_GetCurrentPosition(seeking, &s1);
    dc = (c1 - c0) / 10000.0; dp = (p1 - p0) / 1000.0; ds = (s1 - s0) / 10000.0; dq = (q1 - q0) / 1000.0;
    printf("info: over %.0f ms real: sound played %.1f ms, clock %.1f ms, stream position %.1f ms\n", dq, dp, dc, ds);
    sprintf(msg, "the stand-in plays at %d/1000 of real time (%.1f ms for %.1f)", permille, dp, dq * permille / 1000);
    check(dp - dq * permille / 1000 < 15 && dp - dq * permille / 1000 > -15, msg);
    sprintf(msg, "the renderer's clock follows the sound: %.1f ms off after %.0f ms", dc - dp, dp);
    check(dc - dp < 25 && dc - dp > -25, msg);
    sprintf(msg, "the stream position follows the sound: %.1f ms off", ds - dp);
    check(ds - dp < 40 && ds - dp > -40, msg);
    check(!back, "the clock never goes back");

    event = CreateEventW(NULL, FALSE, FALSE, NULL);
    IReferenceClock_GetTime(clock, &t);
    target = t + 300 * 10000;
    hr = IReferenceClock_AdviseTime(clock, target, 0, (HEVENT)event, &cookie);
    check(hr == S_OK, "AdviseTime");
    check(WaitForSingleObject(event, 3000) == WAIT_OBJECT_0, "the advise fires");
    IReferenceClock_GetTime(clock, &t);
    sprintf(msg, "it fires on the clock's time (%.1f ms from the target)", (t - target) / 10000.0);
    check(t - target > -15 * 10000 && t - target < 40 * 10000, msg);

    IMediaControl_Stop(control);
    IReferenceClock_GetTime(clock, &c0);
    Sleep(200);
    IReferenceClock_GetTime(clock, &c1);
    sprintf(msg, "stopped, it goes on with real time (%.1f ms in 200)", (c1 - c0) / 10000.0);
    check(c1 - c0 > 150 * 10000 && c1 - c0 < 300 * 10000, msg);
    IReferenceClock_Release(clock);
    IMediaSeeking_Release(seeking);
    IMediaControl_Release(control);
    IMediaFilter_Release(mf);
    IGraphBuilder_Release(graph);
    return 0;
}

static int real(void)
{
    WAVEFORMATEX wf = { WAVE_FORMAT_PCM, 2, 48000, 192000, 4, 16, 0 };
    DSBUFFERDESC desc = { sizeof(desc), DSBCAPS_GETCURRENTPOSITION2 | DSBCAPS_GLOBALFOCUS, 192000, 0, &wf };
    IDirectSound8 *ds;
    IDirectSoundBuffer *buf;
    DWORD play, write, last = 0;
    LONGLONG t0, el, total = 0, ahead, most = -1000000, lag = 0;
    DWORD adv;
    int back = 0;
    char msg[200];

    if (FAILED(DirectSoundCreate8(NULL, &ds, NULL)))
    {
        printf("SKIP: no sound device\n");
        return 77;
    }
    IDirectSound8_SetCooperativeLevel(ds, GetDesktopWindow(), DSSCL_PRIORITY);
    if (FAILED(IDirectSound8_CreateSoundBuffer(ds, &desc, &buf, NULL))) { check(0, "sound buffer"); return 1; }
    IDirectSoundBuffer_Play(buf, 0, 0, DSBPLAY_LOOPING);
    t0 = now_us();
    while ((el = now_us() - t0) < 2500000)
    {
        IDirectSoundBuffer_GetCurrentPosition(buf, &play, &write);
        adv = (play + 192000 - last) % 192000;
        if (adv > 96000) back++; /* went back */
        else total += adv;
        last = play;
        ahead = total * 1000000 / 192000 - el;
        if (ahead > most) most = ahead;
        lag = -ahead;
        Sleep(5);
    }
    IDirectSoundBuffer_Stop(buf);
    printf("info: play cursor at most %.1f ms ahead of real time; %.1f ms behind it at the end\n", most / 1000.0, lag / 1000.0);
    sprintf(msg, "the play cursor is what has played: never ahead of real time (%.1f ms)", most / 1000.0);
    check(most < 12000, msg);
    check(!back, "the play cursor never goes back");
    sprintf(msg, "and it moves (%.1f ms behind)", lag / 1000.0);
    check(lag < 500000, msg);
    IDirectSoundBuffer_Release(buf);
    IDirectSound8_Release(ds);
    return 0;
}

int main(int argc, char **argv)
{
    int rc;
    setvbuf(stdout, NULL, _IONBF, 0);
    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (argc > 1 && !strcmp(argv[1], "real")) rc = real();
    else rc = mock(argc > 2 ? atoi(argv[2]) : 1010, argc > 3 ? atoi(argv[3]) : 8);
    if (rc == 77) { printf("RESULT: SKIP\n"); return 77; }
    printf("RESULT: %s\n", !rc && !failures ? "PASS" : "FAIL");
    return 0;
}
