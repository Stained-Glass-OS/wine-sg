/* wmvcore profile XML (patch 2864), run by test/wmvw-xml-gate.sh:
 * IWMProfileManager::SaveProfile and LoadProfileByData: the text of a saved
 * profile, the length protocol, a full round trip of every field, hand
 * written documents, malformed documents, and the profile of an opened
 * reader.
 *
 *   wmvw-xml-probe.exe [path-of-sample.wmv] */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <wmsdk.h>
#include <nserror.h>
#include <stdio.h>
#include <string.h>

#ifndef ASF_E_BUFFERTOOSMALL
#define ASF_E_BUFFERTOOSMALL ((HRESULT)0xc00d07d1)
#endif
static const GUID WMFORMAT_WaveFormatEx = {0x05589f81, 0xc356, 0x11ce, {0xbf, 0x01, 0x00, 0xaa, 0x00, 0x55, 0x59, 0x5a}};
static const GUID WMMEDIATYPE_Script = {0x73636d64, 0x0000, 0x0010, {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}};
static const GUID WMFORMAT_VideoInfo = {0x05589f80, 0xc356, 0x11ce, {0xbf, 0x01, 0x00, 0xaa, 0x00, 0x55, 0x59, 0x5a}};

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}
#define CHECKF(ok, ...) do { char _b[400]; snprintf(_b, sizeof(_b), __VA_ARGS__); check(ok, _b); } while (0)
#define CHECK_HR(hr, exp, what) do { HRESULT _h = (hr); CHECKF(_h == (HRESULT)(exp), "%s = %08lx (got %08lx)", what, (unsigned long)(exp), (unsigned long)_h); } while (0)

static IWMProfileManager *mgr;

static WCHAR *save(IWMProfile *p, HRESULT *hr_out)
{
    DWORD len = 0;
    WCHAR *buf;
    HRESULT hr = IWMProfileManager_SaveProfile(mgr, p, NULL, &len);

    if (hr_out) *hr_out = hr;
    if (FAILED(hr) || !len) return NULL;
    buf = calloc(len + 1, sizeof(WCHAR));
    hr = IWMProfileManager_SaveProfile(mgr, p, buf, &len);
    if (hr_out) *hr_out = hr;
    if (FAILED(hr)) { free(buf); return NULL; }
    return buf;
}

static void add_stream(IWMProfile *p, const GUID *type, WORD number, DWORD bitrate, const WCHAR *name, const WCHAR *conn,
        WM_MEDIA_TYPE *mt, BOOL window)
{
    IWMStreamConfig *c;
    IWMMediaProps *props;

    IWMProfile_CreateNewStream(p, type, &c);
    IWMStreamConfig_SetStreamNumber(c, number);
    IWMStreamConfig_SetBitrate(c, bitrate);
    if (name) IWMStreamConfig_SetStreamName(c, name);
    if (conn) IWMStreamConfig_SetConnectionName(c, conn);
    if (window) IWMStreamConfig_SetBufferWindow(c, 4500);
    if (mt)
    {
        IWMStreamConfig_QueryInterface(c, &IID_IWMMediaProps, (void **)&props);
        IWMMediaProps_SetMediaType(props, mt);
        IWMMediaProps_Release(props);
    }
    IWMProfile_AddStream(p, c);
    IWMStreamConfig_Release(c);
}

static BYTE audio_fmt[18 + 6] = {1, 0, 2, 0, 0x44, 0xac, 0, 0, 0x10, 0xb1, 2, 0, 4, 0, 16, 0, 6, 0, 1, 2, 3, 4, 0xaa, 0xff};
static BYTE video_fmt[88 + 3];

