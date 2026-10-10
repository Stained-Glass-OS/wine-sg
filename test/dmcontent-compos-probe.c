/* dmcompos (patches/sg/2963), run by test/dmcontent-compos-gate.sh: chord maps are loaded from RIFF data
 * (scale pattern), put into chord map tracks (SetParam, Load), cloned, joined and asked (also through the extended
 * methods); signpost tracks are loaded, saved, cloned and joined; ChangeChordMap changes the chord map and the scale
 * of a segment; the template object is created. Every answer is compared with the data.
 *
 *   dmcontent-compos-probe.exe */
#include "dmcontent-common.h"

static void chordmap_form(DWORD scale, const WCHAR *name)
{
    DMUS_IO_CHORDMAP header = {{0}};
    DWORD junk = 0x1234;
    int at, list;

    lstrcpyW(header.wszLoadName, name);
    header.dwScalePattern = scale;
    header.dwFlags = 0;
    at = begin("RIFF", "DMPR");
    list = begin("LIST", "UNFO");
    chunk("UNAM", name, (lstrlenW(name) + 1) * sizeof(WCHAR));
    end(list);
    chunk("perh", &header, sizeof(header));
    chunk("chdt", &junk, sizeof(junk));
    end(at);
}

static IDirectMusicChordMap *make_chordmap(DWORD scale, const WCHAR *name)
{
    IDirectMusicChordMap *map = NULL;
    IPersistStream *ps;
    IStream *stream;
    HRESULT hr;

    chordmap_form(scale, name);
    stream = make_stream();
    hr = CoCreateInstance(&CLSID_DirectMusicChordMap, NULL, CLSCTX_INPROC_SERVER, &IID_IDirectMusicChordMap, (void **)&map);
    CHECKF(hr == S_OK, "create a chord map (%#lx)", hr);
    IDirectMusicChordMap_QueryInterface(map, &IID_IPersistStream, (void **)&ps);
    hr = IPersistStream_Load(ps, stream);
    CHECKF(hr == S_OK, "load the chord map %ls (%#lx)", name, hr);
    IPersistStream_Release(ps);
    IStream_Release(stream);
    return map;
}

static ULONG refs(IUnknown *unk)
{
    IUnknown_AddRef(unk);
    return IUnknown_Release(unk);
}

static HRESULT join_tracks(IDirectMusicTrack *track, IDirectMusicTrack *other, MUSIC_TIME time, IDirectMusicTrack **result)
{
    IDirectMusicTrack8 *track8;
    HRESULT hr;
    IDirectMusicTrack_QueryInterface(track, &IID_IDirectMusicTrack8, (void **)&track8);
    hr = IDirectMusicTrack8_Join(track8, other, time, NULL, 0, result);
    IDirectMusicTrack8_Release(track8);
    return hr;
}

static void set_map(IDirectMusicTrack *track, IDirectMusicChordMap *map, MUSIC_TIME time)
{
    HRESULT hr = IDirectMusicTrack_SetParam(track, &GUID_IDirectMusicChordMap, time, map);
    CHECKF(hr == S_OK, "chord map track: SetParam at %ld (%#lx)", time, hr);
}

static void expect_map(IDirectMusicTrack *track, MUSIC_TIME time, IDirectMusicChordMap *map, MUSIC_TIME next, const char *tag)
{
    IDirectMusicChordMap *got = NULL;
    MUSIC_TIME got_next = 0x1234;
    ULONG before = map ? refs((IUnknown *)map) : 0;
    HRESULT hr = IDirectMusicTrack_GetParam(track, &GUID_IDirectMusicChordMap, time, &got_next, &got);

    CHECKF(hr == S_OK && got == map && got_next == next && refs((IUnknown *)map) == before + 1,
            "%s: chord map at %ld is the expected one, referenced, next %ld (%#lx, %p vs %p, next %ld)", tag, time, next, hr, got, map,
            got_next);
    if (got) IDirectMusicChordMap_Release(got);
}

