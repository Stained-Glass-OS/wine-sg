/* dmime music value conversions, segment Compose, PlaySegmentEx / Stop / GetResolvedTime flags
 * (patches/sg/2913-2917), run by test/dmime-conv-gate.sh. Table-driven value checks.
 *
 *   dmime-conv-probe.exe */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <dmusici.h>
#include <dmusicf.h>
#include <dmplugin.h>
#include <dmerror.h>
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


/* ---- music value conversions ---- */
#define C_MAJOR_CHORD 0x91   /* root, third, fifth */
#define A_MINOR_CHORD 0x89
#define MAJOR_SCALE 0xab5

static DMUS_CHORD_KEY make_chord(BYTE chord_root, DWORD chord_pattern, BYTE scale_root)
{
    DMUS_CHORD_KEY chord = {{'T','e','s','t',0}};
    chord.bSubChordCount = 1;
    chord.SubChordList[0].dwChordPattern = chord_pattern;
    chord.SubChordList[0].dwScalePattern = MAJOR_SCALE;
    chord.SubChordList[0].dwLevels = 1;
    chord.SubChordList[0].bChordRoot = chord_root;
    chord.SubChordList[0].bScaleRoot = scale_root;
    chord.dwScale = MAJOR_SCALE;
    chord.bKey = scale_root;
    return chord;
}

#define MV(oct, pos, scale, acc) ((WORD)(((oct) << 12) | ((pos) << 8) | ((scale) << 4) | ((acc) & 0xf)))

