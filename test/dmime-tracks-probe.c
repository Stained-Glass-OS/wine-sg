/* dmime track objects (patches/sg/2950), run by test/dmime-tracks-gate.sh:
 * Init / InitPlay / EndPlay / Play / PlayEx / Clone / Join / GetParam / GetParamEx / SetParamEx and
 * IPersistStream::Load of the sysex, lyrics, tempo, time signature, marker, sequence, segment trigger,
 * wave and parameter control tracks and of the tool graph. The tracks are loaded from RIFF data built
 * here, played into a performance whose tool graph holds a collecting tool, and the messages
 * they send are compared with the data that was loaded.
 *
 *   dmime-tracks-probe.exe */
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

/* ---- collecting tool ---- */
struct msg
{
    DWORD type;
    MUSIC_TIME mtTime;
    DWORD pchannel;
    DWORD track_id;
    DWORD group;
    double tempo;
    BYTE sig[4];
    DWORD len;
    BYTE data[16];
    WCHAR text[16];
    BYTE midi[3];
    WORD gridsperbeat;
};
static struct msg msgs[32];
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
    switch (pmsg->dwType)
    {
    case DMUS_PMSGT_TEMPO:
        m->tempo = ((DMUS_TEMPO_PMSG *)pmsg)->dblTempo;
        break;
    case DMUS_PMSGT_TIMESIG:
        m->sig[0] = ((DMUS_TIMESIG_PMSG *)pmsg)->bBeatsPerMeasure;
        m->sig[1] = ((DMUS_TIMESIG_PMSG *)pmsg)->bBeat;
        m->gridsperbeat = ((DMUS_TIMESIG_PMSG *)pmsg)->wGridsPerBeat;
        break;
    case DMUS_PMSGT_SYSEX:
        m->len = ((DMUS_SYSEX_PMSG *)pmsg)->dwLen;
        memcpy(m->data, ((DMUS_SYSEX_PMSG *)pmsg)->abData, min(m->len, sizeof(m->data)));
        break;
    case DMUS_PMSGT_LYRIC:
        lstrcpynW(m->text, ((DMUS_LYRIC_PMSG *)pmsg)->wszString, 16);
        break;
    case DMUS_PMSGT_MIDI:
        m->midi[0] = ((DMUS_MIDI_PMSG *)pmsg)->bStatus;
        m->midi[1] = ((DMUS_MIDI_PMSG *)pmsg)->bByte1;
        m->midi[2] = ((DMUS_MIDI_PMSG *)pmsg)->bByte2;
        break;
    case DMUS_PMSGT_NOTE:
        m->midi[1] = ((DMUS_NOTE_PMSG *)pmsg)->bMidiValue;
        m->midi[2] = ((DMUS_NOTE_PMSG *)pmsg)->bVelocity;
        m->len = ((DMUS_NOTE_PMSG *)pmsg)->mtDuration;
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
static IDirectMusicTool collector = {&tool_vtbl};

/* ---- a tool class that loads itself, for the tool graph ---- */
DEFINE_GUID(CLSID_ProbeTool, 0x7a2f0c11, 0x5a6e, 0x4d11, 0x8b, 0x2e, 0x00, 0x11, 0x22, 0x33, 0x44, 0x55);
struct ptool
{
    IDirectMusicTool IDirectMusicTool_iface;
    IPersistStream IPersistStream_iface;
    LONG ref;
    DWORD value;
};
static struct ptool *ptool_from_tool(IDirectMusicTool *iface) { return (struct ptool *)iface; }
static HRESULT WINAPI ptool_QueryInterface(IDirectMusicTool *iface, REFIID riid, void **out)
{
    struct ptool *t = ptool_from_tool(iface);
    *out = NULL;
    if (IsEqualGUID(riid, &IID_IUnknown) || IsEqualGUID(riid, &IID_IDirectMusicTool)) *out = &t->IDirectMusicTool_iface;
    else if (IsEqualGUID(riid, &IID_IPersistStream)) *out = &t->IPersistStream_iface;
    else return E_NOINTERFACE;
    InterlockedIncrement(&t->ref);
    return S_OK;
}
static ULONG WINAPI ptool_AddRef(IDirectMusicTool *iface) { return InterlockedIncrement(&ptool_from_tool(iface)->ref); }
static ULONG WINAPI ptool_Release(IDirectMusicTool *iface)
{
    struct ptool *t = ptool_from_tool(iface);
    LONG ref = InterlockedDecrement(&t->ref);
    if (!ref) HeapFree(GetProcessHeap(), 0, t);
    return ref;
}
static const IDirectMusicToolVtbl ptool_vtbl = {ptool_QueryInterface, ptool_AddRef, ptool_Release, tool_Init,
        tool_GetMsgDeliveryType, tool_GetMediaTypeArraySize, tool_GetMediaTypes, tool_ProcessPMsg, tool_Flush};
static struct ptool *ptool_from_persist(IPersistStream *iface)
{
    return (struct ptool *)((char *)iface - offsetof(struct ptool, IPersistStream_iface));
}
static HRESULT WINAPI pps_QueryInterface(IPersistStream *iface, REFIID riid, void **out)
{
    return ptool_QueryInterface(&ptool_from_persist(iface)->IDirectMusicTool_iface, riid, out);
}
static ULONG WINAPI pps_AddRef(IPersistStream *iface) { return InterlockedIncrement(&ptool_from_persist(iface)->ref); }
static ULONG WINAPI pps_Release(IPersistStream *iface)
{
    return ptool_Release(&ptool_from_persist(iface)->IDirectMusicTool_iface);
}
static HRESULT WINAPI pps_GetClassID(IPersistStream *iface, CLSID *id) { *id = CLSID_ProbeTool; return S_OK; }
static HRESULT WINAPI pps_IsDirty(IPersistStream *iface) { return S_FALSE; }
static HRESULT WINAPI pps_Load(IPersistStream *iface, IStream *stream)
{
    struct ptool *t = ptool_from_persist(iface);
    DWORD hdr[2];
    /* the graph hands over the stream at the start of the 'tdat' chunk */
    if (FAILED(IStream_Read(stream, hdr, sizeof(hdr), NULL)) || hdr[0] != mmioFOURCC('t', 'd', 'a', 't')) return E_FAIL;
    return IStream_Read(stream, &t->value, sizeof(t->value), NULL);
}
static HRESULT WINAPI pps_Save(IPersistStream *iface, IStream *stream, BOOL clear) { return E_NOTIMPL; }
static HRESULT WINAPI pps_GetSizeMax(IPersistStream *iface, ULARGE_INTEGER *size) { return E_NOTIMPL; }
static const IPersistStreamVtbl pps_vtbl = {pps_QueryInterface, pps_AddRef, pps_Release, pps_GetClassID,
        pps_IsDirty, pps_Load, pps_Save, pps_GetSizeMax};

static HRESULT WINAPI cf_QueryInterface(IClassFactory *iface, REFIID riid, void **out)
{
    if (IsEqualGUID(riid, &IID_IUnknown) || IsEqualGUID(riid, &IID_IClassFactory))
    {
        *out = iface;
        return S_OK;
    }
    *out = NULL;
    return E_NOINTERFACE;
}
static ULONG WINAPI cf_AddRef(IClassFactory *iface) { return 2; }
static ULONG WINAPI cf_Release(IClassFactory *iface) { return 1; }
static HRESULT WINAPI cf_CreateInstance(IClassFactory *iface, IUnknown *outer, REFIID riid, void **out)
{
    struct ptool *t = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*t));
    HRESULT hr;
    t->IDirectMusicTool_iface.lpVtbl = &ptool_vtbl;
    t->IPersistStream_iface.lpVtbl = &pps_vtbl;
    t->ref = 1;
    hr = IDirectMusicTool_QueryInterface(&t->IDirectMusicTool_iface, riid, out);
    IDirectMusicTool_Release(&t->IDirectMusicTool_iface);
    return hr;
}
static HRESULT WINAPI cf_LockServer(IClassFactory *iface, BOOL lock) { return S_OK; }
static const IClassFactoryVtbl cf_vtbl = {cf_QueryInterface, cf_AddRef, cf_Release, cf_CreateInstance, cf_LockServer};
static IClassFactory probe_factory = {&cf_vtbl};