static void test_chordmap(void)
{
    IDirectMusicChordMap *map;
    DMUS_OBJECTDESC desc = {sizeof(desc)};
    IDirectMusicObject *object;
    DWORD scale = 7;
    HRESULT hr;

    map = make_chordmap(0xab5ab5, L"mapA");
    if (!map) return;
    hr = IDirectMusicChordMap_GetScale(map, &scale);
    CHECKF(hr == S_OK && scale == 0xab5ab5, "chord map scale is the pattern of the 'perh' chunk (%#lx, %#lx)", hr, scale);
    hr = IDirectMusicChordMap_GetScale(map, NULL);
    CHECKF(hr == E_POINTER, "GetScale without pointer is E_POINTER (%#lx)", hr);
    IDirectMusicChordMap_QueryInterface(map, &IID_IDirectMusicObject, (void **)&object);
    hr = IDirectMusicObject_GetDescriptor(object, &desc);
    CHECKF(hr == S_OK && (desc.dwValidData & DMUS_OBJ_NAME) && !lstrcmpW(desc.wszName, L"mapA") && (desc.dwValidData & DMUS_OBJ_CLASS)
            && (desc.dwValidData & DMUS_OBJ_LOADED), "the loaded chord map has the name mapA, its class, and is loaded (%#lx, %#lx, %ls)", hr,
            desc.dwValidData, desc.wszName);
    IDirectMusicObject_Release(object);
    IDirectMusicChordMap_Release(map);

    hr = CoCreateInstance(&CLSID_DirectMusicChordMap, NULL, CLSCTX_INPROC_SERVER, &IID_IDirectMusicChordMap, (void **)&map);
    scale = 7;
    hr = IDirectMusicChordMap_GetScale(map, &scale);
    CHECKF(hr == S_OK && scale == 0, "a chord map which is not loaded has the scale zero (%#lx, %#lx)", hr, scale);
    IDirectMusicChordMap_Release(map);
}

