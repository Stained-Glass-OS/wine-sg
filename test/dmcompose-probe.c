/* dmcompos composer (patches/sg/2918-2919), run by test/dmcompose-gate.sh: ComposeTemplateFromShape,
 * ComposeSegmentFromTemplate, ComposeSegmentFromShape, signpost track Compose, ComposeTransition and AutoTransition.
 * The results are compared with golden strings: for a template its length, the commands (measure/command/level) and
 * the signposts (measure/flags, read back from the saved track); for a segment its length and chords (name@measure.beat);
 * the golden values were produced by the implementation and reviewed (see notes/2918.txt: the algorithm is the
 * implementation's own, the documentation does not give one).
 *
 *   dmcompose-probe.exe [dump]   (dump prints the strings instead of comparing) */
#include "dmcontent-common.h"

static int dump;

static IDirectMusicComposer *composer;

/* ---- builders ---- */
static IDirectMusicStyle *make_style(BYTE beats, BYTE beat, double tempo)
{
    DMUS_IO_STYLE st = {{beats, beat, 4}, tempo};
    IDirectMusicStyle *style = NULL;
    IPersistStream *ps;
    IStream *stream;
    HRESULT hr;
    int at, list;

    at = begin("RIFF", "DMST");
    chunk("styh", &st, sizeof(st));
    list = begin("LIST", "UNFO");
    chunk("UNAM", L"s", 4);
    end(list);
    end(at);
    stream = make_stream();
    hr = CoCreateInstance(&CLSID_DirectMusicStyle, NULL, CLSCTX_INPROC_SERVER, &IID_IDirectMusicStyle, (void **)&style);
    CHECKF(hr == S_OK, "create a style (%#lx)", hr);
    IDirectMusicStyle_QueryInterface(style, &IID_IPersistStream, (void **)&ps);
    hr = IPersistStream_Load(ps, stream);
    CHECKF(hr == S_OK, "load the style (%#lx)", hr);
    IPersistStream_Release(ps);
    IStream_Release(stream);
    return style;
}

static IDirectMusicChordMap *make_chordmap(DWORD scale)
{
    DMUS_IO_CHORDMAP header = {{0}};
    IDirectMusicChordMap *map = NULL;
    IPersistStream *ps;
    IStream *stream;
    HRESULT hr;
    int at;

    lstrcpyW(header.wszLoadName, L"map");
    header.dwScalePattern = scale;
    at = begin("RIFF", "DMPR");
    chunk("perh", &header, sizeof(header));
    end(at);
    stream = make_stream();
    CoCreateInstance(&CLSID_DirectMusicChordMap, NULL, CLSCTX_INPROC_SERVER, &IID_IDirectMusicChordMap, (void **)&map);
    IDirectMusicChordMap_QueryInterface(map, &IID_IPersistStream, (void **)&ps);
    hr = IPersistStream_Load(ps, stream);
    CHECKF(hr == S_OK, "load a chord map (%#lx)", hr);
    IPersistStream_Release(ps);
    IStream_Release(stream);
    return map;
}

#define MAJOR 0xab5ab5
#define MINOR 0x5ad5ad

/* ---- describing segments ---- */
static char text[4096];

static void append(const char *fmt, ...)
{
    va_list args;
    size_t len = strlen(text);

    va_start(args, fmt);
    vsnprintf(text + len, sizeof(text) - len, fmt, args);
    va_end(args);
}

/* the commands at the start of the measures (4/4 template measures of 3072) */
static void describe_commands(IDirectMusicSegment *segment, MUSIC_TIME measure_len, MUSIC_TIME length)
{
    MUSIC_TIME t;

    append(" C:");
    for (t = 0; t < length; t += measure_len)
    {
        DMUS_COMMAND_PARAM_2 command;

        if (SUCCEEDED(IDirectMusicSegment_GetParam(segment, &GUID_CommandParam2, 0xFFFFFFFF, DMUS_SEG_ANYTRACK, t,
                NULL, &command)) && command.mtTime == 0)
            append("%ld/%u/%u,", t / measure_len, command.bCommand, command.bGrooveLevel);
    }
}

static void describe_signposts(IDirectMusicSegment *segment)
{
    IDirectMusicTrack *track = NULL;
    IPersistStream *ps;
    IStream *stream;
    DWORD buffer[64] = {0};
    int n, i;

    append(" S:");
    if (FAILED(IDirectMusicSegment_GetTrack(segment, &CLSID_DirectMusicSignPostTrack, 0xFFFFFFFF, 0, &track)))
    {
        append("none");
        return;
    }
    IDirectMusicTrack_QueryInterface(track, &IID_IPersistStream, (void **)&ps);
    stream = new_stream();
    IPersistStream_Save(ps, stream, FALSE);
    n = stream_bytes(stream, buffer, sizeof(buffer));
    /* sgnp chunk: id, size, item size, then {time, chords, measure (+ padding)} */
    for (i = 0; 12 + (i + 1) * 12 <= n && i < (int)((buffer[1] - 4) / buffer[2]); i++)
        append("%lu/%04lx,", buffer[3 + i * 3 + 2] & 0xffff, buffer[3 + i * 3 + 1]);
    IStream_Release(stream);
    IPersistStream_Release(ps);
    IDirectMusicTrack_Release(track);
}

static void describe_chords(IDirectMusicSegment *segment)
{
    MUSIC_TIME t = 0, next;
    DMUS_CHORD_KEY chord;

    append(" H:");
    while (SUCCEEDED(IDirectMusicSegment_GetParam(segment, &GUID_ChordParam, 0xFFFFFFFF, DMUS_SEG_ANYTRACK, t, &next, &chord)))
    {
        append("%ls@%u.%u,", chord.wszName, chord.wMeasure, chord.bBeat);
        if (next <= 0) break;
        t += next;
    }
}

static const char *describe_template(IDirectMusicSegment *segment)
{
    MUSIC_TIME length = 0;

    text[0] = 0;
    IDirectMusicSegment_GetLength(segment, &length);
    append("len=%ld", length);
    describe_commands(segment, 3072, length);
    describe_signposts(segment);
    return text;
}

static const char *describe_segment(IDirectMusicSegment *segment, MUSIC_TIME measure_len)
{
    MUSIC_TIME length = 0;

    text[0] = 0;
    IDirectMusicSegment_GetLength(segment, &length);
    append("len=%ld", length);
    describe_commands(segment, measure_len, length);
    describe_chords(segment);
    return text;
}

