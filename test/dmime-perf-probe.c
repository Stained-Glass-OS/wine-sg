/* dmime performance, segment, segment state and audio path methods (patches/sg/2952, 2953), run by
 * test/dmime-perf-gate.sh: IsPlaying, Invalidate, SetParam / GetParam / GetParamEx, the
 * time methods (GetQueueTime, AdjustTime, GetResolvedTime, TimeToRhythm, RhythmToTime), the music value
 * conversions for fixed notes, PlaySegmentEx start times and flags, StopEx, the port and PChannel
 * bookkeeping, the performance tool's Flush, the segment's track configuration / PChannels / audio path
 * configuration / notifications / SetParam, the segment state's seek, track configuration and
 * GetObjectInPath, and the audio path's GetObjectInPath, Activate, SetVolume and ConvertPChannel.
 * The parts which need audio are skipped (said) when the performance cannot start audio.
 *
 *   dmime-perf-probe.exe */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <dmusici.h>
#include <dmusicf.h>
#include <dmerror.h>
#include <dsound.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define ARRAY_SIZE_(a) (sizeof(a) / sizeof((a)[0]))

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}
#define CHECKF(ok, ...) do { char _b[300]; snprintf(_b, sizeof(_b), __VA_ARGS__); check(ok, _b); } while (0)

/* ---- RIFF building ---- */
static char riff[4096];
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
static HRESULT load(IUnknown *object)
{
    IPersistStream *ps;
    IStream *stream = make_stream();
    HRESULT hr;
    IUnknown_QueryInterface(object, &IID_IPersistStream, (void **)&ps);
    hr = IPersistStream_Load(ps, stream);
    IPersistStream_Release(ps);
    IStream_Release(stream);
    return hr;
}

/* ---- collecting tool ---- */
struct msg
{
    DWORD type;
    MUSIC_TIME mtTime;
    DWORD track_id;
    double tempo;
    BYTE note;
    DWORD sysex_len;
};
static struct msg msgs[256];
static int msg_count;
static CRITICAL_SECTION msg_lock;
static DWORD delivery = DMUS_PMSGF_TOOL_IMMEDIATE;

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
    *type = delivery;
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
    EnterCriticalSection(&msg_lock);
    if (msg_count < ARRAY_SIZE_(msgs))
    {
        m = msgs + msg_count++;
        memset(m, 0, sizeof(*m));
        m->type = pmsg->dwType;
        m->mtTime = pmsg->mtTime;
        m->track_id = pmsg->dwVirtualTrackID;
        if (pmsg->dwType == DMUS_PMSGT_TEMPO) m->tempo = ((DMUS_TEMPO_PMSG *)pmsg)->dblTempo;
        if (pmsg->dwType == DMUS_PMSGT_NOTE) m->note = ((DMUS_NOTE_PMSG *)pmsg)->bMidiValue;
        if (pmsg->dwType == DMUS_PMSGT_SYSEX) m->sysex_len = ((DMUS_SYSEX_PMSG *)pmsg)->dwLen;
    }
    LeaveCriticalSection(&msg_lock);
    return DMUS_S_FREE;
}
static HRESULT WINAPI tool_Flush(IDirectMusicTool *iface, IDirectMusicPerformance *perf, DMUS_PMSG *pmsg, REFERENCE_TIME time)
{
    return S_OK;
}
static IDirectMusicToolVtbl tool_vtbl = {tool_QueryInterface, tool_AddRef, tool_Release, tool_Init,
        tool_GetMsgDeliveryType, tool_GetMediaTypeArraySize, tool_GetMediaTypes, tool_ProcessPMsg, tool_Flush};
static IDirectMusicTool collector = {&tool_vtbl};

static int count_msgs(DWORD type, int note)
{
    int i, n = 0;
    EnterCriticalSection(&msg_lock);
    for (i = 0; i < msg_count; i++)
        if (msgs[i].type == type && (note < 0 || msgs[i].note == note)) n++;
    LeaveCriticalSection(&msg_lock);
    return n;
}
static void clear_msgs(void)
{
    EnterCriticalSection(&msg_lock);
    msg_count = 0;
    LeaveCriticalSection(&msg_lock);
}

/* ---- helpers ---- */
static IDirectMusicPerformance8 *perf;

static IDirectMusicPerformance8 *create_perf(BOOL audio, DWORD apath)
{
    IDirectMusicPerformance8 *p = NULL;
    IDirectMusicGraph *graph;
    HRESULT hr = CoCreateInstance(&CLSID_DirectMusicPerformance, NULL, CLSCTX_INPROC_SERVER, &IID_IDirectMusicPerformance8, (void **)&p);
    if (hr != S_OK) return NULL;
    if (audio) hr = IDirectMusicPerformance8_InitAudio(p, NULL, NULL, NULL, apath, 64, DMUS_AUDIOF_ALL, NULL);
    else hr = IDirectMusicPerformance8_Init(p, NULL, NULL, NULL);
    if (FAILED(hr))
    {
        printf("note  performance %s failed (%#lx)\n", audio ? "InitAudio" : "Init", hr);
        IDirectMusicPerformance8_Release(p);
        return NULL;
    }
    CoCreateInstance(&CLSID_DirectMusicGraph, NULL, CLSCTX_INPROC_SERVER, &IID_IDirectMusicGraph, (void **)&graph);
    IDirectMusicGraph_InsertTool(graph, &collector, NULL, 0, -1);
    IDirectMusicPerformance8_SetGraph(p, graph);
    IDirectMusicGraph_Release(graph);
    return p;
}

static IDirectMusicSegment8 *create_segment(MUSIC_TIME length)
{
    IDirectMusicSegment8 *seg = NULL;
    CoCreateInstance(&CLSID_DirectMusicSegment, NULL, CLSCTX_INPROC_SERVER, &IID_IDirectMusicSegment8, (void **)&seg);
    IDirectMusicSegment8_SetLength(seg, length);
    return seg;
}

static IDirectMusicTrack *create_track(const CLSID *clsid)
{
    IDirectMusicTrack *track = NULL;
    CoCreateInstance(clsid, NULL, CLSCTX_INPROC_SERVER, &IID_IDirectMusicTrack, (void **)&track);
    return track;
}

static HRESULT play(IDirectMusicPerformance8 *p, IDirectMusicSegment8 *seg, DWORD flags, INT64 start, IDirectMusicSegmentState **state, IUnknown *path)
{
    return IDirectMusicPerformance8_PlaySegmentEx(p, (IUnknown *)seg, NULL, NULL, flags | DMUS_SEGF_SECONDARY, start, state, NULL, path);
}