static void test_music_values(void)
{
    static const struct
    {
        int chord;   /* 0 C major chord, 1 A minor chord, 2 C major chord with D as scale root */
        BYTE mode;
        WORD music;
        BYTE midi;
        HRESULT hr;
        const char *what;
    } to_midi[] =
    {
        {0, DMUS_PLAYMODE_NORMALCHORD, MV(5, 0, 0, 0), 60, S_OK, "chord root"},
        {0, DMUS_PLAYMODE_NORMALCHORD, MV(5, 1, 0, 0), 64, S_OK, "chord third"},
        {0, DMUS_PLAYMODE_NORMALCHORD, MV(5, 2, 0, 0), 67, S_OK, "chord fifth"},
        {0, DMUS_PLAYMODE_NORMALCHORD, MV(5, 2, 0, 1), 68, S_OK, "sharp fifth"},
        {0, DMUS_PLAYMODE_NORMALCHORD, MV(5, 2, 0, -1), 66, S_OK, "flat fifth"},
        {0, DMUS_PLAYMODE_NORMALCHORD, MV(5, 2, 0, -8), 59, S_OK, "accidental -8"},
        {0, DMUS_PLAYMODE_NORMALCHORD, MV(5, 2, 0, 7), 74, S_OK, "accidental +7"},
        {0, DMUS_PLAYMODE_NORMALCHORD, MV(5, 2, 3, 0), 67, S_OK, "chord intervals ignore the scale offset"},
        {0, DMUS_PLAYMODE_NORMALCHORD, MV(5, 3, 0, 0), 67, DMUS_S_OVER_CHORD, "position beyond the chord"},
        {0, DMUS_PLAYMODE_NORMALCHORD, MV(0, 0, 0, 0), 0, S_OK, "octave 0"},
        {0, DMUS_PLAYMODE_NORMALCHORD, MV(10, 2, 0, 0), 127, S_OK, "highest note"},
        {0, DMUS_PLAYMODE_NORMALCHORD, MV(10, 2, 0, 1), 116, DMUS_S_DOWN_OCTAVE, "above 127 moves down an octave"},
        {0, DMUS_PLAYMODE_NORMALCHORD, MV(0, 0, 0, -1), 11, DMUS_S_UP_OCTAVE, "below 0 moves up an octave"},
        {1, DMUS_PLAYMODE_NORMALCHORD, MV(5, 0, 0, 0), 69, S_OK, "A minor root"},
        {1, DMUS_PLAYMODE_NORMALCHORD, MV(5, 1, 0, 0), 72, S_OK, "A minor third"},
        {1, DMUS_PLAYMODE_NORMALCHORD, MV(5, 2, 0, 0), 76, S_OK, "A minor fifth"},
        {1, DMUS_PLAYMODE_NORMALCHORD, MV(10, 2, 0, 0), 124, DMUS_S_DOWN_OCTAVE, "A minor fifth in octave 10"},
        {0, DMUS_PLAYMODE_MELODIC, MV(5, 0, 0, 0), 60, S_OK, "scale offset 0"},
        {0, DMUS_PLAYMODE_MELODIC, MV(5, 0, 1, 0), 62, S_OK, "scale offset 1"},
        {0, DMUS_PLAYMODE_MELODIC, MV(5, 0, 2, 0), 64, S_OK, "scale offset 2"},
        {0, DMUS_PLAYMODE_MELODIC, MV(5, 0, 3, 0), 65, S_OK, "scale offset 3"},
        {0, DMUS_PLAYMODE_MELODIC, MV(5, 0, 7, 0), 72, S_OK, "scale offset 7"},
        {0, DMUS_PLAYMODE_MELODIC, MV(5, 3, 1, 1), 63, S_OK, "melodic ignores the chord position"},
        {1, DMUS_PLAYMODE_MELODIC, MV(5, 0, 1, 0), 71, S_OK, "scale offset from the A minor root"},
        {1, DMUS_PLAYMODE_MELODIC, MV(5, 0, 2, 0), 72, S_OK, "scale offset 2 from the A minor root"},
        {0, DMUS_PLAYMODE_ALWAYSPLAY, MV(5, 1, 1, 0), 65, S_OK, "third plus one scale step"},
        {0, DMUS_PLAYMODE_ALWAYSPLAY, MV(5, 2, 1, 1), 70, S_OK, "fifth plus one step, sharp"},
        {2, DMUS_PLAYMODE_PEDALPOINT, MV(5, 0, 0, 0), 62, S_OK, "key root"},
        {2, DMUS_PLAYMODE_PEDALPOINT, MV(5, 0, 1, 0), 64, S_OK, "key root plus one step"},
        {2, DMUS_PLAYMODE_PEDALPOINTCHORD, MV(5, 1, 0, 0), 66, S_OK, "key root with the chord intervals"},
        {1, DMUS_PLAYMODE_FIXEDTOCHORD, 60, 69, S_OK, "fixed to the chord root"},
        {1, DMUS_PLAYMODE_FIXEDTOCHORD, 100, 109, S_OK, "fixed to the chord root, high"},
        {1, DMUS_PLAYMODE_FIXEDTOCHORD, 120, 117, DMUS_S_DOWN_OCTAVE, "fixed to the chord root, over 127"},
        {1, DMUS_PLAYMODE_FIXEDTOCHORD, 200, 0, DMUS_E_CANNOT_CONVERT, "not a note"},
        {2, DMUS_PLAYMODE_FIXEDTOKEY, 60, 62, S_OK, "fixed to the key"},
        {0, DMUS_PLAYMODE_NONE, MV(5, 0, 0, 0), 0, DMUS_E_CANNOT_CONVERT, "play mode none"},
    };
    static const struct
    {
        int chord;
        BYTE mode;
        BYTE midi;
        WORD music;
        HRESULT hr;
        const char *what;
    } to_music[] =
    {
        {0, DMUS_PLAYMODE_NORMALCHORD, 60, MV(5, 0, 0, 0), S_OK, "root"},
        {0, DMUS_PLAYMODE_NORMALCHORD, 64, MV(5, 1, 0, 0), S_OK, "third"},
        {0, DMUS_PLAYMODE_NORMALCHORD, 67, MV(5, 2, 0, 0), S_OK, "fifth"},
        {0, DMUS_PLAYMODE_NORMALCHORD, 61, MV(5, 0, 0, 1), S_OK, "sharp root"},
        {0, DMUS_PLAYMODE_NORMALCHORD, 66, MV(5, 2, 0, -1), S_OK, "flat fifth"},
        {0, DMUS_PLAYMODE_NORMALCHORD, 12, MV(1, 0, 0, 0), S_OK, "octave 1"},
        {0, DMUS_PLAYMODE_NORMALCHORD, 127, MV(10, 2, 0, 0), S_OK, "highest note"},
        {1, DMUS_PLAYMODE_NORMALCHORD, 69, MV(5, 0, 0, 0), S_OK, "A minor root"},
        {1, DMUS_PLAYMODE_NORMALCHORD, 72, MV(5, 1, 0, 0), S_OK, "A minor third"},
        {1, DMUS_PLAYMODE_NORMALCHORD, 3, MV(0, 0, 0, -6), S_OK, "below the root"},
        {1, DMUS_PLAYMODE_NORMALCHORD, 0, 0, DMUS_E_CANNOT_CONVERT, "out of accidental range"},
        {0, DMUS_PLAYMODE_MELODIC, 62, MV(5, 0, 1, 0), S_OK, "scale offset 1"},
        {0, DMUS_PLAYMODE_MELODIC, 72, MV(6, 0, 0, 0), S_OK, "the next octave is not written as scale offset 7"},
        {0, DMUS_PLAYMODE_MELODIC, 61, MV(5, 0, 0, 1), S_OK, "tie goes to the lower scale offset"},
        {1, DMUS_PLAYMODE_MELODIC, 71, MV(5, 0, 1, 0), S_OK, "A minor scale offset 1"},
        {0, DMUS_PLAYMODE_ALWAYSPLAY, 65, MV(5, 1, 1, 0), S_OK, "third plus one step"},
        {0, DMUS_PLAYMODE_ALWAYSPLAY, 67, MV(5, 2, 0, 0), S_OK, "fifth, no scale step"},
        {2, DMUS_PLAYMODE_PEDALPOINT, 64, MV(5, 0, 1, 0), S_OK, "key root plus one step"},
        {1, DMUS_PLAYMODE_FIXEDTOCHORD, 69, 60, S_OK, "fixed to the chord root"},
        {1, DMUS_PLAYMODE_FIXEDTOCHORD, 109, 100, S_OK, "fixed to the chord root, high"},
        {1, DMUS_PLAYMODE_FIXEDTOCHORD, 3, 6, DMUS_S_UP_OCTAVE, "fixed to the chord root, below the root"},
        {2, DMUS_PLAYMODE_FIXEDTOKEY, 62, 60, S_OK, "fixed to the key"},
        {0, DMUS_PLAYMODE_NONE, 60, 0, DMUS_E_CANNOT_CONVERT, "play mode none"},
    };
    DMUS_CHORD_KEY chords[3];
    unsigned int i;
    BYTE midi;
    WORD music;
    HRESULT hr;
    int mode_i;

    chords[0] = make_chord(0, C_MAJOR_CHORD, 0);
    chords[1] = make_chord(9, A_MINOR_CHORD, 0);
    chords[2] = make_chord(0, C_MAJOR_CHORD, 2);

    for (i = 0; i < ARRAY_SIZE_(to_midi); i++)
    {
        midi = 0xaa;
        hr = IDirectMusicPerformance8_MusicToMIDI(perf, to_midi[i].music, &chords[to_midi[i].chord], to_midi[i].mode, 0, &midi);
        CHECKF(hr == to_midi[i].hr && (FAILED(hr) || midi == to_midi[i].midi), "MusicToMIDI mode %#x, %s (%#lx, %u, expected %#lx, %u)",
                to_midi[i].mode, to_midi[i].what, hr, midi, to_midi[i].hr, to_midi[i].midi);
    }
    for (i = 0; i < ARRAY_SIZE_(to_music); i++)
    {
        music = 0xaaaa;
        hr = IDirectMusicPerformance8_MIDIToMusic(perf, to_music[i].midi, &chords[to_music[i].chord], to_music[i].mode, 0, &music);
        CHECKF(hr == to_music[i].hr && (FAILED(hr) || music == to_music[i].music), "MIDIToMusic mode %#x, %s (%#lx, %#x, expected %#lx, %#x)",
                to_music[i].mode, to_music[i].what, hr, music, to_music[i].hr, to_music[i].music);
    }

    /* what MIDIToMusic returns MusicToMIDI turns back into the same note, in the modes with intervals */
    for (mode_i = 0; mode_i < 3; mode_i++)
    {
        static const BYTE modes[] = {DMUS_PLAYMODE_NORMALCHORD, DMUS_PLAYMODE_MELODIC, DMUS_PLAYMODE_ALWAYSPLAY};
        int bad = 0, done = 0, c, note;

        for (c = 0; c < 3; c++)
            for (note = 0; note < 128; note++)
            {
                hr = IDirectMusicPerformance8_MIDIToMusic(perf, note, &chords[c], modes[mode_i], 0, &music);
                if (hr != S_OK) continue;
                done++;
                midi = 0xaa;
                hr = IDirectMusicPerformance8_MusicToMIDI(perf, music, &chords[c], modes[mode_i], 0, &midi);
                if (hr != S_OK || midi != note) bad++;
            }
        CHECKF(done > 300 && !bad, "round trip of mode %#x (%d notes, %d wrong)", modes[mode_i], done, bad);
    }

    /* level selection: the second sub chord (level 1) is an A minor chord */
    {
        DMUS_CHORD_KEY two = make_chord(0, C_MAJOR_CHORD, 0);
        two.bSubChordCount = 2;
        two.SubChordList[1] = two.SubChordList[0];
        two.SubChordList[1].dwChordPattern = A_MINOR_CHORD;
        two.SubChordList[1].bChordRoot = 9;
        two.SubChordList[0].dwLevels = 1;
        two.SubChordList[1].dwLevels = 2;
        hr = IDirectMusicPerformance8_MusicToMIDI(perf, MV(5, 0, 0, 0), &two, DMUS_PLAYMODE_NORMALCHORD, 1, &midi);
        CHECKF(hr == S_OK && midi == 69, "MusicToMIDI takes the sub chord of the level (%#lx, %u)", hr, midi);
        hr = IDirectMusicPerformance8_MusicToMIDI(perf, MV(5, 0, 0, 0), &two, DMUS_PLAYMODE_NORMALCHORD, 0, &midi);
        CHECKF(hr == S_OK && midi == 60, "MusicToMIDI level 0 (%#lx, %u)", hr, midi);
        hr = IDirectMusicPerformance8_MusicToMIDI(perf, MV(5, 0, 0, 0), &two, DMUS_PLAYMODE_NORMALCHORD, 5, &midi);
        CHECKF(hr == S_OK && midi == 60, "a level no sub chord claims uses the first one (%#lx, %u)", hr, midi);
        two.bSubChordCount = 0;
        hr = IDirectMusicPerformance8_MusicToMIDI(perf, MV(5, 0, 0, 0), &two, DMUS_PLAYMODE_NORMALCHORD, 0, &midi);
        CHECKF(hr == DMUS_E_CANNOT_CONVERT, "a chord without sub chords cannot convert (%#lx)", hr);
    }
    hr = IDirectMusicPerformance8_MusicToMIDI(perf, 0, NULL, DMUS_PLAYMODE_NORMALCHORD, 0, &midi);
    CHECKF(hr == E_POINTER, "MusicToMIDI without a chord is E_POINTER (%#lx)", hr);
    hr = IDirectMusicPerformance8_MIDIToMusic(perf, 0, NULL, DMUS_PLAYMODE_NORMALCHORD, 0, &music);
    CHECKF(hr == E_POINTER, "MIDIToMusic without a chord is E_POINTER (%#lx)", hr);
    hr = IDirectMusicPerformance8_MusicToMIDI(perf, 0, &chords[0], DMUS_PLAYMODE_NORMALCHORD, 0, NULL);
    CHECKF(hr == E_POINTER, "MusicToMIDI without result is E_POINTER (%#lx)", hr);
}

