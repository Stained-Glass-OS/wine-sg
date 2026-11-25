/* wmeditor2-probe: IWMMetadataEditor2::OpenEx (patches/sg/9103).
 *
 * The metadata editor had no IWMMetadataEditor2, so fre:ac's WMA tagger -- which opens every file
 * through OpenEx, for reading (GENERIC_READ, FILE_SHARE_READ) and for writing (GENERIC_READ |
 * GENERIC_WRITE, no sharing) -- read no tags and wrote none, without a word. Writes a WMA file with
 * a title through the writer, then: OpenEx for reading lists the attributes as fre:ac walks them
 * (GetAttributeIndices of stream 0 with no name, GetAttributeByIndexEx), a Flush there fails and
 * leaves the file as it was; OpenEx for writing with no sharing keeps others out while open, adds
 * WM/AlbumTitle and WM/TrackNumber, Flush, Close; a fresh OpenEx reads them back.
 *
 * Prints name=value lines; see test/wmeditor2-gate.sh.
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

DEFINE_GUID(p_WMMEDIATYPE_Audio, 0x73647561, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);
DEFINE_GUID(p_WMMEDIASUBTYPE_PCM, 0x00000001, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);
DEFINE_GUID(p_WMFORMAT_WaveFormatEx, 0x05589f81, 0xc356, 0x11ce, 0xbf, 0x01, 0x00, 0xaa, 0x00, 0x55, 0x59, 0x5a);
DEFINE_GUID(p_IID_IWMMetadataEditor2, 0x203cffe3, 0x2e18, 0x4fdf, 0xb5, 0x9d, 0x6e, 0x71, 0x53, 0x05, 0x34, 0xcf);

typedef struct editor2 editor2;
typedef struct
{
    HRESULT (WINAPI *QueryInterface)(editor2 *, REFIID, void **);
    ULONG (WINAPI *AddRef)(editor2 *);
    ULONG (WINAPI *Release)(editor2 *);
    HRESULT (WINAPI *Open)(editor2 *, const WCHAR *);
    HRESULT (WINAPI *Close)(editor2 *);
    HRESULT (WINAPI *Flush)(editor2 *);
    HRESULT (WINAPI *OpenEx)(editor2 *, const WCHAR *, DWORD, DWORD);
} editor2_vtbl;
struct editor2 { const editor2_vtbl *lpVtbl; };

static HRESULT (WINAPI *pWMCreateEditor)(IWMMetadataEditor **);
static HRESULT (WINAPI *pWMCreateProfileManager)(IWMProfileManager **);
static HRESULT (WINAPI *pWMCreateWriter)(IUnknown *, IWMWriter **);

static const WCHAR path[] = L"C:\\tags.wma";

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

static BOOL make_file(void)
{
    WAVEFORMATEX wfx = {WAVE_FORMAT_PCM, 2, 44100, 44100 * 4, 4, 16, 0};
    IWMProfileManager *manager;
    IWMInputMediaProps *input;
    IWMHeaderInfo *header;
    IWMProfile *profile;
    IWMWriter *writer;
    WM_MEDIA_TYPE mt = {0};
    HRESULT hr;
    int c;

    pWMCreateProfileManager(&manager);
    profile = wma_profile(manager);
    IWMProfileManager_Release(manager);
    if (!profile) return FALSE;
    pWMCreateWriter(NULL, &writer);
    IWMWriter_SetProfile(writer, profile);
    IWMProfile_Release(profile);
    IWMWriter_GetInputProps(writer, 0, &input);
    mt.majortype = p_WMMEDIATYPE_Audio;
    mt.subtype = p_WMMEDIASUBTYPE_PCM;
    mt.bFixedSizeSamples = TRUE;
    mt.lSampleSize = 4;
    mt.formattype = p_WMFORMAT_WaveFormatEx;
    mt.cbFormat = sizeof(wfx);
    mt.pbFormat = (BYTE *)&wfx;
    IWMInputMediaProps_SetMediaType(input, &mt);
    IWMWriter_SetInputProps(writer, 0, input);
    IWMInputMediaProps_Release(input);
    if (SUCCEEDED(IWMWriter_QueryInterface(writer, &IID_IWMHeaderInfo, (void **)&header)))
    {
        static const WCHAR title[] = L"Probe Title";
        IWMHeaderInfo_SetAttribute(header, 0, L"Title", WMT_TYPE_STRING, (const BYTE *)title, sizeof(title));
        IWMHeaderInfo_Release(header);
    }
    IWMWriter_SetOutputFilename(writer, path);
    hr = IWMWriter_BeginWriting(writer);
    for (c = 0; c < 20 && SUCCEEDED(hr); c++)
    {
        INSSBuffer *sample;
        BYTE *data;
        DWORD len;
        IWMWriter_AllocateSample(writer, 4410, &sample);
        INSSBuffer_GetBufferAndLength(sample, &data, &len);
        memset(data, 0, 4410);
        INSSBuffer_SetLength(sample, 4410);
        hr = IWMWriter_WriteSample(writer, 0, (QWORD)c * 250000, 0, sample);
        INSSBuffer_Release(sample);
    }
    if (SUCCEEDED(hr)) hr = IWMWriter_EndWriting(writer);
    IWMWriter_Release(writer);
    return SUCCEEDED(hr);
}

/* every attribute of stream 0 as fre:ac's tagger reads them; the value of NAME (or empty) */
static int walk(editor2 *editor, const WCHAR *want, WCHAR *value, DWORD value_size, DWORD *dword)
{
    IWMHeaderInfo3 *info;
    WORD lang = 0, count = 0, indices[64], i;
    int found = 0;

    *value = 0;
    if (FAILED(editor->lpVtbl->QueryInterface(editor, &IID_IWMHeaderInfo3, (void **)&info))) return -1;
    IWMHeaderInfo3_GetAttributeIndices(info, 0, NULL, &lang, NULL, &count);
    if (count > 64) count = 64;
    IWMHeaderInfo3_GetAttributeIndices(info, 0, NULL, &lang, indices, &count);
    for (i = 0; i < count; i++)
    {
        WCHAR name[256];
        WORD name_len = 256;
        DWORD length = 0;
        BYTE data[1024];
        WMT_ATTR_DATATYPE type;
        IWMHeaderInfo3_GetAttributeByIndexEx(info, 0, indices[i], name, &name_len, NULL, NULL, NULL, &length);
        if (length > sizeof(data)) continue;
        name_len = 256;
        if (FAILED(IWMHeaderInfo3_GetAttributeByIndexEx(info, 0, indices[i], name, &name_len, &type, NULL, data, &length)))
            continue;
        if (!wcscmp(name, want))
        {
            found = 1;
            if (type == WMT_TYPE_STRING) lstrcpynW(value, (WCHAR *)data, value_size);
            if (type == WMT_TYPE_DWORD && dword) memcpy(dword, data, 4);
        }
    }
    IWMHeaderInfo3_Release(info);
    return found ? count : 0;
}