static void test_basic(void)
{
    IDirectMusicSegment8 *seg1 = create_segment(100000), *seg2 = create_segment(100000);
    IDirectMusicSegmentState *state1, *state2;
    IDirectMusicTool *tool;
    DMUS_PMSG pmsg = {0};
    HRESULT hr;

    hr = play(perf, seg1, 0, 0, &state1, NULL);
    CHECKF(hr == S_OK && state1, "PlaySegmentEx (%#lx)", hr);
    hr = IDirectMusicPerformance8_IsPlaying(perf, NULL, NULL);
    CHECKF(hr == S_OK, "IsPlaying(NULL, NULL) while a segment plays is S_OK (%#lx)", hr);
    hr = IDirectMusicPerformance8_IsPlaying(perf, (IDirectMusicSegment *)seg1, NULL);
    CHECKF(hr == S_OK, "IsPlaying(segment) is S_OK (%#lx)", hr);
    hr = IDirectMusicPerformance8_IsPlaying(perf, (IDirectMusicSegment *)seg1, state1);
    CHECKF(hr == S_OK, "IsPlaying(segment, state) is S_OK (%#lx)", hr);
    hr = IDirectMusicPerformance8_IsPlaying(perf, NULL, state1);
    CHECKF(hr == S_OK, "IsPlaying(NULL, state) is S_OK (%#lx)", hr);
    hr = IDirectMusicPerformance8_IsPlaying(perf, (IDirectMusicSegment *)seg2, NULL);
    CHECKF(hr == S_FALSE, "IsPlaying(another segment) is S_FALSE (%#lx)", hr);

    hr = play(perf, seg2, 0, 0, &state2, NULL);
    CHECKF(hr == S_OK, "second PlaySegmentEx (%#lx)", hr);
    hr = IDirectMusicPerformance8_IsPlaying(perf, (IDirectMusicSegment *)seg1, state2);
    CHECKF(hr == S_FALSE, "IsPlaying(segment, state of another segment) is S_FALSE (%#lx)", hr);

    /* StopEx takes states, segments or nothing */
    hr = IDirectMusicPerformance8_StopEx(perf, (IUnknown *)state1, 0, 0);
    CHECKF(hr == S_OK, "StopEx(state) (%#lx)", hr);
    hr = IDirectMusicPerformance8_IsPlaying(perf, NULL, state1);
    CHECKF(hr == S_FALSE, "the stopped state is not playing (%#lx)", hr);
    hr = IDirectMusicPerformance8_IsPlaying(perf, NULL, state2);
    CHECKF(hr == S_OK, "the other state keeps playing (%#lx)", hr);
    hr = IDirectMusicPerformance8_StopEx(perf, (IUnknown *)perf, 0, 0);
    CHECKF(hr == E_INVALIDARG, "StopEx(performance) is E_INVALIDARG (%#lx)", hr);
    hr = IDirectMusicPerformance8_StopEx(perf, (IUnknown *)seg2, 0, 0);
    CHECKF(hr == S_OK, "StopEx(segment) (%#lx)", hr);
    hr = IDirectMusicPerformance8_IsPlaying(perf, NULL, NULL);
    CHECKF(hr == S_FALSE, "nothing plays after the segment was stopped (%#lx)", hr);

    IDirectMusicSegmentState_Release(state1);
    IDirectMusicSegmentState_Release(state2);
    hr = play(perf, seg1, 0, 0, &state1, NULL);
    hr = IDirectMusicPerformance8_StopEx(perf, NULL, 0, 0);
    CHECKF(hr == S_OK, "StopEx(NULL) (%#lx)", hr);
    hr = IDirectMusicPerformance8_IsPlaying(perf, NULL, NULL);
    CHECKF(hr == S_FALSE, "nothing plays after StopEx(NULL) (%#lx)", hr);
    IDirectMusicSegmentState_Release(state1);

    hr = IDirectMusicPerformance8_PlaySegmentEx(perf, NULL, NULL, NULL, 0, 0, NULL, NULL, NULL);
    CHECKF(hr == E_POINTER, "PlaySegmentEx without source is E_POINTER (%#lx)", hr);
    hr = IDirectMusicPerformance8_PlaySegmentEx(perf, (IUnknown *)perf, NULL, NULL, 0, 0, NULL, NULL, NULL);
    CHECKF(hr == E_NOINTERFACE, "PlaySegmentEx of a performance is E_NOINTERFACE (%#lx)", hr);

    IDirectMusicPerformance8_QueryInterface(perf, &IID_IDirectMusicTool, (void **)&tool);
    hr = IDirectMusicTool_Flush(tool, (IDirectMusicPerformance *)perf, &pmsg, 0);
    CHECKF(hr == S_OK, "the performance tool's Flush is S_OK (%#lx)", hr);
    IDirectMusicTool_Release(tool);

    IDirectMusicSegment8_Release(seg1);
    IDirectMusicSegment8_Release(seg2);
}

/* the messages the tracks send reach the performance as the last tool, which must accept them */
static void test_final_tool(void)
{
    static const DWORD types[] = {DMUS_PMSGT_TEMPO, DMUS_PMSGT_TIMESIG, DMUS_PMSGT_LYRIC, DMUS_PMSGT_DIRTY, DMUS_PMSGT_SYSEX};
    unsigned int i;
    HRESULT hr;

    for (i = 0; i < ARRAY_SIZE_(types); i++)
    {
        DMUS_SYSEX_PMSG *msg;

        hr = IDirectMusicPerformance8_AllocPMsg(perf, sizeof(*msg) + 8, (DMUS_PMSG **)&msg);
        msg->dwType = types[i];
        msg->dwFlags = DMUS_PMSGF_REFTIME | DMUS_PMSGF_TOOL_IMMEDIATE;
        msg->rtTime = -1;
        msg->dwLen = 3;
        memcpy(msg->abData, "\xf0\x01\xf7", 3);
        hr = IDirectMusicPerformance8_SendPMsg(perf, (DMUS_PMSG *)msg);
        CHECKF(hr == S_OK, "SendPMsg of message type %#lx to the performance tool (%#lx)", types[i], hr);
    }
}

static void test_ports(void)
{
    IDirectMusicPort *port, *port2;
    DWORD group, channel;
    HRESULT hr;

    hr = IDirectMusicPerformance8_AddPort(perf, NULL);
    CHECKF(hr == S_OK, "AddPort of the default port (%#lx)", hr);
    hr = IDirectMusicPerformance8_PChannelInfo(perf, 0, &port, &group, &channel);
    CHECKF(hr == S_OK && port && group == 1 && channel == 0, "PChannelInfo(0) after AddPort (%#lx)", hr);
    if (hr != S_OK) return;

    hr = IDirectMusicPerformance8_RemovePort(perf, NULL);
    CHECKF(hr == E_POINTER, "RemovePort(NULL) is E_POINTER (%#lx)", hr);
    hr = IDirectMusicPerformance8_RemovePort(perf, port);
    CHECKF(hr == S_OK, "RemovePort of the added port (%#lx)", hr);
    port2 = (IDirectMusicPort *)0xdeadbeef;
    hr = IDirectMusicPerformance8_PChannelInfo(perf, 0, &port2, NULL, NULL);
    CHECKF(hr == E_INVALIDARG && port2 == (IDirectMusicPort *)0xdeadbeef, "the channels of the removed port are gone (%#lx)", hr);
    hr = IDirectMusicPerformance8_RemovePort(perf, port);
    CHECKF(hr == S_FALSE, "RemovePort of a port which is not added is S_FALSE (%#lx)", hr);

    hr = IDirectMusicPerformance8_AddPort(perf, port);
    CHECKF(hr == S_OK, "AddPort of an existing port (%#lx)", hr);
    hr = IDirectMusicPerformance8_AssignPChannelBlock(perf, 0, port, 2);
    CHECKF(hr == S_OK, "AssignPChannelBlock after AddPort (%#lx)", hr);
    hr = IDirectMusicPerformance8_PChannelInfo(perf, 3, &port2, &group, &channel);
    CHECKF(hr == S_OK && port2 == port && group == 2 && channel == 3, "PChannelInfo(3) (%#lx, %lu, %lu)", hr, group, channel);
    if (hr == S_OK) IDirectMusicPort_Release(port2);

    hr = IDirectMusicPerformance8_AssignPChannelBlock(perf, MAXDWORD / 16, port, 0);
    CHECKF(hr == E_INVALIDARG, "AssignPChannelBlock of the last block is E_INVALIDARG (%#lx)", hr);
    hr = IDirectMusicPerformance8_AssignPChannelBlock(perf, MAXDWORD / 16 - 1, port, 0);
    CHECKF(hr == E_INVALIDARG, "AssignPChannelBlock of the second to last block is E_INVALIDARG (%#lx)", hr);
    hr = IDirectMusicPerformance8_AssignPChannelBlock(perf, MAXDWORD / 16 - 2, port, 0);
    CHECKF(hr == S_OK, "AssignPChannelBlock of the third to last block (%#lx)", hr);
    hr = IDirectMusicPerformance8_AssignPChannel(perf, MAXDWORD - 3, port, 0, 4);
    CHECKF(hr == S_OK, "AssignPChannel in the last block (%#lx)", hr);
    port2 = (IDirectMusicPort *)0xdeadbeef;
    hr = IDirectMusicPerformance8_PChannelInfo(perf, MAXDWORD - 3, &port2, NULL, NULL);
    CHECKF(hr == E_INVALIDARG && port2 == (IDirectMusicPort *)0xdeadbeef, "a channel of the last block cannot be read back (%#lx)", hr);
    hr = IDirectMusicPerformance8_AssignPChannel(perf, MAXDWORD - 17, port, 1, 9);
    hr = IDirectMusicPerformance8_PChannelInfo(perf, MAXDWORD - 17, &port2, &group, &channel);
    CHECKF(hr == S_OK && group == 1 && channel == 9, "a channel of the second to last block can (%#lx)", hr);
    if (hr == S_OK) IDirectMusicPort_Release(port2);
    hr = IDirectMusicPerformance8_AssignPChannel(perf, 5, NULL, 0, 0);
    CHECKF(hr == E_POINTER, "AssignPChannel without port is E_POINTER (%#lx)", hr);

    IDirectMusicPort_Release(port);
}