static void expect_text(const char *what, const char *got, const char *want)
{
    if (dump) printf("DUMP  %s = %s\n", want, got);
    else CHECKF(!strcmp(got, want), "%s: %s (want %s)", what, got, want);
}

/* ---- templates ---- */
struct template_row
{
    WORD measures, shape;
    BOOL intro, end;
    WORD end_length;
    const char *want;
};

static const struct template_row template_rows[] = {
    {8,  DMUS_SHAPET_FALLING,  FALSE, FALSE, 0, "len=24576 C:0/0/100,1/0/86,2/0/72,3/0/58,4/0/43,5/0/29,6/0/15,7/0/1, S:0/0100,4/0100,"},
    {8,  DMUS_SHAPET_RISING,   FALSE, FALSE, 0, "len=24576 C:0/0/1,1/0/14,2/0/28,3/0/42,4/0/57,5/0/71,6/0/85,7/0/100, S:0/0100,4/1000,"},
    {12, DMUS_SHAPET_PEAKING,  FALSE, FALSE, 0, "len=36864 C:0/0/1,1/0/18,2/0/36,3/0/54,4/0/72,5/0/90,6/0/92,7/0/74,8/0/56,9/0/38,10/0/20,11/0/1, S:0/0100,4/1000,8/0100,"},
    {8,  DMUS_SHAPET_LEVEL,    FALSE, FALSE, 0, "len=24576 C:0/0/50, S:0/0100,4/0100,"},
    {8,  DMUS_SHAPET_LOUD,     FALSE, FALSE, 0, "len=24576 C:0/0/90, S:0/0100,4/1000,"},
    {8,  DMUS_SHAPET_QUIET,    FALSE, FALSE, 0, "len=24576 C:0/0/10, S:0/0100,4/0100,"},
    {8,  DMUS_SHAPET_LOOPABLE, FALSE, FALSE, 0, "len=24576 C:0/0/50, S:0/0100,4/1000,"},
    {8,  DMUS_SHAPET_RANDOM,   FALSE, FALSE, 0, "len=24576 C:0/0/56,1/0/6,2/0/14,3/0/61,4/0/79,5/0/41,6/0/91,7/0/23, S:0/0100,4/1000,"},
    {16, DMUS_SHAPET_SONG,     FALSE, FALSE, 0, "len=49152 C:0/0/40,4/0/80,8/0/40,12/0/80, S:0/0100,4/0800,8/0100,12/1000,"},
    {8,  DMUS_SHAPET_RISING,   TRUE,  FALSE, 0, "len=24576 C:0/2/1,1/0/1,2/0/16,3/0/33,4/0/50,5/0/66,6/0/83,7/0/100, S:0/0100,1/0100,5/1000,"},
    {8,  DMUS_SHAPET_RISING,   FALSE, TRUE,  2, "len=24576 C:0/0/1,1/0/20,2/0/40,3/0/60,4/0/80,5/0/100,6/4/100, S:0/0100,4/1000,6/8100,"},
    {8,  DMUS_SHAPET_RISING,   TRUE,  TRUE,  0, "len=24576 C:0/2/1,1/0/1,2/0/20,3/0/40,4/0/60,5/0/80,6/0/100,7/4/100, S:0/0100,1/0100,5/1000,7/8100,"},
    {1,  DMUS_SHAPET_FALLING,  FALSE, FALSE, 0, "len=3072 C:0/0/100, S:0/0100,"},
};

static void test_template(void)
{
    IDirectMusicSegment *segment;
    HRESULT hr;
    int i;

    for (i = 0; i < ARRAY_SIZE_(template_rows); i++)
    {
        const struct template_row *row = template_rows + i;
        char what[96];

        segment = NULL;
        hr = IDirectMusicComposer_ComposeTemplateFromShape(composer, row->measures, row->shape, row->intro, row->end,
                row->end_length, &segment);
        snprintf(what, sizeof(what), "template %u measures, shape %u, intro %d, end %d/%u (%#lx)", row->measures,
                row->shape, row->intro, row->end, row->end_length, hr);
        CHECKF(hr == S_OK && segment, "%s is S_OK", what);
        if (!segment) continue;
        expect_text(what, describe_template(segment), row->want);
        IDirectMusicSegment_Release(segment);
    }

    /* the same arguments give the same template (a fixed seed) */
    {
        char first[4096];
        IDirectMusicSegment *a = NULL, *b = NULL;

        IDirectMusicComposer_ComposeTemplateFromShape(composer, 20, DMUS_SHAPET_RANDOM, TRUE, TRUE, 2, &a);
        strcpy(first, describe_template(a));
        IDirectMusicComposer_ComposeTemplateFromShape(composer, 20, DMUS_SHAPET_RANDOM, TRUE, TRUE, 2, &b);
        CHECKF(!strcmp(first, describe_template(b)), "a random template is the same every time");
        IDirectMusicSegment_Release(a);
        IDirectMusicSegment_Release(b);
    }

    /* errors */
    hr = IDirectMusicComposer_ComposeTemplateFromShape(composer, 8, DMUS_SHAPET_LEVEL, FALSE, FALSE, 0, NULL);
    CHECKF(hr == E_POINTER, "template without a result pointer is E_POINTER (%#lx)", hr);
    segment = (IDirectMusicSegment *)0xdeadbeef;
    hr = IDirectMusicComposer_ComposeTemplateFromShape(composer, 0, DMUS_SHAPET_LEVEL, FALSE, FALSE, 0, &segment);
    CHECKF(hr == E_INVALIDARG && !segment, "template of 0 measures is E_INVALIDARG and clears the result (%#lx)", hr);
    hr = IDirectMusicComposer_ComposeTemplateFromShape(composer, 8, DMUS_SHAPET_SONG + 1, FALSE, FALSE, 0, &segment);
    CHECKF(hr == E_INVALIDARG, "unknown shape is E_INVALIDARG (%#lx)", hr);
    hr = IDirectMusicComposer_ComposeTemplateFromShape(composer, 8, DMUS_SHAPET_LEVEL, FALSE, TRUE, 5, &segment);
    CHECKF(hr == E_INVALIDARG, "ending of 5 measures is E_INVALIDARG (%#lx)", hr);
    hr = IDirectMusicComposer_ComposeTemplateFromShape(composer, 2, DMUS_SHAPET_LEVEL, TRUE, TRUE, 2, &segment);
    CHECKF(hr == E_INVALIDARG, "intro and ending longer than the template is E_INVALIDARG (%#lx)", hr);
}

