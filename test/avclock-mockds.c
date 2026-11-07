/* A stand-in dsound.dll for test/avclock-gate.sh: its sound buffers "play"
 * at SGMOCKDS_PERMILLE thousandths of real time (1010 = 1% fast), the way a
 * sound device's clock drifts from the system's. The play cursor moves in
 * 10 ms steps, as a mixer's does. sgmock_played_us() gives what the playing
 * buffer has played, for the probe to compare the renderer's clock with. */
#define COBJMACROS
#include <windows.h>
#include <dsound.h>
#include <stdlib.h>

static LONG permille = 1000;
static struct buffer *current;       /* the last buffer that played (the renderer keeps it) */

struct buffer
{
    IDirectSoundBuffer IDirectSoundBuffer_iface;
    LONG ref;
    BYTE *data;
    DWORD size, bps, align;
    BOOL playing;
    LONGLONG start_qpc, base_us;     /* played before the current Play */
};

static LONGLONG now_qpc(void) { LARGE_INTEGER t; QueryPerformanceCounter(&t); return t.QuadPart; }
static LONGLONG qpf(void) { LARGE_INTEGER f; QueryPerformanceFrequency(&f); return f.QuadPart; }

static LONGLONG buffer_played_us(struct buffer *b)
{
    LONGLONG us = b->base_us;
    if (b->playing)
    {
        LONGLONG real = (now_qpc() - b->start_qpc) * 1000000 / qpf();
        real -= real % 10000;
        us += real * permille / 1000;
    }
    return us;
}

static struct buffer *impl_buf(IDirectSoundBuffer *iface) { return CONTAINING_RECORD(iface, struct buffer, IDirectSoundBuffer_iface); }

static HRESULT WINAPI b_QI(IDirectSoundBuffer *iface, REFIID iid, void **out)
{
    if (IsEqualGUID(iid, &IID_IUnknown) || IsEqualGUID(iid, &IID_IDirectSoundBuffer))
    {
        *out = iface; IDirectSoundBuffer_AddRef(iface); return S_OK;
    }
    *out = NULL; return E_NOINTERFACE;
}
static ULONG WINAPI b_AddRef(IDirectSoundBuffer *iface) { return InterlockedIncrement(&impl_buf(iface)->ref); }
static ULONG WINAPI b_Release(IDirectSoundBuffer *iface)
{
    struct buffer *b = impl_buf(iface);
    ULONG ref = InterlockedDecrement(&b->ref);
    if (!ref) { if (current == b) current = NULL; free(b->data); free(b); }
    return ref;
}
static HRESULT WINAPI b_GetCaps(IDirectSoundBuffer *iface, DSBCAPS *caps) { return E_NOTIMPL; }
static HRESULT WINAPI b_GetCurrentPosition(IDirectSoundBuffer *iface, DWORD *play, DWORD *write)
{
    struct buffer *b = impl_buf(iface);
    LONGLONG us = buffer_played_us(b), bytes = us * b->bps / 1000000;
    DWORD pos;
    bytes -= bytes % b->align;
    pos = (DWORD)(bytes % b->size);
    if (play) *play = pos;
    if (write) *write = (pos + (b->bps / 100 / b->align) * b->align) % b->size;
    return S_OK;
}
static HRESULT WINAPI b_GetFormat(IDirectSoundBuffer *iface, WAVEFORMATEX *f, DWORD n, DWORD *w) { return E_NOTIMPL; }
static HRESULT WINAPI b_GetVolume(IDirectSoundBuffer *iface, LONG *v) { *v = 0; return S_OK; }
static HRESULT WINAPI b_GetPan(IDirectSoundBuffer *iface, LONG *v) { *v = 0; return S_OK; }
static HRESULT WINAPI b_GetFrequency(IDirectSoundBuffer *iface, DWORD *v) { return E_NOTIMPL; }
static HRESULT WINAPI b_GetStatus(IDirectSoundBuffer *iface, DWORD *s)
{
    *s = impl_buf(iface)->playing ? DSBSTATUS_PLAYING | DSBSTATUS_LOOPING : 0; return S_OK;
}
static HRESULT WINAPI b_Initialize(IDirectSoundBuffer *iface, IDirectSound *ds, const DSBUFFERDESC *d) { return DSERR_ALREADYINITIALIZED; }
static HRESULT WINAPI b_Lock(IDirectSoundBuffer *iface, DWORD off, DWORD n, void **p1, DWORD *n1, void **p2, DWORD *n2, DWORD flags)
{
    struct buffer *b = impl_buf(iface);
    if (flags & DSBLOCK_ENTIREBUFFER) { off = 0; n = b->size; }
    if (off >= b->size || n > b->size) return DSERR_INVALIDPARAM;
    *p1 = b->data + off;
    *n1 = min(n, b->size - off);
    if (p2) *p2 = n > *n1 ? b->data : NULL;
    if (n2) *n2 = n - *n1;
    return S_OK;
}
static HRESULT WINAPI b_Play(IDirectSoundBuffer *iface, DWORD r, DWORD p, DWORD flags)
{
    struct buffer *b = impl_buf(iface);
    if (!b->playing) { b->start_qpc = now_qpc(); b->playing = TRUE; }
    current = b;
    return S_OK;
}
static HRESULT WINAPI b_SetCurrentPosition(IDirectSoundBuffer *iface, DWORD pos) { return S_OK; }
static HRESULT WINAPI b_SetFormat(IDirectSoundBuffer *iface, const WAVEFORMATEX *f) { return S_OK; }
static HRESULT WINAPI b_SetVolume(IDirectSoundBuffer *iface, LONG v) { return S_OK; }
static HRESULT WINAPI b_SetPan(IDirectSoundBuffer *iface, LONG v) { return S_OK; }
static HRESULT WINAPI b_SetFrequency(IDirectSoundBuffer *iface, DWORD v) { return S_OK; }
static HRESULT WINAPI b_Stop(IDirectSoundBuffer *iface)
{
    struct buffer *b = impl_buf(iface);
    if (b->playing) { b->base_us = buffer_played_us(b); b->playing = FALSE; }
    return S_OK;
}
static HRESULT WINAPI b_Unlock(IDirectSoundBuffer *iface, void *p1, DWORD n1, void *p2, DWORD n2) { return S_OK; }
static HRESULT WINAPI b_Restore(IDirectSoundBuffer *iface) { return S_OK; }