static BYTE *read_file(DWORD *size)
{
    HANDLE file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    BYTE *data;
    DWORD read;
    if (file == INVALID_HANDLE_VALUE) return NULL;
    *size = GetFileSize(file, NULL);
    data = malloc(*size + 1);
    ReadFile(file, data, *size, &read, NULL);
    CloseHandle(file);
    return data;
}

int main(void)
{
    IWMMetadataEditor *editor;
    IWMHeaderInfo3 *info;
    editor2 *ed2;
    WCHAR value[256];
    DWORD size1, size2, track = 0;
    BYTE *before, *after;
    HMODULE wmvcore;
    HANDLE other;
    HRESULT hr;
    int n;

    CoInitialize(NULL);
    wmvcore = LoadLibraryA("wmvcore.dll");
    pWMCreateEditor = (void *)GetProcAddress(wmvcore, "WMCreateEditor");
    pWMCreateProfileManager = (void *)GetProcAddress(wmvcore, "WMCreateProfileManager");
    pWMCreateWriter = (void *)GetProcAddress(wmvcore, "WMCreateWriter");

    say("file", make_file(), "a WMA file with a title");
    pWMCreateEditor(&editor);
    hr = IWMMetadataEditor_QueryInterface(editor, &p_IID_IWMMetadataEditor2, (void **)&ed2);
    say("interface", hr == S_OK, "IWMMetadataEditor2 hr %#lx", hr);
    if (FAILED(hr)) { printf("done=1\n"); return 0; }

    /* for reading */
    hr = ed2->lpVtbl->OpenEx(ed2, path, GENERIC_READ, FILE_SHARE_READ);
    n = walk(ed2, L"Title", value, 256, NULL);
    say("read_open", hr == S_OK, "OpenEx(GENERIC_READ, FILE_SHARE_READ) hr %#lx", hr);
    say("read_tags", n > 0 && !wcscmp(value, L"Probe Title"), "%d attributes, Title \"%ls\"", n, value);
    other = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    say("read_shared", other != INVALID_HANDLE_VALUE, "another reader may open it (error %lu)",
            other == INVALID_HANDLE_VALUE ? GetLastError() : 0);
    if (other != INVALID_HANDLE_VALUE) CloseHandle(other);
    before = read_file(&size1);
    ed2->lpVtbl->QueryInterface(ed2, &IID_IWMHeaderInfo3, (void **)&info);
    {
        static const WCHAR album[] = L"Read Only Album";
        WORD index;
        IWMHeaderInfo3_AddAttribute(info, 0, L"WM/AlbumTitle", &index, WMT_TYPE_STRING, 0, (const BYTE *)album, sizeof(album));
    }
    IWMHeaderInfo3_Release(info);
    hr = ed2->lpVtbl->Flush(ed2);
    after = read_file(&size2);
    say("read_no_flush", FAILED(hr) && before && after && size1 == size2 && !memcmp(before, after, size1),
            "Flush on a file opened for reading: hr %#lx, file %s", hr,
            before && after && size1 == size2 && !memcmp(before, after, size1) ? "unchanged" : "CHANGED");
    free(before);
    free(after);
    hr = ed2->lpVtbl->Close(ed2);
    say("read_close", hr == S_OK, "Close hr %#lx", hr);

    /* for writing, no sharing */
    hr = ed2->lpVtbl->OpenEx(ed2, path, GENERIC_READ | GENERIC_WRITE, 0);
    say("write_open", hr == S_OK, "OpenEx(GENERIC_READ | GENERIC_WRITE, 0) hr %#lx", hr);
    other = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    say("write_exclusive", other == INVALID_HANDLE_VALUE && GetLastError() == ERROR_SHARING_VIOLATION,
            "another opener is kept out (error %lu)", other == INVALID_HANDLE_VALUE ? GetLastError() : 0);
    if (other != INVALID_HANDLE_VALUE) CloseHandle(other);
    ed2->lpVtbl->QueryInterface(ed2, &IID_IWMHeaderInfo3, (void **)&info);
    {
        static const WCHAR album[] = L"Probe Album";
        DWORD number = 7;
        WORD index;
        HRESULT hr2;
        hr = IWMHeaderInfo3_AddAttribute(info, 0, L"WM/AlbumTitle", &index, WMT_TYPE_STRING, 0, (const BYTE *)album, sizeof(album));
        hr2 = IWMHeaderInfo3_AddAttribute(info, 0, L"WM/TrackNumber", &index, WMT_TYPE_DWORD, 0, (const BYTE *)&number, 4);
        say("add", hr == S_OK && hr2 == S_OK, "AddAttribute %#lx %#lx", hr, hr2);
    }
    IWMHeaderInfo3_Release(info);
    hr = ed2->lpVtbl->Flush(ed2);
    say("write_flush", hr == S_OK, "Flush hr %#lx", hr);
    other = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    say("still_exclusive", other == INVALID_HANDLE_VALUE, "after Flush the file is still the editor's (error %lu)",
            other == INVALID_HANDLE_VALUE ? GetLastError() : 0);
    if (other != INVALID_HANDLE_VALUE) CloseHandle(other);
    hr = ed2->lpVtbl->Close(ed2);
    other = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    say("released", hr == S_OK && other != INVALID_HANDLE_VALUE, "Close hr %#lx, file free again", hr);
    if (other != INVALID_HANDLE_VALUE) CloseHandle(other);

    hr = ed2->lpVtbl->OpenEx(ed2, path, GENERIC_READ, FILE_SHARE_READ);
    walk(ed2, L"WM/AlbumTitle", value, 256, NULL);
    n = walk(ed2, L"WM/TrackNumber", value + 128, 128, &track);
    say("written", hr == S_OK && !wcscmp(value, L"Probe Album") && n > 0 && track == 7,
            "WM/AlbumTitle \"%ls\", WM/TrackNumber %lu", value, track);
    walk(ed2, L"Title", value, 256, NULL);
    say("title_kept", !wcscmp(value, L"Probe Title"), "Title \"%ls\"", value);
    ed2->lpVtbl->Close(ed2);

    hr = ed2->lpVtbl->OpenEx(ed2, L"C:\\missing.wma", GENERIC_READ, FILE_SHARE_READ);
    say("missing", FAILED(hr), "a missing file fails: hr %#lx", hr);

    ed2->lpVtbl->Release(ed2);
    IWMMetadataEditor_Release(editor);
    printf("done=1\n");
    return 0;
}
