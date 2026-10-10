/* winegstreamer wm_reader.c: header info, language list, packet size,
 * timecode, playlist burn and the sync reader's own output settings, max
 * output sample size and range-by-frame (patches/sg/2920), run by
 * test/wgrest-wmreader-gate.sh with test/wgst-wmreader-test.wmv.
 *
 *   wgrest-wmreader-probe.exe FILE.wmv */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <wmsdk.h>
#include <stdio.h>
#include <string.h>

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#ifndef ASF_E_BUFFERTOOSMALL
#define ASF_E_BUFFERTOOSMALL ((HRESULT)0xc00d07d1)
#endif
#ifndef ASF_E_NOTFOUND
#define ASF_E_NOTFOUND ((HRESULT)0xc00d07f0)
#endif
#ifndef NS_E_INVALID_REQUEST
#define NS_E_INVALID_REQUEST ((HRESULT)0xc00d002b)
#endif

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}
#define CHECKF(ok, ...) do { char _b[300]; snprintf(_b, sizeof(_b), __VA_ARGS__); check(ok, _b); } while (0)
#define CHECKHR(hr, exp, what) CHECKF((hr) == (HRESULT)(exp), "%s: hr %#lx (want %#lx)", what, (unsigned long)(hr), (unsigned long)(HRESULT)(exp))

static HRESULT (WINAPI *pWMCreateSyncReader)(IUnknown *, DWORD, IWMSyncReader **);
static const WCHAR *path;

static IWMSyncReader *open_reader(int do_open)
{
    IWMSyncReader *reader = NULL;
    HRESULT hr = pWMCreateSyncReader(NULL, 0, &reader);
    if (hr == S_OK && do_open) hr = IWMSyncReader_Open(reader, path);
    if (hr != S_OK) { printf("FAIL  cannot open the reader: %#lx\n", (unsigned long)hr); exit(1); }
    return reader;
}

/* ---- IWMHeaderInfo ---------------------------------------------------- */
struct attr_row { const WCHAR *name; WMT_ATTR_DATATYPE type; DWORD size; };
static const struct attr_row attrs[] = {
    {L"Duration", WMT_TYPE_QWORD, 8}, {L"Seekable", WMT_TYPE_BOOL, 4},
};