/* ---- helpers for the time tests ---- */
static IDirectMusicSegment8 *create_marker_segment(MUSIC_TIME length, const MUSIC_TIME *play, int play_count,
        const MUSIC_TIME *valid, int valid_count)
{
    IDirectMusicSegment8 *seg = create_segment(length);
    IDirectMusicTrack *track = create_track(&CLSID_DirectMusicMarkerTrack);
    int a;

    a = begin("LIST", "MARK");
    if (valid_count) array_chunk("vals", valid, sizeof(*valid), valid_count);
    if (play_count) array_chunk("play", play, sizeof(*play), play_count);
    end(a);
    load((IUnknown *)track);
    IDirectMusicSegment8_InsertTrack(seg, track, 1);
    IDirectMusicTrack_Release(track);
    return seg;
}

static MUSIC_TIME music_of(REFERENCE_TIME rt)
{
    MUSIC_TIME mt = 0;
    IDirectMusicPerformance8_ReferenceToMusicTime(perf, rt, &mt);
    return mt;
}
static REFERENCE_TIME ref_of(MUSIC_TIME mt)
{
    REFERENCE_TIME rt = 0;
    IDirectMusicPerformance8_MusicToReferenceTime(perf, mt, &rt);
    return rt;
}

static HRESULT play_primary(IDirectMusicSegment8 *seg, DWORD flags, INT64 start, IDirectMusicSegmentState **state)
{
    return IDirectMusicPerformance8_PlaySegmentEx(perf, (IUnknown *)seg, NULL, NULL, flags, start, state, NULL, NULL);
}

