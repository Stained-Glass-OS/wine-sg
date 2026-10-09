/* Device topology and session events (patches/sg/1666), run by
 * test/audiotopo-gate.sh on the default render endpoint:
 *  - IDeviceTopology: the endpoint's connector, linked to its adapter's
 *    jack (IKsJackDescription); the adapter's topology with its stream
 *    connector, volume and mute subunits, signal path, part IDs and
 *    control interfaces; IAudioVolumeLevel and IAudioMute act on the
 *    endpoint volume, and their change callbacks are told of changes;
 *  - session events: IAudioSessionNotification hears of a new session,
 *    IAudioSessionEvents of volume, channel volume, display name and state
 *    changes (with the event context); duck notification registration;
 *    IAudioClient2::GetBufferSizeLimits.
 * These were stubs (E_NOTIMPL, or success with nothing done).
 * Prints SKIP when there is no audio endpoint. */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <audiopolicy.h>
#include <endpointvolume.h>
#include <devicetopology.h>
#include <stdio.h>

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

DEFINE_GUID(node_mute, 0x02b223c0,0xc557,0x11d0,0x8a,0x2b,0x00,0xa0,0xc9,0x25,0x5a,0xc1);
DEFINE_GUID(node_volume, 0x3a5acc00,0xc557,0x11d0,0x8a,0x2b,0x00,0xa0,0xc9,0x25,0x5a,0xc1);
static const GUID my_context = {0x5347aaaa,0x1666,0x4d00,{1,2,3,4,5,6,7,8}};

/* IControlChangeNotify */
struct change_sink { IControlChangeNotify iface; LONG calls; GUID ctx; };
static HRESULT WINAPI cn_qi(IControlChangeNotify *iface, REFIID riid, void **out)
{
    if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &IID_IControlChangeNotify)) { *out = iface; return S_OK; }
    *out = NULL;
    return E_NOINTERFACE;
}
static ULONG WINAPI cn_addref(IControlChangeNotify *iface) { return 2; }
static ULONG WINAPI cn_release(IControlChangeNotify *iface) { return 1; }
static HRESULT WINAPI cn_notify(IControlChangeNotify *iface, DWORD pid, const GUID *ctx)
{
    struct change_sink *s = (struct change_sink *)iface;
    if (ctx) s->ctx = *ctx;
    InterlockedIncrement(&s->calls);
    return S_OK;
}
static IControlChangeNotifyVtbl cn_vtbl = { cn_qi, cn_addref, cn_release, cn_notify };

/* IAudioSessionEvents */
struct events { IAudioSessionEvents iface; LONG volume, channel, name, state; float level; GUID ctx;
                AudioSessionState last_state; DWORD changed; };
static HRESULT WINAPI ev_qi(IAudioSessionEvents *iface, REFIID riid, void **out)
{
    if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &IID_IAudioSessionEvents)) { *out = iface; return S_OK; }
    *out = NULL;
    return E_NOINTERFACE;
}
static ULONG WINAPI ev_addref(IAudioSessionEvents *iface) { return 2; }
static ULONG WINAPI ev_release(IAudioSessionEvents *iface) { return 1; }
static HRESULT WINAPI ev_name(IAudioSessionEvents *iface, LPCWSTR name, LPCGUID ctx)
{
    struct events *e = (struct events *)iface;
    if (!lstrcmpW(name, L"Probe session")) InterlockedIncrement(&e->name);
    return S_OK;
}
static HRESULT WINAPI ev_icon(IAudioSessionEvents *iface, LPCWSTR path, LPCGUID ctx) { return S_OK; }
static HRESULT WINAPI ev_volume(IAudioSessionEvents *iface, float level, BOOL mute, LPCGUID ctx)
{
    struct events *e = (struct events *)iface;
    e->level = level;
    if (ctx) e->ctx = *ctx;
    InterlockedIncrement(&e->volume);
    return S_OK;
}
static HRESULT WINAPI ev_channel(IAudioSessionEvents *iface, DWORD count, float *levels, DWORD changed, LPCGUID ctx)
{
    struct events *e = (struct events *)iface;
    e->changed = changed;
    if (count && levels[0] > 0.29f && levels[0] < 0.31f) InterlockedIncrement(&e->channel);
    return S_OK;
}
static HRESULT WINAPI ev_group(IAudioSessionEvents *iface, LPCGUID group, LPCGUID ctx) { return S_OK; }
static HRESULT WINAPI ev_state(IAudioSessionEvents *iface, AudioSessionState state)
{
    struct events *e = (struct events *)iface;
    e->last_state = state;
    InterlockedIncrement(&e->state);
    return S_OK;
}
static HRESULT WINAPI ev_disconnect(IAudioSessionEvents *iface, AudioSessionDisconnectReason reason) { return S_OK; }
static IAudioSessionEventsVtbl ev_vtbl = { ev_qi, ev_addref, ev_release, ev_name, ev_icon, ev_volume, ev_channel,
                                           ev_group, ev_state, ev_disconnect };

