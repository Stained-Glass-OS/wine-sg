/* dsound's FIXME stubs (patches/sg/2833), run by test/dsound-stubs-gate.sh:
 * Restore on both buffer kinds, AcquireResources, the capture buffer's
 * Initialize / GetObjectInPath / GetFXStatus, the class factory's LockServer
 * and IKsPropertySet::Set. The gate also fails when any of them logs a FIXME.
 * Parts that need a render or capture device are skipped (said) without one.
 *
 *   dsound-stubs-probe.exe */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <dsound.h>
#include <stdio.h>
#include <string.h>

DEFINE_GUID(CLSID_DSPrivate, 0x11ab3ec0, 0x25ec, 0x11d1, 0xa4, 0xd8, 0x00, 0xc0, 0x4f, 0xc2, 0x8a, 0xca);
DEFINE_GUID(IID_KsPropSet, 0x31efac30, 0x515c, 0x11d0, 0xa9, 0xaa, 0x00, 0xaa, 0x00, 0x61, 0xbe, 0x93);
DEFINE_GUID(GUID_ParamEq, 0x120ced89, 0x3bf4, 0x4173, 0xa1, 0x32, 0x3c, 0xb4, 0x06, 0xcf, 0x32, 0x31);

#ifndef E_PROP_ID_UNSUPPORTED
#define E_PROP_ID_UNSUPPORTED ((HRESULT)0x80070490)
#endif
static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}
#define CHECKF(ok, ...) do { char _b[256]; snprintf(_b, sizeof(_b), __VA_ARGS__); check(ok, _b); } while (0)