static void test_time(void)
{
    REFERENCE_TIME t0, t1, t2, latency, queue, resolved, now;
    DMUS_TIMESIGNATURE sig = {0, 4, 4, 2};
    MUSIC_TIME mt, time;
    WORD measure;
    BYTE beat, grid, midi;
    WORD music;
    short offset;
    HRESULT hr;

    IDirectMusicPerformance8_GetTime(perf, &t0, NULL);
    hr = IDirectMusicPerformance8_AdjustTime(perf, 100000000);
    CHECKF(hr == S_OK, "AdjustTime (%#lx)", hr);
    IDirectMusicPerformance8_GetTime(perf, &t1, NULL);
    CHECKF(t1 - t0 >= 99000000 && t1 - t0 < 110000000, "AdjustTime moved the performance time by 10 seconds (%lld)", (long long)(t1 - t0));
    hr = IDirectMusicPerformance8_AdjustTime(perf, -100000000);
    IDirectMusicPerformance8_GetTime(perf, &t2, NULL);
    CHECKF(t2 - t0 >= 0 && t2 - t0 < 20000000, "a negative AdjustTime moves it back (%lld)", (long long)(t2 - t0));

    IDirectMusicPerformance8_SetBumperLength(perf, 100);
    hr = IDirectMusicPerformance8_GetQueueTime(perf, NULL);
    CHECKF(hr == E_POINTER, "GetQueueTime without pointer is E_POINTER (%#lx)", hr);
    IDirectMusicPerformance8_GetQueueTime(perf, &queue);
    IDirectMusicPerformance8_GetLatencyTime(perf, &latency);
    CHECKF(llabs((queue - latency) - 1000000) < 100000, "the queue time is a bumper length after the latency time (%lld)", (long long)(queue - latency));
    IDirectMusicPerformance8_SetBumperLength(perf, 50);

    hr = IDirectMusicPerformance8_GetResolvedTime(perf, 0, NULL, 0);
    CHECKF(hr == E_POINTER, "GetResolvedTime without pointer is E_POINTER (%#lx)", hr);
    IDirectMusicPerformance8_GetTime(perf, &now, NULL);
    hr = IDirectMusicPerformance8_GetResolvedTime(perf, now + 123456789, &resolved, 0);
    CHECKF(hr == S_OK && resolved == now + 123456789, "GetResolvedTime without flags keeps the time (%#lx)", hr);
    hr = IDirectMusicPerformance8_GetResolvedTime(perf, 0, &resolved, DMUS_TIME_RESOLVE_AFTERPREPARETIME);
    IDirectMusicPerformance8_GetTime(perf, &now, NULL);
    CHECKF(hr == S_OK && resolved >= now - 1000000 + 10000000 - 100000 && resolved < now + 10000000 + 5000000, "GetResolvedTime after the prepare time (%lld)", (long long)(resolved - now));
    hr = IDirectMusicPerformance8_GetResolvedTime(perf, 0, &resolved, DMUS_TIME_RESOLVE_AFTERLATENCYTIME);
    IDirectMusicPerformance8_GetLatencyTime(perf, &latency);
    CHECKF(hr == S_OK && resolved >= latency - 1000000 && resolved <= latency + 1000000, "GetResolvedTime after the latency time (%lld)", (long long)(resolved - latency));
    hr = IDirectMusicPerformance8_GetResolvedTime(perf, 0, &resolved, DMUS_TIME_RESOLVE_AFTERQUEUETIME);
    IDirectMusicPerformance8_GetQueueTime(perf, &queue);
    CHECKF(hr == S_OK && resolved >= queue - 1000000 && resolved <= queue + 1000000, "GetResolvedTime after the queue time (%lld)", (long long)(resolved - queue));

    {
        static const struct { DWORD flag; MUSIC_TIME unit; const char *name; } units[] = {
            {DMUS_TIME_RESOLVE_GRID, 384, "grid"}, {DMUS_TIME_RESOLVE_BEAT, 768, "beat"}, {DMUS_TIME_RESOLVE_MEASURE, 3072, "measure"}};
        unsigned int i;
        for (i = 0; i < ARRAY_SIZE_(units); i++)
        {
            IDirectMusicPerformance8_GetTime(perf, &now, NULL);
            hr = IDirectMusicPerformance8_GetResolvedTime(perf, 0, &resolved, units[i].flag);
            IDirectMusicPerformance8_ReferenceToMusicTime(perf, resolved, &mt);
            IDirectMusicPerformance8_ReferenceToMusicTime(perf, now, &time);
            CHECKF(hr == S_OK && resolved >= now - 20000 && (mt + 1) % units[i].unit <= 2 && mt - time < units[i].unit + 2,
                    "GetResolvedTime to the next %s (%#lx, music time %ld, now %ld)", units[i].name, hr, mt, time);
        }
    }

    hr = IDirectMusicPerformance8_TimeToRhythm(perf, 3072 + 768 * 2 + 384 + 5, &sig, &measure, &beat, &grid, &offset);
    CHECKF(hr == S_OK && measure == 1 && beat == 2 && grid == 1 && offset == 5, "TimeToRhythm in 4/4 (%#lx, %u, %u, %u, %d)", hr, measure, beat, grid, offset);
    hr = IDirectMusicPerformance8_RhythmToTime(perf, 1, 2, 1, 5, &sig, &time);
    CHECKF(hr == S_OK && time == 3072 + 768 * 2 + 384 + 5, "RhythmToTime in 4/4 (%#lx, %ld)", hr, time);
    sig.mtTime = 1000; sig.bBeatsPerMeasure = 3; sig.bBeat = 8; sig.wGridsPerBeat = 2;
    hr = IDirectMusicPerformance8_TimeToRhythm(perf, 1000 + 1152 * 2 + 384 + 192 + 7, &sig, &measure, &beat, &grid, &offset);
    CHECKF(hr == S_OK && measure == 2 && beat == 1 && grid == 1 && offset == 7, "TimeToRhythm in 3/8 starting at 1000 (%#lx, %u, %u, %u, %d)", hr, measure, beat, grid, offset);
    hr = IDirectMusicPerformance8_RhythmToTime(perf, 2, 1, 1, 7, &sig, &time);
    CHECKF(hr == S_OK && time == 1000 + 1152 * 2 + 384 + 192 + 7, "RhythmToTime in 3/8 starting at 1000 (%#lx, %ld)", hr, time);
    hr = IDirectMusicPerformance8_TimeToRhythm(perf, 3072 + 768 * 2, NULL, &measure, &beat, &grid, &offset);
    CHECKF(hr == S_OK && measure == 1 && beat == 2 && grid == 0 && offset == 0, "TimeToRhythm without signature uses 4/4 (%#lx, %u, %u)", hr, measure, beat);
    hr = IDirectMusicPerformance8_TimeToRhythm(perf, 0, &sig, NULL, &beat, &grid, &offset);
    CHECKF(hr == E_POINTER, "TimeToRhythm without pointer is E_POINTER (%#lx)", hr);
    hr = IDirectMusicPerformance8_RhythmToTime(perf, 0, 0, 0, 0, NULL, &time);
    CHECKF(hr == E_POINTER, "RhythmToTime without signature is E_POINTER (%#lx)", hr);

    music = 0xdead; midi = 0xdd;
    hr = IDirectMusicPerformance8_MIDIToMusic(perf, 60, NULL, DMUS_PLAYMODE_FIXED, 0, &music);
    CHECKF(hr == S_OK && music == 60, "MIDIToMusic of a fixed note (%#lx, %u)", hr, music);
    hr = IDirectMusicPerformance8_MusicToMIDI(perf, 61, NULL, DMUS_PLAYMODE_FIXED, 0, &midi);
    CHECKF(hr == S_OK && midi == 61, "MusicToMIDI of a fixed note (%#lx, %u)", hr, midi);
    hr = IDirectMusicPerformance8_MusicToMIDI(perf, 300, NULL, DMUS_PLAYMODE_FIXED, 0, &midi);
    CHECKF(hr == S_OK && midi == 127, "MusicToMIDI limits a fixed note to 127 (%#lx, %u)", hr, midi);
    hr = IDirectMusicPerformance8_MIDIToMusic(perf, 60, NULL, DMUS_PLAYMODE_FIXED, 0, NULL);
    CHECKF(hr == E_POINTER, "MIDIToMusic without result is E_POINTER (%#lx)", hr);
}

