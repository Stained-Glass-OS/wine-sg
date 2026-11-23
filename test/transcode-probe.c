/* Probe for the transcode API (wine-sg 9405; test/transcode-gate.sh): MFCreateTranscodeTopology and
 * MFCreateTranscodeSinkActivate, as Microsoft's tutorials use them ("Tutorial: Encoding an MP4 File",
 * "Tutorial: Encoding a WMA File"): a source from the source resolver, a transcode profile with an audio
 * type from MFTranscodeGetAudioOutputAvailableTypes and a container type, the topology run by a Media
 * Session to its end, to AAC in MPEG-4, WMA in ASF and MP3, at the source's rate and at another (a resampler
 * in the topology). Zoom's media adapter imports MFCreateTranscodeTopology. The written files are read
 * back with a source reader and must hold the 1 kHz tone the WAV source had. */
#define COBJMACROS
#define _WIN32_WINNT 0x0a00
#define WINVER 0x0a00
#include <windows.h>
#include <initguid.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mferror.h>
#include <mfreadwrite.h>
#include <wmcontainer.h>
#include <math.h>
#include <stdio.h>

DEFINE_GUID(IID_IMFTranscodeSinkInfoProvider_, 0x8cffcd2e, 0x5a03, 0x4a3a, 0xaf, 0xf7, 0xed, 0xcd, 0x10, 0x7c, 0x62, 0x0e);
typedef struct { DWORD dwVideoStreamID; IMFMediaType *pVideoMediaType; DWORD dwAudioStreamID; IMFMediaType *pAudioMediaType; } SINK_INFO;
typedef struct sink_info_provider sink_info_provider;
typedef struct
{
    HRESULT (WINAPI *QueryInterface)(sink_info_provider *, REFIID, void **);
    ULONG (WINAPI *AddRef)(sink_info_provider *);
    ULONG (WINAPI *Release)(sink_info_provider *);
    HRESULT (WINAPI *SetOutputFile)(sink_info_provider *, const WCHAR *);
    HRESULT (WINAPI *SetOutputByteStream)(sink_info_provider *, IMFActivate *);
    HRESULT (WINAPI *SetProfile)(sink_info_provider *, IMFTranscodeProfile *);
    HRESULT (WINAPI *GetSinkInfo)(sink_info_provider *, SINK_INFO *);
} sink_info_provider_vtbl;
struct sink_info_provider { const sink_info_provider_vtbl *lpVtbl; };

/* IMFTranscodeProfile: not in mingw-w64's mfidl.h */
typedef struct
{
    HRESULT (WINAPI *QueryInterface)(IMFTranscodeProfile *, REFIID, void **);
    ULONG (WINAPI *AddRef)(IMFTranscodeProfile *);
    ULONG (WINAPI *Release)(IMFTranscodeProfile *);
    HRESULT (WINAPI *SetAudioAttributes)(IMFTranscodeProfile *, IMFAttributes *);
    HRESULT (WINAPI *GetAudioAttributes)(IMFTranscodeProfile *, IMFAttributes **);
    HRESULT (WINAPI *SetVideoAttributes)(IMFTranscodeProfile *, IMFAttributes *);
    HRESULT (WINAPI *GetVideoAttributes)(IMFTranscodeProfile *, IMFAttributes **);
    HRESULT (WINAPI *SetContainerAttributes)(IMFTranscodeProfile *, IMFAttributes *);
    HRESULT (WINAPI *GetContainerAttributes)(IMFTranscodeProfile *, IMFAttributes **);
} profile_vtbl;
#define PVT(p) (*(const profile_vtbl **)(p))
#define IMFTranscodeProfile_SetAudioAttributes(p, a) PVT(p)->SetAudioAttributes(p, a)
#define IMFTranscodeProfile_SetVideoAttributes(p, a) PVT(p)->SetVideoAttributes(p, a)
#define IMFTranscodeProfile_SetContainerAttributes(p, a) PVT(p)->SetContainerAttributes(p, a)
#define IMFTranscodeProfile_Release(p) PVT(p)->Release(p)

static int failures;
#define CHECK(c, ...) do { if (c) printf("ok    " __VA_ARGS__); else { failures++; printf("FAIL  " __VA_ARGS__); } } while (0)

#define RATE 44100
#define SECONDS 2
#define TONE 1000.0