static void test_header_info(void)
{
    IWMSyncReader *reader = open_reader(1), *closed = open_reader(0);
    IWMHeaderInfo3 *hi, *hi_closed;
    WMT_ATTR_DATATYPE type;
    WORD count, sn, nlen, size, idx[4], n, lang;
    DWORD size32;
    WCHAR name[64];
    BYTE value[16];
    HRESULT hr;
    unsigned i;
    QWORD q;

    IWMSyncReader_QueryInterface(reader, &IID_IWMHeaderInfo3, (void **)&hi);
    IWMSyncReader_QueryInterface(closed, &IID_IWMHeaderInfo3, (void **)&hi_closed);

    hr = IWMHeaderInfo3_GetAttributeCount(hi, 0, NULL);
    CHECKHR(hr, E_INVALIDARG, "GetAttributeCount(NULL)");
    count = 0xdead;
    hr = IWMHeaderInfo3_GetAttributeCount(hi_closed, 0, &count);
    CHECKHR(hr, E_UNEXPECTED, "GetAttributeCount before Open");
    count = 0xdead;
    hr = IWMHeaderInfo3_GetAttributeCount(hi, 0, &count);
    CHECKF(hr == S_OK && count == ARRAY_SIZE(attrs), "file attribute count %u (hr %#lx)", count, (unsigned long)hr);
    count = 0xdead;
    hr = IWMHeaderInfo3_GetAttributeCount(hi, 1, &count);
    CHECKF(hr == S_OK && count == 0, "stream 1 has no attributes (%u)", count);
    count = 0xdead;
    hr = IWMHeaderInfo3_GetAttributeCountEx(hi, 0, &count);
    CHECKF(hr == S_OK && count == ARRAY_SIZE(attrs), "GetAttributeCountEx agrees (%u)", count);

    for (i = 0; i < ARRAY_SIZE(attrs); i++)
    {
        char what[100];
        snprintf(what, sizeof(what), "attribute %u", i);

        sn = 0; nlen = 0xdead; size = 0xdead; type = 0xdead;
        hr = IWMHeaderInfo3_GetAttributeByIndex(hi, i, &sn, NULL, &nlen, &type, NULL, &size);
        CHECKF(hr == S_OK && nlen == wcslen(attrs[i].name) + 1 && type == attrs[i].type && size == attrs[i].size,
                "%s: NULL buffers give the lengths and the type (hr %#lx nlen %u type %d size %u)", what,
                (unsigned long)hr, nlen, type, size);
        sn = 0; nlen = 2; size = 0xdead; type = 0xdead; name[0] = 0xcccc;
        hr = IWMHeaderInfo3_GetAttributeByIndex(hi, i, &sn, name, &nlen, &type, NULL, &size);
        CHECKF(hr == ASF_E_BUFFERTOOSMALL && nlen == wcslen(attrs[i].name) + 1 && name[0] == 0xcccc && type == 0xdead,
                "%s: short name buffer (hr %#lx nlen %u)", what, (unsigned long)hr, nlen);
        sn = 0; nlen = ARRAY_SIZE(name); size = 1; type = 0xdead;
        hr = IWMHeaderInfo3_GetAttributeByIndex(hi, i, &sn, name, &nlen, &type, value, &size);
        CHECKF(hr == ASF_E_BUFFERTOOSMALL && size == attrs[i].size, "%s: short value buffer (hr %#lx size %u)", what,
                (unsigned long)hr, size);
        sn = 0; nlen = ARRAY_SIZE(name); size = sizeof(value); type = 0xdead;
        memset(value, 0, sizeof(value));
        hr = IWMHeaderInfo3_GetAttributeByIndex(hi, i, &sn, name, &nlen, &type, value, &size);
        CHECKF(hr == S_OK && !wcscmp(name, attrs[i].name) && type == attrs[i].type && size == attrs[i].size && sn == 0,
                "%s: name %ls type %d size %u (hr %#lx)", what, name, type, size, (unsigned long)hr);
        if (attrs[i].type == WMT_TYPE_QWORD)
        {
            memcpy(&q, value, 8);
            CHECKF(q > 0, "%s: duration %I64u is read", what, q);
        }
        else
            CHECKF(*(BOOL *)value == TRUE, "%s: seekable is TRUE", what);

        /* the same through ByName, ByIndexEx and Indices */
        sn = 0; size = sizeof(value);
        hr = IWMHeaderInfo3_GetAttributeByName(hi, &sn, attrs[i].name, &type, value, &size);
        CHECKF(hr == S_OK && type == attrs[i].type, "%s: ByName finds it", what);
        sn = 0; lang = 0xdead; size32 = sizeof(value); nlen = ARRAY_SIZE(name); type = 0xdead;
        hr = IWMHeaderInfo3_GetAttributeByIndexEx(hi, 0, i, name, &nlen, &type, &lang, value, &size32);
        CHECKF(hr == S_OK && !wcscmp(name, attrs[i].name) && lang == 0 && size32 == attrs[i].size && type == attrs[i].type,
                "%s: ByIndexEx (hr %#lx)", what, (unsigned long)hr);
        n = 4; lang = 0;
        hr = IWMHeaderInfo3_GetAttributeIndices(hi, 0, attrs[i].name, &lang, idx, &n);
        CHECKF(hr == S_OK && n == 1 && idx[0] == i, "%s: GetAttributeIndices gives index %u (hr %#lx n %u)", what,
                idx[0], (unsigned long)hr, n);
    }
    n = 4;
    hr = IWMHeaderInfo3_GetAttributeIndices(hi, 0, L"No/Such", NULL, idx, &n);
    CHECKHR(hr, ASF_E_NOTFOUND, "GetAttributeIndices of an unknown name");
    n = 4;
    hr = IWMHeaderInfo3_GetAttributeIndices(hi, 0, L"Duration", NULL, NULL, &n);
    CHECKF(hr == S_OK && n == 1, "GetAttributeIndices with no buffer counts");
    n = 0;
    hr = IWMHeaderInfo3_GetAttributeIndices(hi, 0, L"Duration", NULL, idx, &n);
    CHECKHR(hr, ASF_E_BUFFERTOOSMALL, "GetAttributeIndices with room for none");

    sn = 0; nlen = ARRAY_SIZE(name); size = sizeof(value);
    hr = IWMHeaderInfo3_GetAttributeByIndex(hi, ARRAY_SIZE(attrs), &sn, name, &nlen, &type, value, &size);
    CHECKHR(hr, E_INVALIDARG, "GetAttributeByIndex out of range");
    hr = IWMHeaderInfo3_GetAttributeByIndex(hi, 0, NULL, name, &nlen, &type, value, &size);
    CHECKHR(hr, E_INVALIDARG, "GetAttributeByIndex without stream number");
    sn = 1;
    hr = IWMHeaderInfo3_GetAttributeByIndex(hi, 0, &sn, name, &nlen, &type, value, &size);
    CHECKHR(hr, E_INVALIDARG, "GetAttributeByIndex on a stream (no attributes)");
    sn = 0; nlen = ARRAY_SIZE(name); size = sizeof(value);
    hr = IWMHeaderInfo3_GetAttributeByName(hi, &sn, L"No/Such", &type, value, &size);
    CHECKHR(hr, ASF_E_NOTFOUND, "GetAttributeByName of an unknown name");

    /* markers and scripts: none in the file */
    count = 0xdead;
    hr = IWMHeaderInfo3_GetMarkerCount(hi, &count);
    CHECKF(hr == S_OK && count == 0, "no markers (hr %#lx)", (unsigned long)hr);
    hr = IWMHeaderInfo3_GetMarkerCount(hi, NULL);
    CHECKHR(hr, E_INVALIDARG, "GetMarkerCount(NULL)");
    hr = IWMHeaderInfo3_GetMarkerCount(hi_closed, &count);
    CHECKHR(hr, E_UNEXPECTED, "GetMarkerCount before Open");
    nlen = ARRAY_SIZE(name);
    hr = IWMHeaderInfo3_GetMarker(hi, 0, name, &nlen, &q);
    CHECKHR(hr, E_INVALIDARG, "GetMarker(0) with none");
    count = 0xdead;
    hr = IWMHeaderInfo3_GetScriptCount(hi, &count);
    CHECKF(hr == S_OK && count == 0, "no scripts (hr %#lx)", (unsigned long)hr);
    hr = IWMHeaderInfo3_GetScriptCount(hi, NULL);
    CHECKHR(hr, E_INVALIDARG, "GetScriptCount(NULL)");
    {
        WORD tl = 8, cl = 8; WCHAR t[8], c[8];
        hr = IWMHeaderInfo3_GetScript(hi, 0, t, &tl, c, &cl, &q);
        CHECKHR(hr, E_INVALIDARG, "GetScript(0) with none");
    }

    /* a reader's header is read-only */
    hr = IWMHeaderInfo3_SetAttribute(hi, 0, L"Title", WMT_TYPE_STRING, (BYTE *)L"x", 4);
    CHECKHR(hr, E_NOTIMPL, "SetAttribute is refused");
    hr = IWMHeaderInfo3_AddMarker(hi, L"m", 0);
    CHECKHR(hr, E_NOTIMPL, "AddMarker is refused");
    hr = IWMHeaderInfo3_RemoveMarker(hi, 0);
    CHECKHR(hr, E_NOTIMPL, "RemoveMarker is refused");
    hr = IWMHeaderInfo3_AddScript(hi, L"t", L"c", 0);
    CHECKHR(hr, E_NOTIMPL, "AddScript is refused");
    hr = IWMHeaderInfo3_RemoveScript(hi, 0);
    CHECKHR(hr, E_NOTIMPL, "RemoveScript is refused");
    hr = IWMHeaderInfo3_ModifyAttribute(hi, 0, 0, WMT_TYPE_DWORD, 0, (BYTE *)&size32, 4);
    CHECKHR(hr, E_NOTIMPL, "ModifyAttribute is refused");
    hr = IWMHeaderInfo3_AddAttribute(hi, 0, L"x", &n, WMT_TYPE_DWORD, 0, (BYTE *)&size32, 4);
    CHECKHR(hr, E_NOTIMPL, "AddAttribute is refused");
    hr = IWMHeaderInfo3_DeleteAttribute(hi, 0, 0);
    CHECKHR(hr, E_NOTIMPL, "DeleteAttribute is refused");
    hr = IWMHeaderInfo3_AddCodecInfo(hi, L"x", L"y", WMT_CODECINFO_AUDIO, 2, value);
    CHECKHR(hr, E_NOTIMPL, "AddCodecInfo is refused");

    /* codec info: one per stream, video then audio in this file; the codec
     * information is the FOURCC (video) or the format tag (audio) */
    {
        DWORD cc = 0xdead;
        WORD nl, dl, isz;
        WCHAR dsc[8];
        WMT_CODEC_INFO_TYPE ct;
        int seen_video = 0, seen_audio = 0;

        hr = IWMHeaderInfo3_GetCodecInfoCount(hi, NULL);
        CHECKHR(hr, E_INVALIDARG, "GetCodecInfoCount(NULL)");
        hr = IWMHeaderInfo3_GetCodecInfoCount(hi_closed, &cc);
        CHECKHR(hr, E_UNEXPECTED, "GetCodecInfoCount before Open");
        hr = IWMHeaderInfo3_GetCodecInfoCount(hi, &cc);
        CHECKF(hr == S_OK && cc == 2, "two codecs (hr %#lx, %lu)", (unsigned long)hr, cc);
        for (i = 0; i < cc; i++)
        {
            nl = 0xdead; dl = 0xdead; isz = 0xdead;
            hr = IWMHeaderInfo3_GetCodecInfo(hi, i, &nl, NULL, &dl, NULL, &ct, &isz, NULL);
            CHECKF(hr == S_OK && nl > 1 && dl == 1 && (isz == 4 || isz == 2), "codec %u sizes (hr %#lx nl %u dl %u isz %u)", i,
                    (unsigned long)hr, nl, dl, isz);
            nl = ARRAY_SIZE(name); dl = ARRAY_SIZE(dsc); isz = sizeof(value); ct = 0xdead;
            hr = IWMHeaderInfo3_GetCodecInfo(hi, i, &nl, name, &dl, dsc, &ct, &isz, value);
            if (ct == WMT_CODECINFO_VIDEO)
            {
                seen_video++;
                CHECKF(hr == S_OK && isz == 4 && !memcmp(value, "WMV1", 4) && !wcscmp(name, L"WMV1"),
                        "video codec is WMV1 (name %ls)", name);
            }
            else if (ct == WMT_CODECINFO_AUDIO)
            {
                WORD tag;
                seen_audio++;
                memcpy(&tag, value, 2);
                CHECKF(hr == S_OK && isz == 2 && tag == 0x160, "audio codec has format tag 0x160 (tag %#x)", tag);
            }
            nl = 1;
            hr = IWMHeaderInfo3_GetCodecInfo(hi, i, &nl, name, &dl, dsc, &ct, &isz, value);
            CHECKHR(hr, ASF_E_BUFFERTOOSMALL, "GetCodecInfo with a short name buffer");
        }
        CHECKF(seen_video == 1 && seen_audio == 1, "one video and one audio codec");
        nl = dl = isz = 8;
        hr = IWMHeaderInfo3_GetCodecInfo(hi, 2, &nl, name, &dl, dsc, &ct, &isz, value);
        CHECKHR(hr, E_INVALIDARG, "GetCodecInfo out of range");
    }
    IWMHeaderInfo3_Release(hi);
    IWMHeaderInfo3_Release(hi_closed);
    IWMSyncReader_Release(reader);
    IWMSyncReader_Release(closed);
}

