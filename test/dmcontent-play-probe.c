/* dmstyle track playback (patches/sg/2954), run by test/dmcontent-play-gate.sh: a style with parts, patterns (a groove, a fill,
 * a motif) is built as RIFF data; the style, motif and audition tracks are played into a performance whose graph holds a
 * collecting tool, and the note and curve messages (times, pitches, channels, positions, variations) are compared with a table.
 * The command and chord tracks of a mock segment state drive the pattern and the pitches. */
#include "dmcontent-common.h"

#define MEASURE 3072
static const GUID part_a = {0xa0000001, 1, 1, {1, 0, 0, 0, 0, 0, 0, 1}};
static const GUID part_b = {0xa0000002, 1, 1, {1, 0, 0, 0, 0, 0, 0, 2}};
static const GUID part_c = {0xa0000003, 1, 1, {1, 0, 0, 0, 0, 0, 0, 3}};
#define MELODIC (DMUS_PLAYMODE_CHORD_ROOT | DMUS_PLAYMODE_SCALE_INTERVALS)
#define NORMALCHORD (DMUS_PLAYMODE_CHORD_ROOT | DMUS_PLAYMODE_CHORD_INTERVALS)
#define PEDALPOINT (DMUS_PLAYMODE_KEY_ROOT | DMUS_PLAYMODE_SCALE_INTERVALS)

static DMUS_IO_STYLENOTE note(MUSIC_TIME grid, DWORD var, MUSIC_TIME dur, short off, WORD value, BYTE vel, BYTE mode)
{
    DMUS_IO_STYLENOTE n = {grid, var, dur, off, value, vel, 1, 2, 3, 0, mode, 0};
    return n;
}

static void add_part(const GUID *id, BYTE mode, DWORD choice0, DWORD choice1, const DMUS_IO_STYLENOTE *notes, int nn,
        const DMUS_IO_STYLECURVE *curves, int nc)
{
    DMUS_IO_STYLEPART p = {{4, 4, 4}};
    int at, list;

    p.guidPartID = *id;
    p.wNbrMeasures = 1;
    p.bPlayModeFlags = mode;
    p.dwVariationChoices[0] = choice0;
    p.dwVariationChoices[1] = choice1;
    at = begin("LIST", "part");
    chunk("prth", &p, sizeof(p));
    list = begin("LIST", "UNFO");
    chunk("UNAM", L"p", 4);
    end(list);
    if (nn) array_chunk("note", notes, sizeof(*notes), nn);
    if (nc) array_chunk("crve", curves, sizeof(*curves), nc);
    end(at);
}

struct ref { const GUID *part; DWORD pch; BYTE lock, random; };
static void add_pattern(const WCHAR *name, BYTE bottom, BYTE top, WORD emb, const struct ref *refs, int nrefs)
{
    DMUS_IO_PATTERN p = {{4, 4, 4}, bottom, top, emb, 1, 0, 0, 0};
    int at, list, i, pref;

    at = begin("LIST", "pttn");
    chunk("ptnh", &p, sizeof(p));
    list = begin("LIST", "UNFO");
    chunk("UNAM", name, (lstrlenW(name) + 1) * sizeof(WCHAR));
    end(list);
    for (i = 0; i < nrefs; i++)
    {
        DMUS_IO_PARTREF r = {0};
        r.guidPartID = *refs[i].part;
        r.bVariationLockID = refs[i].lock;
        r.bRandomVariation = refs[i].random;
        r.dwPChannel = refs[i].pch;
        pref = begin("LIST", "pref");
        chunk("prfc", &r, sizeof(r));
        list = begin("LIST", "UNFO");
        chunk("UNAM", L"r", 4);
        end(list);
        end(pref);
    }
    end(at);
}