static void write_wav(const WCHAR *path)
{
    DWORD n = RATE * SECONDS * 2, size = n * 2, i, written;
    short *pcm = malloc(size);
    BYTE header[44];
    HANDLE h;

    for (i = 0; i < n; i += 2)
        pcm[i] = pcm[i + 1] = (short)(12000 * sin(2 * 3.14159265358979 * TONE * (i / 2) / RATE));
    memcpy(header, "RIFF", 4); *(DWORD *)(header + 4) = 36 + size; memcpy(header + 8, "WAVEfmt ", 8);
    *(DWORD *)(header + 16) = 16; *(WORD *)(header + 20) = 1; *(WORD *)(header + 22) = 2;
    *(DWORD *)(header + 24) = RATE; *(DWORD *)(header + 28) = RATE * 4; *(WORD *)(header + 32) = 4; *(WORD *)(header + 34) = 16;
    memcpy(header + 36, "data", 4); *(DWORD *)(header + 40) = size;
    h = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    WriteFile(h, header, 44, &written, NULL);
    WriteFile(h, pcm, size, &written, NULL);
    CloseHandle(h);
    free(pcm);
}

static double tone_of(const short *pcm, UINT32 frames, UINT32 channels, UINT32 rate)
{
    UINT32 i, crossings = 0, start = frames / 4, end = frames * 3 / 4;
    double energy = 0;
    if (end <= start + 2) return 0;
    for (i = start + 1; i < end; i++)
    {
        if ((pcm[(i - 1) * channels] < 0) != (pcm[i * channels] < 0)) crossings++;
        energy += (double)pcm[i * channels] * pcm[i * channels];
    }
    if (energy / (end - start) < 1000.0 * 1000.0) return 0;
    return crossings * (double)rate / 2.0 / (end - start);
}

static IMFMediaSource *open_source(const WCHAR *path)
{
    IMFSourceResolver *resolver;
    MF_OBJECT_TYPE type;
    IUnknown *object = NULL;
    IMFMediaSource *source = NULL;
    HRESULT hr;

    MFCreateSourceResolver(&resolver);
    hr = IMFSourceResolver_CreateObjectFromURL(resolver, path, MF_RESOLUTION_MEDIASOURCE, NULL, &type, &object);
    IMFSourceResolver_Release(resolver);
    if (FAILED(hr)) { printf("      source resolver %#lx\n", hr); return NULL; }
    IUnknown_QueryInterface(object, &IID_IMFMediaSource, (void **)&source);
    IUnknown_Release(object);
    return source;
}

static IMFAttributes *audio_type_for(const GUID *subtype, UINT32 want_rate)
{
    IMFCollection *types = NULL;
    IMFAttributes *found = NULL;
    DWORD count = 0, i;

    if (FAILED(MFTranscodeGetAudioOutputAvailableTypes(subtype, MFT_ENUM_FLAG_ALL, NULL, &types))) return NULL;
    IMFCollection_GetElementCount(types, &count);
    for (i = 0; i < count && !found; i++)
    {
        IUnknown *u; IMFAttributes *a; UINT32 rate = 0, ch = 0;
        IMFCollection_GetElement(types, i, &u);
        IUnknown_QueryInterface(u, &IID_IMFAttributes, (void **)&a);
        IUnknown_Release(u);
        IMFAttributes_GetUINT32(a, &MF_MT_AUDIO_SAMPLES_PER_SECOND, &rate);
        IMFAttributes_GetUINT32(a, &MF_MT_AUDIO_NUM_CHANNELS, &ch);
        if (rate == want_rate && ch == 2) found = a; else IMFAttributes_Release(a);
    }
    IMFCollection_Release(types);
    return found;
}

static IMFTranscodeProfile *make_profile(const GUID *audio_subtype, const GUID *container, UINT32 rate)
{
    IMFTranscodeProfile *profile;
    IMFAttributes *audio = audio_type_for(audio_subtype, rate), *c;

    if (!audio) return NULL;
    MFCreateTranscodeProfile(&profile);
    IMFTranscodeProfile_SetAudioAttributes(profile, audio);
    IMFAttributes_Release(audio);
    if (container)
    {
        MFCreateAttributes(&c, 1);
        IMFAttributes_SetGUID(c, &MF_TRANSCODE_CONTAINERTYPE, container);
        IMFTranscodeProfile_SetContainerAttributes(profile, c);
        IMFAttributes_Release(c);
    }
    return profile;
}