/* ---- language list, packet size, timecode, playlist ------------------- */
static void test_misc_interfaces(void)
{
    IWMSyncReader *reader = open_reader(1), *closed = open_reader(0);
    IWMLanguageList *ll, *ll_closed;
    IWMPacketSize2 *ps;
    IWMReaderTimecode *tc;
    IWMReaderPlaylistBurn *pb;
    WORD count, idx, len;
    DWORD size, s, e;
    WCHAR lang[16];
    HRESULT hr, hrs[2];

    IWMSyncReader_QueryInterface(reader, &IID_IWMLanguageList, (void **)&ll);
    IWMSyncReader_QueryInterface(closed, &IID_IWMLanguageList, (void **)&ll_closed);
    hr = IWMLanguageList_GetLanguageCount(ll, NULL);
    CHECKHR(hr, E_INVALIDARG, "GetLanguageCount(NULL)");
    count = 0xdead;
    hr = IWMLanguageList_GetLanguageCount(ll_closed, &count);
    CHECKHR(hr, E_UNEXPECTED, "GetLanguageCount before Open");
    hr = IWMLanguageList_GetLanguageCount(ll, &count);
    CHECKF(hr == S_OK && count == 0, "no languages in the file (hr %#lx count %u)", (unsigned long)hr, count);
    len = ARRAY_SIZE(lang);
    hr = IWMLanguageList_GetLanguageDetails(ll, 0, lang, &len);
    CHECKHR(hr, E_INVALIDARG, "GetLanguageDetails(0) of an empty list");
    hr = IWMLanguageList_GetLanguageDetails(ll, 0, lang, NULL);
    CHECKHR(hr, E_INVALIDARG, "GetLanguageDetails with no length");
    hr = IWMLanguageList_AddLanguageByRFC1766String(ll, L"en-us", &idx);
    CHECKHR(hr, E_NOTIMPL, "AddLanguageByRFC1766String is refused on a reader");
    IWMLanguageList_Release(ll);
    IWMLanguageList_Release(ll_closed);

    IWMSyncReader_QueryInterface(reader, &IID_IWMPacketSize2, (void **)&ps);
    hr = IWMPacketSize2_GetMaxPacketSize(ps, NULL);
    CHECKHR(hr, E_INVALIDARG, "GetMaxPacketSize(NULL)");
    hr = IWMPacketSize2_GetMinPacketSize(ps, NULL);
    CHECKHR(hr, E_INVALIDARG, "GetMinPacketSize(NULL)");
    size = 0;
    hr = IWMPacketSize2_GetMaxPacketSize(ps, &size);
    CHECKF(hr == S_OK && size == 3200, "max packet size 3200 (hr %#lx, %lu)", (unsigned long)hr, size);
    size = 0;
    hr = IWMPacketSize2_GetMinPacketSize(ps, &size);
    CHECKF(hr == S_OK && size == 3200, "min packet size 3200 (hr %#lx, %lu)", (unsigned long)hr, size);
    hr = IWMPacketSize2_SetMaxPacketSize(ps, 4096);
    CHECKHR(hr, E_NOTIMPL, "SetMaxPacketSize is refused on a reader");
    hr = IWMPacketSize2_SetMinPacketSize(ps, 4096);
    CHECKHR(hr, E_NOTIMPL, "SetMinPacketSize is refused on a reader");
    IWMPacketSize2_Release(ps);

    IWMSyncReader_QueryInterface(reader, &IID_IWMReaderTimecode, (void **)&tc);
    hr = IWMReaderTimecode_GetTimecodeRangeCount(tc, 1, NULL);
    CHECKHR(hr, E_INVALIDARG, "GetTimecodeRangeCount(NULL)");
    count = 0xdead;
    hr = IWMReaderTimecode_GetTimecodeRangeCount(tc, 1, &count);
    CHECKF(hr == S_OK && count == 0, "no timecode ranges on stream 1 (hr %#lx count %u)", (unsigned long)hr, count);
    hr = IWMReaderTimecode_GetTimecodeRangeCount(tc, 2, &count);
    CHECKF(hr == S_OK && count == 0, "no timecode ranges on stream 2");
    hr = IWMReaderTimecode_GetTimecodeRangeCount(tc, 9, &count);
    CHECKHR(hr, E_INVALIDARG, "GetTimecodeRangeCount of a stream that does not exist");
    hr = IWMReaderTimecode_GetTimecodeRangeBounds(tc, 1, 0, &s, &e);
    CHECKHR(hr, E_INVALIDARG, "GetTimecodeRangeBounds(0) with no ranges");
    hr = IWMReaderTimecode_GetTimecodeRangeBounds(tc, 1, 0, NULL, &e);
    CHECKHR(hr, E_INVALIDARG, "GetTimecodeRangeBounds(NULL)");
    IWMReaderTimecode_Release(tc);

    IWMSyncReader_QueryInterface(reader, &IID_IWMReaderPlaylistBurn, (void **)&pb);
    hr = IWMReaderPlaylistBurn_GetInitResults(pb, 2, NULL);
    CHECKHR(hr, E_INVALIDARG, "GetInitResults(NULL)");
    hr = IWMReaderPlaylistBurn_GetInitResults(pb, 2, hrs);
    CHECKHR(hr, E_UNEXPECTED, "GetInitResults without a burn");
    hr = IWMReaderPlaylistBurn_InitPlaylistBurn(pb, 0, NULL, NULL, NULL);
    CHECKHR(hr, E_INVALIDARG, "InitPlaylistBurn with no files");
    hr = IWMReaderPlaylistBurn_Cancel(pb);
    CHECKHR(hr, S_OK, "Cancel with nothing to cancel");
    hr = IWMReaderPlaylistBurn_EndPlaylistBurn(pb, S_OK);
    CHECKHR(hr, S_OK, "EndPlaylistBurn with nothing to end");
    IWMReaderPlaylistBurn_Release(pb);

    IWMSyncReader_Release(reader);
    IWMSyncReader_Release(closed);
}