/* IAudioSessionNotification */
struct created { IAudioSessionNotification iface; LONG calls; };
static HRESULT WINAPI sn_qi(IAudioSessionNotification *iface, REFIID riid, void **out)
{
    if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &IID_IAudioSessionNotification)) { *out = iface; return S_OK; }
    *out = NULL;
    return E_NOINTERFACE;
}
static ULONG WINAPI sn_addref(IAudioSessionNotification *iface) { return 2; }
static ULONG WINAPI sn_release(IAudioSessionNotification *iface) { return 1; }
static HRESULT WINAPI sn_created(IAudioSessionNotification *iface, IAudioSessionControl *control)
{
    struct created *c = (struct created *)iface;
    AudioSessionState state;
    if (control && SUCCEEDED(IAudioSessionControl_GetState(control, &state))) InterlockedIncrement(&c->calls);
    return S_OK;
}
static IAudioSessionNotificationVtbl sn_vtbl = { sn_qi, sn_addref, sn_release, sn_created };

/* IAudioVolumeDuckNotification */
static HRESULT WINAPI dn_qi(IAudioVolumeDuckNotification *iface, REFIID riid, void **out)
{
    *out = iface;
    return S_OK;
}
static ULONG WINAPI dn_addref(IAudioVolumeDuckNotification *iface) { return 2; }
static ULONG WINAPI dn_release(IAudioVolumeDuckNotification *iface) { return 1; }
static HRESULT WINAPI dn_duck(IAudioVolumeDuckNotification *iface, LPCWSTR id, UINT32 n) { return S_OK; }
static HRESULT WINAPI dn_unduck(IAudioVolumeDuckNotification *iface, LPCWSTR id) { return S_OK; }
static IAudioVolumeDuckNotificationVtbl dn_vtbl = { dn_qi, dn_addref, dn_release, dn_duck, dn_unduck };

static IPart *one_part(IPartsList *list)
{
    IPart *part = NULL;
    UINT n = 0;
    if (!list) return NULL;
    IPartsList_GetCount(list, &n);
    if (n == 1) IPartsList_GetPart(list, 0, &part);
    IPartsList_Release(list);
    return part;
}

static BOOL subtype_is(IPart *part, const GUID *want)
{
    GUID g;
    return part && SUCCEEDED(IPart_GetSubType(part, &g)) && IsEqualGUID(&g, want);
}