static IDirectSoundBufferVtbl buffer_vtbl =
{
    b_QI, b_AddRef, b_Release, b_GetCaps, b_GetCurrentPosition, b_GetFormat, b_GetVolume, b_GetPan,
    b_GetFrequency, b_GetStatus, b_Initialize, b_Lock, b_Play, b_SetCurrentPosition, b_SetFormat,
    b_SetVolume, b_SetPan, b_SetFrequency, b_Stop, b_Unlock, b_Restore,
};

struct dsound { IDirectSound8 IDirectSound8_iface; LONG ref; };
static struct dsound *impl_ds(IDirectSound8 *iface) { return CONTAINING_RECORD(iface, struct dsound, IDirectSound8_iface); }

static HRESULT WINAPI d_QI(IDirectSound8 *iface, REFIID iid, void **out)
{
    if (IsEqualGUID(iid, &IID_IUnknown) || IsEqualGUID(iid, &IID_IDirectSound) || IsEqualGUID(iid, &IID_IDirectSound8))
    {
        *out = iface; IDirectSound8_AddRef(iface); return S_OK;
    }
    *out = NULL; return E_NOINTERFACE;
}
static ULONG WINAPI d_AddRef(IDirectSound8 *iface) { return InterlockedIncrement(&impl_ds(iface)->ref); }
static ULONG WINAPI d_Release(IDirectSound8 *iface)
{
    struct dsound *d = impl_ds(iface);
    ULONG ref = InterlockedDecrement(&d->ref);
    if (!ref) free(d);
    return ref;
}
static HRESULT WINAPI d_CreateSoundBuffer(IDirectSound8 *iface, const DSBUFFERDESC *desc, IDirectSoundBuffer **out, IUnknown *outer)
{
    struct buffer *b;
    if (!desc || !desc->lpwfxFormat || !desc->dwBufferBytes) return DSERR_INVALIDPARAM;
    if (!(b = calloc(1, sizeof(*b)))) return E_OUTOFMEMORY;
    b->IDirectSoundBuffer_iface.lpVtbl = &buffer_vtbl;
    b->ref = 1;
    b->size = desc->dwBufferBytes;
    b->bps = desc->lpwfxFormat->nAvgBytesPerSec;
    b->align = desc->lpwfxFormat->nBlockAlign ? desc->lpwfxFormat->nBlockAlign : 1;
    if (!(b->data = calloc(1, b->size))) { free(b); return E_OUTOFMEMORY; }
    *out = &b->IDirectSoundBuffer_iface;
    return S_OK;
}
static HRESULT WINAPI d_GetCaps(IDirectSound8 *iface, DSCAPS *caps) { return E_NOTIMPL; }
static HRESULT WINAPI d_Duplicate(IDirectSound8 *iface, IDirectSoundBuffer *in, IDirectSoundBuffer **out) { return E_NOTIMPL; }
static HRESULT WINAPI d_SetCooperativeLevel(IDirectSound8 *iface, HWND hwnd, DWORD level) { return S_OK; }
static HRESULT WINAPI d_Compact(IDirectSound8 *iface) { return S_OK; }
static HRESULT WINAPI d_GetSpeakerConfig(IDirectSound8 *iface, DWORD *c) { *c = DSSPEAKER_STEREO; return S_OK; }
static HRESULT WINAPI d_SetSpeakerConfig(IDirectSound8 *iface, DWORD c) { return S_OK; }
static HRESULT WINAPI d_Initialize(IDirectSound8 *iface, const GUID *g) { return DSERR_ALREADYINITIALIZED; }
static HRESULT WINAPI d_VerifyCertification(IDirectSound8 *iface, DWORD *c) { *c = DS_UNCERTIFIED; return S_OK; }

