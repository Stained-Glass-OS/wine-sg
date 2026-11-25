/* wmfilesink-probe: the Windows Media file sink (patches/sg/9104).
 *
 * WMCreateWriterFileSink was a stub: fre:ac's WMA encoder (and the SDK's pattern of a writer with
 * a sink of the program's own choosing) died on the call. Creates a file sink, opens it on a file,
 * gives it to a writer with IWMWriterAdvanced::AddSink (no SetOutputFilename), writes two seconds
 * of PCM audio through a WMA profile and checks: the file is an ASF file whose header carries the
 * final counts (the play duration, the packet count, the file size: written by the sink at the
 * end), GetFileSize matches the file, a synchronous reader opens it and gives the two seconds
 * back; a second sink with auto-indexing off, Close before the end, and the IWMWriterFileSink2/3
 * methods.
 *
 * Prints name=value lines; see test/wmfilesink-gate.sh.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <wmsdkidl.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

DEFINE_GUID(p_WMMEDIATYPE_Audio, 0x73647561, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);
DEFINE_GUID(p_WMMEDIASUBTYPE_PCM, 0x00000001, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);
DEFINE_GUID(p_WMFORMAT_WaveFormatEx, 0x05589f81, 0xc356, 0x11ce, 0xbf, 0x01, 0x00, 0xaa, 0x00, 0x55, 0x59, 0x5a);
static const GUID header_guid = {0x75b22630, 0x668e, 0x11cf, {0xa6, 0xd9, 0x00, 0xaa, 0x00, 0x62, 0xce, 0x6c}};
static const GUID fpo_guid = {0x8cabdca1, 0xa947, 0x11cf, {0x8e, 0xe4, 0x00, 0xc0, 0x0c, 0x20, 0x53, 0x65}};

/* IWMWriterFileSink, 2, 3: the public interfaces (wmsdkidl.h of the Windows Media Format SDK) */
DEFINE_GUID(p_IID_IWMWriterFileSink,  0x96406be5, 0x2b2b, 0x11d3, 0xb3, 0x6b, 0x00, 0xc0, 0x4f, 0x61, 0x08, 0xff);
DEFINE_GUID(p_IID_IWMWriterFileSink2, 0x14282ba7, 0x4aef, 0x4205, 0x8c, 0xe5, 0xc2, 0x29, 0x03, 0x5a, 0x05, 0xbc);
DEFINE_GUID(p_IID_IWMWriterFileSink3, 0x3fea4feb, 0x2945, 0x47a7, 0xa1, 0xdd, 0xc5, 0x3a, 0x8f, 0xc4, 0xc4, 0x5c);
typedef struct fsink fsink;
typedef struct
{
    HRESULT (WINAPI *QueryInterface)(fsink *, REFIID, void **);
    ULONG (WINAPI *AddRef)(fsink *);
    ULONG (WINAPI *Release)(fsink *);
    HRESULT (WINAPI *OnHeader)(fsink *, INSSBuffer *);
    HRESULT (WINAPI *IsRealTime)(fsink *, BOOL *);
    HRESULT (WINAPI *AllocateDataUnit)(fsink *, DWORD, INSSBuffer **);
    HRESULT (WINAPI *OnDataUnit)(fsink *, INSSBuffer *);
    HRESULT (WINAPI *OnEndWriting)(fsink *);
    HRESULT (WINAPI *Open)(fsink *, const WCHAR *);
    HRESULT (WINAPI *Start)(fsink *, QWORD);
    HRESULT (WINAPI *Stop)(fsink *, QWORD);
    HRESULT (WINAPI *IsStopped)(fsink *, BOOL *);
    HRESULT (WINAPI *GetFileDuration)(fsink *, QWORD *);
    HRESULT (WINAPI *GetFileSize)(fsink *, QWORD *);
    HRESULT (WINAPI *Close)(fsink *);
    HRESULT (WINAPI *IsClosed)(fsink *, BOOL *);
    HRESULT (WINAPI *SetAutoIndexing)(fsink *, BOOL);
    HRESULT (WINAPI *GetAutoIndexing)(fsink *, BOOL *);
    HRESULT (WINAPI *SetControlStream)(fsink *, WORD, BOOL);
    HRESULT (WINAPI *GetMode)(fsink *, DWORD *);
    HRESULT (WINAPI *OnDataUnitEx)(fsink *, void *);
    HRESULT (WINAPI *SetUnbufferedIO)(fsink *, BOOL, BOOL);
    HRESULT (WINAPI *GetUnbufferedIO)(fsink *, BOOL *);
    HRESULT (WINAPI *CompleteOperations)(fsink *);
} fsink_vtbl;
struct fsink { const fsink_vtbl *lpVtbl; };