/* ---- output settings -------------------------------------------------- */
struct setting_row
{
    const WCHAR *name;
    WMT_ATTR_DATATYPE type;
    DWORD deflt[2];          /* default value on audio / video output */
    HRESULT get[2], set[2];  /* result on audio / video output */
    DWORD newval;
};
#define IV E_INVALIDARG
#define IR NS_E_INVALID_REQUEST
static const struct setting_row settings[] = {
    {L"AllowInterlacedOutput",    WMT_TYPE_BOOL,  {0, 0},       {IV, S_OK}, {IV, S_OK}, 1},
    {L"DedicatedDeliveryThread",  WMT_TYPE_BOOL,  {0, 0},       {IR, IR},   {IR, IR},   1},
    {L"DeliverOnReceive",         WMT_TYPE_BOOL,  {0, 0},       {S_OK, S_OK}, {S_OK, S_OK}, 1},
    {L"EnableDiscreteOutput",     WMT_TYPE_BOOL,  {0, 0},       {S_OK, IV}, {S_OK, IV}, 1},
    {L"EnableFrameInterpolation", WMT_TYPE_BOOL,  {0, 0},       {IV, S_OK}, {IV, S_OK}, 1},
    {L"JustInTimeDecode",         WMT_TYPE_BOOL,  {0, 0},       {IR, IR},   {IR, IR},   1},
    {L"NeedsPreviousSample",      WMT_TYPE_BOOL,  {0, 0},       {IV, IR},   {IV, IV},   1},
    {L"ScrambledAudio",           WMT_TYPE_BOOL,  {0, 0},       {IV, IV},   {IV, IV},   1},
    {L"SingleOutputBuffer",       WMT_TYPE_BOOL,  {0, 0},       {IR, IR},   {IR, IR},   1},
    {L"SoftwareScaling",          WMT_TYPE_BOOL,  {0, 1},       {IV, S_OK}, {IV, S_OK}, 0},
    {L"VideoSampleDurations",     WMT_TYPE_BOOL,  {0, 0},       {IV, S_OK}, {IV, S_OK}, 1},
    {L"EnableWMAProSPDIFOutput",  WMT_TYPE_BOOL,  {0, 0},       {IV, IV},   {S_OK, IV}, 1},
    {L"StreamLanguage",           WMT_TYPE_WORD,  {0, 0},       {IR, IR},   {S_OK, S_OK}, 7},
    {L"DynamicRangeControl",      WMT_TYPE_DWORD, {~0u, 0},     {S_OK, IV}, {S_OK, IV}, 2},
    {L"EarlyDataDelivery",        WMT_TYPE_DWORD, {0, 0},       {S_OK, S_OK}, {S_OK, S_OK}, 1500},
    {L"SpeakerConfig",            WMT_TYPE_DWORD, {~0u, 0},     {S_OK, IV}, {S_OK, IV}, 3},
};