static IDirectSound8Vtbl dsound_vtbl =
{
    d_QI, d_AddRef, d_Release, d_CreateSoundBuffer, d_GetCaps, d_Duplicate, d_SetCooperativeLevel,
    d_Compact, d_GetSpeakerConfig, d_SetSpeakerConfig, d_Initialize, d_VerifyCertification,
};

__declspec(dllexport) HRESULT WINAPI DirectSoundCreate8(const GUID *guid, IDirectSound8 **out, IUnknown *outer)
{
    struct dsound *d;
    char v[16];
    if (GetEnvironmentVariableA("SGMOCKDS_PERMILLE", v, sizeof(v)) && atoi(v) > 0) permille = atoi(v);
    if (!(d = calloc(1, sizeof(*d)))) return E_OUTOFMEMORY;
    d->IDirectSound8_iface.lpVtbl = &dsound_vtbl;
    d->ref = 1;
    *out = &d->IDirectSound8_iface;
    return S_OK;
}

__declspec(dllexport) HRESULT WINAPI DirectSoundCreate(const GUID *guid, IDirectSound **out, IUnknown *outer)
{
    return DirectSoundCreate8(guid, (IDirectSound8 **)out, outer);
}

/* devenum lists the devices for the audio renderer category */
__declspec(dllexport) HRESULT WINAPI DirectSoundEnumerateW(LPDSENUMCALLBACKW cb, void *context)
{
    static const GUID mock = {0x5347ac10, 0x6267, 0x4d0c, {0x9a, 0x11, 0x6f, 0x63, 0x6b, 0x64, 0x73, 0x01}};
    if (cb(NULL, L"Primary Sound Driver", L"", context)) cb((GUID *)&mock, L"SG stand-in sound", L"mock", context);
    return S_OK;
}

__declspec(dllexport) HRESULT WINAPI DirectSoundEnumerateA(LPDSENUMCALLBACKA cb, void *context)
{
    cb(NULL, "Primary Sound Driver", "", context);
    return S_OK;
}

__declspec(dllexport) LONGLONG WINAPI sgmock_played_us(void) { return current ? buffer_played_us(current) : -1; }