/* ---- GetResolvedTime with markers and the segment end, Stop at a time ---- */
static void test_resolve_stop(void)
{
    static const MUSIC_TIME markers[] = {5000, 20000};
    IDirectMusicSegment8 *primary = create_marker_segment(100000, markers, 2, NULL, 0), *seg = create_segment(100000);
    IDirectMusicSegmentState *pstate, *state;
    REFERENCE_TIME resolved, input;
    MUSIC_TIME start, now;
    HRESULT hr;
    int i;

    /* no primary segment plays: there is nothing to resolve to */
    input = ref_of(777777);
    hr = IDirectMusicPerformance8_GetResolvedTime(perf, input, &resolved, DMUS_TIME_RESOLVE_SEGMENTEND);
    CHECKF(hr == S_OK && resolved == input, "SEGMENTEND without a primary segment keeps the time (%#lx)", hr);
    hr = IDirectMusicPerformance8_GetResolvedTime(perf, input, &resolved, DMUS_TIME_RESOLVE_MARKER);
    CHECKF(hr == S_OK && resolved == input, "MARKER without a primary segment keeps the time (%#lx)", hr);

    {
        MUSIC_TIME next = 0, found = -1;
        hr = IDirectMusicSegment8_GetParam(primary, &GUID_Play_Marker, -1, DMUS_SEG_ANYTRACK, 100, &next, &found);
        CHECKF(hr == S_OK && found == 4900 && next == 19900, "the marker track answers GetParam(GUID_Play_Marker) (%#lx, %ld, next %ld)", hr, found, next);
    }
    hr = play_primary(primary, 0, 0, &pstate);
    CHECKF(hr == S_OK, "PlaySegmentEx of the primary segment with markers (%#lx)", hr);
    IDirectMusicSegmentState_GetStartTime(pstate, &start);

    {
        static const struct { MUSIC_TIME at; DWORD flags; MUSIC_TIME expect; const char *what; } table[] =
        {
            {100, DMUS_TIME_RESOLVE_SEGMENTEND, 100000, "SEGMENTEND is the end of the primary segment"},
            {70000, DMUS_TIME_RESOLVE_SEGMENTEND, 100000, "SEGMENTEND late in the segment"},
            {100, DMUS_TIME_RESOLVE_MARKER, 5000, "MARKER is the first marker"},
            {5000, DMUS_TIME_RESOLVE_MARKER, 5000, "a time on a marker stays there"},
            {5001, DMUS_TIME_RESOLVE_MARKER, 20000, "MARKER is the next marker"},
            {30000, DMUS_TIME_RESOLVE_MARKER, 30000, "MARKER after the last marker keeps the time"},
            {30000, DMUS_TIME_RESOLVE_MARKER | DMUS_TIME_RESOLVE_SEGMENTEND, 100000, "no marker left, the segment end"},
            {100, DMUS_TIME_RESOLVE_MARKER | DMUS_TIME_RESOLVE_SEGMENTEND, 5000, "a marker comes before the segment end"},
        };

        for (i = 0; i < ARRAY_SIZE_(table); i++)
        {
            input = ref_of(start + table[i].at);
            hr = IDirectMusicPerformance8_GetResolvedTime(perf, input, &resolved, table[i].flags);
            CHECKF(hr == S_OK && abs(music_of(resolved) - (start + table[i].expect)) <= 2, "GetResolvedTime: %s (%#lx, %ld, expected %ld)",
                    table[i].what, hr, music_of(resolved) - start, table[i].expect);
        }
    }

    /* Stop at a time: the segment plays on until then */
    hr = play(perf, seg, 0, 0, &state, NULL);
    IDirectMusicPerformance8_GetTime(perf, NULL, &now);
    hr = IDirectMusicPerformance8_Stop(perf, (IDirectMusicSegment *)seg, NULL, now + 1000, 0);
    CHECKF(hr == S_OK, "Stop with a music time (%#lx)", hr);
    hr = IDirectMusicPerformance8_IsPlaying(perf, NULL, state);
    CHECKF(hr == S_OK, "the segment still plays right after Stop with a future time (%#lx)", hr);
    Sleep(1700);
    hr = IDirectMusicPerformance8_IsPlaying(perf, NULL, state);
    CHECKF(hr == S_FALSE, "the segment stopped at the time (%#lx)", hr);
    IDirectMusicSegmentState_Release(state);

    hr = play(perf, seg, 0, 0, &state, NULL);
    IDirectMusicPerformance8_GetTime(perf, &input, NULL);
    hr = IDirectMusicPerformance8_StopEx(perf, (IUnknown *)state, input + 10000000, DMUS_SEGF_REFTIME);
    CHECKF(hr == S_OK, "StopEx with a reference time (%#lx)", hr);
    Sleep(300);
    hr = IDirectMusicPerformance8_IsPlaying(perf, NULL, state);
    CHECKF(hr == S_OK, "still playing before the reference time (%#lx)", hr);
    Sleep(1300);
    hr = IDirectMusicPerformance8_IsPlaying(perf, NULL, state);
    CHECKF(hr == S_FALSE, "stopped at the reference time (%#lx)", hr);
    IDirectMusicSegmentState_Release(state);

    /* the next measure of the primary segment (3072 ticks, two seconds at most) */
    hr = play(perf, seg, 0, 0, &state, NULL);
    hr = IDirectMusicPerformance8_Stop(perf, NULL, state, 0, DMUS_SEGF_MEASURE);
    CHECKF(hr == S_OK, "Stop on a measure (%#lx)", hr);
    hr = IDirectMusicPerformance8_IsPlaying(perf, NULL, state);
    CHECKF(hr == S_OK, "the segment plays until the measure ends (%#lx)", hr);
    Sleep(2600);
    hr = IDirectMusicPerformance8_IsPlaying(perf, NULL, state);
    CHECKF(hr == S_FALSE, "the segment stopped at the next measure (%#lx)", hr);
    IDirectMusicSegmentState_Release(state);

    /* all the segments, a segment state which ended before its stop time stays quiet */
    hr = play(perf, seg, 0, 0, &state, NULL);
    IDirectMusicPerformance8_GetTime(perf, NULL, &now);
    hr = IDirectMusicPerformance8_Stop(perf, NULL, NULL, now + 800, 0);
    CHECKF(hr == S_OK, "Stop of everything at a time (%#lx)", hr);
    hr = IDirectMusicPerformance8_IsPlaying(perf, NULL, state);
    CHECKF(hr == S_OK, "everything still plays right after it (%#lx)", hr);
    Sleep(1500);
    hr = IDirectMusicPerformance8_IsPlaying(perf, NULL, state);
    CHECKF(hr == S_FALSE, "everything stopped at the time (%#lx)", hr);
    IDirectMusicSegmentState_Release(state);

    IDirectMusicPerformance8_Stop(perf, NULL, NULL, 0, 0);
    IDirectMusicSegmentState_Release(pstate);
    IDirectMusicSegment8_Release(primary);
    IDirectMusicSegment8_Release(seg);
}