/* runs a session to its end: MESessionEnded, then Close and MESessionClosed */
static HRESULT run_session(IMFTopology *topology)
{
    IMFMediaSession *session;
    PROPVARIANT start;
    DWORD deadline = GetTickCount() + 60000;
    BOOL ended = FALSE, closed = FALSE;
    HRESULT hr, status = S_OK;

    if (FAILED(hr = MFCreateMediaSession(NULL, &session))) return hr;
    hr = IMFMediaSession_SetTopology(session, 0, topology);
    PropVariantInit(&start);
    if (SUCCEEDED(hr)) hr = IMFMediaSession_Start(session, &GUID_NULL, &start);
    while (SUCCEEDED(hr) && !closed && GetTickCount() < deadline)
    {
        IMFMediaEvent *event;
        MediaEventType type;
        HRESULT ev_hr;

        if (FAILED(IMFMediaSession_GetEvent(session, MF_EVENT_FLAG_NO_WAIT, &event)))
        {
            Sleep(10);
            continue;
        }
        IMFMediaEvent_GetType(event, &type);
        IMFMediaEvent_GetStatus(event, &ev_hr);
        IMFMediaEvent_Release(event);
        if (FAILED(ev_hr) && SUCCEEDED(status)) { status = ev_hr; printf("      session event %lu failed %#lx\n", type, ev_hr); }
        if (type == MESessionEnded || (FAILED(ev_hr) && !ended))
        {
            ended = TRUE;
            IMFMediaSession_Close(session);
        }
        if (type == MESessionClosed) closed = TRUE;
    }
    if (SUCCEEDED(hr) && !closed) hr = HRESULT_FROM_WIN32(ERROR_TIMEOUT);
    IMFMediaSession_Shutdown(session);
    IMFMediaSession_Release(session);
    return FAILED(hr) ? hr : status;
}

static void check_output(const WCHAR *path, const GUID *subtype, const char *what, UINT32 rate)
{
    IMFSourceReader *reader;
    IMFMediaType *type;
    DWORD flags, total = 0;
    short *decoded = NULL;
    HRESULT hr;

    hr = MFCreateSourceReaderFromURL(path, NULL, &reader);
    CHECK(hr == S_OK, "%s: the file opens in a source reader %#lx\n", what, hr);
    if (FAILED(hr)) return;
    /* the stream's format, from the file itself: Wine's source reader hands out decoded audio as the native
     * type. MPEG-4: an "mp4a" sample entry with its "esds"; ASF: the stream's type in the header, read with
     * mf.dll's ContentInfo */
    {
        HANDLE h = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
        DWORD size = GetFileSize(h, NULL), got = 0, i;
        BYTE *data = malloc(size + 1);
        BOOL match = FALSE;

        ReadFile(h, data, size, &got, NULL);
        CloseHandle(h);
        if (IsEqualGUID(subtype, &MFAudioFormat_MP3))
            match = got > 4 && (!memcmp(data, "ID3", 3) || (data[0] == 0xff && (data[1] & 0xe0) == 0xe0));
        else if (IsEqualGUID(subtype, &MFAudioFormat_AAC))
        {
            BOOL mp4a = FALSE, esds = FALSE;
            for (i = 0; i + 4 <= got; i++)
            {
                if (!memcmp(data + i, "mp4a", 4)) mp4a = TRUE;
                if (!memcmp(data + i, "esds", 4)) esds = TRUE;
            }
            match = mp4a && esds;
        }
        else
        {
            IMFASFContentInfo *info;
            IMFASFProfile *profile;
            IMFASFStreamConfig *sc;
            IMFMediaBuffer *b;
            GUID sub = {0};
            BYTE *p;
            WORD n;

            MFCreateMemoryBuffer(got, &b);
            IMFMediaBuffer_Lock(b, &p, NULL, NULL); memcpy(p, data, got); IMFMediaBuffer_Unlock(b);
            IMFMediaBuffer_SetCurrentLength(b, got);
            MFCreateASFContentInfo(&info);
            if (SUCCEEDED(IMFASFContentInfo_ParseHeader(info, b, 0)) && SUCCEEDED(IMFASFContentInfo_GetProfile(info, &profile)))
            {
                if (SUCCEEDED(IMFASFProfile_GetStream(profile, 0, &n, &sc)))
                {
                    if (SUCCEEDED(IMFASFStreamConfig_GetMediaType(sc, &type)))
                    {
                        IMFMediaType_GetGUID(type, &MF_MT_SUBTYPE, &sub);
                        IMFMediaType_Release(type);
                    }
                    IMFASFStreamConfig_Release(sc);
                }
                IMFASFProfile_Release(profile);
            }
            IMFASFContentInfo_Release(info);
            IMFMediaBuffer_Release(b);
            match = IsEqualGUID(&sub, subtype);
        }
        free(data);
        CHECK(match, "%s: its audio is the profile's subtype\n", what);
    }
    MFCreateMediaType(&type);
    IMFMediaType_SetGUID(type, &MF_MT_MAJOR_TYPE, &MFMediaType_Audio);
    IMFMediaType_SetGUID(type, &MF_MT_SUBTYPE, &MFAudioFormat_PCM);
    IMFMediaType_SetUINT32(type, &MF_MT_AUDIO_BITS_PER_SAMPLE, 16);
    IMFSourceReader_SetCurrentMediaType(reader, MF_SOURCE_READER_FIRST_AUDIO_STREAM, NULL, type);
    IMFMediaType_Release(type);
    for (;;)
    {
        IMFSample *s = NULL; LONGLONG ts; IMFMediaBuffer *b; BYTE *p; DWORD len;
        hr = IMFSourceReader_ReadSample(reader, MF_SOURCE_READER_FIRST_AUDIO_STREAM, 0, NULL, &flags, &ts, &s);
        if (FAILED(hr) || (flags & MF_SOURCE_READERF_ENDOFSTREAM) || !s) { if (s) IMFSample_Release(s); break; }
        IMFSample_ConvertToContiguousBuffer(s, &b);
        IMFMediaBuffer_Lock(b, &p, NULL, &len);
        decoded = realloc(decoded, total + len);
        memcpy((BYTE *)decoded + total, p, len);
        total += len;
        IMFMediaBuffer_Unlock(b);
        IMFMediaBuffer_Release(b);
        IMFSample_Release(s);
    }
    IMFSourceReader_Release(reader);
    CHECK(total > rate * 4 * SECONDS * 9 / 10, "%s: %lu PCM bytes (~%u)\n", what, total, rate * 4 * SECONDS);
    {
        double tone = decoded ? tone_of(decoded, total / 4, 2, rate) : 0;
        CHECK(fabs(tone - TONE) < TONE * 0.03, "%s: the %.0f Hz tone\n", what, tone);
    }
    free(decoded);
}

