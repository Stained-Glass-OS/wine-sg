/* dsound todo_wine groups (patch 2804), run by test/winmmtodo-dsound-gate.sh:
 * SetFX negotiates the buffer's own format with the effect (a 24-bit buffer is
 * refused by the built-in effects, 8 and 16 bit are accepted), result codes are
 * left alone on an invalid call and "unknown" for a refused type, a failed call
 * removes the effects, a deferred-location buffer has no location before
 * AcquireResources or Play, and a WAVEFORMATEXTENSIBLE with an unknown sub
 * format is INVALIDPARAM (without a FIXME: checked by the gate).
 *
 *   winmmtodo-dsound-probe.exe */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <dsound.h>
#include <mmreg.h>
#include <stdio.h>
#include <string.h>

DEFINE_GUID(GUID_ParamEq, 0x120ced89, 0x3bf4, 0x4173, 0xa1, 0x32, 0x3c, 0xb4, 0x06, 0xcf, 0x32, 0x31);
DEFINE_GUID(IID_MediaObject, 0xd8ad0f58, 0x5494, 0x4102, 0x97, 0xc5, 0xec, 0x79, 0x8e, 0x59, 0xbc, 0xf4);
DEFINE_GUID(GUID_AllObjects_, 0xaa114de5, 0xc262, 0x4169, 0xa1, 0xc8, 0x23, 0xd6, 0x98, 0xcc, 0x73, 0xb5);

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}
#define CHECKF(ok, ...) do { char _b[300]; snprintf(_b, sizeof(_b), __VA_ARGS__); check(ok, _b); } while (0)

static IDirectSoundBuffer8 *make_fx_buffer(IDirectSound8 *ds, WORD bits)
{
    WAVEFORMATEX fmt = {WAVE_FORMAT_PCM, 1, 22050, 0, 0, bits, 0};
    DSBUFFERDESC desc;
    IDirectSoundBuffer *b;
    IDirectSoundBuffer8 *b8 = NULL;

    fmt.nBlockAlign = fmt.nChannels * bits / 8;
    fmt.nAvgBytesPerSec = fmt.nSamplesPerSec * fmt.nBlockAlign;
    memset(&desc, 0, sizeof(desc));
    desc.dwSize = sizeof(desc);
    desc.dwFlags = DSBCAPS_CTRLFX | DSBCAPS_GLOBALFOCUS;
    desc.dwBufferBytes = fmt.nAvgBytesPerSec / 5;
    desc.lpwfxFormat = &fmt;
    if (SUCCEEDED(IDirectSound8_CreateSoundBuffer(ds, &desc, &b, NULL)))
    {
        IDirectSoundBuffer_QueryInterface(b, &IID_IDirectSoundBuffer8, (void **)&b8);
        IDirectSoundBuffer_Release(b);
    }
    return b8;
}

