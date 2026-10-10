/* dmstyle tracks (patches/sg/2960), run by test/dmcontent-styletracks-gate.sh: the command, chord, mute,
 * style, motif and audition tracks are loaded from RIFF data built here (or filled with SetParam), asked
 * for their parameters, cloned, joined, saved and played, and the answers are compared with the data.
 *
 *   dmcontent-styletracks-probe.exe */
#include "dmcontent-common.h"

#define MEASURE (DMUS_PPQ * 4)

static void expect_cmd(IDirectMusicTrack *track, MUSIC_TIME time, MUSIC_TIME next, int cmd, int level,
        int range, int repeat, const char *tag)
{
    DMUS_COMMAND_PARAM p = {0xee, 0xee, 0xee, 0xee};
    MUSIC_TIME got_next = 0x1234;
    HRESULT hr = IDirectMusicTrack_GetParam(track, &GUID_CommandParam, time, &got_next, &p);
    CHECKF(hr == S_OK && p.bCommand == cmd && p.bGrooveLevel == level && p.bGrooveRange == range
            && p.bRepeatMode == repeat && got_next == next,
            "%s: command at %ld is %d/%d/%d/%d next %ld (got %#lx %d/%d/%d/%d next %ld)", tag, time, cmd, level, range,
            repeat, next, hr, p.bCommand, p.bGrooveLevel, p.bGrooveRange, p.bRepeatMode, got_next);
}

static HRESULT join_tracks(IDirectMusicTrack *track, IDirectMusicTrack *other, MUSIC_TIME time, IUnknown *context,
        DWORD group, IDirectMusicTrack **result)
{
    IDirectMusicTrack8 *track8;
    HRESULT hr;
    IDirectMusicTrack_QueryInterface(track, &IID_IDirectMusicTrack8, (void **)&track8);
    hr = IDirectMusicTrack8_Join(track8, other, time, context, group, result);
    IDirectMusicTrack8_Release(track8);
    return hr;
}

static void load_from(IDirectMusicTrack *track, IStream *stream, HRESULT *hr)
{
    IPersistStream *ps2;
    IDirectMusicTrack_QueryInterface(track, &IID_IPersistStream, (void **)&ps2);
    *hr = IPersistStream_Load(ps2, stream);
    IPersistStream_Release(ps2);
}