static IDirectMusicStyle8 *make_play_style(void)
{
    DMUS_IO_STYLE st = {{4, 4, 4}, 100.0};
    DMUS_IO_STYLENOTE a[] = {
        note(0, 1, 100, 0, 60, 90, 0),
        note(5, 1, 200, -10, 2, 80, MELODIC),
        note(8, 1, 300, 0, 0x1002, 70, NORMALCHORD),
        note(12, 1, 50, 0, 0x0012, 60, PEDALPOINT),
        note(14, 1, 40, 0, 0, 50, DMUS_PLAYMODE_NONE),
        note(2, 2, 60, 0, 62, 100, 0),
    };
    DMUS_IO_STYLECURVE curve = {4, 1, 384, 50, 5, 0, 127, 64, 3, 2, 7, 1, 0, 0};
    DMUS_IO_STYLENOTE b[] = {
        note(0, 1, 100, 0, 70, 100, 0), note(4, 1, 100, 0, 0, 100, MELODIC), note(8, 1, 100, 0, 0, 100, MELODIC),
    };
    DMUS_IO_STYLENOTE c[] = { note(0, 1, 10, 0, 50, 90, 0), note(0, 2, 10, 0, 51, 90, 0) };
    const struct ref groove[] = {{&part_a, 5, 0, 0}}, fill[] = {{&part_b, 6, 0, 0}}, motif[] = {{&part_a, 9, 0, 0}},
        lock[] = {{&part_a, 5, 1, 1}, {&part_c, 8, 1, 1}};
    IDirectMusicStyle8 *style = NULL;
    IPersistStream *ps;
    IStream *stream;
    HRESULT hr;
    int at, list;

    at = begin("RIFF", "DMST");
    chunk("styh", &st, sizeof(st));
    list = begin("LIST", "UNFO");
    chunk("UNAM", L"play", sizeof(L"play"));
    end(list);
    add_part(&part_a, MELODIC, 1, 1, a, 6, &curve, 1);
    add_part(&part_b, 0, 1, 0, b, 3, NULL, 0);
    add_part(&part_c, 0, 1, 1, c, 2, NULL, 0);
    add_pattern(L"groove", 1, 100, 0, groove, 1);
    add_pattern(L"fill", 1, 100, DMUS_EMBELLISHT_FILL, fill, 1);
    add_pattern(L"motif", 1, 100, DMUS_EMBELLISHT_MOTIF, motif, 1);
    add_pattern(L"locked", 200, 201, 0, lock, 2);
    end(at);
    stream = make_stream();
    hr = CoCreateInstance(&CLSID_DirectMusicStyle, NULL, CLSCTX_INPROC_SERVER, &IID_IDirectMusicStyle8, (void **)&style);
    CHECKF(hr == S_OK, "create a style (%#lx)", hr);
    IDirectMusicStyle8_QueryInterface(style, &IID_IPersistStream, (void **)&ps);
    hr = IPersistStream_Load(ps, stream);
    CHECKF(hr == S_OK, "load the style (%#lx)", hr);
    IPersistStream_Release(ps);
    IStream_Release(stream);
    return style;
}

/* ---- the expected messages ---- */
struct exp
{
    DWORD type;
    MUSIC_TIME time;
    DWORD pch;
    int midi;               /* notes */
    MUSIC_TIME duration;
    WORD music;
    BYTE vel, mode, level;
    WORD measure;
    BYTE beat, grid;
    short offset;
};
#define N(t, pch, midi, dur, music, vel, mode, measure, beat, grid, off) \
    {DMUS_PMSGT_NOTE, t, pch, midi, dur, music, vel, mode, 0, measure, beat, grid, off}

static const struct msg *find_msg(DWORD type, MUSIC_TIME time, DWORD pch)
{
    int i;
    for (i = 0; i < msg_count; i++)
        if (msgs[i].type == type && msgs[i].mtTime == time && msgs[i].pchannel == pch) return &msgs[i];
    return NULL;
}

