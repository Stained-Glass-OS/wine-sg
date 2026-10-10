/* dmstyle style object (patches/sg/2962), run by test/dmcontent-style-gate.sh: a style with two bands and six
 * patterns (grooves, fills, two motifs) is built here as RIFF data, loaded, and EnumBand, GetDefaultBand,
 * EnumMotif, EnumPattern, GetEmbellishmentLength and GetMotif are compared with that data.
 *
 *   dmcontent-style-probe.exe */
#include "dmcontent-common.h"

static void band_form(DWORD patch, DWORD channel, const WCHAR *name)
{
    DMUS_IO_INSTRUMENT inst = {0};
    int at, list, inner;

    inst.dwPatch = patch;
    inst.dwPChannel = channel;
    inst.dwFlags = DMUS_IO_INST_PATCH;
    at = begin("RIFF", "DMBD");
    if (name)
    {
        list = begin("LIST", "UNFO");
        chunk("UNAM", name, (lstrlenW(name) + 1) * sizeof(WCHAR));
        end(list);
    }
    list = begin("LIST", "lbil");
    inner = begin("LIST", "lbin");
    chunk("bins", &inst, sizeof(inst));
    end(inner);
    end(list);
    end(at);
}

static void add_pattern(const WCHAR *name, BYTE beats, BYTE beat, BYTE bottom, BYTE top, WORD emb, WORD measures,
        const DMUS_IO_MOTIFSETTINGS *settings, DWORD band_patch)
{
    DMUS_IO_PATTERN p = {{beats, beat, 4}, bottom, top, emb, measures, 0, 0, 0};
    int at, list;

    at = begin("LIST", "pttn");
    chunk("ptnh", &p, sizeof(p));
    list = begin("LIST", "UNFO");
    chunk("UNAM", name, (lstrlenW(name) + 1) * sizeof(WCHAR));
    end(list);
    if (settings) chunk("mtfs", settings, sizeof(*settings));
    if (band_patch) band_form(band_patch, 3, NULL);
    end(at);
}

