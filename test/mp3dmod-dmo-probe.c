/* Probe for patches/sg/4577: the MP3 decoder DMO's GetInputCurrentType,
 * GetOutputCurrentType, AllocateStreamingResources, FreeStreamingResources,
 * GetInputStatus and Lock. Table driven; lines "ok"/"FAIL", last
 * "RESULT: PASS|FAIL". */
#define COBJMACROS
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <mmreg.h>
#include <mediaobj.h>
#include <mediaerr.h>

static int failures;
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

static const GUID clsid_mp3dec = {0xbbeea841,0x0a63,0x4f52,{0xa7,0xab,0xa9,0xb3,0xa8,0x4e,0xd3,0x8a}};
static const GUID iid_mediaobject = {0xd8ad0f58,0x5494,0x4102,{0x97,0xc5,0xec,0x79,0x8e,0x59,0xbc,0xf4}};
static const GUID mt_audio = {0x73647561,0x0000,0x0010,{0x80,0x00,0x00,0xaa,0x00,0x38,0x9b,0x71}};
static const GUID st_mp3 = {0x00000055,0x0000,0x0010,{0x80,0x00,0x00,0xaa,0x00,0x38,0x9b,0x71}};
static const GUID st_pcm = {0x00000001,0x0000,0x0010,{0x80,0x00,0x00,0xaa,0x00,0x38,0x9b,0x71}};
static const GUID ft_wfx = {0x05589f81,0xc356,0x11ce,{0xbf,0x01,0x00,0xaa,0x00,0x55,0x59,0x5a}};

/* a media buffer over a static array */
struct buffer { IMediaBuffer iface; LONG ref; BYTE data[8192]; DWORD len; };
static HRESULT WINAPI buf_qi(IMediaBuffer *iface, REFIID iid, void **out) { *out = iface; IMediaBuffer_AddRef(iface); return S_OK; }
static ULONG WINAPI buf_addref(IMediaBuffer *iface) { return InterlockedIncrement(&((struct buffer *)iface)->ref); }
static ULONG WINAPI buf_release(IMediaBuffer *iface) { return InterlockedDecrement(&((struct buffer *)iface)->ref); }
static HRESULT WINAPI buf_setlength(IMediaBuffer *iface, DWORD len) { ((struct buffer *)iface)->len = len; return S_OK; }
static HRESULT WINAPI buf_getmax(IMediaBuffer *iface, DWORD *max) { *max = sizeof(((struct buffer *)iface)->data); return S_OK; }
static HRESULT WINAPI buf_get(IMediaBuffer *iface, BYTE **data, DWORD *len)
{
    struct buffer *b = (struct buffer *)iface;
    if (data) *data = b->data;
    if (len) *len = b->len;
    return S_OK;
}
static IMediaBufferVtbl buf_vtbl = {buf_qi, buf_addref, buf_release, buf_setlength, buf_getmax, buf_get};

static IMediaObject *dmo;
static struct buffer inbuf = {{&buf_vtbl}, 1};
static volatile LONG done;

static DWORD WINAPI locked_out(void *arg)
{
    DWORD flags;
    IMediaObject_GetInputStatus(dmo, 0, &flags);
    InterlockedExchange(&done, 1);
    return 0;
}