static void test_params(void)
{
    static const DMUS_IO_TEMPO_ITEM items[] = {{0, 100}};
    IDirectMusicSegment8 *seg = create_segment(100000);
    IDirectMusicTrack *track = create_track(&CLSID_DirectMusicTempoTrack);
    IDirectMusicSegmentState *state;
    DMUS_TEMPO_PARAM param;
    MUSIC_TIME next, start;
    DWORD track_id = 0;
    HRESULT hr;
    int i, j;

    array_chunk("tetr", items, sizeof(items[0]), 1);
    load((IUnknown *)track);
    IDirectMusicSegment8_InsertTrack(seg, track, 1);
    IDirectMusicTrack_Release(track);

    hr = IDirectMusicPerformance8_GetParam(perf, &GUID_TempoParam, -1, DMUS_SEG_ALLTRACKS, 0, &next, &param);
    CHECKF(hr == DMUS_E_NOT_FOUND, "GetParam without any playing segment is DMUS_E_NOT_FOUND (%#lx)", hr);
    param.dblTempo = 150;
    param.mtTime = 0;
    hr = IDirectMusicPerformance8_SetParam(perf, &GUID_TempoParam, -1, DMUS_SEG_ALLTRACKS, 1000, &param);
    CHECKF(hr == DMUS_E_NOT_FOUND, "SetParam without any playing segment is DMUS_E_NOT_FOUND (%#lx)", hr);

    clear_msgs();
    hr = IDirectMusicPerformance8_PlaySegmentEx(perf, (IUnknown *)seg, NULL, NULL, 0, 5000, &state, NULL, NULL);
    CHECKF(hr == S_OK, "PlaySegmentEx of the primary segment (%#lx)", hr);
    IDirectMusicSegmentState_GetStartTime(state, &start);

    hr = IDirectMusicPerformance8_SetParam(perf, NULL, -1, DMUS_SEG_ALLTRACKS, 1000, &param);
    CHECKF(hr == E_POINTER, "SetParam without type is E_POINTER (%#lx)", hr);
    hr = IDirectMusicPerformance8_SetParam(perf, &GUID_TempoParam, 0x2, DMUS_SEG_ALLTRACKS, 1000, &param);
    CHECKF(hr == DMUS_E_TRACK_NOT_FOUND, "SetParam for a group without tracks is DMUS_E_TRACK_NOT_FOUND (%#lx)", hr);
    hr = IDirectMusicPerformance8_SetParam(perf, &GUID_TempoParam, -1, DMUS_SEG_ALLTRACKS, 1000, &param);
    CHECKF(hr == S_OK, "SetParam reaches the primary segment's tracks (%#lx)", hr);
    memset(&param, 0, sizeof(param));
    hr = IDirectMusicPerformance8_GetParam(perf, &GUID_TempoParam, -1, DMUS_SEG_ALLTRACKS, 1000, &next, &param);
    CHECKF(hr == S_OK && param.dblTempo == 150, "GetParam returns what SetParam stored (%#lx, %g)", hr, param.dblTempo);

    for (i = 0; i < 40 && !track_id; i++)
    {
        EnterCriticalSection(&msg_lock);
        for (j = 0; j < msg_count; j++)
            if (msgs[j].type == DMUS_PMSGT_TEMPO) track_id = msgs[j].track_id;
        LeaveCriticalSection(&msg_lock);
        if (!track_id) Sleep(25);
    }
    CHECKF(track_id != 0, "the tempo track sent its tempo with a virtual track id (%lu)", track_id);

    memset(&param, 0, sizeof(param));
    next = 0xdeadbeef;
    hr = IDirectMusicPerformance8_GetParamEx(perf, &GUID_TempoParam, track_id, -1, DMUS_SEG_ALLTRACKS, start + 1000, &next, &param);
    CHECKF(hr == S_OK && param.dblTempo == 150, "GetParamEx finds the track by its virtual track id (%#lx, %g)", hr, param.dblTempo);
    memset(&param, 0, sizeof(param));
    hr = IDirectMusicPerformance8_GetParamEx(perf, &GUID_TempoParam, track_id, -1, DMUS_SEG_ALLTRACKS, start + 500, &next, &param);
    CHECKF(hr == S_OK && param.dblTempo == 100, "GetParamEx takes the time relative to the segment's start (%#lx, %g)", hr, param.dblTempo);
    hr = IDirectMusicPerformance8_GetParamEx(perf, &GUID_TempoParam, 99999, -1, DMUS_SEG_ALLTRACKS, start + 10, &next, &param);
    CHECKF(hr == DMUS_E_TRACK_NOT_FOUND, "GetParamEx of an unknown track id is DMUS_E_TRACK_NOT_FOUND (%#lx)", hr);
    memset(&param, 0, sizeof(param));
    hr = IDirectMusicPerformance8_GetParamEx(perf, &GUID_TempoParam, 0, -1, DMUS_SEG_ALLTRACKS, 1000, &next, &param);
    CHECKF(hr == S_OK && param.dblTempo == 150, "GetParamEx without track id searches the segments (%#lx, %g)", hr, param.dblTempo);
    hr = IDirectMusicPerformance8_GetParamEx(perf, NULL, track_id, -1, DMUS_SEG_ALLTRACKS, 0, &next, &param);
    CHECKF(hr == E_POINTER, "GetParamEx without type is E_POINTER (%#lx)", hr);

    IDirectMusicPerformance8_Stop(perf, NULL, NULL, 0, 0);
    IDirectMusicSegmentState_Release(state);
    IDirectMusicSegment8_Release(seg);
}