/* ---- helpers ---- */
static IDirectMusicPerformance *perf;

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
static const char *what_msg(int i, DWORD type, MUSIC_TIME time)
{
    static char b[128];
    snprintf(b, sizeof(b), "message %d is type %#lx at %ld", i, type, time);
    return b;
}
#define EXPECT_COUNT(n, tag) CHECKF(msg_count == (n), "%s sends %d messages (got %d)", tag, (n), msg_count)
#define EXPECT_MSG(i, t, time) CHECKF(msg_count > (i) && msgs[i].type == (t) && msgs[i].mtTime == (time), \
        "%s", what_msg(i, t, time))

static void test_all_tracks(void)
{
    static const struct { const CLSID *clsid; const char *name; BOOL has8; BOOL play_null_ok; } classes[] = {
        {&CLSID_DirectMusicLyricsTrack, "lyrics", TRUE}, {&CLSID_DirectMusicMarkerTrack, "marker", FALSE, TRUE},
        {&CLSID_DirectMusicParamControlTrack, "paramcontrol", TRUE}, {&CLSID_DirectMusicSegmentTriggerTrack, "segtrigger", TRUE},
        {&CLSID_DirectMusicSeqTrack, "seq", TRUE}, {&CLSID_DirectMusicSysExTrack, "sysex", TRUE},
        {&CLSID_DirectMusicTempoTrack, "tempo", TRUE}, {&CLSID_DirectMusicTimeSigTrack, "timesig", FALSE},
        {&CLSID_DirectMusicWaveTrack, "wave", TRUE},
    };
    unsigned int i;

    for (i = 0; i < ARRAY_SIZE_(classes); i++)
    {
        IDirectMusicTrack *track = create_track(classes[i].clsid), *result;
        IDirectMusicTrack8 *track8;
        const char *name = classes[i].name;
        void *state = (void *)0xdeadbeef;
        HRESULT hr;

        if (!track) continue;

        hr = IDirectMusicTrack_Init(track, NULL);
        CHECKF(hr == S_OK, "%s Init is S_OK (%#lx)", name, hr);
        hr = IDirectMusicTrack_InitPlay(track, NULL, NULL, &state, 1, 0);
        CHECKF(hr == S_OK, "%s InitPlay is S_OK (%#lx)", name, hr);
        if (classes[i].clsid != &CLSID_DirectMusicTempoTrack)
            hr = IDirectMusicTrack_EndPlay(track, state);
        else
            hr = IDirectMusicTrack_EndPlay(track, state), state = NULL;
        CHECKF(hr == S_OK, "%s EndPlay is S_OK (%#lx)", name, hr);

        result = (IDirectMusicTrack *)0xdeadbeef;
        hr = IDirectMusicTrack_Clone(track, 0, 0, NULL);
        CHECKF(hr == E_POINTER, "%s Clone(NULL) is E_POINTER (%#lx)", name, hr);
        hr = IDirectMusicTrack_Clone(track, 5, 2, &result);
        CHECKF(hr == E_INVALIDARG && result == (IDirectMusicTrack *)0xdeadbeef, "%s Clone(5, 2) is E_INVALIDARG (%#lx)", name, hr);
        hr = IDirectMusicTrack_Clone(track, -1, 2, &result);
        CHECKF(hr == E_INVALIDARG, "%s Clone(-1, 2) is E_INVALIDARG (%#lx)", name, hr);
        result = clone(track, 0, 100);
        if (result) IDirectMusicTrack_Release(result);

        if (!classes[i].play_null_ok)
        {
            hr = IDirectMusicTrack_Play(track, NULL, 0, 10, 0, 0, NULL, NULL, 1);
            CHECKF(hr == E_POINTER, "%s Play without performance is E_POINTER (%#lx)", name, hr);
        }

        if (SUCCEEDED(IDirectMusicTrack_QueryInterface(track, &IID_IDirectMusicTrack8, (void **)&track8)))
        {
            hr = IDirectMusicTrack8_PlayEx(track8, NULL, 0, 0, 0, 0, NULL, NULL, 0);
            CHECKF(hr == E_POINTER, "%s PlayEx without performance is E_POINTER (%#lx)", name, hr);
            IDirectMusicTrack8_Release(track8);
        }
        else CHECKF(!classes[i].has8, "%s has no IDirectMusicTrack8", name);

        IDirectMusicTrack_Release(track);
    }
}