/* ---- segments from templates ---- */
static IDirectMusicSegment *make_template(WORD measures, WORD shape, BOOL intro, BOOL end, WORD end_length)
{
    IDirectMusicSegment *template = NULL;

    IDirectMusicComposer_ComposeTemplateFromShape(composer, measures, shape, intro, end, end_length, &template);
    return template;
}

struct segment_row
{
    WORD measures, shape;
    BOOL intro, end;
    WORD end_length, activity;
    int style;    /* 0: 4/4, 1: 3/4 */
    DWORD scale;
    const char *want;
};

static const struct segment_row segment_rows[] = {
    {8, DMUS_SHAPET_RISING, FALSE, FALSE, 0, 0, 0, MAJOR, "len=24576 C:0/0/1,1/0/14,2/0/28,3/0/42,4/0/57,5/0/71,6/0/85,7/0/100, H:CM@0.0,GM@4.0,"},
    {8, DMUS_SHAPET_RISING, FALSE, FALSE, 0, 1, 0, MAJOR, "len=24576 C:0/0/1,1/0/14,2/0/28,3/0/42,4/0/57,5/0/71,6/0/85,7/0/100, H:CM@0.0,FM@2.0,GM@4.0,Dm@6.0,"},
    {8, DMUS_SHAPET_RISING, FALSE, FALSE, 0, 2, 0, MAJOR, "len=24576 C:0/0/1,1/0/14,2/0/28,3/0/42,4/0/57,5/0/71,6/0/85,7/0/100, H:CM@0.0,FM@1.0,Dm@2.0,GM@3.0,GM@4.0,Dm@5.0,GM@6.0,Dm@7.0,"},
    {8, DMUS_SHAPET_RISING, FALSE, FALSE, 0, 3, 0, MAJOR, "len=24576 C:0/0/1,1/0/14,2/0/28,3/0/42,4/0/57,5/0/71,6/0/85,7/0/100, H:CM@0.0,FM@0.2,Dm@1.0,GM@1.2,Em@2.0,FM@2.2,CM@3.0,GM@3.2,GM@4.0,Dm@4.2,GM@5.0,Dm@5.2,GM@6.0,CM@6.2,Dm@7.0,Bdim@7.2,"},
    {8, DMUS_SHAPET_RISING, TRUE,  TRUE,  2, 2, 0, MAJOR, "len=24576 C:0/2/1,1/0/1,2/0/25,3/0/50,4/0/75,5/0/100,6/4/100, H:CM@0.0,CM@1.0,FM@2.0,GM@3.0,CM@4.0,GM@5.0,GM@6.0,CM@7.0,"},
    {8, DMUS_SHAPET_FALLING, FALSE, FALSE, 0, 1, 0, MINOR, "len=24576 C:0/0/100,1/0/86,2/0/72,3/0/58,4/0/43,5/0/29,6/0/15,7/0/1, H:Cm@0.0,Fm@2.0,Cm@4.0,Gm@6.0,"},
    {8, DMUS_SHAPET_RISING, FALSE, FALSE, 0, 1, 1, MAJOR, "len=18432 C:0/0/1,1/0/14,2/0/28,3/0/42,4/0/57,5/0/71,6/0/85,7/0/100, H:CM@0.0,FM@2.2,GM@4.0,Dm@6.2,"},
    {4, DMUS_SHAPET_LOOPABLE, FALSE, FALSE, 0, 3, 0, MAJOR, "len=12288 C:0/0/50, H:CM@0.0,FM@0.2,Dm@1.0,GM@1.2,Em@2.0,FM@2.2,CM@3.0,GM@3.2,"},
};

static MUSIC_TIME measure_len_of(int style) { return style ? 3 * 768 : 4 * 768; }