static void test_command(void)
{
    static const DMUS_IO_COMMAND cmds[] = {
        {0, 0, 0, DMUS_COMMANDT_GROOVE, 10, 5, 1},
        {MEASURE, 1, 0, DMUS_COMMANDT_FILL, 20, 0, 0},
        {MEASURE * 2, 2, 0, DMUS_COMMANDT_BREAK, 30, 2, 3},
    };
    IDirectMusicTrack *track = create_track(&CLSID_DirectMusicCommandTrack), *copy, *join;
    DMUS_COMMAND_PARAM_2 p2;
    DMUS_COMMAND_PARAM p, set;
    IPersistStream *ps;
    IStream *stream;
    MUSIC_TIME next;
    DWORD bytes[16];
    HRESULT hr;
    int i;

    if (!track) return;

    hr = IDirectMusicTrack_GetParam(track, &GUID_CommandParam, 0, NULL, &p);
    CHECKF(hr == DMUS_E_NOT_FOUND, "command: GetParam without commands is DMUS_E_NOT_FOUND (%#lx)", hr);

    array_chunk("cmnd", cmds, sizeof(cmds[0]), 3);
    hr = load_track(track);
    CHECKF(hr == S_OK, "command: Load is S_OK (%#lx)", hr);

    expect_cmd(track, 0, MEASURE, DMUS_COMMANDT_GROOVE, 10, 5, 1, "command");
    expect_cmd(track, MEASURE - 1, 1, DMUS_COMMANDT_GROOVE, 10, 5, 1, "command");
    expect_cmd(track, MEASURE, MEASURE, DMUS_COMMANDT_FILL, 20, 0, 0, "command");
    expect_cmd(track, MEASURE * 2 + 5, 0, DMUS_COMMANDT_BREAK, 30, 2, 3, "command");
    hr = IDirectMusicTrack_GetParam(track, &GUID_CommandParam, -1, NULL, &p);
    CHECKF(hr == DMUS_E_NOT_FOUND, "command: GetParam before the first command is DMUS_E_NOT_FOUND (%#lx)", hr);
    hr = IDirectMusicTrack_GetParam(track, &GUID_CommandParam, 0, NULL, NULL);
    CHECKF(hr == E_POINTER, "command: GetParam without data is E_POINTER (%#lx)", hr);

    memset(&p2, 0xee, sizeof(p2));
    next = 99;
    hr = IDirectMusicTrack_GetParam(track, &GUID_CommandParam2, 4000, &next, &p2);
    CHECKF(hr == S_OK && p2.mtTime == MEASURE - 4000 && p2.bCommand == DMUS_COMMANDT_FILL && p2.bGrooveLevel == 20
            && next == MEASURE * 2 - 4000, "command: CommandParam2 at 4000 (%#lx, mtTime %ld, next %ld)", hr, p2.mtTime, next);
    hr = IDirectMusicTrack_GetParam(track, &GUID_CommandParam2, MEASURE * 2, &next, &p2);
    CHECKF(hr == S_OK && p2.mtTime == 0 && p2.bCommand == DMUS_COMMANDT_BREAK && p2.bGrooveRange == 2 && p2.bRepeatMode == 3
            && next == 0, "command: CommandParam2 at the last command (%#lx, mtTime %ld, next %ld)", hr, p2.mtTime, next);

    memset(&p, 0xee, sizeof(p));
    next = 99;
    hr = IDirectMusicTrack_GetParam(track, &GUID_CommandParamNext, 100, &next, &p);
    CHECKF(hr == S_OK && p.bCommand == DMUS_COMMANDT_FILL && p.bGrooveLevel == 20 && next == MEASURE * 2 - 100,
            "command: CommandParamNext at 100 is the fill (%#lx, command %d, next %ld)", hr, p.bCommand, next);
    hr = IDirectMusicTrack_GetParam(track, &GUID_CommandParamNext, MEASURE * 2, &next, &p);
    CHECKF(hr == DMUS_E_NOT_FOUND, "command: CommandParamNext after the last command is DMUS_E_NOT_FOUND (%#lx)", hr);

    /* SetParam adds a command, or replaces the one at the same time */
    set.bCommand = DMUS_COMMANDT_END; set.bGrooveLevel = 40; set.bGrooveRange = 7; set.bRepeatMode = 2;
    hr = IDirectMusicTrack_SetParam(track, &GUID_CommandParam, MEASURE * 3, &set);
    CHECKF(hr == S_OK, "command: SetParam adds a command (%#lx)", hr);
    expect_cmd(track, MEASURE * 3 + 10, 0, DMUS_COMMANDT_END, 40, 7, 2, "command");
    expect_cmd(track, MEASURE * 2 + 10, MEASURE - 10, DMUS_COMMANDT_BREAK, 30, 2, 3, "command");
    set.bCommand = DMUS_COMMANDT_INTRO; set.bGrooveLevel = 21;
    hr = IDirectMusicTrack_SetParam(track, &GUID_CommandParam, MEASURE, &set);
    CHECKF(hr == S_OK, "command: SetParam replaces a command (%#lx)", hr);
    expect_cmd(track, MEASURE, MEASURE, DMUS_COMMANDT_INTRO, 21, 7, 2, "command");
    set.bCommand = DMUS_COMMANDT_FILL; set.bGrooveLevel = 20; set.bGrooveRange = 0; set.bRepeatMode = 0;
    IDirectMusicTrack_SetParam(track, &GUID_CommandParam, MEASURE, &set);
    hr = IDirectMusicTrack_SetParam(track, &GUID_CommandParam, 0, NULL);
    CHECKF(hr == E_POINTER, "command: SetParam without data is E_POINTER (%#lx)", hr);
    hr = IDirectMusicTrack_SetParam(track, &GUID_CommandParamNext, 0, &set);
    CHECKF(hr == DMUS_E_SET_UNSUPPORTED, "command: SetParam(CommandParamNext) is DMUS_E_SET_UNSUPPORTED (%#lx)", hr);

    /* Clone: [start, end) moved to zero, the command in effect at start comes first */
    copy = clone(track, MEASURE, MEASURE * 3);
    if (copy)
    {
        expect_cmd(copy, 0, MEASURE, DMUS_COMMANDT_FILL, 20, 0, 0, "command clone [3072, 9216)");
        expect_cmd(copy, MEASURE, 0, DMUS_COMMANDT_BREAK, 30, 2, 3, "command clone [3072, 9216)");
        hr = IDirectMusicTrack_GetParam(copy, &GUID_CommandParam, MEASURE * 2, &next, &p);
        CHECKF(hr == S_OK && p.bCommand == DMUS_COMMANDT_BREAK && next == 0, "command clone has nothing from 9216 (%#lx)", hr);
        IDirectMusicTrack_Release(copy);
    }
    copy = clone(track, 1000, 7000);
    if (copy)
    {
        expect_cmd(copy, 0, MEASURE - 1000, DMUS_COMMANDT_GROOVE, 10, 5, 1, "command clone [1000, 7000)");
        expect_cmd(copy, MEASURE - 1000, MEASURE, DMUS_COMMANDT_FILL, 20, 0, 0, "command clone [1000, 7000)");
        expect_cmd(copy, MEASURE * 2 - 1000, 0, DMUS_COMMANDT_BREAK, 30, 2, 3, "command clone [1000, 7000)");
        IDirectMusicTrack_Release(copy);
    }
    copy = clone(track, 0, 0);
    if (copy) IDirectMusicTrack_Release(copy);

    /* Join: this track before the join time, the other one from the join time */
    copy = create_track(&CLSID_DirectMusicCommandTrack);
    set.bCommand = DMUS_COMMANDT_END; set.bGrooveLevel = 99; set.bGrooveRange = 1; set.bRepeatMode = 1;
    IDirectMusicTrack_SetParam(copy, &GUID_CommandParam, 0, &set);
    set.bCommand = DMUS_COMMANDT_FILL; set.bGrooveLevel = 98;
    IDirectMusicTrack_SetParam(copy, &GUID_CommandParam, 500, &set);
    join = NULL;
    hr = join_tracks(track, copy, 4000, NULL, 0, &join);
    CHECKF(hr == S_OK && join, "command: Join is S_OK (%#lx)", hr);
    if (join)
    {
        expect_cmd(join, 0, MEASURE, DMUS_COMMANDT_GROOVE, 10, 5, 1, "command join");
        expect_cmd(join, MEASURE, 4000 - MEASURE, DMUS_COMMANDT_FILL, 20, 0, 0, "command join");
        expect_cmd(join, 4000, 500, DMUS_COMMANDT_END, 99, 1, 1, "command join");
        expect_cmd(join, 4600, 0, DMUS_COMMANDT_FILL, 98, 1, 1, "command join");
        IDirectMusicTrack_Release(join);
    }
    {
        IDirectMusicTrack *other = create_track(&CLSID_DirectMusicMuteTrack);
        hr = join_tracks(track, other, 4000, NULL, 0, &join);
        CHECKF(hr == DMUS_E_TYPE_UNSUPPORTED, "command: Join with a mute track is DMUS_E_TYPE_UNSUPPORTED (%#lx)", hr);
        IDirectMusicTrack_Release(other);
    }
    hr = join_tracks(track, copy, -1, NULL, 0, &join);
    CHECKF(hr == E_INVALIDARG, "command: Join at a negative time is E_INVALIDARG (%#lx)", hr);
    IDirectMusicTrack_Release(copy);

    /* Save writes the chunk Load reads */
    stream = new_stream();
    IDirectMusicTrack_QueryInterface(track, &IID_IPersistStream, (void **)&ps);
    hr = IPersistStream_Save(ps, stream, TRUE);
    CHECKF(hr == S_OK, "command: Save is S_OK (%#lx)", hr);
    memset(bytes, 0, sizeof(bytes));
    i = stream_bytes(stream, bytes, sizeof(bytes));
    CHECKF(i >= 12 && bytes[0] == mmioFOURCC('c','m','n','d') && bytes[1] == 4 + 4 * 12 && bytes[2] == 12,
            "command: Save writes 'cmnd' with 4 commands of 12 bytes (%d bytes, size %lu)", i, bytes[1]);
    rewind_stream(stream);
    copy = create_track(&CLSID_DirectMusicCommandTrack);
    load_from(copy, stream, &hr);
    CHECKF(hr == S_OK, "command: Load of the saved data is S_OK (%#lx)", hr);
    expect_cmd(copy, MEASURE * 3, 0, DMUS_COMMANDT_END, 40, 7, 2, "command reloaded");
    expect_cmd(copy, MEASURE + 5, MEASURE - 5, DMUS_COMMANDT_FILL, 20, 0, 0, "command reloaded");
    IDirectMusicTrack_Release(copy);
    IStream_Release(stream);
    IPersistStream_Release(ps);

    /* Load of another chunk */
    copy = create_track(&CLSID_DirectMusicCommandTrack);
    chunk("mute", cmds, 4);
    hr = load_track(copy);
    CHECKF(hr == E_FAIL, "command: Load of another chunk is E_FAIL (%#lx)", hr);
    IDirectMusicTrack_Release(copy);

    /* notifications: S_OK for the command notification, S_FALSE for others */
    hr = IDirectMusicTrack_AddNotificationType(track, &GUID_NOTIFICATION_SEGMENT);
    CHECKF(hr == S_FALSE, "command: AddNotificationType(segment) is S_FALSE (%#lx)", hr);
    hr = play(track, 0, MEASURE * 2, 1000);
    CHECKF(hr == S_OK, "command: Play is S_OK (%#lx)", hr);
    EXPECT_COUNT(0, "command Play without notification");
    hr = IDirectMusicTrack_AddNotificationType(track, &GUID_NOTIFICATION_COMMAND);
    CHECKF(hr == S_OK, "command: AddNotificationType(command) is S_OK (%#lx)", hr);
    hr = play(track, 0, MEASURE * 2, 1000);
    CHECKF(hr == S_OK, "command: Play with notification is S_OK (%#lx)", hr);
    EXPECT_COUNT(2, "command Play of [0, 6144) with the notification");
    CHECKF(msg_count == 2 && msgs[0].type == DMUS_PMSGT_NOTIFICATION && IsEqualGUID(&msgs[0].guid, &GUID_NOTIFICATION_COMMAND)
            && msgs[0].mtTime == 1000 && msgs[1].mtTime == 1000 + MEASURE,
            "command notifications are at 1000 and 4072 (%ld, %ld)", msgs[0].mtTime, msgs[1].mtTime);
    hr = play(track, MEASURE + 1, MEASURE * 2, 0);
    EXPECT_COUNT(0, "command Play of (3072, 6144)");
    hr = IDirectMusicTrack_RemoveNotificationType(track, &GUID_NOTIFICATION_COMMAND);
    CHECKF(hr == S_OK, "command: RemoveNotificationType is S_OK (%#lx)", hr);
    hr = play(track, 0, MEASURE * 2, 1000);
    EXPECT_COUNT(0, "command Play after RemoveNotificationType");

    /* extended methods: reference times are converted through the performance of InitPlay */
    {
        IDirectMusicTrack8 *track8;
        void *state = NULL;
        REFERENCE_TIME rt, rt_next = 0x77;
        MUSIC_TIME mt_start = 0;
        DMUS_COMMAND_PARAM ex = {0};

        IDirectMusicTrack_QueryInterface(track, &IID_IDirectMusicTrack8, (void **)&track8);
        hr = IDirectMusicTrack8_InitPlay(track8, NULL, perf, &state, 3, 0);
        CHECKF(hr == S_OK && state, "command: InitPlay is S_OK and gives a state (%#lx)", hr);
        IDirectMusicPerformance_MusicToReferenceTime(perf, MEASURE * 2 + 5, &rt);
        hr = IDirectMusicTrack8_GetParamEx(track8, &GUID_CommandParam, rt, &rt_next, &ex, state, 0);
        CHECKF(hr == S_OK && ex.bCommand == DMUS_COMMANDT_BREAK && ex.bGrooveLevel == 30 && rt_next > 0,
                "command: GetParamEx at the break (%#lx, command %d, next %ld)", hr, ex.bCommand, (long)rt_next);
        IDirectMusicPerformance_ReferenceToMusicTime(perf, rt + rt_next, &mt_start);
        CHECKF(abs(mt_start - MEASURE * 3) <= 2, "command: GetParamEx next is the command at 9216 (%ld)", mt_start);
        set.bCommand = DMUS_COMMANDT_END; set.bGrooveLevel = 55; set.bGrooveRange = 4; set.bRepeatMode = 3;
        IDirectMusicPerformance_MusicToReferenceTime(perf, MEASURE * 5, &rt);
        hr = IDirectMusicTrack8_SetParamEx(track8, &GUID_CommandParam, rt, &set, state, 0);
        CHECKF(hr == S_OK, "command: SetParamEx is S_OK (%#lx)", hr);
        expect_cmd(track, MEASURE * 5 + 1, 0, DMUS_COMMANDT_END, 55, 4, 3, "command SetParamEx");
        hr = IDirectMusicTrack8_GetParamEx(track8, &GUID_CommandParam, rt, NULL, &ex, NULL, 0);
        CHECKF(hr == E_POINTER, "command: GetParamEx without play state is E_POINTER (%#lx)", hr);
        hr = IDirectMusicTrack8_PlayEx(track8, state, 0, 195312, rt, 0, perf, NULL, 3);
        CHECKF(hr == S_OK, "command: PlayEx is S_OK (%#lx)", hr);
        hr = IDirectMusicTrack8_EndPlay(track8, state);
        CHECKF(hr == S_OK, "command: EndPlay is S_OK (%#lx)", hr);
        IDirectMusicTrack8_Release(track8);
    }

    IDirectMusicTrack_Release(track);
}

