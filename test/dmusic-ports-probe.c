/* dmusic's ports, buffer and clock stubs (patches/sg/2971), run by test/dmusic-ports-gate.sh:
 * the software synth port (channel groups, priorities, running statistics, append, DeviceIoControl),
 * IDirectMusicBuffer (TotalTime, ResetReadPtr, GetNextEvent) and the master clock.
 *
 *   dmusic-ports-probe.exe */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <dmusici.h>
#include <dmusics.h>
#include <dmerror.h>
#include <stdio.h>
#include <string.h>

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}
#define CHECKF(ok, ...) do { char _b[256]; snprintf(_b, sizeof(_b), __VA_ARGS__); check(ok, _b); } while (0)

static const GUID g_system = { 0x58d58419, 0x71b4, 0x11d1, { 0xa7, 0x4c, 0x00, 0x00, 0xf8, 0x75, 0xac, 0x12 } };
static const GUID g_dsound = { 0x58d58420, 0x71b4, 0x11d1, { 0xa7, 0x4c, 0x00, 0x00, 0xf8, 0x75, 0xac, 0x12 } };

static const char *gs(const GUID *g)
{
    static char b[4][40];
    static int n;
    char *s = b[n++ & 3];
    snprintf(s, 40, "%08lx-%04x-%04x-%02x%02x", g->Data1, g->Data2, g->Data3, g->Data4[0], g->Data4[1]);
    return s;
}

