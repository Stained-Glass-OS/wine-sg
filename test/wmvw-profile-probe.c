/* wmvcore profile objects (patch 2863), run by test/wmvw-profile-gate.sh:
 * WMCreateProfileManager -> CreateEmptyProfile, IWMProfile/2/3 (name,
 * description, storage format, streams, mutual exclusions, bandwidth sharing,
 * stream prioritization, expected packet count), IWMStreamConfig, IWMMediaProps.
 * Everything is checked by value. Needs no file.
 *
 *   wmvw-profile-probe.exe */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <wmsdk.h>
#include <nserror.h>
#ifndef ASF_E_BUFFERTOOSMALL
#define ASF_E_BUFFERTOOSMALL ((HRESULT)0xc00d07d1)
#endif
#include <stdio.h>
#include <string.h>

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}
#define CHECKF(ok, ...) do { char _b[320]; snprintf(_b, sizeof(_b), __VA_ARGS__); check(ok, _b); } while (0)
#define CHECK_HR(hr, exp, what) do { HRESULT _h = (hr); CHECKF(_h == (HRESULT)(exp), "%s = %08lx (got %08lx)", what, (unsigned long)(exp), (unsigned long)_h); } while (0)

static const GUID WMFORMAT_WaveFormatEx = {0x05589f81, 0xc356, 0x11ce, {0xbf, 0x01, 0x00, 0xaa, 0x00, 0x55, 0x59, 0x5a}};
static HMODULE wmvcore;
static FARPROC proc(const char *name)
{
    FARPROC p;
    if (!wmvcore) wmvcore = LoadLibraryW(L"wmvcore.dll");
    p = wmvcore ? GetProcAddress(wmvcore, name) : NULL;
    if (!p) CHECKF(0, "wmvcore.dll exports %s", name);
    return p;
}

static void check_string(const char *what, HRESULT (*get)(void *, WCHAR *, DWORD *), void *obj, const WCHAR *expected)
{
    DWORD len = 0, need = wcslen(expected) + 1;
    WCHAR buf[64];
    HRESULT hr;

    hr = get(obj, NULL, &len);
    CHECKF(hr == S_OK && len == need, "%s length query = %lu (%08lx, %lu)", what, need, hr, len);
    len = need - 1;
    memset(buf, 0xcc, sizeof(buf));
    hr = get(obj, buf, &len);
    CHECKF(hr == ASF_E_BUFFERTOOSMALL && len == need, "%s short buffer = ASF_E_BUFFERTOOSMALL, %lu (%08lx, %lu)", what, need, hr, len);
    len = need;
    hr = get(obj, buf, &len);
    CHECKF(hr == S_OK && len == need && !wcscmp(buf, expected), "%s = %ls (%08lx, %ls)", what, expected, hr, buf);
    hr = get(obj, buf, NULL);
    CHECKF(hr == E_INVALIDARG, "%s NULL length = E_INVALIDARG (%08lx)", what, hr);
}

static HRESULT get_name(void *p, WCHAR *b, DWORD *l) { return IWMProfile_GetName((IWMProfile *)p, b, l); }
static HRESULT get_desc(void *p, WCHAR *b, DWORD *l) { return IWMProfile_GetDescription((IWMProfile *)p, b, l); }
static HRESULT get_sname(void *p, WCHAR *b, DWORD *l)
{
    WORD n = l ? *l : 0; HRESULT hr = IWMStreamConfig_GetStreamName((IWMStreamConfig *)p, b, l ? &n : NULL); if (l) *l = n; return hr;
}
static HRESULT get_cname(void *p, WCHAR *b, DWORD *l)
{
    WORD n = l ? *l : 0; HRESULT hr = IWMStreamConfig_GetConnectionName((IWMStreamConfig *)p, b, l ? &n : NULL); if (l) *l = n; return hr;
}

static DWORD stream_count(IWMProfile *p)
{
    DWORD n = 0xdead;
    IWMProfile_GetStreamCount(p, &n);
    return n;
}