static void expect_chord(IDirectMusicTrack *track, MUSIC_TIME time, MUSIC_TIME next, const WCHAR *name, int measure,
        int beat, int subchords, DWORD scale, int key, int flags, const char *tag)
{
    DMUS_CHORD_KEY k;
    MUSIC_TIME got_next = 0x1234;
    HRESULT hr;

    memset(&k, 0xee, sizeof(k));
    hr = IDirectMusicTrack_GetParam(track, &GUID_ChordParam, time, &got_next, &k);
    CHECKF(hr == S_OK && !lstrcmpW(k.wszName, name) && k.wMeasure == measure && k.bBeat == beat && k.bSubChordCount == subchords
            && k.dwScale == scale && k.bKey == key && k.bFlags == flags && got_next == next,
            "%s: chord at %ld is %ls m%d b%d x%d scale %#lx key %d flags %d next %ld (got %#lx m%d b%d x%d scale %#lx key %d flags %d next %ld)",
            tag, time, name, measure, beat, subchords, scale, key, flags, next, hr, k.wMeasure, k.bBeat,
            k.bSubChordCount, k.dwScale, k.bKey, k.bFlags, got_next);
}

static void build_chord_track(void)
{
    static const DMUS_IO_SUBCHORD subs[] = {
        {0x91, 0xab5ab5, 0x1, 3, 2, 0},
        {0x891, 0xab5ab5, 0x2, 5, 7, 7},
    };
    DMUS_IO_CHORD c1 = {L"Cmaj", 0, 0, 0, 1}, c2 = {L"Dm7", MEASURE + DMUS_PPQ, 1, 1, 0};
    DWORD scale = 0xab5ab5 | (5 << 24), size, count;
    int list, at;

    list = begin("LIST", "cord");
    chunk("crdh", &scale, 4);
    at = begin("crdb", NULL);
    size = sizeof(c1);
    put(&size, 4); put(&c1, sizeof(c1));
    count = 1; put(&count, 4);
    size = sizeof(DMUS_IO_SUBCHORD); put(&size, 4);
    put(&subs[0], sizeof(subs[0]));
    end(at);
    at = begin("crdb", NULL);
    size = sizeof(c2);
    put(&size, 4); put(&c2, sizeof(c2));
    count = 2; put(&count, 4);
    size = sizeof(DMUS_IO_SUBCHORD); put(&size, 4);
    put(subs, sizeof(subs));
    end(at);
    end(list);
}