static void test_from_template(IDirectMusicStyle *style44, IDirectMusicStyle *style34, IDirectMusicChordMap *major,
        IDirectMusicChordMap *minor)
{
    IDirectMusicSegment *template, *segment;
    HRESULT hr;
    int i;

    for (i = 0; i < ARRAY_SIZE_(segment_rows); i++)
    {
        const struct segment_row *row = segment_rows + i;
        IDirectMusicStyle *style = row->style ? style34 : style44;
        IDirectMusicChordMap *map = row->scale == MINOR ? minor : major;
        char what[128];

        template = make_template(row->measures, row->shape, row->intro, row->end, row->end_length);
        segment = NULL;
        hr = IDirectMusicComposer_ComposeSegmentFromTemplate(composer, style, template, row->activity, map, &segment);
        snprintf(what, sizeof(what), "segment from template: %u measures, shape %u, intro %d, end %d, activity %u, style %d, scale %lx (%#lx)",
                row->measures, row->shape, row->intro, row->end, row->activity, row->style, row->scale, hr);
        CHECKF(hr == S_OK && segment, "%s is S_OK", what);
        if (segment)
        {
            expect_text(what, describe_segment(segment, measure_len_of(row->style)), row->want);
            IDirectMusicSegment_Release(segment);
        }
        IDirectMusicSegment_Release(template);
    }

    /* the tracks of the segment */
    template = make_template(8, DMUS_SHAPET_RISING, FALSE, FALSE, 0);
    segment = NULL;
    hr = IDirectMusicComposer_ComposeSegmentFromTemplate(composer, style34, template, 1, minor, &segment);
    CHECKF(hr == S_OK && segment, "compose for the track checks (%#lx)", hr);
    if (segment)
    {
        IDirectMusicStyle *got_style = NULL;
        IDirectMusicChordMap *got_map = NULL;
        DMUS_TEMPO_PARAM tempo = {0};
        DWORD scale = 0;

        hr = IDirectMusicSegment_GetParam(segment, &GUID_IDirectMusicStyle, 0xFFFFFFFF, DMUS_SEG_ANYTRACK, 0, NULL, &got_style);
        CHECKF(hr == S_OK && got_style == style34, "the segment has the style (%#lx)", hr);
        if (got_style) IDirectMusicStyle_Release(got_style);
        hr = IDirectMusicSegment_GetParam(segment, &GUID_IDirectMusicChordMap, 0xFFFFFFFF, DMUS_SEG_ANYTRACK, 0, NULL, &got_map);
        CHECKF(hr == S_OK && got_map == minor, "the segment has the chord map (%#lx)", hr);
        if (got_map)
        {
            IDirectMusicChordMap_GetScale(got_map, &scale);
            IDirectMusicChordMap_Release(got_map);
        }
        hr = IDirectMusicSegment_GetParam(segment, &GUID_TempoParam, 0xFFFFFFFF, DMUS_SEG_ANYTRACK, 0, NULL, &tempo);
        CHECKF(hr == S_OK && tempo.dblTempo == 120.0, "the segment has the tempo of the style (%#lx, %f)", hr, tempo.dblTempo);
        IDirectMusicSegment_Release(segment);
    }

    /* determinism: the same arguments, the same chords */
    {
        char first[4096];
        IDirectMusicSegment *a = NULL, *b = NULL;
        IDirectMusicSegment *big = make_template(24, DMUS_SHAPET_RANDOM, TRUE, TRUE, 2);

        IDirectMusicComposer_ComposeSegmentFromTemplate(composer, style44, big, 3, major, &a);
        strcpy(first, describe_segment(a, 3072));
        IDirectMusicComposer_ComposeSegmentFromTemplate(composer, style44, big, 3, major, &b);
        CHECKF(!strcmp(first, describe_segment(b, 3072)), "composing twice gives the same chords");
        IDirectMusicSegment_Release(a);
        IDirectMusicSegment_Release(b);
        IDirectMusicSegment_Release(big);
    }

    /* errors */
    segment = (IDirectMusicSegment *)0xdeadbeef;
    hr = IDirectMusicComposer_ComposeSegmentFromTemplate(composer, style44, template, 1, major, NULL);
    CHECKF(hr == E_POINTER, "no result pointer is E_POINTER (%#lx)", hr);
    hr = IDirectMusicComposer_ComposeSegmentFromTemplate(composer, style44, NULL, 1, major, &segment);
    CHECKF(hr == E_INVALIDARG, "no template is E_INVALIDARG (%#lx)", hr);
    hr = IDirectMusicComposer_ComposeSegmentFromTemplate(composer, style44, template, 4, major, &segment);
    CHECKF(hr == E_INVALIDARG, "activity 4 is E_INVALIDARG (%#lx)", hr);
    hr = IDirectMusicComposer_ComposeSegmentFromTemplate(composer, NULL, template, 1, major, &segment);
    CHECKF(hr == E_INVALIDARG, "no style and no style track in the template is E_INVALIDARG (%#lx)", hr);
    hr = IDirectMusicComposer_ComposeSegmentFromTemplate(composer, style44, template, 1, NULL, &segment);
    CHECKF(hr == E_INVALIDARG, "no chord map and no chord map track in the template is E_INVALIDARG (%#lx)", hr);

    /* a template which has the style and the chord map in tracks does not need them as arguments */
    {
        IDirectMusicSegment *with_tracks = make_template(8, DMUS_SHAPET_RISING, FALSE, FALSE, 0), *composed = NULL;
        IDirectMusicSegment *again = NULL;
        IDirectMusicTrack *t;
        char first[4096];

        t = create_track(&CLSID_DirectMusicStyleTrack);
        IDirectMusicTrack_SetParam(t, &GUID_IDirectMusicStyle, 0, style44);
        IDirectMusicSegment_InsertTrack(with_tracks, t, 1);
        IDirectMusicTrack_Release(t);
        t = create_track(&CLSID_DirectMusicChordMapTrack);
        IDirectMusicTrack_SetParam(t, &GUID_IDirectMusicChordMap, 0, major);
        IDirectMusicSegment_InsertTrack(with_tracks, t, 1);
        IDirectMusicTrack_Release(t);

        hr = IDirectMusicComposer_ComposeSegmentFromTemplate(composer, NULL, with_tracks, 1, NULL, &composed);
        CHECKF(hr == S_OK && composed, "a template with a style track and a chord map track needs no arguments (%#lx)", hr);
        IDirectMusicComposer_ComposeSegmentFromTemplate(composer, style44, template, 1, major, &again);
        if (composed && again)
        {
            strcpy(first, describe_segment(composed, 3072));
            CHECKF(!strcmp(first, describe_segment(again, 3072)), "arguments from the tracks give the same segment");
        }
        if (composed) IDirectMusicSegment_Release(composed);
        if (again) IDirectMusicSegment_Release(again);
        IDirectMusicSegment_Release(with_tracks);
    }
    IDirectMusicSegment_Release(template);
}