static void test_play_start(void)
{
    IDirectMusicSegment8 *seg = create_segment(100000), *primary_seg = create_segment(100000);
    IDirectMusicSegmentState *state, *primary_state;
    REFERENCE_TIME now, rt;
    MUSIC_TIME start, expect, current;
    HRESULT hr;

    hr = play(perf, seg, 0, 5000, &state, NULL);
    IDirectMusicSegmentState_GetStartTime(state, &start);
    CHECKF(hr == S_OK && start == 5000, "PlaySegmentEx keeps a music start time (%#lx, %ld)", hr, start);
    IDirectMusicSegmentState_Release(state);

    IDirectMusicPerformance8_GetTime(perf, &now, NULL);
    rt = now + 30000000;
    hr = play(perf, seg, DMUS_SEGF_REFTIME, rt, &state, NULL);
    IDirectMusicPerformance8_ReferenceToMusicTime(perf, rt, &expect);
    IDirectMusicSegmentState_GetStartTime(state, &start);
    CHECKF(hr == S_OK && abs(start - expect) <= 2, "PlaySegmentEx with DMUS_SEGF_REFTIME starts at the reference time (%#lx, %ld, expected %ld)", hr, start, expect);
    IDirectMusicSegmentState_Release(state);

    /* grid, beat and measure boundaries are those of the primary segment */
    hr = IDirectMusicPerformance8_PlaySegmentEx(perf, (IUnknown *)primary_seg, NULL, NULL, 0, 0, &primary_state, NULL, NULL);
    CHECKF(hr == S_OK, "PlaySegmentEx of a primary segment (%#lx)", hr);
    IDirectMusicPerformance8_GetTime(perf, NULL, &current);
    hr = play(perf, seg, DMUS_SEGF_BEAT, 0, &state, NULL);
    IDirectMusicSegmentState_GetStartTime(state, &start);
    CHECKF(hr == S_OK && start >= current && (start + 1) % 768 <= 2, "PlaySegmentEx with DMUS_SEGF_BEAT starts on a beat (%#lx, %ld, now %ld)", hr, start, current);
    IDirectMusicSegmentState_Release(state);

    hr = play(perf, seg, DMUS_SEGF_MEASURE, 0, &state, NULL);
    IDirectMusicSegmentState_GetStartTime(state, &start);
    CHECKF(hr == S_OK && start >= current && (start + 1) % 3072 <= 2, "PlaySegmentEx with DMUS_SEGF_MEASURE starts on a measure (%#lx, %ld)", hr, start);
    IDirectMusicSegmentState_Release(state);

    IDirectMusicSegment8_SetDefaultResolution(seg, DMUS_SEGF_BEAT);
    hr = play(perf, seg, DMUS_SEGF_DEFAULT, 0, &state, NULL);
    IDirectMusicSegmentState_GetStartTime(state, &start);
    CHECKF(hr == S_OK && start >= current && (start + 1) % 768 <= 2, "DMUS_SEGF_DEFAULT uses the segment's default resolution (%#lx, %ld)", hr, start);
    IDirectMusicSegmentState_Release(state);

    IDirectMusicPerformance8_GetTime(perf, &now, NULL);
    hr = play(perf, seg, DMUS_SEGF_AFTERPREPARETIME, 0, &state, NULL);
    IDirectMusicPerformance8_ReferenceToMusicTime(perf, now + 10000000, &expect);
    IDirectMusicSegmentState_GetStartTime(state, &start);
    CHECKF(hr == S_OK && start >= expect - 2, "DMUS_SEGF_AFTERPREPARETIME starts after the prepare time (%#lx, %ld, expected %ld)", hr, start, expect);
    IDirectMusicSegmentState_Release(state);

    IDirectMusicPerformance8_Stop(perf, NULL, NULL, 0, 0);
    IDirectMusicSegmentState_Release(primary_state);
    IDirectMusicSegment8_Release(primary_seg);
    IDirectMusicSegment8_Release(seg);
}

static void test_invalidate(void)
{
    DMUS_IO_SEQ_ITEM items[] = {{1500, 10, 2, 0, 0x90, 60, 100}, {1800, 10, 2, 0, 0x90, 61, 100}, {2100, 10, 2, 0, 0x90, 62, 100}};
    IDirectMusicSegment8 *seg = create_segment(4000);
    IDirectMusicTrack *track = create_track(&CLSID_DirectMusicSeqTrack);
    IDirectMusicSegmentState *state;
    IDirectMusicGraph *graph;
    MUSIC_TIME start;
    HRESULT hr;
    int a;

    a = begin("seqt", NULL);
    array_chunk("evtl", items, sizeof(items[0]), 3);
    end(a);
    load((IUnknown *)track);
    IDirectMusicSegment8_InsertTrack(seg, track, 1);
    IDirectMusicTrack_Release(track);

    /* notes are held back until their time, the one at 1500 is dropped and sent again by Invalidate(1000) */
    IDirectMusicPerformance8_GetGraph(perf, &graph);
    IDirectMusicGraph_RemoveTool(graph, &collector);
    delivery = DMUS_PMSGF_TOOL_ATTIME;
    IDirectMusicGraph_InsertTool(graph, &collector, NULL, 0, -1);
    clear_msgs();
    hr = play(perf, seg, 0, 0, &state, NULL);
    CHECKF(hr == S_OK, "PlaySegmentEx of the sequence (%#lx)", hr);
    IDirectMusicSegmentState_GetStartTime(state, &start);
    hr = IDirectMusicPerformance8_Invalidate(perf, start + 1000, 0);
    CHECKF(hr == S_OK, "Invalidate (%#lx)", hr);
    Sleep(2800);
    IDirectMusicGraph_RemoveTool(graph, &collector);
    delivery = DMUS_PMSGF_TOOL_IMMEDIATE;
    IDirectMusicGraph_InsertTool(graph, &collector, NULL, 0, -1);
    IDirectMusicGraph_Release(graph);
    CHECKF(count_msgs(DMUS_PMSGT_NOTE, 60) == 1, "the note after the invalidated time is delivered once, not twice (%d)", count_msgs(DMUS_PMSGT_NOTE, 60));
    CHECKF(count_msgs(DMUS_PMSGT_NOTE, 61) == 1, "the second note is delivered once (%d)", count_msgs(DMUS_PMSGT_NOTE, 61));
    CHECKF(count_msgs(DMUS_PMSGT_NOTE, 62) == 1, "the third note is delivered once (%d)", count_msgs(DMUS_PMSGT_NOTE, 62));

    IDirectMusicPerformance8_Stop(perf, NULL, NULL, 0, 0);
    IDirectMusicSegmentState_Release(state);
    IDirectMusicSegment8_Release(seg);
}