static void test_chord(void)
{
    IDirectMusicTrack *track = create_track(&CLSID_DirectMusicChordTrack), *copy, *join;
    DMUS_CHORD_KEY k;
    DMUS_RHYTHM_PARAM r;
    IPersistStream *ps;
    IStream *stream;
    MUSIC_TIME next = 7;
    DWORD bytes[8];
    HRESULT hr;
    int i;

    if (!track) return;

    hr = IDirectMusicTrack_GetParam(track, &GUID_ChordParam, 0, NULL, &k);
    CHECKF(hr == DMUS_E_NOT_FOUND, "chord: GetParam without chords is DMUS_E_NOT_FOUND (%#lx)", hr);

    build_chord_track();
    hr = load_track(track);
    CHECKF(hr == S_OK, "chord: Load is S_OK (%#lx)", hr);

    expect_chord(track, 0, MEASURE + DMUS_PPQ, L"Cmaj", 0, 0, 1, 0xab5ab5, 5, 1, "chord");
    expect_chord(track, 3000, MEASURE + DMUS_PPQ - 3000, L"Cmaj", 0, 0, 1, 0xab5ab5, 5, 1, "chord");
    expect_chord(track, MEASURE + DMUS_PPQ, 0, L"Dm7", 1, 1, 2, 0xab5ab5, 5, 0, "chord");
    expect_chord(track, MEASURE * 3, 0, L"Dm7", 1, 1, 2, 0xab5ab5, 5, 0, "chord");
    hr = IDirectMusicTrack_GetParam(track, &GUID_ChordParam, -1, NULL, &k);
    CHECKF(hr == DMUS_E_NOT_FOUND, "chord: GetParam before the first chord is DMUS_E_NOT_FOUND (%#lx)", hr);
    hr = IDirectMusicTrack_GetParam(track, &GUID_ChordParam, MEASURE + DMUS_PPQ, NULL, &k);
    CHECKF(hr == S_OK && k.SubChordList[0].dwChordPattern == 0x91 && k.SubChordList[0].dwScalePattern == 0xab5ab5
            && k.SubChordList[0].dwInversionPoints == 1 && k.SubChordList[0].dwLevels == 3
            && k.SubChordList[0].bChordRoot == 2 && k.SubChordList[0].bScaleRoot == 0
            && k.SubChordList[1].dwChordPattern == 0x891 && k.SubChordList[1].dwInversionPoints == 2
            && k.SubChordList[1].dwLevels == 5 && k.SubChordList[1].bChordRoot == 7 && k.SubChordList[1].bScaleRoot == 7,
            "chord: the subchords of the second chord (%#lx)", hr);
    hr = IDirectMusicTrack_GetParam(track, &GUID_ChordParam, 0, NULL, NULL);
    CHECKF(hr == E_POINTER, "chord: GetParam without data is E_POINTER (%#lx)", hr);

    /* rhythm: a bit for each grid of the measure of the time, where a chord starts */
    memset(&r, 0, sizeof(r));
    r.TimeSig.bBeatsPerMeasure = 4; r.TimeSig.bBeat = 4; r.TimeSig.wGridsPerBeat = 4;
    r.dwRhythmPattern = 0xdeadbeef;
    hr = IDirectMusicTrack_GetParam(track, &GUID_RhythmParam, 100, &next, &r);
    CHECKF(hr == S_OK && r.dwRhythmPattern == 1 && next == MEASURE - 100,
            "chord: RhythmParam of the first measure is 1 (%#lx, %#lx, next %ld)", hr, r.dwRhythmPattern, next);
    hr = IDirectMusicTrack_GetParam(track, &GUID_RhythmParam, MEASURE + 5, &next, &r);
    CHECKF(hr == S_OK && r.dwRhythmPattern == (1 << 4) && next == MEASURE - 5,
            "chord: RhythmParam of the second measure is 1<<4 (%#lx, %#lx, next %ld)", hr, r.dwRhythmPattern, next);
    hr = IDirectMusicTrack_GetParam(track, &GUID_RhythmParam, MEASURE * 2, &next, &r);
    CHECKF(hr == S_OK && r.dwRhythmPattern == 0 && next == MEASURE, "chord: RhythmParam of the third measure is 0 (%#lx, %#lx, next %ld)", hr, r.dwRhythmPattern, next);
    r.TimeSig.bBeat = 0;
    hr = IDirectMusicTrack_GetParam(track, &GUID_RhythmParam, 0, NULL, &r);
    CHECKF(hr == E_INVALIDARG, "chord: RhythmParam of an empty time signature is E_INVALIDARG (%#lx)", hr);

    /* SetParam adds a chord, or replaces the one at the same time */
    memset(&k, 0, sizeof(k));
    lstrcpyW(k.wszName, L"G7");
    k.wMeasure = 2; k.bBeat = 0; k.bSubChordCount = 1; k.bFlags = 2;
    k.SubChordList[0].dwChordPattern = 0x491; k.SubChordList[0].bChordRoot = 9;
    hr = IDirectMusicTrack_SetParam(track, &GUID_ChordParam, MEASURE * 2, &k);
    CHECKF(hr == S_OK, "chord: SetParam adds a chord (%#lx)", hr);
    expect_chord(track, MEASURE * 2 + 1, 0, L"G7", 2, 0, 1, 0xab5ab5, 5, 2, "chord");
    expect_chord(track, MEASURE + DMUS_PPQ + 1, MEASURE - DMUS_PPQ - 1, L"Dm7", 1, 1, 2, 0xab5ab5, 5, 0, "chord");
    lstrcpyW(k.wszName, L"Am");
    hr = IDirectMusicTrack_SetParam(track, &GUID_ChordParam, MEASURE * 2, &k);
    CHECKF(hr == S_OK, "chord: SetParam replaces a chord (%#lx)", hr);
    expect_chord(track, MEASURE * 2, 0, L"Am", 2, 0, 1, 0xab5ab5, 5, 2, "chord");
    hr = IDirectMusicTrack_SetParam(track, &GUID_ChordParam, 0, NULL);
    CHECKF(hr == E_POINTER, "chord: SetParam without data is E_POINTER (%#lx)", hr);
    k.bSubChordCount = 9;
    hr = IDirectMusicTrack_SetParam(track, &GUID_ChordParam, 5, &k);
    CHECKF(hr == E_INVALIDARG, "chord: SetParam with 9 subchords is E_INVALIDARG (%#lx)", hr);

    /* Clone and Join */
    copy = clone(track, MEASURE, MEASURE * 3);
    if (copy)
    {
        expect_chord(copy, 0, DMUS_PPQ, L"Cmaj", 0, 0, 1, 0xab5ab5, 5, 1, "chord clone [3072, 9216)");
        expect_chord(copy, DMUS_PPQ, MEASURE - DMUS_PPQ, L"Dm7", 1, 1, 2, 0xab5ab5, 5, 0, "chord clone [3072, 9216)");
        expect_chord(copy, MEASURE, 0, L"Am", 2, 0, 1, 0xab5ab5, 5, 2, "chord clone [3072, 9216)");
        IDirectMusicTrack_Release(copy);
    }
    copy = create_track(&CLSID_DirectMusicChordTrack);
    lstrcpyW(k.wszName, L"F"); k.bSubChordCount = 1; k.dwScale = 0x123; k.bKey = 0;
    IDirectMusicTrack_SetParam(copy, &GUID_ChordParam, 0, &k);
    join = NULL;
    hr = join_tracks(track, copy, 5000, NULL, 0, &join);
    CHECKF(hr == S_OK && join, "chord: Join is S_OK (%#lx)", hr);
    if (join)
    {
        expect_chord(join, 0, MEASURE + DMUS_PPQ, L"Cmaj", 0, 0, 1, 0xab5ab5, 5, 1, "chord join");
        expect_chord(join, 4999, 1, L"Dm7", 1, 1, 2, 0xab5ab5, 5, 0, "chord join");
        expect_chord(join, 5000, 0, L"F", 2, 0, 1, 0xab5ab5, 5, 2, "chord join");
        IDirectMusicTrack_Release(join);
    }
    hr = join_tracks(track, copy, -5, NULL, 0, &join);
    CHECKF(hr == E_INVALIDARG, "chord: Join at a negative time is E_INVALIDARG (%#lx)", hr);
    IDirectMusicTrack_Release(copy);

    /* Save writes a list Load reads back */
    stream = new_stream();
    IDirectMusicTrack_QueryInterface(track, &IID_IPersistStream, (void **)&ps);
    hr = IPersistStream_Save(ps, stream, TRUE);
    CHECKF(hr == S_OK, "chord: Save is S_OK (%#lx)", hr);
    memset(bytes, 0, sizeof(bytes));
    i = stream_bytes(stream, bytes, sizeof(bytes));
    CHECKF(i == sizeof(bytes) && bytes[0] == mmioFOURCC('L','I','S','T') && bytes[2] == mmioFOURCC('c','o','r','d')
            && bytes[3] == mmioFOURCC('c','r','d','h') && bytes[4] == 4 && bytes[5] == (0xab5ab5 | (5 << 24)),
            "chord: Save writes the 'cord' list with the 'crdh' scale (%#lx, %#lx)", bytes[2], bytes[5]);
    rewind_stream(stream);
    copy = create_track(&CLSID_DirectMusicChordTrack);
    load_from(copy, stream, &hr);
    CHECKF(hr == S_OK, "chord: Load of the saved list is S_OK (%#lx)", hr);
    expect_chord(copy, 0, MEASURE + DMUS_PPQ, L"Cmaj", 0, 0, 1, 0xab5ab5, 5, 1, "chord reloaded");
    expect_chord(copy, MEASURE + DMUS_PPQ, MEASURE - DMUS_PPQ, L"Dm7", 1, 1, 2, 0xab5ab5, 5, 0, "chord reloaded");
    expect_chord(copy, MEASURE * 2, 0, L"Am", 2, 0, 1, 0xab5ab5, 5, 2, "chord reloaded");
    IDirectMusicTrack_Release(copy);
    IStream_Release(stream);
    IPersistStream_Release(ps);

    copy = create_track(&CLSID_DirectMusicChordTrack);
    chunk("mute", bytes, 4);
    hr = load_track(copy);
    CHECKF(hr == E_FAIL, "chord: Load of another chunk is E_FAIL (%#lx)", hr);
    IDirectMusicTrack_Release(copy);

    /* notifications */
    hr = IDirectMusicTrack_AddNotificationType(track, &GUID_NOTIFICATION_COMMAND);
    CHECKF(hr == S_FALSE, "chord: AddNotificationType(command) is S_FALSE (%#lx)", hr);
    hr = IDirectMusicTrack_AddNotificationType(track, &GUID_NOTIFICATION_CHORD);
    CHECKF(hr == S_OK, "chord: AddNotificationType(chord) is S_OK (%#lx)", hr);
    hr = play(track, 0, MEASURE * 3, 100);
    CHECKF(hr == S_OK, "chord: Play is S_OK (%#lx)", hr);
    EXPECT_COUNT(3, "chord Play of [0, 9216) with the notification");
    CHECKF(msg_count == 3 && msgs[0].type == DMUS_PMSGT_NOTIFICATION && IsEqualGUID(&msgs[0].guid, &GUID_NOTIFICATION_CHORD)
            && msgs[0].mtTime == 100 && msgs[1].mtTime == 100 + MEASURE + DMUS_PPQ && msgs[2].mtTime == 100 + MEASURE * 2,
            "chord notifications are at the chords (%ld, %ld, %ld)", msgs[0].mtTime, msgs[1].mtTime, msgs[2].mtTime);
    hr = IDirectMusicTrack_RemoveNotificationType(track, &GUID_NOTIFICATION_CHORD);
    CHECKF(hr == S_OK, "chord: RemoveNotificationType is S_OK (%#lx)", hr);
    hr = play(track, 0, MEASURE * 3, 100);
    EXPECT_COUNT(0, "chord Play after RemoveNotificationType");

    IDirectMusicTrack_Release(track);
}