static void test_chordmap_track(void)
{
    IDirectMusicChordMap *a = make_chordmap(1, L"A"), *b = make_chordmap(2, L"B"), *c = make_chordmap(3, L"C");
    IDirectMusicTrack *track = create_track(&CLSID_DirectMusicChordMapTrack), *copy, *join, *other;
    IDirectMusicTrack8 *track8;
    IDirectMusicChordMap *got = NULL;
    ULONG ra, rb;
    HRESULT hr;

    if (!track || !a || !b || !c) return;
    ra = refs((IUnknown *)a);
    rb = refs((IUnknown *)b);

    set_map(track, a, 0);
    set_map(track, b, 2000);
    CHECKF(refs((IUnknown *)a) == ra + 1 && refs((IUnknown *)b) == rb + 1, "chord map track references its chord maps");
    expect_map(track, 0, a, 2000, "chord map track");
    expect_map(track, 1000, a, 1000, "chord map track");
    expect_map(track, 2000, b, 0, "chord map track");
    expect_map(track, 5000, b, 0, "chord map track");
    set_map(track, c, 2000);
    expect_map(track, 2500, c, 0, "chord map track after a chord map set at the same time");
    CHECKF(refs((IUnknown *)b) == rb, "the replaced chord map is released (%lu, expected %lu)", refs((IUnknown *)b), rb);
    set_map(track, b, 2000);
    hr = IDirectMusicTrack_SetParam(track, &GUID_IDirectMusicChordMap, 0, NULL);
    CHECKF(hr == E_POINTER, "SetParam without a chord map is E_POINTER (%#lx)", hr);
    hr = IDirectMusicTrack_SetParam(track, &GUID_IDirectMusicChordMap, -5, a);
    CHECKF(hr == E_INVALIDARG, "SetParam at a negative time is E_INVALIDARG (%#lx)", hr);
    hr = IDirectMusicTrack_SetParam(track, &GUID_IDirectMusicChordMap, 5, track);
    CHECKF(hr == E_NOINTERFACE, "SetParam of an object which is no chord map is E_NOINTERFACE (%#lx)", hr);
    hr = IDirectMusicTrack_GetParam(track, &GUID_IDirectMusicChordMap, 0, NULL, NULL);
    CHECKF(hr == E_POINTER, "GetParam without data is E_POINTER (%#lx)", hr);

    /* Play has nothing to send */
    hr = play(track, 0, 3000, 0);
    CHECKF(hr == S_OK && msg_count == 0, "chord map track: Play is S_OK and sends nothing (%#lx, %d)", hr, msg_count);

    /* Clone: [1500, 3000) moved to zero, the chord map in effect at start comes first */
    copy = clone(track, 1500, 3000);
    if (copy)
    {
        expect_map(copy, 0, a, 500, "chord map clone [1500, 3000)");
        expect_map(copy, 600, b, 0, "chord map clone [1500, 3000)");
        IDirectMusicTrack_Release(copy);
    }
    hr = IDirectMusicTrack_Clone(track, 9, 3, &copy);
    CHECKF(hr == E_INVALIDARG, "Clone(9, 3) is E_INVALIDARG (%#lx)", hr);

    /* Join: this track before the join time, the other one from it */
    other = create_track(&CLSID_DirectMusicChordMapTrack);
    set_map(other, c, 0);
    set_map(other, a, 300);
    join = NULL;
    hr = join_tracks(track, other, 1000, &join);
    CHECKF(hr == S_OK && join, "chord map track: Join is S_OK (%#lx)", hr);
    if (join)
    {
        expect_map(join, 0, a, 1000, "chord map join");
        expect_map(join, 1000, c, 300, "chord map join");
        expect_map(join, 1300, a, 0, "chord map join");
        IDirectMusicTrack_Release(join);
    }
    hr = join_tracks(track, other, -1, &join);
    CHECKF(hr == E_INVALIDARG, "Join at a negative time is E_INVALIDARG (%#lx)", hr);
    IDirectMusicTrack_Release(other);
    other = create_track(&CLSID_DirectMusicMuteTrack);
    hr = join_tracks(track, other, 5, &join);
    CHECKF(hr == DMUS_E_TYPE_UNSUPPORTED, "Join with a mute track is DMUS_E_TYPE_UNSUPPORTED (%#lx)", hr);
    IDirectMusicTrack_Release(other);

    /* extended methods with the performance of InitPlay */
    IDirectMusicTrack_QueryInterface(track, &IID_IDirectMusicTrack8, (void **)&track8);
    {
        void *state = NULL;
        REFERENCE_TIME rt, rt_next = 0;
        MUSIC_TIME mt = 0;

        hr = IDirectMusicTrack8_InitPlay(track8, NULL, perf, &state, 7, 0);
        CHECKF(hr == S_OK && state, "chord map track: InitPlay gives a state (%#lx)", hr);
        IDirectMusicPerformance_MusicToReferenceTime(perf, 1200, &rt);
        hr = IDirectMusicTrack8_GetParamEx(track8, &GUID_IDirectMusicChordMap, rt, &rt_next, &got, state, 0);
        CHECKF(hr == S_OK && got == a, "chord map track: GetParamEx at 1200 is the first chord map (%#lx)", hr);
        if (got) IDirectMusicChordMap_Release(got);
        IDirectMusicPerformance_ReferenceToMusicTime(perf, rt + rt_next, &mt);
        CHECKF(abs(mt - 2000) <= 2, "chord map track: GetParamEx next (relative) is the chord map at 2000 (%ld)", mt);
        IDirectMusicPerformance_MusicToReferenceTime(perf, 4000, &rt);
        hr = IDirectMusicTrack8_SetParamEx(track8, &GUID_IDirectMusicChordMap, rt, c, state, 0);
        CHECKF(hr == S_OK, "chord map track: SetParamEx adds a chord map (%#lx)", hr);
        expect_map(track, 4100, c, 0, "chord map track after SetParamEx");
        hr = IDirectMusicTrack8_GetParamEx(track8, &GUID_IDirectMusicChordMap, rt, NULL, &got, NULL, 0);
        CHECKF(hr == E_POINTER, "chord map track: GetParamEx without play state is E_POINTER (%#lx)", hr);
        hr = IDirectMusicTrack8_EndPlay(track8, state);
        CHECKF(hr == S_OK, "chord map track: EndPlay is S_OK (%#lx)", hr);
    }
    IDirectMusicTrack8_Release(track8);
    IDirectMusicTrack_Release(track);
    CHECKF(refs((IUnknown *)a) == ra && refs((IUnknown *)b) == rb, "chord map track released its chord maps (%lu, %lu, expected %lu, %lu)",
            refs((IUnknown *)a), refs((IUnknown *)b), ra, rb);
    IDirectMusicChordMap_Release(a);
    IDirectMusicChordMap_Release(b);
    IDirectMusicChordMap_Release(c);
}