static void test_ex_params(void)
{
    static const struct { const CLSID *clsid; const char *name; HRESULT get, set; } classes[] = {
        {&CLSID_DirectMusicLyricsTrack, "lyrics", DMUS_E_GET_UNSUPPORTED, DMUS_E_SET_UNSUPPORTED},
        {&CLSID_DirectMusicParamControlTrack, "paramcontrol", DMUS_E_GET_UNSUPPORTED, DMUS_E_SET_UNSUPPORTED},
        {&CLSID_DirectMusicSegmentTriggerTrack, "segtrigger", DMUS_E_GET_UNSUPPORTED, S_OK},
        {&CLSID_DirectMusicWaveTrack, "wave", DMUS_E_GET_UNSUPPORTED, DMUS_E_TYPE_UNSUPPORTED},
    };
    unsigned int i;

    for (i = 0; i < ARRAY_SIZE_(classes); i++)
    {
        IDirectMusicTrack *track = create_track(classes[i].clsid);
        IDirectMusicTrack8 *track8;
        char param[16];
        HRESULT hr;

        IDirectMusicTrack_QueryInterface(track, &IID_IDirectMusicTrack8, (void **)&track8);
        hr = IDirectMusicTrack8_GetParamEx(track8, &GUID_BandParam, 0, NULL, param, NULL, 0);
        CHECKF(hr == classes[i].get, "%s GetParamEx is %#lx (%#lx)", classes[i].name, classes[i].get, hr);
        hr = IDirectMusicTrack8_SetParamEx(track8, &GUID_BandParam, 0, param, NULL, 0);
        CHECKF(hr == classes[i].set, "%s SetParamEx is %#lx (%#lx)", classes[i].name, classes[i].set, hr);
        IDirectMusicTrack8_Release(track8);
        IDirectMusicTrack_Release(track);
    }
}

static void test_sysex(void)
{
    static const BYTE data1[] = {0xf0, 0x01, 0xf7}, data2[] = {0xf0, 0x7f, 0x02, 0xf7};
    DMUS_IO_SYSEX_ITEM item1 = {10, 3, sizeof(data1)}, item2 = {50, 4, sizeof(data2)};
    IDirectMusicTrack *track = create_track(&CLSID_DirectMusicSysExTrack), *copy;
    IDirectMusicTrack8 *track8;
    REFERENCE_TIME now;
    MUSIC_TIME base;
    HRESULT hr;
    int at;

    at = begin("syex", NULL);
    put(&item1, sizeof(item1)); put(data1, sizeof(data1));
    put(&item2, sizeof(item2)); put(data2, sizeof(data2));
    end(at);
    hr = load_track(track);
    CHECKF(hr == S_OK, "sysex Load (%#lx)", hr);

    hr = play(track, 0, 100, 1000);
    CHECKF(hr == S_OK, "sysex Play (%#lx)", hr);
    EXPECT_COUNT(2, "sysex Play(0, 100)");
    EXPECT_MSG(0, DMUS_PMSGT_SYSEX, 1010);
    CHECKF(msg_count > 1 && msgs[0].pchannel == 3 && msgs[0].len == 3 && !memcmp(msgs[0].data, data1, 3),
            "sysex message 0 carries channel 3 and the first data");
    EXPECT_MSG(1, DMUS_PMSGT_SYSEX, 1050);
    CHECKF(msg_count > 1 && msgs[1].pchannel == 4 && msgs[1].len == 4 && !memcmp(msgs[1].data, data2, 4) && msgs[1].track_id == 7,
            "sysex message 1 carries channel 4, the second data and the track id");
    play(track, 20, 100, 0);
    EXPECT_COUNT(1, "sysex Play(20, 100)");
    EXPECT_MSG(0, DMUS_PMSGT_SYSEX, 50);

    copy = clone(track, 40, 100);
    play(copy, 0, 100, 0);
    EXPECT_COUNT(1, "sysex clone(40, 100) Play");
    EXPECT_MSG(0, DMUS_PMSGT_SYSEX, 10);
    CHECKF(msg_count && msgs[0].pchannel == 4 && msgs[0].len == 4 && !memcmp(msgs[0].data, data2, 4),
            "sysex clone(40, 100) kept the second item's data");
    IDirectMusicTrack_Release(copy);
    copy = clone(track, 0, 40);
    play(copy, 0, 100, 0);
    EXPECT_COUNT(1, "sysex clone(0, 40) Play");
    EXPECT_MSG(0, DMUS_PMSGT_SYSEX, 10);
    IDirectMusicTrack_Release(copy);

    /* reference time version, 195312 is 30 ticks at 120 bpm */
    IDirectMusicTrack_QueryInterface(track, &IID_IDirectMusicTrack8, (void **)&track8);
    IDirectMusicPerformance_GetTime(perf, &now, NULL);
    IDirectMusicPerformance_ReferenceToMusicTime(perf, now, &base);
    msg_count = 0;
    hr = IDirectMusicTrack8_PlayEx(track8, NULL, 0, 195312, now, 0, perf, NULL, 7);
    CHECKF(hr == S_OK, "sysex PlayEx (%#lx)", hr);
    EXPECT_COUNT(1, "sysex PlayEx(0, 30 ticks)");
    CHECKF(msg_count && abs(msgs[0].mtTime - (base + 10)) <= 2, "sysex PlayEx message is at the offset plus 10 (%ld, base %ld)",
            msg_count ? msgs[0].mtTime : 0, base);
    IDirectMusicTrack8_Release(track8);

    /* sysex data that is cut short */
    IDirectMusicTrack_Release(track);
    track = create_track(&CLSID_DirectMusicSysExTrack);
    at = begin("syex", NULL);
    put(&item1, sizeof(item1)); put(data1, 2);
    end(at);
    hr = load_track(track);
    CHECKF(hr == DMUS_E_UNSUPPORTED_STREAM, "sysex Load of a truncated item fails (%#lx)", hr);
    IDirectMusicTrack_Release(track);

    track = create_track(&CLSID_DirectMusicSysExTrack);
    at = begin("junk", NULL); put(&item1, sizeof(item1)); end(at);
    hr = load_track(track);
    CHECKF(hr == DMUS_E_UNSUPPORTED_STREAM, "sysex Load of another chunk fails (%#lx)", hr);
    IDirectMusicTrack_Release(track);
}

static void add_lyric(const char *text, MUSIC_TIME logical, MUSIC_TIME physical)
{
    DMUS_IO_LYRICSTRACK_EVENTHEADER header = {0, 0, logical, physical};
    WCHAR wide[16];
    int at = begin("LIST", "lyre"), i;

    chunk("lyrh", &header, sizeof(header));
    for (i = 0; text[i]; i++) wide[i] = text[i];
    wide[i] = 0;
    chunk("lyrn", wide, (i + 1) * sizeof(WCHAR));
    end(at);
}