static void expect_mute(IDirectMusicTrack *track, DWORD channel, MUSIC_TIME time, DWORD map, BOOL mute, MUSIC_TIME next,
        const char *tag)
{
    DMUS_MUTE_PARAM p;
    MUSIC_TIME got_next = 0x1234;
    HRESULT hr;

    p.dwPChannel = channel; p.dwPChannelMap = 0xdead; p.fMute = 0xdead;
    hr = IDirectMusicTrack_GetParam(track, &GUID_MuteParam, time, &got_next, &p);
    CHECKF(hr == S_OK && p.dwPChannelMap == map && !p.fMute == !mute && got_next == next,
            "%s: channel %lu at %ld maps to %#lx mute %d next %ld (got %#lx %#lx %d next %ld)", tag, channel, time, map, mute, next,
            hr, p.dwPChannelMap, p.fMute, got_next);
}

static void test_mute(void)
{
    static const DMUS_IO_MUTE items[] = {
        {1000, 3, 0xffffffff},
        {2000, 5, 9},
        {3000, 3, 3},
    };
    IDirectMusicTrack *track = create_track(&CLSID_DirectMusicMuteTrack), *copy;
    DMUS_MUTE_PARAM p;
    IPersistStream *ps;
    IStream *stream;
    DWORD bytes[8];
    HRESULT hr;
    int i;

    if (!track) return;

    expect_mute(track, 3, 500, 3, FALSE, 0, "mute without items");
    array_chunk("mute", items, sizeof(items[0]), 3);
    hr = load_track(track);
    CHECKF(hr == S_OK, "mute: Load is S_OK (%#lx)", hr);

    expect_mute(track, 3, 500, 3, FALSE, 500, "mute");
    expect_mute(track, 3, 1000, 0xffffffff, TRUE, 2000, "mute");
    expect_mute(track, 3, 2999, 0xffffffff, TRUE, 1, "mute");
    expect_mute(track, 3, 3000, 3, FALSE, 0, "mute");
    expect_mute(track, 5, 1999, 5, FALSE, 1, "mute");
    expect_mute(track, 5, 2500, 9, FALSE, 0, "mute");
    expect_mute(track, 7, 2500, 7, FALSE, 0, "mute");
    hr = IDirectMusicTrack_GetParam(track, &GUID_MuteParam, 0, NULL, NULL);
    CHECKF(hr == E_POINTER, "mute: GetParam without data is E_POINTER (%#lx)", hr);

    /* SetParam: a mute, a mapping */
    p.dwPChannel = 7; p.dwPChannelMap = 1; p.fMute = TRUE;
    hr = IDirectMusicTrack_SetParam(track, &GUID_MuteParam, 100, &p);
    CHECKF(hr == S_OK, "mute: SetParam is S_OK (%#lx)", hr);
    expect_mute(track, 7, 150, 0xffffffff, TRUE, 0, "mute SetParam(mute)");
    expect_mute(track, 7, 50, 7, FALSE, 50, "mute SetParam(mute)");
    p.dwPChannelMap = 12; p.fMute = FALSE;
    IDirectMusicTrack_SetParam(track, &GUID_MuteParam, 200, &p);
    expect_mute(track, 7, 250, 12, FALSE, 0, "mute SetParam(map)");

    copy = clone(track, 1500, 3500);
    if (copy)
    {
        expect_mute(copy, 3, 0, 0xffffffff, TRUE, 1500, "mute clone [1500, 3500)");
        expect_mute(copy, 3, 1500, 3, FALSE, 0, "mute clone [1500, 3500)");
        expect_mute(copy, 5, 600, 9, FALSE, 0, "mute clone [1500, 3500)");
        expect_mute(copy, 5, 0, 5, FALSE, 500, "mute clone [1500, 3500)");
        IDirectMusicTrack_Release(copy);
    }

    stream = new_stream();
    IDirectMusicTrack_QueryInterface(track, &IID_IPersistStream, (void **)&ps);
    hr = IPersistStream_Save(ps, stream, TRUE);
    CHECKF(hr == S_OK, "mute: Save is S_OK (%#lx)", hr);
    memset(bytes, 0, sizeof(bytes));
    i = stream_bytes(stream, bytes, sizeof(bytes));
    CHECKF(i == sizeof(bytes) && bytes[0] == mmioFOURCC('m','u','t','e') && bytes[1] == 4 + 5 * 12 && bytes[2] == 12
            && bytes[3] == 100 && bytes[4] == 7 && bytes[5] == 0xffffffff,
            "mute: Save writes 'mute' with 5 items, the first at 100 for channel 7 (size %lu, %#lx)", bytes[1], bytes[5]);
    rewind_stream(stream);
    copy = create_track(&CLSID_DirectMusicMuteTrack);
    load_from(copy, stream, &hr);
    CHECKF(hr == S_OK, "mute: Load of the saved data is S_OK (%#lx)", hr);
    expect_mute(copy, 7, 250, 12, FALSE, 0, "mute reloaded");
    expect_mute(copy, 3, 1500, 0xffffffff, TRUE, 1500, "mute reloaded");
    IDirectMusicTrack_Release(copy);
    IStream_Release(stream);
    IPersistStream_Release(ps);

    copy = create_track(&CLSID_DirectMusicMuteTrack);
    chunk("cmnd", bytes, 4);
    hr = load_track(copy);
    CHECKF(hr == DMUS_E_UNSUPPORTED_STREAM, "mute: Load of another chunk is DMUS_E_UNSUPPORTED_STREAM (%#lx)", hr);
    IDirectMusicTrack_Release(copy);

    hr = play(track, 0, 5000, 0);
    CHECKF(hr == S_OK, "mute: Play is S_OK (%#lx)", hr);
    EXPECT_COUNT(0, "mute Play");
    hr = IDirectMusicTrack_Play(track, NULL, 0, 5000, 0, 0, NULL, NULL, 0);
    CHECKF(hr == S_OK, "mute: Play without performance is S_OK (%#lx)", hr);

    {
        IDirectMusicTrack8 *track8;
        void *state = NULL;
        REFERENCE_TIME rt;
        DMUS_MUTE_PARAM mp;

        IDirectMusicTrack_QueryInterface(track, &IID_IDirectMusicTrack8, (void **)&track8);
        hr = IDirectMusicTrack8_InitPlay(track8, NULL, perf, &state, 3, 0);
        CHECKF(hr == S_OK && state, "mute: InitPlay with a state pointer gives a state (%#lx)", hr);
        IDirectMusicPerformance_MusicToReferenceTime(perf, 1500, &rt);
        mp.dwPChannel = 3;
        hr = IDirectMusicTrack8_GetParamEx(track8, &GUID_MuteParam, rt, NULL, &mp, state, 0);
        CHECKF(hr == S_OK && mp.fMute && mp.dwPChannelMap == 0xffffffff, "mute: GetParamEx at 1500 is muted (%#lx)", hr);
        IDirectMusicPerformance_MusicToReferenceTime(perf, 3500, &rt);
        hr = IDirectMusicTrack8_GetParamEx(track8, &GUID_MuteParam, rt, NULL, &mp, state, 0);
        CHECKF(hr == S_OK && !mp.fMute && mp.dwPChannelMap == 3, "mute: GetParamEx at 3500 is not muted (%#lx)", hr);
        hr = IDirectMusicTrack8_EndPlay(track8, state);
        CHECKF(hr == S_OK, "mute: EndPlay is S_OK (%#lx)", hr);
        IDirectMusicTrack8_Release(track8);
    }

    IDirectMusicTrack_Release(track);
}