static void test_empty_profile(IWMProfileManager *mgr)
{
    static const WMT_VERSION versions[] = {WMT_VER_4_0, WMT_VER_7_0, WMT_VER_8_0, WMT_VER_9_0};
    IWMProfile3 *p3;
    IWMProfile2 *p2;
    IWMProfile *p;
    WMT_STORAGE_FORMAT storage;
    IWMStreamPrioritization *prio;
    QWORD packets;
    unsigned int i;
    WMT_VERSION v;
    DWORD n;
    GUID id;
    HRESULT hr;

    for (i = 0; i < 4; i++)
    {
        p = NULL;
        hr = IWMProfileManager_CreateEmptyProfile(mgr, versions[i], &p);
        CHECKF(hr == S_OK && p, "CreateEmptyProfile(%#x) (%08lx)", versions[i], hr);
        if (!p) return;
        v = 0;
        hr = IWMProfile_GetVersion(p, &v);
        CHECKF(hr == S_OK && v == versions[i], "GetVersion = %#x (%#x)", versions[i], v);
        IWMProfile_Release(p);
    }
    p = (void *)1;
    CHECK_HR(IWMProfileManager_CreateEmptyProfile(mgr, 0x50000, &p), E_INVALIDARG, "CreateEmptyProfile(5.0)");
    CHECKF(p == NULL, "failed CreateEmptyProfile clears the output");

    hr = IWMProfileManager_CreateEmptyProfile(mgr, WMT_VER_9_0, &p);
    if (FAILED(hr)) return;
    CHECK_HR(IWMProfile_QueryInterface(p, &IID_IWMProfile2, (void **)&p2), S_OK, "QI IWMProfile2");
    CHECK_HR(IWMProfile_QueryInterface(p, &IID_IWMProfile3, (void **)&p3), S_OK, "QI IWMProfile3");
    IWMProfile2_Release(p2);
    check_string("empty name", get_name, p, L"");
    check_string("empty description", get_desc, p, L"");
    CHECK_HR(IWMProfile_SetName(p, NULL), E_INVALIDARG, "SetName(NULL)");
    CHECK_HR(IWMProfile_SetName(p, L"My profile"), S_OK, "SetName");
    check_string("name", get_name, p, L"My profile");
    CHECK_HR(IWMProfile_SetDescription(p, L"Test description"), S_OK, "SetDescription");
    check_string("description", get_desc, p, L"Test description");
    CHECK_HR(IWMProfile_SetDescription(p, NULL), E_INVALIDARG, "SetDescription(NULL)");

    CHECKF(stream_count(p) == 0, "empty profile has no streams");
    CHECK_HR(IWMProfile_GetStreamCount(p, NULL), E_INVALIDARG, "GetStreamCount(NULL)");
    n = 7;
    CHECK_HR(IWMProfile_GetMutualExclusionCount(p, &n), S_OK, "GetMutualExclusionCount");
    CHECKF(n == 0, "no mutual exclusions");
    CHECK_HR(IWMProfile3_GetBandwidthSharingCount(p3, &n), S_OK, "GetBandwidthSharingCount");
    CHECKF(n == 0, "no bandwidth sharing");
    prio = (void *)1;
    CHECK_HR(IWMProfile3_GetStreamPrioritization(p3, &prio), S_FALSE, "GetStreamPrioritization (none)");
    CHECKF(prio == NULL, "no prioritization object");
    CHECK_HR(IWMProfile3_RemoveStreamPrioritization(p3), S_OK, "RemoveStreamPrioritization (none)");
    memset(&id, 0x55, sizeof(id));
    CHECK_HR(IWMProfile3_GetProfileID(p3, &id), S_OK, "GetProfileID");
    CHECKF(IsEqualGUID(&id, &GUID_NULL), "profile id is GUID_NULL");
    CHECK_HR(IWMProfile3_GetProfileID(p3, NULL), E_INVALIDARG, "GetProfileID(NULL)");
    CHECK_HR(IWMProfile3_GetStorageFormat(p3, &storage), S_OK, "GetStorageFormat");
    CHECKF(storage == WMT_Storage_Format_V1, "default storage format V1 (%d)", storage);
    CHECK_HR(IWMProfile3_SetStorageFormat(p3, 5), E_INVALIDARG, "SetStorageFormat(5)");
    CHECK_HR(IWMProfile3_SetStorageFormat(p3, WMT_Storage_Format_MP3), S_OK, "SetStorageFormat(MP3)");
    IWMProfile3_GetStorageFormat(p3, &storage);
    CHECKF(storage == WMT_Storage_Format_MP3, "storage format MP3 (%d)", storage);
    packets = 99;
    CHECK_HR(IWMProfile3_GetExpectedPacketCount(p3, 100000000, &packets), S_OK, "GetExpectedPacketCount (empty)");
    CHECKF(packets == 0, "no packets for an empty profile (%I64u)", packets);
    CHECK_HR(IWMProfile3_GetExpectedPacketCount(p3, 100, NULL), E_INVALIDARG, "GetExpectedPacketCount(NULL)");

    IWMProfile3_Release(p3);
    IWMProfile_Release(p);
}