/* ---- a track which composes ---- */
struct ctrack
{
    IDirectMusicTrack8 IDirectMusicTrack8_iface;
    LONG ref;
    int composed;
};
static IUnknown *compose_context;
static DWORD compose_group;
static int compose_calls;
static int play_calls, dirty_play_calls, composed_play_calls;
static DWORD last_play_flags;
static IDirectMusicTrack8Vtbl ctrack_vtbl;

static struct ctrack *impl_ctrack(IDirectMusicTrack8 *iface) { return (struct ctrack *)iface; }
static struct ctrack *ctrack_create(int composed)
{
    struct ctrack *t = calloc(1, sizeof(*t));
    t->IDirectMusicTrack8_iface.lpVtbl = &ctrack_vtbl;
    t->ref = 1;
    t->composed = composed;
    return t;
}
static HRESULT WINAPI ct_QueryInterface(IDirectMusicTrack8 *iface, REFIID riid, void **out)
{
    if (IsEqualGUID(riid, &IID_IUnknown) || IsEqualGUID(riid, &IID_IDirectMusicTrack) || IsEqualGUID(riid, &IID_IDirectMusicTrack8))
    {
        *out = iface;
        InterlockedIncrement(&impl_ctrack(iface)->ref);
        return S_OK;
    }
    *out = NULL;
    return E_NOINTERFACE;
}
static ULONG WINAPI ct_AddRef(IDirectMusicTrack8 *iface) { return InterlockedIncrement(&impl_ctrack(iface)->ref); }
static ULONG WINAPI ct_Release(IDirectMusicTrack8 *iface)
{
    LONG ref = InterlockedDecrement(&impl_ctrack(iface)->ref);
    if (!ref) free(impl_ctrack(iface));
    return ref;
}
static HRESULT WINAPI ct_Init(IDirectMusicTrack8 *iface, IDirectMusicSegment *seg) { return S_OK; }
static HRESULT WINAPI ct_InitPlay(IDirectMusicTrack8 *iface, IDirectMusicSegmentState *st, IDirectMusicPerformance *p, void **data, DWORD id, DWORD flags) { *data = NULL; return S_OK; }
static HRESULT WINAPI ct_EndPlay(IDirectMusicTrack8 *iface, void *data) { return S_OK; }
static HRESULT WINAPI ct_Play(IDirectMusicTrack8 *iface, void *data, MUSIC_TIME s, MUSIC_TIME e, MUSIC_TIME o, DWORD f, IDirectMusicPerformance *p, IDirectMusicSegmentState *st, DWORD id)
{
    play_calls++;
    if (impl_ctrack(iface)->composed) composed_play_calls++;
    last_play_flags = f;
    if (f & DMUS_TRACKF_DIRTY) dirty_play_calls++;
    return S_OK;
}
static HRESULT WINAPI ct_GetParam(IDirectMusicTrack8 *iface, REFGUID type, MUSIC_TIME t, MUSIC_TIME *next, void *param) { return DMUS_E_GET_UNSUPPORTED; }
static HRESULT WINAPI ct_SetParam(IDirectMusicTrack8 *iface, REFGUID type, MUSIC_TIME t, void *param) { return DMUS_E_SET_UNSUPPORTED; }
static HRESULT WINAPI ct_IsParamSupported(IDirectMusicTrack8 *iface, REFGUID type) { return DMUS_E_TYPE_UNSUPPORTED; }
static HRESULT WINAPI ct_AddNotificationType(IDirectMusicTrack8 *iface, REFGUID type) { return S_OK; }
static HRESULT WINAPI ct_Clone(IDirectMusicTrack8 *iface, MUSIC_TIME s, MUSIC_TIME e, IDirectMusicTrack **out)
{
    *out = (IDirectMusicTrack *)&ctrack_create(impl_ctrack(iface)->composed)->IDirectMusicTrack8_iface;
    return S_OK;
}
static HRESULT WINAPI ct_PlayEx(IDirectMusicTrack8 *iface, void *d, REFERENCE_TIME s, REFERENCE_TIME e, REFERENCE_TIME o, DWORD f, IDirectMusicPerformance *p, IDirectMusicSegmentState *st, DWORD id) { return E_NOTIMPL; }
static HRESULT WINAPI ct_GetParamEx(IDirectMusicTrack8 *iface, REFGUID type, REFERENCE_TIME t, REFERENCE_TIME *next, void *param, void *d, DWORD f) { return E_NOTIMPL; }
static HRESULT WINAPI ct_SetParamEx(IDirectMusicTrack8 *iface, REFGUID type, REFERENCE_TIME t, void *param, void *d, DWORD f) { return E_NOTIMPL; }
static HRESULT WINAPI ct_Compose(IDirectMusicTrack8 *iface, IUnknown *context, DWORD group, IDirectMusicTrack **out)
{
    compose_calls++;
    compose_context = context;
    compose_group = group;
    *out = (IDirectMusicTrack *)&ctrack_create(1)->IDirectMusicTrack8_iface;
    return S_OK;
}
static HRESULT WINAPI ct_Join(IDirectMusicTrack8 *iface, IDirectMusicTrack *t, MUSIC_TIME j, IUnknown *c, DWORD g, IDirectMusicTrack **out) { return E_NOTIMPL; }
static IDirectMusicTrack8Vtbl ctrack_vtbl = {ct_QueryInterface, ct_AddRef, ct_Release, ct_Init, ct_InitPlay, ct_EndPlay, ct_Play,
        ct_GetParam, ct_SetParam, ct_IsParamSupported, ct_AddNotificationType, ct_AddNotificationType, ct_Clone, ct_PlayEx,
        ct_GetParamEx, ct_SetParamEx, ct_Compose, ct_Join};