static HRESULT (WINAPI *pWMCreateWriterFileSink)(fsink **);
static HRESULT (WINAPI *pWMCreateProfileManager)(IWMProfileManager **);
static HRESULT (WINAPI *pWMCreateWriter)(IUnknown *, IWMWriter **);
static HRESULT (WINAPI *pWMCreateSyncReader)(IUnknown *, DWORD, IWMSyncReader **);

#define RATE 44100
#define CHUNK 1764              /* 10 ms of stereo 16-bit */
#define CHUNKS 200              /* two seconds */

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

/* a profile of one WMA 2 stream, 128 kbps stereo 44.1 kHz, as an encoder's format lists it */
static IWMProfile *wma_profile(IWMProfileManager *manager)
{
    BYTE wma[18 + 10] = {0};
    WAVEFORMATEX *wfx = (WAVEFORMATEX *)wma;
    IWMStreamConfig *config;
    IWMMediaProps *props;
    IWMProfile *profile;
    WM_MEDIA_TYPE mt = {0};
    DWORD samples = 2048, align = 743;
    WORD options = 1;
    HRESULT hr;

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
    if (FAILED(IWMProfileManager_CreateEmptyProfile(manager, WMT_VER_9_0, &profile))) return NULL;
    if (FAILED(IWMProfile_CreateNewStream(profile, &p_WMMEDIATYPE_Audio, &config))) return NULL;
    IWMStreamConfig_SetStreamNumber(config, 1);
    IWMStreamConfig_SetStreamName(config, (WCHAR *)L"Audio");
    IWMStreamConfig_SetConnectionName(config, (WCHAR *)L"Audio");
    IWMStreamConfig_SetBitrate(config, 128000);
    IWMStreamConfig_SetBufferWindow(config, 3000);
    mt.majortype = p_WMMEDIATYPE_Audio;
    mt.subtype = p_WMMEDIATYPE_Audio;
    mt.subtype.Data1 = 0x161;
    mt.bFixedSizeSamples = TRUE;
    mt.lSampleSize = 743;
    mt.formattype = p_WMFORMAT_WaveFormatEx;
    mt.cbFormat = sizeof(wma);
    mt.pbFormat = wma;
    IWMStreamConfig_QueryInterface(config, &IID_IWMMediaProps, (void **)&props);
    hr = IWMMediaProps_SetMediaType(props, &mt);
    IWMMediaProps_Release(props);
    if (SUCCEEDED(hr)) hr = IWMProfile_AddStream(profile, config);
    IWMStreamConfig_Release(config);
    if (FAILED(hr)) { IWMProfile_Release(profile); return NULL; }
    return profile;
}

static IWMWriter *make_writer(IWMProfileManager *manager)
{
    IWMInputMediaProps *input;
    IWMProfile *profile;
    IWMWriter *writer;
    WAVEFORMATEX wfx = {WAVE_FORMAT_PCM, 2, RATE, RATE * 4, 4, 16, 0};
    WM_MEDIA_TYPE mt = {0};
    HRESULT hr;

    profile = wma_profile(manager);
    if (!profile) return NULL;
    if (FAILED(pWMCreateWriter(NULL, &writer))) return NULL;
    hr = IWMWriter_SetProfile(writer, profile);
    IWMProfile_Release(profile);
    if (FAILED(hr)) return NULL;
    if (FAILED(IWMWriter_GetInputProps(writer, 0, &input))) return NULL;
    mt.majortype = p_WMMEDIATYPE_Audio;
    mt.subtype = p_WMMEDIASUBTYPE_PCM;
    mt.bFixedSizeSamples = TRUE;
    mt.lSampleSize = 4;
    mt.formattype = p_WMFORMAT_WaveFormatEx;
    mt.cbFormat = sizeof(wfx);
    mt.pbFormat = (BYTE *)&wfx;
    IWMInputMediaProps_SetMediaType(input, &mt);
    hr = IWMWriter_SetInputProps(writer, 0, input);
    IWMInputMediaProps_Release(input);
    return SUCCEEDED(hr) ? writer : NULL;
}