static int measure_shift; /* measures the pattern was moved by a later style start */
static void expect_notes(const char *tag, const struct exp *rows, int count, MUSIC_TIME shift)
{
    int i;

    CHECKF(msg_count == count, "%s sends %d messages (got %d)", tag, count, msg_count);
    for (i = 0; i < count; i++)
    {
        const struct exp *e = rows + i;
        const struct msg *m = find_msg(e->type, e->time + shift, e->pch);

        if (!m) { CHECKF(0, "%s: message %d at %ld on channel %lu is sent", tag, i, e->time + shift, e->pch); continue; }
        if (e->type == DMUS_PMSGT_CURVE)
        {
            CHECKF(m->duration == 384 && m->reset_duration == 50 && m->start_value == 0 && m->end_value == 127
                    && m->reset_value == 64 && m->curve_type == 3 && m->shape == 2 && m->cc == 7 && m->curve_flags == 1
                    && m->offset == 5 && m->measure == e->measure + measure_shift && m->beat == e->beat && m->grid == e->grid
                    && m->track_id == 7 && (m->flags & DMUS_PMSGF_MUSICTIME),
                    "%s: curve at %ld: duration %ld reset %ld values %d..%d/%d type %d shape %d cc %d flags %d offset %d pos %d.%d.%d track %lu flags %#lx",
                    tag, m->mtTime, m->duration, m->reset_duration, m->start_value, m->end_value, m->reset_value,
                    m->curve_type, m->shape, m->cc, m->curve_flags, m->offset, m->measure, m->beat, m->grid, m->track_id, m->flags);
            continue;
        }
        CHECKF(m->midi_value == e->midi && m->duration == e->duration && m->music == e->music && m->velocity == e->vel
                && m->play_mode == e->mode && m->measure == e->measure + measure_shift && m->beat == e->beat && m->grid == e->grid
                && m->offset == e->offset && m->note_flags == DMUS_NOTEF_NOTEON && m->track_id == 7
                && m->group == (DWORD)-1 && (m->flags & DMUS_PMSGF_MUSICTIME),
                "%s: note at %ld ch %lu: midi %d (want %d) duration %ld (%ld) music %#x (%#x) velocity %d (%d) mode %d (%d) pos %d.%d.%d (%d.%d.%d) offset %d (%d) flags %#x track %lu",
                tag, m->mtTime, m->pchannel, m->midi_value, e->midi, m->duration, e->duration, m->music, e->music, m->velocity,
                e->vel, m->play_mode, e->mode, m->measure, m->beat, m->grid, e->measure + measure_shift, e->beat, e->grid, m->offset,
                e->offset, m->note_flags, m->track_id);
    }
}

/* variation 0 of the part A in the default chord (C major, scale C major), starting at 0 */
static const struct exp var0[] = {
    N(0, 5, 60, 100, 60, 90, 0, 0, 0, 0, 0),
    N(950, 5, 40, 200, 2, 80, MELODIC, 0, 1, 1, -10),
    N(1536, 5, 55, 300, 0x1002, 70, NORMALCHORD, 0, 2, 0, 0),
    N(2304, 5, 41, 50, 0x0012, 60, PEDALPOINT, 0, 3, 0, 0),
    N(2688, 5, 36, 40, 0, 50, MELODIC, 0, 3, 2, 0),
    {DMUS_PMSGT_CURVE, 773, 5, 0, 0, 0, 0, 0, 0, 0, 1, 0, 5},
};

static IDirectMusicTrack *make_style_track(IDirectMusicStyle8 *style, MUSIC_TIME at, DWORD seed)
{
    IDirectMusicTrack *track = create_track(&CLSID_DirectMusicStyleTrack);
    HRESULT hr = IDirectMusicTrack_SetParam(track, &GUID_IDirectMusicStyle, at, style);
    CHECKF(hr == S_OK, "style track: SetParam(style at %ld) (%#lx)", at, hr);
    if (seed) IDirectMusicTrack_SetParam(track, &GUID_SeedVariations, 0, &seed);
    return track;
}

static HRESULT play_state(IDirectMusicTrack *track, MUSIC_TIME start, MUSIC_TIME end, MUSIC_TIME offset,
        IDirectMusicSegmentState *state)
{
    msg_count = 0;
    return IDirectMusicTrack_Play(track, NULL, start, end, offset, 0, perf, state, 7);
}