static void test_port(IDirectMusic8 *dm)
{
    static const DWORD defprio[3] = { DAUD_CHAN1_DEF_VOICE_PRIORITY, DAUD_CHAN2_DEF_VOICE_PRIORITY,
            DAUD_CHAN10_DEF_VOICE_PRIORITY };
    static const int chans[3] = { 0, 1, 9 };
    DMUS_PORTPARAMS params = { sizeof(params) };
    IDirectMusicPortDownload *download;
    IDirectMusicThru *thru;
    IDirectMusicPort *port;
    DMUS_SYNTHSTATS stats;
    DWORD groups, prio, append, ret, i;
    HRESULT hr;

    params.dwValidParams = DMUS_PORTPARAMS_CHANNELGROUPS | DMUS_PORTPARAMS_AUDIOCHANNELS;
    params.dwChannelGroups = 2;
    params.dwAudioChannels = 2;
    hr = IDirectMusic8_CreatePort(dm, &GUID_NULL, &params, &port, NULL);
    CHECKF(hr == S_OK, "CreatePort (%#lx)", hr);
    if (hr != S_OK) return;

    hr = IDirectMusicPort_GetNumChannelGroups(port, &groups);
    CHECKF(hr == S_OK && groups == 2, "GetNumChannelGroups is the requested count (%#lx, %lu)", hr, groups);
    hr = IDirectMusicPort_GetNumChannelGroups(port, NULL);
    CHECKF(hr == E_POINTER, "GetNumChannelGroups(NULL) (%#lx)", hr);
    hr = IDirectMusicPort_SetNumChannelGroups(port, 0);
    CHECKF(hr == E_INVALIDARG, "SetNumChannelGroups(0) (%#lx)", hr);
    hr = IDirectMusicPort_SetNumChannelGroups(port, 1000000);
    CHECKF(hr == E_INVALIDARG, "SetNumChannelGroups(1000000) (%#lx)", hr);
    hr = IDirectMusicPort_GetNumChannelGroups(port, &groups);
    CHECKF(hr == S_OK && groups == 2, "count unchanged after failures (%lu)", groups);
    hr = IDirectMusicPort_SetNumChannelGroups(port, 3);
    CHECKF(hr == S_OK, "SetNumChannelGroups(3) (%#lx)", hr);
    hr = IDirectMusicPort_GetNumChannelGroups(port, &groups);
    CHECKF(hr == S_OK && groups == 3, "count is 3 (%lu)", groups);

    for (i = 0; i < 3; i++)
    {
        prio = 0xdeadbeef;
        hr = IDirectMusicPort_GetChannelPriority(port, 1, chans[i], &prio);
        CHECKF(hr == S_OK && prio == defprio[i], "default priority of channel %d (%#lx, %lu, want %lu)",
                chans[i], hr, prio, defprio[i]);
    }
    hr = IDirectMusicPort_SetChannelPriority(port, 1, 5, 1234);
    CHECKF(hr == S_OK, "SetChannelPriority (%#lx)", hr);
    hr = IDirectMusicPort_SetChannelPriority(port, 3, 15, 4321);
    CHECKF(hr == S_OK, "SetChannelPriority group 3 (%#lx)", hr);
    prio = 0;
    hr = IDirectMusicPort_GetChannelPriority(port, 1, 5, &prio);
    CHECKF(hr == S_OK && prio == 1234, "priority is stored (%#lx, %lu)", hr, prio);
    hr = IDirectMusicPort_GetChannelPriority(port, 3, 15, &prio);
    CHECKF(hr == S_OK && prio == 4321, "priority of group 3 is stored (%#lx, %lu)", hr, prio);
    hr = IDirectMusicPort_GetChannelPriority(port, 2, 5, &prio);
    CHECKF(hr == S_OK && prio != 1234, "priority is per group (%lu)", prio);
    hr = IDirectMusicPort_SetChannelPriority(port, 1, 16, 1);
    CHECKF(hr == E_INVALIDARG, "SetChannelPriority channel 16 (%#lx)", hr);
    hr = IDirectMusicPort_SetChannelPriority(port, 0, 1, 1);
    CHECKF(hr == E_INVALIDARG, "SetChannelPriority group 0 (%#lx)", hr);
    hr = IDirectMusicPort_GetChannelPriority(port, 1, 16, &prio);
    CHECKF(hr == E_INVALIDARG, "GetChannelPriority channel 16 (%#lx)", hr);
    hr = IDirectMusicPort_GetChannelPriority(port, 1, 1, NULL);
    CHECKF(hr == E_POINTER, "GetChannelPriority(NULL) (%#lx)", hr);

    memset(&stats, 0xcc, sizeof(stats));
    stats.dwSize = sizeof(stats);
    hr = IDirectMusicPort_GetRunningStats(port, &stats);
    CHECKF(hr == S_OK && (stats.dwValidStats & DMUS_SYNTHSTATS_VOICES) && stats.dwVoices == 0,
            "GetRunningStats reports the voice count (%#lx, valid %#lx, voices %lu)", hr, stats.dwValidStats, stats.dwVoices);
    hr = IDirectMusicPort_GetRunningStats(port, NULL);
    CHECKF(hr == E_INVALIDARG, "GetRunningStats(NULL) (%#lx)", hr);

    ret = 0xdeadbeef;
    hr = IDirectMusicPort_DeviceIoControl(port, 0, NULL, 0, NULL, 0, &ret, NULL);
    CHECKF(hr == E_NOTIMPL, "DeviceIoControl (%#lx)", hr);
    hr = IDirectMusicPort_Read(port, NULL);
    CHECKF(hr == E_POINTER, "Read(NULL) (%#lx)", hr);

    hr = IDirectMusicPort_QueryInterface(port, &IID_IDirectMusicPortDownload, (void **)&download);
    CHECKF(hr == S_OK, "QI PortDownload (%#lx)", hr);
    if (hr == S_OK)
    {
        append = 0;
        hr = IDirectMusicPortDownload_GetAppend(download, &append);
        CHECKF(hr == S_OK && append == 2, "GetAppend is 2 (%#lx, %lu)", hr, append);
        hr = IDirectMusicPortDownload_GetAppend(download, NULL);
        CHECKF(hr == E_POINTER, "GetAppend(NULL) (%#lx)", hr);
        IDirectMusicPortDownload_Release(download);
    }

    thru = (void *)0xdeadbeef;
    hr = IDirectMusicPort_QueryInterface(port, &IID_IDirectMusicThru, (void **)&thru);
    CHECKF(hr == E_NOINTERFACE && !thru, "synth port has no IDirectMusicThru (%#lx, %p)", hr, thru);
    if (hr == S_OK) IDirectMusicThru_Release(thru);

    IDirectMusicPort_Release(port);
}