static HRESULT write_tone(IWMWriter *writer, int chunks, BOOL restart)
{
    static int phase, base;
    HRESULT hr = S_OK;
    int c, i;

    if (restart) base = 0;
    for (c = 0; c < chunks && SUCCEEDED(hr); c++)
    {
        INSSBuffer *sample;
        BYTE *data;
        DWORD len;
        if (FAILED(hr = IWMWriter_AllocateSample(writer, CHUNK, &sample))) break;
        INSSBuffer_GetBufferAndLength(sample, &data, &len);
        for (i = 0; i < CHUNK / 4; i++, phase++)
        {
            short v = (short)(8000 * sin(phase * 2 * 3.14159265 * 440 / RATE));
            memcpy(data + i * 4, &v, 2);
            memcpy(data + i * 4 + 2, &v, 2);
        }
        INSSBuffer_SetLength(sample, CHUNK);
        hr = IWMWriter_WriteSample(writer, 0, (QWORD)(base + c) * 100000, 0, sample);
        INSSBuffer_Release(sample);
    }
    base += c;
    return hr;
}

static BYTE *read_file(const WCHAR *path, DWORD *size)
{
    HANDLE file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    BYTE *data;
    DWORD read;
    if (file == INVALID_HANDLE_VALUE) return NULL;
    *size = GetFileSize(file, NULL);
    data = malloc(*size + 1);
    ReadFile(file, data, *size, &read, NULL);
    CloseHandle(file);
    return data;
}

/* the file properties object's file size, packet count and play duration (100 ns) */
static BOOL file_props(const BYTE *data, DWORD size, UINT64 *file_size, UINT64 *packets, UINT64 *duration, BOOL *index)
{
    UINT64 header_size, pos, obj;
    DWORD count, i;

    *index = FALSE;
    if (size < 30 || memcmp(data, &header_guid, 16)) return FALSE;
    memcpy(&header_size, data + 16, 8);
    memcpy(&count, data + 24, 4);
    *file_size = *packets = *duration = 0;
    for (i = 0, pos = 30; i < count && pos + 24 <= header_size; i++, pos += obj)
    {
        memcpy(&obj, data + pos + 16, 8);
        if (obj < 24) return FALSE;
        if (!memcmp(data + pos, &fpo_guid, 16))
        {
            memcpy(file_size, data + pos + 40, 8);
            memcpy(packets, data + pos + 56, 8);
            {
                UINT64 preroll;     /* the play duration counts the preroll (ms) too */
                memcpy(duration, data + pos + 64, 8);
                memcpy(&preroll, data + pos + 80, 8);
                *duration -= min(*duration, preroll * 10000);
            }
        }
    }
    /* anything after the data object: an index */
    if (header_size + 50 <= size)
    {
        UINT64 data_size;
        memcpy(&data_size, data + header_size + 16, 8);
        *index = header_size + data_size < size;
    }
    return TRUE;
}