static void test_lyrics(void)
{
    IDirectMusicTrack *track = create_track(&CLSID_DirectMusicLyricsTrack), *copy;
    HRESULT hr;
    int a, b;

    a = begin("LIST", "lyrt");
    b = begin("LIST", "lyrl");
    add_lyric("Hello", 20, 25);
    add_lyric("World", 60, 70);
    end(b);
    end(a);
    hr = load_track(track);
    CHECKF(hr == S_OK, "lyrics Load (%#lx)", hr);

    hr = play(track, 0, 50, 0);
    CHECKF(hr == S_OK, "lyrics Play (%#lx)", hr);
    EXPECT_COUNT(1, "lyrics Play(0, 50)");
    EXPECT_MSG(0, DMUS_PMSGT_LYRIC, 25);
    CHECKF(msg_count && !lstrcmpW(msgs[0].text, L"Hello") && msgs[0].track_id == 7, "lyrics message text is Hello");
    play(track, 0, 200, 100);
    EXPECT_COUNT(2, "lyrics Play(0, 200, 100)");
    EXPECT_MSG(1, DMUS_PMSGT_LYRIC, 170);
    CHECKF(msg_count > 1 && !lstrcmpW(msgs[1].text, L"World"), "lyrics second message text is World");

    copy = clone(track, 30, 100);
    play(copy, 0, 100, 5);
    EXPECT_COUNT(1, "lyrics clone(30, 100) Play");
    EXPECT_MSG(0, DMUS_PMSGT_LYRIC, 45);
    CHECKF(msg_count && !lstrcmpW(msgs[0].text, L"World"), "lyrics clone kept the text");
    IDirectMusicTrack_Release(copy);
    IDirectMusicTrack_Release(track);
}

static void test_tempo(void)
{
    static const DMUS_IO_TEMPO_ITEM items[] = {{100, 80}, {300, 60}, {200, 20}, {4000, 50}};
    static const DMUS_IO_TEMPO_ITEM items_a[] = {{0, 100}, {200, 120}}, items_b[] = {{0, 60}, {100, 70}};
    IDirectMusicTrack *track = create_track(&CLSID_DirectMusicTempoTrack), *copy, *a, *b;
    IDirectMusicTrack8 *track8, *a8;
    REFERENCE_TIME rt0, rt300, next_rt;
    DMUS_TEMPO_PARAM param;
    MUSIC_TIME next;
    void *state = NULL;
    HRESULT hr;

    array_chunk("tetr", items, sizeof(items[0]), 4);
    hr = load_track(track);
    CHECKF(hr == S_OK, "tempo Load (%#lx)", hr);

    hr = play(track, 0, 1000, 0);
    CHECKF(hr == S_OK, "tempo Play (%#lx)", hr);
    EXPECT_COUNT(3, "tempo Play(0, 1000)");
    EXPECT_MSG(0, DMUS_PMSGT_TEMPO, 100);
    CHECKF(msg_count && msgs[0].tempo == 80 && msgs[0].group == (DWORD)-1 && msgs[0].track_id == 7, "tempo message 0 is 80 bpm");
    EXPECT_MSG(1, DMUS_PMSGT_TEMPO, 300);
    CHECKF(msg_count > 1 && msgs[1].tempo == 60, "tempo message 1 is 60 bpm");
    CHECKF(msg_count > 2 && msgs[2].type == DMUS_PMSGT_DIRTY && msgs[2].mtTime == 300, "tempo is followed by a DMUS_PMSGT_DIRTY message");
    play(track, 0, 50, 0);
    EXPECT_COUNT(0, "tempo Play(0, 50)");
    play(track, 400, 5000, 10);
    EXPECT_COUNT(2, "tempo Play(400, 5000)");
    EXPECT_MSG(0, DMUS_PMSGT_TEMPO, 4010);
    CHECKF(msg_count && msgs[0].tempo == 50, "tempo message at 4010 is 50 bpm");

    copy = clone(track, 150, 350);
    play(copy, 0, 1000, 0);
    EXPECT_COUNT(3, "tempo clone(150, 350) Play");
    EXPECT_MSG(0, DMUS_PMSGT_TEMPO, 0);
    CHECKF(msg_count && msgs[0].tempo == 80, "tempo clone starts with the tempo in effect (80)");
    EXPECT_MSG(1, DMUS_PMSGT_TEMPO, 150);
    CHECKF(msg_count > 1 && msgs[1].tempo == 60, "tempo clone item 300 moved to 150");
    IDirectMusicTrack_Release(copy);

    /* disable / enable */
    hr = IDirectMusicTrack_SetParam(track, &GUID_DisableTempo, 0, &param);
    CHECKF(hr == S_OK, "tempo SetParam(GUID_DisableTempo) (%#lx)", hr);
    hr = IDirectMusicTrack_GetParam(track, &GUID_TempoParam, 0, &next, &param);
    CHECKF(hr == DMUS_E_TYPE_DISABLED, "tempo GetParam when disabled is DMUS_E_TYPE_DISABLED (%#lx)", hr);
    play(track, 0, 1000, 0);
    EXPECT_COUNT(0, "disabled tempo Play");
    hr = IDirectMusicTrack_SetParam(track, &GUID_EnableTempo, 0, &param);
    CHECKF(hr == S_OK, "tempo SetParam(GUID_EnableTempo) (%#lx)", hr);
    hr = IDirectMusicTrack_GetParam(track, &GUID_TempoParam, 0, &next, &param);
    CHECKF(hr == S_OK, "tempo GetParam once enabled (%#lx)", hr);
    IDirectMusicTrack_Release(track);

    /* join */
    a = create_track(&CLSID_DirectMusicTempoTrack);
    array_chunk("tetr", items_a, sizeof(items_a[0]), 2);
    load_track(a);
    b = create_track(&CLSID_DirectMusicTempoTrack);
    array_chunk("tetr", items_b, sizeof(items_b[0]), 2);
    load_track(b);
    IDirectMusicTrack_QueryInterface(a, &IID_IDirectMusicTrack8, (void **)&a8);
    copy = (IDirectMusicTrack *)0xdeadbeef;
    hr = IDirectMusicTrack8_Join(a8, NULL, 0, NULL, 0, NULL);
    CHECKF(hr == E_POINTER, "tempo Join without pointers is E_POINTER (%#lx)", hr);
    hr = IDirectMusicTrack8_Join(a8, b, 150, NULL, 0, &copy);
    CHECKF(hr == S_OK && copy != (IDirectMusicTrack *)0xdeadbeef && copy != a && copy != b, "tempo Join (%#lx)", hr);
    if (hr == S_OK)
    {
        play(copy, 0, 1000, 0);
        EXPECT_COUNT(4, "joined tempo track Play");
        EXPECT_MSG(0, DMUS_PMSGT_TEMPO, 0);
        CHECKF(msg_count && msgs[0].tempo == 100, "joined track starts with the first track (100)");
        EXPECT_MSG(1, DMUS_PMSGT_TEMPO, 150);
        CHECKF(msg_count > 1 && msgs[1].tempo == 60, "joined track continues with the second track at the join time (60)");
        EXPECT_MSG(2, DMUS_PMSGT_TEMPO, 250);
        CHECKF(msg_count > 2 && msgs[2].tempo == 70, "joined track shifts the second track items (70 at 250)");
        IDirectMusicTrack_Release(copy);
    }
    {
        IDirectMusicTrack *sysex = create_track(&CLSID_DirectMusicSysExTrack);
        hr = IDirectMusicTrack8_Join(a8, sysex, 150, NULL, 0, &copy);
        CHECKF(hr == DMUS_E_TYPE_UNSUPPORTED, "tempo Join with another kind of track fails (%#lx)", hr);
        IDirectMusicTrack_Release(sysex);
    }

    /* the extended methods go through the performance stored by InitPlay */
    hr = IDirectMusicTrack_InitPlay(a, NULL, perf, &state, 1, 0);
    CHECKF(hr == S_OK && state, "tempo InitPlay returns state data (%#lx)", hr);
    IDirectMusicTrack_QueryInterface(a, &IID_IDirectMusicTrack8, (void **)&track8);
    IDirectMusicPerformance_MusicToReferenceTime(perf, 0, &rt0);
    IDirectMusicPerformance_MusicToReferenceTime(perf, 300, &rt300);
    memset(&param, 0xcd, sizeof(param));
    next_rt = 0xdeadbeef;
    hr = IDirectMusicTrack8_GetParamEx(track8, &GUID_TempoParam, rt0, &next_rt, &param, state, 0);
    CHECKF(hr == S_OK && param.dblTempo == 100 && param.mtTime == 0, "tempo GetParamEx at music time 0 (%#lx, %g, %ld)", hr, param.dblTempo, param.mtTime);
    CHECKF(llabs(next_rt - 200 * 6510) < 3 * 6510, "tempo GetParamEx next time is 200 ticks away (%lld)", (long long)next_rt);
    hr = IDirectMusicTrack8_GetParamEx(track8, &GUID_TempoParam, rt0, NULL, &param, NULL, 0);
    CHECKF(hr == E_POINTER, "tempo GetParamEx without state is E_POINTER (%#lx)", hr);
    hr = IDirectMusicTrack8_GetParamEx(track8, &GUID_BandParam, rt0, NULL, &param, state, 0);
    CHECKF(hr == DMUS_E_GET_UNSUPPORTED, "tempo GetParamEx of another type is DMUS_E_GET_UNSUPPORTED (%#lx)", hr);
    param.dblTempo = 90;
    param.mtTime = 0;
    hr = IDirectMusicTrack8_SetParamEx(track8, &GUID_TempoParam, rt300, &param, state, 0);
    CHECKF(hr == S_OK, "tempo SetParamEx (%#lx)", hr);
    memset(&param, 0xcd, sizeof(param));
    hr = IDirectMusicTrack_GetParam(a, &GUID_TempoParam, 300, &next, &param);
    CHECKF(hr == S_OK && param.dblTempo == 90, "tempo SetParamEx inserted a tempo at music time 300 (%g)", param.dblTempo);
    hr = IDirectMusicTrack_EndPlay(a, state);
    CHECKF(hr == S_OK, "tempo EndPlay (%#lx)", hr);
    hr = IDirectMusicTrack_EndPlay(a, NULL);
    CHECKF(hr == E_POINTER, "tempo EndPlay without state is E_POINTER (%#lx)", hr);
    IDirectMusicTrack8_Release(track8);
    IDirectMusicTrack8_Release(a8);
    IDirectMusicTrack_Release(a);
    IDirectMusicTrack_Release(b);
}

