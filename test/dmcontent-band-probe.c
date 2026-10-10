/* dmband band track (patches/sg/2961), run by test/dmcontent-band-gate.sh: bands with instruments are loaded
 * from RIFF data built here, put into band tracks (Load and SetParam), played into a performance whose tool
 * graph collects the messages, cloned, joined, and asked through the extended methods; every answer is
 * compared with the data.
 *
 *   dmcontent-band-probe.exe */
#include "dmcontent-common.h"

static IDirectMusicBand *make_band(DWORD patch, DWORD channel)
{
    DMUS_IO_INSTRUMENT inst = {0};
    IDirectMusicBand *band = NULL;
    IPersistStream *ps;
    IStream *stream;
    int at, list, inner;
    HRESULT hr;

    inst.dwPatch = patch;
    inst.dwPChannel = channel;
    inst.dwFlags = DMUS_IO_INST_PATCH;
    at = begin("RIFF", "DMBD");
    list = begin("LIST", "lbil");
    inner = begin("LIST", "lbin");
    chunk("bins", &inst, sizeof(inst));
    end(inner);
    end(list);
    end(at);
    stream = make_stream();

    hr = CoCreateInstance(&CLSID_DirectMusicBand, NULL, CLSCTX_INPROC_SERVER, &IID_IDirectMusicBand, (void **)&band);
    CHECKF(hr == S_OK, "create a band (%#lx)", hr);
    IDirectMusicBand_QueryInterface(band, &IID_IPersistStream, (void **)&ps);
    hr = IPersistStream_Load(ps, stream);
    CHECKF(hr == S_OK, "load the band of patch %#lx (%#lx)", patch, hr);
    IPersistStream_Release(ps);
    IStream_Release(stream);
    return band;
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

static void set_band(IDirectMusicTrack *track, IDirectMusicBand *band, MUSIC_TIME time, MUSIC_TIME physical)
{
    DMUS_BAND_PARAM p;
    HRESULT hr;

    p.mtTimePhysical = physical;
    p.pBand = band;
    hr = IDirectMusicTrack_SetParam(track, &GUID_BandParam, time, &p);
    CHECKF(hr == S_OK, "SetParam(BandParam) at %ld (%#lx)", time, hr);
}

/* GetParam(BandParam) at time returns the band, its physical time, and the logical time of the next band */
static void expect_band(IDirectMusicTrack *track, MUSIC_TIME time, IDirectMusicBand *band, MUSIC_TIME physical,
        MUSIC_TIME next, const char *tag)
{
    DMUS_BAND_PARAM p = {0, NULL};
    MUSIC_TIME got_next = 0x1234;
    HRESULT hr = IDirectMusicTrack_GetParam(track, &GUID_BandParam, time, &got_next, &p);

    CHECKF(hr == S_OK && p.pBand == band && p.mtTimePhysical == physical && got_next == next,
            "%s: band at %ld is the expected one with physical time %ld, next %ld (%#lx, %p vs %p, %ld, next %ld)", tag, time, physical,
            next, hr, p.pBand, band, p.mtTimePhysical, got_next);
    if (p.pBand) IDirectMusicBand_Release(p.pBand);
}

static void expect_patch(int i, MUSIC_TIME time, DWORD channel, int instrument, int msb, int lsb, const char *tag)
{
    CHECKF(msg_count > i && msgs[i].type == DMUS_PMSGT_PATCH && msgs[i].mtTime == time && msgs[i].pchannel == channel
            && msgs[i].midi[0] == instrument && msgs[i].midi[1] == msb && msgs[i].midi[2] == lsb && msgs[i].track_id == 7,
            "%s: message %d is the patch %d/%d/%d for channel %lu at %ld (%d messages, type %#lx at %ld channel %lu patch %d/%d/%d)", tag, i,
            instrument, msb, lsb, channel, time, msg_count, msg_count > i ? msgs[i].type : 0, msg_count > i ? msgs[i].mtTime : 0,
            msg_count > i ? msgs[i].pchannel : 0, msg_count > i ? msgs[i].midi[0] : 0, msg_count > i ? msgs[i].midi[1] : 0,
            msg_count > i ? msgs[i].midi[2] : 0);
}

static void test_band_track(void)
{
    IDirectMusicBand *b0 = make_band(0x010203, 4), *b1 = make_band(0x000510, 5), *b2 = make_band(0x000020, 6), *b3 = make_band(0x000030, 7);
    IDirectMusicTrack *track = create_track(&CLSID_DirectMusicBandTrack), *copy, *join, *other;
    IDirectMusicTrack8 *track8;
    ULONG r0, r1, r2;
    DMUS_BAND_PARAM p = {0, NULL};
    HRESULT hr;

    if (!track || !b0 || !b1 || !b2 || !b3) return;
    r0 = refs((IUnknown *)b0);
    r1 = refs((IUnknown *)b1);

    hr = IDirectMusicTrack_GetParam(track, &GUID_BandParam, 0, NULL, &p);
    CHECKF(hr == DMUS_E_NOT_FOUND, "band track: GetParam without bands is DMUS_E_NOT_FOUND (%#lx)", hr);

    set_band(track, b0, 0, 0);
    set_band(track, b1, 1000, 900);
    set_band(track, b2, 2000, 1900);
    CHECKF(refs((IUnknown *)b0) == r0 + 1 && refs((IUnknown *)b1) == r1 + 1, "band track references its bands");
    expect_band(track, 0, b0, 0, 1000, "band track");
    expect_band(track, 1500, b1, 900, 2000, "band track");
    expect_band(track, 2500, b2, 1900, 2000, "band track");
    hr = IDirectMusicTrack_GetParam(track, &GUID_BandParam, 0, NULL, NULL);
    CHECKF(hr == E_POINTER, "band track: GetParam(BandParam) without data is E_POINTER (%#lx)", hr);
    /* a band put in front of the others is sorted in */
    set_band(track, b3, 500, 400);
    expect_band(track, 600, b3, 400, 1000, "band track after a band set between the others");
    expect_band(track, 1000, b1, 900, 2000, "band track after a band set between the others");

    /* Play: the bands inside of [start, end), moved by the offset, as patch messages */
    hr = play(track, 0, 1500, 100);
    CHECKF(hr == S_OK, "band track: Play is S_OK (%#lx)", hr);
    EXPECT_COUNT(3, "band Play of [0, 1500)");
    expect_patch(0, 100, 4, 3, 1, 2, "band Play of [0, 1500)");
    expect_patch(1, 600, 7, 0x30, 0, 0, "band Play of [0, 1500)");
    expect_patch(2, 1100, 5, 0x10, 0, 5, "band Play of [0, 1500)");

    /* starting or seeking inside of the track sends the band which is in effect first */
    msg_count = 0;
    hr = IDirectMusicTrack_Play(track, NULL, 1200, 2500, 0, DMUS_TRACKF_SEEK, perf, NULL, 7);
    CHECKF(hr == S_OK, "band track: Play with seek is S_OK (%#lx)", hr);
    EXPECT_COUNT(2, "band Play of [1200, 2500) with seek");
    expect_patch(0, 1200, 5, 0x10, 0, 5, "band Play of [1200, 2500) with seek");
    expect_patch(1, 2000, 6, 0x20, 0, 0, "band Play of [1200, 2500) with seek");
    msg_count = 0;
    hr = IDirectMusicTrack_Play(track, NULL, 1000, 1500, 0, DMUS_TRACKF_START, perf, NULL, 7);
    EXPECT_COUNT(1, "band Play of [1000, 1500) with start at the time of a band");
    expect_patch(0, 1000, 5, 0x10, 0, 5, "band Play of [1000, 1500) with start");
    msg_count = 0;
    hr = IDirectMusicTrack_Play(track, NULL, 1200, 1500, 0, 0, perf, NULL, 7);
    EXPECT_COUNT(0, "band Play of [1200, 1500) without seek or start");
    hr = IDirectMusicTrack_Play(track, NULL, 0, 3000, 0, 0, NULL, NULL, 7);
    CHECKF(hr == DMUS_S_END, "band track: Play without performance is DMUS_S_END (%#lx)", hr);

    /* Clone: [start, end) moved to zero, the band in effect at start comes first */
    copy = clone(track, 700, 2500);
    if (copy)
    {
        expect_band(copy, 0, b3, 0, 300, "band clone [700, 2500)");
        expect_band(copy, 300, b1, 200, 1300, "band clone [700, 2500)");
        expect_band(copy, 1300, b2, 1200, 1300, "band clone [700, 2500)");
        IDirectMusicTrack_Release(copy);
    }
    copy = clone(track, 0, 1200);
    if (copy)
    {
        expect_band(copy, 0, b0, 0, 500, "band clone [0, 1200)");
        expect_band(copy, 1100, b1, 900, 1000, "band clone [0, 1200)");
        hr = IDirectMusicTrack_GetParam(copy, &GUID_BandParam, 1100, NULL, &p);
        if (p.pBand) IDirectMusicBand_Release(p.pBand);
        CHECKF(hr == S_OK, "band clone has its last band (%#lx)", hr);
        IDirectMusicTrack_Release(copy);
    }
    hr = IDirectMusicTrack_Clone(track, 10, 5, &copy);
    CHECKF(hr == E_INVALIDARG, "band track: Clone(10, 5) is E_INVALIDARG (%#lx)", hr);

    /* Join: this track before the join time, the other one from the join time */
    other = create_track(&CLSID_DirectMusicBandTrack);
    set_band(other, b3, 0, 0);
    set_band(other, b0, 300, 300);
    join = NULL;
    hr = join_tracks(track, other, 1500, &join);
    CHECKF(hr == S_OK && join, "band track: Join is S_OK (%#lx)", hr);
    if (join)
    {
        expect_band(join, 0, b0, 0, 500, "band join");
        expect_band(join, 1400, b1, 900, 1500, "band join");
        expect_band(join, 1500, b3, 1500, 1800, "band join");
        expect_band(join, 1800, b0, 1800, 1800, "band join");
        IDirectMusicTrack_Release(join);
    }
    hr = join_tracks(track, other, -1, &join);
    CHECKF(hr == E_INVALIDARG, "band track: Join at a negative time is E_INVALIDARG (%#lx)", hr);
    hr = join_tracks(track, NULL, 5, &join);
    CHECKF(hr == E_POINTER, "band track: Join without a track is E_POINTER (%#lx)", hr);
    IDirectMusicTrack_Release(other);
    other = create_track(&CLSID_DirectMusicMuteTrack);
    hr = join_tracks(track, other, 5, &join);
    CHECKF(hr == DMUS_E_TYPE_UNSUPPORTED, "band track: Join with a mute track is DMUS_E_TYPE_UNSUPPORTED (%#lx)", hr);
    IDirectMusicTrack_Release(other);

    /* the extended methods use the performance of InitPlay */
    IDirectMusicTrack_QueryInterface(track, &IID_IDirectMusicTrack8, (void **)&track8);
    {
        void *state = NULL;
        REFERENCE_TIME rt, rt_next = 0;
        MUSIC_TIME mt = 0;
        DMUS_BAND_PARAM ex = {0, NULL};

        hr = IDirectMusicTrack8_InitPlay(track8, NULL, perf, &state, 7, 0);
        CHECKF(hr == S_OK && state, "band track: InitPlay gives a state (%#lx)", hr);
        IDirectMusicPerformance_MusicToReferenceTime(perf, 1200, &rt);
        hr = IDirectMusicTrack8_GetParamEx(track8, &GUID_BandParam, rt, &rt_next, &ex, state, 0);
        CHECKF(hr == S_OK && ex.pBand == b1 && ex.mtTimePhysical == 900, "band track: GetParamEx at 1200 is the second band (%#lx)", hr);
        IDirectMusicPerformance_ReferenceToMusicTime(perf, rt + rt_next, &mt);
        CHECKF(abs(mt - 2000) <= 2, "band track: GetParamEx next (relative to the time) is the band at 2000 (%ld)", mt);
        if (ex.pBand) IDirectMusicBand_Release(ex.pBand);
        IDirectMusicPerformance_MusicToReferenceTime(perf, 4000, &rt);
        ex.pBand = b3; ex.mtTimePhysical = 4000;
        hr = IDirectMusicTrack8_SetParamEx(track8, &GUID_BandParam, rt, &ex, state, 0);
        CHECKF(hr == S_OK, "band track: SetParamEx adds a band (%#lx)", hr);
        expect_band(track, 4100, b3, 4000, 4000, "band track after SetParamEx");
        hr = IDirectMusicTrack8_GetParamEx(track8, &GUID_BandParam, rt, NULL, &ex, NULL, 0);
        CHECKF(hr == E_POINTER, "band track: GetParamEx without play state is E_POINTER (%#lx)", hr);
        IDirectMusicPerformance_GetTime(perf, &rt, NULL);
        msg_count = 0;
        hr = IDirectMusicTrack8_PlayEx(track8, state, 0, 10000000, rt, DMUS_TRACKF_START, perf, NULL, 7);
        CHECKF(hr == S_OK, "band track: PlayEx is S_OK (%#lx)", hr);
        CHECKF(msg_count >= 3, "band track: PlayEx of one second sends the patches of the first bands (%d messages)", msg_count);
        hr = IDirectMusicTrack8_EndPlay(track8, state);
        CHECKF(hr == S_OK, "band track: EndPlay is S_OK (%#lx)", hr);
    }
    IDirectMusicTrack8_Release(track8);

    /* GUID_IDirectMusicBand adds a band, GUID_Clear_All_Bands removes them all */
    copy = create_track(&CLSID_DirectMusicBandTrack);
    r2 = refs((IUnknown *)b2);
    hr = IDirectMusicTrack_SetParam(copy, &GUID_IDirectMusicBand, 250, b2);
    CHECKF(hr == S_OK, "band track: SetParam(IDirectMusicBand) (%#lx)", hr);
    expect_band(copy, 300, b2, 250, 0, "band track SetParam(IDirectMusicBand)");
    hr = IDirectMusicTrack_SetParam(copy, &GUID_IDirectMusicBand, 250, NULL);
    CHECKF(hr == E_POINTER, "band track: SetParam(IDirectMusicBand) without data is E_POINTER (%#lx)", hr);
    hr = IDirectMusicTrack_SetParam(copy, &GUID_IDirectMusicBand, 250, copy);
    CHECKF(hr == E_NOINTERFACE, "band track: SetParam(IDirectMusicBand) of another object is E_NOINTERFACE (%#lx)", hr);
    hr = IDirectMusicTrack_SetParam(copy, &GUID_Clear_All_Bands, 0, NULL);
    CHECKF(hr == S_OK, "band track: SetParam(Clear_All_Bands) (%#lx)", hr);
    hr = IDirectMusicTrack_GetParam(copy, &GUID_BandParam, 300, NULL, &p);
    CHECKF(hr == DMUS_E_NOT_FOUND, "band track: no band is left after Clear_All_Bands (%#lx)", hr);
    CHECKF(refs((IUnknown *)b2) == r2, "band track released the cleared band (%lu, expected %lu)", refs((IUnknown *)b2), r2);

    /* downloads: the parameter is the performance */
    hr = IDirectMusicTrack_SetParam(track, &GUID_Download, 0, perf);
    CHECKF(hr == S_OK, "band track: SetParam(Download) with a performance (%#lx)", hr);
    hr = IDirectMusicTrack_SetParam(track, &GUID_Download, 0, NULL);
    CHECKF(hr == E_POINTER, "band track: SetParam(Download) without data is E_POINTER (%#lx)", hr);
    hr = IDirectMusicTrack_SetParam(track, &GUID_Download, 0, copy);
    CHECKF(hr == E_NOINTERFACE, "band track: SetParam(Download) of another object is E_NOINTERFACE (%#lx)", hr);
    hr = IDirectMusicTrack_SetParam(track, &GUID_Unload, 0, NULL);
    CHECKF(hr == S_OK, "band track: SetParam(Unload) (%#lx)", hr);
    IDirectMusicTrack_Release(copy);

    IDirectMusicTrack_Release(track);
    CHECKF(refs((IUnknown *)b0) == r0 && refs((IUnknown *)b1) == r1, "band track released its bands (%lu, %lu, expected %lu, %lu)",
            refs((IUnknown *)b0), refs((IUnknown *)b1), r0, r1);
    IDirectMusicBand_Release(b0);
    IDirectMusicBand_Release(b1);
    IDirectMusicBand_Release(b2);
    IDirectMusicBand_Release(b3);
}

/* a band track loaded from a DMBT form: header, two bands */
static void test_band_track_load(void)
{
    IDirectMusicTrack *track = create_track(&CLSID_DirectMusicBandTrack), *copy;
    DMUS_IO_BAND_TRACK_HEADER header = {TRUE};
    DMUS_IO_BAND_ITEM_HEADER2 item;
    DMUS_IO_INSTRUMENT inst = {0};
    IPersistStream *ps;
    DMUS_BAND_PARAM p = {0, NULL};
    IStream *stream;
    int at, list, lbnd, band, ilist, inner, i;
    HRESULT hr;

    if (!track) return;

    at = begin("RIFF", "DMBT");
    chunk("bdth", &header, sizeof(header));
    list = begin("LIST", "lbdl");
    for (i = 0; i < 2; i++)
    {
        lbnd = begin("LIST", "lbnd");
        item.lBandTimeLogical = i ? 960 : -1;
        item.lBandTimePhysical = i ? 900 : 0;
        chunk("bd2h", &item, sizeof(item));
        inst.dwPatch = i ? 0x000007 : 0x000011;
        inst.dwPChannel = 10 + i;
        inst.dwFlags = DMUS_IO_INST_PATCH;
        band = begin("RIFF", "DMBD");
        ilist = begin("LIST", "lbil");
        inner = begin("LIST", "lbin");
        chunk("bins", &inst, sizeof(inst));
        end(inner);
        end(ilist);
        end(band);
        end(lbnd);
    }
    end(list);
    end(at);

    hr = load_track(track);
    CHECKF(hr == S_OK, "band track: Load is S_OK (%#lx)", hr);

    hr = IDirectMusicTrack_GetParam(track, &GUID_BandParam, 2000, NULL, &p);
    CHECKF(hr == S_OK && p.pBand && p.mtTimePhysical == 900, "band track: the last loaded band is at physical time 900 (%#lx, %ld)", hr,
            p.mtTimePhysical);
    if (p.pBand) IDirectMusicBand_Release(p.pBand);

    /* the band with the time -1 is sent when the track starts, the other one at its time */
    hr = play(track, 0, 2000, 50);
    CHECKF(hr == S_OK, "band track: Play of the loaded track is S_OK (%#lx)", hr);
    EXPECT_COUNT(2, "loaded band Play of [0, 2000)");
    expect_patch(0, -1, 10, 0x11, 0, 0, "loaded band Play (the band of the track start is sent at time -1)");
    expect_patch(1, 960 + 50, 11, 7, 0, 0, "loaded band Play");
    msg_count = 0;
    hr = IDirectMusicTrack_Play(track, NULL, 100, 2000, 0, 0, perf, NULL, 7);
    EXPECT_COUNT(1, "loaded band Play of [100, 2000) without start");

    /* Clone keeps the start bands of a copy which starts at zero; Save is not supported */
    copy = clone(track, 0, 2000);
    if (copy)
    {
        msg_count = 0;
        hr = IDirectMusicTrack_Play(copy, NULL, 0, 2000, 0, DMUS_TRACKF_START, perf, NULL, 7);
        EXPECT_COUNT(2, "band clone Play of [0, 2000) with start");
        IDirectMusicTrack_Release(copy);
    }
    stream = new_stream();
    IDirectMusicTrack_QueryInterface(track, &IID_IPersistStream, (void **)&ps);
    hr = IPersistStream_Save(ps, stream, TRUE);
    CHECKF(hr == E_NOTIMPL, "band track: Save is E_NOTIMPL (%#lx)", hr);
    IPersistStream_Release(ps);
    IStream_Release(stream);

    IDirectMusicTrack_Release(track);
}

int main(void)
{
    CoInitialize(NULL);
    if (!setup_perf()) return 77;

    test_band_track();
    test_band_track_load();

    teardown_perf();
    CoUninitialize();
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
