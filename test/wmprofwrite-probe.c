/* wmprofwrite-probe: system profiles and streams with no buffer window can be written (patches/sg/9110).
 *
 * The version 8 system profiles (IWMProfileManager::LoadSystemProfile) described their WMA stream
 * without the WMA 2 format's own data, so the writer found no encoder and BeginWriting failed for
 * every one of them; and a stream made with CreateNewStream and never given a buffer window (-1)
 * became a preroll of 4294967 seconds in the file properties and in every time the reader gave.
 * Writes one second of PCM with each system profile and reads it back; then a profile whose
 * stream has no buffer window: its file's duration and preroll must be sane.
 *
 * Prints name=value lines; see test/wmprofwrite-gate.sh.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <wmsdkidl.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include <math.h>

DEFINE_GUID(p_WMMEDIATYPE_Audio, 0x73647561, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);
DEFINE_GUID(p_WMMEDIATYPE_Video, 0x73646976, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);
DEFINE_GUID(p_WMMEDIASUBTYPE_PCM, 0x00000001, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);
DEFINE_GUID(p_WMFORMAT_WaveFormatEx, 0x05589f81, 0xc356, 0x11ce, 0xbf, 0x01, 0x00, 0xaa, 0x00, 0x55, 0x59, 0x5a);

static HRESULT (WINAPI *pWMCreateProfileManager)(IWMProfileManager **);
static HRESULT (WINAPI *pWMCreateWriter)(IUnknown *, IWMWriter **);
static HRESULT (WINAPI *pWMCreateSyncReader)(IUnknown *, DWORD, IWMSyncReader **);

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

static const WAVEFORMATEX *config_wfx(IWMStreamConfig *config, BYTE *buffer, DWORD size)
{
    IWMMediaProps *props;
    WM_MEDIA_TYPE *mt = (WM_MEDIA_TYPE *)buffer;
    HRESULT hr;

    if (FAILED(IWMStreamConfig_QueryInterface(config, &IID_IWMMediaProps, (void **)&props))) return NULL;
    hr = IWMMediaProps_GetMediaType(props, mt, &size);
    IWMMediaProps_Release(props);
    if (FAILED(hr) || !IsEqualGUID(&mt->majortype, &p_WMMEDIATYPE_Audio)
            || !IsEqualGUID(&mt->formattype, &p_WMFORMAT_WaveFormatEx) || mt->cbFormat < 16)
        return NULL;
    /* a copy of the format of its own: the media type's may live in the object */
    {
        static BYTE copy[256];
        memcpy(copy, mt->pbFormat, min(mt->cbFormat, sizeof(copy)));
        return (const WAVEFORMATEX *)copy;
    }
}

/* one second of a tone in that format, written and read back; the number of PCM bytes back */
static UINT64 last_duration, last_preroll;
static IWMProfile *use_profile;

/* the file properties object's play duration less the preroll, and the preroll (ms) */
static void read_durations(const WCHAR *path)
{
    static const GUID fpo = {0x8cabdca1, 0xa947, 0x11cf, {0x8e, 0xe4, 0x00, 0xc0, 0x0c, 0x20, 0x53, 0x65}};
    HANDLE file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    BYTE data[8192];
    DWORD read = 0, i;

    last_duration = last_preroll = 0;
    if (file == INVALID_HANDLE_VALUE) return;
    ReadFile(file, data, sizeof(data), &read, NULL);
    CloseHandle(file);
    for (i = 30; i + 88 <= read; i++)
        if (!memcmp(data + i, &fpo, 16))
        {
            memcpy(&last_duration, data + i + 64, 8);
            memcpy(&last_preroll, data + i + 80, 8);
            last_duration -= min(last_duration, last_preroll * 10000);
            break;
        }
}