/* ---- from shapes ---- */
static void test_from_shape(IDirectMusicStyle *style44, IDirectMusicChordMap *major)
{
    static const struct { WORD shape, activity; BOOL intro, end; } rows[] = {
        {DMUS_SHAPET_FALLING, 0, FALSE, FALSE}, {DMUS_SHAPET_PEAKING, 2, TRUE, FALSE},
        {DMUS_SHAPET_SONG, 3, FALSE, TRUE}, {DMUS_SHAPET_RANDOM, 1, TRUE, TRUE},
    };
    int i;
    HRESULT hr;
    IDirectMusicSegment *segment = NULL;

    for (i = 0; i < ARRAY_SIZE_(rows); i++)
    {
        IDirectMusicSegment *shape_segment = NULL, *template, *template_segment = NULL;
        char first[4096];

        hr = IDirectMusicComposer_ComposeSegmentFromShape(composer, style44, 16, rows[i].shape, rows[i].activity,
                rows[i].intro, rows[i].end, major, &shape_segment);
        CHECKF(hr == S_OK && shape_segment, "ComposeSegmentFromShape(shape %u, activity %u, intro %d, end %d) (%#lx)",
                rows[i].shape, rows[i].activity, rows[i].intro, rows[i].end, hr);
        if (!shape_segment) continue;
        strcpy(first, describe_segment(shape_segment, 3072));
        /* the template of the shape has an ending of one measure */
        template = make_template(16, rows[i].shape, rows[i].intro, rows[i].end, 1);
        IDirectMusicComposer_ComposeSegmentFromTemplate(composer, style44, template, rows[i].activity, major, &template_segment);
        CHECKF(template_segment && !strcmp(first, describe_segment(template_segment, 3072)),
                "the segment of shape %u is the segment of its template", rows[i].shape);
        expect_text("shape segment", first, i == 0 ? "len=49152 C:0/0/100,1/0/94,2/0/87,3/0/80,4/0/74,5/0/67,6/0/60,7/0/54,8/0/47,9/0/40,10/0/34,11/0/27,12/0/20,13/0/14,14/0/7,15/0/1, H:CM@0.0,FM@4.0,FM@8.0,CM@12.0," : i == 1 ? "len=49152 C:0/2/1,1/0/1,2/0/14,3/0/28,4/0/42,5/0/56,6/0/70,7/0/84,8/0/100,9/0/86,10/0/72,11/0/58,12/0/44,13/0/30,14/0/16,15/0/1, H:CM@0.0,CM@1.0,FM@2.0,GM@3.0,CM@4.0,GM@5.0,Am@6.0,Dm@7.0,Am@8.0,GM@9.0,Am@10.0,Dm@11.0,GM@12.0,CM@13.0,FM@14.0,Dm@15.0," : i == 2 ? "len=49152 C:0/0/40,4/0/80,8/0/40,12/0/80,15/4/80, H:CM@0.0,FM@0.2,Dm@1.0,GM@1.2,Em@2.0,FM@2.2,CM@3.0,GM@3.2,FM@4.0,CM@4.2,FM@5.0,CM@5.2,FM@6.0,Bdim@6.2,CM@7.0,Am@7.2,CM@8.0,FM@8.2,Bdim@9.0,CM@9.2,FM@10.0,GM@10.2,Em@11.0,Bdim@11.2,GM@12.0,Em@12.2,Am@13.0,Em@13.2,Bdim@14.0,Em@14.2,CM@15.0,CM@15.2," : "len=49152 C:0/2/56,1/0/56,2/0/6,3/0/14,4/0/61,5/0/79,6/0/41,7/0/91,8/0/23,9/0/19,10/0/58,11/0/17,12/0/98,13/0/2,14/0/40,15/4/40, H:CM@0.0,CM@1.0,FM@3.0,GM@5.0,Am@7.0,CM@9.0,Dm@11.0,CM@13.0,CM@15.0,");
        if (template_segment) IDirectMusicSegment_Release(template_segment);
        IDirectMusicSegment_Release(template);
        IDirectMusicSegment_Release(shape_segment);
    }

    hr = IDirectMusicComposer_ComposeSegmentFromShape(composer, NULL, 8, DMUS_SHAPET_LEVEL, 1, FALSE, FALSE, major, &segment);
    CHECKF(hr == E_POINTER, "no style is E_POINTER (%#lx)", hr);
    hr = IDirectMusicComposer_ComposeSegmentFromShape(composer, style44, 8, DMUS_SHAPET_LEVEL, 1, FALSE, FALSE, NULL, &segment);
    CHECKF(hr == E_POINTER, "no chord map is E_POINTER (%#lx)", hr);
    hr = IDirectMusicComposer_ComposeSegmentFromShape(composer, style44, 8, DMUS_SHAPET_LEVEL, 1, FALSE, FALSE, major, NULL);
    CHECKF(hr == E_POINTER, "no result pointer is E_POINTER (%#lx)", hr);
    hr = IDirectMusicComposer_ComposeSegmentFromShape(composer, style44, 0, DMUS_SHAPET_LEVEL, 1, FALSE, FALSE, major, &segment);
    CHECKF(hr == E_INVALIDARG, "0 measures is E_INVALIDARG (%#lx)", hr);
    hr = IDirectMusicComposer_ComposeSegmentFromShape(composer, style44, 8, DMUS_SHAPET_LEVEL, 4, FALSE, FALSE, major, &segment);
    CHECKF(hr == E_INVALIDARG, "activity 4 is E_INVALIDARG (%#lx)", hr);
    hr = IDirectMusicComposer_ComposeSegmentFromShape(composer, style44, 8, 99, 1, FALSE, FALSE, major, &segment);
    CHECKF(hr == E_INVALIDARG, "shape 99 is E_INVALIDARG (%#lx)", hr);
}

/* ---- the signpost track ---- */
static void signpost_form(const DMUS_IO_SIGNPOST *items, int count)
{
    array_chunk("sgnp", items, sizeof(*items), count);
}