static void test_render(HWND hwnd)
{
    IDirectSound8 *ds;
    IDirectSoundBuffer *primary, *secondary, *plain, *tmp;
    IDirectSoundBuffer8 *fx8;
    WAVEFORMATEX fmt = {WAVE_FORMAT_PCM, 1, 22050, 44100, 2, 16, 0};
    DSBUFFERDESC desc;
    DWORD results[2];
    DSEFFECTDESC effect;
    HRESULT hr;

    hr = DirectSoundCreate8(NULL, &ds, NULL);
    if (FAILED(hr))
    {
        printf("note  DirectSoundCreate8 failed (%#lx): the render part is skipped\n", hr);
        return;
    }
    hr = IDirectSound8_SetCooperativeLevel(ds, hwnd, DSSCL_PRIORITY);
    CHECKF(hr == S_OK, "SetCooperativeLevel (%#lx)", hr);

    memset(&desc, 0, sizeof(desc));
    desc.dwSize = sizeof(desc);
    desc.dwFlags = DSBCAPS_PRIMARYBUFFER;
    hr = IDirectSound8_CreateSoundBuffer(ds, &desc, &primary, NULL);
    CHECKF(hr == S_OK, "primary buffer (%#lx)", hr);
    if (hr == S_OK)
    {
        hr = IDirectSoundBuffer_Restore(primary);
        CHECKF(hr == DS_OK, "primary Restore is DS_OK (%#lx)", hr);
    }

    memset(&desc, 0, sizeof(desc));
    desc.dwSize = sizeof(desc);
    desc.dwFlags = DSBCAPS_CTRLFX | DSBCAPS_GLOBALFOCUS;
    desc.dwBufferBytes = 44100;
    desc.lpwfxFormat = &fmt;
    hr = IDirectSound8_CreateSoundBuffer(ds, &desc, &secondary, NULL);
    CHECKF(hr == S_OK, "secondary buffer with DSBCAPS_CTRLFX (%#lx)", hr);
    if (hr != S_OK) goto done;
    hr = IDirectSoundBuffer_Restore(secondary);
    CHECKF(hr == DS_OK, "secondary Restore is DS_OK (%#lx)", hr);

    hr = IDirectSoundBuffer_QueryInterface(secondary, &IID_IDirectSoundBuffer8, (void **)&fx8);
    CHECKF(hr == S_OK, "IDirectSoundBuffer8 (%#lx)", hr);
    if (hr != S_OK) goto done;

    hr = IDirectSoundBuffer8_AcquireResources(fx8, 0, 0, NULL);
    CHECKF(hr == DS_OK, "AcquireResources with no effects set and none asked is DS_OK (%#lx)", hr);
    hr = IDirectSoundBuffer8_AcquireResources(fx8, 0x40, 0, NULL);
    CHECKF(hr == DSERR_INVALIDPARAM, "AcquireResources with an unknown flag is DSERR_INVALIDPARAM (%#lx)", hr);
    results[0] = 0x55;
    hr = IDirectSoundBuffer8_AcquireResources(fx8, 0, 1, results);
    CHECKF(hr == DSERR_INVALIDPARAM, "AcquireResources for one effect when none was set is DSERR_INVALIDPARAM (%#lx)", hr);

    memset(&effect, 0, sizeof(effect));
    effect.dwSize = sizeof(effect);
    effect.guidDSFXClass = GUID_ParamEq;
    results[0] = 0x55;
    hr = IDirectSoundBuffer8_SetFX(fx8, 1, &effect, results);
    if (hr == DS_OK && results[0] == DSFXR_LOCSOFTWARE)
    {
        results[0] = 0x55;
        hr = IDirectSoundBuffer8_AcquireResources(fx8, 0, 1, results);
        CHECKF(hr == DS_OK && results[0] == DSFXR_LOCSOFTWARE, "AcquireResources after SetFX gives DSFXR_LOCSOFTWARE (%#lx, %#lx)", hr, results[0]);
        hr = IDirectSoundBuffer8_AcquireResources(fx8, 0, 2, results);
        CHECKF(hr == DSERR_INVALIDPARAM, "AcquireResources for two effects when one was set is DSERR_INVALIDPARAM (%#lx)", hr);
        hr = IDirectSoundBuffer8_AcquireResources(fx8, 0, 1, NULL);
        CHECKF(hr == DSERR_INVALIDPARAM, "AcquireResources without a result array is DSERR_INVALIDPARAM (%#lx)", hr);
        IDirectSoundBuffer8_SetFX(fx8, 0, NULL, NULL);
    }
    else printf("note  the parametric EQ effect is not available (%#lx, %#lx): the effect case is skipped\n", hr, results[0]);
    IDirectSoundBuffer8_Release(fx8);

    memset(&desc, 0, sizeof(desc));
    desc.dwSize = sizeof(desc);
    desc.dwFlags = DSBCAPS_GLOBALFOCUS;
    desc.dwBufferBytes = 44100;
    desc.lpwfxFormat = &fmt;
    hr = IDirectSound8_CreateSoundBuffer(ds, &desc, &plain, NULL);
    if (hr == S_OK)
    {
        hr = IDirectSoundBuffer_QueryInterface(plain, &IID_IDirectSoundBuffer8, (void **)&fx8);
        hr = IDirectSoundBuffer8_AcquireResources(fx8, 0, 0, NULL);
        CHECKF(hr == DS_OK, "AcquireResources with no effects on a buffer without DSBCAPS_CTRLFX is DS_OK (%#lx)", hr);
        hr = IDirectSoundBuffer8_AcquireResources(fx8, DSBPLAY_LOCSOFTWARE, 0, NULL);
        CHECKF(hr == DS_OK, "the software location may be asked for (%#lx)", hr);
        hr = IDirectSoundBuffer8_AcquireResources(fx8, DSBPLAY_LOCHARDWARE, 0, NULL);
        CHECKF(hr == DSERR_INVALIDPARAM, "the hardware location cannot (%#lx)", hr);
        results[0] = 0x55;
        hr = IDirectSoundBuffer8_AcquireResources(fx8, 0, 1, results);
        CHECKF(hr == DSERR_CONTROLUNAVAIL, "asking for an effect on a buffer without DSBCAPS_CTRLFX is DSERR_CONTROLUNAVAIL (%#lx)", hr);
        IDirectSoundBuffer8_Release(fx8);
        IDirectSoundBuffer_Release(plain);
    }
    IDirectSoundBuffer_Release(secondary);
done:
    if (primary) IDirectSoundBuffer_Release(primary);
    IDirectSound8_Release(ds);
    (void)tmp;
}