static void test_timesig(void)
{
    static const DMUS_IO_TIMESIGNATURE_ITEM items[] = {{0, 4, 4, 2}, {100, 3, 4, 2}, {300, 6, 8, 2}};
    IDirectMusicTrack *track = create_track(&CLSID_DirectMusicTimeSigTrack), *copy;
    DMUS_TIMESIGNATURE sig;
    MUSIC_TIME next;
    HRESULT hr;
    int a;

    a = begin("LIST", "TIMS");
    array_chunk("tims", items, sizeof(items[0]), 3);
    end(a);
    hr = load_track(track);
    CHECKF(hr == S_OK, "timesig Load (%#lx)", hr);

    memset(&sig, 0xcd, sizeof(sig));
    next = 0xdeadbeef;
    hr = IDirectMusicTrack_GetParam(track, &GUID_TimeSignature, 150, &next, &sig);
    CHECKF(hr == S_OK && sig.mtTime == -50 && sig.bBeatsPerMeasure == 3 && sig.bBeat == 4 && sig.wGridsPerBeat == 2 && next == 150,
            "timesig GetParam(150) is 3/4 starting at -50, next 150 (%#lx, %ld, %d/%d, %ld)", hr, sig.mtTime, sig.bBeatsPerMeasure, sig.bBeat, next);
    hr = IDirectMusicTrack_GetParam(track, &GUID_TimeSignature, 0, &next, &sig);
    CHECKF(hr == S_OK && sig.mtTime == 0 && sig.bBeatsPerMeasure == 4 && next == 100, "timesig GetParam(0) is 4/4, next 100 (%#lx, %d, %ld)", hr, sig.bBeatsPerMeasure, next);
    hr = IDirectMusicTrack_GetParam(track, &GUID_TimeSignature, 300, &next, &sig);
    CHECKF(hr == S_OK && sig.mtTime == 0 && sig.bBeatsPerMeasure == 6 && sig.bBeat == 8 && next == 0, "timesig GetParam(300) is 6/8, no next (%#lx, %d/%d, %ld)", hr, sig.bBeatsPerMeasure, sig.bBeat, next);
    hr = IDirectMusicTrack_GetParam(track, &GUID_TimeSignature, 0, NULL, NULL);
    CHECKF(hr == E_POINTER, "timesig GetParam without data is E_POINTER (%#lx)", hr);
    hr = IDirectMusicTrack_GetParam(track, &GUID_TempoParam, 0, NULL, &sig);
    CHECKF(hr == DMUS_E_GET_UNSUPPORTED, "timesig GetParam of another type is DMUS_E_GET_UNSUPPORTED (%#lx)", hr);

    hr = play(track, 0, 350, 10);
    CHECKF(hr == S_OK, "timesig Play (%#lx)", hr);
    EXPECT_COUNT(3, "timesig Play(0, 350)");
    EXPECT_MSG(0, DMUS_PMSGT_TIMESIG, 10);
    EXPECT_MSG(1, DMUS_PMSGT_TIMESIG, 110);
    CHECKF(msg_count > 1 && msgs[1].sig[0] == 3 && msgs[1].sig[1] == 4 && msgs[1].gridsperbeat == 2, "timesig message 1 is 3/4, 2 grids");
    EXPECT_MSG(2, DMUS_PMSGT_TIMESIG, 310);
    CHECKF(msg_count > 2 && msgs[2].sig[0] == 6 && msgs[2].sig[1] == 8, "timesig message 2 is 6/8");

    copy = clone(track, 120, 400);
    hr = IDirectMusicTrack_GetParam(copy, &GUID_TimeSignature, 0, &next, &sig);
    CHECKF(hr == S_OK && sig.bBeatsPerMeasure == 3 && sig.mtTime == 0 && next == 180, "timesig clone(120, 400) starts with 3/4, next 180 (%#lx, %d, %ld)", hr, sig.bBeatsPerMeasure, next);
    IDirectMusicTrack_Release(copy);

    hr = IDirectMusicTrack_SetParam(track, &GUID_DisableTimeSig, 0, NULL);
    CHECKF(hr == S_OK, "timesig SetParam(GUID_DisableTimeSig) (%#lx)", hr);
    hr = IDirectMusicTrack_GetParam(track, &GUID_TimeSignature, 0, &next, &sig);
    CHECKF(hr == DMUS_E_TYPE_DISABLED, "timesig GetParam when disabled is DMUS_E_TYPE_DISABLED (%#lx)", hr);
    play(track, 0, 350, 0);
    EXPECT_COUNT(0, "disabled timesig Play");
    hr = IDirectMusicTrack_SetParam(track, &GUID_EnableTimeSig, 0, NULL);
    hr = IDirectMusicTrack_GetParam(track, &GUID_TimeSignature, 0, &next, &sig);
    CHECKF(hr == S_OK, "timesig GetParam once enabled (%#lx)", hr);
    IDirectMusicTrack_Release(track);

    track = create_track(&CLSID_DirectMusicTimeSigTrack);
    hr = IDirectMusicTrack_GetParam(track, &GUID_TimeSignature, 0, &next, &sig);
    CHECKF(hr == DMUS_E_NOT_FOUND, "timesig GetParam without items is DMUS_E_NOT_FOUND (%#lx)", hr);
    IDirectMusicTrack_Release(track);
}