/* a style loaded from a RIFF form with a style header (time signature and tempo) and a name */
static IDirectMusicStyle8 *make_style(BYTE beats, BYTE beat, WORD grids, double tempo, const WCHAR *name)
{
    DMUS_IO_STYLE st = {{beats, beat, grids}, tempo};
    IDirectMusicStyle8 *style = NULL;
    IPersistStream *ps;
    IStream *stream;
    int at, list;
    HRESULT hr;

    at = begin("RIFF", "DMST");
    chunk("styh", &st, sizeof(st));
    list = begin("LIST", "UNFO");
    chunk("UNAM", name, (lstrlenW(name) + 1) * sizeof(WCHAR));
    end(list);
    end(at);
    stream = make_stream();

    hr = CoCreateInstance(&CLSID_DirectMusicStyle, NULL, CLSCTX_INPROC_SERVER, &IID_IDirectMusicStyle8, (void **)&style);
    CHECKF(hr == S_OK, "create a style (%#lx)", hr);
    IDirectMusicStyle8_QueryInterface(style, &IID_IPersistStream, (void **)&ps);
    hr = IPersistStream_Load(ps, stream);
    CHECKF(hr == S_OK, "load the style %ls (%#lx)", name, hr);
    IPersistStream_Release(ps);
    IStream_Release(stream);
    return style;
}

static ULONG refs(IUnknown *unk)
{
    IUnknown_AddRef(unk);
    return IUnknown_Release(unk);
}

static void expect_style(IDirectMusicTrack *track, MUSIC_TIME time, IDirectMusicStyle8 *style, MUSIC_TIME next, const char *tag)
{
    IDirectMusicStyle8 *got = NULL;
    MUSIC_TIME got_next = 0x1234;
    ULONG before = style ? refs((IUnknown *)style) : 0;
    HRESULT hr = IDirectMusicTrack_GetParam(track, &GUID_IDirectMusicStyle, time, &got_next, &got);
    CHECKF(hr == S_OK && got == style && got_next == next && refs((IUnknown *)style) == before + 1,
            "%s: style at %ld is the expected one, referenced, next %ld (%#lx, %p vs %p, next %ld)", tag, time, next, hr, got, style,
            got_next);
    if (got) IDirectMusicStyle8_Release(got);
}