static void test_segment_config(void)
{
    DMUS_IO_SYSEX_ITEM item = {2000, 0, 3};
    static const BYTE data[] = {0xf0, 0x01, 0xf7};
    IDirectMusicSegment8 *seg = create_segment(6000), *tmp;
    IDirectMusicTrack *track = create_track(&CLSID_DirectMusicSysExTrack);
    IDirectMusicSegmentState *state, *dummy;
    IDirectMusicSegmentState8 *state8;
    DWORD channels[2] = {3, 4};
    IUnknown *unk;
    MUSIC_TIME length, seek;
    HRESULT hr;
    int a;

    a = begin("syex", NULL);
    put(&item, sizeof(item)); put(data, sizeof(data));
    end(a);
    load((IUnknown *)track);
    IDirectMusicSegment8_InsertTrack(seg, track, 1);

    /* a new segment is one tick long, a dummy state from InitPlay has no tracks */
    CoCreateInstance(&CLSID_DirectMusicSegment, NULL, CLSCTX_INPROC_SERVER, &IID_IDirectMusicSegment8, (void **)&tmp);
    IDirectMusicSegment8_GetLength(tmp, &length);
    CHECKF(length == 1, "a new segment is 1 tick long (%ld)", length);
    hr = IDirectMusicSegment8_InitPlay(tmp, &dummy, (IDirectMusicPerformance *)perf, 0);
    CHECKF(hr == S_OK && dummy, "InitPlay (%#lx)", hr);
    seek = 0xdeadbeef;
    hr = IDirectMusicSegmentState_GetSeek(dummy, &seek);
    CHECKF(hr == S_OK && seek == 0, "the seek of an InitPlay state is 0 (%#lx, %ld)", hr, seek);
    hr = IDirectMusicSegmentState_GetSeek(dummy, NULL);
    CHECKF(hr == E_POINTER, "GetSeek without pointer is E_POINTER (%#lx)", hr);
    IDirectMusicSegmentState_Release(dummy);
    IDirectMusicSegment8_Release(tmp);

    hr = IDirectMusicSegment8_SetPChannelsUsed(seg, 1, NULL);
    CHECKF(hr == E_POINTER, "SetPChannelsUsed without array is E_POINTER (%#lx)", hr);
    hr = IDirectMusicSegment8_SetPChannelsUsed(seg, 2, channels);
    CHECKF(hr == S_OK, "SetPChannelsUsed (%#lx)", hr);
    hr = IDirectMusicSegment8_SetPChannelsUsed(seg, 0, NULL);
    CHECKF(hr == S_OK, "SetPChannelsUsed with no channels (%#lx)", hr);

    unk = (IUnknown *)0xdeadbeef;
    hr = IDirectMusicSegment8_GetAudioPathConfig(seg, &unk);
    CHECKF(hr == DMUS_E_NO_AUDIOPATH_CONFIG && !unk, "GetAudioPathConfig without configuration is DMUS_E_NO_AUDIOPATH_CONFIG (%#lx)", hr);
    hr = IDirectMusicSegment8_GetAudioPathConfig(seg, NULL);
    CHECKF(hr == E_POINTER, "GetAudioPathConfig without pointer is E_POINTER (%#lx)", hr);

    hr = IDirectMusicSegment8_AddNotificationType(seg, &GUID_NOTIFICATION_SEGMENT);
    CHECKF(hr == S_OK, "AddNotificationType (%#lx)", hr);
    hr = IDirectMusicSegment8_RemoveNotificationType(seg, &GUID_NOTIFICATION_SEGMENT);
    CHECKF(hr == S_OK, "RemoveNotificationType (%#lx)", hr);
    hr = IDirectMusicSegment8_AddNotificationType(seg, NULL);
    CHECKF(hr == E_POINTER, "AddNotificationType without type is E_POINTER (%#lx)", hr);

    /* SetParam counts the tracks which support the parameter, like GetParam */
    {
        IDirectMusicSegment8 *two = create_segment(100);
        IDirectMusicTrack *trigger;
        char buf[16] = {0};
        int k;

        for (k = 0; k < 2; k++)
        {
            trigger = create_track(&CLSID_DirectMusicSegmentTriggerTrack);
            IDirectMusicSegment8_InsertTrack(two, trigger, 1);
            IDirectMusicTrack_Release(trigger);
        }
        hr = IDirectMusicSegment8_SetParam(two, NULL, 1, 0, 0, buf);
        CHECKF(hr == E_POINTER, "SetParam without type is E_POINTER (%#lx)", hr);
        hr = IDirectMusicSegment8_SetParam(two, &GUID_Valid_Start_Time, 1, 1, 0, buf);
        CHECKF(hr == S_OK, "SetParam of the second supporting track (%#lx)", hr);
        hr = IDirectMusicSegment8_SetParam(two, &GUID_Valid_Start_Time, 1, 2, 0, buf);
        CHECKF(hr == DMUS_E_TRACK_NOT_FOUND, "SetParam of a third supporting track is DMUS_E_TRACK_NOT_FOUND (%#lx)", hr);
        hr = IDirectMusicSegment8_SetParam(two, &GUID_Valid_Start_Time, 2, DMUS_SEG_ALLTRACKS, 0, buf);
        CHECKF(hr == DMUS_E_TRACK_NOT_FOUND, "SetParam for a group without tracks is DMUS_E_TRACK_NOT_FOUND (%#lx)", hr);
        IDirectMusicSegment8_Release(two);
    }

    hr = IDirectMusicSegment8_SetTrackConfig(seg, &CLSID_DirectMusicLyricsTrack, 1, 0, 0, DMUS_TRACKCONFIG_PLAY_ENABLED);
    CHECKF(hr == DMUS_E_TRACK_NOT_FOUND, "SetTrackConfig for a class without tracks is DMUS_E_TRACK_NOT_FOUND (%#lx)", hr);
    hr = IDirectMusicSegment8_SetTrackConfig(seg, &CLSID_DirectMusicSysExTrack, 2, 0, 0, DMUS_TRACKCONFIG_PLAY_ENABLED);
    CHECKF(hr == DMUS_E_TRACK_NOT_FOUND, "SetTrackConfig for a group without tracks is DMUS_E_TRACK_NOT_FOUND (%#lx)", hr);

    /* the sysex is sent in a later chunk of the segment, a state which switched the track off never sends it */
    clear_msgs();
    hr = play(perf, seg, 0, 0, &state, NULL);
    CHECKF(hr == S_OK, "PlaySegmentEx of the sysex segment (%#lx)", hr);
    IDirectMusicSegmentState_GetSeek(state, &seek);
    CHECKF(seek > 1000 && seek < 2000, "the seek of a playing state is the position reached (%ld)", seek);
    IDirectMusicSegmentState_QueryInterface(state, &IID_IDirectMusicSegmentState8, (void **)&state8);
    hr = IDirectMusicSegmentState8_SetTrackConfig(state8, &CLSID_DirectMusicSysExTrack, 1, DMUS_SEG_ALLTRACKS, 0, DMUS_TRACKCONFIG_PLAY_ENABLED);
    CHECKF(hr == S_OK, "the segment state's SetTrackConfig (%#lx)", hr);
    hr = IDirectMusicSegmentState8_SetTrackConfig(state8, &CLSID_DirectMusicSysExTrack, 1, 3, 0, DMUS_TRACKCONFIG_PLAY_ENABLED);
    CHECKF(hr == DMUS_E_TRACK_NOT_FOUND, "the segment state's SetTrackConfig for a missing index is DMUS_E_TRACK_NOT_FOUND (%#lx)", hr);
    IDirectMusicSegmentState8_Release(state8);
    Sleep(2500);
    CHECKF(count_msgs(DMUS_PMSGT_SYSEX, -1) == 0, "a disabled track is not played (%d messages)", count_msgs(DMUS_PMSGT_SYSEX, -1));
    IDirectMusicPerformance8_Stop(perf, NULL, NULL, 0, 0);
    IDirectMusicSegmentState_Release(state);

    clear_msgs();
    hr = play(perf, seg, 0, 0, &state, NULL);
    Sleep(2500);
    CHECKF(count_msgs(DMUS_PMSGT_SYSEX, -1) == 1, "an enabled track is played (%d messages)", count_msgs(DMUS_PMSGT_SYSEX, -1));
    IDirectMusicPerformance8_Stop(perf, NULL, NULL, 0, 0);
    IDirectMusicSegmentState_Release(state);

    hr = IDirectMusicSegment8_SetTrackConfig(seg, &CLSID_DirectMusicSysExTrack, 1, DMUS_SEG_ALLTRACKS, 0, DMUS_TRACKCONFIG_PLAY_ENABLED);
    CHECKF(hr == S_OK, "the segment's SetTrackConfig (%#lx)", hr);
    clear_msgs();
    hr = play(perf, seg, 0, 0, &state, NULL);
    Sleep(2500);
    CHECKF(count_msgs(DMUS_PMSGT_SYSEX, -1) == 0, "a track the segment switched off is not played by new states (%d messages)", count_msgs(DMUS_PMSGT_SYSEX, -1));
    IDirectMusicPerformance8_Stop(perf, NULL, NULL, 0, 0);
    IDirectMusicSegmentState_Release(state);

    IDirectMusicTrack_Release(track);
    IDirectMusicSegment8_Release(seg);
}

static void test_audiopath_config(void)
{
    IDirectMusicSegment8 *seg = create_segment(100);
    IUnknown *config = NULL;
    GUID guid = GUID_NULL;
    HRESULT hr;
    int a, b;

    /* a segment form with an embedded audio path configuration */
    a = begin("RIFF", "DMSG");
    {
        DMUS_IO_SEGMENT_HEADER head = {.mtLength = 100};
        chunk("segh", &head, sizeof(head));
    }
    chunk("guid", &guid, sizeof(guid));
    b = begin("RIFF", "DMAP");
    chunk("guid", &guid, sizeof(guid));
    end(b);
    end(a);
    hr = load((IUnknown *)seg);
    CHECKF(hr == S_OK, "Load of a segment with an embedded audio path configuration (%#lx)", hr);
    hr = IDirectMusicSegment8_GetAudioPathConfig(seg, &config);
    CHECKF(hr == S_OK && config, "GetAudioPathConfig returns the embedded configuration (%#lx)", hr);
    if (config)
    {
        IDirectMusicObject *object;
        DMUS_OBJECTDESC desc = {sizeof(desc)};
        hr = IUnknown_QueryInterface(config, &IID_IDirectMusicObject, (void **)&object);
        CHECKF(hr == S_OK, "the configuration is a DirectMusic object (%#lx)", hr);
        if (hr == S_OK)
        {
            IDirectMusicObject_GetDescriptor(object, &desc);
            CHECKF(IsEqualGUID(&desc.guidClass, &CLSID_DirectMusicAudioPathConfig), "its class is the audio path configuration");
            IDirectMusicObject_Release(object);
        }
        IUnknown_Release(config);
    }
    IDirectMusicSegment8_Release(seg);
}