static void test_buffer(IDirectMusic8 *dm)
{
    DMUS_BUFFERDESC desc = { sizeof(desc) };
    IDirectMusicBuffer *buf;
    REFERENCE_TIME rt;
    DWORD group, len;
    BYTE *data, raw[3] = { 0x90, 0x40, 0x7f };
    HRESULT hr;

    desc.cbBuffer = 1024;
    hr = IDirectMusic8_CreateMusicBuffer(dm, &desc, &buf, NULL);
    CHECKF(hr == S_OK, "CreateMusicBuffer (%#lx)", hr);
    if (hr != S_OK) return;

    rt = 0x77;
    hr = IDirectMusicBuffer_TotalTime(buf, &rt);
    CHECKF(hr == S_OK && rt == 0, "TotalTime of an empty buffer (%#lx, %I64d)", hr, rt);
    hr = IDirectMusicBuffer_TotalTime(buf, NULL);
    CHECKF(hr == E_POINTER, "TotalTime(NULL) (%#lx)", hr);
    hr = IDirectMusicBuffer_GetNextEvent(buf, &rt, &group, &len, &data);
    CHECKF(hr == S_FALSE, "GetNextEvent on an empty buffer (%#lx)", hr);

    hr = IDirectMusicBuffer_PackStructured(buf, 1000, 4, 0x7f4090);
    CHECKF(hr == S_OK, "PackStructured (%#lx)", hr);
    hr = IDirectMusicBuffer_PackUnstructured(buf, 1250, 5, 3, raw);
    CHECKF(hr == S_OK, "PackUnstructured (%#lx)", hr);
    hr = IDirectMusicBuffer_PackStructured(buf, 1700, 6, 0x000080);
    CHECKF(hr == S_OK, "PackStructured 2 (%#lx)", hr);

    hr = IDirectMusicBuffer_TotalTime(buf, &rt);
    CHECKF(hr == S_OK && rt == 700, "TotalTime is the span of the events (%#lx, %I64d)", hr, rt);

    rt = 0; group = 0; len = 0; data = NULL;
    hr = IDirectMusicBuffer_GetNextEvent(buf, &rt, &group, &len, &data);
    CHECKF(hr == S_OK && rt == 1000 && group == 4 && len == 3 && data && data[0] == 0x90 && data[1] == 0x40 && data[2] == 0x7f,
            "first event (%#lx, rt %I64d, group %lu, len %lu)", hr, rt, group, len);
    hr = IDirectMusicBuffer_GetNextEvent(buf, &rt, &group, &len, &data);
    CHECKF(hr == S_OK && rt == 1250 && group == 5 && len == 3 && data && !memcmp(data, raw, 3),
            "second event (%#lx, rt %I64d, group %lu, len %lu)", hr, rt, group, len);
    hr = IDirectMusicBuffer_GetNextEvent(buf, NULL, &group, NULL, NULL);
    CHECKF(hr == S_OK && group == 6, "third event with NULL out params (%#lx, group %lu)", hr, group);
    hr = IDirectMusicBuffer_GetNextEvent(buf, &rt, &group, &len, &data);
    CHECKF(hr == S_FALSE, "no fourth event (%#lx)", hr);
    hr = IDirectMusicBuffer_ResetReadPtr(buf);
    CHECKF(hr == S_OK, "ResetReadPtr (%#lx)", hr);
    hr = IDirectMusicBuffer_GetNextEvent(buf, &rt, &group, &len, &data);
    CHECKF(hr == S_OK && rt == 1000 && group == 4, "first event again after ResetReadPtr (%#lx, %I64d)", hr, rt);

    IDirectMusicBuffer_Flush(buf);
    hr = IDirectMusicBuffer_GetNextEvent(buf, &rt, &group, &len, &data);
    CHECKF(hr == S_FALSE, "Flush empties the read side (%#lx)", hr);
    IDirectMusicBuffer_Release(buf);
}