static void test_transcode(const WCHAR *wav, const GUID *subtype, const GUID *container, UINT32 rate, const WCHAR *name, const char *what)
{
    IMFTranscodeProfile *profile = make_profile(subtype, container, rate);
    IMFMediaSource *source;
    IMFTopology *topology = NULL;
    WCHAR path[MAX_PATH];
    WORD nodes = 0;
    HRESULT hr;

    printf("-- %s\n", what);
    CHECK(profile != NULL, "%s: an audio type from MFTranscodeGetAudioOutputAvailableTypes\n", what);
    if (!profile) return;
    GetTempPathW(MAX_PATH, path);
#ifdef KEEP_FILE
    wcscpy(path, L"Z:\\var\\tmp\\gmedia\\");
#endif
    wcscat(path, name);
    DeleteFileW(path);
    source = open_source(wav);
    CHECK(source != NULL, "%s: the WAV source\n", what);
    if (!source) { IMFTranscodeProfile_Release(profile); return; }
    hr = MFCreateTranscodeTopology(source, path, profile, &topology);
    CHECK(hr == S_OK, "%s: MFCreateTranscodeTopology %#lx\n", what, hr);
    if (SUCCEEDED(hr))
    {
        IMFTopology_GetNodeCount(topology, &nodes);
        CHECK(nodes == 3, "%s: a partial topology of source, encoder and sink (%u nodes)\n", what, nodes);
        hr = run_session(topology);
        CHECK(hr == S_OK, "%s: the session runs to its end %#lx\n", what, hr);
        IMFTopology_Release(topology);
    }
    IMFMediaSource_Shutdown(source);
    IMFMediaSource_Release(source);
    IMFTranscodeProfile_Release(profile);
    if (SUCCEEDED(hr)) check_output(path, subtype, what, rate);
#ifndef KEEP_FILE
    DeleteFileW(path);
#endif
}