static void test_signpost_compose(IDirectMusicStyle *style34, IDirectMusicChordMap *minor)
{
    DMUS_IO_SIGNPOST items[] = {
        {0, DMUS_SIGNPOSTF_1, 0},
        {2 * 3072, DMUS_SIGNPOSTF_4, 2},
        {4 * 3072, DMUS_SIGNPOSTF_1 | DMUS_SIGNPOSTF_CADENCE, 4},
    };
    IDirectMusicTrack *track = create_track(&CLSID_DirectMusicSignPostTrack), *result = NULL;
    IDirectMusicTrack8 *track8;
    IDirectMusicSegment *segment = NULL, *plain = NULL;
    IDirectMusicSegment8 *segment8;
    HRESULT hr;
    IPersistStream *ps;
    IStream *stream;

    signpost_form(items, 3);
    stream = make_stream();
    IDirectMusicTrack_QueryInterface(track, &IID_IPersistStream, (void **)&ps);
    hr = IPersistStream_Load(ps, stream);
    CHECKF(hr == S_OK, "load the signposts (%#lx)", hr);
    IPersistStream_Release(ps);
    IStream_Release(stream);
    IDirectMusicTrack_QueryInterface(track, &IID_IDirectMusicTrack8, (void **)&track8);

    CoCreateInstance(&CLSID_DirectMusicSegment, NULL, CLSCTX_INPROC_SERVER, &IID_IDirectMusicSegment, (void **)&segment);
    IDirectMusicSegment_SetLength(segment, 6 * 2304);
    {
        IDirectMusicTrack *t = create_track(&CLSID_DirectMusicStyleTrack);
        IDirectMusicTrack_SetParam(t, &GUID_IDirectMusicStyle, 0, style34);
        IDirectMusicSegment_InsertTrack(segment, t, 1);
        IDirectMusicTrack_Release(t);
        t = create_track(&CLSID_DirectMusicChordMapTrack);
        IDirectMusicTrack_SetParam(t, &GUID_IDirectMusicChordMap, 0, minor);
        IDirectMusicSegment_InsertTrack(segment, t, 1);
        IDirectMusicTrack_Release(t);
    }

    hr = IDirectMusicTrack8_Compose(track8, NULL, 0, NULL);
    CHECKF(hr == E_POINTER, "Compose without the result is E_POINTER (%#lx)", hr);
    hr = IDirectMusicTrack8_Compose(track8, NULL, 0, &result);
    CHECKF(hr == E_INVALIDARG && !result, "Compose without a context is E_INVALIDARG (%#lx)", hr);
    hr = IDirectMusicTrack8_Compose(track8, (IUnknown *)composer, 0, &result);
    CHECKF(hr == E_INVALIDARG && !result, "Compose with a context which is no segment is E_INVALIDARG (%#lx)", hr);
    hr = IDirectMusicTrack8_Compose(track8, (IUnknown *)segment, 0, &result);
    CHECKF(hr == S_OK && result, "Compose with a segment (%#lx)", hr);
    if (result)
    {
        IDirectMusicSegment *holder = NULL;
        CLSID class = {0};
        IPersistStream *p;

        IDirectMusicTrack_QueryInterface(result, &IID_IPersistStream, (void **)&p);
        IPersistStream_GetClassID(p, &class);
        IPersistStream_Release(p);
        CHECKF(IsEqualGUID(&class, &CLSID_DirectMusicChordTrack), "the composed track is a chord track");
        CoCreateInstance(&CLSID_DirectMusicSegment, NULL, CLSCTX_INPROC_SERVER, &IID_IDirectMusicSegment, (void **)&holder);
        IDirectMusicSegment_SetLength(holder, 6 * 2304);
        IDirectMusicSegment_InsertTrack(holder, result, 1);
        text[0] = 0;
        describe_chords(holder);
        expect_text("signpost Compose (3/4 style, minor)", text, " H:Cm@0.0,Fm@2.0,Gm@4.0,Cm@5.0,");
        IDirectMusicSegment_Release(holder);
        IDirectMusicTrack_Release(result);
    }

    /* IDirectMusicSegment8::Compose replaces the signpost track by the chord track */
    CoCreateInstance(&CLSID_DirectMusicSegment, NULL, CLSCTX_INPROC_SERVER, &IID_IDirectMusicSegment, (void **)&plain);
    IDirectMusicSegment_SetLength(plain, 6 * 2304);
    {
        IDirectMusicTrack *t = create_track(&CLSID_DirectMusicStyleTrack);
        IDirectMusicTrack_SetParam(t, &GUID_IDirectMusicStyle, 0, style34);
        IDirectMusicSegment_InsertTrack(plain, t, 1);
        IDirectMusicTrack_Release(t);
        IDirectMusicSegment_InsertTrack(plain, track, 1);
    }
    IDirectMusicSegment_QueryInterface(plain, &IID_IDirectMusicSegment8, (void **)&segment8);
    {
        IDirectMusicSegment *composed = NULL;
        IDirectMusicTrack *left = NULL;

        hr = IDirectMusicSegment8_Compose(segment8, 0, NULL, NULL, &composed);
        CHECKF(hr == S_OK && composed, "IDirectMusicSegment8::Compose with a signpost track (%#lx)", hr);
        if (composed)
        {
            /* no chord map: the major scale */
            text[0] = 0;
            describe_chords(composed);
            expect_text("segment Compose (3/4 style, no chord map)", text, " H:CM@0.0,FM@2.0,GM@4.0,CM@5.0,");
            hr = IDirectMusicSegment_GetTrack(composed, &CLSID_DirectMusicSignPostTrack, 0xFFFFFFFF, 0, &left);
            CHECKF(FAILED(hr), "the signpost track is gone from the composed segment (%#lx)", hr);
            if (left) IDirectMusicTrack_Release(left);
            IDirectMusicSegment_Release(composed);
        }
    }
    IDirectMusicSegment8_Release(segment8);
    IDirectMusicSegment_Release(plain);
    IDirectMusicTrack8_Release(track8);
    IDirectMusicTrack_Release(track);
    IDirectMusicSegment_Release(segment);
}

/* ---- transitions ---- */
static IDirectMusicSegment *plain_segment(MUSIC_TIME length)
{
    IDirectMusicSegment *segment = NULL;

    CoCreateInstance(&CLSID_DirectMusicSegment, NULL, CLSCTX_INPROC_SERVER, &IID_IDirectMusicSegment, (void **)&segment);
    IDirectMusicSegment_SetLength(segment, length);
    return segment;
}

struct chord_spec { MUSIC_TIME time; int root; DWORD pattern; const WCHAR *name; };

static void add_chords(IDirectMusicSegment *segment, DWORD scale, const struct chord_spec *chords, int count)
{
    IDirectMusicTrack *t = create_track(&CLSID_DirectMusicChordTrack);
    int i;

    for (i = 0; i < count; i++)
    {
        DMUS_CHORD_KEY chord = {{0}};

        lstrcpyW(chord.wszName, chords[i].name);
        chord.bSubChordCount = 1;
        chord.SubChordList[0].dwChordPattern = chords[i].pattern;
        chord.SubChordList[0].dwScalePattern = scale;
        chord.SubChordList[0].bChordRoot = chords[i].root;
        chord.dwScale = scale;
        IDirectMusicTrack_SetParam(t, &GUID_ChordParam, chords[i].time, &chord);
    }
    IDirectMusicSegment_InsertTrack(segment, t, 1);
    IDirectMusicTrack_Release(t);
}

static void add_style(IDirectMusicSegment *segment, IDirectMusicStyle *style)
{
    IDirectMusicTrack *t = create_track(&CLSID_DirectMusicStyleTrack);

    IDirectMusicTrack_SetParam(t, &GUID_IDirectMusicStyle, 0, style);
    IDirectMusicSegment_InsertTrack(segment, t, 1);
    IDirectMusicTrack_Release(t);
}

static void add_groove(IDirectMusicSegment *segment, BYTE level)
{
    IDirectMusicTrack *t = create_track(&CLSID_DirectMusicCommandTrack);
    DMUS_COMMAND_PARAM command = {DMUS_COMMANDT_GROOVE, level, 0, 0};

    IDirectMusicTrack_SetParam(t, &GUID_CommandParam, 0, &command);
    IDirectMusicSegment_InsertTrack(segment, t, 1);
    IDirectMusicTrack_Release(t);
}

struct transition_row
{
    int from, to;           /* 0: none, 1: chords D minor, then G major at 3072, groove 30; 2: chord E minor and groove 70 */
    MUSIC_TIME time;
    WORD command;
    DWORD flags;
    const char *want;
};