static int composed_of(IDirectMusicSegment *seg, DWORD index)
{
    IDirectMusicTrack *track;
    int ret = -1;
    if (IDirectMusicSegment_GetTrack(seg, &GUID_NULL, -1, index, &track) == S_OK)
    {
        if (((IDirectMusicTrack8 *)track)->lpVtbl == &ctrack_vtbl) ret = impl_ctrack((IDirectMusicTrack8 *)track)->composed;
        IDirectMusicTrack_Release(track);
    }
    return ret;
}
static int track_count(IDirectMusicSegment *seg)
{
    IDirectMusicTrack *track;
    int n = 0;
    while (IDirectMusicSegment_GetTrack(seg, &GUID_NULL, -1, n, &track) == S_OK) { IDirectMusicTrack_Release(track); n++; }
    return n;
}
static IUnknown *unk_of(void *obj)
{
    IUnknown *u = NULL;
    IUnknown_QueryInterface((IUnknown *)obj, &IID_IUnknown, (void **)&u);
    if (u) IUnknown_Release(u);
    return u;
}

static void test_compose(void)
{
    static const MUSIC_TIME markers[] = {100};
    IDirectMusicSegment8 *tmpl = create_marker_segment(5000, markers, 1, NULL, 0), *from = create_segment(2000), *to = create_segment(3000);
    struct ctrack *ct = ctrack_create(0);
    IDirectMusicSegment *composed = (IDirectMusicSegment *)0xdeadbeef;
    MUSIC_TIME length = 0;
    HRESULT hr;

    IDirectMusicSegment8_InsertTrack(tmpl, (IDirectMusicTrack *)&ct->IDirectMusicTrack8_iface, 4);
    IDirectMusicTrack8_Release(&ct->IDirectMusicTrack8_iface);

    hr = IDirectMusicSegment8_Compose(tmpl, 0, (IDirectMusicSegment *)from, (IDirectMusicSegment *)to, NULL);
    CHECKF(hr == E_POINTER, "Compose without a result is E_POINTER (%#lx)", hr);

    hr = IDirectMusicSegment8_Compose(tmpl, 100, (IDirectMusicSegment *)from, (IDirectMusicSegment *)to, &composed);
    CHECKF(hr == S_OK && composed && composed != (IDirectMusicSegment *)tmpl, "Compose makes a new segment (%#lx)", hr);
    if (hr != S_OK) goto done;
    IDirectMusicSegment_GetLength(composed, &length);
    CHECKF(length == 5000, "the composed segment has the length of the template (%ld)", length);
    CHECKF(track_count(composed) == 2, "the composed segment has the template's tracks (%d)", track_count(composed));
    CHECKF(composed_of(composed, 0) == 1 || composed_of(composed, 1) == 1, "the track which composes was replaced by its result (%d, %d)", composed_of(composed, 0), composed_of(composed, 1));
    CHECKF(composed_of((IDirectMusicSegment *)tmpl, 0) == 0 || composed_of((IDirectMusicSegment *)tmpl, 1) == 0, "the template keeps its track (%d, %d)", composed_of((IDirectMusicSegment *)tmpl, 0), composed_of((IDirectMusicSegment *)tmpl, 1));
    CHECKF(compose_calls == 1 && compose_context == unk_of(to) && compose_group == 4, "the track composes with the target segment as context and its group (%d, %p, %lu)", compose_calls, compose_context, compose_group);
    IDirectMusicSegment_Release(composed);

    hr = IDirectMusicSegment8_Compose(tmpl, 0, (IDirectMusicSegment *)from, NULL, &composed);
    CHECKF(hr == S_OK && compose_context == unk_of(from), "without a target the source segment is the context (%#lx)", hr);
    if (hr == S_OK) IDirectMusicSegment_Release(composed);
    hr = IDirectMusicSegment8_Compose(tmpl, 0, NULL, NULL, &composed);
    CHECKF(hr == S_OK && compose_context == unk_of(tmpl), "without segments the template is the context (%#lx)", hr);
    if (hr == S_OK) IDirectMusicSegment_Release(composed);

done:
    IDirectMusicSegment8_Release(tmpl);
    IDirectMusicSegment8_Release(from);
    IDirectMusicSegment8_Release(to);
}

static void wait_mid_beat(void)
{
    MUSIC_TIME now;
    for (;;)
    {
        IDirectMusicPerformance8_GetTime(perf, NULL, &now);
        if (now % 768 >= 150 && now % 768 <= 450) return;
        Sleep(50);
    }
}