/* a chord map track loaded from a LIST 'cmap' holding a chord map form */
static void test_chordmap_track_load(void)
{
    IDirectMusicTrack *track = create_track(&CLSID_DirectMusicChordMapTrack);
    IDirectMusicChordMap *got = NULL;
    DWORD scale = 0;
    int list;
    HRESULT hr;

    if (!track) return;
    list = begin("LIST", "cmap");
    chordmap_form(0x555, L"loaded");
    end(list);
    hr = load_track(track);
    CHECKF(hr == S_OK, "chord map track: Load is S_OK (%#lx)", hr);
    hr = IDirectMusicTrack_GetParam(track, &GUID_IDirectMusicChordMap, 0, NULL, &got);
    CHECKF(hr == S_OK && got, "chord map track: the loaded chord map is found (%#lx)", hr);
    if (got)
    {
        IDirectMusicChordMap_GetScale(got, &scale);
        CHECKF(scale == 0x555, "chord map track: the loaded chord map has the scale 0x555 (%#lx)", scale);
        IDirectMusicChordMap_Release(got);
    }
    chordmap_form(0x555, L"x");
    hr = load_track(track);
    CHECKF(hr == DMUS_E_TRACK_NOT_FOUND, "chord map track: Load of another form is DMUS_E_TRACK_NOT_FOUND (%#lx)", hr);
    riff_len = 0;
    IDirectMusicTrack_Release(track);
}

static void signposts(const DMUS_IO_SIGNPOST *items, int count)
{
    DWORD size = sizeof(*items);
    int at = begin("sgnp", NULL);

    put(&size, 4);
    put(items, size * count);
    end(at);
}

/* the signposts of a track, as Save writes them */
static int saved_signposts(IDirectMusicTrack *track, DMUS_IO_SIGNPOST *items, int max)
{
    IPersistStream *ps;
    IStream *stream = new_stream();
    BYTE data[512];
    DWORD id, size, item_size;
    int got;
    HRESULT hr;

    IDirectMusicTrack_QueryInterface(track, &IID_IPersistStream, (void **)&ps);
    hr = IPersistStream_Save(ps, stream, TRUE);
    IPersistStream_Release(ps);
    got = stream_bytes(stream, data, sizeof(data));
    IStream_Release(stream);
    if (hr != S_OK || got < 12) return -1;
    memcpy(&id, data, 4);
    memcpy(&size, data + 4, 4);
    memcpy(&item_size, data + 8, 4);
    if (id != mmioFOURCC('s', 'g', 'n', 'p') || item_size != sizeof(DMUS_IO_SIGNPOST) || size < 4 || (int)(size + 8) > got) return -1;
    got = (size - 4) / item_size;
    if (got > max) return -1;
    memcpy(items, data + 12, got * item_size);
    return got;
}

