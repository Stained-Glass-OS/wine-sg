/* wmvcore system profiles, writer statistics/time and input settings
 * (patches 2867-2869), run by test/wmvw-sysprof-gate.sh. Table-driven.
 *
 *   wmvw-sysprof-probe.exe */
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
#define E_INVALIDPROFILE ((HRESULT)0xc00d0bc6)

static HRESULT (WINAPI *create_mgr)(IWMProfileManager **);
static HRESULT (WINAPI *create_writer)(IUnknown *, IWMWriter **);
static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}
#define CHECKF(ok, ...) do { char _b[400]; snprintf(_b, sizeof(_b), __VA_ARGS__); check(ok, _b); } while (0)
#define CHECK(ok, what) check(ok, what)
#define CHECK_HR(hr, exp, what) do { HRESULT _h = (hr); CHECKF(_h == (HRESULT)(exp), "%s = %08lx (got %08lx)", what, (unsigned long)(exp), (unsigned long)_h); } while (0)

static const struct { DWORD bitrate; WORD channels; } expected[] = {
    {28800, 1}, {28800, 2}, {32000, 2}, {48000, 2}, {64000, 2}, {96000, 2}, {128000, 2},
};

static void test_system_profiles(IWMProfileManager *mgr)
{
    IWMProfileManager2 *mgr2;
    static const struct { WMT_VERSION version; DWORD count; } counts[] = {
        {WMT_VER_8_0, 7}, {WMT_VER_9_0, 0}, {WMT_VER_7_0, 0}, {WMT_VER_4_0, 0},
    };
    GUID id = {0x11111111, 0x2222, 0x3333, {1, 2, 3, 4, 5, 6, 7, 8}};
    IWMProfile *profile;
    DWORD count, i, n;
    HRESULT hr;

    CHECK_HR(IWMProfileManager_QueryInterface(mgr, &IID_IWMProfileManager2, (void **)&mgr2), S_OK, "QI IWMProfileManager2");
    CHECK_HR(IWMProfileManager_GetSystemProfileCount(mgr, NULL), E_INVALIDARG, "GetSystemProfileCount(NULL)");
    profile = (IWMProfile *)1;
    CHECK_HR(IWMProfileManager_LoadSystemProfile(mgr, 0, NULL), E_INVALIDARG, "LoadSystemProfile(NULL)");
    CHECK_HR(IWMProfileManager_LoadProfileByID(mgr, NULL, &profile), E_INVALIDARG, "LoadProfileByID(NULL id)");
    CHECK_HR(IWMProfileManager_LoadProfileByID(mgr, &id, &profile), E_INVALIDPROFILE, "LoadProfileByID(unknown)");
    CHECK(profile == NULL, "LoadProfileByID clears the result");

    for (n = 0; n < sizeof(counts) / sizeof(counts[0]); n++)
    {
        IWMProfileManager2_SetSystemProfileVersion(mgr2, counts[n].version);
        count = 99;
        CHECK_HR(IWMProfileManager_GetSystemProfileCount(mgr, &count), S_OK, "GetSystemProfileCount");
        CHECKF(count == counts[n].count, "version %x: count %lu (got %lu)", counts[n].version, counts[n].count, count);
        profile = (IWMProfile *)1;
        CHECK_HR(IWMProfileManager_LoadSystemProfile(mgr, counts[n].count, &profile), E_INVALIDARG, "LoadSystemProfile(past the end)");
        CHECK(profile == NULL, "past the end clears the result");
    }

    IWMProfileManager2_SetSystemProfileVersion(mgr2, WMT_VER_8_0);
    for (i = 0; i < sizeof(expected) / sizeof(expected[0]); i++)
    {
        IWMStreamConfig *config;
        IWMMediaProps *props;
        WM_MEDIA_TYPE *mt;
        DWORD streams = 9, size = 0, bitrate = 0;
        WCHAR name[128];
        WMT_VERSION version = 0;
        WORD channels, tag;

        hr = IWMProfileManager_LoadSystemProfile(mgr, i, &profile);
        CHECKF(hr == S_OK, "LoadSystemProfile(%lu) = %08lx", i, (unsigned long)hr);
        if (FAILED(hr)) continue;
        IWMProfile_GetVersion(profile, &version);
        CHECKF(version == WMT_VER_8_0, "profile %lu version %x", i, version);
        IWMProfile_GetStreamCount(profile, &streams);
        CHECKF(streams == 1, "profile %lu has one stream (%lu)", i, streams);
        n = 0;
        IWMProfile_GetName(profile, NULL, &n);
        CHECKF(n > 1, "profile %lu has a name (%lu)", i, n);
        n = 128;
        IWMProfile_GetName(profile, name, &n);
        CHECKF(wcsstr(name, L"Kbps") != NULL, "profile %lu name %ls", i, name);
        if (SUCCEEDED(IWMProfile_GetStream(profile, 0, &config)))
        {
            GUID type;
            IWMStreamConfig_GetStreamType(config, &type);
            CHECK(!memcmp(&type, &WMMEDIATYPE_Audio, sizeof(type)), "stream is audio");
            IWMStreamConfig_GetBitrate(config, &bitrate);
            CHECKF(bitrate == expected[i].bitrate, "profile %lu bit rate %lu (got %lu)", i, expected[i].bitrate, bitrate);
            IWMStreamConfig_QueryInterface(config, &IID_IWMMediaProps, (void **)&props);
            IWMMediaProps_GetMediaType(props, NULL, &size);
            mt = malloc(size);
            IWMMediaProps_GetMediaType(props, mt, &size);
            channels = mt->cbFormat >= 18 ? mt->pbFormat[2] : 0;
            tag = mt->cbFormat >= 18 ? mt->pbFormat[0] | (mt->pbFormat[1] << 8) : 0;
            CHECKF(channels == expected[i].channels, "profile %lu channels %u (got %u)", i, expected[i].channels, channels);
            CHECKF(tag == 0x161, "profile %lu format tag %x", i, tag);
            free(mt);
            IWMMediaProps_Release(props);
            IWMStreamConfig_Release(config);
        }
        IWMProfile_Release(profile);
    }
    IWMProfileManager2_Release(mgr2);
}