static DWORD encode(IWMProfileManager *manager, IWMStreamConfig *config, const WAVEFORMATEX *wfx, DWORD index)
{
    WAVEFORMATEX pcm = {WAVE_FORMAT_PCM, wfx->nChannels, wfx->nSamplesPerSec,
            wfx->nSamplesPerSec * wfx->nChannels * 2, wfx->nChannels * 2, 16, 0};
    DWORD chunk = pcm.nAvgBytesPerSec / 10, c, i, back = 0;
    WM_MEDIA_TYPE mt = {0};
    IWMInputMediaProps *input;
    IWMSyncReader *reader;
    IWMProfile *profile;
    IWMWriter *writer;
    WCHAR path[64];
    HRESULT hr;

    swprintf(path, 64, L"C:\\format%lu.wma", index);
    if (use_profile)
    {
        profile = use_profile;
        IWMProfile_AddRef(profile);
        hr = S_OK;
        goto have_profile;
    }
    if (FAILED(IWMProfileManager_CreateEmptyProfile(manager, WMT_VER_9_0, &profile))) return 0;
    IWMStreamConfig_SetStreamNumber(config, 1);
    IWMStreamConfig_SetStreamName(config, (WCHAR *)L"Audio");
    IWMStreamConfig_SetConnectionName(config, (WCHAR *)L"Audio");
    hr = IWMProfile_AddStream(profile, config);
have_profile:
    if (FAILED(hr) || FAILED(pWMCreateWriter(NULL, &writer))) { IWMProfile_Release(profile); return 0; }
    hr = IWMWriter_SetProfile(writer, profile);
    IWMProfile_Release(profile);
    if (SUCCEEDED(hr) && SUCCEEDED(hr = IWMWriter_GetInputProps(writer, 0, &input)))
    {
        mt.majortype = p_WMMEDIATYPE_Audio;
        mt.subtype = p_WMMEDIASUBTYPE_PCM;
        mt.bFixedSizeSamples = TRUE;
        mt.lSampleSize = pcm.nBlockAlign;
        mt.formattype = p_WMFORMAT_WaveFormatEx;
        mt.cbFormat = sizeof(pcm);
        mt.pbFormat = (BYTE *)&pcm;
        IWMInputMediaProps_SetMediaType(input, &mt);
        hr = IWMWriter_SetInputProps(writer, 0, input);
        IWMInputMediaProps_Release(input);
    }
    if (SUCCEEDED(hr)) hr = IWMWriter_SetOutputFilename(writer, path);
    if (SUCCEEDED(hr)) hr = IWMWriter_BeginWriting(writer);
    for (c = 0; c < 10 && SUCCEEDED(hr); c++)
    {
        INSSBuffer *sample;
        BYTE *data;
        DWORD len;
        if (FAILED(hr = IWMWriter_AllocateSample(writer, chunk, &sample))) break;
        INSSBuffer_GetBufferAndLength(sample, &data, &len);
        for (i = 0; i < chunk / 2; i++)
        {
            short v = (short)(6000 * sin((c * chunk / 2 + i) / pcm.nChannels * 2 * 3.14159265 * 440 / pcm.nSamplesPerSec));
            memcpy(data + i * 2, &v, 2);
        }
        INSSBuffer_SetLength(sample, chunk);
        hr = IWMWriter_WriteSample(writer, 0, (QWORD)c * 1000000, 0, sample);
        INSSBuffer_Release(sample);
    }
    if (SUCCEEDED(hr)) hr = IWMWriter_EndWriting(writer);
    IWMWriter_Release(writer);
    if (FAILED(hr))
    {
        printf("      format %lu: writing failed, hr %#lx\n", index, hr);
        return 0;
    }

    if (SUCCEEDED(pWMCreateSyncReader(NULL, 0, &reader)))
    {
        if (SUCCEEDED(IWMSyncReader_Open(reader, path)))
        {
            QWORD time, dur;
            DWORD flags, output;
            WORD stream;
            INSSBuffer *sample;
            while (IWMSyncReader_GetNextSample(reader, 0, &sample, &time, &dur, &flags, &output, &stream) == S_OK)
            {
                DWORD len = 0;
                INSSBuffer_GetLength(sample, &len);
                back += len;
                INSSBuffer_Release(sample);
            }
            IWMSyncReader_Close(reader);
        }
        IWMSyncReader_Release(reader);
    }
    read_durations(path);
    DeleteFileW(path);
    return back;
}