/* ---- a segment state whose segment holds a command and a chord track ---- */
static IDirectMusicSegment *mock_segment;
static HRESULT WINAPI ss_QueryInterface(IDirectMusicSegmentState *iface, REFIID riid, void **out)
{
    if (IsEqualGUID(riid, &IID_IUnknown) || IsEqualGUID(riid, &IID_IDirectMusicSegmentState)) { *out = iface; return S_OK; }
    *out = NULL;
    return E_NOINTERFACE;
}
static ULONG WINAPI ss_AddRef(IDirectMusicSegmentState *iface) { return 2; }
static ULONG WINAPI ss_Release(IDirectMusicSegmentState *iface) { return 1; }
static HRESULT WINAPI ss_GetRepeats(IDirectMusicSegmentState *iface, DWORD *r) { *r = 0; return S_OK; }
static HRESULT WINAPI ss_GetSegment(IDirectMusicSegmentState *iface, IDirectMusicSegment **seg)
{
    *seg = mock_segment;
    IDirectMusicSegment_AddRef(mock_segment);
    return S_OK;
}
static HRESULT WINAPI ss_GetStartTime(IDirectMusicSegmentState *iface, MUSIC_TIME *t) { *t = 0; return S_OK; }
static HRESULT WINAPI ss_GetSeek(IDirectMusicSegmentState *iface, MUSIC_TIME *t) { *t = 0; return S_OK; }
static HRESULT WINAPI ss_GetStartPoint(IDirectMusicSegmentState *iface, MUSIC_TIME *t) { *t = 0; return S_OK; }
static const IDirectMusicSegmentStateVtbl ss_vtbl = {ss_QueryInterface, ss_AddRef, ss_Release, ss_GetRepeats, ss_GetSegment,
        ss_GetStartTime, ss_GetSeek, ss_GetStartPoint};
static IDirectMusicSegmentState mock_state = {(IDirectMusicSegmentStateVtbl *)&ss_vtbl};

static void build_segment(void)
{
    static const DMUS_IO_COMMAND cmds[] = {
        {0, 0, 0, DMUS_COMMANDT_FILL, 50, 0, 0},
        {MEASURE, 1, 0, DMUS_COMMANDT_GROOVE, 10, 0, 0},
    };
    static const DMUS_IO_SUBCHORD subs[] = {{0x91, 0xab5, 0, 1, 2, 0}, {0x91, 0xab5, 0, 1, 7, 0}};
    DMUS_IO_CHORD c1 = {L"D", 0, 0, 0, 0}, c2 = {L"G", 1536, 0, 2, 0};
    IDirectMusicTrack *track;
    DWORD scale = 0xab5, size, count;
    int list, at, i;
    HRESULT hr;

    CoCreateInstance(&CLSID_DirectMusicSegment, NULL, CLSCTX_INPROC_SERVER, &IID_IDirectMusicSegment, (void **)&mock_segment);
    IDirectMusicSegment_SetLength(mock_segment, 100000);

    track = create_track(&CLSID_DirectMusicCommandTrack);
    array_chunk("cmnd", cmds, sizeof(cmds[0]), 2);
    hr = load_track(track);
    CHECKF(hr == S_OK, "command track loads (%#lx)", hr);
    IDirectMusicSegment_InsertTrack(mock_segment, track, 1);
    IDirectMusicTrack_Release(track);

    track = create_track(&CLSID_DirectMusicChordTrack);
    list = begin("LIST", "cord");
    chunk("crdh", &scale, 4);
    for (i = 0; i < 2; i++)
    {
        const DMUS_IO_CHORD *c = i ? &c2 : &c1;
        at = begin("crdb", NULL);
        size = sizeof(*c); put(&size, 4); put(c, size);
        count = 1; put(&count, 4);
        size = sizeof(DMUS_IO_SUBCHORD); put(&size, 4); put(&subs[i], size);
        end(at);
    }
    end(list);
    hr = load_track(track);
    CHECKF(hr == S_OK, "chord track loads (%#lx)", hr);
    IDirectMusicSegment_InsertTrack(mock_segment, track, 1);
    IDirectMusicTrack_Release(track);
}

