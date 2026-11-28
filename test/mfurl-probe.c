/* mfurl-probe: Media Foundation opens file URLs (patches/sg/9420).
 *
 * The file scheme handler cut "file://" off a URL and took the rest for a path: file:///C:/... became
 * "/C:/...", with %20 for each space -- no file. Media players and the media engine handed a file URL
 * (a .NET or WinRT Uri's, file:///C:/Users/.../My%20Video.mp4) failed to open it. A file URL is now
 * the path it names, as a URL's rules have it.
 *
 * The probe writes C:\sg media\tone.wav (0.5 s PCM) and opens it with the source resolver under each
 * spelling; then the media engine plays the file:/// one to its end.
 *
 * Prints name=value lines; see test/mfurl-gate.sh.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfmediaengine.h>
#include <stdio.h>

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

static BOOL write_wav(const char *path)
{
    static short pcm[8000];
    struct { char riff[4]; DWORD size; char wave[4]; char fmt[4]; DWORD fmtsize; WAVEFORMATEX wfx; } __attribute__((packed)) h;
    char data[4] = { 'd', 'a', 't', 'a' };
    DWORD size = sizeof(pcm), written, i;
    HANDLE f;

    for (i = 0; i < ARRAYSIZE(pcm); i++) pcm[i] = (short)((i % 36 < 18) ? 3000 : -3000);
    memcpy(h.riff, "RIFF", 4); memcpy(h.wave, "WAVE", 4); memcpy(h.fmt, "fmt ", 4);
    h.fmtsize = 16;
    h.wfx.wFormatTag = WAVE_FORMAT_PCM; h.wfx.nChannels = 1; h.wfx.nSamplesPerSec = 16000; h.wfx.wBitsPerSample = 16;
    h.wfx.nBlockAlign = 2; h.wfx.nAvgBytesPerSec = 32000;
    h.size = 4 + 8 + 16 + 8 + size;
    CreateDirectoryA("C:\\sg media", NULL);
    f = CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    if (f == INVALID_HANDLE_VALUE) return FALSE;
    WriteFile(f, &h, 4 + 4 + 4 + 4 + 4 + 16, &written, NULL);
    WriteFile(f, data, 4, &written, NULL);
    WriteFile(f, &size, 4, &written, NULL);
    WriteFile(f, pcm, size, &written, NULL);
    CloseHandle(f);
    return written == size;
}

static HRESULT resolve(const WCHAR *url)
{
    IMFSourceResolver *res;
    MF_OBJECT_TYPE type;
    IUnknown *obj;
    HRESULT hr;
    MFCreateSourceResolver(&res);
    hr = IMFSourceResolver_CreateObjectFromURL(res, url, MF_RESOLUTION_MEDIASOURCE, NULL, &type, &obj);
    if (SUCCEEDED(hr))
    {
        IMFMediaSource *src;
        if (SUCCEEDED(IUnknown_QueryInterface(obj, &IID_IMFMediaSource, (void **)&src)))
        {
            IMFMediaSource_Shutdown(src);
            IMFMediaSource_Release(src);
        }
        IUnknown_Release(obj);
    }
    IMFSourceResolver_Release(res);
    return hr;
}

struct notify { IMFMediaEngineNotify iface; volatile LONG ended, error; };
static HRESULT WINAPI n_QI(IMFMediaEngineNotify *iface, REFIID iid, void **out)
{ if (IsEqualGUID(iid, &IID_IUnknown) || IsEqualGUID(iid, &IID_IMFMediaEngineNotify)) { *out = iface; return S_OK; } *out = NULL; return E_NOINTERFACE; }
static ULONG WINAPI n_AddRef(IMFMediaEngineNotify *iface) { return 2; }
static ULONG WINAPI n_Release(IMFMediaEngineNotify *iface) { return 1; }
static HRESULT WINAPI n_EventNotify(IMFMediaEngineNotify *iface, DWORD event, DWORD_PTR p1, DWORD p2)
{
    struct notify *n = (struct notify *)iface;
    if (event == MF_MEDIA_ENGINE_EVENT_ENDED) n->ended = 1;
    if (event == MF_MEDIA_ENGINE_EVENT_ERROR) n->error = 1;
    return S_OK;
}
static const IMFMediaEngineNotifyVtbl n_vtbl = { n_QI, n_AddRef, n_Release, n_EventNotify };

int main(void)
{
    static const struct { const char *name; const WCHAR *url; } urls[] =
    {
        { "url_escaped", L"file:///C:/sg%20media/tone.wav" },
        { "url_spaces",  L"file:///C:/sg media/tone.wav" },
        { "url_backslash", L"file:///C:\\sg media\\tone.wav" },
        { "url_drive",   L"file://C:/sg media/tone.wav" },
        { "path",        L"C:\\sg media\\tone.wav" },
    };
    IMFMediaEngineClassFactory *factory;
    struct notify n = { { &n_vtbl } };
    IMFMediaEngine *engine;
    IMFAttributes *attr;
    unsigned int i;
    HRESULT hr;
    DWORD t0;
    BSTR url;

    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    MFStartup(MF_VERSION, 0);
    say("write", write_wav("C:\\sg media\\tone.wav"), "C:\\sg media\\tone.wav written");
    for (i = 0; i < ARRAYSIZE(urls); i++)
    {
        hr = resolve(urls[i].url);
        say(urls[i].name, hr == S_OK, "%ls: %#lx", urls[i].url, hr);
    }

    CoCreateInstance(&CLSID_MFMediaEngineClassFactory, NULL, CLSCTX_INPROC_SERVER, &IID_IMFMediaEngineClassFactory, (void **)&factory);
    MFCreateAttributes(&attr, 2);
    IMFAttributes_SetUnknown(attr, &MF_MEDIA_ENGINE_CALLBACK, (IUnknown *)&n.iface);
    IMFAttributes_SetUINT32(attr, &MF_MEDIA_ENGINE_VIDEO_OUTPUT_FORMAT, 87);
    hr = IMFMediaEngineClassFactory_CreateInstance(factory, 0, attr, &engine);
    url = SysAllocString(urls[0].url);
    if (SUCCEEDED(hr)) hr = IMFMediaEngine_SetSource(engine, url);
    if (SUCCEEDED(hr)) IMFMediaEngine_Play(engine);
    t0 = GetTickCount();
    while (SUCCEEDED(hr) && !n.ended && !n.error && GetTickCount() - t0 < 10000) Sleep(20);
    say("engine", n.ended && !n.error, "the media engine plays %ls to its end (ended %ld, error %ld, %#lx)", urls[0].url, n.ended, n.error, hr);
    fflush(stdout);
    TerminateProcess(GetCurrentProcess(), 0);
    return 0;
}