static void expect_timesig(IDirectMusicTrack *track, MUSIC_TIME time, MUSIC_TIME mt, int beats, int beat, int grids,
        MUSIC_TIME next, const char *tag)
{
    DMUS_TIMESIGNATURE ts;
    MUSIC_TIME got_next = 0x1234;
    HRESULT hr;

    memset(&ts, 0xee, sizeof(ts));
    hr = IDirectMusicTrack_GetParam(track, &GUID_TimeSignature, time, &got_next, &ts);
    CHECKF(hr == S_OK && ts.mtTime == mt && ts.bBeatsPerMeasure == beats && ts.bBeat == beat && ts.wGridsPerBeat == grids
            && got_next == next, "%s: time signature at %ld is %d/%d/%d at %ld next %ld (%#lx %d/%d/%d at %ld next %ld)", tag, time,
            beats, beat, grids, mt, next, hr, ts.bBeatsPerMeasure, ts.bBeat, ts.wGridsPerBeat, ts.mtTime, got_next);
}

static void test_style_track(void)
{
    IDirectMusicStyle8 *a = make_style(3, 4, 4, 90.0, L"styleA"), *b = make_style(6, 8, 2, 140.5, L"styleB");
    IDirectMusicTrack *track = create_track(&CLSID_DirectMusicStyleTrack), *copy, *join;
    IDirectMusicStyle8 *got;
    DWORD seed = 77;
    ULONG ra, rb;
    HRESULT hr;

    if (!track || !a || !b) return;
    ra = refs((IUnknown *)a); rb = refs((IUnknown *)b);

    hr = IDirectMusicTrack_GetParam(track, &GUID_IDirectMusicStyle, 0, NULL, &got);
    CHECKF(hr == DMUS_E_NOT_FOUND, "style track: GetParam(style) without styles is DMUS_E_NOT_FOUND (%#lx)", hr);
    hr = IDirectMusicTrack_GetParam(track, &GUID_TimeSignature, 0, NULL, &got);
    CHECKF(hr == DMUS_E_NOT_FOUND, "style track: GetParam(time signature) without styles is DMUS_E_NOT_FOUND (%#lx)", hr);

    hr = IDirectMusicTrack_SetParam(track, &GUID_IDirectMusicStyle, 0, a);
    CHECKF(hr == S_OK, "style track: SetParam(style A at 0) (%#lx)", hr);
    hr = IDirectMusicTrack_SetParam(track, &GUID_IDirectMusicStyle, 3000, b);
    CHECKF(hr == S_OK, "style track: SetParam(style B at 3000) (%#lx)", hr);
    CHECKF(refs((IUnknown *)a) == ra + 1 && refs((IUnknown *)b) == rb + 1, "style track keeps a reference to each style (%lu, %lu)",
            refs((IUnknown *)a), refs((IUnknown *)b));
    hr = IDirectMusicTrack_SetParam(track, &GUID_IDirectMusicStyle, 0, NULL);
    CHECKF(hr == E_POINTER, "style track: SetParam(style) without data is E_POINTER (%#lx)", hr);
    hr = IDirectMusicTrack_SetParam(track, &GUID_IDirectMusicStyle, -1, a);
    CHECKF(hr == E_INVALIDARG, "style track: SetParam(style) at a negative time is E_INVALIDARG (%#lx)", hr);
    hr = IDirectMusicTrack_SetParam(track, &GUID_IDirectMusicStyle, 10, track);
    CHECKF(hr == E_NOINTERFACE, "style track: SetParam(style) of an object which is no style is E_NOINTERFACE (%#lx)", hr);

    expect_style(track, 0, a, 3000, "style track");
    expect_style(track, 2999, a, 1, "style track");
    expect_style(track, 3000, b, 0, "style track");
    expect_style(track, 9000, b, 0, "style track");
    expect_style(track, -50, a, 50, "style track before the first style");
    hr = IDirectMusicTrack_GetParam(track, &GUID_IDirectMusicStyle, 0, NULL, NULL);
    CHECKF(hr == E_POINTER, "style track: GetParam(style) without data is E_POINTER (%#lx)", hr);

    expect_timesig(track, 1000, -1000, 3, 4, 4, 2000, "style track");
    expect_timesig(track, 3000, 0, 6, 8, 2, 0, "style track");
    expect_timesig(track, 4000, -1000, 6, 8, 2, 0, "style track");
    hr = IDirectMusicTrack_SetParam(track, &GUID_DisableTimeSig, 0, NULL);
    CHECKF(hr == S_OK, "style track: SetParam(DisableTimeSig) (%#lx)", hr);
    hr = IDirectMusicTrack_GetParam(track, &GUID_TimeSignature, 1000, NULL, &got);
    CHECKF(hr == DMUS_E_TYPE_DISABLED, "style track: GetParam(time signature) is DMUS_E_TYPE_DISABLED after DisableTimeSig (%#lx)", hr);
    expect_style(track, 0, a, 3000, "style track with the time signature disabled");
    hr = IDirectMusicTrack_SetParam(track, &GUID_EnableTimeSig, 0, NULL);
    CHECKF(hr == S_OK, "style track: SetParam(EnableTimeSig) (%#lx)", hr);
    expect_timesig(track, 3500, -500, 6, 8, 2, 0, "style track enabled again");

    hr = IDirectMusicTrack_SetParam(track, &GUID_SeedVariations, 0, &seed);
    CHECKF(hr == S_OK, "style track: SetParam(SeedVariations) (%#lx)", hr);
    hr = IDirectMusicTrack_SetParam(track, &GUID_SeedVariations, 0, NULL);
    CHECKF(hr == E_POINTER, "style track: SetParam(SeedVariations) without data is E_POINTER (%#lx)", hr);

    /* a style at the time of another one replaces it */
    hr = IDirectMusicTrack_SetParam(track, &GUID_IDirectMusicStyle, 3000, a);
    CHECKF(hr == S_OK, "style track: SetParam(style A at 3000) (%#lx)", hr);
    expect_style(track, 3500, a, 0, "style track after the replace");
    CHECKF(refs((IUnknown *)b) == rb, "style track released the replaced style (%lu)", refs((IUnknown *)b));
    hr = IDirectMusicTrack_SetParam(track, &GUID_IDirectMusicStyle, 3000, b);

    /* Clone and Join */
    copy = clone(track, 3000, 9000);
    if (copy)
    {
        expect_style(copy, 0, b, 0, "style track clone [3000, 9000)");
        expect_timesig(copy, 5, -5, 6, 8, 2, 0, "style track clone [3000, 9000)");
        IDirectMusicTrack_Release(copy);
    }
    copy = clone(track, 1000, 9000);
    if (copy)
    {
        expect_style(copy, 0, a, 2000, "style track clone [1000, 9000)");
        expect_style(copy, 2000, b, 0, "style track clone [1000, 9000)");
        IDirectMusicTrack_Release(copy);
    }
    copy = create_track(&CLSID_DirectMusicStyleTrack);
    IDirectMusicTrack_SetParam(copy, &GUID_IDirectMusicStyle, 0, b);
    join = NULL;
    hr = join_tracks(track, copy, 2000, NULL, 0, &join);
    CHECKF(hr == S_OK && join, "style track: Join is S_OK (%#lx)", hr);
    if (join)
    {
        expect_style(join, 1999, a, 1, "style track join");
        expect_style(join, 2000, b, 0, "style track join");
        IDirectMusicTrack_Release(join);
    }
    hr = join_tracks(track, copy, -1, NULL, 0, &join);
    CHECKF(hr == E_INVALIDARG, "style track: Join at a negative time is E_INVALIDARG (%#lx)", hr);
    hr = join_tracks(track, NULL, 0, NULL, 0, &join);
    CHECKF(hr == E_POINTER, "style track: Join without a track is E_POINTER (%#lx)", hr);
    IDirectMusicTrack_Release(copy);

    hr = IDirectMusicTrack_AddNotificationType(track, &GUID_NOTIFICATION_SEGMENT);
    CHECKF(hr == S_FALSE, "style track: AddNotificationType is S_FALSE (%#lx)", hr);
    hr = IDirectMusicTrack_RemoveNotificationType(track, &GUID_NOTIFICATION_SEGMENT);
    CHECKF(hr == S_FALSE, "style track: RemoveNotificationType is S_FALSE (%#lx)", hr);

    {
        IDirectMusicTrack8 *track8;
        void *state = NULL;
        REFERENCE_TIME rt;
        DMUS_TIMESIGNATURE ts;

        IDirectMusicTrack_QueryInterface(track, &IID_IDirectMusicTrack8, (void **)&track8);
        hr = IDirectMusicTrack8_InitPlay(track8, NULL, perf, &state, 5, 0);
        CHECKF(hr == S_OK && state, "style track: InitPlay gives a state (%#lx)", hr);
        IDirectMusicPerformance_MusicToReferenceTime(perf, 3500, &rt);
        hr = IDirectMusicTrack8_GetParamEx(track8, &GUID_TimeSignature, rt, NULL, &ts, state, 0);
        CHECKF(hr == S_OK && ts.bBeatsPerMeasure == 6 && abs(ts.mtTime + 500) <= 2, "style track: GetParamEx(time signature) at 3500 (%#lx, %d, %ld)",
                hr, ts.bBeatsPerMeasure, ts.mtTime);
        IDirectMusicPerformance_MusicToReferenceTime(perf, 6000, &rt);
        hr = IDirectMusicTrack8_SetParamEx(track8, &GUID_IDirectMusicStyle, rt, a, state, 0);
        CHECKF(hr == S_OK, "style track: SetParamEx(style) (%#lx)", hr);
        expect_style(track, 6001, a, 0, "style track SetParamEx");
        hr = IDirectMusicTrack8_EndPlay(track8, state);
        CHECKF(hr == S_OK, "style track: EndPlay (%#lx)", hr);
        IDirectMusicTrack8_Release(track8);
    }

    IDirectMusicTrack_Release(track);
    CHECKF(refs((IUnknown *)a) == ra && refs((IUnknown *)b) == rb, "style track released its styles (%lu, %lu, expected %lu, %lu)",
            refs((IUnknown *)a), refs((IUnknown *)b), ra, rb);
    IDirectMusicStyle8_Release(a);
    IDirectMusicStyle8_Release(b);
}