static void test_marker(void)
{
    static const DMUS_IO_VALID_START vals[] = {{20}, {80}};
    static const DMUS_IO_PLAY_MARKER marks[] = {{50}};
    IDirectMusicTrack *track = create_track(&CLSID_DirectMusicMarkerTrack), *copy;
    MUSIC_TIME next, mark;
    HRESULT hr;
    int a;

    a = begin("LIST", "MARK");
    array_chunk("vals", vals, sizeof(vals[0]), 2);
    array_chunk("play", marks, sizeof(marks[0]), 1);
    end(a);
    hr = load_track(track);
    CHECKF(hr == S_OK, "marker Load (%#lx)", hr);

    mark = 0xdeadbeef; next = 0xdeadbeef;
    hr = IDirectMusicTrack_GetParam(track, &GUID_Valid_Start_Time, 0, &next, &mark);
    CHECKF(hr == S_OK && mark == 20 && next == 80, "marker valid start at 0 is 20, next 80 (%#lx, %ld, %ld)", hr, mark, next);
    hr = IDirectMusicTrack_GetParam(track, &GUID_Valid_Start_Time, 21, &next, &mark);
    CHECKF(hr == S_OK && mark == 59 && next == 0, "marker valid start at 21 is 59 away (%#lx, %ld, %ld)", hr, mark, next);
    hr = IDirectMusicTrack_GetParam(track, &GUID_Valid_Start_Time, 81, &next, &mark);
    CHECKF(hr == DMUS_E_NOT_FOUND, "marker valid start after the last one is DMUS_E_NOT_FOUND (%#lx)", hr);
    hr = IDirectMusicTrack_GetParam(track, &GUID_Play_Marker, 0, &next, &mark);
    CHECKF(hr == S_OK && mark == 50, "marker play marker at 0 is 50 (%#lx, %ld)", hr, mark);
    hr = IDirectMusicTrack_GetParam(track, &GUID_Play_Marker, 60, &next, &mark);
    CHECKF(hr == S_FALSE, "marker play marker after the last one is S_FALSE (%#lx)", hr);
    hr = IDirectMusicTrack_GetParam(track, &GUID_Valid_Start_Time, 0, NULL, NULL);
    CHECKF(hr == E_POINTER, "marker GetParam without data is E_POINTER (%#lx)", hr);

    copy = clone(track, 30, 100);
    hr = IDirectMusicTrack_GetParam(copy, &GUID_Valid_Start_Time, 0, &next, &mark);
    CHECKF(hr == S_OK && mark == 50, "marker clone(30, 100) has the valid start at 50 (%#lx, %ld)", hr, mark);
    hr = IDirectMusicTrack_GetParam(copy, &GUID_Play_Marker, 0, &next, &mark);
    CHECKF(hr == S_OK && mark == 20, "marker clone(30, 100) has the play marker at 20 (%#lx, %ld)", hr, mark);
    IDirectMusicTrack_Release(copy);
    IDirectMusicTrack_Release(track);

    track = create_track(&CLSID_DirectMusicMarkerTrack);
    a = begin("LIST", "XXXX");
    end(a);
    hr = load_track(track);
    CHECKF(hr == DMUS_E_UNSUPPORTED_STREAM, "marker Load of another list fails (%#lx)", hr);
    IDirectMusicTrack_Release(track);
}

