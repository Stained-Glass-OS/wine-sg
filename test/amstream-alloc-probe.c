/* Probe for patches/sg/4580: amstream's DirectDraw and audio streams allocate
 * samples with a backing object of their own (IMediaStream::AllocateSample)
 * and make samples sharing another sample's backing object
 * (IMediaStream::CreateSharedSample). Table driven; lines "ok"/"FAIL", last
 * "RESULT: PASS|FAIL". */
#define COBJMACROS
#define INITGUID
#include <windows.h>
#include <stdio.h>
#include <mmreg.h>
#include <ddraw.h>
#include <mmstream.h>
#include <amstream.h>
#include <ddstream.h>
#include <austream.h>

static int failures;
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

static IDirectDrawSurface *surface_of(IStreamSample *sample, RECT *rect)
{
    IDirectDrawStreamSample *dds;
    IDirectDrawSurface *surface = NULL;

    if (FAILED(IStreamSample_QueryInterface(sample, &IID_IDirectDrawStreamSample, (void **)&dds))) return NULL;
    IDirectDrawStreamSample_GetSurface(dds, &surface, rect);
    IDirectDrawStreamSample_Release(dds);
    if (surface) IDirectDrawSurface_Release(surface);
    return surface;
}

static IAudioData *data_of(IStreamSample *sample)
{
    IAudioStreamSample *as;
    IAudioData *data = NULL;

    if (FAILED(IStreamSample_QueryInterface(sample, &IID_IAudioStreamSample, (void **)&as))) return NULL;
    IAudioStreamSample_GetAudioData(as, &data);
    IAudioStreamSample_Release(as);
    if (data) IAudioData_Release(data);
    return data;
}

int main(int argc, char **argv)
{
    IAMMultiMediaStream *mms;
    IMediaStream *video = NULL, *audio = NULL;
    IStreamSample *v1 = NULL, *v2 = NULL, *a1 = NULL, *a2 = NULL, *x;
    IAudioMediaStream *audio_stream;
    WAVEFORMATEX format = {WAVE_FORMAT_PCM, 2, 44100, 44100 * 4, 4, 16, 0};
    DWORD size, actual;
    BYTE *ptr;
    RECT rect;
    HRESULT hr;

    CoInitialize(NULL);
    hr = CoCreateInstance(&CLSID_AMMultiMediaStream, NULL, CLSCTX_INPROC_SERVER, &IID_IAMMultiMediaStream, (void **)&mms);
    check(hr == S_OK, "multimedia stream %#lx", hr);
    if (FAILED(hr)) { printf("RESULT: FAIL\n"); return 1; }
    IAMMultiMediaStream_Initialize(mms, STREAMTYPE_READ, 0, NULL);
    hr = IAMMultiMediaStream_AddMediaStream(mms, NULL, &MSPID_PrimaryVideo, 0, &video);
    check(hr == S_OK, "video stream %#lx", hr);
    hr = IAMMultiMediaStream_AddMediaStream(mms, NULL, &MSPID_PrimaryAudio, 0, &audio);
    check(hr == S_OK, "audio stream %#lx", hr);
    if (!video || !audio) { printf("RESULT: FAIL\n"); return 1; }

    /* argument checks */
    check(IMediaStream_AllocateSample(video, 0, NULL) == E_POINTER, "video AllocateSample(NULL)");
    check(IMediaStream_AllocateSample(audio, 0, NULL) == E_POINTER, "audio AllocateSample(NULL)");
    x = (void *)0xdead;
    check(IMediaStream_CreateSharedSample(video, NULL, 0, &x) == E_POINTER && !x, "video CreateSharedSample(NULL existing)");
    x = (void *)0xdead;
    check(IMediaStream_CreateSharedSample(audio, NULL, 0, &x) == E_POINTER && !x, "audio CreateSharedSample(NULL existing)");

    /* DirectDraw stream: a surface of its own, then one shared */
    hr = IMediaStream_AllocateSample(video, 0, &v1);
    check(hr == S_OK && v1 && surface_of(v1, &rect), "video AllocateSample: %#lx, a surface", hr);
    if (v1)
    {
        RECT rect2;
        hr = IMediaStream_CreateSharedSample(video, v1, 0, &v2);
        check(hr == S_OK && v2 && v2 != v1 && surface_of(v2, &rect2) == surface_of(v1, &rect) && EqualRect(&rect, &rect2),
              "video CreateSharedSample: %#lx, the same surface and rectangle", hr);
        if (v2) IStreamSample_Release(v2);
    }

    /* audio stream: an IAudioData of its own with one second of buffer in the stream's format */
    hr = IMediaStream_QueryInterface(audio, &IID_IAudioMediaStream, (void **)&audio_stream);
    hr = IAudioMediaStream_SetFormat(audio_stream, &format);
    check(hr == S_OK, "audio SetFormat %#lx", hr);
    hr = IMediaStream_AllocateSample(audio, 0, &a1);
    check(hr == S_OK && a1 && data_of(a1), "audio AllocateSample: %#lx", hr);
    if (a1 && data_of(a1))
    {
        WAVEFORMATEX got;
        size = actual = 0; ptr = NULL;
        hr = IAudioData_GetInfo(data_of(a1), &size, &ptr, &actual);
        check(hr == S_OK && size == 44100 * 4 && ptr, "its buffer: %#lx %lu %p", hr, size, ptr);
        IAudioData_GetFormat(data_of(a1), &got);
        check(got.nSamplesPerSec == 44100 && got.nChannels == 2, "its format: %lu Hz %u channels", got.nSamplesPerSec, got.nChannels);
        hr = IMediaStream_CreateSharedSample(audio, a1, 0, &a2);
        check(hr == S_OK && a2 && a2 != a1 && data_of(a2) == data_of(a1), "audio CreateSharedSample: %#lx, the same IAudioData", hr);
        if (a2) IStreamSample_Release(a2);
    }
    IAudioMediaStream_Release(audio_stream);

    /* a sample of the other kind cannot be shared */
    if (v1 && a1)
    {
        x = (void *)0xdead;
        hr = IMediaStream_CreateSharedSample(video, a1, 0, &x);
        check(hr == MS_E_INCOMPATIBLE && !x, "video stream, audio sample: %#lx", hr);
        x = (void *)0xdead;
        hr = IMediaStream_CreateSharedSample(audio, v1, 0, &x);
        check(hr == MS_E_INCOMPATIBLE && !x, "audio stream, video sample: %#lx", hr);
    }
    if (v1) IStreamSample_Release(v1);
    if (a1) IStreamSample_Release(a1);

    IMediaStream_Release(video);
    IMediaStream_Release(audio);
    IAMMultiMediaStream_Release(mms);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