int main(void)
{
    struct change_sink level_sink = { { &cn_vtbl } }, mute_sink = { { &cn_vtbl } };
    struct events events = { { &ev_vtbl } };
    struct created created = { { &sn_vtbl } };
    IAudioVolumeDuckNotification duck = { &dn_vtbl };
    IMMDeviceEnumerator *mme;
    IMMDevice *dev;
    IDeviceTopology *topo, *adapter;
    IConnector *conn, *jack_conn, *stream_conn;
    IPart *jack, *mute_part, *volume_part, *stream_part;
    IPartsList *path;
    IKsJackDescription *jacks;
    KSJACK_DESCRIPTION desc;
    IAudioVolumeLevel *level;
    IAudioMute *mute;
    IAudioEndpointVolume *aev;
    IControlInterface *ci;
    IAudioSessionManager2 *mgr;
    IAudioClient *client;
    IAudioClient2 *client2;
    IAudioSessionControl *control;
    ISimpleAudioVolume *simple;
    IChannelAudioVolume *chan;
    WAVEFORMATEX *fmt;
    WCHAR *id = NULL, *adapter_id = NULL, *endpoint_id = NULL, *name;
    UINT count, partid, channels;
    float min, max, step, db, got;
    BOOL b, connected;
    DataFlow flow;
    PartType type;
    GUID iid, session_guid;
    REFERENCE_TIME lmin, lmax;
    HRESULT hr;
    int i;

    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    CoCreateInstance(&CLSID_MMDeviceEnumerator, NULL, CLSCTX_INPROC_SERVER, &IID_IMMDeviceEnumerator, (void **)&mme);
    if (!mme || FAILED(IMMDeviceEnumerator_GetDefaultAudioEndpoint(mme, eRender, eMultimedia, &dev)))
    {
        printf("SKIP\n");
        return 0;
    }
    IMMDevice_GetId(dev, &endpoint_id);
    IMMDevice_Activate(dev, &IID_IAudioEndpointVolume, CLSCTX_INPROC_SERVER, NULL, (void **)&aev);

    /* the endpoint's topology */
    hr = IMMDevice_Activate(dev, &IID_IDeviceTopology, CLSCTX_INPROC_SERVER, NULL, (void **)&topo);
    check(hr == S_OK, "IDeviceTopology");
    if (hr != S_OK) goto done;
    check(IDeviceTopology_GetDeviceId(topo, &id) == S_OK && !lstrcmpW(id, endpoint_id), "its device is the endpoint");
    CoTaskMemFree(id);
    check(IDeviceTopology_GetConnectorCount(topo, &count) == S_OK && count == 1, "one connector");
    IDeviceTopology_GetConnector(topo, 0, &conn);
    check(IConnector_GetDataFlow(conn, &flow) == S_OK && flow == In, "data flows into it");
    check(IConnector_IsConnected(conn, &connected) == S_OK && connected, "it is connected");
    check(IConnector_GetDeviceIdConnectedTo(conn, &adapter_id) == S_OK && lstrcmpW(adapter_id, endpoint_id),
          "to the adapter's device");
    check(IConnector_GetConnectedTo(conn, &jack_conn) == S_OK, "GetConnectedTo");
    IConnector_QueryInterface(jack_conn, &IID_IPart, (void **)&jack);
    check(jack && IPart_GetPartType(jack, &type) == S_OK && type == Connector, "the jack is a connector part");
    check(IConnector_GetDataFlow(jack_conn, &flow) == S_OK && flow == Out, "data flows out of it");
    hr = IPart_Activate(jack, CLSCTX_INPROC_SERVER, &IID_IKsJackDescription, (void **)&jacks);
    check(hr == S_OK && IKsJackDescription_GetJackCount(jacks, &count) == S_OK && count == 1 &&
          IKsJackDescription_GetJackDescription(jacks, 0, &desc) == S_OK && desc.IsConnected,
          "IKsJackDescription: one connected jack");
    if (hr == S_OK) IKsJackDescription_Release(jacks);

    /* the adapter's */
    check(IPart_GetTopologyObjects(jack, &adapter) == S_OK && IDeviceTopology_GetDeviceId(adapter, &id) == S_OK &&
          !lstrcmpW(id, adapter_id), "the jack's topology is the adapter's");
    CoTaskMemFree(id);
    check(IDeviceTopology_GetConnectorCount(adapter, &count) == S_OK && count == 2 &&
          IDeviceTopology_GetSubunitCount(adapter, &count) == S_OK && count == 2, "two connectors, two subunits");
    path = NULL;
    IPart_EnumPartsIncoming(jack, &path);
    mute_part = one_part(path);
    check(subtype_is(mute_part, &node_mute), "before the jack: the mute");
    path = NULL;
    if (mute_part) IPart_EnumPartsIncoming(mute_part, &path);
    volume_part = one_part(path);
    check(subtype_is(volume_part, &node_volume), "before it: the volume");
    path = NULL;
    if (volume_part) IPart_EnumPartsIncoming(volume_part, &path);
    stream_part = one_part(path);
    check(stream_part && IPart_QueryInterface(stream_part, &IID_IConnector, (void **)&stream_conn) == S_OK &&
          IConnector_IsConnected(stream_conn, &connected) == S_OK && !connected, "and the stream connector");
    path = NULL;
    check(IPart_EnumPartsOutgoing(jack, &path) == HRESULT_FROM_WIN32(ERROR_NOT_FOUND), "nothing after the jack");
    check(stream_part && IDeviceTopology_GetSignalPath(adapter, stream_part, jack, FALSE, &path) == S_OK &&
          IPartsList_GetCount(path, &count) == S_OK && count == 4, "the signal path: four parts");
    check(volume_part && IPart_GetLocalId(volume_part, &partid) == S_OK &&
          IDeviceTopology_GetPartById(adapter, partid, &stream_part) == S_OK, "GetPartById");
    check(volume_part && IPart_GetControlInterfaceCount(volume_part, &count) == S_OK && count == 1 &&
          IPart_GetControlInterface(volume_part, 0, &ci) == S_OK && IControlInterface_GetIID(ci, &iid) == S_OK &&
          IsEqualGUID(&iid, &IID_IAudioVolumeLevel) && IControlInterface_GetName(ci, &name) == S_OK,
          "the volume's control interface");

    /* the controls */
    hr = volume_part ? IPart_Activate(volume_part, CLSCTX_INPROC_SERVER, &IID_IAudioVolumeLevel, (void **)&level) : E_FAIL;
    check(hr == S_OK, "IAudioVolumeLevel");
    if (hr == S_OK)
    {
        IAudioEndpointVolume_GetChannelCount(aev, &channels);
        check(IAudioVolumeLevel_GetChannelCount(level, &count) == S_OK && count == channels, "its channels");
        check(IAudioVolumeLevel_GetLevelRange(level, 0, &min, &max, &step) == S_OK && min < max, "its range");
        db = min + (max - min) / 2;
        check(IAudioVolumeLevel_SetLevel(level, 0, db, NULL) == S_OK &&
              IAudioEndpointVolume_GetChannelVolumeLevel(aev, 0, &got) == S_OK && got > db - 1 && got < db + 1,
              "SetLevel is the endpoint's channel volume");
        check(IPart_RegisterControlChangeCallback(volume_part, &IID_IAudioVolumeLevel, &level_sink.iface) == S_OK,
              "a volume change callback");
        IAudioEndpointVolume_SetMasterVolumeLevelScalar(aev, 0.25f, &my_context);
        check(level_sink.calls >= 1 && IsEqualGUID(&level_sink.ctx, &my_context), "is told, with the context");
        check(IPart_UnregisterControlChangeCallback(volume_part, &level_sink.iface) == S_OK, "and removed");
        IAudioVolumeLevel_Release(level);
    }
    hr = mute_part ? IPart_Activate(mute_part, CLSCTX_INPROC_SERVER, &IID_IAudioMute, (void **)&mute) : E_FAIL;
    check(hr == S_OK, "IAudioMute");
    if (hr == S_OK)
    {
        check(IPart_RegisterControlChangeCallback(mute_part, &IID_IAudioMute, &mute_sink.iface) == S_OK,
              "a mute change callback");
        check(IAudioMute_SetMute(mute, TRUE, NULL) == S_OK && IAudioEndpointVolume_GetMute(aev, &b) == S_OK && b,
              "SetMute mutes the endpoint");
        check(mute_sink.calls == 1, "the mute callback is told");
        IAudioMute_SetMute(mute, FALSE, NULL);
        IPart_UnregisterControlChangeCallback(mute_part, &mute_sink.iface);
        IAudioMute_Release(mute);
    }

    /* sessions */
    hr = IMMDevice_Activate(dev, &IID_IAudioSessionManager2, CLSCTX_INPROC_SERVER, NULL, (void **)&mgr);
    check(hr == S_OK && IAudioSessionManager2_RegisterSessionNotification(mgr, &created.iface) == S_OK,
          "RegisterSessionNotification");
    check(IAudioSessionManager2_RegisterDuckNotification(mgr, NULL, &duck) == S_OK &&
          IAudioSessionManager2_UnregisterDuckNotification(mgr, &duck) == S_OK &&
          IAudioSessionManager2_UnregisterDuckNotification(mgr, &duck) == E_INVALIDARG,
          "duck notifications register and unregister");
    IMMDevice_Activate(dev, &IID_IAudioClient, CLSCTX_INPROC_SERVER, NULL, (void **)&client);
    IAudioClient_GetMixFormat(client, &fmt);
    if (SUCCEEDED(IAudioClient_QueryInterface(client, &IID_IAudioClient2, (void **)&client2)))
    {
        check(IAudioClient2_GetBufferSizeLimits(client2, fmt, TRUE, &lmin, &lmax) == S_OK && lmin > 0 &&
              lmax == 20000000, "GetBufferSizeLimits");
        IAudioClient2_Release(client2);
    }
    CoCreateGuid(&session_guid);
    hr = IAudioClient_Initialize(client, AUDCLNT_SHAREMODE_SHARED, 0, 5000000, 0, fmt, &session_guid);
    check(hr == S_OK, "a stream in a new session");
    check(created.calls == 1, "OnSessionCreated");
    IAudioClient_GetService(client, &IID_IAudioSessionControl, (void **)&control);
    check(IAudioSessionControl_RegisterAudioSessionNotification(control, &events.iface) == S_OK,
          "RegisterAudioSessionNotification");
    IAudioClient_GetService(client, &IID_ISimpleAudioVolume, (void **)&simple);
    ISimpleAudioVolume_SetMasterVolume(simple, 0.5f, &my_context);
    check(events.volume == 1 && events.level == 0.5f && IsEqualGUID(&events.ctx, &my_context),
          "OnSimpleVolumeChanged, with the context");
    IAudioClient_GetService(client, &IID_IChannelAudioVolume, (void **)&chan);
    IChannelAudioVolume_SetChannelVolume(chan, 0, 0.3f, NULL);
    check(events.channel == 1 && events.changed == 0, "OnChannelVolumeChanged");
    IAudioSessionControl_SetDisplayName(control, L"Probe session", NULL);
    check(events.name == 1, "OnDisplayNameChanged");
    IAudioClient_Start(client);
    check(events.state >= 1 && events.last_state == AudioSessionStateActive, "OnStateChanged: active");
    IAudioClient_Stop(client);
    check(events.last_state == AudioSessionStateInactive, "OnStateChanged: inactive");
    check(IAudioSessionControl_UnregisterAudioSessionNotification(control, &events.iface) == S_OK, "unregistered");
    i = events.volume;
    ISimpleAudioVolume_SetMasterVolume(simple, 1.0f, NULL);
    check(events.volume == i, "and no longer told");
    IAudioSessionManager2_UnregisterSessionNotification(mgr, &created.iface);

done:
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