static void test_motif_audition(void)
{
    static const struct { const CLSID *clsid; const char *name; } classes[] = {
        {&CLSID_DirectMusicMotifTrack, "motif"}, {&CLSID_DirectMusicAuditionTrack, "audition"},
    };
    unsigned int i;

    for (i = 0; i < ARRAY_SIZE_(classes); i++)
    {
        IDirectMusicTrack *track = create_track(classes[i].clsid), *copy;
        const char *name = classes[i].name;
        DWORD seed = 5;
        char buf[64] = {0};
        HRESULT hr;

        if (!track) continue;

        hr = IDirectMusicTrack_GetParam(track, &GUID_Valid_Start_Time, 0, NULL, buf);
        CHECKF(hr == DMUS_E_NOT_FOUND, "%s: GetParam(Valid_Start_Time) of a track without data is DMUS_E_NOT_FOUND (%#lx)", name, hr);
        hr = IDirectMusicTrack_GetParam(track, &GUID_Valid_Start_Time, 0, NULL, NULL);
        CHECKF(hr == E_POINTER, "%s: GetParam(Valid_Start_Time) without data is E_POINTER (%#lx)", name, hr);
        hr = IDirectMusicTrack_SetParam(track, &GUID_SeedVariations, 0, &seed);
        CHECKF(hr == S_OK, "%s: SetParam(SeedVariations) (%#lx)", name, hr);
        hr = IDirectMusicTrack_SetParam(track, &GUID_SeedVariations, 0, NULL);
        CHECKF(hr == E_POINTER, "%s: SetParam(SeedVariations) without data is E_POINTER (%#lx)", name, hr);
        hr = IDirectMusicTrack_SetParam(track, &GUID_DisableTimeSig, 0, NULL);
        CHECKF(hr == S_OK, "%s: SetParam(DisableTimeSig) (%#lx)", name, hr);
        hr = IDirectMusicTrack_SetParam(track, &GUID_EnableTimeSig, 0, NULL);
        CHECKF(hr == S_OK, "%s: SetParam(EnableTimeSig) (%#lx)", name, hr);
        if (classes[i].clsid == &CLSID_DirectMusicAuditionTrack)
        {
            hr = IDirectMusicTrack_GetParam(track, &GUID_Variations, 0, NULL, buf);
            CHECKF(hr == DMUS_E_NOT_FOUND, "%s: GetParam(Variations) of a track without data is DMUS_E_NOT_FOUND (%#lx)", name, hr);
        }

        hr = IDirectMusicTrack_AddNotificationType(track, &GUID_NOTIFICATION_SEGMENT);
        CHECKF(hr == S_FALSE, "%s: AddNotificationType is S_FALSE (%#lx)", name, hr);
        hr = IDirectMusicTrack_RemoveNotificationType(track, &GUID_NOTIFICATION_SEGMENT);
        CHECKF(hr == S_FALSE, "%s: RemoveNotificationType is S_FALSE (%#lx)", name, hr);

        copy = clone(track, 0, 100);
        if (copy) IDirectMusicTrack_Release(copy);
        hr = IDirectMusicTrack_Clone(track, 10, 5, &copy);
        CHECKF(hr == E_INVALIDARG, "%s: Clone(10, 5) is E_INVALIDARG (%#lx)", name, hr);

        if (classes[i].clsid == &CLSID_DirectMusicMotifTrack)
        {
            IPersistStream *ps;
            IStream *stream = make_stream();
            IDirectMusicTrack_QueryInterface(track, &IID_IPersistStream, (void **)&ps);
            hr = IPersistStream_Load(ps, stream);
            CHECKF(hr == E_NOTIMPL, "%s: Load is E_NOTIMPL (%#lx)", name, hr);
            IPersistStream_Release(ps);
            IStream_Release(stream);
        }

        hr = IDirectMusicTrack_Play(track, NULL, 0, 100, 0, 0, NULL, NULL, 0);
        CHECKF(hr == E_POINTER, "%s: Play without performance is E_POINTER (%#lx)", name, hr);

        IDirectMusicTrack_Release(track);
    }
}

int main(void)
{
    CoInitialize(NULL);
    if (!setup_perf()) return 77;

    test_command();
    test_chord();
    test_mute();
    test_style_track();
    test_motif_audition();

    teardown_perf();
    CoUninitialize();
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