static void test_audiopath(IDirectMusicPerformance8 *p)
{
    IDirectMusicAudioPath *path, *path2, *got;
    IDirectSoundBuffer8 *buffer8;
    IDirectSoundBuffer *buffer;
    IDirectMusicPerformance8 *perf8;
    IDirectMusicSegment8 *seg1 = create_segment(100000), *seg2 = create_segment(100000);
    IDirectMusicSegmentState *state1, *state2;
    IDirectMusicGraph *graph;
    IDirectMusicTool *tool;
    IDirectMusicPort *port;
    IUnknown *unk;
    DWORD out;
    LONG volume;
    HRESULT hr;

    hr = IDirectMusicPerformance8_GetDefaultAudioPath(p, &path);
    CHECKF(hr == S_OK && path, "GetDefaultAudioPath (%#lx)", hr);
    if (!path) return;

    hr = IDirectMusicAudioPath_GetObjectInPath(path, 0, DMUS_PATH_AUDIOPATH, 0, &GUID_All_Objects, 0, &IID_IDirectMusicAudioPath, (void **)&got);
    CHECKF(hr == S_OK && got == path, "the AUDIOPATH stage returns the audio path itself (%#lx)", hr);
    if (hr == S_OK) IDirectMusicAudioPath_Release(got);
    IDirectMusicPerformance8_QueryInterface(p, &IID_IDirectMusicPerformance8, (void **)&perf8);
    hr = IDirectMusicAudioPath_GetObjectInPath(path, 0, DMUS_PATH_PERFORMANCE, 0, &GUID_All_Objects, 0, &IID_IDirectMusicPerformance8, (void **)&unk);
    CHECKF(hr == S_OK && unk == (IUnknown *)perf8, "the PERFORMANCE stage returns the performance (%#lx)", hr);
    if (hr == S_OK) IUnknown_Release(unk);
    unk = (IUnknown *)0xdeadbeef;
    hr = IDirectMusicAudioPath_GetObjectInPath(path, 0, DMUS_PATH_PERFORMANCE, 0, &GUID_All_Objects, 0, &GUID_NULL, (void **)&unk);
    CHECKF(hr == E_NOINTERFACE && !unk, "the PERFORMANCE stage honours the interface id (%#lx)", hr);
    IDirectMusicPerformance8_Release(perf8);

    hr = IDirectMusicAudioPath_GetObjectInPath(path, 0, DMUS_PATH_PORT, 0, &GUID_All_Objects, 0, &IID_IDirectMusicPort, (void **)&port);
    CHECKF(hr == S_OK && port, "the PORT stage returns the port of the channel (%#lx)", hr);
    if (hr == S_OK) IDirectMusicPort_Release(port);
    hr = IDirectMusicAudioPath_GetObjectInPath(path, DMUS_PCHANNEL_ALL, DMUS_PATH_PORT, 0, &GUID_All_Objects, 0, &IID_IDirectMusicPort, (void **)&port);
    CHECKF(hr == S_OK, "the PORT stage accepts DMUS_PCHANNEL_ALL (%#lx)", hr);
    if (hr == S_OK) IDirectMusicPort_Release(port);

    hr = IDirectMusicAudioPath_GetObjectInPath(path, DMUS_PCHANNEL_ALL, DMUS_PATH_BUFFER, 0, &GUID_All_Objects, 0, &IID_IDirectSoundBuffer8, (void **)&buffer8);
    CHECKF(hr == S_OK, "the BUFFER stage returns the buffer (%#lx)", hr);
    hr = IDirectMusicAudioPath_GetObjectInPath(path, DMUS_PCHANNEL_ALL, DMUS_PATH_BUFFER, 1, &GUID_All_Objects, 0, &IID_IDirectSoundBuffer8, (void **)&unk);
    CHECKF(hr == DMUS_E_NOT_FOUND, "a second buffer is DMUS_E_NOT_FOUND (%#lx)", hr);
    hr = IDirectMusicAudioPath_GetObjectInPath(path, DMUS_PCHANNEL_ALL, DMUS_PATH_BUFFER_DMO, 0, &GUID_All_Objects, 0, &IID_IUnknown, (void **)&unk);
    CHECKF(FAILED(hr), "the BUFFER_DMO stage of a buffer without effects fails (%#lx)", hr);
    hr = IDirectMusicAudioPath_GetObjectInPath(path, DMUS_PCHANNEL_ALL, DMUS_PATH_PRIMARY_BUFFER, 0, &GUID_All_Objects, 0, &IID_IDirectSoundBuffer, (void **)&buffer);
    CHECKF(hr == S_OK, "the PRIMARY_BUFFER stage returns the primary buffer (%#lx)", hr);
    if (hr == S_OK) IDirectSoundBuffer_Release(buffer);
    hr = IDirectMusicAudioPath_GetObjectInPath(path, 0, 0x9999, 0, &GUID_All_Objects, 0, &IID_IUnknown, (void **)&unk);
    CHECKF(hr == E_INVALIDARG && !unk, "an unknown stage is E_INVALIDARG (%#lx)", hr);
    hr = IDirectMusicAudioPath_GetObjectInPath(path, 0, DMUS_PATH_PORT, 0, &GUID_All_Objects, 0, &IID_IUnknown, NULL);
    CHECKF(hr == E_POINTER, "GetObjectInPath without result pointer is E_POINTER (%#lx)", hr);

    /* tools of the audio path graph and of the performance graph */
    hr = IDirectMusicAudioPath_GetObjectInPath(path, 0, DMUS_PATH_AUDIOPATH_TOOL, 0, &GUID_All_Objects, 0, &IID_IDirectMusicTool, (void **)&tool);
    CHECKF(hr == DMUS_E_NOT_FOUND, "the AUDIOPATH_TOOL stage without graph is DMUS_E_NOT_FOUND (%#lx)", hr);
    hr = IDirectMusicAudioPath_GetObjectInPath(path, 0, DMUS_PATH_AUDIOPATH_GRAPH, 0, &GUID_All_Objects, 0, &IID_IDirectMusicGraph, (void **)&graph);
    CHECKF(hr == S_OK, "the AUDIOPATH_GRAPH stage creates the graph (%#lx)", hr);
    if (hr == S_OK)
    {
        IDirectMusicGraph_InsertTool(graph, &collector, NULL, 0, -1);
        IDirectMusicGraph_Release(graph);
    }
    hr = IDirectMusicAudioPath_GetObjectInPath(path, 0, DMUS_PATH_AUDIOPATH_TOOL, 0, &GUID_All_Objects, 0, &IID_IDirectMusicTool, (void **)&tool);
    CHECKF(hr == S_OK && tool == &collector, "the AUDIOPATH_TOOL stage returns the first tool (%#lx)", hr);
    hr = IDirectMusicAudioPath_GetObjectInPath(path, 0, DMUS_PATH_AUDIOPATH_TOOL, 0, &GUID_All_Objects, 1, &IID_IDirectMusicTool, (void **)&tool);
    CHECKF(hr == DMUS_E_NOT_FOUND, "a second tool is DMUS_E_NOT_FOUND (%#lx)", hr);
    hr = IDirectMusicAudioPath_GetObjectInPath(path, 0, DMUS_PATH_AUDIOPATH_TOOL, 0, &CLSID_DirectMusicSegment, 0, &IID_IDirectMusicTool, (void **)&tool);
    CHECKF(hr == DMUS_E_NOT_FOUND, "no tool is of a class it does not have (%#lx)", hr);
    hr = IDirectMusicAudioPath_GetObjectInPath(path, 0, DMUS_PATH_PERFORMANCE_TOOL, 0, &GUID_All_Objects, 0, &IID_IDirectMusicTool, (void **)&tool);
    CHECKF(hr == S_OK && tool == &collector, "the PERFORMANCE_TOOL stage returns the tool of the performance graph (%#lx)", hr);

    /* the volume of the buffer */
    hr = IDirectMusicAudioPath_SetVolume(path, -500, 0);
    IDirectSoundBuffer8_GetVolume(buffer8, &volume);
    CHECKF(hr == S_OK && volume == -500, "SetVolume without duration (%#lx, %ld)", hr, volume);
    hr = IDirectMusicAudioPath_SetVolume(path, -2500, 2000);
    CHECKF(hr == S_OK, "SetVolume over 2 seconds (%#lx)", hr);
    Sleep(150);
    IDirectSoundBuffer8_GetVolume(buffer8, &volume);
    CHECKF(volume < -500 && volume > -2400, "the volume is on its way after 150 ms (%ld)", volume);
    Sleep(2300);
    IDirectSoundBuffer8_GetVolume(buffer8, &volume);
    CHECKF(volume == -2500, "the volume reached its target (%ld)", volume);
    hr = IDirectMusicAudioPath_SetVolume(path, -100, 3000);
    hr = IDirectMusicAudioPath_SetVolume(path, 0, 0);
    Sleep(250);
    IDirectSoundBuffer8_GetVolume(buffer8, &volume);
    CHECKF(volume == 0, "an immediate SetVolume stops the ramp (%ld)", volume);
    IDirectSoundBuffer8_Release(buffer8);

    /* PChannels */
    out = 0xdeadbeef;
    hr = IDirectMusicAudioPath_ConvertPChannel(path, 5, &out);
    CHECKF(hr == S_OK && out == 5, "ConvertPChannel(5) (%#lx, %lu)", hr, out);
    hr = IDirectMusicAudioPath_ConvertPChannel(path, DMUS_PCHANNEL_BROADCAST_PERFORMANCE, &out);
    CHECKF(hr == S_OK && out == DMUS_PCHANNEL_BROADCAST_PERFORMANCE, "ConvertPChannel of a broadcast value (%#lx)", hr);
    hr = IDirectMusicAudioPath_ConvertPChannel(path, 1000, &out);
    CHECKF(hr == DMUS_E_NOT_FOUND, "ConvertPChannel of a channel the path does not have is DMUS_E_NOT_FOUND (%#lx)", hr);
    hr = IDirectMusicAudioPath_ConvertPChannel(path, 5, NULL);
    CHECKF(hr == E_POINTER, "ConvertPChannel without result is E_POINTER (%#lx)", hr);

    hr = IDirectMusicAudioPath_Activate(path, TRUE);
    hr = IDirectMusicAudioPath_Activate(path, TRUE);
    CHECKF(hr == S_FALSE, "Activate(TRUE) of an active path is S_FALSE (%#lx)", hr);
    hr = IDirectMusicAudioPath_Activate(path, FALSE);
    CHECKF(hr == S_OK, "Activate(FALSE) (%#lx)", hr);
    hr = IDirectMusicAudioPath_Activate(path, FALSE);
    CHECKF(hr == S_FALSE, "Activate(FALSE) of an inactive path is S_FALSE (%#lx)", hr);
    IDirectMusicAudioPath_Activate(path, TRUE);

    /* a segment state works with the path it plays on */
    hr = IDirectMusicPerformance8_CreateStandardAudioPath(p, DMUS_APATH_DYNAMIC_STEREO, 16, TRUE, &path2);
    CHECKF(hr == S_OK && path2, "CreateStandardAudioPath (%#lx)", hr);
    hr = play(p, seg1, 0, 0, &state1, NULL);
    hr = play(p, seg2, 0, 0, &state2, (IUnknown *)path2);
    CHECKF(hr == S_OK, "PlaySegmentEx on an audio path (%#lx)", hr);
    hr = IDirectMusicSegmentState_QueryInterface(state2, &IID_IUnknown, (void **)&unk);
    if (unk) IUnknown_Release(unk);
    {
        IDirectMusicSegmentState8 *s8;
        got = NULL;
        IDirectMusicSegmentState_QueryInterface(state2, &IID_IDirectMusicSegmentState8, (void **)&s8);
        hr = IDirectMusicSegmentState8_GetObjectInPath(s8, 0, DMUS_PATH_AUDIOPATH, 0, &GUID_All_Objects, 0, &IID_IDirectMusicAudioPath, (void **)&got);
        CHECKF(hr == S_OK && got == path2, "a state played on an audio path reports it (%#lx)", hr);
        if (got) IDirectMusicAudioPath_Release(got);
        IDirectMusicSegmentState8_Release(s8);
        IDirectMusicSegmentState_QueryInterface(state1, &IID_IDirectMusicSegmentState8, (void **)&s8);
        got = NULL;
        hr = IDirectMusicSegmentState8_GetObjectInPath(s8, 0, DMUS_PATH_AUDIOPATH, 0, &GUID_All_Objects, 0, &IID_IDirectMusicAudioPath, (void **)&got);
        CHECKF(hr == S_OK && got == path, "a state played without audio path reports the default one (%#lx)", hr);
        if (got) IDirectMusicAudioPath_Release(got);
        IDirectMusicSegmentState8_Release(s8);
    }
    hr = IDirectMusicPerformance8_StopEx(p, (IUnknown *)path2, 0, 0);
    CHECKF(hr == S_OK, "StopEx(audio path) (%#lx)", hr);
    hr = IDirectMusicPerformance8_IsPlaying(p, NULL, state2);
    CHECKF(hr == S_FALSE, "StopEx(audio path) stopped the state on the path (%#lx)", hr);
    hr = IDirectMusicPerformance8_IsPlaying(p, NULL, state1);
    CHECKF(hr == S_OK, "StopEx(audio path) kept the state of the other path (%#lx)", hr);

    IDirectMusicPerformance8_Stop(p, NULL, NULL, 0, 0);
    IDirectMusicSegmentState_Release(state1);
    IDirectMusicSegmentState_Release(state2);
    IDirectMusicAudioPath_Release(path2);
    IDirectMusicAudioPath_Release(path);
    IDirectMusicSegment8_Release(seg1);
    IDirectMusicSegment8_Release(seg2);
}