static void test_errors(const WCHAR *wav)
{
    IMFTranscodeProfile *profile;
    IMFMediaSource *source = open_source(wav);
    IMFTopology *topology = NULL;
    IMFAttributes *video, *c;
    IMFActivate *activate;
    sink_info_provider *info;
    SINK_INFO si;
    HRESULT hr;

    printf("-- errors, sink activation object\n");
    if (!source) { CHECK(0, "no source\n"); return; }
    profile = make_profile(&MFAudioFormat_AAC, NULL, RATE);
    hr = profile ? MFCreateTranscodeTopology(source, L"x.mp4", profile, &topology) : E_FAIL;
    CHECK(hr == MF_E_TRANSCODE_NO_CONTAINERTYPE, "no container type: MF_E_TRANSCODE_NO_CONTAINERTYPE (%#lx)\n", hr);
    if (profile) IMFTranscodeProfile_Release(profile);

    MFCreateTranscodeProfile(&profile);
    MFCreateAttributes(&video, 2);
    IMFAttributes_SetGUID(video, &MF_MT_SUBTYPE, &MFVideoFormat_H264);
    IMFTranscodeProfile_SetVideoAttributes(profile, video);
    IMFAttributes_Release(video);
    MFCreateAttributes(&c, 1);
    IMFAttributes_SetGUID(c, &MF_TRANSCODE_CONTAINERTYPE, &MFTranscodeContainerType_MPEG4);
    IMFTranscodeProfile_SetContainerAttributes(profile, c);
    IMFAttributes_Release(c);
    hr = MFCreateTranscodeTopology(source, L"x.mp4", profile, &topology);
    CHECK(hr == MF_E_TRANSCODE_PROFILE_NO_MATCHING_STREAMS, "a video profile for an audio source: MF_E_TRANSCODE_PROFILE_NO_MATCHING_STREAMS (%#lx)\n", hr);
    IMFTranscodeProfile_Release(profile);
    IMFMediaSource_Shutdown(source);
    IMFMediaSource_Release(source);

    hr = MFCreateTranscodeSinkActivate(&activate);
    CHECK(hr == S_OK, "MFCreateTranscodeSinkActivate %#lx\n", hr);
    if (FAILED(hr)) return;
    hr = IMFActivate_QueryInterface(activate, &IID_IMFTranscodeSinkInfoProvider_, (void **)&info);
    CHECK(hr == S_OK, "it is an IMFTranscodeSinkInfoProvider %#lx\n", hr);
    if (SUCCEEDED(hr))
    {
        profile = make_profile(&MFAudioFormat_AAC, &MFTranscodeContainerType_MPEG4, RATE);
        hr = profile ? info->lpVtbl->SetProfile(info, profile) : E_FAIL;
        CHECK(hr == S_OK, "SetProfile %#lx\n", hr);
        memset(&si, 0, sizeof(si));
        hr = info->lpVtbl->GetSinkInfo(info, &si);
        CHECK(hr == S_OK && si.pAudioMediaType && !si.pVideoMediaType && si.dwAudioStreamID,
              "GetSinkInfo: the profile's audio type, stream %lu\n", si.dwAudioStreamID);
        if (si.pAudioMediaType) IMFMediaType_Release(si.pAudioMediaType);
        info->lpVtbl->SetOutputFile(info, L"y.mp4");
        hr = info->lpVtbl->SetOutputByteStream(info, activate);
        CHECK(FAILED(hr), "an output file and a byte stream exclude each other (%#lx)\n", hr);
        if (profile) IMFTranscodeProfile_Release(profile);
        info->lpVtbl->Release(info);
    }
    IMFActivate_Release(activate);
}

int main(void)
{
    WCHAR wav[MAX_PATH];

    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    MFStartup(MF_VERSION, MFSTARTUP_FULL);
    GetTempPathW(MAX_PATH, wav);
    wcscat(wav, L"transcode-probe.wav");
    write_wav(wav);
#ifndef ONLY_WMA
    test_errors(wav);
    test_transcode(wav, &MFAudioFormat_AAC, &MFTranscodeContainerType_MPEG4, RATE, L"transcode-probe.mp4", "WAV -> AAC in MPEG-4");
    test_transcode(wav, &MFAudioFormat_AAC, &MFTranscodeContainerType_MPEG4, 48000, L"transcode-probe48.mp4",
            "WAV 44.1 kHz -> AAC 48 kHz (a resampler in the topology)");
#endif
    test_transcode(wav, &MFAudioFormat_WMAudioV8, &MFTranscodeContainerType_ASF, RATE, L"transcode-probe.wma", "WAV -> WMA in ASF");
    test_transcode(wav, &MFAudioFormat_WMAudioV8, &MFTranscodeContainerType_ASF, 22050, L"transcode-probe22.wma",
            "WAV 44.1 kHz -> WMA 22.05 kHz (a resampler in the topology)");
    test_transcode(wav, &MFAudioFormat_MP3, &MFTranscodeContainerType_MP3, RATE, L"transcode-probe.mp3", "WAV -> MP3");
    DeleteFileW(wav);
    MFShutdown();
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