static DWORD type_size(WMT_ATTR_DATATYPE t) { return t == WMT_TYPE_WORD ? 2 : t == WMT_TYPE_BOOL ? 4 : 4; }

static DWORD output_of_type(IWMSyncReader *reader, const GUID *want)
{
    DWORD i, n = 0;
    IWMSyncReader_GetOutputCount(reader, &n);
    for (i = 0; i < n; i++)
    {
        IWMOutputMediaProps *p; GUID t;
        IWMSyncReader_GetOutputProps(reader, i, &p);
        IWMOutputMediaProps_GetType(p, &t);
        IWMOutputMediaProps_Release(p);
        if (IsEqualGUID(&t, want)) return i;
    }
    return 0xdead;
}

static void test_output_settings(void)
{
    IWMSyncReader *reader = open_reader(0), *r2;
    DWORD out[2], k, v;
    WMT_ATTR_DATATYPE type;
    WORD size;
    HRESULT hr;
    unsigned i;

    type = WMT_TYPE_BOOL; size = 4; v = 0;
    hr = IWMSyncReader_GetOutputSetting(reader, 0, L"AllowInterlacedOutput", &type, (BYTE *)&v, &size);
    CHECKHR(hr, E_UNEXPECTED, "GetOutputSetting before Open");
    hr = IWMSyncReader_SetOutputSetting(reader, 0, L"AllowInterlacedOutput", WMT_TYPE_BOOL, (BYTE *)&v, 4);
    CHECKHR(hr, E_UNEXPECTED, "SetOutputSetting before Open");
    IWMSyncReader_Release(reader);

    reader = open_reader(1);
    out[0] = output_of_type(reader, &WMMEDIATYPE_Audio);
    out[1] = output_of_type(reader, &WMMEDIATYPE_Video);
    CHECKF(out[0] != 0xdead && out[1] != 0xdead && out[0] != out[1], "audio output %lu, video output %lu", out[0], out[1]);

    for (i = 0; i < ARRAY_SIZE(settings); i++)
    {
        const struct setting_row *r = &settings[i];
        for (k = 0; k < 2; k++)
        {
            char what[120];
            DWORD want_size = type_size(r->type);
            snprintf(what, sizeof(what), "%ls on the %s output", r->name, k ? "video" : "audio");

            type = 0xdead; size = want_size; v = 0xbaadf00d;
            hr = IWMSyncReader_GetOutputSetting(reader, out[k], r->name, &type, (BYTE *)&v, &size);
            if (r->get[k] != S_OK)
                CHECKF(hr == r->get[k], "get %s: hr %#lx (want %#lx)", what, (unsigned long)hr, (unsigned long)r->get[k]);
            else
            {
                DWORD mask = r->type == WMT_TYPE_WORD ? 0xffff : ~0u;
                CHECKF(hr == S_OK && type == r->type && size == want_size && (v & mask) == (r->deflt[k] & mask),
                        "get %s: default (hr %#lx type %d size %u value %#lx)", what, (unsigned long)hr, type, size, v);
            }
            /* no buffer: type and size, buffer too short */
            if (r->get[k] == S_OK)
            {
                type = 0xdead; size = 0xdead;
                hr = IWMSyncReader_GetOutputSetting(reader, out[k], r->name, &type, NULL, &size);
                CHECKF(hr == S_OK && type == r->type && size == want_size, "get %s with no buffer: size %u", what, size);
                size = 1; v = 0xbaadf00d;
                hr = IWMSyncReader_GetOutputSetting(reader, out[k], r->name, &type, (BYTE *)&v, &size);
                CHECKF(hr == ASF_E_BUFFERTOOSMALL && size == want_size, "get %s with a 1 byte buffer (hr %#lx)", what, (unsigned long)hr);
            }
            /* set */
            v = r->newval;
            hr = IWMSyncReader_SetOutputSetting(reader, out[k], r->name, r->type, (BYTE *)&v, want_size);
            CHECKF(hr == r->set[k], "set %s: hr %#lx (want %#lx)", what, (unsigned long)hr, (unsigned long)r->set[k]);
            if (r->set[k] == S_OK && r->get[k] == S_OK)
            {
                DWORD mask = r->type == WMT_TYPE_WORD ? 0xffff : ~0u, want = r->newval;
                v = 0xbaadf00d; size = want_size;
                hr = IWMSyncReader_GetOutputSetting(reader, out[k], r->name, &type, (BYTE *)&v, &size);
                CHECKF(hr == S_OK && (v & mask) == want, "get %s after set: %#lx (want %#lx)", what, v, want);
            }
            /* the wrong type or size is refused */
            if (r->set[k] == S_OK)
            {
                v = 1;
                hr = IWMSyncReader_SetOutputSetting(reader, out[k], r->name, r->type == WMT_TYPE_WORD ? WMT_TYPE_DWORD : WMT_TYPE_WORD,
                        (BYTE *)&v, r->type == WMT_TYPE_WORD ? 4 : 2);
                CHECKF(hr == E_INVALIDARG, "set %s with the wrong type: hr %#lx", what, (unsigned long)hr);
            }
        }
    }
    /* a BOOL is stored as 0 or 1 */
    v = 0x100;
    hr = IWMSyncReader_SetOutputSetting(reader, out[0], L"DeliverOnReceive", WMT_TYPE_BOOL, (BYTE *)&v, 4);
    v = 0; size = 4;
    IWMSyncReader_GetOutputSetting(reader, out[0], L"DeliverOnReceive", &type, (BYTE *)&v, &size);
    CHECKF(v == 1, "a non-zero BOOL reads back as TRUE (%#lx)", v);

    /* settings belong to one output */
    v = 1;
    IWMSyncReader_SetOutputSetting(reader, out[1], L"VideoSampleDurations", WMT_TYPE_BOOL, (BYTE *)&v, 4);
    v = 0;
    IWMSyncReader_SetOutputSetting(reader, out[1], L"EarlyDataDelivery", WMT_TYPE_DWORD, (BYTE *)&v, 4);
    v = 0xbaad; size = 4;
    IWMSyncReader_GetOutputSetting(reader, out[0], L"EarlyDataDelivery", &type, (BYTE *)&v, &size);
    CHECKF(v == 1500, "EarlyDataDelivery of the audio output is not that of the video output (%lu)", v);

    /* bad output, unknown name, bad arguments */
    v = 0; size = 4;
    hr = IWMSyncReader_GetOutputSetting(reader, 9, L"DeliverOnReceive", &type, (BYTE *)&v, &size);
    CHECKHR(hr, E_INVALIDARG, "GetOutputSetting of output 9");
    hr = IWMSyncReader_SetOutputSetting(reader, 9, L"DeliverOnReceive", WMT_TYPE_BOOL, (BYTE *)&v, 4);
    CHECKHR(hr, E_INVALIDARG, "SetOutputSetting of output 9");
    hr = IWMSyncReader_GetOutputSetting(reader, out[0], L"NoSuchSetting", &type, (BYTE *)&v, &size);
    CHECKHR(hr, E_INVALIDARG, "GetOutputSetting of an unknown name");
    hr = IWMSyncReader_SetOutputSetting(reader, out[0], L"NoSuchSetting", WMT_TYPE_BOOL, (BYTE *)&v, 4);
    CHECKHR(hr, E_INVALIDARG, "SetOutputSetting of an unknown name");
    hr = IWMSyncReader_GetOutputSetting(reader, out[0], L"DeliverOnReceive", NULL, (BYTE *)&v, &size);
    CHECKHR(hr, E_INVALIDARG, "GetOutputSetting without a type");
    hr = IWMSyncReader_GetOutputSetting(reader, out[0], L"DeliverOnReceive", &type, (BYTE *)&v, NULL);
    CHECKHR(hr, E_INVALIDARG, "GetOutputSetting without a length");

    /* another reader has its own settings */
    r2 = open_reader(1);
    size = 4; v = 0xbaad;
    IWMSyncReader_GetOutputSetting(r2, output_of_type(r2, &WMMEDIATYPE_Video), L"VideoSampleDurations", &type, (BYTE *)&v, &size);
    CHECKF(v == 0, "a second reader starts with the defaults (%lu)", v);
    IWMSyncReader_Release(r2);
    IWMSyncReader_Release(reader);
}