static void test_capture(void)
{
    IDirectSoundCapture8 *cap;
    IDirectSoundCaptureBuffer *buffer;
    IDirectSoundCaptureBuffer8 *buffer8;
    WAVEFORMATEX fmt = {WAVE_FORMAT_PCM, 1, 22050, 44100, 2, 16, 0};
    DSCBUFFERDESC desc;
    DWORD status[2] = {0x55, 0x55};
    void *obj = (void *)1;
    HRESULT hr;

    hr = DirectSoundCaptureCreate8(NULL, &cap, NULL);
    if (FAILED(hr)) { printf("note  DirectSoundCaptureCreate8 failed (%#lx): the capture part is skipped\n", hr); return; }
    memset(&desc, 0, sizeof(desc));
    desc.dwSize = sizeof(desc);
    desc.dwBufferBytes = 44100;
    desc.lpwfxFormat = &fmt;
    hr = IDirectSoundCapture_CreateCaptureBuffer(cap, &desc, &buffer, NULL);
    if (FAILED(hr)) { printf("note  CreateCaptureBuffer failed (%#lx): skipped\n", hr); IDirectSoundCapture_Release(cap); return; }
    IDirectSoundCaptureBuffer_QueryInterface(buffer, &IID_IDirectSoundCaptureBuffer8, (void **)&buffer8);

    hr = IDirectSoundCaptureBuffer_Initialize(buffer, (IDirectSoundCapture *)cap, &desc);
    CHECKF(hr == DSERR_ALREADYINITIALIZED, "capture buffer Initialize is DSERR_ALREADYINITIALIZED (%#lx)", hr);
    hr = IDirectSoundCaptureBuffer8_GetObjectInPath(buffer8, &GUID_NULL, 0, &IID_IUnknown, &obj);
    CHECKF(hr == DSERR_CONTROLUNAVAIL && !obj, "GetObjectInPath is DSERR_CONTROLUNAVAIL with a NULL object (%#lx)", hr);
    hr = IDirectSoundCaptureBuffer8_GetFXStatus(buffer8, 1, status);
    CHECKF(hr == DSERR_CONTROLUNAVAIL, "GetFXStatus is DSERR_CONTROLUNAVAIL (%#lx)", hr);
    hr = IDirectSoundCaptureBuffer8_GetFXStatus(buffer8, 1, NULL);
    CHECKF(hr == DSERR_INVALIDPARAM, "GetFXStatus with no array for one effect is DSERR_INVALIDPARAM (%#lx)", hr);

    IDirectSoundCaptureBuffer8_Release(buffer8);
    IDirectSoundCaptureBuffer_Release(buffer);
    IDirectSoundCapture_Release(cap);
}

static void test_factory_and_propset(void)
{
    HRESULT (WINAPI *get_class_object)(REFCLSID, REFIID, void **) =
        (void *)GetProcAddress(GetModuleHandleA("dsound.dll"), "DllGetClassObject");
    IClassFactory *factory = NULL;
    IKsPropertySet *ks = NULL;
    HRESULT hr;
    char data[8] = {0};

    check(get_class_object != NULL, "dsound exports DllGetClassObject");
    if (get_class_object && get_class_object(&CLSID_DirectSound8, &IID_IClassFactory, (void **)&factory) == S_OK)
    {
        hr = IClassFactory_LockServer(factory, TRUE);
        CHECKF(hr == S_OK, "LockServer(TRUE) is S_OK (%#lx)", hr);
        hr = IClassFactory_LockServer(factory, FALSE);
        CHECKF(hr == S_OK, "LockServer(FALSE) is S_OK (%#lx)", hr);
        IClassFactory_Release(factory);
    }
    else check(0, "class factory for DirectSound8");

    hr = CoCreateInstance(&CLSID_DSPrivate, NULL, CLSCTX_INPROC_SERVER, &IID_KsPropSet, (void **)&ks);
    if (hr == S_OK)
    {
        hr = IKsPropertySet_Set(ks, &GUID_NULL, 1, NULL, 0, data, sizeof(data));
        CHECKF(hr == E_PROP_ID_UNSUPPORTED, "IKsPropertySet::Set is E_PROP_ID_UNSUPPORTED (%#lx)", hr);
        IKsPropertySet_Release(ks);
    }
    else printf("note  the DirectSound private property set is not creatable (%#lx)\n", hr);
}

int main(void)
{
    HWND hwnd;

    CoInitialize(NULL);
    hwnd = CreateWindowExA(0, "static", "dsound probe", WS_POPUP, 0, 0, 10, 10, NULL, NULL, NULL, NULL);
    test_render(hwnd);
    test_capture();
    test_factory_and_propset();
    if (hwnd) DestroyWindow(hwnd);
    printf("%s\n", failures ? "RESULT: FAIL" : "RESULT: PASS");
    return failures != 0;
}