static void test_clock(IDirectMusic8 *dm)
{
    IReferenceClock *clock, *clock2;
    DMUS_CLOCKINFO info;
    DWORD_PTR cookie = 0;
    HRESULT hr;
    GUID guid;
    DWORD i;

    hr = IDirectMusic8_GetMasterClock(dm, &guid, &clock);
    CHECKF(hr == S_OK && IsEqualGUID(&guid, &g_system), "default master clock is the system clock (%#lx, %s)", hr, gs(&guid));
    if (hr != S_OK) return;

    memset(&info, 0, sizeof(info));
    info.dwSize = sizeof(info);
    for (i = 0; i < 3; i++)
    {
        hr = IDirectMusic8_EnumMasterClock(dm, i, &info);
        if (i == 0) CHECKF(hr == S_OK && IsEqualGUID(&info.guidClock, &g_system), "clock 0 (%#lx)", hr);
        if (i == 1) CHECKF(hr == S_OK && IsEqualGUID(&info.guidClock, &g_dsound), "clock 1 (%#lx)", hr);
        if (i == 2) CHECKF(hr == S_FALSE, "no clock 2 (%#lx)", hr);
    }
    hr = IDirectMusic8_EnumMasterClock(dm, 0, NULL);
    CHECKF(hr == E_INVALIDARG, "EnumMasterClock(NULL) (%#lx)", hr);

    hr = IDirectMusic8_SetMasterClock(dm, &g_dsound);
    CHECKF(hr == S_OK, "SetMasterClock(dsound) (%#lx)", hr);
    hr = IDirectMusic8_GetMasterClock(dm, &guid, &clock2);
    CHECKF(hr == S_OK && IsEqualGUID(&guid, &g_dsound), "master clock guid follows SetMasterClock (%s)", gs(&guid));
    if (hr == S_OK) IReferenceClock_Release(clock2);
    hr = IDirectMusic8_SetMasterClock(dm, &GUID_NULL);
    CHECKF(hr == E_INVALIDARG, "SetMasterClock(GUID_NULL) (%#lx)", hr);
    hr = IDirectMusic8_SetMasterClock(dm, NULL);
    CHECKF(hr == E_INVALIDARG, "SetMasterClock(NULL) (%#lx)", hr);
    hr = IDirectMusic8_GetMasterClock(dm, &guid, NULL);
    CHECKF(hr == S_OK && IsEqualGUID(&guid, &g_dsound), "failed SetMasterClock keeps the clock (%s)", gs(&guid));
    hr = IDirectMusic8_SetMasterClock(dm, &g_system);
    CHECKF(hr == S_OK, "SetMasterClock(system) (%#lx)", hr);

    hr = IReferenceClock_AdviseTime(clock, 0, 0, 0, &cookie);
    CHECKF(hr == E_NOTIMPL, "AdviseTime (%#lx)", hr);
    hr = IReferenceClock_AdvisePeriodic(clock, 0, 1, 0, &cookie);
    CHECKF(hr == E_NOTIMPL, "AdvisePeriodic (%#lx)", hr);
    hr = IReferenceClock_Unadvise(clock, 1);
    CHECKF(hr == E_NOTIMPL, "Unadvise (%#lx)", hr);

    hr = IDirectMusic8_SetExternalMasterClock(dm, NULL);
    CHECKF(hr == E_POINTER, "SetExternalMasterClock(NULL) (%#lx)", hr);
    hr = IDirectMusic8_SetExternalMasterClock(dm, clock);
    CHECKF(hr == S_OK, "SetExternalMasterClock (%#lx)", hr);
    hr = IDirectMusic8_GetMasterClock(dm, &guid, &clock2);
    CHECKF(hr == S_OK && IsEqualGUID(&guid, &GUID_NULL) && clock2 == clock,
            "external clock has GUID_NULL and is returned (%s)", gs(&guid));
    if (hr == S_OK) IReferenceClock_Release(clock2);
    IReferenceClock_Release(clock);
}

int main(void)
{
    IDirectMusic8 *dm;
    HRESULT hr;

    CoInitialize(NULL);
    hr = CoCreateInstance(&CLSID_DirectMusic, NULL, CLSCTX_INPROC_SERVER, &IID_IDirectMusic8, (void **)&dm);
    CHECKF(hr == S_OK, "create DirectMusic (%#lx)", hr);
    if (hr != S_OK) { printf("RESULT: FAIL\n"); return 1; }
    hr = IDirectMusic8_SetDirectSound(dm, NULL, NULL);
    CHECKF(hr == S_OK, "SetDirectSound(NULL) (%#lx)", hr);

    test_clock(dm);
    test_buffer(dm);
    test_port(dm);

    IDirectMusic8_Release(dm);
    CoUninitialize();
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
