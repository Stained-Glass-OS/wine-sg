/* Helpers shared by the dmcontent probes (dmstyle / dmband / dmscript / dmcompos, patches 2960-2969):
 * result counting, a RIFF stream builder, a performance whose graph holds a collecting tool,
 * and small wrappers to create, load and play tracks. */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <dmusici.h>
#include <dmusicf.h>
#include <dmerror.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#define ARRAY_SIZE_(a) (sizeof(a) / sizeof((a)[0]))

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}
#define CHECKF(ok, ...) do { char _b[256]; snprintf(_b, sizeof(_b), __VA_ARGS__); check(ok, _b); } while (0)

/* ---- RIFF building ---- */
static char riff[16384];
static int riff_len;
static void put(const void *data, int size) { memcpy(riff + riff_len, data, size); riff_len += size; }
static int begin(const char *id, const char *type)
{
    DWORD zero = 0;
    int at;
    put(id, 4);
    at = riff_len;
    put(&zero, 4);
    if (type) put(type, 4);
    return at;
}
static void end(int at)
{
    DWORD size = riff_len - at - 4;
    memcpy(riff + at, &size, 4);
    if (riff_len & 1) riff[riff_len++] = 0;
}
static void chunk(const char *id, const void *data, int size)
{
    int at = begin(id, NULL);
    put(data, size);
    end(at);
}
static void array_chunk(const char *id, const void *items, DWORD item_size, int count)
{
    int at = begin(id, NULL);
    put(&item_size, 4);
    put(items, item_size * count);
    end(at);
}
static IStream *make_stream(void)
{
    static const LARGE_INTEGER zero;
    IStream *stream;
    CreateStreamOnHGlobal(NULL, TRUE, &stream);
    IStream_Write(stream, riff, riff_len, NULL);
    IStream_Seek(stream, zero, STREAM_SEEK_SET, NULL);
    riff_len = 0;
    return stream;
}
/* the bytes written to a stream, from its start */
static int stream_bytes(IStream *stream, void *buffer, int size)
{
    static const LARGE_INTEGER zero;
    ULONG got = 0;
    IStream_Seek(stream, zero, STREAM_SEEK_SET, NULL);
    IStream_Read(stream, buffer, size, &got);
    return got;
}
static IStream *new_stream(void)
{
    IStream *stream;
    CreateStreamOnHGlobal(NULL, TRUE, &stream);
    return stream;
}
static void rewind_stream(IStream *stream)
{
    static const LARGE_INTEGER zero;
    IStream_Seek(stream, zero, STREAM_SEEK_SET, NULL);
}

/* ---- collecting tool ---- */
struct msg
{
    DWORD type;
    MUSIC_TIME mtTime;
    DWORD pchannel;
    DWORD track_id;
    DWORD group;
    GUID guid;
    DWORD option;
    BYTE midi[3];
    DWORD len;
    DWORD flags;
};
static struct msg msgs[64];
static int msg_count;

static HRESULT WINAPI tool_QueryInterface(IDirectMusicTool *iface, REFIID riid, void **out)
{
    if (IsEqualGUID(riid, &IID_IUnknown) || IsEqualGUID(riid, &IID_IDirectMusicTool))
    {
        *out = iface;
        return S_OK;
    }
    *out = NULL;
    return E_NOINTERFACE;
}
static ULONG WINAPI tool_AddRef(IDirectMusicTool *iface) { return 2; }
static ULONG WINAPI tool_Release(IDirectMusicTool *iface) { return 1; }
static HRESULT WINAPI tool_Init(IDirectMusicTool *iface, IDirectMusicGraph *graph) { return E_NOTIMPL; }
static HRESULT WINAPI tool_GetMsgDeliveryType(IDirectMusicTool *iface, DWORD *type)
{
    *type = DMUS_PMSGF_TOOL_IMMEDIATE;
    return S_OK;
}
static HRESULT WINAPI tool_GetMediaTypeArraySize(IDirectMusicTool *iface, DWORD *size)
{
    *size = 0;
    return S_OK;
}
static HRESULT WINAPI tool_GetMediaTypes(IDirectMusicTool *iface, DWORD **types, DWORD size) { return E_NOTIMPL; }
static HRESULT WINAPI tool_ProcessPMsg(IDirectMusicTool *iface, IDirectMusicPerformance *perf, DMUS_PMSG *pmsg)
{
    struct msg *m;
    if (msg_count >= ARRAY_SIZE_(msgs)) return DMUS_S_FREE;
    m = msgs + msg_count++;
    memset(m, 0, sizeof(*m));
    m->type = pmsg->dwType;
    m->mtTime = pmsg->mtTime;
    m->pchannel = pmsg->dwPChannel;
    m->track_id = pmsg->dwVirtualTrackID;
    m->group = pmsg->dwGroupID;
    m->flags = pmsg->dwFlags;
    switch (pmsg->dwType)
    {
    case DMUS_PMSGT_NOTIFICATION:
        m->guid = ((DMUS_NOTIFICATION_PMSG *)pmsg)->guidNotificationType;
        m->option = ((DMUS_NOTIFICATION_PMSG *)pmsg)->dwNotificationOption;
        break;
    case DMUS_PMSGT_MIDI:
        m->midi[0] = ((DMUS_MIDI_PMSG *)pmsg)->bStatus;
        m->midi[1] = ((DMUS_MIDI_PMSG *)pmsg)->bByte1;
        m->midi[2] = ((DMUS_MIDI_PMSG *)pmsg)->bByte2;
        break;
    case DMUS_PMSGT_PATCH:
        m->midi[0] = ((DMUS_PATCH_PMSG *)pmsg)->byInstrument;
        m->midi[1] = ((DMUS_PATCH_PMSG *)pmsg)->byMSB;
        m->midi[2] = ((DMUS_PATCH_PMSG *)pmsg)->byLSB;
        break;
    }
    return DMUS_S_FREE;
}
static HRESULT WINAPI tool_Flush(IDirectMusicTool *iface, IDirectMusicPerformance *perf, DMUS_PMSG *pmsg, REFERENCE_TIME time)
{
    return S_OK;
}
static const IDirectMusicToolVtbl tool_vtbl = {tool_QueryInterface, tool_AddRef, tool_Release, tool_Init,
        tool_GetMsgDeliveryType, tool_GetMediaTypeArraySize, tool_GetMediaTypes, tool_ProcessPMsg, tool_Flush};