int main(int argc, char **argv)
{
    BOOL do_perf = argc < 2 || !strcmp(argv[1], "perf"), do_audio = argc < 2 || !strcmp(argv[1], "audiopath");
    IDirectMusicPerformance8 *audio_perf;
    HRESULT hr;

    CoInitialize(NULL);
    InitializeCriticalSection(&msg_lock);

    hr = CoCreateInstance(&CLSID_DirectMusicPerformance, NULL, CLSCTX_INPROC_SERVER, &IID_IDirectMusicPerformance8, (void **)&perf);
    if (hr != S_OK)
    {
        printf("SKIP  no DirectMusic performance (%#lx)\n", hr);
        return 77;
    }
    IDirectMusicPerformance8_Release(perf);
    if (!(perf = create_perf(FALSE, 0)))
    {
        printf("SKIP  performance Init failed\n");
        return 77;
    }

    if (do_perf)
    {
        test_basic();
        test_ports();
        test_final_tool();
        test_time();
        test_params();
        test_play_start();
        test_invalidate();
        test_segment_config();
        test_audiopath_config();
    }
    IDirectMusicPerformance8_CloseDown(perf);
    IDirectMusicPerformance8_Release(perf);

    if (!do_audio) ;
    else if ((audio_perf = create_perf(TRUE, DMUS_APATH_SHARED_STEREOPLUSREVERB)))
    {
        test_audiopath(audio_perf);
        IDirectMusicPerformance8_CloseDown(audio_perf);
        IDirectMusicPerformance8_Release(audio_perf);
    }
    else printf("note  the audio path part is skipped\n");

    CoUninitialize();
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
