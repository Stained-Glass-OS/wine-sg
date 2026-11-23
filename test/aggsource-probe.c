/* MFCreateAggregateSource (wine-sg 8901; test/aggsource-gate.sh): two WAV file sources (8 kHz mono and
 * 16 kHz stereo, both calling their stream 0) joined into one source, read through a source reader as
 * Qt Multimedia reads a camera + microphone aggregate. */
#define COBJMACROS
#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mferror.h>
#include <mfreadwrite.h>
#include <stdio.h>

HRESULT WINAPI MFCreateAggregateSource(IMFCollection *collection, IMFMediaSource **source);
HRESULT WINAPI MFCreateCollection(IMFCollection **collection);

static int failures;
static void check(int ok, const char *what)
{
    printf("%s %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) failures++;
}

static void write_wav(const WCHAR *path, DWORD rate, WORD channels, DWORD ms)
{
    DWORD frames = rate * ms / 1000, data = frames * channels * 2, n, i;
    BYTE hdr[44];
    HANDLE f = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    short *pcm = calloc(frames * channels, 2);

    for (i = 0; i < frames * channels; i++) pcm[i] = (short)((i * 97) & 0x3fff);
    memcpy(hdr, "RIFF", 4); *(DWORD *)(hdr + 4) = 36 + data; memcpy(hdr + 8, "WAVEfmt ", 8);
    *(DWORD *)(hdr + 16) = 16; *(WORD *)(hdr + 20) = 1; *(WORD *)(hdr + 22) = channels;
    *(DWORD *)(hdr + 24) = rate; *(DWORD *)(hdr + 28) = rate * channels * 2; *(WORD *)(hdr + 32) = channels * 2;
    *(WORD *)(hdr + 34) = 16; memcpy(hdr + 36, "data", 4); *(DWORD *)(hdr + 40) = data;
    WriteFile(f, hdr, 44, &n, NULL); WriteFile(f, pcm, data, &n, NULL); CloseHandle(f); free(pcm);
}

static IMFMediaSource *open_source(const WCHAR *path)
{
    IMFSourceResolver *resolver;
    MF_OBJECT_TYPE type;
    IUnknown *obj = NULL;
    IMFMediaSource *source = NULL;
    HRESULT hr;

    MFCreateSourceResolver(&resolver);
    hr = IMFSourceResolver_CreateObjectFromURL(resolver, path, MF_RESOLUTION_MEDIASOURCE, NULL, &type, &obj);
    if (SUCCEEDED(hr)) IUnknown_QueryInterface(obj, &IID_IMFMediaSource, (void **)&source);
    if (obj) IUnknown_Release(obj);
    IMFSourceResolver_Release(resolver);
    return source;
}

int main(void)
{
    WCHAR dir[MAX_PATH], a[MAX_PATH], b[MAX_PATH];
    IMFMediaSource *s1, *s2, *agg = NULL;
    IMFCollection *empty, *coll;
    IMFPresentationDescriptor *pd;
    IMFStreamDescriptor *sd;
    IMFSourceReader *reader;
    IMFMediaType *type;
    DWORD count = 0, id[2] = {99, 99}, chars = 0, i;
    UINT32 rate[2] = {0, 0};
    int samples[2] = {0, 0}, eos[2] = {0, 0};
    BOOL selected;
    HRESULT hr;

    setvbuf(stdout, NULL, _IONBF, 0);
    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    MFStartup(MF_VERSION, MFSTARTUP_FULL);
    GetTempPathW(MAX_PATH, dir);
    swprintf(a, MAX_PATH, L"%lsaggsrc-a.wav", dir); swprintf(b, MAX_PATH, L"%lsaggsrc-b.wav", dir);
    write_wav(a, 8000, 1, 500); write_wav(b, 16000, 2, 500);
    s1 = open_source(a); s2 = open_source(b);
    if (!s1 || !s2) { printf("SKIP no WAV media source\n"); return 77; }

    MFCreateCollection(&empty);
    hr = MFCreateAggregateSource(empty, &agg);
    check(hr == E_INVALIDARG, "an empty collection: E_INVALIDARG (documented)");
    MFCreateCollection(&coll);
    IMFCollection_AddElement(coll, (IUnknown *)s1);
    IMFCollection_AddElement(coll, (IUnknown *)s2);
    hr = MFCreateAggregateSource(coll, &agg);
    check(hr == S_OK && agg, "two sources aggregate");
    if (!agg) goto done;

    check(SUCCEEDED(IMFMediaSource_GetCharacteristics(agg, &chars)), "GetCharacteristics");
    hr = IMFMediaSource_CreatePresentationDescriptor(agg, &pd);
    check(hr == S_OK, "CreatePresentationDescriptor");
    if (SUCCEEDED(hr))
    {
        IMFPresentationDescriptor_GetStreamDescriptorCount(pd, &count);
        check(count == 2, "the presentation descriptor lists both sources' streams");
        for (i = 0; i < count && i < 2; i++)
        {
            if (FAILED(IMFPresentationDescriptor_GetStreamDescriptorByIndex(pd, i, &selected, &sd))) continue;
            IMFStreamDescriptor_GetStreamIdentifier(sd, &id[i]);
            check(selected, "each stream is selected by default (as in its own source)");
            IMFStreamDescriptor_Release(sd);
        }
        printf("stream ids %lu %lu\n", id[0], id[1]);
        check(id[0] != id[1], "the two streams have different identifiers (both sources call theirs 0)");
        IMFPresentationDescriptor_Release(pd);
    }

    hr = MFCreateSourceReaderFromMediaSource(agg, NULL, &reader);
    check(hr == S_OK, "a source reader opens the aggregate");
    if (FAILED(hr)) goto done;
    for (i = 0; i < 2; i++)
    {
        if (SUCCEEDED(IMFSourceReader_GetCurrentMediaType(reader, i, &type)))
        {
            IMFMediaType_GetUINT32(type, &MF_MT_AUDIO_SAMPLES_PER_SECOND, &rate[i]);
            IMFMediaType_Release(type);
        }
    }
    printf("rates %u %u\n", rate[0], rate[1]);
    check(rate[0] == 8000 && rate[1] == 16000, "stream 0 is the first source's (8 kHz), stream 1 the second's (16 kHz)");
    for (i = 0; i < 400 && !(eos[0] && eos[1]); i++)
    {
        DWORD idx = (i & 1) && !eos[1] ? 1 : (eos[0] ? 1 : 0), actual, flags = 0;
        LONGLONG ts;
        IMFSample *sample = NULL;

        hr = IMFSourceReader_ReadSample(reader, idx, 0, &actual, &flags, &ts, &sample);
        if (FAILED(hr)) { printf("ReadSample(%lu) %#lx\n", idx, hr); break; }
        if (sample) { samples[actual]++; IMFSample_Release(sample); }
        if (flags & MF_SOURCE_READERF_ENDOFSTREAM) eos[actual] = 1;
    }
    printf("samples %d %d eos %d %d\n", samples[0], samples[1], eos[0], eos[1]);
    check(samples[0] > 0 && samples[1] > 0, "samples arrive from both sources' streams");
    check(eos[0] && eos[1], "both streams reach their end");
    IMFSourceReader_Release(reader);
    hr = IMFMediaSource_Shutdown(agg);
    check(hr == S_OK || hr == MF_E_SHUTDOWN, "Shutdown (the reader may have shut it down already)");
    check(IMFMediaSource_CreatePresentationDescriptor(agg, &pd) == MF_E_SHUTDOWN, "after Shutdown: MF_E_SHUTDOWN");
done:
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