/* ---- max sample size, range by frame ---------------------------------- */
static void test_ranges(void)
{
    IWMSyncReader *reader = open_reader(1);
    IWMProfile3 *profile;
    DWORD out_v, out_a, max_o, max_s, n, flags, output;
    INSSBuffer *buf;
    QWORD pts, dur, start, first_pts = 0, last_pts = 0;
    WORD sn_v = 1, sn_a = 2, sn_out;
    unsigned frames = 0;
    HRESULT hr;
    IWMStreamConfig *c;
    GUID type;
    WMT_TIMECODE_EXTENSION_DATA tc = {0};
    WORD i;

    IWMSyncReader_QueryInterface(reader, &IID_IWMProfile3, (void **)&profile);
    for (i = 0; i < 2; i++)
    {
        IWMProfile3_GetStream(profile, i, &c);
        IWMStreamConfig_GetStreamType(c, &type);
        if (IsEqualGUID(&type, &WMMEDIATYPE_Video)) IWMStreamConfig_GetStreamNumber(c, &sn_v);
        else IWMStreamConfig_GetStreamNumber(c, &sn_a);
        IWMStreamConfig_Release(c);
    }
    IWMProfile3_Release(profile);
    out_v = output_of_type(reader, &WMMEDIATYPE_Video);
    out_a = output_of_type(reader, &WMMEDIATYPE_Audio);

    for (n = 0; n < 2; n++)
    {
        DWORD out = n ? out_v : out_a;
        WORD sn = n ? sn_v : sn_a;
        max_o = max_s = 0;
        hr = IWMSyncReader_GetMaxOutputSampleSize(reader, out, &max_o);
        CHECKF(hr == S_OK && max_o > 0, "GetMaxOutputSampleSize(%s) = %lu (hr %#lx)", n ? "video" : "audio", max_o, (unsigned long)hr);
        hr = IWMSyncReader_GetMaxStreamSampleSize(reader, sn, &max_s);
        CHECKF(hr == S_OK && max_s == max_o, "the output and its stream agree (%lu, %lu)", max_o, max_s);
    }
    hr = IWMSyncReader_GetMaxOutputSampleSize(reader, 9, &max_o);
    CHECKHR(hr, E_INVALIDARG, "GetMaxOutputSampleSize of output 9");
    hr = IWMSyncReader_GetMaxOutputSampleSize(reader, 0, NULL);
    CHECKHR(hr, E_INVALIDARG, "GetMaxOutputSampleSize(NULL)");

    /* range by frame */
    hr = IWMSyncReader2_SetRangeByFrame((IWMSyncReader2 *)reader, 9, 1, 1);
    CHECKHR(hr, E_INVALIDARG, "SetRangeByFrame of stream 9");
    hr = IWMSyncReader2_SetRangeByFrame((IWMSyncReader2 *)reader, sn_a, 1, 1);
    CHECKHR(hr, NS_E_INVALID_REQUEST, "SetRangeByFrame of the audio stream");
    hr = IWMSyncReader2_SetRangeByFrame((IWMSyncReader2 *)reader, sn_v, 1, -1);
    CHECKHR(hr, E_INVALIDARG, "SetRangeByFrame with negative frame count");
    hr = IWMSyncReader2_SetRangeByFrameEx((IWMSyncReader2 *)reader, sn_v, 1, 1, NULL);
    CHECKHR(hr, E_INVALIDARG, "SetRangeByFrameEx without the start time");

    start = 0xdead;
    hr = IWMSyncReader2_SetRangeByFrameEx((IWMSyncReader2 *)reader, sn_v, 0, 1, &start);
    CHECKF(hr == S_OK && start == 0, "frame 0 starts at 0 (hr %#lx %I64u)", (unsigned long)hr, start);
    {
        QWORD s10 = 0, s20 = 0;
        hr = IWMSyncReader2_SetRangeByFrameEx((IWMSyncReader2 *)reader, sn_v, 10, 2, &s10);
        CHECKHR(hr, S_OK, "SetRangeByFrameEx(10, 2)");
        hr = IWMSyncReader2_SetRangeByFrameEx((IWMSyncReader2 *)reader, sn_v, 20, 2, &s20);
        CHECKF(hr == S_OK && s10 > 0 && s20 == 2 * s10, "start time is frame * frame duration (%I64u, %I64u)", s10, s20);
        dur = s10 / 10;
        printf("note: frame duration %I64u\n", dur);
        hr = IWMSyncReader2_SetRangeByFrame((IWMSyncReader2 *)reader, sn_v, 20, 3);
        CHECKHR(hr, S_OK, "SetRangeByFrame(20, 3)");
        for (;;)
        {
            hr = IWMSyncReader_GetNextSample(reader, sn_v, &buf, &pts, &dur, &flags, &output, &sn_out);
            if (hr != S_OK) break;
            INSSBuffer_Release(buf);
            if (!frames++) first_pts = pts;
            last_pts = pts;
        }
        CHECKF(frames >= 1 && first_pts >= s20 - 2 * dur && first_pts <= s20 + 2 * dur,
                "the first video sample (%I64u) is at the requested frame (%I64u), %u frames read", first_pts, s20, frames);
        CHECKF(last_pts < s20 + 5 * (s10 / 10), "reading stops after about the 3 frames asked for (last %I64u)", last_pts);
    }

    /* timecode ranges: none in the file */
    hr = IWMSyncReader2_SetRangeByTimecode((IWMSyncReader2 *)reader, sn_v, NULL, &tc);
    CHECKHR(hr, E_INVALIDARG, "SetRangeByTimecode without a start");
    hr = IWMSyncReader2_SetRangeByTimecode((IWMSyncReader2 *)reader, 9, &tc, &tc);
    CHECKHR(hr, E_INVALIDARG, "SetRangeByTimecode of stream 9");
    hr = IWMSyncReader2_SetRangeByTimecode((IWMSyncReader2 *)reader, sn_v, &tc, &tc);
    CHECKHR(hr, NS_E_INVALID_REQUEST, "SetRangeByTimecode on a stream with no timecode");
    IWMSyncReader_Release(reader);
}

int main(int argc, char **argv)
{
    static WCHAR wpath[MAX_PATH];
    HMODULE wmvcore;

    if (argc < 2) { printf("usage: %s file.wmv\n", argv[0]); return 2; }
    MultiByteToWideChar(CP_ACP, 0, argv[1], -1, wpath, MAX_PATH);
    path = wpath;
    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    wmvcore = LoadLibraryA("wmvcore.dll");
    pWMCreateSyncReader = wmvcore ? (void *)GetProcAddress(wmvcore, "WMCreateSyncReader") : NULL;
    if (!pWMCreateSyncReader) { printf("SKIP  no wmvcore\n"); return 77; }

    test_header_info();
    test_misc_interfaces();
    test_output_settings();
    test_ranges();

    printf("%s\nRESULT: %s\n", failures ? "FAILURES" : "all checks passed", failures ? "FAIL" : "PASS");
    return failures ? 1 : 0;
}