static void test_sequence(void)
{
    DMUS_IO_SEQ_ITEM items[] = {{10, 5, 2, 0, 0xb0, 7, 100}, {40, 20, 2, 0, 0x90, 60, 99}};
    IDirectMusicTrack *track = create_track(&CLSID_DirectMusicSeqTrack), *copy;
    HRESULT hr;
    int a;

    a = begin("seqt", NULL);
    array_chunk("evtl", items, sizeof(items[0]), 2);
    end(a);
    hr = load_track(track);
    CHECKF(hr == S_OK, "seq Load (%#lx)", hr);

    hr = play(track, 0, 100, 500);
    CHECKF(hr == S_OK, "seq Play (%#lx)", hr);
    EXPECT_COUNT(2, "seq Play(0, 100)");
    EXPECT_MSG(0, DMUS_PMSGT_MIDI, 510);
    CHECKF(msg_count && msgs[0].midi[0] == 0xb0 && msgs[0].midi[1] == 7 && msgs[0].midi[2] == 100 && msgs[0].pchannel == 2, "seq message 0 is the controller change");
    EXPECT_MSG(1, DMUS_PMSGT_NOTE, 540);
    CHECKF(msg_count > 1 && msgs[1].midi[1] == 60 && msgs[1].midi[2] == 99 && msgs[1].len == 20, "seq message 1 is the note");

    copy = clone(track, 30, 100);
    play(copy, 0, 100, 0);
    EXPECT_COUNT(1, "seq clone(30, 100) Play");
    EXPECT_MSG(0, DMUS_PMSGT_NOTE, 10);
    IDirectMusicTrack_Release(copy);
    copy = clone(track, 0, 30);
    play(copy, 0, 100, 0);
    EXPECT_COUNT(1, "seq clone(0, 30) Play");
    EXPECT_MSG(0, DMUS_PMSGT_MIDI, 10);
    IDirectMusicTrack_Release(copy);
    IDirectMusicTrack_Release(track);
}

static void test_paramcontrol(void)
{
    DMUS_IO_PARAMCONTROLTRACK_OBJECTHEADER object = {0, {0}, 2, 1, 0, {0x1234}, 0};
    DMUS_IO_PARAMCONTROLTRACK_PARAMHEADER param = {0, 2};
    DMUS_IO_PARAMCONTROLTRACK_CURVEINFO curves[2] = {{10, 20, 0.0f, 1.0f, 1, 0}, {100, 200, 1.0f, 0.0f, 1, 0}};
    IDirectMusicTrack *track = create_track(&CLSID_DirectMusicParamControlTrack), *copy;
    HRESULT hr;
    int a, b, c;

    a = begin("LIST", "prmt");
    b = begin("LIST", "prol");
    chunk("proh", &object, sizeof(object));
    c = begin("LIST", "prpl");
    chunk("prph", &param, sizeof(param));
    array_chunk("prcc", curves, sizeof(curves[0]), 2);
    end(c);
    end(b);
    end(a);
    hr = load_track(track);
    CHECKF(hr == S_OK, "paramcontrol Load (%#lx)", hr);

    hr = IDirectMusicTrack_Play(track, NULL, 0, 100, 0, 0, perf, NULL, 1);
    CHECKF(hr == S_OK, "paramcontrol Play (%#lx)", hr);
    copy = clone(track, 0, 50);
    if (copy) IDirectMusicTrack_Release(copy);
    IDirectMusicTrack_Release(track);

    /* a curve array with a wrong element size is refused */
    track = create_track(&CLSID_DirectMusicParamControlTrack);
    a = begin("LIST", "prmt");
    b = begin("LIST", "prol");
    chunk("proh", &object, sizeof(object));
    c = begin("LIST", "prpl");
    chunk("prph", &param, sizeof(param));
    array_chunk("prcc", curves, sizeof(curves[0]) - 4, 1);
    end(c);
    end(b);
    end(a);
    hr = load_track(track);
    CHECKF(hr == DMUS_E_UNSUPPORTED_STREAM, "paramcontrol Load with a wrong curve size fails (%#lx)", hr);
    IDirectMusicTrack_Release(track);

    track = create_track(&CLSID_DirectMusicParamControlTrack);
    a = begin("LIST", "XXXX");
    end(a);
    hr = load_track(track);
    CHECKF(hr == DMUS_E_UNSUPPORTED_STREAM, "paramcontrol Load of another list fails (%#lx)", hr);
    {
        IPersistStream *ps;
        IDirectMusicTrack_QueryInterface(track, &IID_IPersistStream, (void **)&ps);
        hr = IPersistStream_Load(ps, NULL);
        CHECKF(hr == E_POINTER, "paramcontrol Load without stream is E_POINTER (%#lx)", hr);
        IPersistStream_Release(ps);
    }
    IDirectMusicTrack_Release(track);
}

static void test_wave(void)
{
    IDirectMusicTrack *track = create_track(&CLSID_DirectMusicWaveTrack), *copy;
    HRESULT hr;

    /* parameters that only change the track's state */
    hr = IDirectMusicTrack_SetParam(track, &GUID_Disable_Auto_Download, 0, NULL);
    CHECKF(hr == S_OK, "wave SetParam(GUID_Disable_Auto_Download) (%#lx)", hr);
    hr = IDirectMusicTrack_SetParam(track, &GUID_Enable_Auto_Download, 0, NULL);
    CHECKF(hr == S_OK, "wave SetParam(GUID_Enable_Auto_Download) (%#lx)", hr);
    hr = IDirectMusicTrack_SetParam(track, &GUID_Unload, 0, NULL);
    CHECKF(hr == S_OK, "wave SetParam(GUID_Unload) (%#lx)", hr);
    hr = IDirectMusicTrack_SetParam(track, &GUID_UnloadFromAudioPath, 0, NULL);
    CHECKF(hr == S_OK, "wave SetParam(GUID_UnloadFromAudioPath) (%#lx)", hr);
    hr = IDirectMusicTrack_SetParam(track, &GUID_Download, 0, perf);
    CHECKF(hr == S_OK, "wave SetParam(GUID_Download) with the performance (%#lx)", hr);
    copy = clone(track, 0, 10);
    if (copy) IDirectMusicTrack_Release(copy);
    IDirectMusicTrack_Release(track);
}

static void test_segtrigger(void)
{
    IDirectMusicTrack *track = create_track(&CLSID_DirectMusicSegmentTriggerTrack), *copy;
    HRESULT hr;
    int a, b;
    DMUS_IO_SEGMENT_TRACK_HEADER header = {0};

    /* an empty trigger track loads and plays nothing */
    a = begin("LIST", "segt");
    chunk("sgth", &header, sizeof(header));
    b = begin("LIST", "lsgl");
    end(b);
    end(a);
    hr = load_track(track);
    CHECKF(hr == S_OK, "segtrigger Load (%#lx)", hr);
    hr = play(track, 0, 100, 0);
    CHECKF(hr == S_OK, "segtrigger Play (%#lx)", hr);
    copy = clone(track, 0, 50);
    if (copy) IDirectMusicTrack_Release(copy);
    IDirectMusicTrack_Release(track);
}