static IWMProfile *build_profile(void)
{
    WM_MEDIA_TYPE amt = {0}, vmt = {0};
    IWMProfile3 *p3;
    IWMProfile *p;
    IWMMutualExclusion *mutex;
    IWMBandwidthSharing *sharing;
    IWMStreamPrioritization *prio;
    WM_STREAM_PRIORITY_RECORD rec[2] = {{2, TRUE}, {5, FALSE}};
    unsigned int i;

    for (i = 0; i < sizeof(video_fmt); i++) video_fmt[i] = i * 7 + 3;
    video_fmt[48] = 40; video_fmt[49] = video_fmt[50] = video_fmt[51] = 0;
    amt.majortype = WMMEDIATYPE_Audio; amt.subtype = WMMEDIASUBTYPE_PCM; amt.bFixedSizeSamples = TRUE; amt.lSampleSize = 4;
    amt.formattype = WMFORMAT_WaveFormatEx; amt.cbFormat = sizeof(audio_fmt); amt.pbFormat = audio_fmt;
    vmt.majortype = WMMEDIATYPE_Video; vmt.subtype = WMMEDIASUBTYPE_WVC1; vmt.bTemporalCompression = TRUE;
    vmt.formattype = WMFORMAT_VideoInfo; vmt.cbFormat = sizeof(video_fmt); vmt.pbFormat = video_fmt;

    IWMProfileManager_CreateEmptyProfile(mgr, WMT_VER_8_0, &p);
    IWMProfile_QueryInterface(p, &IID_IWMProfile3, (void **)&p3);
    IWMProfile_SetName(p, L"Name & <\"quoted\"> é");
    IWMProfile_SetDescription(p, L"It's a description");
    IWMProfile3_SetStorageFormat(p3, WMT_Storage_Format_MP3);
    add_stream(p, &WMMEDIATYPE_Audio, 5, 64000, L"Audio <1>", L"Audio409", &amt, TRUE);
    add_stream(p, &WMMEDIATYPE_Video, 2, 300000, NULL, L"Video409", &vmt, FALSE);
    add_stream(p, &WMMEDIATYPE_Script, 7, 1000, L"", NULL, NULL, FALSE);
    IWMProfile_CreateNewMutualExclusion(p, &mutex);
    IWMMutualExclusion_SetType(mutex, &WMMEDIATYPE_Video);
    IWMMutualExclusion_AddStream(mutex, 5);
    IWMMutualExclusion_AddStream(mutex, 2);
    IWMProfile_AddMutualExclusion(p, mutex);
    IWMMutualExclusion_Release(mutex);
    IWMProfile3_CreateNewBandwidthSharing(p3, &sharing);
    IWMBandwidthSharing_SetBandwidth(sharing, 200000, 3000);
    IWMBandwidthSharing_AddStream(sharing, 2);
    IWMBandwidthSharing_AddStream(sharing, 7);
    IWMProfile3_AddBandwidthSharing(p3, sharing);
    IWMBandwidthSharing_Release(sharing);
    IWMProfile3_CreateNewStreamPrioritization(p3, &prio);
    IWMStreamPrioritization_SetPriorityRecords(prio, rec, 2);
    IWMProfile3_SetStreamPrioritization(p3, prio);
    IWMStreamPrioritization_Release(prio);
    IWMProfile3_Release(p3);
    return p;
}

static WCHAR *get_str(HRESULT (*f)(void *, WCHAR *, DWORD *), void *obj)
{
    DWORD len = 0;
    WCHAR *s;
    f(obj, NULL, &len);
    s = calloc(len + 1, sizeof(WCHAR));
    f(obj, s, &len);
    return s;
}
static HRESULT gname(void *p, WCHAR *b, DWORD *l) { return IWMProfile_GetName((IWMProfile *)p, b, l); }
static HRESULT gdesc(void *p, WCHAR *b, DWORD *l) { return IWMProfile_GetDescription((IWMProfile *)p, b, l); }
static HRESULT gsname(void *p, WCHAR *b, DWORD *l) { WORD n = *l; HRESULT hr = IWMStreamConfig_GetStreamName((IWMStreamConfig *)p, b, &n); *l = n; return hr; }
static HRESULT gcname(void *p, WCHAR *b, DWORD *l) { WORD n = *l; HRESULT hr = IWMStreamConfig_GetConnectionName((IWMStreamConfig *)p, b, &n); *l = n; return hr; }