int main(void)
{
    IDirectSound8 *ds;
    IDirectSoundBuffer8 *b;
    IDirectSoundBuffer *plain;
    DSEFFECTDESC fx[2];
    DWORD res[2], status, s1, s2;
    void *p1, *p2;
    HRESULT hr;
    IUnknown *obj;
    HWND hwnd = CreateWindowA("static", "x", WS_POPUP, 0, 0, 10, 10, NULL, NULL, NULL, NULL);

    hr = DirectSoundCreate8(NULL, &ds, NULL);
    if (FAILED(hr))
    {
        printf("note  DirectSoundCreate8 failed (%#lx): skipped\n", hr);
        printf("RESULT: PASS\n");
        return 0;
    }
    IDirectSound8_SetCooperativeLevel(ds, hwnd, DSSCL_PRIORITY);

    memset(fx, 0, sizeof(fx));
    fx[0].dwSize = fx[1].dwSize = sizeof(fx[0]);
    fx[0].guidDSFXClass = GUID_DSFX_STANDARD_PARAMEQ;

    b = make_fx_buffer(ds, 16);
    check(b != NULL, "16-bit effects buffer");
    if (b)
    {
        res[0] = 0xdeadbeef;
        hr = IDirectSoundBuffer8_SetFX(b, 1, fx, res);
        CHECKF(hr == DS_OK && res[0] == DSFXR_LOCSOFTWARE, "16-bit SetFX: %#lx, result %#lx", hr, res[0]);
        hr = IDirectSoundBuffer8_GetObjectInPath(b, &GUID_All_Objects, 0, &IID_MediaObject, (void **)&obj);
        CHECKF(hr == DS_OK, "effect is in the path (%#lx)", hr);
        if (hr == DS_OK) IUnknown_Release(obj);

        /* an invalid call leaves the result codes alone */
        IDirectSoundBuffer8_Lock(b, 0, 0, &p1, &s1, &p2, &s2, DSBLOCK_ENTIREBUFFER);
        res[0] = 0xdeadbeef;
        hr = IDirectSoundBuffer8_SetFX(b, 1, fx, res);
        CHECKF(hr == DSERR_INVALIDCALL && res[0] == 0xdeadbeef, "SetFX on a locked buffer: %#lx, result %#lx", hr, res[0]);
        IDirectSoundBuffer8_Unlock(b, p1, s1, p2, s2);

        /* a failing second effect: first is PRESENT, second UNKNOWN, and nothing stays set */
        fx[1].guidDSFXClass = GUID_NULL;
        res[0] = res[1] = 0xdeadbeef;
        hr = IDirectSoundBuffer8_SetFX(b, 2, fx, res);
        CHECKF(hr == REGDB_E_CLASSNOTREG && res[0] == DSFXR_PRESENT && res[1] == DSFXR_UNKNOWN,
               "SetFX with an unregistered effect: %#lx, results %#lx/%#lx", hr, res[0], res[1]);
        hr = IDirectSoundBuffer8_GetObjectInPath(b, &GUID_All_Objects, 0, &IID_MediaObject, (void **)&obj);
        CHECKF(hr == DSERR_OBJECTNOTFOUND, "a failed SetFX removed the earlier effects (%#lx)", hr);
        IDirectSoundBuffer8_Release(b);
    }

    b = make_fx_buffer(ds, 8);
    if (b)
    {
        res[0] = 0xdeadbeef;
        hr = IDirectSoundBuffer8_SetFX(b, 1, fx, res);
        CHECKF(hr == DS_OK && res[0] == DSFXR_LOCSOFTWARE, "8-bit SetFX: %#lx, result %#lx", hr, res[0]);
        IDirectSoundBuffer8_Release(b);
    }

    /* the built-in effects do not take 24-bit PCM: the buffer's own format is offered */
    b = make_fx_buffer(ds, 24);
    if (b)
    {
        res[0] = 0xdeadbeef;
        hr = IDirectSoundBuffer8_SetFX(b, 1, fx, res);
        CHECKF(hr == (HRESULT)0x80040205 && res[0] == DSFXR_UNKNOWN, "24-bit SetFX is refused: %#lx, result %#lx", hr, res[0]);
        IDirectSoundBuffer8_Release(b);
    }
    else printf("note  no 24-bit buffer here: that part is skipped\n");

    /* deferred location */
    {
        WAVEFORMATEX fmt = {WAVE_FORMAT_PCM, 2, 44100, 176400, 4, 16, 0};
        DSBUFFERDESC desc;
        IDirectSoundBuffer8 *d8;

        memset(&desc, 0, sizeof(desc));
        desc.dwSize = sizeof(desc);
        desc.dwFlags = DSBCAPS_LOCDEFER | DSBCAPS_CTRLVOLUME;
        desc.dwBufferBytes = 17640;
        desc.lpwfxFormat = &fmt;
        hr = IDirectSound8_CreateSoundBuffer(ds, &desc, &plain, NULL);
        if (SUCCEEDED(hr))
        {
            IDirectSoundBuffer_QueryInterface(plain, &IID_IDirectSoundBuffer8, (void **)&d8);
            status = 0xffff;
            IDirectSoundBuffer8_GetStatus(d8, &status);
            CHECKF(status == 0, "LOCDEFER buffer has no location yet (%#lx)", status);
            hr = IDirectSoundBuffer8_AcquireResources(d8, 0, 0, NULL);
            IDirectSoundBuffer8_GetStatus(d8, &status);
            CHECKF(hr == DS_OK && status == DSBSTATUS_LOCSOFTWARE, "after AcquireResources: %#lx, status %#lx", hr, status);
            IDirectSoundBuffer8_Release(d8);
            IDirectSoundBuffer_Release(plain);

            hr = IDirectSound8_CreateSoundBuffer(ds, &desc, &plain, NULL);
            IDirectSoundBuffer_Play(plain, 0, 0, 0);
            IDirectSoundBuffer_GetStatus(plain, &status);
            CHECKF((status & DSBSTATUS_LOCSOFTWARE) && (status & DSBSTATUS_PLAYING), "after Play the location is software (%#lx)", status);
            IDirectSoundBuffer_Stop(plain);
            IDirectSoundBuffer_Release(plain);
        }
    }

    /* an unknown sub format */
    {
        WAVEFORMATEXTENSIBLE wfe;
        DSBUFFERDESC desc;

        memset(&wfe, 0, sizeof(wfe));
        wfe.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
        wfe.Format.nChannels = 2;
        wfe.Format.nSamplesPerSec = 44100;
        wfe.Format.wBitsPerSample = 16;
        wfe.Format.nBlockAlign = 4;
        wfe.Format.nAvgBytesPerSec = 176400;
        wfe.Format.cbSize = 22;
        wfe.Samples.wValidBitsPerSample = 16;
        wfe.SubFormat = GUID_DSFX_STANDARD_PARAMEQ;
        memset(&desc, 0, sizeof(desc));
        desc.dwSize = sizeof(desc);
        desc.dwBufferBytes = 17640;
        desc.lpwfxFormat = &wfe.Format;
        hr = IDirectSound8_CreateSoundBuffer(ds, &desc, &plain, NULL);
        CHECKF(hr == DSERR_INVALIDPARAM, "unknown sub format is INVALIDPARAM (%#lx)", hr);
        if (SUCCEEDED(hr)) IDirectSoundBuffer_Release(plain);
    }

    IDirectSound8_Release(ds);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
