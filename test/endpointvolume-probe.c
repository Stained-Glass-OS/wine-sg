/* endpointvolume-probe -- IAudioEndpointVolume on the default render device,
 * as MeediOS's MasterVolume plugin and every "system volume" control use it
 * (patches/sg/0777). Prints one line per check: "name OK" or "name FAIL ...";
 * "SKIP" alone when there is no audio endpoint here. */
#define COBJMACROS
#define INITGUID
#include <windows.h>
#include <stdio.h>
#include <mmdeviceapi.h>
#include <endpointvolume.h>

static LONG notified;
static float notified_level;
static UINT notified_channels;

static HRESULT WINAPI cb_qi(IAudioEndpointVolumeCallback *iface, REFIID riid, void **ppv)
{
    if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &IID_IAudioEndpointVolumeCallback)) {
        *ppv = iface;
        return S_OK;
    }
    *ppv = NULL;
    return E_NOINTERFACE;
}
static ULONG WINAPI cb_addref(IAudioEndpointVolumeCallback *iface) { return 2; }
static ULONG WINAPI cb_release(IAudioEndpointVolumeCallback *iface) { return 1; }
static HRESULT WINAPI cb_notify(IAudioEndpointVolumeCallback *iface, PAUDIO_VOLUME_NOTIFICATION_DATA d)
{
    notified_level = d->fMasterVolume;
    notified_channels = d->nChannels;
    InterlockedIncrement(&notified);
    return S_OK;
}
static IAudioEndpointVolumeCallbackVtbl cb_vtbl = { cb_qi, cb_addref, cb_release, cb_notify };
static IAudioEndpointVolumeCallback cb = { &cb_vtbl };

static IAudioEndpointVolume *open_volume(IMMDevice *dev)
{
    IAudioEndpointVolume *v = NULL;
    IMMDevice_Activate(dev, &IID_IAudioEndpointVolume, CLSCTX_INPROC_SERVER, NULL, (void **)&v);
    return v;
}

#define CHECK(name, cond, ...) do { if (cond) printf("%s OK\n", name); \
    else { printf("%s FAIL ", name); printf(__VA_ARGS__); printf("\n"); } } while (0)

int main(void)
{
    IMMDeviceEnumerator *en;
    IMMDevice *dev;
    IAudioEndpointVolume *a, *b;
    UINT n = 0, step = 0, steps = 0;
    float level = -1, db = 1, mn, mx, inc;
    BOOL mute;
    HRESULT hr;

    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (FAILED(CoCreateInstance(&CLSID_MMDeviceEnumerator, NULL, CLSCTX_INPROC_SERVER, &IID_IMMDeviceEnumerator,
                                (void **)&en)) ||
        FAILED(IMMDeviceEnumerator_GetDefaultAudioEndpoint(en, eRender, eMultimedia, &dev))) {
        printf("SKIP\n");
        return 0;
    }
    a = open_volume(dev);
    b = open_volume(dev);
    if (!a || !b) { printf("activate FAIL no IAudioEndpointVolume\n"); return 1; }

    hr = IAudioEndpointVolume_GetChannelCount(a, &n);
    CHECK("channels", hr == S_OK && n >= 1, "hr %08lx n %u", hr, n);

    hr = IAudioEndpointVolume_RegisterControlChangeNotify(b, &cb);
    hr = IAudioEndpointVolume_SetMasterVolumeLevelScalar(a, 0.5f, NULL);
    IAudioEndpointVolume_GetMasterVolumeLevelScalar(b, &level);
    CHECK("scalar", hr == S_OK && level > 0.49f && level < 0.51f, "hr %08lx read back %f", hr, level);
    IAudioEndpointVolume_GetMasterVolumeLevel(b, &db);
    CHECK("db", db < -5.9f && db > -6.1f, "0.5 is %f dB, want about -6", db);
    CHECK("notify", notified >= 1 && notified_level > 0.49f && notified_level < 0.51f && notified_channels == n,
          "%ld calls, level %f, %u channels", notified, notified_level, notified_channels);

    if (n) {
        hr = IAudioEndpointVolume_SetChannelVolumeLevelScalar(a, n - 1, 0.25f, NULL);
        level = -1;
        IAudioEndpointVolume_GetChannelVolumeLevelScalar(b, n - 1, &level);
        CHECK("channel", hr == S_OK && level > 0.24f && level < 0.26f, "hr %08lx read back %f", hr, level);
        hr = IAudioEndpointVolume_GetChannelVolumeLevelScalar(b, n, &level);
        CHECK("channel-range", hr == E_INVALIDARG, "channel %u (one past) gave %08lx", n, hr);
    }
    hr = IAudioEndpointVolume_GetVolumeStepInfo(a, &step, &steps);
    CHECK("steps", hr == S_OK && steps > 1 && step < steps, "hr %08lx %u of %u", hr, step, steps);
    hr = IAudioEndpointVolume_GetVolumeRange(a, &mn, &mx, &inc);
    CHECK("range", hr == S_OK && mn < mx && inc > 0, "hr %08lx %f..%f by %f", hr, mn, mx, inc);
    IAudioEndpointVolume_SetMute(a, FALSE, NULL);
    hr = IAudioEndpointVolume_SetMute(a, TRUE, NULL);
    IAudioEndpointVolume_GetMute(b, &mute);
    CHECK("mute", hr == S_OK && mute && IAudioEndpointVolume_SetMute(a, TRUE, NULL) == S_FALSE,
          "hr %08lx mute %d", hr, mute);
    IAudioEndpointVolume_UnregisterControlChangeNotify(b, &cb);
    IAudioEndpointVolume_SetMute(a, FALSE, NULL);
    IAudioEndpointVolume_SetMasterVolumeLevelScalar(a, 1.0f, NULL);
    return 0;
}