static void test_writer(IWMWriter *writer, IWMProfileManager *mgr)
{
    static const struct { WORD stream; DWORD bitrate; HRESULT hr; const char *what; } stats[] = {
        {0, 128000, S_OK, "overall"}, {1, 128000, S_OK, "stream 1"}, {2, 0, E_INVALIDARG, "absent stream"},
    };
    static const struct { const WCHAR *name; WMT_ATTR_DATATYPE type; DWORD value; } defaults[] = {
        {L"InitialPatternForInput", WMT_TYPE_DWORD, 0}, {L"DeinterlaceMode", WMT_TYPE_DWORD, 0}, {L"InterlaceCoding", WMT_TYPE_BOOL, 0},
    };
    IWMWriterAdvanced3 *adv;
    IWMProfile *profile;
    WM_WRITER_STATISTICS ws;
    WM_WRITER_STATISTICS_EX wse;
    WMT_ATTR_DATATYPE type;
    QWORD time = 5;
    DWORD value, v2;
    WORD len;
    unsigned i;

    CHECK_HR(IWMWriter_QueryInterface(writer, &IID_IWMWriterAdvanced3, (void **)&adv), S_OK, "QI IWMWriterAdvanced3");

    CHECK_HR(IWMWriterAdvanced3_GetWriterTime(adv, NULL), E_INVALIDARG, "GetWriterTime(NULL)");
    CHECK_HR(IWMWriterAdvanced3_GetWriterTime(adv, &time), S_OK, "GetWriterTime");
    CHECK(time == 0, "writer time is 0 while idle");
    CHECK_HR(IWMWriterAdvanced3_GetStatistics(adv, 0, NULL), E_INVALIDARG, "GetStatistics(NULL)");
    CHECK_HR(IWMWriterAdvanced3_GetStatisticsEx(adv, 0, NULL), E_INVALIDARG, "GetStatisticsEx(NULL)");
    CHECK_HR(IWMWriterAdvanced3_GetStatistics(adv, 0, &ws), E_INVALIDPROFILE, "GetStatistics without a profile");
    CHECK_HR(IWMWriterAdvanced3_GetStatisticsEx(adv, 0, &wse), E_INVALIDPROFILE, "GetStatisticsEx without a profile");
    len = 4;
    CHECK_HR(IWMWriterAdvanced3_GetInputSetting(adv, 0, L"DeinterlaceMode", &type, (BYTE *)&value, &len), E_INVALIDPROFILE, "GetInputSetting without a profile");
    CHECK_HR(IWMWriter_SetProfileByID(writer, NULL), E_INVALIDARG, "SetProfileByID(NULL)");
    {
        GUID id = {0x11111111, 0x2222, 0x3333, {1, 2, 3, 4, 5, 6, 7, 8}};
        CHECK_HR(IWMWriter_SetProfileByID(writer, &id), E_INVALIDPROFILE, "SetProfileByID(unknown)");
    }

    CHECK_HR(IWMProfileManager_LoadSystemProfile(mgr, 6, &profile), S_OK, "LoadSystemProfile(6)");
    CHECK_HR(IWMWriter_SetProfile(writer, profile), S_OK, "SetProfile");
    IWMProfile_Release(profile);

    for (i = 0; i < 3; i++)
    {
        memset(&ws, 0xcc, sizeof(ws));
        {
            HRESULT hr = IWMWriterAdvanced3_GetStatistics(adv, stats[i].stream, &ws);
            CHECKF(hr == stats[i].hr, "GetStatistics %s = %08lx (got %08lx)", stats[i].what, (unsigned long)stats[i].hr, (unsigned long)hr);
        }
        if (stats[i].hr == S_OK)
        {
            CHECKF(ws.dwExpectedBitrate == stats[i].bitrate, "%s expected bit rate %lu (got %lu)", stats[i].what, stats[i].bitrate, ws.dwExpectedBitrate);
            CHECKF(ws.qwSampleCount == 0 && ws.qwByteCount == 0 && ws.qwDroppedSampleCount == 0 && ws.qwDroppedByteCount == 0
                    && ws.dwCurrentBitrate == 0 && ws.dwAverageBitrate == 0 && ws.dwCurrentSampleRate == 0
                    && ws.dwAverageSampleRate == 0 && ws.dwExpectedSampleRate == 0, "%s other statistics are zero", stats[i].what);
        }
        memset(&wse, 0xcc, sizeof(wse));
        {
            HRESULT hr = IWMWriterAdvanced3_GetStatisticsEx(adv, stats[i].stream, &wse);
            CHECKF(hr == stats[i].hr, "GetStatisticsEx %s = %08lx (got %08lx)", stats[i].what, (unsigned long)stats[i].hr, (unsigned long)hr);
            if (hr == S_OK)
            {
                static const char zero[sizeof(wse)];
                CHECKF(!memcmp(&wse, zero, sizeof(wse)), "%s extended statistics are zero", stats[i].what);
            }
        }
    }

    for (i = 0; i < 3; i++)
    {
        len = 4; value = 0xdead;
        CHECK_HR(IWMWriterAdvanced3_GetInputSetting(adv, 0, defaults[i].name, &type, (BYTE *)&value, &len), S_OK, "GetInputSetting default");
        CHECKF(type == defaults[i].type && value == defaults[i].value && len == 4, "%ls default: type %d value %lu len %u", defaults[i].name, type, value, len);
        len = 0;
        CHECK_HR(IWMWriterAdvanced3_GetInputSetting(adv, 0, defaults[i].name, &type, NULL, &len), S_OK, "GetInputSetting size query");
        CHECKF(len == 4, "%ls size 4 (%u)", defaults[i].name, len);
        len = 2;
        CHECK_HR(IWMWriterAdvanced3_GetInputSetting(adv, 0, defaults[i].name, &type, (BYTE *)&value, &len), ASF_E_BUFFERTOOSMALL, "GetInputSetting short buffer");
    }
    v2 = 2;
    CHECK_HR(IWMWriterAdvanced3_SetInputSetting(adv, 0, L"DeinterlaceMode", WMT_TYPE_DWORD, (BYTE *)&v2, 4), S_OK, "SetInputSetting");
    len = 4;
    IWMWriterAdvanced3_GetInputSetting(adv, 0, L"DeinterlaceMode", &type, (BYTE *)&value, &len);
    CHECKF(value == 2, "the setting reads back (%lu)", value);
    len = 4;
    IWMWriterAdvanced3_GetInputSetting(adv, 0, L"InitialPatternForInput", &type, (BYTE *)&value, &len);
    CHECKF(value == 0, "other settings are unchanged (%lu)", value);
    CHECK_HR(IWMWriterAdvanced3_SetInputSetting(adv, 0, L"DeinterlaceMode", WMT_TYPE_WORD, (BYTE *)&v2, 2), E_INVALIDARG, "SetInputSetting wrong type");
    CHECK_HR(IWMWriterAdvanced3_SetInputSetting(adv, 0, L"DeinterlaceMode", WMT_TYPE_DWORD, (BYTE *)&v2, 2), E_INVALIDARG, "SetInputSetting wrong length");
    CHECK_HR(IWMWriterAdvanced3_SetInputSetting(adv, 0, L"NoSuchSetting", WMT_TYPE_DWORD, (BYTE *)&v2, 4), E_INVALIDARG, "SetInputSetting unknown name");
    CHECK_HR(IWMWriterAdvanced3_SetInputSetting(adv, 1, L"DeinterlaceMode", WMT_TYPE_DWORD, (BYTE *)&v2, 4), E_INVALIDARG, "SetInputSetting bad input");
    CHECK_HR(IWMWriterAdvanced3_SetInputSetting(adv, 0, NULL, WMT_TYPE_DWORD, (BYTE *)&v2, 4), E_INVALIDARG, "SetInputSetting NULL name");
    CHECK_HR(IWMWriterAdvanced3_GetInputSetting(adv, 1, L"DeinterlaceMode", &type, (BYTE *)&value, &len), E_INVALIDARG, "GetInputSetting bad input");
    CHECK_HR(IWMWriterAdvanced3_GetInputSetting(adv, 0, L"NoSuchSetting", &type, (BYTE *)&value, &len), E_INVALIDARG, "GetInputSetting unknown name");
    IWMWriterAdvanced3_Release(adv);
}

int main(void)
{
    IWMProfileManager *mgr;
    IWMWriter *writer;
    HRESULT hr;
    HMODULE mod;

    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    mod = LoadLibraryW(L"wmvcore.dll");
    create_mgr = (void *)GetProcAddress(mod, "WMCreateProfileManager");
    create_writer = (void *)GetProcAddress(mod, "WMCreateWriter");
    if (!create_mgr || !create_writer) { check(0, "wmvcore.dll exports"); goto out; }
    hr = create_mgr(&mgr);
    CHECK_HR(hr, S_OK, "WMCreateProfileManager");
    if (FAILED(hr)) goto out;
    hr = create_writer(NULL, &writer);
    CHECK_HR(hr, S_OK, "WMCreateWriter");
    if (FAILED(hr)) goto out;
    test_system_profiles(mgr);
    test_writer(writer, mgr);
    IWMWriter_Release(writer);
    IWMProfileManager_Release(mgr);
out:
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