static void expect_signposts(IDirectMusicTrack *track, const DMUS_IO_SIGNPOST *want, int count, const char *tag)
{
    DMUS_IO_SIGNPOST items[8];
    int got = saved_signposts(track, items, 8), i, same = got == count;

    for (i = 0; same && i < count; i++)
        same = items[i].mtTime == want[i].mtTime && items[i].dwChords == want[i].dwChords && items[i].wMeasure == want[i].wMeasure;
    CHECKF(same, "%s: the track holds the %d expected signposts (%d, first time %ld chords %#lx)", tag, count, got,
            got > 0 ? items[0].mtTime : -1, got > 0 ? items[0].dwChords : 0);
}

static void test_signpost_track(void)
{
    static const DMUS_IO_SIGNPOST data[] = {{0, 0x11, 1}, {1000, 0x22, 2}, {2000, 0x33, 3}};
    IDirectMusicTrack *track = create_track(&CLSID_DirectMusicSignPostTrack), *copy, *join, *other;
    IDirectMusicTrack8 *track8;
    HRESULT hr;

    if (!track) return;
    signposts(data, 3);
    hr = load_track(track);
    CHECKF(hr == S_OK, "signpost track: Load is S_OK (%#lx)", hr);
    expect_signposts(track, data, 3, "loaded signpost track");

    /* Clone: [500, 2500) moved to zero (signposts are points: none is carried) */
    copy = clone(track, 500, 2500);
    if (copy)
    {
        static const DMUS_IO_SIGNPOST want[] = {{500, 0x22, 2}, {1500, 0x33, 3}};
        expect_signposts(copy, want, 2, "signpost clone [500, 2500)");
        IDirectMusicTrack_Release(copy);
    }
    hr = IDirectMusicTrack_Clone(track, 9, 3, &copy);
    CHECKF(hr == E_INVALIDARG, "signpost track: Clone(9, 3) is E_INVALIDARG (%#lx)", hr);

    /* Join: this track before the join time, the other one from it */
    other = create_track(&CLSID_DirectMusicSignPostTrack);
    signposts(&data[2], 1);
    load_track(other);
    join = NULL;
    hr = join_tracks(track, other, 1500, &join);
    CHECKF(hr == S_OK && join, "signpost track: Join is S_OK (%#lx)", hr);
    if (join)
    {
        static const DMUS_IO_SIGNPOST want[] = {{0, 0x11, 1}, {1000, 0x22, 2}, {3500, 0x33, 3}};
        expect_signposts(join, want, 3, "signpost join");
        IDirectMusicTrack_Release(join);
    }
    hr = join_tracks(track, other, -1, &join);
    CHECKF(hr == E_INVALIDARG, "signpost track: Join at a negative time is E_INVALIDARG (%#lx)", hr);
    IDirectMusicTrack_Release(other);

    /* Load of a wrong chunk, notifications, Play */
    chunk("cmnd", data, 4);
    hr = load_track(track);
    CHECKF(hr == DMUS_E_CHUNKNOTFOUND, "signpost track: Load of another chunk is DMUS_E_CHUNKNOTFOUND (%#lx)", hr);
    riff_len = 0;
    IDirectMusicTrack_QueryInterface(track, &IID_IDirectMusicTrack8, (void **)&track8);
    hr = IDirectMusicTrack8_AddNotificationType(track8, NULL);
    CHECKF(hr == E_POINTER, "signpost track: AddNotificationType without a type is E_POINTER (%#lx)", hr);
    hr = IDirectMusicTrack8_AddNotificationType(track8, &GUID_NOTIFICATION_CHORD);
    CHECKF(hr == S_FALSE, "signpost track: AddNotificationType is S_FALSE (%#lx)", hr);
    hr = IDirectMusicTrack8_GetParam(track8, &GUID_ChordParam, 0, NULL, NULL);
    CHECKF(hr == E_NOTIMPL, "signpost track: GetParam is E_NOTIMPL (%#lx)", hr);
    hr = IDirectMusicTrack8_Compose(track8, NULL, 0, NULL);
    CHECKF(hr == E_POINTER, "signpost track: Compose without a result is E_POINTER (%#lx)", hr);
    IDirectMusicTrack8_Release(track8);
    hr = play(track, 0, 3000, 0);
    CHECKF(hr == S_OK && msg_count == 0, "signpost track: Play is S_OK and sends nothing (%#lx, %d)", hr, msg_count);
    IDirectMusicTrack_Release(track);
}