static void make_wfx(WM_MEDIA_TYPE *mt, WAVEFORMATEX *wfx)
{
    memset(wfx, 0, sizeof(*wfx));
    wfx->wFormatTag = WAVE_FORMAT_PCM;
    wfx->nChannels = 2;
    wfx->nSamplesPerSec = 44100;
    wfx->wBitsPerSample = 16;
    wfx->nBlockAlign = 4;
    wfx->nAvgBytesPerSec = 44100 * 4;
    memset(mt, 0, sizeof(*mt));
    mt->majortype = WMMEDIATYPE_Audio;
    mt->subtype = WMMEDIASUBTYPE_PCM;
    mt->bFixedSizeSamples = TRUE;
    mt->lSampleSize = 4;
    mt->formattype = WMFORMAT_WaveFormatEx;
    mt->cbFormat = sizeof(*wfx);
    mt->pbFormat = (BYTE *)wfx;
}

static void test_stream_configs(IWMProfileManager *mgr)
{
    IWMProfile *p;
    IWMStreamConfig *c1, *c2, *c3, *got;
    IWMMediaProps *props;
    WM_MEDIA_TYPE mt, *out;
    WAVEFORMATEX wfx;
    DWORD bitrate, window, size;
    WORD number;
    GUID type;
    HRESULT hr;
    BYTE buf[256];

    if (FAILED(IWMProfileManager_CreateEmptyProfile(mgr, WMT_VER_9_0, &p))) return;

    hr = IWMProfile_CreateNewStream(p, &WMMEDIATYPE_Audio, &c1);
    CHECKF(hr == S_OK && c1, "CreateNewStream(audio) (%08lx)", hr);
    if (!c1) return;
    CHECKF(stream_count(p) == 0, "a new stream is not in the profile yet");
    CHECK_HR(IWMProfile_CreateNewStream(p, NULL, &c2), E_INVALIDARG, "CreateNewStream(NULL type)");
    CHECK_HR(IWMProfile_CreateNewStream(p, &GUID_NULL, &c2), E_INVALIDARG, "CreateNewStream(GUID_NULL)");
    CHECK_HR(IWMProfile_CreateNewStream(p, &WMMEDIATYPE_Audio, NULL), E_INVALIDARG, "CreateNewStream(NULL out)");

    number = 0;
    CHECK_HR(IWMStreamConfig_GetStreamNumber(c1, &number), S_OK, "GetStreamNumber");
    CHECKF(number == 1, "first new stream is number 1 (%u)", number);
    CHECK_HR(IWMStreamConfig_GetStreamType(c1, &type), S_OK, "GetStreamType");
    CHECKF(IsEqualGUID(&type, &WMMEDIATYPE_Audio), "stream type audio");
    bitrate = 77; window = 77;
    CHECK_HR(IWMStreamConfig_GetBitrate(c1, &bitrate), S_OK, "GetBitrate");
    CHECK_HR(IWMStreamConfig_GetBufferWindow(c1, &window), S_OK, "GetBufferWindow");
    CHECKF(bitrate == 0 && window == 0xffffffff, "defaults: bitrate 0, window -1 (%lu, %lx)", bitrate, window);
    check_string("empty stream name", get_sname, c1, L"");
    check_string("empty connection name", get_cname, c1, L"");
    CHECK_HR(IWMStreamConfig_SetStreamNumber(c1, 0), E_INVALIDARG, "SetStreamNumber(0)");
    CHECK_HR(IWMStreamConfig_SetStreamNumber(c1, 64), E_INVALIDARG, "SetStreamNumber(64)");
    CHECK_HR(IWMStreamConfig_SetStreamNumber(c1, 63), S_OK, "SetStreamNumber(63)");
    CHECK_HR(IWMStreamConfig_SetStreamNumber(c1, 5), S_OK, "SetStreamNumber(5)");
    CHECK_HR(IWMStreamConfig_SetStreamName(c1, L"Audio stream"), S_OK, "SetStreamName");
    check_string("stream name", get_sname, c1, L"Audio stream");
    CHECK_HR(IWMStreamConfig_SetConnectionName(c1, L"Audio409"), S_OK, "SetConnectionName");
    check_string("connection name", get_cname, c1, L"Audio409");
    CHECK_HR(IWMStreamConfig_SetStreamName(c1, NULL), E_INVALIDARG, "SetStreamName(NULL)");
    CHECK_HR(IWMStreamConfig_SetBitrate(c1, 64000), S_OK, "SetBitrate");
    CHECK_HR(IWMStreamConfig_SetBufferWindow(c1, 3000), S_OK, "SetBufferWindow");
    CHECK_HR(IWMStreamConfig_GetBitrate(c1, NULL), E_INVALIDARG, "GetBitrate(NULL)");

    /* media props */
    CHECK_HR(IWMStreamConfig_QueryInterface(c1, &IID_IWMMediaProps, (void **)&props), S_OK, "QI IWMMediaProps");
    CHECK_HR(IWMMediaProps_GetType(props, &type), S_OK, "props GetType");
    CHECKF(IsEqualGUID(&type, &WMMEDIATYPE_Audio), "props type audio");
    size = 0;
    CHECK_HR(IWMMediaProps_GetMediaType(props, NULL, &size), S_OK, "GetMediaType size (unset)");
    CHECKF(size == sizeof(WM_MEDIA_TYPE), "unset media type is the bare structure (%lu)", size);
    memset(buf, 0xcc, sizeof(buf));
    out = (WM_MEDIA_TYPE *)buf;
    CHECK_HR(IWMMediaProps_GetMediaType(props, out, &size), S_OK, "GetMediaType (unset)");
    CHECKF(IsEqualGUID(&out->majortype, &WMMEDIATYPE_Audio) && IsEqualGUID(&out->subtype, &GUID_NULL) && !out->cbFormat,
            "unset media type holds only the major type");
    make_wfx(&mt, &wfx);
    mt.majortype = WMMEDIATYPE_Video;
    CHECK_HR(IWMMediaProps_SetMediaType(props, &mt), E_INVALIDARG, "SetMediaType(wrong major type)");
    CHECK_HR(IWMMediaProps_SetMediaType(props, NULL), E_INVALIDARG, "SetMediaType(NULL)");
    mt.majortype = WMMEDIATYPE_Audio;
    CHECK_HR(IWMMediaProps_SetMediaType(props, &mt), S_OK, "SetMediaType(PCM)");
    size = 0;
    CHECK_HR(IWMMediaProps_GetMediaType(props, NULL, &size), S_OK, "GetMediaType size");
    CHECKF(size == sizeof(WM_MEDIA_TYPE) + sizeof(WAVEFORMATEX), "size = structure + format (%lu)", size);
    size--;
    CHECK_HR(IWMMediaProps_GetMediaType(props, out, &size), ASF_E_BUFFERTOOSMALL, "GetMediaType short buffer");
    CHECKF(size == sizeof(WM_MEDIA_TYPE) + sizeof(WAVEFORMATEX), "short buffer reports the size (%lu)", size);
    memset(buf, 0xcc, sizeof(buf));
    size = sizeof(WM_MEDIA_TYPE) + sizeof(WAVEFORMATEX);
    CHECK_HR(IWMMediaProps_GetMediaType(props, out, &size), S_OK, "GetMediaType");
    CHECKF(IsEqualGUID(&out->subtype, &WMMEDIASUBTYPE_PCM) && out->cbFormat == sizeof(WAVEFORMATEX)
            && out->pbFormat == (BYTE *)(out + 1) && ((WAVEFORMATEX *)out->pbFormat)->nSamplesPerSec == 44100
            && out->bFixedSizeSamples && out->lSampleSize == 4, "media type round trip");
    IWMMediaProps_Release(props);

    /* a copy is not the profile's stream: AddStream takes it */
    CHECK_HR(IWMProfile_AddStream(p, NULL), E_INVALIDARG, "AddStream(NULL)");
    CHECK_HR(IWMProfile_AddStream(p, c1), S_OK, "AddStream(5)");
    CHECKF(stream_count(p) == 1, "one stream");
    CHECK_HR(IWMProfile_AddStream(p, c1), NS_E_INVALID_STREAM, "AddStream(5) again");
    CHECKF(stream_count(p) == 1, "still one stream");
    got = (void *)1;
    CHECK_HR(IWMProfile_GetStream(p, 1, &got), E_INVALIDARG, "GetStream(1)");
    CHECK_HR(IWMProfile_GetStream(p, 0, &got), S_OK, "GetStream(0)");
    number = 0; bitrate = 0; window = 0;
    IWMStreamConfig_GetStreamNumber(got, &number);
    IWMStreamConfig_GetBitrate(got, &bitrate);
    IWMStreamConfig_GetBufferWindow(got, &window);
    CHECKF(number == 5 && bitrate == 64000 && window == 3000, "stream copy: 5, 64000, 3000 (%u, %lu, %lu)", number, bitrate, window);
    check_string("added stream name", get_sname, got, L"Audio stream");
    check_string("added connection name", get_cname, got, L"Audio409");
    IWMStreamConfig_QueryInterface(got, &IID_IWMMediaProps, (void **)&props);
    size = sizeof(buf);
    CHECK_HR(IWMMediaProps_GetMediaType(props, (WM_MEDIA_TYPE *)buf, &size), S_OK, "added stream GetMediaType");
    CHECKF(((WM_MEDIA_TYPE *)buf)->cbFormat == sizeof(WAVEFORMATEX), "added stream keeps the media type");
    IWMMediaProps_Release(props);

    /* ReconfigStream copies the changes back */
    IWMStreamConfig_SetBitrate(got, 128000);
    IWMStreamConfig_Release(got);
    CHECK_HR(IWMProfile_GetStreamByNumber(p, 5, &got), S_OK, "GetStreamByNumber(5)");
    IWMStreamConfig_GetBitrate(got, &bitrate);
    CHECKF(bitrate == 64000, "an edited copy does not change the profile (%lu)", bitrate);
    IWMStreamConfig_SetBitrate(got, 128000);
    IWMStreamConfig_SetStreamName(got, L"Renamed");
    CHECK_HR(IWMProfile_ReconfigStream(p, got), S_OK, "ReconfigStream");
    IWMStreamConfig_Release(got);
    IWMProfile_GetStreamByNumber(p, 5, &got);
    IWMStreamConfig_GetBitrate(got, &bitrate);
    CHECKF(bitrate == 128000, "ReconfigStream stores the bitrate (%lu)", bitrate);
    check_string("reconfigured name", get_sname, got, L"Renamed");
    IWMStreamConfig_Release(got);
    CHECK_HR(IWMProfile_ReconfigStream(p, NULL), E_INVALIDARG, "ReconfigStream(NULL)");

    /* numbers: the lowest unused */
    CHECK_HR(IWMProfile_GetStreamByNumber(p, 6, &got), NS_E_NO_STREAM, "GetStreamByNumber(6)");
    CHECK_HR(IWMProfile_GetStreamByNumber(p, 0, &got), NS_E_NO_STREAM, "GetStreamByNumber(0)");
    CHECK_HR(IWMProfile_CreateNewStream(p, &WMMEDIATYPE_Video, &c2), S_OK, "CreateNewStream(video)");
    IWMStreamConfig_GetStreamNumber(c2, &number);
    CHECKF(number == 1, "new stream takes the lowest free number (%u)", number);
    CHECK_HR(IWMProfile_AddStream(p, c2), S_OK, "AddStream(1)");
    CHECK_HR(IWMProfile_CreateNewStream(p, &WMMEDIATYPE_Video, &c3), S_OK, "CreateNewStream(video) #2");
    IWMStreamConfig_GetStreamNumber(c3, &number);
    CHECKF(number == 2, "next new stream is 2 (%u)", number);
    CHECK_HR(IWMProfile_AddStream(p, c3), S_OK, "AddStream(2)");
    CHECKF(stream_count(p) == 3, "three streams");
    IWMProfile_GetStream(p, 1, &got);
    IWMStreamConfig_GetStreamNumber(got, &number);
    CHECKF(number == 1, "streams keep their order of addition (%u)", number);
    IWMStreamConfig_Release(got);

    /* expected packet count: 40000+40000+... bytes / 3200 */
    {
        IWMProfile3 *p3;
        QWORD packets = 0;
        IWMStreamConfig *cc;

        IWMProfile_GetStreamByNumber(p, 1, &cc);
        IWMStreamConfig_SetBitrate(cc, 192000);
        IWMProfile_ReconfigStream(p, cc);
        IWMStreamConfig_Release(cc);
        IWMProfile_GetStreamByNumber(p, 2, &cc);
        IWMStreamConfig_SetBitrate(cc, 128000);
        IWMProfile_ReconfigStream(p, cc);
        IWMStreamConfig_Release(cc);
        IWMProfile_QueryInterface(p, &IID_IWMProfile3, (void **)&p3);
        /* (192000 + 128000 + 128000) / 8 = 56000 bytes per second, 10 s */
        CHECK_HR(IWMProfile3_GetExpectedPacketCount(p3, 100000000, &packets), S_OK, "GetExpectedPacketCount");
        CHECKF(packets == 175, "expected packets for 10 s = 175 (%I64u)", packets);
        IWMProfile3_Release(p3);
    }

    /* removal */
    CHECK_HR(IWMProfile_RemoveStreamByNumber(p, 5), S_OK, "RemoveStreamByNumber(5)");
    CHECKF(stream_count(p) == 2, "two streams left");
    CHECK_HR(IWMProfile_RemoveStreamByNumber(p, 5), NS_E_NO_STREAM, "RemoveStreamByNumber(5) again");
    CHECK_HR(IWMProfile_RemoveStream(p, NULL), E_INVALIDARG, "RemoveStream(NULL)");
    CHECK_HR(IWMProfile_RemoveStream(p, c2), S_OK, "RemoveStream(config of 1)");
    CHECKF(stream_count(p) == 1, "one stream left");
    CHECK_HR(IWMProfile_GetStreamByNumber(p, 1, &got), NS_E_NO_STREAM, "removed stream is gone");
    IWMProfile_GetStream(p, 0, &got);
    IWMStreamConfig_GetStreamNumber(got, &number);
    CHECKF(number == 2, "the remaining stream is 2 (%u)", number);
    IWMStreamConfig_Release(got);

    IWMStreamConfig_Release(c1);
    IWMStreamConfig_Release(c2);
    IWMStreamConfig_Release(c3);
    IWMProfile_Release(p);
}