static void test_play_flags(void)
{
    static const MUSIC_TIME valid[] = {100, 500, 768, 1000, 3072};
    IDirectMusicSegment8 *a = create_segment(3000), *b = create_segment(1500), *c = create_segment(1000), *plain = create_segment(100000);
    IDirectMusicSegmentState *sa, *sb, *sc, *sp;
    MUSIC_TIME start, now, point;
    HRESULT hr;
    int i;

    /* DMUS_SEGF_QUEUE: after the primary segment, and after the ones queued before */
    hr = play_primary(a, 0, 0, &sa);
    IDirectMusicSegmentState_GetStartTime(sa, &start);
    hr = play_primary(b, DMUS_SEGF_QUEUE, 0, &sb);
    CHECKF(hr == S_OK, "PlaySegmentEx with DMUS_SEGF_QUEUE (%#lx)", hr);
    IDirectMusicSegmentState_GetStartTime(sb, &now);
    CHECKF(abs(now - (start + 3000)) <= 2, "a queued segment starts when the primary segment ends (%ld, expected %ld)", now, start + 3000);
    hr = play_primary(c, DMUS_SEGF_QUEUE, 0, &sc);
    IDirectMusicSegmentState_GetStartTime(sc, &now);
    CHECKF(abs(now - (start + 4500)) <= 2, "the next queued segment starts when the first queued one ends (%ld, expected %ld)", now, start + 4500);
    IDirectMusicSegmentState_Release(sc);
    IDirectMusicSegmentState_Release(sb);
    IDirectMusicPerformance8_Stop(perf, NULL, NULL, 0, 0);
    IDirectMusicSegmentState_Release(sa);

    IDirectMusicPerformance8_GetTime(perf, NULL, &now);
    hr = play_primary(b, DMUS_SEGF_QUEUE, 0, &sb);
    IDirectMusicSegmentState_GetStartTime(sb, &start);
    CHECKF(hr == S_OK && start >= now && start - now < 100, "a queued segment without a primary segment starts at once (%ld)", start - now);
    IDirectMusicSegmentState_Release(sb);
    IDirectMusicPerformance8_Stop(perf, NULL, NULL, 0, 0);

    /* DMUS_SEGF_TIMESIG_ALWAYS: boundaries without a primary segment */
    wait_mid_beat();
    IDirectMusicPerformance8_GetTime(perf, NULL, &now);
    hr = play(perf, c, DMUS_SEGF_BEAT, 0, &sc, NULL);
    IDirectMusicSegmentState_GetStartTime(sc, &start);
    CHECKF(hr == S_OK && start - now < 100, "a beat without a primary segment starts at once (%ld)", start - now);
    IDirectMusicSegmentState_Release(sc);
    hr = play(perf, c, DMUS_SEGF_BEAT | DMUS_SEGF_TIMESIG_ALWAYS, 0, &sc, NULL);
    IDirectMusicSegmentState_GetStartTime(sc, &start);
    CHECKF(hr == S_OK && start - now > 150 && (start + 1) % 768 <= 2, "DMUS_SEGF_TIMESIG_ALWAYS finds the beat without a primary segment (%ld, now %ld)", start, now);
    IDirectMusicSegmentState_Release(sc);
    IDirectMusicPerformance8_Stop(perf, NULL, NULL, 0, 0);

    /* valid start points */
    {
        static const struct { DWORD flags; MUSIC_TIME start_point; MUSIC_TIME expect; const char *what; } table[] =
        {
            {0, 0, 0, "no flag"},
            {DMUS_SEGF_VALID_START_TICK, 0, 100, "a valid start on any tick"},
            {DMUS_SEGF_VALID_START_GRID, 0, 768, "a valid start on a grid"},
            {DMUS_SEGF_VALID_START_BEAT, 0, 768, "a valid start on a beat"},
            {DMUS_SEGF_VALID_START_MEASURE, 0, 3072, "a valid start on a measure"},
            {DMUS_SEGF_VALID_START_BEAT, 800, 3072, "a valid start after the start point"},
            {DMUS_SEGF_VALID_START_BEAT | DMUS_SEGF_ALIGN, 0, 768, "ALIGN with a valid start"},
            {DMUS_SEGF_ALIGN, 0, 0, "ALIGN alone"},
            {DMUS_SEGF_VALID_START_MEASURE, 3500, 3500, "no valid start left"},
        };
        for (i = 0; i < ARRAY_SIZE_(table); i++)
        {
            IDirectMusicSegment8 *seg = create_marker_segment(6000, NULL, 0, valid, ARRAY_SIZE_(valid));
            IDirectMusicSegmentState8 *st8;

            if (table[i].start_point) IDirectMusicSegment8_SetStartPoint(seg, table[i].start_point);
            hr = play(perf, seg, table[i].flags, 0, &sc, NULL);
            point = -1;
            IDirectMusicSegmentState_QueryInterface(sc, &IID_IDirectMusicSegmentState8, (void **)&st8);
            IDirectMusicSegmentState8_GetStartPoint(st8, &point);
            CHECKF(hr == S_OK && point == table[i].expect, "valid start: %s (%#lx, %ld, expected %ld)", table[i].what, hr, point, table[i].expect);
            IDirectMusicSegmentState8_Release(st8);
            IDirectMusicSegmentState_Release(sc);
            IDirectMusicPerformance8_Stop(perf, NULL, NULL, 0, 0);
            IDirectMusicSegment8_Release(seg);
        }
    }

    /* DMUS_SEGF_INVALIDATE_PRI: the primary segment plays its tracks again from the start time */
    {
        struct ctrack *ct = ctrack_create(0);
        IDirectMusicSegment8 *prim = create_segment(100000);

        IDirectMusicSegment8_InsertTrack(prim, (IDirectMusicTrack *)&ct->IDirectMusicTrack8_iface, 1);
        IDirectMusicTrack8_Release(&ct->IDirectMusicTrack8_iface);
        play_calls = dirty_play_calls = 0;
        hr = play_primary(prim, 0, 0, &sp);
        Sleep(200);
        CHECKF(hr == S_OK && play_calls >= 1, "the primary segment's track played (%d)", play_calls);
        dirty_play_calls = 0;
        hr = play(perf, c, 0, 0, &sc, NULL);
        Sleep(200);
        CHECKF(dirty_play_calls == 0, "a secondary segment does not invalidate the primary one (%d)", dirty_play_calls);
        IDirectMusicSegmentState_Release(sc);
        hr = play(perf, c, DMUS_SEGF_INVALIDATE_PRI | DMUS_SEGF_NOINVALIDATE, 0, &sc, NULL);
        Sleep(200);
        CHECKF(hr == S_OK && dirty_play_calls == 1 && (last_play_flags & DMUS_TRACKF_DIRTY), "DMUS_SEGF_INVALIDATE_PRI plays the primary segment's track again (%d, %#lx)", dirty_play_calls, last_play_flags);
        IDirectMusicSegmentState_Release(sc);
        IDirectMusicPerformance8_Stop(perf, NULL, NULL, 0, 0);
        IDirectMusicSegmentState_Release(sp);
        IDirectMusicSegment8_Release(prim);
    }

    /* the segment state it comes from stops when the new segment starts */
    hr = play(perf, plain, 0, 0, &sa, NULL);
    IDirectMusicPerformance8_GetTime(perf, NULL, &now);
    hr = IDirectMusicPerformance8_PlaySegmentEx(perf, (IUnknown *)c, NULL, NULL, DMUS_SEGF_SECONDARY, now + 1000, &sc, (IUnknown *)sa, NULL);
    CHECKF(hr == S_OK, "PlaySegmentEx with a segment state to replace (%#lx)", hr);
    hr = IDirectMusicPerformance8_IsPlaying(perf, NULL, sa);
    CHECKF(hr == S_OK, "the segment state plays until the new segment starts (%#lx)", hr);
    Sleep(1700);
    hr = IDirectMusicPerformance8_IsPlaying(perf, NULL, sa);
    CHECKF(hr == S_FALSE, "the segment state stopped when the new segment started (%#lx)", hr);
    IDirectMusicSegmentState_Release(sc);
    IDirectMusicSegmentState_Release(sa);
    hr = play(perf, plain, 0, 0, &sa, NULL);
    hr = IDirectMusicPerformance8_PlaySegmentEx(perf, (IUnknown *)c, NULL, NULL, DMUS_SEGF_SECONDARY, 0, &sc, (IUnknown *)sa, NULL);
    hr = IDirectMusicPerformance8_IsPlaying(perf, NULL, sa);
    CHECKF(hr == S_FALSE, "a new segment which starts now replaces it at once (%#lx)", hr);
    IDirectMusicSegmentState_Release(sc);
    IDirectMusicSegmentState_Release(sa);
    hr = IDirectMusicPerformance8_PlaySegmentEx(perf, (IUnknown *)c, NULL, NULL, DMUS_SEGF_SECONDARY, 0, &sc, (IUnknown *)perf, NULL);
    CHECKF(hr == E_INVALIDARG, "a source which is neither a segment state nor an audio path is E_INVALIDARG (%#lx)", hr);
    IDirectMusicPerformance8_Stop(perf, NULL, NULL, 0, 0);

    /* flags which need nothing from us */
    hr = play(perf, c, DMUS_SEGF_USE_AUDIOPATH | DMUS_SEGF_NOINVALIDATE | DMUS_SEGF_ALIGN, 0, &sc, NULL);
    CHECKF(hr == S_OK, "USE_AUDIOPATH, NOINVALIDATE and ALIGN are accepted (%#lx)", hr);
    IDirectMusicSegmentState_Release(sc);
    IDirectMusicPerformance8_Stop(perf, NULL, NULL, 0, 0);

    IDirectMusicSegment8_Release(a);
    IDirectMusicSegment8_Release(b);
    IDirectMusicSegment8_Release(c);
    IDirectMusicSegment8_Release(plain);
}