static IDirectMusicTool collector = {(IDirectMusicToolVtbl *)&tool_vtbl};

static IDirectMusicPerformance *perf;

/* a performance (no audio) whose graph holds the collecting tool; 0 when DirectMusic is not usable */
static int setup_perf(void)
{
    IDirectMusicGraph *graph;
    HRESULT hr;

    hr = CoCreateInstance(&CLSID_DirectMusicPerformance, NULL, CLSCTX_INPROC_SERVER, &IID_IDirectMusicPerformance, (void **)&perf);
    if (hr != S_OK)
    {
        printf("SKIP  no DirectMusic performance (%#lx)\n", hr);
        return 0;
    }
    hr = IDirectMusicPerformance_Init(perf, NULL, NULL, NULL);
    if (hr != S_OK)
    {
        printf("SKIP  performance Init failed (%#lx)\n", hr);
        return 0;
    }
    CoCreateInstance(&CLSID_DirectMusicGraph, NULL, CLSCTX_INPROC_SERVER, &IID_IDirectMusicGraph, (void **)&graph);
    IDirectMusicGraph_InsertTool(graph, &collector, NULL, 0, -1);
    IDirectMusicPerformance_SetGraph(perf, graph);
    IDirectMusicGraph_Release(graph);
    return 1;
}
static void teardown_perf(void)
{
    IDirectMusicPerformance_CloseDown(perf);
    IDirectMusicPerformance_Release(perf);
}

static IDirectMusicTrack *create_track(const CLSID *clsid)
{
    IDirectMusicTrack *track = NULL;
    HRESULT hr = CoCreateInstance(clsid, NULL, CLSCTX_INPROC_SERVER, &IID_IDirectMusicTrack, (void **)&track);
    if (hr != S_OK) printf("FAIL  CoCreateInstance(%08lx) %#lx\n", clsid->Data1, hr), failures++;
    return track;
}
static HRESULT load_track(IDirectMusicTrack *track)
{
    IPersistStream *ps;
    IStream *stream = make_stream();
    HRESULT hr;
    IDirectMusicTrack_QueryInterface(track, &IID_IPersistStream, (void **)&ps);
    hr = IPersistStream_Load(ps, stream);
    IPersistStream_Release(ps);
    IStream_Release(stream);
    return hr;
}
static HRESULT play(IDirectMusicTrack *track, MUSIC_TIME start, MUSIC_TIME stop, MUSIC_TIME offset)
{
    msg_count = 0;
    return IDirectMusicTrack_Play(track, NULL, start, stop, offset, DMUS_TRACKF_START, perf, NULL, 7);
}
static IDirectMusicTrack *clone(IDirectMusicTrack *track, MUSIC_TIME start, MUSIC_TIME stop)
{
    IDirectMusicTrack *result = NULL;
    HRESULT hr = IDirectMusicTrack_Clone(track, start, stop, &result);
    CHECKF(hr == S_OK && result && result != track, "Clone(%ld, %ld) is S_OK and a new track (%#lx)", start, stop, hr);
    return result;
}
#define EXPECT_COUNT(n, tag) CHECKF(msg_count == (n), "%s sends %d messages (got %d)", tag, (n), msg_count)
