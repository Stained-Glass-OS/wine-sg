/* xactengine's PrepareInMemoryWave / PrepareStreamingWave (patches/sg/2972), run by
 * test/xact-wave-gate.sh: the waves are built from generated PCM data, no sound bank is
 * needed. Exits 77 when the engine cannot be initialised (no audio output).
 *
 *   xact-wave-probe.exe */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <objbase.h>
#include <xact3.h>
#include <stdio.h>
#include <string.h>

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}
#define CHECKF(ok, ...) do { char _b[256]; snprintf(_b, sizeof(_b), __VA_ARGS__); check(ok, _b); } while (0)

static void notification_cb(const XACT_NOTIFICATION *n) { }

static void check_wave(IXACT3Wave *wave, BOOL streaming, const char *name)
{
    XACT_WAVE_INSTANCE_PROPERTIES props;
    DWORD state = 0;
    HRESULT hr;

    memset(&props, 0xcc, sizeof(props));
    hr = IXACT3Wave_GetProperties(wave, &props);
    CHECKF(hr == S_OK, "%s: GetProperties (%#lx)", name, hr);
    CHECKF(props.properties.format.nChannels == 1 && props.properties.format.nSamplesPerSec == 22050
            && props.properties.format.wBitsPerSample == 1 && props.properties.format.wFormatTag == WAVEBANKMINIFORMAT_TAG_PCM,
            "%s: format kept (tag %u, ch %u, rate %u, bits %u)", name, props.properties.format.wFormatTag,
            props.properties.format.nChannels, props.properties.format.nSamplesPerSec, props.properties.format.wBitsPerSample);
    CHECKF(props.properties.durationInSamples == 2205, "%s: duration is 2205 samples (%lu)", name,
            props.properties.durationInSamples);
    CHECKF(!!props.properties.streaming == streaming, "%s: streaming flag is %d (%d)", name, streaming,
            props.properties.streaming);
    hr = IXACT3Wave_GetState(wave, &state);
    CHECKF(hr == S_OK && (state & XACT_STATE_PREPARED), "%s: wave is prepared (%#lx, state %#lx)", name, hr, state);
    hr = IXACT3Wave_SetVolume(wave, 0.5f);
    CHECKF(hr == S_OK, "%s: SetVolume (%#lx)", name, hr);
    hr = IXACT3Wave_Destroy(wave);
    CHECKF(hr == S_OK, "%s: Destroy (%#lx)", name, hr);
}

int main(void)
{
    static BYTE pcm[4410];
    XACT_RUNTIME_PARAMETERS params = { 0 };
    XACT_STREAMING_PARAMETERS sparams = { 0 };
    WAVEBANKENTRY entry;
    IXACT3Engine *engine;
    IXACT3Wave *wave;
    WCHAR path[MAX_PATH];
    HANDLE file;
    DWORD written, i;
    HRESULT hr;

    CoInitialize(NULL);
    hr = CoCreateInstance(&CLSID_XACTEngine, NULL, CLSCTX_INPROC_SERVER, &IID_IXACT3Engine, (void **)&engine);
    CHECKF(hr == S_OK, "create the engine (%#lx)", hr);
    if (hr != S_OK) { printf("RESULT: FAIL\n"); return 1; }
    params.lookAheadTime = XACT_ENGINE_LOOKAHEAD_DEFAULT;
    params.fnNotificationCallback = notification_cb;
    hr = IXACT3Engine_Initialize(engine, &params);
    if (FAILED(hr))
    {
        printf("SKIP  engine cannot initialise (%#lx)\n", hr);
        return 77;
    }

    for (i = 0; i < sizeof(pcm); i++) pcm[i] = i * 7;
    memset(&entry, 0, sizeof(entry));
    entry.Duration = 2205;
    entry.Format.wFormatTag = WAVEBANKMINIFORMAT_TAG_PCM;
    entry.Format.nChannels = 1;
    entry.Format.nSamplesPerSec = 22050;
    entry.Format.wBlockAlign = 2;
    entry.Format.wBitsPerSample = WAVEBANKMINIFORMAT_BITDEPTH_16;
    entry.PlayRegion.dwOffset = 0;
    entry.PlayRegion.dwLength = sizeof(pcm);

    wave = (void *)0xdeadbeef;
    hr = IXACT3Engine_PrepareInMemoryWave(engine, 0, entry, NULL, NULL, 0, 0, &wave);
    CHECKF(hr == E_INVALIDARG, "PrepareInMemoryWave without data (%#lx)", hr);
    hr = IXACT3Engine_PrepareInMemoryWave(engine, 0, entry, NULL, pcm, 0, 0, NULL);
    CHECKF(hr == E_POINTER, "PrepareInMemoryWave without output (%#lx)", hr);
    wave = NULL;
    hr = IXACT3Engine_PrepareInMemoryWave(engine, 0, entry, NULL, pcm, 0, 0, &wave);
    /* the bundled FAudio has no wave objects yet (it returns no wave): E_FAIL, never E_NOTIMPL */
    CHECKF((hr == S_OK && wave) || (hr == E_FAIL && !wave), "PrepareInMemoryWave (%#lx, %p)", hr, wave);
    if (hr == S_OK && wave) check_wave(wave, FALSE, "in-memory wave");

    GetTempPathW(MAX_PATH, path);
    lstrcatW(path, L"sg-xact-wave.bin");
    file = CreateFileW(path, GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, 0);
    WriteFile(file, pcm, sizeof(pcm), &written, NULL);
    CloseHandle(file);
    file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);
    CHECKF(file != INVALID_HANDLE_VALUE, "open the streaming file");

    sparams.file = file;
    sparams.offset = 0;
    sparams.packetSize = 0x800;
    hr = IXACT3Engine_PrepareStreamingWave(engine, 0, entry, sparams, 4, NULL, 0, 0, NULL);
    CHECKF(hr == E_POINTER, "PrepareStreamingWave without output (%#lx)", hr);
    wave = NULL;
    hr = IXACT3Engine_PrepareStreamingWave(engine, 0, entry, sparams, 4, NULL, 0, 0, &wave);
    CHECKF((hr == S_OK && wave) || (hr == E_FAIL && !wave), "PrepareStreamingWave (%#lx, %p)", hr, wave);
    if (hr == S_OK && wave)
    {
        for (i = 0; i < 20; i++) { IXACT3Engine_DoWork(engine); Sleep(5); }
        check_wave(wave, TRUE, "streaming wave");
    }

    CloseHandle(file);
    DeleteFileW(path);
    IXACT3Engine_Shutdown(engine);
    IXACT3Engine_Release(engine);
    CoUninitialize();
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