static const struct transition_row transition_rows[] = {
    {1, 2, 3072, DMUS_COMMANDT_GROOVE,       0,                      "len=3072 C:0/0/70, H:DM@0.0,"},
    {1, 2, 3072, DMUS_COMMANDT_FILL,         0,                      "len=3072 C:0/1/70, H:DM@0.0,"},
    {1, 2, 3072, DMUS_COMMANDT_INTRO,        0,                      "len=3072 C:0/2/70, H:DM@0.0,"},
    {1, 2, 3072, DMUS_COMMANDT_BREAK,        0,                      "len=3072 C:0/3/70, H:DM@0.0,"},
    {1, 2, 3072, DMUS_COMMANDT_END,          0,                      "len=3072 C:0/4/70, H:DM@0.0,"},
    {1, 2, 3072, DMUS_COMMANDT_ENDANDINTRO,  0,                      "len=6144 C:0/4/70,1/2/70, H:GM@0.0,DM@1.0,"},
    {1, 2, 3072, DMUS_COMMANDT_FILL,         DMUS_COMPOSEF_LONG,     "len=9216 C:0/0/70,2/1/70, H:GM@0.0,GM@1.0,DM@2.0,"},
    {1, 2, 3072, DMUS_COMMANDT_FILL,         DMUS_COMPOSEF_MODULATE, "len=3072 C:0/1/70, H:BM@0.0,"},
    {1, 2, 0,    DMUS_COMMANDT_FILL,         0,                      "len=3072 C:0/1/70, H:AM@0.0,"},
    {0, 2, 0,    DMUS_COMMANDT_INTRO,        0,                      "len=3072 C:0/2/70, H:BM@0.0,"},
    {1, 0, 3072, DMUS_COMMANDT_END,          0,                      "len=3072 C:0/4/30, H:GM@0.0,"},
};

static void test_transition(IDirectMusicStyle *style44, IDirectMusicChordMap *major)
{
    IDirectMusicSegment *from = plain_segment(4 * 3072), *to = plain_segment(4 * 3072), *transition;
    IDirectMusicSegment *empty = plain_segment(3072), *src[3] = {NULL};
    HRESULT hr;
    int i;

    add_style(from, style44);
    {
        /* D minor at the start (root 2, pattern 1 + 3 + 7 semitones), G major from the second measure */
        const struct chord_spec chords[] = {{0, 2, 0x89, L"Dm"}, {3072, 7, 0x91, L"GM"}};
        add_chords(from, MINOR, chords, 2);
    }
    add_groove(from, 30);
    add_style(to, style44);
    {
        const struct chord_spec chords[] = {{0, 4, 0x89, L"Em"}};
        add_chords(to, MAJOR, chords, 1);
    }
    add_groove(to, 70);
    src[1] = from;
    src[2] = to;

    for (i = 0; i < ARRAY_SIZE_(transition_rows); i++)
    {
        const struct transition_row *row = transition_rows + i;
        char what[128];

        transition = NULL;
        hr = IDirectMusicComposer_ComposeTransition(composer, src[row->from], src[row->to], row->time, row->command,
                row->flags, major, &transition);
        snprintf(what, sizeof(what), "transition from %d at %ld, to %d, command %u, flags %#lx (%#lx)", row->from, row->time,
                row->to, row->command, row->flags, hr);
        CHECKF(hr == S_OK && transition, "%s is S_OK", what);
        if (!transition) continue;
        expect_text(what, describe_segment(transition, 3072), row->want);
        IDirectMusicSegment_Release(transition);
    }

    /* without a chord map argument the chord map of the segments is used (none: the major scale); the track checks */
    transition = NULL;
    hr = IDirectMusicComposer_ComposeTransition(composer, from, to, 3072, DMUS_COMMANDT_FILL, 0, NULL, &transition);
    CHECKF(hr == S_OK && transition, "a transition without a chord map (%#lx)", hr);
    if (transition)
    {
        IDirectMusicStyle *got = NULL;
        DMUS_TEMPO_PARAM tempo = {0};

        hr = IDirectMusicSegment_GetParam(transition, &GUID_IDirectMusicStyle, 0xFFFFFFFF, DMUS_SEG_ANYTRACK, 0, NULL, &got);
        CHECKF(hr == S_OK && got == style44, "the transition has the style of the segments (%#lx)", hr);
        if (got) IDirectMusicStyle_Release(got);
        hr = IDirectMusicSegment_GetParam(transition, &GUID_TempoParam, 0xFFFFFFFF, DMUS_SEG_ANYTRACK, 0, NULL, &tempo);
        CHECKF(hr == S_OK && tempo.dblTempo == 100.0, "the transition has the tempo of the style (%#lx, %f)", hr, tempo.dblTempo);
        IDirectMusicSegment_Release(transition);
    }

    transition = (IDirectMusicSegment *)0xdeadbeef;
    hr = IDirectMusicComposer_ComposeTransition(composer, NULL, NULL, 0, 0, 0, major, &transition);
    CHECKF(hr == E_INVALIDARG && !transition, "both segments NULL is E_INVALIDARG (%#lx)", hr);
    hr = IDirectMusicComposer_ComposeTransition(composer, from, to, 0, 6, 0, major, &transition);
    CHECKF(hr == E_INVALIDARG, "command 6 is E_INVALIDARG (%#lx)", hr);
    hr = IDirectMusicComposer_ComposeTransition(composer, from, to, -1, 0, 0, major, &transition);
    CHECKF(hr == E_INVALIDARG, "negative time is E_INVALIDARG (%#lx)", hr);
    hr = IDirectMusicComposer_ComposeTransition(composer, from, to, 0, 0, 0, major, NULL);
    CHECKF(hr == E_POINTER, "no result pointer is E_POINTER (%#lx)", hr);
    hr = IDirectMusicComposer_ComposeTransition(composer, empty, empty, 0, 0, 0, major, &transition);
    CHECKF(hr == E_INVALIDARG, "segments without a style are E_INVALIDARG (%#lx)", hr);

    IDirectMusicSegment_Release(empty);
    IDirectMusicSegment_Release(from);
    IDirectMusicSegment_Release(to);
}

/* ---- AutoTransition ---- */

/* plays the segment and waits until it plays (it starts a little after now) */
static IDirectMusicSegmentState *play_and_wait(IDirectMusicPerformance8 *perf8, IDirectMusicSegment *segment)
{
    IDirectMusicSegmentState *state = NULL, *playing = NULL;
    MUSIC_TIME now;
    int i;

    IDirectMusicPerformance8_PlaySegmentEx(perf8, (IUnknown *)segment, NULL, NULL, 0, 0, &state, NULL, NULL);
    for (i = 0; i < 200; i++)
    {
        IDirectMusicPerformance8_GetTime(perf8, NULL, &now);
        if (IDirectMusicPerformance8_GetSegmentState(perf8, &playing, now) == S_OK)
        {
            IDirectMusicSegmentState_Release(playing);
            break;
        }
        Sleep(25);
    }
    return state;
}