static void test_autotransition(void)
{
    struct ctrack *ct = ctrack_create(0);
    IDirectMusicSegment8 *tmpl = create_segment(1500), *main_seg = create_segment(2000);
    IDirectMusicSegmentState *state;
    MUSIC_TIME now, start;
    HRESULT hr;

    IDirectMusicSegment8_InsertTrack(tmpl, (IDirectMusicTrack *)&ct->IDirectMusicTrack8_iface, 1);
    IDirectMusicTrack8_Release(&ct->IDirectMusicTrack8_iface);

    composed_play_calls = play_calls = compose_calls = 0;
    IDirectMusicPerformance8_GetTime(perf, NULL, &now);
    hr = IDirectMusicPerformance8_PlaySegmentEx(perf, (IUnknown *)main_seg, NULL, (IUnknown *)tmpl, DMUS_SEGF_SECONDARY | DMUS_SEGF_AUTOTRANSITION, 0, &state, NULL, NULL);
    CHECKF(hr == S_OK && state, "PlaySegmentEx with DMUS_SEGF_AUTOTRANSITION (%#lx)", hr);
    IDirectMusicSegmentState_GetStartTime(state, &start);
    CHECKF(start - now >= 1500 && start - now < 1600, "the segment starts when the transition ends (%ld after now)", start - now);
    Sleep(300);
    CHECKF(compose_calls == 1 && composed_play_calls >= 1, "the composed transition plays (%d composes, %d plays)", compose_calls, composed_play_calls);
    CHECKF(play_calls == composed_play_calls, "the template's own track does not play (%d, %d)", play_calls, composed_play_calls);
    IDirectMusicSegmentState_Release(state);
    IDirectMusicPerformance8_Stop(perf, NULL, NULL, 0, 0);

    IDirectMusicPerformance8_GetTime(perf, NULL, &now);
    hr = IDirectMusicPerformance8_PlaySegmentEx(perf, (IUnknown *)main_seg, NULL, NULL, DMUS_SEGF_SECONDARY | DMUS_SEGF_AUTOTRANSITION, 0, &state, NULL, NULL);
    IDirectMusicSegmentState_GetStartTime(state, &start);
    CHECKF(hr == S_OK && start - now < 100, "without a transition template the segment starts at once (%#lx, %ld)", hr, start - now);
    IDirectMusicSegmentState_Release(state);
    hr = IDirectMusicPerformance8_PlaySegmentEx(perf, (IUnknown *)main_seg, NULL, (IUnknown *)perf, DMUS_SEGF_SECONDARY | DMUS_SEGF_AUTOTRANSITION, 0, &state, NULL, NULL);
    CHECKF(hr == E_NOINTERFACE, "a transition which is not a segment is E_NOINTERFACE (%#lx)", hr);
    IDirectMusicPerformance8_GetTime(perf, NULL, &now);
    hr = IDirectMusicPerformance8_PlaySegmentEx(perf, (IUnknown *)main_seg, NULL, (IUnknown *)tmpl, DMUS_SEGF_SECONDARY, 0, &state, NULL, NULL);
    IDirectMusicSegmentState_GetStartTime(state, &start);
    CHECKF(hr == S_OK && start - now < 100, "the template is ignored without DMUS_SEGF_AUTOTRANSITION (%ld)", start - now);
    IDirectMusicSegmentState_Release(state);
    IDirectMusicPerformance8_Stop(perf, NULL, NULL, 0, 0);
    IDirectMusicSegment8_Release(tmpl);
    IDirectMusicSegment8_Release(main_seg);
}
/*TESTS_END*/

int main(int argc, char **argv)
{
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

    test_music_values();
    test_resolve_stop();
    test_compose();
    test_play_flags();
    test_autotransition();

    IDirectMusicPerformance8_CloseDown(perf);
    IDirectMusicPerformance8_Release(perf);
    CoUninitialize();
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