static LONG WINAPI stub_handler(EXCEPTION_POINTERS *info)
{
    if (info->ExceptionRecord->ExceptionCode == 0x80000100)
    {
        say("stub", FALSE, "WMCreateWriterFileSink is an unimplemented stub");
        printf("done=1\n");
        fflush(stdout);
        ExitProcess(0);
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

int main(void)
{
    static const WCHAR path[] = L"C:\\sink.wma", path2[] = L"C:\\sink2.wma", path3[] = L"C:\\closed.wma";
    IWMProfileManager *manager;
    IWMWriterAdvanced *advanced;
    IWMSyncReader *reader;
    IWMWriter *writer;
    fsink *sink, *sink2, *sink3;
    void *unk;
    HMODULE wmvcore;
    UINT64 file_size, packets, duration;
    QWORD qsize = 0, qdur = 0;
    BOOL index, b;
    DWORD size, mode = 0;
    BYTE *data;
    HRESULT hr;

    CoInitialize(NULL);
    wmvcore = LoadLibraryA("wmvcore.dll");
    pWMCreateWriterFileSink = (void *)GetProcAddress(wmvcore, "WMCreateWriterFileSink");
    pWMCreateProfileManager = (void *)GetProcAddress(wmvcore, "WMCreateProfileManager");
    pWMCreateWriter = (void *)GetProcAddress(wmvcore, "WMCreateWriter");
    pWMCreateSyncReader = (void *)GetProcAddress(wmvcore, "WMCreateSyncReader");

    /* a stub export raises EXCEPTION_WINE_STUB (and the process dies): say so first */
    AddVectoredExceptionHandler(1, stub_handler);
    say("export", pWMCreateWriterFileSink != NULL, "WMCreateWriterFileSink %p", pWMCreateWriterFileSink);
    if (!pWMCreateWriterFileSink) { printf("done=1\n"); return 0; }

    hr = pWMCreateWriterFileSink(&sink);
    say("create", hr == S_OK && sink, "hr %#lx", hr);
    if (FAILED(hr)) return 1;
    hr = sink->lpVtbl->QueryInterface(sink, &IID_IWMWriterSink, &unk);
    if (SUCCEEDED(hr)) IUnknown_Release((IUnknown *)unk);
    {
        HRESULT hr2 = sink->lpVtbl->QueryInterface(sink, &p_IID_IWMWriterFileSink2, &unk);
        HRESULT hr3;
        if (SUCCEEDED(hr2)) IUnknown_Release((IUnknown *)unk);
        hr3 = sink->lpVtbl->QueryInterface(sink, &p_IID_IWMWriterFileSink3, &unk);
        if (SUCCEEDED(hr3)) IUnknown_Release((IUnknown *)unk);
        say("interfaces", hr == S_OK && hr2 == S_OK && hr3 == S_OK, "IWMWriterSink %#lx, FileSink2 %#lx, FileSink3 %#lx", hr, hr2, hr3);
    }
    b = FALSE;
    sink->lpVtbl->GetAutoIndexing(sink, &b);
    sink->lpVtbl->GetMode(sink, &mode);
    say("defaults", b && (mode & 1 /* WMT_FM_SINGLE_BUFFERS */), "auto-indexing %d, mode %#lx", b, mode);
    b = TRUE;
    sink->lpVtbl->IsRealTime(sink, &b);
    say("not_realtime", !b, "IsRealTime %d", b);

    hr = sink->lpVtbl->Open(sink, path);
    say("open", hr == S_OK, "hr %#lx", hr);

    pWMCreateProfileManager(&manager);
    writer = make_writer(manager);
    say("writer", writer != NULL, "%p", writer);
    if (!writer) { printf("done=1\n"); return 1; }
    IWMWriter_QueryInterface(writer, &IID_IWMWriterAdvanced, (void **)&advanced);
    hr = IWMWriterAdvanced_AddSink(advanced, (IWMWriterSink *)sink);
    say("add_sink", hr == S_OK, "hr %#lx", hr);
    hr = IWMWriter_BeginWriting(writer);
    say("begin", hr == S_OK, "hr %#lx", hr);
    hr = write_tone(writer, CHUNKS, TRUE);
    say("write", hr == S_OK, "hr %#lx", hr);
    hr = IWMWriter_EndWriting(writer);
    say("end", hr == S_OK, "hr %#lx", hr);
    sink->lpVtbl->GetFileSize(sink, &qsize);
    sink->lpVtbl->GetFileDuration(sink, &qdur);
    IWMWriterAdvanced_RemoveSink(advanced, (IWMWriterSink *)sink);
    IWMWriterAdvanced_Release(advanced);
    IWMWriter_Release(writer);

    data = read_file(path, &size);
    if (data && file_props(data, size, &file_size, &packets, &duration, &index))
    {
        say("asf", TRUE, "%lu bytes", size);
        say("final_size", file_size == size, "file properties file size %I64u, file %lu", file_size, size);
        say("final_packets", packets > 0,
                "%I64u packets", packets);
        say("final_duration", duration > 18000000 && duration < 30000000, "play duration less the preroll %I64u (100 ns)", duration);
        say("sink_size", qsize == size, "GetFileSize %I64u", qsize);
        say("sink_duration", qdur > 15000000 && qdur < 25000000, "GetFileDuration %I64u", qdur);
    }
    else say("asf", FALSE, "no ASF file (%p, %lu)", data, data ? size : 0);
    free(data);

    /* a reader gives the two seconds back */
    if (SUCCEEDED(hr = pWMCreateSyncReader(NULL, 0, &reader)) && SUCCEEDED(hr = IWMSyncReader_Open(reader, path)))
    {
        QWORD time = 0, dur = 0, last = 0;
        DWORD flags, output, bytes = 0, n = 0;
        WORD stream;
        INSSBuffer *sample;
        while (IWMSyncReader_GetNextSample(reader, 0, &sample, &time, &dur, &flags, &output, &stream) == S_OK)
        {
            DWORD len = 0;
            INSSBuffer_GetLength(sample, &len);
            bytes += len;
            last = time + dur;
            n++;
            INSSBuffer_Release(sample);
        }
        say("read_back", n > 0 && last > 17000000, "%lu samples, %lu bytes, to %I64u", n, bytes, last);
        IWMSyncReader_Close(reader);
        IWMSyncReader_Release(reader);
    }
    else say("read_back", FALSE, "hr %#lx", hr);

    /* auto-indexing off: the same, with nothing after the data object (an audio-only file has no
     * index anyway: the writer indexes video streams); and a sink closed before the end keeps
     * what it had and drops the rest */
    pWMCreateWriterFileSink(&sink2);
    pWMCreateWriterFileSink(&sink3);
    sink2->lpVtbl->SetAutoIndexing(sink2, FALSE);
    b = TRUE;
    sink2->lpVtbl->GetAutoIndexing(sink2, &b);
    say("auto_index_off", !b, "GetAutoIndexing %d", b);
    sink2->lpVtbl->Open(sink2, path2);
    sink3->lpVtbl->Open(sink3, path3);
    writer = make_writer(manager);
    IWMWriter_QueryInterface(writer, &IID_IWMWriterAdvanced, (void **)&advanced);
    IWMWriterAdvanced_AddSink(advanced, (IWMWriterSink *)sink2);
    IWMWriterAdvanced_AddSink(advanced, (IWMWriterSink *)sink3);
    IWMWriter_BeginWriting(writer);
    write_tone(writer, CHUNKS / 2, TRUE);
    hr = sink3->lpVtbl->Close(sink3);
    b = FALSE;
    sink3->lpVtbl->IsClosed(sink3, &b);
    say("close", hr == S_OK && b, "Close %#lx, IsClosed %d", hr, b);
    sink3->lpVtbl->GetFileSize(sink3, &qsize);
    write_tone(writer, CHUNKS / 2, FALSE);
    hr = IWMWriter_EndWriting(writer);
    {
        QWORD after = 0;
        DWORD s2 = 0, s3 = 0;
        BYTE *d2 = read_file(path2, &s2), *d3 = read_file(path3, &s3);
        sink3->lpVtbl->GetFileSize(sink3, &after);
        say("second_sink", hr == S_OK && d2 && file_props(d2, s2, &file_size, &packets, &duration, &index)
                && file_size == s2 && duration > 18000000, "end %#lx, %lu bytes, size field %I64u", hr, s2, file_size);
        say("closed_kept", d3 && s3 == qsize && after == qsize && s3 > 0 && s3 < s2,
                "closed file %lu bytes (at Close %I64u, after %I64u)", s3, qsize, after);
        free(d2);
        free(d3);
    }
    IWMWriterAdvanced_Release(advanced);
    IWMWriter_Release(writer);
    sink->lpVtbl->Release(sink);
    sink2->lpVtbl->Release(sink2);
    sink3->lpVtbl->Release(sink3);
    IWMProfileManager_Release(manager);

    printf("done=1\n");
    return 0;
}