static void test_style_track(IDirectMusicStyle8 *style)
{
    static const struct exp var1_note = N(3456, 5, 62, 60, 62, 100, 0, 1, 0, 2, 0);
    static const struct exp fill_rows[] = {
        N(0, 6, 70, 100, 70, 100, 0, 0, 0, 0, 0),
        N(768, 6, 38, 100, 0, 100, MELODIC, 0, 1, 0, 0),   /* chord root 2 */
        N(1536, 6, 43, 100, 0, 100, MELODIC, 0, 2, 0, 0),  /* the chord changes to root 7 */
    };
    struct exp rows[16];
    IDirectMusicTrack *track;
    MUSIC_TIME shift;
    HRESULT hr;
    int i;

    track = make_style_track(style, 0, 0);
    hr = play_state(track, 0, MEASURE, 0, NULL);
    CHECKF(hr == S_OK, "style track: Play(0, one measure) (%#lx)", hr);
    expect_notes("style track: one measure, default command and chord", var0, ARRAY_SIZE_(var0), 0);
    CHECKF(msg_count && find_msg(DMUS_PMSGT_NOTE, 0, 5) && find_msg(DMUS_PMSGT_NOTE, 0, 5)->velocity == 90
            && find_msg(DMUS_PMSGT_NOTE, 0, 5)->time_range == 1 && find_msg(DMUS_PMSGT_NOTE, 0, 5)->dur_range == 2
            && find_msg(DMUS_PMSGT_NOTE, 0, 5)->vel_range == 3, "style track: the ranges of the note are copied");

    hr = play_state(track, 0, 2 * MEASURE, 0, NULL);
    for (i = 0; i < ARRAY_SIZE_(var0); i++) rows[i] = var0[i];
    rows[i++] = var1_note;
    CHECKF(hr == S_OK, "style track: Play(0, two measures) (%#lx)", hr);
    expect_notes("style track: two measures (the second pattern cycle uses variation 1)", rows, i, 0);

    shift = 10000;
    hr = play_state(track, 0, MEASURE, shift, NULL);
    CHECKF(hr == S_OK, "style track: Play with a time offset (%#lx)", hr);
    expect_notes("style track: time offset 10000", var0, ARRAY_SIZE_(var0), shift);

    hr = play_state(track, 1000, 2400, 0, NULL);
    CHECKF(hr == S_OK, "style track: Play(1000, 2400) (%#lx)", hr);
    rows[0] = var0[2]; rows[1] = var0[3];
    expect_notes("style track: a range inside the pattern", rows, 2, 0);

    hr = play_state(track, 960, 1536, 0, NULL);
    expect_notes("style track: the range includes its start and excludes its end", var0 + 1, 1, 0);

    hr = play_state(track, 5000, 5000, 0, NULL);
    CHECKF(hr == S_OK && msg_count == 0, "style track: an empty range sends nothing (%#lx, %d)", hr, msg_count);
    IDirectMusicTrack_Release(track);

    track = make_style_track(style, MEASURE, 0);
    hr = play_state(track, 0, 2 * MEASURE, 0, NULL);
    CHECKF(hr == S_OK, "style track: style at 3072 (%#lx)", hr);
    measure_shift = 1;
    expect_notes("style track: nothing before the style starts, then its pattern", var0, ARRAY_SIZE_(var0), MEASURE);
    measure_shift = 0;
    IDirectMusicTrack_Release(track);

    /* the command track and the chord track of the segment */
    track = make_style_track(style, 0, 0);
    hr = play_state(track, 0, 2 * MEASURE, 0, &mock_state);
    CHECKF(hr == S_OK, "style track: Play with a segment state (%#lx)", hr);
    for (i = 0; i < ARRAY_SIZE_(fill_rows); i++) rows[i] = fill_rows[i];
    rows[i++] = var1_note;
    expect_notes("style track: a fill, then a groove, with the chords of the chord track", rows, i, 0);
    IDirectMusicTrack_Release(track);
}