/* a segment with a chord track holding two chords of the scale 0x1111 */
static IDirectMusicSegment *make_segment(BOOL with_map_track, IDirectMusicChordMap *map)
{
    IDirectMusicSegment *segment = NULL;
    IDirectMusicTrack *track;
    DMUS_CHORD_KEY chord = {{0}};
    HRESULT hr;

    hr = CoCreateInstance(&CLSID_DirectMusicSegment, NULL, CLSCTX_INPROC_SERVER, &IID_IDirectMusicSegment, (void **)&segment);
    CHECKF(hr == S_OK, "create a segment (%#lx)", hr);
    if (!segment) return NULL;
    IDirectMusicSegment_SetLength(segment, 4000);

    track = create_track(&CLSID_DirectMusicChordTrack);
    chord.bSubChordCount = 2;
    chord.SubChordList[0].dwScalePattern = 0x1111;
    chord.SubChordList[1].dwScalePattern = 0x1111;
    chord.dwScale = 0x1111;
    lstrcpyW(chord.wszName, L"C");
    IDirectMusicTrack_SetParam(track, &GUID_ChordParam, 0, &chord);
    lstrcpyW(chord.wszName, L"F");
    IDirectMusicTrack_SetParam(track, &GUID_ChordParam, 768, &chord);
    IDirectMusicSegment_InsertTrack(segment, track, 1);
    IDirectMusicTrack_Release(track);

    if (with_map_track)
    {
        track = create_track(&CLSID_DirectMusicChordMapTrack);
        IDirectMusicTrack_SetParam(track, &GUID_IDirectMusicChordMap, 0, map);
        IDirectMusicSegment_InsertTrack(segment, track, 1);
        IDirectMusicTrack_Release(track);
    }
    return segment;
}

static void expect_segment_map(IDirectMusicSegment *segment, IDirectMusicChordMap *map, const char *tag)
{
    IDirectMusicChordMap *got = NULL;
    HRESULT hr = IDirectMusicSegment_GetParam(segment, &GUID_IDirectMusicChordMap, 0xffffffff, 0, 0, NULL, &got);

    CHECKF(hr == S_OK && got == map, "%s: the chord map of the segment is the expected one (%#lx, %p vs %p)", tag, hr, got, map);
    if (got) IDirectMusicChordMap_Release(got);
}

static void expect_segment_scale(IDirectMusicSegment *segment, MUSIC_TIME time, DWORD scale, const char *tag)
{
    DMUS_CHORD_KEY chord = {{0}};
    HRESULT hr = IDirectMusicSegment_GetParam(segment, &GUID_ChordParam, 0xffffffff, 0, time, NULL, &chord);

    CHECKF(hr == S_OK && chord.bSubChordCount == 2 && chord.SubChordList[0].dwScalePattern == scale
            && chord.SubChordList[1].dwScalePattern == scale, "%s: the chord at %ld has the scale %#lx in both subchords (%#lx, %lu subchords, %#lx %#lx)", tag,
            time, scale, hr, (DWORD)chord.bSubChordCount, chord.SubChordList[0].dwScalePattern, chord.SubChordList[1].dwScalePattern);
}