static void test_lists(IWMProfileManager *mgr)
{
    static const GUID mutex_unknown = {0xd6e22a03, 0x35da, 0x11d1, {0x90, 0x34, 0x00, 0xa0, 0xc9, 0x03, 0x49, 0xbe}};
    static const GUID sharing_exclusive = {0xaf6060aa, 0x5197, 0x11d2, {0xb6, 0xaf, 0x00, 0xc0, 0x4f, 0xd9, 0x08, 0xe9}};
    IWMMutualExclusion *mutex, *m2;
    IWMBandwidthSharing *sharing, *sharing2;
    IWMStreamPrioritization *prio, *prio2;
    IWMStreamConfig *c;
    IWMProfile3 *p3;
    IWMProfile *p;
    WM_STREAM_PRIORITY_RECORD records[3], out[3];
    WORD numbers[4], count, n;
    DWORD bitrate, buffer, cnt;
    ULONG ref;
    GUID type;
    HRESULT hr;

    if (FAILED(IWMProfileManager_CreateEmptyProfile(mgr, WMT_VER_8_0, &p))) return;
    IWMProfile_QueryInterface(p, &IID_IWMProfile3, (void **)&p3);
    for (n = 0; n < 3; n++)
    {
        IWMProfile_CreateNewStream(p, n == 2 ? &WMMEDIATYPE_Video : &WMMEDIATYPE_Audio, &c);
        IWMProfile_AddStream(p, c);
        IWMStreamConfig_Release(c);
    }

    CHECK_HR(IWMProfile_CreateNewMutualExclusion(p, &mutex), S_OK, "CreateNewMutualExclusion");
    CHECK_HR(IWMProfile_CreateNewMutualExclusion(p, NULL), E_INVALIDARG, "CreateNewMutualExclusion(NULL)");
    CHECKF(mutex != NULL, "mutual exclusion created");
    if (!mutex) return;
    CHECK_HR(IWMMutualExclusion_GetType(mutex, &type), S_OK, "mutex GetType");
    CHECKF(IsEqualGUID(&type, &mutex_unknown), "default mutex type is WMMUTEX_Unknown");
    CHECK_HR(IWMMutualExclusion_SetType(mutex, &WMMEDIATYPE_Audio), S_OK, "mutex SetType");
    IWMMutualExclusion_GetType(mutex, &type);
    CHECKF(IsEqualGUID(&type, &WMMEDIATYPE_Audio), "mutex type stored");
    count = 9;
    CHECK_HR(IWMMutualExclusion_GetStreams(mutex, NULL, &count), S_OK, "mutex GetStreams count");
    CHECKF(count == 0, "empty list");
    CHECK_HR(IWMMutualExclusion_AddStream(mutex, 0), E_INVALIDARG, "mutex AddStream(0)");
    CHECK_HR(IWMMutualExclusion_AddStream(mutex, 64), E_INVALIDARG, "mutex AddStream(64)");
    CHECK_HR(IWMMutualExclusion_AddStream(mutex, 1), S_OK, "mutex AddStream(1)");
    CHECK_HR(IWMMutualExclusion_AddStream(mutex, 2), S_OK, "mutex AddStream(2)");
    CHECK_HR(IWMMutualExclusion_AddStream(mutex, 1), NS_E_INVALID_STREAM, "mutex AddStream(1) again");
    count = 1;
    CHECK_HR(IWMMutualExclusion_GetStreams(mutex, numbers, &count), ASF_E_BUFFERTOOSMALL, "mutex GetStreams short");
    CHECKF(count == 2, "short list reports 2 (%u)", count);
    count = 4;
    CHECK_HR(IWMMutualExclusion_GetStreams(mutex, numbers, &count), S_OK, "mutex GetStreams");
    CHECKF(count == 2 && numbers[0] == 1 && numbers[1] == 2, "streams 1, 2 (%u: %u %u)", count, numbers[0], numbers[1]);
    CHECK_HR(IWMMutualExclusion_RemoveStream(mutex, 9), NS_E_INVALID_STREAM, "mutex RemoveStream(9)");
    CHECK_HR(IWMProfile_AddMutualExclusion(p, mutex), S_OK, "AddMutualExclusion");
    CHECK_HR(IWMProfile_AddMutualExclusion(p, NULL), E_INVALIDARG, "AddMutualExclusion(NULL)");
    IWMProfile_GetMutualExclusionCount(p, &cnt);
    CHECKF(cnt == 1, "one mutual exclusion");
    CHECK_HR(IWMProfile_GetMutualExclusion(p, 1, &m2), E_INVALIDARG, "GetMutualExclusion(1)");
    CHECK_HR(IWMProfile_GetMutualExclusion(p, 0, &m2), S_OK, "GetMutualExclusion(0)");
    CHECKF(m2 == mutex, "the profile hands out the added object");
    IWMMutualExclusion_Release(m2);
    /* removing a stream from the profile removes it from the lists */
    CHECK_HR(IWMProfile_RemoveStreamByNumber(p, 1), S_OK, "RemoveStreamByNumber(1)");
    count = 4;
    IWMMutualExclusion_GetStreams(mutex, numbers, &count);
    CHECKF(count == 1 && numbers[0] == 2, "profile stream removal updates the mutex (%u)", count);
    CHECK_HR(IWMProfile_RemoveMutualExclusion(p, NULL), E_INVALIDARG, "RemoveMutualExclusion(NULL)");
    IWMProfile_CreateNewMutualExclusion(p, &m2);
    CHECK_HR(IWMProfile_RemoveMutualExclusion(p, m2), E_INVALIDARG, "RemoveMutualExclusion(foreign)");
    IWMMutualExclusion_Release(m2);
    ref = IWMMutualExclusion_AddRef(mutex);
    IWMMutualExclusion_Release(mutex);
    CHECKF(ref == 3, "the profile holds a reference (%lu)", ref);
    CHECK_HR(IWMProfile_RemoveMutualExclusion(p, mutex), S_OK, "RemoveMutualExclusion");
    ref = IWMMutualExclusion_AddRef(mutex);
    IWMMutualExclusion_Release(mutex);
    CHECKF(ref == 2, "the reference is released (%lu)", ref);
    IWMMutualExclusion_Release(mutex);

    /* bandwidth sharing */
    CHECK_HR(IWMProfile3_CreateNewBandwidthSharing(p3, &sharing), S_OK, "CreateNewBandwidthSharing");
    IWMBandwidthSharing_GetType(sharing, &type);
    CHECKF(IsEqualGUID(&type, &sharing_exclusive), "default sharing type is exclusive");
    bitrate = buffer = 77;
    CHECK_HR(IWMBandwidthSharing_GetBandwidth(sharing, &bitrate, &buffer), S_OK, "GetBandwidth");
    CHECKF(!bitrate && !buffer, "default bandwidth is 0");
    CHECK_HR(IWMBandwidthSharing_SetBandwidth(sharing, 100000, 5000), S_OK, "SetBandwidth");
    IWMBandwidthSharing_GetBandwidth(sharing, &bitrate, &buffer);
    CHECKF(bitrate == 100000 && buffer == 5000, "bandwidth stored (%lu, %lu)", bitrate, buffer);
    CHECK_HR(IWMBandwidthSharing_GetBandwidth(sharing, NULL, &buffer), E_INVALIDARG, "GetBandwidth(NULL)");
    IWMBandwidthSharing_AddStream(sharing, 2);
    IWMBandwidthSharing_AddStream(sharing, 3);
    CHECK_HR(IWMBandwidthSharing_AddStream(sharing, 3), NS_E_INVALID_STREAM, "sharing AddStream(3) again");
    CHECK_HR(IWMProfile3_AddBandwidthSharing(p3, sharing), S_OK, "AddBandwidthSharing");
    IWMProfile3_GetBandwidthSharingCount(p3, &cnt);
    CHECKF(cnt == 1, "one bandwidth sharing");
    CHECK_HR(IWMProfile3_GetBandwidthSharing(p3, 1, &sharing2), E_INVALIDARG, "GetBandwidthSharing(1)");
    CHECK_HR(IWMProfile_RemoveStreamByNumber(p, 3), S_OK, "RemoveStreamByNumber(3)");
    count = 4;
    IWMBandwidthSharing_GetStreams(sharing, numbers, &count);
    CHECKF(count == 1 && numbers[0] == 2, "profile stream removal updates the sharing (%u)", count);
    CHECK_HR(IWMProfile3_RemoveBandwidthSharing(p3, sharing), S_OK, "RemoveBandwidthSharing");
    CHECK_HR(IWMProfile3_RemoveBandwidthSharing(p3, sharing), E_INVALIDARG, "RemoveBandwidthSharing again");
    IWMBandwidthSharing_Release(sharing);

    /* stream prioritization */
    CHECK_HR(IWMProfile3_CreateNewStreamPrioritization(p3, &prio), S_OK, "CreateNewStreamPrioritization");
    count = 9;
    IWMStreamPrioritization_GetPriorityRecords(prio, NULL, &count);
    CHECKF(count == 0, "no priority records");
    records[0].wStreamNumber = 2; records[0].fMandatory = TRUE;
    records[1].wStreamNumber = 5; records[1].fMandatory = FALSE;
    records[2].wStreamNumber = 2; records[2].fMandatory = FALSE;
    CHECK_HR(IWMStreamPrioritization_SetPriorityRecords(prio, records, 3), E_INVALIDARG, "SetPriorityRecords(duplicate)");
    records[2].wStreamNumber = 0;
    CHECK_HR(IWMStreamPrioritization_SetPriorityRecords(prio, records, 3), E_INVALIDARG, "SetPriorityRecords(stream 0)");
    CHECK_HR(IWMStreamPrioritization_SetPriorityRecords(prio, records, 2), S_OK, "SetPriorityRecords(2)");
    count = 1;
    CHECK_HR(IWMStreamPrioritization_GetPriorityRecords(prio, out, &count), ASF_E_BUFFERTOOSMALL, "GetPriorityRecords short");
    count = 3;
    CHECK_HR(IWMStreamPrioritization_GetPriorityRecords(prio, out, &count), S_OK, "GetPriorityRecords");
    CHECKF(count == 2 && out[0].wStreamNumber == 2 && out[0].fMandatory && out[1].wStreamNumber == 5 && !out[1].fMandatory,
            "records stored in order");
    CHECK_HR(IWMStreamPrioritization_SetPriorityRecords(prio, records, 1), S_OK, "SetPriorityRecords(1)");
    count = 3;
    IWMStreamPrioritization_GetPriorityRecords(prio, out, &count);
    CHECKF(count == 1, "records replaced (%u)", count);
    CHECK_HR(IWMProfile3_SetStreamPrioritization(p3, NULL), E_INVALIDARG, "SetStreamPrioritization(NULL)");
    CHECK_HR(IWMProfile3_SetStreamPrioritization(p3, prio), S_OK, "SetStreamPrioritization");
    prio2 = NULL;
    hr = IWMProfile3_GetStreamPrioritization(p3, &prio2);
    CHECKF(hr == S_OK && prio2 == prio, "GetStreamPrioritization hands out the set object (%08lx)", hr);
    if (prio2) IWMStreamPrioritization_Release(prio2);
    CHECK_HR(IWMProfile3_RemoveStreamPrioritization(p3), S_OK, "RemoveStreamPrioritization");
    CHECK_HR(IWMProfile3_GetStreamPrioritization(p3, &prio2), S_FALSE, "GetStreamPrioritization after removal");
    IWMStreamPrioritization_Release(prio);

    IWMProfile3_Release(p3);
    IWMProfile_Release(p);
}

int main(int argc, char **argv)
{
    HRESULT (WINAPI *create)(IWMProfileManager **) = NULL;
    IWMProfileManager *mgr = NULL;
    HRESULT hr;

    CoInitialize(NULL);
    create = (void *)proc("WMCreateProfileManager");
    if (create)
    {
        hr = create(&mgr);
        CHECKF(hr == S_OK && mgr, "WMCreateProfileManager (%08lx)", hr);
    }
    if (mgr)
    {
        test_empty_profile(mgr);
        test_stream_configs(mgr);
        test_lists(mgr);
        IWMProfileManager_Release(mgr);
    }
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