static void test_auto_transition(IDirectMusicStyle *style44, IDirectMusicChordMap *major)
{
    IDirectMusicSegment *from = plain_segment(8 * 3072), *to = plain_segment(4 * 3072), *transition = NULL;
    IDirectMusicSegmentState *transition_state = NULL, *to_state = NULL, *from_state = NULL;
    IDirectMusicPerformance8 *perf8;
    IDirectMusicPerformance *fresh = NULL;
    MUSIC_TIME start = 0, to_start = 0, length = 0;
    HRESULT hr;
    const struct chord_spec from_chord[] = {{0, 0, 0x91, L"CM"}}, to_chord[] = {{0, 4, 0x89, L"Em"}};

    add_style(from, style44);
    add_chords(from, MAJOR, from_chord, 1);
    add_style(to, style44);
    add_chords(to, MAJOR, to_chord, 1);

    hr = IDirectMusicComposer_AutoTransition(composer, NULL, to, DMUS_COMMANDT_FILL, 0, major, NULL, NULL, NULL);
    CHECKF(hr == E_POINTER, "AutoTransition without a performance is E_POINTER (%#lx)", hr);

    /* a performance which is not initialized cannot play */
    CoCreateInstance(&CLSID_DirectMusicPerformance, NULL, CLSCTX_INPROC_SERVER, &IID_IDirectMusicPerformance, (void **)&fresh);
    hr = IDirectMusicComposer_AutoTransition(composer, fresh, to, DMUS_COMMANDT_FILL, 0, major, &transition, &to_state, &transition_state);
    CHECKF(hr == DMUS_E_NO_MASTER_CLOCK && !to_state && !transition_state, "AutoTransition on an uninitialized performance is DMUS_E_NO_MASTER_CLOCK (%#lx)", hr);
    IDirectMusicPerformance_Release(fresh);
    if (transition) IDirectMusicSegment_Release(transition);
    transition = NULL;

    IDirectMusicPerformance_QueryInterface(perf, &IID_IDirectMusicPerformance8, (void **)&perf8);

    /* a transition to nothing: only the transition plays, its chord is the one of the segment which plays */
    from_state = play_and_wait(perf8, from);
    hr = IDirectMusicComposer_AutoTransition(composer, perf, NULL, DMUS_COMMANDT_END, 0, major, &transition, &to_state, &transition_state);
    CHECKF(hr == S_OK && transition && !to_state && transition_state, "AutoTransition to nothing plays only the transition (%#lx)", hr);
    if (transition) expect_text("auto transition to nothing", describe_segment(transition, 3072), "len=3072 C:0/4/50, H:CM@0.0,");
    if (transition) IDirectMusicSegment_Release(transition);
    if (transition_state) IDirectMusicSegmentState_Release(transition_state);
    if (from_state) IDirectMusicSegmentState_Release(from_state);
    IDirectMusicPerformance_Stop(perf, NULL, NULL, 0, 0);
    transition = NULL;
    transition_state = to_state = NULL;

    from_state = play_and_wait(perf8, from);
    hr = IDirectMusicComposer_AutoTransition(composer, perf, to, DMUS_COMMANDT_FILL, 0, major, &transition, &to_state, &transition_state);
    CHECKF(hr == S_OK && transition && to_state && transition_state, "AutoTransition (%#lx)", hr);
    if (transition && to_state && transition_state)
    {
        IDirectMusicSegment *played = NULL;

        IDirectMusicSegment_GetLength(transition, &length);
        IDirectMusicSegmentState_GetStartTime(transition_state, &start);
        IDirectMusicSegmentState_GetStartTime(to_state, &to_start);
        CHECKF(length == 3072, "the transition is one measure long (%ld)", length);
        CHECKF(to_start == start + length, "the segment follows the transition at its end (%ld, %ld + %ld)", to_start, start, length);
        IDirectMusicSegmentState_GetSegment(to_state, &played);
        CHECKF(played == to, "the segment state is the one of the segment");
        if (played) IDirectMusicSegment_Release(played);
        played = NULL;
        IDirectMusicSegmentState_GetSegment(transition_state, &played);
        CHECKF(played == transition, "the transition state is the one of the transition");
        if (played) IDirectMusicSegment_Release(played);
        expect_text("auto transition", describe_segment(transition, 3072), "len=3072 C:0/1/50, H:GM@0.0,");
    }
    if (transition) IDirectMusicSegment_Release(transition);
    if (to_state) IDirectMusicSegmentState_Release(to_state);
    if (transition_state) IDirectMusicSegmentState_Release(transition_state);
    if (from_state) IDirectMusicSegmentState_Release(from_state);

    IDirectMusicPerformance8_Release(perf8);
    IDirectMusicPerformance_Stop(perf, NULL, NULL, 0, 0);
    IDirectMusicSegment_Release(from);
    IDirectMusicSegment_Release(to);
}

int main(int argc, char **argv)
{
    IDirectMusicStyle *style44, *style34;
    IDirectMusicChordMap *major, *minor;
    HRESULT hr;

    dump = argc > 1 && !strcmp(argv[1], "dump");
    CoInitialize(NULL);
    if (!setup_perf()) return 77;

    hr = CoCreateInstance(&CLSID_DirectMusicComposer, NULL, CLSCTX_INPROC_SERVER, &IID_IDirectMusicComposer, (void **)&composer);
    CHECKF(hr == S_OK, "create the composer (%#lx)", hr);
    style44 = make_style(4, 4, 100.0);
    style34 = make_style(3, 4, 120.0);
    major = make_chordmap(MAJOR);
    minor = make_chordmap(MINOR);

    test_template();
    test_from_template(style44, style34, major, minor);
    test_from_shape(style44, major);
    test_signpost_compose(style34, minor);
    test_transition(style44, major);
    test_auto_transition(style44, major);

    IDirectMusicChordMap_Release(major);
    IDirectMusicChordMap_Release(minor);
    IDirectMusicStyle_Release(style44);
    IDirectMusicStyle_Release(style34);
    IDirectMusicComposer_Release(composer);
    teardown_perf();
    CoUninitialize();
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