static void test_composer(void)
{
    IDirectMusicChordMap *a = make_chordmap(0x1111, L"A"), *b = make_chordmap(0xabcdef, L"B");
    IDirectMusicComposer *composer = NULL;
    IDirectMusicSegment *segment;
    HRESULT hr;

    hr = CoCreateInstance(&CLSID_DirectMusicComposer, NULL, CLSCTX_INPROC_SERVER, &IID_IDirectMusicComposer, (void **)&composer);
    CHECKF(hr == S_OK, "create the composer (%#lx)", hr);
    if (!composer || !a || !b) return;

    /* the chord map is replaced, the scales of the chords stay */
    segment = make_segment(TRUE, a);
    expect_segment_map(segment, a, "before ChangeChordMap");
    hr = IDirectMusicComposer_ChangeChordMap(composer, segment, FALSE, b);
    CHECKF(hr == S_OK, "ChangeChordMap without track scale is S_OK (%#lx)", hr);
    expect_segment_map(segment, b, "ChangeChordMap without track scale");
    expect_segment_scale(segment, 0, 0x1111, "ChangeChordMap without track scale");
    expect_segment_scale(segment, 768, 0x1111, "ChangeChordMap without track scale");

    /* with the track scale every chord gets the scale of the chord map */
    hr = IDirectMusicComposer_ChangeChordMap(composer, segment, TRUE, a);
    CHECKF(hr == S_OK, "ChangeChordMap with track scale is S_OK (%#lx)", hr);
    hr = IDirectMusicComposer_ChangeChordMap(composer, segment, TRUE, b);
    CHECKF(hr == S_OK, "ChangeChordMap with track scale and the other chord map is S_OK (%#lx)", hr);
    expect_segment_map(segment, b, "ChangeChordMap with track scale");
    expect_segment_scale(segment, 0, 0xabcdef, "ChangeChordMap with track scale");
    expect_segment_scale(segment, 800, 0xabcdef, "ChangeChordMap with track scale");
    IDirectMusicSegment_Release(segment);

    /* a segment without a chord map track gets one */
    segment = make_segment(FALSE, NULL);
    hr = IDirectMusicComposer_ChangeChordMap(composer, segment, FALSE, a);
    CHECKF(hr == S_OK, "ChangeChordMap of a segment without a chord map track is S_OK (%#lx)", hr);
    expect_segment_map(segment, a, "ChangeChordMap of a segment without a chord map track");
    IDirectMusicSegment_Release(segment);

    hr = IDirectMusicComposer_ChangeChordMap(composer, NULL, FALSE, a);
    CHECKF(hr == E_POINTER, "ChangeChordMap without a segment is E_POINTER (%#lx)", hr);
    segment = make_segment(FALSE, NULL);
    hr = IDirectMusicComposer_ChangeChordMap(composer, segment, FALSE, NULL);
    CHECKF(hr == E_POINTER, "ChangeChordMap without a chord map is E_POINTER (%#lx)", hr);
    IDirectMusicSegment_Release(segment);

    IDirectMusicComposer_Release(composer);
    IDirectMusicChordMap_Release(a);
    IDirectMusicChordMap_Release(b);
}

static void test_template(void)
{
    IPersistStream *ps = NULL;
    IStream *stream;
    CLSID class = {0};
    HRESULT hr;

    hr = CoCreateInstance(&CLSID_DirectMusicTemplate, NULL, CLSCTX_INPROC_SERVER, &IID_IPersistStream, (void **)&ps);
    CHECKF(hr == S_OK && ps, "create the template object (%#lx)", hr);
    if (!ps) return;
    hr = IPersistStream_GetClassID(ps, &class);
    CHECKF(hr == S_OK && IsEqualGUID(&class, &CLSID_DirectMusicTemplate), "the template class is CLSID_DirectMusicTemplate (%#lx)", hr);
    hr = IPersistStream_IsDirty(ps);
    CHECKF(hr == S_FALSE, "template IsDirty is S_FALSE (%#lx)", hr);
    stream = new_stream();
    hr = IPersistStream_Load(ps, stream);
    CHECKF(hr == E_NOTIMPL, "template Load is E_NOTIMPL (%#lx)", hr);
    IStream_Release(stream);
    IPersistStream_Release(ps);
}

int main(void)
{
    CoInitialize(NULL);
    if (!setup_perf()) return 77;

    test_chordmap();
    test_chordmap_track();
    test_chordmap_track_load();
    test_signpost_track();
    test_composer();
    test_template();

    teardown_perf();
    CoUninitialize();
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