int main(int argc, char **argv)
{
    MPEGLAYER3WAVEFORMAT mp3 = {{0}};
    WAVEFORMATEX pcm = {0};
    DMO_MEDIA_TYPE in = {{0}}, out = {{0}}, got;
    DWORD flags;
    HANDLE t;
    HRESULT hr;
    unsigned int i;

    CoInitialize(NULL);
    hr = CoCreateInstance(&clsid_mp3dec, NULL, CLSCTX_INPROC_SERVER, &iid_mediaobject, (void **)&dmo);
    check(hr == S_OK, "create the MP3 decoder DMO: %#lx", hr);
    if (FAILED(hr)) { printf("RESULT: FAIL\n"); return 1; }

    mp3.wfx.wFormatTag = WAVE_FORMAT_MPEGLAYER3; mp3.wfx.nChannels = 2; mp3.wfx.nSamplesPerSec = 48000;
    in.majortype = mt_audio; in.subtype = st_mp3; in.formattype = ft_wfx; in.cbFormat = sizeof(mp3); in.pbFormat = (BYTE *)&mp3;
    pcm.wFormatTag = WAVE_FORMAT_PCM; pcm.nChannels = 2; pcm.nSamplesPerSec = 48000; pcm.wBitsPerSample = 16;
    pcm.nBlockAlign = 4; pcm.nAvgBytesPerSec = 48000 * 4;
    out.majortype = mt_audio; out.subtype = st_pcm; out.formattype = ft_wfx; out.cbFormat = sizeof(pcm); out.pbFormat = (BYTE *)&pcm;

    /* current types: stream index, then type set, then pointer */
    {
        static const struct { int output, set, index, null; HRESULT want; } cases[] =
        {
            {0, 0, 1, 1, DMO_E_INVALIDSTREAMINDEX}, {0, 0, 0, 1, DMO_E_TYPE_NOT_SET}, {0, 0, 0, 0, DMO_E_TYPE_NOT_SET},
            {0, 1, 1, 0, DMO_E_INVALIDSTREAMINDEX}, {0, 1, 0, 1, E_POINTER}, {0, 1, 0, 0, S_OK},
            {1, 0, 1, 1, DMO_E_INVALIDSTREAMINDEX}, {1, 0, 0, 0, DMO_E_TYPE_NOT_SET},
            {1, 1, 1, 0, DMO_E_INVALIDSTREAMINDEX}, {1, 1, 0, 1, E_POINTER}, {1, 1, 0, 0, S_OK},
        };
        for (i = 0; i < ARRAYSIZE(cases); i++)
        {
            if (cases[i].set)
            {
                hr = IMediaObject_SetInputType(dmo, 0, &in, 0);
                if (cases[i].output && hr == S_OK) hr = IMediaObject_SetOutputType(dmo, 0, &out, 0);
            }
            else
            {
                IMediaObject_SetOutputType(dmo, 0, NULL, DMO_SET_TYPEF_CLEAR);
                if (!cases[i].output) IMediaObject_SetInputType(dmo, 0, NULL, DMO_SET_TYPEF_CLEAR);
                hr = S_OK;
            }
            check(hr == S_OK, "case %u set types", i);
            memset(&got, 0, sizeof(got));
            if (cases[i].output) hr = IMediaObject_GetOutputCurrentType(dmo, cases[i].index, cases[i].null ? NULL : &got);
            else hr = IMediaObject_GetInputCurrentType(dmo, cases[i].index, cases[i].null ? NULL : &got);
            check(hr == cases[i].want, "Get%sCurrentType case %u: %#lx, want %#lx", cases[i].output ? "Output" : "Input", i,
                  hr, cases[i].want);
            if (hr == S_OK)
            {
                const DMO_MEDIA_TYPE *w = cases[i].output ? &out : &in;
                check(IsEqualGUID(&got.subtype, &w->subtype) && got.cbFormat == w->cbFormat &&
                      got.pbFormat && !memcmp(got.pbFormat, w->pbFormat, w->cbFormat), "case %u: a copy of the type", i);
                CoTaskMemFree(got.pbFormat);
            }
        }
    }

    check(IMediaObject_AllocateStreamingResources(dmo) == S_OK, "AllocateStreamingResources");
    check(IMediaObject_FreeStreamingResources(dmo) == S_OK, "FreeStreamingResources");

    /* input status: free until ProcessInput holds a buffer, free again after Flush */
    check(IMediaObject_GetInputStatus(dmo, 1, &flags) == DMO_E_INVALIDSTREAMINDEX, "GetInputStatus index 1");
    check(IMediaObject_GetInputStatus(dmo, 0, NULL) == E_POINTER, "GetInputStatus NULL");
    flags = 0xdead;
    hr = IMediaObject_GetInputStatus(dmo, 0, &flags);
    check(hr == S_OK && flags == DMO_INPUT_STATUSF_ACCEPT_DATA, "GetInputStatus empty: %#lx %#lx", hr, flags);
    inbuf.data[0] = 0xff; inbuf.data[1] = 0xfb; inbuf.data[2] = 0x14; inbuf.data[3] = 0xc4; inbuf.len = 4;
    hr = IMediaObject_ProcessInput(dmo, 0, &inbuf.iface, 0, 0, 0);
    flags = 0xdead;
    IMediaObject_GetInputStatus(dmo, 0, &flags);
    check(hr == S_OK && flags == 0, "GetInputStatus holding a buffer: %#lx %#lx", hr, flags);
    IMediaObject_Flush(dmo);
    flags = 0xdead;
    IMediaObject_GetInputStatus(dmo, 0, &flags);
    check(flags == DMO_INPUT_STATUSF_ACCEPT_DATA, "GetInputStatus after Flush: %#lx", flags);

    /* Lock keeps another thread's calls out until the lock is released */
    check(IMediaObject_Lock(dmo, TRUE) == S_OK, "Lock(TRUE)");
    check(IMediaObject_GetInputStatus(dmo, 0, &flags) == S_OK, "the locking thread still calls in");
    done = 0;
    t = CreateThread(NULL, 0, locked_out, NULL, 0, NULL);
    Sleep(300);
    check(!done, "another thread waits while locked");
    check(IMediaObject_Lock(dmo, FALSE) == S_OK, "Lock(FALSE)");
    check(!WaitForSingleObject(t, 5000) && done, "and runs once unlocked");
    CloseHandle(t);

    IMediaObject_Release(dmo);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