static void test_roundtrip(void)
{
    IWMProfile *p = build_profile(), *q = NULL;
    IWMProfile3 *q3;
    WCHAR *xml, *xml2, *s;
    DWORD len, n, i;
    WORD count, nums[4];
    WMT_VERSION version;
    WMT_STORAGE_FORMAT storage;
    HRESULT hr;
    static const struct { WORD number; const GUID *type; DWORD bitrate; const WCHAR *name, *conn; DWORD window; DWORD mtsize; } streams[] = {
        {5, &WMMEDIATYPE_Audio, 64000, L"Audio <1>", L"Audio409", 4500, sizeof(WM_MEDIA_TYPE) + sizeof(audio_fmt)},
        {2, &WMMEDIATYPE_Video, 300000, L"", L"Video409", 0xffffffff, sizeof(WM_MEDIA_TYPE) + sizeof(video_fmt)},
        {7, &WMMEDIATYPE_Script, 1000, L"", L"", 0xffffffff, sizeof(WM_MEDIA_TYPE)},
    };

    xml = save(p, &hr);
    CHECKF(xml != NULL, "SaveProfile (%08lx)", hr);
    if (!xml) return;
    len = 0;
    hr = IWMProfileManager_SaveProfile(mgr, p, NULL, &len);
    CHECKF(hr == S_OK && len == wcslen(xml) + 1, "length query = text length + 1 (%lu, %zu)", len, wcslen(xml) + 1);
    len--;
    s = calloc(len + 2, sizeof(WCHAR));
    hr = IWMProfileManager_SaveProfile(mgr, p, s, &len);
    CHECKF(hr == ASF_E_BUFFERTOOSMALL && len == wcslen(xml) + 1, "short buffer = ASF_E_BUFFERTOOSMALL with the length (%08lx, %lu)", hr, len);
    free(s);
    CHECKF(wcsstr(xml, L"<profile version=\"524288\" storageformat=\"0\"") == xml, "profile element: version 8.0, storage format MP3");
    CHECKF(wcsstr(xml, L"name=\"Name &amp; &lt;&quot;quoted&quot;&gt; é\"") != NULL, "profile name is escaped");
    CHECKF(wcsstr(xml, L"description=\"It's a description\"") != NULL, "description");
    CHECKF(wcsstr(xml, L"<streamconfig majortype=\"{73647561-0000-0010-8000-00AA00389B71}\" streamnumber=\"5\" streamname=\"Audio &lt;1&gt;\" inputname=\"Audio409\" bitrate=\"64000\" bufferwindow=\"4500\"") != NULL,
            "audio stream element");
    CHECKF(wcsstr(xml, L"<waveformatex wFormatTag=\"1\" nChannels=\"2\" nSamplesPerSec=\"44100\" nAvgBytesPerSec=\"176400\" nBlockAlign=\"4\" wBitsPerSample=\"16\" codecdata=\"01020304AAFF\"/>") != NULL,
            "waveformatex element");
    CHECKF(wcsstr(xml, L"<wmmediatype subtype=\"{00000001-0000-0010-8000-00AA00389B71}\" bFixedSizeSamples=\"1\" bTemporalCompression=\"0\" lSampleSize=\"4\">") != NULL,
            "wmmediatype element");
    CHECKF(wcsstr(xml, L"<videoinfoheader") != NULL && wcsstr(xml, L"<bitmapinfoheader biSize=\"40\"") != NULL && wcsstr(xml, L"<rcsource") != NULL,
            "videoinfoheader elements");
    CHECKF(wcsstr(xml, L"<mutexconfig type=\"{73646976-0000-0010-8000-00AA00389B71}\">") != NULL
            && wcsstr(xml, L"<mutexstream streamnumber=\"5\"/>") != NULL, "mutex elements");
    CHECKF(wcsstr(xml, L"<bandwidthsharing type=\"{AF6060AA-5197-11D2-B6AF-00C04FD908E9}\" bitrate=\"200000\" buffer=\"3000\">") != NULL,
            "bandwidth sharing element");
    CHECKF(wcsstr(xml, L"<prioritystream streamnumber=\"2\" mandatory=\"1\"/>") != NULL, "priority records");

    hr = IWMProfileManager_LoadProfileByData(mgr, xml, &q);
    CHECKF(hr == S_OK && q, "LoadProfileByData(saved) (%08lx)", hr);
    if (!q) return;
    xml2 = save(q, &hr);
    CHECKF(xml2 && !wcscmp(xml, xml2), "saving the loaded profile gives the same text");
    free(xml2);
    IWMProfile_GetVersion(q, &version);
    CHECKF(version == WMT_VER_8_0, "version 8.0 (%#x)", version);
    IWMProfile_QueryInterface(q, &IID_IWMProfile3, (void **)&q3);
    IWMProfile3_GetStorageFormat(q3, &storage);
    CHECKF(storage == WMT_Storage_Format_MP3, "storage format MP3");
    s = get_str(gname, q);
    CHECKF(!wcscmp(s, L"Name & <\"quoted\"> é"), "name restored (%ls)", s);
    free(s);
    s = get_str(gdesc, q);
    CHECKF(!wcscmp(s, L"It's a description"), "description restored (%ls)", s);
    free(s);
    IWMProfile_GetStreamCount(q, &n);
    CHECKF(n == 3, "three streams (%lu)", n);
    for (i = 0; i < n && i < 3; i++)
    {
        IWMStreamConfig *c;
        IWMMediaProps *props;
        WM_MEDIA_TYPE *mt;
        DWORD bitrate, window, size = 0;
        WORD number;
        GUID type;
        BYTE *orig = NULL;

        IWMProfile_GetStream(q, i, &c);
        IWMStreamConfig_GetStreamNumber(c, &number);
        IWMStreamConfig_GetStreamType(c, &type);
        IWMStreamConfig_GetBitrate(c, &bitrate);
        IWMStreamConfig_GetBufferWindow(c, &window);
        CHECKF(number == streams[i].number && IsEqualGUID(&type, streams[i].type) && bitrate == streams[i].bitrate
                && window == streams[i].window, "stream %lu: number, type, bitrate, window (%u, %lu, %lx)", i, number, bitrate, window);
        s = get_str(gsname, c);
        CHECKF(!wcscmp(s, streams[i].name), "stream %lu name (%ls)", i, s);
        free(s);
        s = get_str(gcname, c);
        CHECKF(!wcscmp(s, streams[i].conn), "stream %lu connection name (%ls)", i, s);
        free(s);
        IWMStreamConfig_QueryInterface(c, &IID_IWMMediaProps, (void **)&props);
        IWMMediaProps_GetMediaType(props, NULL, &size);
        CHECKF(size == streams[i].mtsize, "stream %lu media type size %lu (%lu)", i, streams[i].mtsize, size);
        mt = malloc(size);
        IWMMediaProps_GetMediaType(props, mt, &size);
        if (i == 0) orig = audio_fmt; else if (i == 1) orig = video_fmt;
        if (orig)
            CHECKF(mt->cbFormat == streams[i].mtsize - sizeof(WM_MEDIA_TYPE) && !memcmp(mt->pbFormat, orig, mt->cbFormat),
                    "stream %lu format bytes identical", i);
        if (i == 0)
            CHECKF(IsEqualGUID(&mt->subtype, &WMMEDIASUBTYPE_PCM) && mt->bFixedSizeSamples && !mt->bTemporalCompression
                    && mt->lSampleSize == 4 && IsEqualGUID(&mt->formattype, &WMFORMAT_WaveFormatEx), "audio media type fields");
        if (i == 1)
            CHECKF(IsEqualGUID(&mt->subtype, &WMMEDIASUBTYPE_WVC1) && !mt->bFixedSizeSamples && mt->bTemporalCompression
                    && IsEqualGUID(&mt->formattype, &WMFORMAT_VideoInfo), "video media type fields");
        free(mt);
        IWMMediaProps_Release(props);
        IWMStreamConfig_Release(c);
    }
    IWMProfile_GetMutualExclusionCount(q, &n);
    CHECKF(n == 1, "one mutual exclusion");
    if (n)
    {
        IWMMutualExclusion *m;
        GUID type;
        IWMProfile_GetMutualExclusion(q, 0, &m);
        IWMMutualExclusion_GetType(m, &type);
        count = 4;
        IWMMutualExclusion_GetStreams(m, nums, &count);
        CHECKF(IsEqualGUID(&type, &WMMEDIATYPE_Video) && count == 2 && nums[0] == 5 && nums[1] == 2, "mutex type and streams 5, 2 (%u)", count);
        IWMMutualExclusion_Release(m);
    }
    IWMProfile3_GetBandwidthSharingCount(q3, &n);
    CHECKF(n == 1, "one bandwidth sharing");
    if (n)
    {
        IWMBandwidthSharing *b;
        DWORD bitrate, buffer;
        IWMProfile3_GetBandwidthSharing(q3, 0, &b);
        IWMBandwidthSharing_GetBandwidth(b, &bitrate, &buffer);
        count = 4;
        IWMBandwidthSharing_GetStreams(b, nums, &count);
        CHECKF(bitrate == 200000 && buffer == 3000 && count == 2 && nums[0] == 2 && nums[1] == 7, "sharing bandwidth and streams 2, 7");
        IWMBandwidthSharing_Release(b);
    }
    {
        IWMStreamPrioritization *pr;
        WM_STREAM_PRIORITY_RECORD recs[3];

        hr = IWMProfile3_GetStreamPrioritization(q3, &pr);
        CHECKF(hr == S_OK, "prioritization restored (%08lx)", hr);
        if (hr == S_OK)
        {
            count = 3;
            IWMStreamPrioritization_GetPriorityRecords(pr, recs, &count);
            CHECKF(count == 2 && recs[0].wStreamNumber == 2 && recs[0].fMandatory && recs[1].wStreamNumber == 5 && !recs[1].fMandatory,
                    "priority records restored");
            IWMStreamPrioritization_Release(pr);
        }
    }
    IWMProfile3_Release(q3);
    IWMProfile_Release(q);
    IWMProfile_Release(p);
    free(xml);
}