int main(void)
{
    IWMProfileManager2 *manager2;
    IWMProfileManager *manager;
    DWORD count = 0, i, ok = 0;
    BYTE buffer[1024];
    HMODULE wmvcore;

    CoInitialize(NULL);
    wmvcore = LoadLibraryA("wmvcore.dll");
    pWMCreateProfileManager = (void *)GetProcAddress(wmvcore, "WMCreateProfileManager");
    pWMCreateWriter = (void *)GetProcAddress(wmvcore, "WMCreateWriter");
    pWMCreateSyncReader = (void *)GetProcAddress(wmvcore, "WMCreateSyncReader");
    pWMCreateProfileManager(&manager);
    IWMProfileManager_QueryInterface(manager, &IID_IWMProfileManager2, (void **)&manager2);
    IWMProfileManager2_SetSystemProfileVersion(manager2, WMT_VER_8_0);
    IWMProfileManager2_Release(manager2);
    IWMProfileManager_GetSystemProfileCount(manager, &count);
    for (i = 0; i < count; i++)
    {
        IWMStreamConfig *config;
        const WAVEFORMATEX *wfx;
        DWORD back = 0;
        if (FAILED(IWMProfileManager_LoadSystemProfile(manager, i, &use_profile))) continue;
        if (SUCCEEDED(IWMProfile_GetStream(use_profile, 0, &config)))
        {
            if ((wfx = config_wfx(config, buffer, sizeof(buffer))))
                back = encode(manager, config, wfx, i);
            printf("      system profile %lu: %lu Hz, %u ch: %lu PCM bytes back, duration %I64u, preroll %I64u ms\n", i,
                    wfx ? wfx->nSamplesPerSec : 0, wfx ? wfx->nChannels : 0, back, last_duration, last_preroll);
            if (wfx && back >= wfx->nSamplesPerSec * wfx->nChannels * 2 * 8 / 10) ok++;
            IWMStreamConfig_Release(config);
        }
        IWMProfile_Release(use_profile);
    }
    use_profile = NULL;
    say("system_profiles", count && ok == count, "%lu of %lu system profiles write and read back", ok, count);

    /* a stream with no buffer window */
    {
        BYTE wma[18 + 10] = {0};
        WAVEFORMATEX *wfx = (WAVEFORMATEX *)wma;
        IWMStreamConfig *config;
        IWMMediaProps *props;
        IWMProfile *profile;
        WM_MEDIA_TYPE mt = {0};
        DWORD samples = 2048, align = 743, window = 0, back;
        WORD options = 1;

        wfx->wFormatTag = 0x161;
        wfx->nChannels = 2;
        wfx->nSamplesPerSec = 44100;
        wfx->nAvgBytesPerSec = 16000;
        wfx->nBlockAlign = 743;
        wfx->wBitsPerSample = 16;
        wfx->cbSize = 10;
        memcpy(wma + 18, &samples, 4);
        memcpy(wma + 22, &options, 2);
        memcpy(wma + 24, &align, 4);
        IWMProfileManager_CreateEmptyProfile(manager, WMT_VER_9_0, &profile);
        IWMProfile_CreateNewStream(profile, &p_WMMEDIATYPE_Audio, &config);
        IWMStreamConfig_SetBitrate(config, 128000);
        IWMStreamConfig_GetBufferWindow(config, &window);
        mt.majortype = p_WMMEDIATYPE_Audio;
        mt.subtype = p_WMMEDIATYPE_Audio;
        mt.subtype.Data1 = 0x161;
        mt.bFixedSizeSamples = TRUE;
        mt.lSampleSize = 743;
        mt.formattype = p_WMFORMAT_WaveFormatEx;
        mt.cbFormat = sizeof(wma);
        mt.pbFormat = wma;
        IWMStreamConfig_QueryInterface(config, &IID_IWMMediaProps, (void **)&props);
        IWMMediaProps_SetMediaType(props, &mt);
        IWMMediaProps_Release(props);
        back = encode(manager, config, wfx, 99);
        say("no_window_written", back >= 44100 * 4 * 8 / 10, "a stream with no buffer window (%#lx): %lu PCM bytes back", window, back);
        say("no_window_preroll", last_preroll && last_preroll <= 60000, "its file's preroll %I64u ms", last_preroll);
        say("no_window_duration", last_duration > 8000000 && last_duration < 15000000,
                "its play duration less the preroll %I64u (100 ns)", last_duration);
        IWMStreamConfig_Release(config);
        IWMProfile_Release(profile);
    }
    IWMProfileManager_Release(manager);
    printf("done=1\n");
    return 0;
}