static void test_graph(void)
{
    IDirectMusicGraph *graph;
    IDirectMusicTool *tool;
    IPersistStream *ps;
    IStream *stream;
    GUID guid = GUID_NULL;
    HRESULT hr;
    DWORD cookie, value;
    int a, b, c;
    struct { GUID id; LONG index; DWORD count; DWORD ckid; DWORD type; DWORD channels[1]; } header;

    hr = CoRegisterClassObject(&CLSID_ProbeTool, (IUnknown *)&probe_factory, CLSCTX_INPROC_SERVER, REGCLS_MULTIPLEUSE, &cookie);
    CHECKF(hr == S_OK, "register the probe tool class (%#lx)", hr);

    hr = CoCreateInstance(&CLSID_DirectMusicGraph, NULL, CLSCTX_INPROC_SERVER, &IID_IDirectMusicGraph, (void **)&graph);
    CHECKF(hr == S_OK, "create a tool graph (%#lx)", hr);
    IDirectMusicGraph_QueryInterface(graph, &IID_IPersistStream, (void **)&ps);

    a = begin("RIFF", "DMTG");
    chunk("guid", &guid, sizeof(guid));
    b = begin("LIST", "toll");
    for (value = 0; value < 2; value++)
    {
        DWORD data = value ? 0x2222 : 0x1111;
        c = begin("RIFF", "DMTL");
        memset(&header, 0, sizeof(header));
        header.id = CLSID_ProbeTool;
        header.index = value;
        header.count = 1;
        header.ckid = mmioFOURCC('t', 'd', 'a', 't');
        header.channels[0] = 5;
        chunk("tolh", &header, sizeof(header));
        chunk("tdat", &data, sizeof(data));
        end(c);
    }
    end(b);
    end(a);
    stream = make_stream();
    hr = IPersistStream_Load(ps, stream);
    CHECKF(hr == S_OK, "graph Load of two tools (%#lx)", hr);
    IStream_Release(stream);

    for (value = 0; value < 2; value++)
    {
        tool = NULL;
        hr = IDirectMusicGraph_GetTool(graph, value, &tool);
        CHECKF(hr == S_OK, "graph holds tool %lu after Load (%#lx)", value, hr);
        if (hr == S_OK)
        {
            CHECKF(ptool_from_tool(tool)->value == (value ? 0x2222 : 0x1111), "graph tool %lu loaded its data (%#lx)", value, ptool_from_tool(tool)->value);
            IDirectMusicTool_Release(tool);
        }
    }
    hr = IDirectMusicGraph_GetTool(graph, 2, &tool);
    CHECKF(hr == DMUS_E_NOT_FOUND, "graph holds exactly two tools (%#lx)", hr);
    IPersistStream_Release(ps);
    IDirectMusicGraph_Release(graph);

    /* a header that names a class that does not exist */
    hr = CoCreateInstance(&CLSID_DirectMusicGraph, NULL, CLSCTX_INPROC_SERVER, &IID_IDirectMusicGraph, (void **)&graph);
    IDirectMusicGraph_QueryInterface(graph, &IID_IPersistStream, (void **)&ps);
    a = begin("RIFF", "DMTG");
    b = begin("LIST", "toll");
    c = begin("RIFF", "DMTL");
    memset(&header, 0, sizeof(header));
    header.id = guid;
    header.id.Data1 = 0x7a2f0c99;
    header.count = 1;
    header.ckid = mmioFOURCC('t', 'd', 'a', 't');
    chunk("tolh", &header, sizeof(header));
    value = 1;
    chunk("tdat", &value, sizeof(value));
    end(c);
    end(b);
    end(a);
    stream = make_stream();
    hr = IPersistStream_Load(ps, stream);
    CHECKF(FAILED(hr), "graph Load of an unknown tool class fails (%#lx)", hr);
    IStream_Release(stream);
    hr = IDirectMusicGraph_GetTool(graph, 0, &tool);
    CHECKF(hr == DMUS_E_NOT_FOUND, "graph holds no tool after the failed Load (%#lx)", hr);
    a = begin("RIFF", "XXXX");
    end(a);
    stream = make_stream();
    hr = IPersistStream_Load(ps, stream);
    CHECKF(hr == DMUS_E_CHUNKNOTFOUND, "graph Load of another form is DMUS_E_CHUNKNOTFOUND (%#lx)", hr);
    IStream_Release(stream);
    hr = IPersistStream_Load(ps, NULL);
    CHECKF(hr == E_POINTER, "graph Load without stream is E_POINTER (%#lx)", hr);
    IPersistStream_Release(ps);
    IDirectMusicGraph_Release(graph);
    CoRevokeClassObject(cookie);
}

int main(void)
{
    IDirectMusicGraph *graph;
    HRESULT hr;

    CoInitialize(NULL);

    hr = CoCreateInstance(&CLSID_DirectMusicPerformance, NULL, CLSCTX_INPROC_SERVER, &IID_IDirectMusicPerformance, (void **)&perf);
    if (hr != S_OK)
    {
        printf("SKIP  no DirectMusic performance (%#lx)\n", hr);
        return 77;
    }
    hr = IDirectMusicPerformance_Init(perf, NULL, NULL, NULL);
    if (hr != S_OK)
    {
        printf("SKIP  performance Init failed (%#lx)\n", hr);
        return 77;
    }
    CoCreateInstance(&CLSID_DirectMusicGraph, NULL, CLSCTX_INPROC_SERVER, &IID_IDirectMusicGraph, (void **)&graph);
    IDirectMusicGraph_InsertTool(graph, &collector, NULL, 0, -1);
    IDirectMusicPerformance_SetGraph(perf, graph);
    IDirectMusicGraph_Release(graph);

    test_all_tracks();
    test_ex_params();
    test_sysex();
    test_lyrics();
    test_tempo();
    test_timesig();
    test_marker();
    test_sequence();
    test_paramcontrol();
    test_wave();
    test_segtrigger();
    test_graph();

    IDirectMusicPerformance_CloseDown(perf);
    IDirectMusicPerformance_Release(perf);
    CoUninitialize();
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