static void test_handwritten(void)
{
    static const struct { const WCHAR *xml; HRESULT hr; unsigned int streams; WMT_VERSION version; const char *what; } docs[] = {
        {L"<profile version=\"589824\" storageformat=\"1\" name=\"x\" description=\"\"/>", S_OK, 0, WMT_VER_9_0, "self-closing empty profile"},
        {L"<?xml version=\"1.0\"?>\n<!-- c --><PROFILE VERSION='458752'>\n <StreamConfig MajorType='{73647561-0000-0010-8000-00AA00389B71}' StreamNumber='3' Bitrate='1'/>\n</PROFILE>",
            S_OK, 1, WMT_VER_7_0, "XML header, comment, upper case names, single quotes"},
        {L"<profile><streamconfig majortype=\"{73647561-0000-0010-8000-00AA00389B71}\" streamnumber=\"1\"/></profile>", S_OK, 1, WMT_VER_9_0, "no version attribute"},
        {L"<profile version=\"589824\"><streamconfig majortype=\"{73647561-0000-0010-8000-00AA00389B71}\" streamnumber=\"1\"/>"
            L"<streamconfig majortype=\"{73647561-0000-0010-8000-00AA00389B71}\" streamnumber=\"2\"/><unknown a=\"b\"/></profile>", S_OK, 2, WMT_VER_9_0, "two streams, unknown element"},
        {L"", NS_E_INVALID_DATA, 0, 0, "empty text"},
        {L"garbage", NS_E_INVALID_DATA, 0, 0, "not XML"},
        {L"<other/>", NS_E_INVALID_DATA, 0, 0, "wrong root element"},
        {L"<profile version=\"589824\">", NS_E_INVALID_DATA, 0, 0, "unterminated profile"},
        {L"<profile version=\"327680\"/>", NS_E_INVALID_DATA, 0, 0, "version 5.0"},
        {L"<profile version=\"abc\"/>", NS_E_INVALID_DATA, 0, 0, "version not a number"},
        {L"<profile version=\"589824\" storageformat=\"7\"/>", NS_E_INVALID_DATA, 0, 0, "storage format 7"},
        {L"<profile><streamconfig majortype=\"{73647561-0000-0010-8000-00AA00389B71}\" streamnumber=\"0\"/></profile>", NS_E_INVALID_DATA, 0, 0, "stream number 0"},
        {L"<profile><streamconfig majortype=\"{73647561-0000-0010-8000-00AA00389B71}\" streamnumber=\"64\"/></profile>", NS_E_INVALID_DATA, 0, 0, "stream number 64"},
        {L"<profile><streamconfig majortype=\"{73647561-0000-0010-8000-00AA00389B7}\" streamnumber=\"1\"/></profile>", NS_E_INVALID_DATA, 0, 0, "short GUID"},
        {L"<profile><streamconfig streamnumber=\"1\"/></profile>", NS_E_INVALID_DATA, 0, 0, "no major type"},
        {L"<profile><streamconfig majortype=\"{73647561-0000-0010-8000-00AA00389B71}\" streamnumber=\"1\"/>"
            L"<streamconfig majortype=\"{73647561-0000-0010-8000-00AA00389B71}\" streamnumber=\"1\"/></profile>", NS_E_INVALID_DATA, 0, 0, "duplicate stream number"},
        {L"<profile><streamconfig majortype=\"{73647561-0000-0010-8000-00AA00389B71}\" streamnumber=\"1\"", NS_E_INVALID_DATA, 0, 0, "cut in an element"},
        {L"<profile><streamconfig majortype=\"{73647561-0000-0010-8000-00AA00389B71}\" streamnumber=\"1\"></profile>", NS_E_INVALID_DATA, 0, 0, "unclosed streamconfig"},
    };
    unsigned int i;

    for (i = 0; i < sizeof(docs) / sizeof(docs[0]); i++)
    {
        IWMProfile *p = (void *)1;
        HRESULT hr = IWMProfileManager_LoadProfileByData(mgr, docs[i].xml, &p);
        CHECKF(hr == docs[i].hr, "LoadProfileByData: %s = %08lx (got %08lx)", docs[i].what, (unsigned long)docs[i].hr, (unsigned long)hr);
        if (docs[i].hr == S_OK && SUCCEEDED(hr))
        {
            DWORD n = 99;
            WMT_VERSION v = 0;
            IWMProfile_GetStreamCount(p, &n);
            IWMProfile_GetVersion(p, &v);
            CHECKF(n == docs[i].streams && v == docs[i].version, "  %s: %u streams, version %#x (%lu, %#x)", docs[i].what, docs[i].streams, docs[i].version, n, v);
            IWMProfile_Release(p);
        }
        else if (docs[i].hr != S_OK)
            CHECKF(p == NULL, "  %s: output cleared", docs[i].what);
    }
}