static void test_locks(IDirectMusicStyle8 *style)
{
    IDirectMusicTrack *track = make_style_track(style, 0, 1234);
    IDirectMusicTrack *cmd = create_track(&CLSID_DirectMusicCommandTrack);
    IDirectMusicSegment *saved = mock_segment;
    DMUS_IO_COMMAND cmds[1] = {{0, 0, 0, DMUS_COMMANDT_GROOVE, 200, 0, 0}};
    int cycle, a_var1 = 0, same = 0, cycles = 12;
    HRESULT hr;

    array_chunk("cmnd", cmds, sizeof(cmds[0]), 1);
    load_track(cmd);
    /* a segment whose command track asks for groove level 200, which only the pattern with the two locked parts has */
    CoCreateInstance(&CLSID_DirectMusicSegment, NULL, CLSCTX_INPROC_SERVER, &IID_IDirectMusicSegment, (void **)&mock_segment);
    IDirectMusicSegment_SetLength(mock_segment, 100000);
    IDirectMusicSegment_InsertTrack(mock_segment, cmd, 1);

    hr = play_state(track, 0, cycles * MEASURE, 0, &mock_state);
    CHECKF(hr == S_OK, "locked variations: Play (%#lx)", hr);
    for (cycle = 0; cycle < cycles; cycle++)
    {
        MUSIC_TIME base = cycle * MEASURE;
        const struct msg *m = find_msg(DMUS_PMSGT_NOTE, base, 8);
        int a = find_msg(DMUS_PMSGT_NOTE, base + 384, 5) != NULL;
        int c = m && m->midi_value == 51;

        a_var1 += a;
        same += a == c;
    }
    CHECKF(same == cycles, "locked variations: the two locked parts use the same variation in all %d cycles (%d)", cycles, same);
    CHECKF(a_var1 > 0 && a_var1 < cycles, "locked variations: both variations are chosen over %d cycles (%d use variation 1)", cycles, a_var1);
    msg_count = 0;
    IDirectMusicSegment_Release(mock_segment);
    mock_segment = saved;
    IDirectMusicTrack_Release(cmd);
    IDirectMusicTrack_Release(track);
}

static void test_motif(IDirectMusicStyle8 *style)
{
    IDirectMusicSegment *segment;
    IDirectMusicTrack *track;
    struct exp rows[16];
    HRESULT hr;
    int i;

    hr = IDirectMusicStyle8_GetMotif(style, (WCHAR *)L"motif", &segment);
    CHECKF(hr == S_OK, "GetMotif (%#lx)", hr);
    if (hr != S_OK) return;
    hr = IDirectMusicSegment_GetTrack(segment, &CLSID_DirectMusicMotifTrack, 0xffffffff, 0, &track);
    CHECKF(hr == S_OK, "the motif segment has a motif track (%#lx)", hr);
    if (hr != S_OK) return;

    hr = play_state(track, 0, MEASURE, 500, NULL);
    CHECKF(hr == S_OK, "motif track: Play (%#lx)", hr);
    for (i = 0; i < ARRAY_SIZE_(var0); i++) { rows[i] = var0[i]; rows[i].pch = 9; }
    expect_notes("motif track: the motif, time offset 500", rows, ARRAY_SIZE_(var0), 500);

    hr = play_state(track, 1500, MEASURE, 0, NULL);
    rows[0] = var0[2]; rows[0].pch = 9;
    rows[1] = var0[3]; rows[1].pch = 9;
    rows[2] = var0[4]; rows[2].pch = 9;
    expect_notes("motif track: a range of the motif", rows, 3, 0);
    IDirectMusicTrack_Release(track);
    IDirectMusicSegment_Release(segment);
}

static void test_audition(void)
{
    IDirectMusicTrack *track = create_track(&CLSID_DirectMusicAuditionTrack);
    HRESULT hr = play_state(track, 0, MEASURE, 0, NULL);

    CHECKF(hr == S_OK && msg_count == 0, "audition track: Play without data sends nothing (%#lx, %d)", hr, msg_count);
    IDirectMusicTrack_Release(track);
}

int main(void)
{
    IDirectMusicStyle8 *style;

    CoInitialize(NULL);
    if (!setup_perf()) return 77;

    style = make_play_style();
    build_segment();
    if (style)
    {
        test_style_track(style);
        test_locks(style);
        test_motif(style);
        IDirectMusicStyle8_Release(style);
    }
    test_audition();

    teardown_perf();
    CoUninitialize();
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