static IDirectMusicStyle8 *make_style(void)
{
    DMUS_IO_STYLE st = {{3, 4, 4}, 90.0};
    DMUS_IO_MOTIFSETTINGS settings = {3, 192, 100, 700, 0};
    IDirectMusicStyle8 *style = NULL;
    IPersistStream *ps;
    IStream *stream;
    int at, list;
    HRESULT hr;

    at = begin("RIFF", "DMST");
    chunk("styh", &st, sizeof(st));
    list = begin("LIST", "UNFO");
    chunk("UNAM", L"theStyle", sizeof(L"theStyle"));
    end(list);
    band_form(0x11, 1, L"bandA");
    band_form(0x22, 2, L"bandB");
    add_pattern(L"groove1", 3, 4, 1, 50, 0, 4, NULL, 0);
    add_pattern(L"groove2", 3, 4, 40, 100, 0, 8, NULL, 0);
    add_pattern(L"fillA", 3, 4, 1, 100, DMUS_EMBELLISHT_FILL, 1, NULL, 0);
    add_pattern(L"fillB", 3, 4, 1, 100, DMUS_EMBELLISHT_FILL, 2, NULL, 0);
    add_pattern(L"motifX", 4, 4, 1, 100, DMUS_EMBELLISHT_MOTIF, 2, &settings, 0x33);
    add_pattern(L"motifY", 3, 4, 1, 100, DMUS_EMBELLISHT_MOTIF, 1, NULL, 0);
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

static void expect_name(HRESULT hr, const WCHAR *got, const WCHAR *want, const char *tag)
{
    CHECKF(hr == S_OK && !lstrcmpW(got, want), "%s is %ls (%#lx, %ls)", tag, want, hr, got);
}

static void expect_band_patch(IDirectMusicBand *band, int patch, int channel, const char *tag)
{
    IDirectMusicTrack *track = create_track(&CLSID_DirectMusicBandTrack);
    DMUS_BAND_PARAM p = {0, band};
    HRESULT hr;

    /* the patch of the band is what the band sends when played in a band track */
    IDirectMusicTrack_SetParam(track, &GUID_BandParam, 0, &p);
    msg_count = 0;
    hr = IDirectMusicTrack_Play(track, NULL, 0, 10, 0, 0, perf, NULL, 7);
    CHECKF(hr == S_OK && msg_count == 1 && msgs[0].midi[0] == patch && msgs[0].pchannel == channel,
            "%s sends patch %d on channel %d (%#lx, %d messages, patch %d channel %lu)", tag, patch, channel, hr, msg_count,
            msg_count ? msgs[0].midi[0] : -1, msg_count ? msgs[0].pchannel : 0);
    IDirectMusicTrack_Release(track);
}

static void test_style(void)
{
    IDirectMusicStyle8 *style = make_style();
    WCHAR name[DMUS_MAX_NAME];
    IDirectMusicBand *band, *other;
    IDirectMusicSegment *segment;
    IDirectMusicTrack *track;
    DWORD min, max, repeats;
    MUSIC_TIME length, start, loop_start, loop_end;
    DMUS_BAND_PARAM p = {0, NULL};
    HRESULT hr;
    int i;

    if (!style) return;

    /* bands */
    wcscpy(name, L"junk");
    hr = IDirectMusicStyle8_EnumBand(style, 0, name);
    expect_name(hr, name, L"bandA", "EnumBand(0)");
    hr = IDirectMusicStyle8_EnumBand(style, 1, name);
    expect_name(hr, name, L"bandB", "EnumBand(1)");
    wcscpy(name, L"junk");
    hr = IDirectMusicStyle8_EnumBand(style, 2, name);
    CHECKF(hr == S_FALSE && !lstrcmpW(name, L"junk"), "EnumBand(2) is S_FALSE and keeps the name (%#lx)", hr);
    hr = IDirectMusicStyle8_EnumBand(style, 0, NULL);
    CHECKF(hr == E_POINTER, "EnumBand without name is E_POINTER (%#lx)", hr);
    band = NULL;
    hr = IDirectMusicStyle8_GetDefaultBand(style, &band);
    CHECKF(hr == S_OK && band, "GetDefaultBand is S_OK (%#lx)", hr);
    if (band) expect_band_patch(band, 0x11, 1, "the default band");
    other = NULL;
    IDirectMusicStyle8_GetBand(style, (WCHAR *)L"bandB", &other);
    CHECKF(other && other != band, "GetBand(bandB) is not the default band");
    if (other) { expect_band_patch(other, 0x22, 2, "bandB"); IDirectMusicBand_Release(other); }
    if (band) IDirectMusicBand_Release(band);
    hr = IDirectMusicStyle8_GetDefaultBand(style, NULL);
    CHECKF(hr == E_POINTER, "GetDefaultBand without pointer is E_POINTER (%#lx)", hr);

    /* motifs and patterns */
    wcscpy(name, L"junk");
    hr = IDirectMusicStyle8_EnumMotif(style, 0, name);
    expect_name(hr, name, L"motifX", "EnumMotif(0)");
    hr = IDirectMusicStyle8_EnumMotif(style, 1, name);
    expect_name(hr, name, L"motifY", "EnumMotif(1)");
    hr = IDirectMusicStyle8_EnumMotif(style, 2, name);
    CHECKF(hr == S_FALSE, "EnumMotif(2) is S_FALSE (%#lx)", hr);
    {
        static const WCHAR *patterns[] = {L"groove1", L"groove2", L"fillA", L"fillB"};
        for (i = 0; i < 4; i++)
        {
            hr = IDirectMusicStyle8_EnumPattern(style, i, DMUS_STYLET_PATTERN, name);
            expect_name(hr, name, patterns[i], "EnumPattern(pattern)");
        }
        hr = IDirectMusicStyle8_EnumPattern(style, 4, DMUS_STYLET_PATTERN, name);
        CHECKF(hr == S_FALSE, "EnumPattern(4, pattern) is S_FALSE (%#lx)", hr);
        hr = IDirectMusicStyle8_EnumPattern(style, 1, DMUS_STYLET_MOTIF, name);
        expect_name(hr, name, L"motifY", "EnumPattern(1, motif)");
        hr = IDirectMusicStyle8_EnumPattern(style, 2, DMUS_STYLET_MOTIF, name);
        CHECKF(hr == S_FALSE, "EnumPattern(2, motif) is S_FALSE (%#lx)", hr);
        hr = IDirectMusicStyle8_EnumPattern(style, 0, DMUS_STYLET_PATTERN, NULL);
        CHECKF(hr == E_POINTER, "EnumPattern without name is E_POINTER (%#lx)", hr);
    }

    /* embellishment lengths, in measures */
    hr = IDirectMusicStyle8_GetEmbellishmentLength(style, DMUS_COMMANDT_GROOVE, 45, &min, &max);
    CHECKF(hr == S_OK && min == 4 && max == 8, "groove length at level 45 is 4..8 (%#lx, %lu..%lu)", hr, min, max);
    hr = IDirectMusicStyle8_GetEmbellishmentLength(style, DMUS_COMMANDT_GROOVE, 10, &min, &max);
    CHECKF(hr == S_OK && min == 4 && max == 4, "groove length at level 10 is 4..4 (%#lx, %lu..%lu)", hr, min, max);
    hr = IDirectMusicStyle8_GetEmbellishmentLength(style, DMUS_COMMANDT_FILL, 50, &min, &max);
    CHECKF(hr == S_OK && min == 1 && max == 2, "fill length is 1..2 (%#lx, %lu..%lu)", hr, min, max);
    hr = IDirectMusicStyle8_GetEmbellishmentLength(style, DMUS_COMMANDT_BREAK, 50, &min, &max);
    CHECKF(hr == S_OK && min == 0 && max == 0, "break length without breaks is 0..0 (%#lx, %lu..%lu)", hr, min, max);
    hr = IDirectMusicStyle8_GetEmbellishmentLength(style, DMUS_COMMANDT_GROOVE, 200, &min, &max);
    CHECKF(hr == S_OK && min == 0 && max == 0, "groove length at level 200 is 0..0 (%#lx, %lu..%lu)", hr, min, max);
    hr = IDirectMusicStyle8_GetEmbellishmentLength(style, 9, 50, &min, &max);
    CHECKF(hr == E_INVALIDARG, "unknown embellishment type is E_INVALIDARG (%#lx)", hr);
    hr = IDirectMusicStyle8_GetEmbellishmentLength(style, DMUS_COMMANDT_FILL, 50, NULL, &max);
    CHECKF(hr == E_POINTER, "GetEmbellishmentLength without pointer is E_POINTER (%#lx)", hr);

    /* chord map methods are not implemented */
    {
        IDirectMusicChordMap *map = NULL;
        hr = IDirectMusicStyle8_GetDefaultChordMap(style, &map);
        CHECKF(hr == E_NOTIMPL, "GetDefaultChordMap is E_NOTIMPL (%#lx)", hr);
        hr = IDirectMusicStyle8_EnumChordMap(style, 0, name);
        CHECKF(hr == E_NOTIMPL, "EnumChordMap is E_NOTIMPL (%#lx)", hr);
        hr = IDirectMusicStyle8_GetChordMap(style, (WCHAR *)L"x", &map);
        CHECKF(hr == E_NOTIMPL, "GetChordMap is E_NOTIMPL (%#lx)", hr);
    }

    /* GetMotif: a segment of the length of the motif with a motif track and the band of the motif */
    segment = NULL;
    hr = IDirectMusicStyle8_GetMotif(style, (WCHAR *)L"motifX", &segment);
    CHECKF(hr == S_OK && segment, "GetMotif(motifX) is S_OK (%#lx)", hr);
    if (segment)
    {
        length = 0; repeats = 0; start = 0; loop_start = loop_end = 0;
        IDirectMusicSegment_GetLength(segment, &length);
        IDirectMusicSegment_GetRepeats(segment, &repeats);
        IDirectMusicSegment_GetStartPoint(segment, &start);
        IDirectMusicSegment_GetLoopPoints(segment, &loop_start, &loop_end);
        CHECKF(length == 2 * 4 * 768, "motifX segment is 2 measures of 4/4 long (%ld)", length);
        CHECKF(repeats == 3, "motifX segment repeats 3 times (%lu)", repeats);
        CHECKF(start == 192, "motifX segment starts at 192 (%ld)", start);
        CHECKF(loop_start == 100 && loop_end == 700, "motifX segment loops 100..700 (%ld..%ld)", loop_start, loop_end);
        track = NULL;
        hr = IDirectMusicSegment_GetTrack(segment, &CLSID_DirectMusicMotifTrack, 1, 0, &track);
        CHECKF(hr == S_OK && track, "motifX segment has a motif track (%#lx)", hr);
        if (track) IDirectMusicTrack_Release(track);
        hr = IDirectMusicSegment_GetParam(segment, &GUID_BandParam, 1, 0, 0, NULL, &p);
        CHECKF(hr == S_OK && p.pBand, "motifX segment has the band of the motif (%#lx)", hr);
        if (p.pBand)
        {
            expect_band_patch(p.pBand, 0x33, 3, "the motifX band");
            IDirectMusicBand_Release(p.pBand);
        }
        IDirectMusicSegment_Release(segment);
    }
    segment = NULL;
    hr = IDirectMusicStyle8_GetMotif(style, (WCHAR *)L"motifY", &segment);
    CHECKF(hr == S_OK && segment, "GetMotif(motifY) is S_OK (%#lx)", hr);
    if (segment)
    {
        length = 0; repeats = 5; loop_start = loop_end = 5;
        IDirectMusicSegment_GetLength(segment, &length);
        IDirectMusicSegment_GetRepeats(segment, &repeats);
        IDirectMusicSegment_GetLoopPoints(segment, &loop_start, &loop_end);
        CHECKF(length == 3 * 768, "motifY segment is one measure of 3/4 long (%ld)", length);
        CHECKF(repeats == 0 && loop_start == 0 && loop_end == 0, "motifY segment has the default settings (%lu, %ld..%ld)", repeats,
                loop_start, loop_end);
        p.pBand = NULL;
        hr = IDirectMusicSegment_GetParam(segment, &GUID_BandParam, 1, 0, 0, NULL, &p);
        CHECKF(hr != S_OK && !p.pBand, "motifY segment has no band (%#lx)", hr);
        IDirectMusicSegment_Release(segment);
    }
    segment = NULL;
    hr = IDirectMusicStyle8_GetMotif(style, (WCHAR *)L"nothing", &segment);
    CHECKF(hr == DMUS_E_NOT_FOUND && !segment, "GetMotif of an unknown name is DMUS_E_NOT_FOUND (%#lx)", hr);
    hr = IDirectMusicStyle8_GetMotif(style, (WCHAR *)L"groove1", &segment);
    CHECKF(hr == DMUS_E_NOT_FOUND && !segment, "GetMotif of a groove is DMUS_E_NOT_FOUND (%#lx)", hr);
    hr = IDirectMusicStyle8_GetMotif(style, NULL, &segment);
    CHECKF(hr == E_POINTER, "GetMotif without name is E_POINTER (%#lx)", hr);

    IDirectMusicStyle8_Release(style);
}

int main(void)
{
    CoInitialize(NULL);
    if (!setup_perf()) return 77;

    test_style();

    teardown_perf();
    CoUninitialize();
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