static void test_args(void)
{
    IWMProfile *p = NULL, *e;
    DWORD len = 1;
    WCHAR buf[4];

    CHECK_HR(IWMProfileManager_LoadProfileByData(mgr, NULL, &p), E_INVALIDARG, "LoadProfileByData(NULL text)");
    CHECK_HR(IWMProfileManager_LoadProfileByData(mgr, L"<profile/>", NULL), E_INVALIDARG, "LoadProfileByData(NULL out)");
    IWMProfileManager_CreateEmptyProfile(mgr, WMT_VER_9_0, &e);
    CHECK_HR(IWMProfileManager_SaveProfile(mgr, NULL, buf, &len), E_INVALIDARG, "SaveProfile(NULL profile)");
    CHECK_HR(IWMProfileManager_SaveProfile(mgr, e, buf, NULL), E_INVALIDARG, "SaveProfile(NULL length)");
    len = 0;
    CHECK_HR(IWMProfileManager_SaveProfile(mgr, e, buf, &len), ASF_E_BUFFERTOOSMALL, "SaveProfile(buffer of 0)");
    IWMProfile_Release(e);
}

static void test_reader_profile(const char *sample)
{
    HRESULT (WINAPI *create)(IUnknown *, DWORD, IWMSyncReader **) = (void *)GetProcAddress(LoadLibraryW(L"wmvcore.dll"), "WMCreateSyncReader");
    IWMSyncReader *reader = NULL;
    IWMProfile *rp = NULL, *q = NULL;
    WCHAR file[MAX_PATH], *xml;
    DWORD n, n2;
    HRESULT hr;

    if (!sample || !create || !MultiByteToWideChar(CP_ACP, 0, sample, -1, file, MAX_PATH)) { printf("SKIP  no sample\n"); return; }
    if (FAILED(create(NULL, 0, &reader))) { printf("SKIP  no sync reader\n"); return; }
    hr = IWMSyncReader_Open(reader, file);
    if (FAILED(hr)) { printf("SKIP  the sample could not be opened here (%08lx)\n", hr); IWMSyncReader_Release(reader); return; }
    IWMSyncReader_QueryInterface(reader, &IID_IWMProfile, (void **)&rp);
    xml = save(rp, &hr);
    CHECKF(xml != NULL, "SaveProfile of a reader's profile (%08lx)", hr);
    if (xml)
    {
        hr = IWMProfileManager_LoadProfileByData(mgr, xml, &q);
        CHECKF(hr == S_OK, "LoadProfileByData of it (%08lx)", hr);
        if (q)
        {
            IWMProfile_GetStreamCount(rp, &n);
            IWMProfile_GetStreamCount(q, &n2);
            CHECKF(n == n2 && n > 0, "the same number of streams (%lu, %lu)", n, n2);
            IWMProfile_Release(q);
        }
        free(xml);
    }
    IWMProfile_Release(rp);
    IWMSyncReader_Release(reader);
}

int main(int argc, char **argv)
{
    HRESULT (WINAPI *create)(IWMProfileManager **);
    HRESULT hr;

    CoInitialize(NULL);
    create = (void *)GetProcAddress(LoadLibraryW(L"wmvcore.dll"), "WMCreateProfileManager");
    if (!create) { check(0, "wmvcore.dll exports WMCreateProfileManager"); goto out; }
    hr = create(&mgr);
    CHECKF(hr == S_OK && mgr, "WMCreateProfileManager (%08lx)", hr);
    if (!mgr) goto out;
    test_args();
    test_roundtrip();
    test_handwritten();
    test_reader_profile(argc > 1 ? argv[1] : NULL);
    IWMProfileManager_Release(mgr);
out:
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
