/* wmvcore's reader configuration (patches/sg/2854), run by
 * test/wmv-config-gate.sh: IWMReaderNetworkConfig2 (every setting, string
 * getters, port ranges, logging URLs), the reader-advanced state (user
 * clock, manual stream selection, selection callbacks, log client id, play
 * mode, statistics, client info), IReferenceClock (GetTime, AdviseTime,
 * AdvisePeriodic, Unadvise) and IWMReaderStreamClock (GetTime, SetTimer,
 * KillTimer on an open reader, with the program's clock and the reader's).
 *
 *   wmv-config-probe.exe [path-of-sample.wmv] */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <dshow.h>
#include <wmsdk.h>
#include <nserror.h>
#ifndef ASF_E_BUFFERTOOSMALL
#define ASF_E_BUFFERTOOSMALL ((HRESULT)0xc00d07d1)
#endif
#include <stdio.h>
#include <string.h>

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}
#define CHECKF(ok, ...) do { char _b[320]; snprintf(_b, sizeof(_b), __VA_ARGS__); check(ok, _b); } while (0)

/* mingw ships no import library for wmvcore */
static IWMReader *new_reader(void)
{
    static HRESULT (WINAPI *create)(IUnknown *, DWORD, IWMReader **);
    IWMReader *r = NULL;
    HRESULT hr;

    if (!create)
    {
        HMODULE module = LoadLibraryW(L"wmvcore.dll");
        create = module ? (void *)GetProcAddress(module, "WMCreateReader") : NULL;
    }
    if (!create)
    {
        CHECKF(0, "wmvcore.dll does not export WMCreateReader");
        return NULL;
    }
    hr = create(NULL, 0, &r);
    if (FAILED(hr)) CHECKF(0, "WMCreateReader = %08lx", hr);
    return r;
}

static void test_interfaces(IWMReader *reader)
{
    static const IID *iids[] = {&IID_IWMReader, &IID_IWMReaderAdvanced, &IID_IWMReaderAdvanced2, &IID_IWMReaderAdvanced3,
        &IID_IWMReaderAdvanced4, &IID_IWMReaderAdvanced5, &IID_IWMReaderAdvanced6, &IID_IWMReaderNetworkConfig,
        &IID_IWMReaderNetworkConfig2, &IID_IWMReaderStreamClock, &IID_IWMReaderTypeNegotiation, &IID_IReferenceClock,
        &IID_IWMReaderAccelerator};
    unsigned int i;
    IUnknown *unk;
    HRESULT hr;
    GUID bad = {0x12345678, 0x1234, 0x1234, {1, 2, 3, 4, 5, 6, 7, 8}};

    for (i = 0; i < ARRAY_SIZE(iids); i++)
    {
        unk = NULL;
        hr = IWMReader_QueryInterface(reader, iids[i], (void **)&unk);
        CHECKF(hr == S_OK && unk, "QueryInterface of supported interface %u (%08lx)", i, hr);
        if (unk) IUnknown_Release(unk);
    }
    unk = (IUnknown *)1;
    hr = IWMReader_QueryInterface(reader, &bad, (void **)&unk);
    CHECKF(hr == E_NOINTERFACE && !unk, "QueryInterface of an unknown interface = E_NOINTERFACE, NULL (%08lx)", hr);
}

static void check_string(const char *what, HRESULT (*get)(void *, const WCHAR *, WCHAR *, DWORD *), void *obj,
        const WCHAR *proto, const WCHAR *expect)
{
    DWORD len = 99, need = wcslen(expect) + 1;
    WCHAR buf[64];
    HRESULT hr;

    hr = get(obj, proto, NULL, &len);
    CHECKF(hr == S_OK && len == need, "%s: NULL buffer gives the length %lu (%08lx %lu)", what, need, hr, len);
    len = need - 1;
    memset(buf, 0x55, sizeof(buf));
    hr = get(obj, proto, buf, &len);
    CHECKF(hr == ASF_E_BUFFERTOOSMALL && len == need, "%s: a short buffer = ASF_E_BUFFERTOOSMALL with the length (%08lx %lu)", what, hr, len);
    len = need;
    hr = get(obj, proto, buf, &len);
    CHECKF(hr == S_OK && len == need && !wcscmp(buf, expect), "%s: exact buffer (%08lx %ls)", what, hr, buf);
    len = 64;
    hr = get(obj, proto, buf, &len);
    CHECKF(hr == S_OK && len == need && !wcscmp(buf, expect), "%s: long buffer returns the used length", what);
    hr = get(obj, proto, buf, NULL);
    CHECKF(hr == E_INVALIDARG, "%s: NULL length = E_INVALIDARG (%08lx)", what, hr);
}

static HRESULT get_host(void *o, const WCHAR *p, WCHAR *b, DWORD *l) { return IWMReaderNetworkConfig2_GetProxyHostName((IWMReaderNetworkConfig2 *)o, p, b, l); }
static HRESULT get_exc(void *o, const WCHAR *p, WCHAR *b, DWORD *l) { return IWMReaderNetworkConfig2_GetProxyExceptionList((IWMReaderNetworkConfig2 *)o, p, b, l); }

static void test_network(IWMReader *reader)
{
    IWMReaderNetworkConfig2 *nc = NULL;
    static const WCHAR *protos[] = {L"http", L"MMS", L"rtsp"};
    QWORD q = 0;
    DWORD d, n, i;
    BOOL b;
    HRESULT hr;
    WMT_PROXY_SETTINGS ps;
    WM_PORT_NUMBER_RANGE ranges[3], got[3];
    WCHAR buf[64];

    IWMReader_QueryInterface(reader, &IID_IWMReaderNetworkConfig2, (void **)&nc);
    if (!nc) { check(0, "IWMReaderNetworkConfig2"); return; }

    /* defaults and round trips of the plain values */
    hr = IWMReaderNetworkConfig2_GetBufferingTime(nc, &q);
    CHECKF(hr == S_OK && q == 50000000, "bufferingTime defaults to 5 s (%08lx %I64u)", hr, q);
    hr = IWMReaderNetworkConfig2_SetBufferingTime(nc, 12345678);
    IWMReaderNetworkConfig2_GetBufferingTime(nc, &q);
    CHECKF(hr == S_OK && q == 12345678, "bufferingTime round trip (%I64u)", q);
    CHECKF(IWMReaderNetworkConfig2_GetBufferingTime(nc, NULL) == E_INVALIDARG, "GetBufferingTime(NULL) = E_INVALIDARG");

    {
        struct { const char *name; HRESULT (STDMETHODCALLTYPE *get)(IWMReaderNetworkConfig2 *, BOOL *);
                 HRESULT (STDMETHODCALLTYPE *set)(IWMReaderNetworkConfig2 *, BOOL); BOOL deflt; } flags[] = {
            {"ForceRerunAutoProxyDetection", nc->lpVtbl->GetForceRerunAutoProxyDetection, nc->lpVtbl->SetForceRerunAutoProxyDetection, FALSE},
            {"EnableMulticast", nc->lpVtbl->GetEnableMulticast, nc->lpVtbl->SetEnableMulticast, TRUE},
            {"EnableHTTP", nc->lpVtbl->GetEnableHTTP, nc->lpVtbl->SetEnableHTTP, TRUE},
            {"EnableUDP", nc->lpVtbl->GetEnableUDP, nc->lpVtbl->SetEnableUDP, TRUE},
            {"EnableTCP", nc->lpVtbl->GetEnableTCP, nc->lpVtbl->SetEnableTCP, TRUE},
            {"EnableContentCaching", nc->lpVtbl->GetEnableContentCaching, nc->lpVtbl->SetEnableContentCaching, TRUE},
            {"EnableFastCache", nc->lpVtbl->GetEnableFastCache, nc->lpVtbl->SetEnableFastCache, TRUE},
            {"EnableResends", nc->lpVtbl->GetEnableResends, nc->lpVtbl->SetEnableResends, TRUE},
            {"EnableThinning", nc->lpVtbl->GetEnableThinning, nc->lpVtbl->SetEnableThinning, TRUE},
        };
        for (i = 0; i < ARRAY_SIZE(flags); i++)
        {
            b = 5; hr = flags[i].get(nc, &b);
            CHECKF(hr == S_OK && b == flags[i].deflt, "%s defaults to %d (%08lx %d)", flags[i].name, flags[i].deflt, hr, b);
            hr = flags[i].set(nc, !flags[i].deflt);
            b = 5; flags[i].get(nc, &b);
            CHECKF(hr == S_OK && b == !flags[i].deflt, "%s flips (%08lx %d)", flags[i].name, hr, b);
            hr = flags[i].set(nc, 7); /* any non-zero is TRUE */
            b = 5; flags[i].get(nc, &b);
            CHECKF(b == TRUE, "%s stores TRUE for a non-zero value (%d)", flags[i].name, b);
            flags[i].set(nc, flags[i].deflt);
            CHECKF(flags[i].get(nc, NULL) == E_INVALIDARG, "%s(NULL) = E_INVALIDARG", flags[i].name);
        }
        /* each flag is its own */
        IWMReaderNetworkConfig2_SetEnableUDP(nc, FALSE);
        IWMReaderNetworkConfig2_GetEnableTCP(nc, &b);
        CHECKF(b == TRUE, "EnableTCP is not EnableUDP");
        IWMReaderNetworkConfig2_SetEnableUDP(nc, TRUE);
    }

    d = 99; hr = IWMReaderNetworkConfig2_GetConnectionBandwidth(nc, &d);
    CHECKF(hr == S_OK && d == 0, "connection bandwidth defaults to 0 (%08lx %lu)", hr, d);
    hr = IWMReaderNetworkConfig2_SetConnectionBandwidth(nc, 56000);
    IWMReaderNetworkConfig2_GetConnectionBandwidth(nc, &d);
    CHECKF(hr == S_OK && d == 56000, "connection bandwidth round trip (%lu)", d);
    q = 99; hr = IWMReaderNetworkConfig2_GetAcceleratedStreamingDuration(nc, &q);
    CHECKF(hr == S_OK && q == 0, "accelerated streaming duration defaults to 0 (%08lx)", hr);
    IWMReaderNetworkConfig2_SetAcceleratedStreamingDuration(nc, 30000000);
    IWMReaderNetworkConfig2_GetAcceleratedStreamingDuration(nc, &q);
    CHECKF(q == 30000000, "accelerated streaming duration round trip (%I64u)", q);
    d = 99; hr = IWMReaderNetworkConfig2_GetAutoReconnectLimit(nc, &d);
    CHECKF(hr == S_OK && d == 3, "auto reconnect limit defaults to 3 (%08lx %lu)", hr, d);
    IWMReaderNetworkConfig2_SetAutoReconnectLimit(nc, 10);
    IWMReaderNetworkConfig2_GetAutoReconnectLimit(nc, &d);
    CHECKF(d == 10, "auto reconnect limit round trip (%lu)", d);
    d = 99; hr = IWMReaderNetworkConfig2_GetMaxNetPacketSize(nc, &d);
    CHECKF(hr == S_OK && d == 1500, "max net packet size (%08lx %lu)", hr, d);
    CHECKF(IWMReaderNetworkConfig2_GetMaxNetPacketSize(nc, NULL) == E_INVALIDARG, "GetMaxNetPacketSize(NULL) = E_INVALIDARG");
    CHECKF(IWMReaderNetworkConfig2_GetConnectionBandwidth(nc, NULL) == E_INVALIDARG, "GetConnectionBandwidth(NULL) = E_INVALIDARG");
    CHECKF(IWMReaderNetworkConfig2_ResetProtocolRollover(nc) == S_OK, "ResetProtocolRollover");

    /* the supported protocols are the ones the proxy settings take */
    d = 0; hr = IWMReaderNetworkConfig2_GetNumProtocolsSupported(nc, &d);
    CHECKF(hr == S_OK && d == 3, "three protocols supported (%08lx %lu)", hr, d);
    CHECKF(IWMReaderNetworkConfig2_GetNumProtocolsSupported(nc, NULL) == E_INVALIDARG, "GetNumProtocolsSupported(NULL) = E_INVALIDARG");
    for (i = 0; i < 3; i++)
    {
        WCHAR name[16];
        DWORD len = 16;
        hr = IWMReaderNetworkConfig2_GetSupportedProtocolName(nc, i, name, &len);
        CHECKF(hr == S_OK && name[0], "protocol %lu has a name (%08lx %ls)", i, hr, name);
        d = 0;
        {
            WMT_PROXY_SETTINGS s;
            hr = IWMReaderNetworkConfig2_GetProxySettings(nc, name, &s);
            CHECKF(hr == S_OK, "protocol name %ls is accepted by the proxy settings (%08lx)", name, hr);
        }
    }
    d = 7; hr = IWMReaderNetworkConfig2_GetSupportedProtocolName(nc, 3, buf, &d);
    CHECKF(hr == E_INVALIDARG, "protocol 3 = E_INVALIDARG (%08lx)", hr);
    d = 0; hr = IWMReaderNetworkConfig2_GetSupportedProtocolName(nc, 0, NULL, &d);
    CHECKF(hr == S_OK && d > 1, "protocol name length query (%08lx %lu)", hr, d);
    d = 1; hr = IWMReaderNetworkConfig2_GetSupportedProtocolName(nc, 0, buf, &d);
    CHECKF(hr == ASF_E_BUFFERTOOSMALL, "protocol name in a short buffer (%08lx)", hr);

    /* the proxy settings, per protocol */
    for (i = 0; i < 3; i++)
    {
        ps = 99; hr = IWMReaderNetworkConfig2_GetProxySettings(nc, protos[i], &ps);
        CHECKF(hr == S_OK && ps == WMT_PROXY_SETTING_BROWSER, "%ls proxy setting defaults to the browser's (%08lx %d)", protos[i], hr, ps);
        d = 99; hr = IWMReaderNetworkConfig2_GetProxyPort(nc, protos[i], &d);
        CHECKF(hr == S_OK && d == 0, "%ls proxy port defaults to 0 (%08lx %lu)", protos[i], hr, d);
        b = 5; hr = IWMReaderNetworkConfig2_GetProxyBypassForLocal(nc, protos[i], &b);
        CHECKF(hr == S_OK && b == FALSE, "%ls bypass for local defaults to FALSE", protos[i]);
        check_string("default host name", get_host, nc, protos[i], L"");
        check_string("default exception list", get_exc, nc, protos[i], L"");
    }
    hr = IWMReaderNetworkConfig2_SetProxySettings(nc, L"HTTP", WMT_PROXY_SETTING_MANUAL);
    CHECKF(hr == S_OK, "SetProxySettings (%08lx)", hr);
    hr = IWMReaderNetworkConfig2_SetProxyHostName(nc, L"http", L"proxy.example.com");
    CHECKF(hr == S_OK, "SetProxyHostName (%08lx)", hr);
    hr = IWMReaderNetworkConfig2_SetProxyPort(nc, L"http", 8080);
    CHECKF(hr == S_OK, "SetProxyPort (%08lx)", hr);
    hr = IWMReaderNetworkConfig2_SetProxyExceptionList(nc, L"http", L"*.example.com;localhost");
    CHECKF(hr == S_OK, "SetProxyExceptionList (%08lx)", hr);
    hr = IWMReaderNetworkConfig2_SetProxyBypassForLocal(nc, L"http", 3);
    CHECKF(hr == S_OK, "SetProxyBypassForLocal (%08lx)", hr);
    ps = 99; IWMReaderNetworkConfig2_GetProxySettings(nc, L"http", &ps);
    CHECKF(ps == WMT_PROXY_SETTING_MANUAL, "http proxy setting reads back");
    d = 0; IWMReaderNetworkConfig2_GetProxyPort(nc, L"Http", &d);
    CHECKF(d == 8080, "http proxy port reads back (%lu)", d);
    b = 5; IWMReaderNetworkConfig2_GetProxyBypassForLocal(nc, L"http", &b);
    CHECKF(b == TRUE, "http bypass for local reads back as TRUE (%d)", b);
    check_string("http host name", get_host, nc, L"http", L"proxy.example.com");
    check_string("http exception list", get_exc, nc, L"http", L"*.example.com;localhost");
    for (i = 1; i < 3; i++)
    {
        ps = 99; IWMReaderNetworkConfig2_GetProxySettings(nc, protos[i], &ps);
        d = 99; IWMReaderNetworkConfig2_GetProxyPort(nc, protos[i], &d);
        b = 5; IWMReaderNetworkConfig2_GetProxyBypassForLocal(nc, protos[i], &b);
        CHECKF(ps == WMT_PROXY_SETTING_BROWSER && d == 0 && b == FALSE, "%ls proxy is independent of http (%d %lu %d)", protos[i], ps, d, b);
        check_string("other protocol's host name", get_host, nc, protos[i], L"");
    }
    /* errors */
    CHECKF(IWMReaderNetworkConfig2_SetProxySettings(nc, L"ftp", WMT_PROXY_SETTING_NONE) == E_INVALIDARG, "SetProxySettings(ftp) = E_INVALIDARG");
    CHECKF(IWMReaderNetworkConfig2_SetProxySettings(nc, NULL, WMT_PROXY_SETTING_NONE) == E_INVALIDARG, "SetProxySettings(NULL) = E_INVALIDARG");
    CHECKF(IWMReaderNetworkConfig2_SetProxySettings(nc, L"mms", WMT_PROXY_SETTING_MAX) == E_INVALIDARG, "SetProxySettings(MAX) = E_INVALIDARG");
    CHECKF(IWMReaderNetworkConfig2_SetProxySettings(nc, L"mms", (WMT_PROXY_SETTINGS)-1) == E_INVALIDARG, "SetProxySettings(-1) = E_INVALIDARG");
    CHECKF(IWMReaderNetworkConfig2_GetProxySettings(nc, L"ftp", &ps) == E_INVALIDARG, "GetProxySettings(ftp) = E_INVALIDARG");
    CHECKF(IWMReaderNetworkConfig2_GetProxySettings(nc, L"http", NULL) == E_INVALIDARG, "GetProxySettings(NULL out) = E_INVALIDARG");
    CHECKF(IWMReaderNetworkConfig2_SetProxyPort(nc, L"http", 65536) == E_INVALIDARG, "SetProxyPort(65536) = E_INVALIDARG");
    CHECKF(IWMReaderNetworkConfig2_SetProxyPort(nc, L"ftp", 80) == E_INVALIDARG, "SetProxyPort(ftp) = E_INVALIDARG");
    CHECKF(IWMReaderNetworkConfig2_SetProxyHostName(nc, L"http", NULL) == E_INVALIDARG, "SetProxyHostName(NULL) = E_INVALIDARG");
    CHECKF(IWMReaderNetworkConfig2_SetProxyExceptionList(nc, L"http", NULL) == E_INVALIDARG, "SetProxyExceptionList(NULL) = E_INVALIDARG");
    CHECKF(IWMReaderNetworkConfig2_GetProxyPort(nc, L"ftp", &d) == E_INVALIDARG, "GetProxyPort(ftp) = E_INVALIDARG");
    CHECKF(IWMReaderNetworkConfig2_GetProxyBypassForLocal(nc, L"ftp", &b) == E_INVALIDARG, "GetProxyBypassForLocal(ftp) = E_INVALIDARG");
    d = 8; CHECKF(IWMReaderNetworkConfig2_GetProxyHostName(nc, L"ftp", buf, &d) == E_INVALIDARG, "GetProxyHostName(ftp) = E_INVALIDARG");
    check_string("host name after a failed set", get_host, nc, L"http", L"proxy.example.com");

    /* UDP port ranges */
    d = 99; hr = IWMReaderNetworkConfig2_GetUDPPortRanges(nc, NULL, &d);
    CHECKF(hr == S_OK && d == 0, "no UDP port ranges at first (%08lx %lu)", hr, d);
    ranges[0].wPortBegin = 5000; ranges[0].wPortEnd = 5010;
    ranges[1].wPortBegin = 6000; ranges[1].wPortEnd = 6000;
    ranges[2].wPortBegin = 7000; ranges[2].wPortEnd = 7100;
    hr = IWMReaderNetworkConfig2_SetUDPPortRanges(nc, ranges, 3);
    CHECKF(hr == S_OK, "SetUDPPortRanges (%08lx)", hr);
    d = 99; hr = IWMReaderNetworkConfig2_GetUDPPortRanges(nc, NULL, &d);
    CHECKF(hr == S_OK && d == 3, "count query = 3 (%08lx %lu)", hr, d);
    d = 2; hr = IWMReaderNetworkConfig2_GetUDPPortRanges(nc, got, &d);
    CHECKF(hr == ASF_E_BUFFERTOOSMALL && d == 3, "short array = ASF_E_BUFFERTOOSMALL with the count (%08lx %lu)", hr, d);
    memset(got, 0, sizeof(got));
    d = 3; hr = IWMReaderNetworkConfig2_GetUDPPortRanges(nc, got, &d);
    CHECKF(hr == S_OK && d == 3 && !memcmp(got, ranges, sizeof(ranges)), "ranges read back (%08lx)", hr);
    ranges[0].wPortBegin = 1; /* the stored copy is not the caller's array */
    d = 3; IWMReaderNetworkConfig2_GetUDPPortRanges(nc, got, &d);
    CHECKF(got[0].wPortBegin == 5000, "the ranges are copied");
    ranges[1].wPortBegin = 7; ranges[1].wPortEnd = 3;
    hr = IWMReaderNetworkConfig2_SetUDPPortRanges(nc, ranges, 2);
    CHECKF(hr == E_INVALIDARG, "an inverted range = E_INVALIDARG (%08lx)", hr);
    hr = IWMReaderNetworkConfig2_SetUDPPortRanges(nc, NULL, 2);
    CHECKF(hr == E_INVALIDARG, "NULL ranges with a count = E_INVALIDARG (%08lx)", hr);
    d = 3; hr = IWMReaderNetworkConfig2_GetUDPPortRanges(nc, got, &d);
    CHECKF(d == 3, "refused ranges change nothing");
    hr = IWMReaderNetworkConfig2_SetUDPPortRanges(nc, NULL, 0);
    CHECKF(hr == S_OK, "clearing the ranges (%08lx)", hr);
    d = 99; IWMReaderNetworkConfig2_GetUDPPortRanges(nc, NULL, &d);
    CHECKF(d == 0, "no ranges after clearing (%lu)", d);
    CHECKF(IWMReaderNetworkConfig2_GetUDPPortRanges(nc, NULL, NULL) == E_INVALIDARG, "GetUDPPortRanges(NULL count) = E_INVALIDARG");

    /* logging URLs */
    d = 99; hr = IWMReaderNetworkConfig2_GetLoggingUrlCount(nc, &d);
    CHECKF(hr == S_OK && d == 0, "no logging URLs at first (%08lx %lu)", hr, d);
    CHECKF(IWMReaderNetworkConfig2_AddLoggingUrl(nc, L"http://log.example.com/a") == S_OK, "AddLoggingUrl");
    CHECKF(IWMReaderNetworkConfig2_AddLoggingUrl(nc, L"http://log.example.com/b") == S_OK, "AddLoggingUrl (2)");
    IWMReaderNetworkConfig2_GetLoggingUrlCount(nc, &d);
    CHECKF(d == 2, "two logging URLs (%lu)", d);
    n = 99; hr = IWMReaderNetworkConfig2_GetLoggingUrl(nc, 1, NULL, &n);
    CHECKF(hr == S_OK && n == wcslen(L"http://log.example.com/b") + 1, "logging URL length (%08lx %lu)", hr, n);
    n = 64; hr = IWMReaderNetworkConfig2_GetLoggingUrl(nc, 0, buf, &n);
    CHECKF(hr == S_OK && !wcscmp(buf, L"http://log.example.com/a"), "logging URL 0 (%08lx)", hr);
    n = 64; hr = IWMReaderNetworkConfig2_GetLoggingUrl(nc, 1, buf, &n);
    CHECKF(hr == S_OK && !wcscmp(buf, L"http://log.example.com/b"), "logging URL 1 is the second added");
    n = 5; hr = IWMReaderNetworkConfig2_GetLoggingUrl(nc, 0, buf, &n);
    CHECKF(hr == ASF_E_BUFFERTOOSMALL && n == wcslen(L"http://log.example.com/a") + 1, "logging URL in a short buffer (%08lx)", hr);
    n = 64; hr = IWMReaderNetworkConfig2_GetLoggingUrl(nc, 2, buf, &n);
    CHECKF(hr == E_INVALIDARG, "logging URL 2 = E_INVALIDARG (%08lx)", hr);
    CHECKF(IWMReaderNetworkConfig2_AddLoggingUrl(nc, NULL) == E_INVALIDARG, "AddLoggingUrl(NULL) = E_INVALIDARG");
    CHECKF(IWMReaderNetworkConfig2_AddLoggingUrl(nc, L"") == E_INVALIDARG, "AddLoggingUrl(\"\") = E_INVALIDARG");
    CHECKF(IWMReaderNetworkConfig2_ResetLoggingUrlList(nc) == S_OK, "ResetLoggingUrlList");
    IWMReaderNetworkConfig2_GetLoggingUrlCount(nc, &d);
    CHECKF(d == 0, "no logging URLs after the reset (%lu)", d);
    CHECKF(IWMReaderNetworkConfig2_GetLoggingUrlCount(nc, NULL) == E_INVALIDARG, "GetLoggingUrlCount(NULL) = E_INVALIDARG");

    IWMReaderNetworkConfig2_Release(nc);
}

static void test_advanced_state(IWMReader *reader)
{
    IWMReaderAdvanced6 *adv = NULL;
    WM_READER_STATISTICS stats;
    WM_READER_CLIENTINFO info;
    WMT_PLAY_MODE mode;
    BOOL b;
    HRESULT hr;
    unsigned int i;

    IWMReader_QueryInterface(reader, &IID_IWMReaderAdvanced6, (void **)&adv);
    if (!adv) { check(0, "IWMReaderAdvanced6"); return; }

    {
        struct { const char *name; HRESULT (STDMETHODCALLTYPE *set)(IWMReaderAdvanced6 *, BOOL);
                 HRESULT (STDMETHODCALLTYPE *get)(IWMReaderAdvanced6 *, BOOL *); } flags[] = {
            {"UserProvidedClock", adv->lpVtbl->SetUserProvidedClock, adv->lpVtbl->GetUserProvidedClock},
            {"ManualStreamSelection", adv->lpVtbl->SetManualStreamSelection, adv->lpVtbl->GetManualStreamSelection},
            {"ReceiveSelectionCallbacks", adv->lpVtbl->SetReceiveSelectionCallbacks, adv->lpVtbl->GetReceiveSelectionCallbacks},
            {"LogClientID", adv->lpVtbl->SetLogClientID, adv->lpVtbl->GetLogClientID},
        };
        for (i = 0; i < ARRAY_SIZE(flags); i++)
        {
            b = 5; hr = flags[i].get(adv, &b);
            CHECKF(hr == S_OK && b == FALSE, "%s defaults to FALSE (%08lx %d)", flags[i].name, hr, b);
            hr = flags[i].set(adv, TRUE);
            b = 5; flags[i].get(adv, &b);
            CHECKF(hr == S_OK && b == TRUE, "%s on (%08lx %d)", flags[i].name, hr, b);
            hr = flags[i].set(adv, FALSE);
            b = 5; flags[i].get(adv, &b);
            CHECKF(hr == S_OK && b == FALSE, "%s off (%08lx %d)", flags[i].name, hr, b);
            CHECKF(flags[i].get(adv, NULL) == E_INVALIDARG, "%s(NULL) = E_INVALIDARG", flags[i].name);
        }
        /* separate flags */
        IWMReaderAdvanced6_SetManualStreamSelection(adv, TRUE);
        IWMReaderAdvanced6_GetLogClientID(adv, &b);
        CHECKF(b == FALSE, "LogClientID is not ManualStreamSelection");
        IWMReaderAdvanced6_GetReceiveSelectionCallbacks(adv, &b);
        CHECKF(b == FALSE, "ReceiveSelectionCallbacks is not ManualStreamSelection");
        IWMReaderAdvanced6_SetManualStreamSelection(adv, FALSE);
    }

    mode = 99; hr = IWMReaderAdvanced6_GetPlayMode(adv, &mode);
    CHECKF(hr == S_OK && mode == WMT_PLAY_MODE_AUTOSELECT, "play mode defaults to autoselect (%08lx %d)", hr, mode);
    for (mode = WMT_PLAY_MODE_AUTOSELECT; mode <= WMT_PLAY_MODE_STREAMING; mode++)
    {
        WMT_PLAY_MODE got = 99;
        hr = IWMReaderAdvanced6_SetPlayMode(adv, mode);
        IWMReaderAdvanced6_GetPlayMode(adv, &got);
        CHECKF(hr == S_OK && got == mode, "play mode %d round trip (%08lx %d)", mode, hr, got);
    }
    hr = IWMReaderAdvanced6_SetPlayMode(adv, 4);
    CHECKF(hr == E_INVALIDARG, "play mode 4 = E_INVALIDARG (%08lx)", hr);
    hr = IWMReaderAdvanced6_SetPlayMode(adv, (WMT_PLAY_MODE)-1);
    CHECKF(hr == E_INVALIDARG, "play mode -1 = E_INVALIDARG (%08lx)", hr);
    IWMReaderAdvanced6_GetPlayMode(adv, &mode);
    CHECKF(mode == WMT_PLAY_MODE_STREAMING, "a refused play mode leaves the old one (%d)", mode);
    CHECKF(IWMReaderAdvanced6_GetPlayMode(adv, NULL) == E_INVALIDARG, "GetPlayMode(NULL) = E_INVALIDARG");

    memset(&stats, 0xaa, sizeof(stats));
    stats.cbSize = sizeof(stats);
    hr = IWMReaderAdvanced6_GetStatistics(adv, &stats);
    CHECKF(hr == S_OK && stats.cbSize == sizeof(stats) && !stats.dwBandwidth && !stats.cPacketsReceived && !stats.cPacketsRecovered
           && !stats.cPacketsLost && !stats.wQuality, "statistics of an idle reader are zero (%08lx)", hr);
    stats.cbSize = sizeof(stats) - 1;
    CHECKF(IWMReaderAdvanced6_GetStatistics(adv, &stats) == E_INVALIDARG, "statistics with a wrong cbSize = E_INVALIDARG");
    CHECKF(IWMReaderAdvanced6_GetStatistics(adv, NULL) == E_INVALIDARG, "statistics(NULL) = E_INVALIDARG");

    memset(&info, 0, sizeof(info));
    info.cbSize = sizeof(info);
    info.wszLang = (WCHAR *)L"en-US";
    hr = IWMReaderAdvanced6_SetClientInfo(adv, &info);
    CHECKF(hr == S_OK, "SetClientInfo (%08lx)", hr);
    info.cbSize = 4;
    CHECKF(IWMReaderAdvanced6_SetClientInfo(adv, &info) == E_INVALIDARG, "SetClientInfo with a wrong cbSize = E_INVALIDARG");
    CHECKF(IWMReaderAdvanced6_SetClientInfo(adv, NULL) == E_INVALIDARG, "SetClientInfo(NULL) = E_INVALIDARG");
    CHECKF(IWMReaderAdvanced6_NotifyLateDelivery(adv, 1000000) == S_OK, "NotifyLateDelivery");
    CHECKF(IWMReaderAdvanced6_AddLogParam(adv, L"ns", L"name", L"value") == S_OK, "AddLogParam");
    CHECKF(IWMReaderAdvanced6_AddLogParam(adv, NULL, L"name", L"value") == E_INVALIDARG, "AddLogParam(NULL) = E_INVALIDARG");
    CHECKF(IWMReaderAdvanced6_SendLogParams(adv) == S_OK, "SendLogParams");
    b = 5; hr = IWMReaderAdvanced6_GetReceiveStreamSamples(adv, 1, &b);
    CHECKF(hr == E_INVALIDARG, "GetReceiveStreamSamples without a file = E_INVALIDARG (%08lx)", hr);
    CHECKF(IWMReaderAdvanced6_GetReceiveStreamSamples(adv, 1, NULL) == E_INVALIDARG, "GetReceiveStreamSamples(NULL) = E_INVALIDARG");

    IWMReaderAdvanced6_Release(adv);
}

static void test_reference_clock(IWMReader *reader)
{
    IReferenceClock *clock = NULL;
    REFERENCE_TIME t1, t2, t3;
    HANDLE ev1, ev2, ev3, sem;
    DWORD_PTR c1, c2, c3, cp;
    LONG count1, count2, prev;
    HRESULT hr;
    DWORD r;

    IWMReader_QueryInterface(reader, &IID_IReferenceClock, (void **)&clock);
    if (!clock) { check(0, "IReferenceClock"); return; }

    t1 = 0; hr = IReferenceClock_GetTime(clock, &t1);
    CHECKF((hr == S_OK || hr == S_FALSE) && t1 > 0, "GetTime (%08lx %I64d)", hr, t1);
    Sleep(60);
    t2 = 0; hr = IReferenceClock_GetTime(clock, &t2);
    CHECKF(hr == S_OK && t2 > t1 && t2 - t1 >= 400000 && t2 - t1 < 30000000, "time advances with real time (%I64d)", t2 - t1);
    t3 = 0; hr = IReferenceClock_GetTime(clock, &t3);
    CHECKF((hr == S_OK && t3 >= t2) || (hr == S_FALSE && t3 == t2), "a repeated call is S_FALSE when the time is the same (%08lx)", hr);
    CHECKF(IReferenceClock_GetTime(clock, NULL) == E_POINTER, "GetTime(NULL) = E_POINTER");

    ev1 = CreateEventW(NULL, TRUE, FALSE, NULL);
    ev2 = CreateEventW(NULL, TRUE, FALSE, NULL);
    ev3 = CreateEventW(NULL, TRUE, FALSE, NULL);
    IReferenceClock_GetTime(clock, &t1);

    /* a time in the future (300 ms), one that is just about now, and one that is past */
    hr = IReferenceClock_AdviseTime(clock, t1, 3000000, (HEVENT)ev1, &c1);
    CHECKF(hr == S_OK && c1, "AdviseTime (%08lx %Iu)", hr, c1);
    hr = IReferenceClock_AdviseTime(clock, t1, 600000000, (HEVENT)ev2, &c2);
    CHECKF(hr == S_OK && c2 && c2 != c1, "AdviseTime far in the future has another cookie (%08lx)", hr);
    hr = IReferenceClock_AdviseTime(clock, t1 - 100000000, 0, (HEVENT)ev3, &c3);
    CHECKF(hr == S_OK, "AdviseTime in the past (%08lx)", hr);
    r = WaitForSingleObject(ev3, 0);
    CHECKF(r == WAIT_OBJECT_0, "a time in the past signals at once");
    r = WaitForSingleObject(ev1, 0);
    CHECKF(r == WAIT_TIMEOUT, "a time in the future does not signal at once");
    r = WaitForSingleObject(ev1, 2000);
    CHECKF(r == WAIT_OBJECT_0, "the event is signalled when its time comes");
    IReferenceClock_GetTime(clock, &t2);
    CHECKF(t2 - t1 >= 2400000, "not before its time (%I64d)", t2 - t1);
    r = WaitForSingleObject(ev2, 0);
    CHECKF(r == WAIT_TIMEOUT, "the far-away event is still waiting");
    hr = IReferenceClock_Unadvise(clock, c2);
    CHECKF(hr == S_OK, "Unadvise (%08lx)", hr);
    hr = IReferenceClock_Unadvise(clock, c2);
    CHECKF(hr == S_FALSE, "Unadvise of a cookie that is gone = S_FALSE (%08lx)", hr);
    hr = IReferenceClock_Unadvise(clock, 12345);
    CHECKF(hr == S_FALSE, "Unadvise of an unknown cookie = S_FALSE (%08lx)", hr);
    IReferenceClock_Unadvise(clock, c1);
    IReferenceClock_Unadvise(clock, c3);

    /* an unadvised time never signals */
    ResetEvent(ev1);
    IReferenceClock_GetTime(clock, &t1);
    hr = IReferenceClock_AdviseTime(clock, t1, 2000000, (HEVENT)ev1, &c1);
    IReferenceClock_Unadvise(clock, c1);
    r = WaitForSingleObject(ev1, 500);
    CHECKF(r == WAIT_TIMEOUT, "an unadvised time does not signal");

    hr = IReferenceClock_AdviseTime(clock, t1, 0, (HEVENT)ev1, NULL);
    CHECKF(hr == E_POINTER, "AdviseTime(NULL cookie) = E_POINTER (%08lx)", hr);
    hr = IReferenceClock_AdviseTime(clock, t1, 0, 0, &c1);
    CHECKF(hr == E_INVALIDARG, "AdviseTime(NULL event) = E_INVALIDARG (%08lx)", hr);

    /* a periodic advise releases the semaphore every period */
    sem = CreateSemaphoreW(NULL, 0, 1000, NULL);
    IReferenceClock_GetTime(clock, &t1);
    hr = IReferenceClock_AdvisePeriodic(clock, t1, 500000, (HSEMAPHORE)sem, &cp);
    CHECKF(hr == S_OK && cp, "AdvisePeriodic (%08lx)", hr);
    Sleep(600);
    hr = IReferenceClock_Unadvise(clock, cp);
    CHECKF(hr == S_OK, "Unadvise the periodic advise (%08lx)", hr);
    count1 = 0;
    while (WaitForSingleObject(sem, 0) == WAIT_OBJECT_0) count1++;
    CHECKF(count1 >= 5 && count1 <= 30, "about twelve 50 ms periods in 600 ms: %ld releases", count1);
    Sleep(200);
    count2 = 0;
    while (WaitForSingleObject(sem, 0) == WAIT_OBJECT_0) count2++;
    CHECKF(count2 == 0, "no more releases after Unadvise (%ld)", count2);
    hr = IReferenceClock_AdvisePeriodic(clock, t1, 0, (HSEMAPHORE)sem, &cp);
    CHECKF(hr == E_INVALIDARG, "AdvisePeriodic(period 0) = E_INVALIDARG (%08lx)", hr);
    hr = IReferenceClock_AdvisePeriodic(clock, t1, -5, (HSEMAPHORE)sem, &cp);
    CHECKF(hr == E_INVALIDARG, "AdvisePeriodic(negative period) = E_INVALIDARG (%08lx)", hr);
    hr = IReferenceClock_AdvisePeriodic(clock, t1, 100000, 0, &cp);
    CHECKF(hr == E_INVALIDARG, "AdvisePeriodic(NULL semaphore) = E_INVALIDARG (%08lx)", hr);
    hr = IReferenceClock_AdvisePeriodic(clock, t1, 100000, (HSEMAPHORE)sem, NULL);
    CHECKF(hr == E_POINTER, "AdvisePeriodic(NULL cookie) = E_POINTER (%08lx)", hr);
    (void)prev;

    CloseHandle(sem); CloseHandle(ev1); CloseHandle(ev2); CloseHandle(ev3);
    IReferenceClock_Release(clock);
}

/* ---- a callback that records OnTime ---- */

struct callback
{
    IWMReaderCallback IWMReaderCallback_iface;
    IWMReaderCallbackAdvanced IWMReaderCallbackAdvanced_iface;
    LONG ref;
    HANDLE opened, ontime;
    LONG ontime_count;
    QWORD last_time;
    void *last_context;
};

static struct callback *impl_cb(IWMReaderCallback *i) { return CONTAINING_RECORD(i, struct callback, IWMReaderCallback_iface); }
static struct callback *impl_cba(IWMReaderCallbackAdvanced *i) { return CONTAINING_RECORD(i, struct callback, IWMReaderCallbackAdvanced_iface); }
static HRESULT WINAPI cb_QI(IWMReaderCallback *i, REFIID riid, void **ppv)
{
    struct callback *c = impl_cb(i);
    if (IsEqualGUID(riid, &IID_IUnknown) || IsEqualGUID(riid, &IID_IWMStatusCallback) || IsEqualGUID(riid, &IID_IWMReaderCallback))
        *ppv = &c->IWMReaderCallback_iface;
    else if (IsEqualGUID(riid, &IID_IWMReaderCallbackAdvanced))
        *ppv = &c->IWMReaderCallbackAdvanced_iface;
    else { *ppv = NULL; return E_NOINTERFACE; }
    IUnknown_AddRef((IUnknown *)*ppv);
    return S_OK;
}
static ULONG WINAPI cb_AddRef(IWMReaderCallback *i) { return InterlockedIncrement(&impl_cb(i)->ref); }
static ULONG WINAPI cb_Release(IWMReaderCallback *i) { return InterlockedDecrement(&impl_cb(i)->ref); }
static HRESULT WINAPI cb_OnStatus(IWMReaderCallback *i, WMT_STATUS status, HRESULT hr, WMT_ATTR_DATATYPE type, BYTE *value, void *context)
{
    if (status == WMT_OPENED) SetEvent(impl_cb(i)->opened);
    return S_OK;
}
static HRESULT WINAPI cb_OnSample(IWMReaderCallback *i, DWORD output, QWORD time, QWORD duration, DWORD flags, INSSBuffer *sample, void *context)
{ return S_OK; }
static const IWMReaderCallbackVtbl cb_vtbl = {cb_QI, cb_AddRef, cb_Release, cb_OnStatus, cb_OnSample};

static HRESULT WINAPI cba_QI(IWMReaderCallbackAdvanced *i, REFIID riid, void **ppv) { return cb_QI(&impl_cba(i)->IWMReaderCallback_iface, riid, ppv); }
static ULONG WINAPI cba_AddRef(IWMReaderCallbackAdvanced *i) { return cb_AddRef(&impl_cba(i)->IWMReaderCallback_iface); }
static ULONG WINAPI cba_Release(IWMReaderCallbackAdvanced *i) { return cb_Release(&impl_cba(i)->IWMReaderCallback_iface); }
static HRESULT WINAPI cba_OnStreamSample(IWMReaderCallbackAdvanced *i, WORD s, QWORD t, QWORD d, DWORD f, INSSBuffer *b, void *c) { return S_OK; }
static HRESULT WINAPI cba_OnTime(IWMReaderCallbackAdvanced *i, QWORD time, void *context)
{
    struct callback *c = impl_cba(i);
    c->last_time = time;
    c->last_context = context;
    InterlockedIncrement(&c->ontime_count);
    SetEvent(c->ontime);
    return S_OK;
}
static HRESULT WINAPI cba_OnStreamSelection(IWMReaderCallbackAdvanced *i, WORD n, WORD *st, WMT_STREAM_SELECTION *s, void *c) { return S_OK; }
static HRESULT WINAPI cba_OnOutputPropsChanged(IWMReaderCallbackAdvanced *i, DWORD o, WM_MEDIA_TYPE *t, void *c) { return S_OK; }
static HRESULT WINAPI cba_AllocateForStream(IWMReaderCallbackAdvanced *i, WORD s, DWORD n, INSSBuffer **b, void *c) { return E_NOTIMPL; }
static HRESULT WINAPI cba_AllocateForOutput(IWMReaderCallbackAdvanced *i, DWORD o, DWORD n, INSSBuffer **b, void *c) { return E_NOTIMPL; }
static const IWMReaderCallbackAdvancedVtbl cba_vtbl = {cba_QI, cba_AddRef, cba_Release, cba_OnStreamSample, cba_OnTime,
    cba_OnStreamSelection, cba_OnOutputPropsChanged, cba_AllocateForStream, cba_AllocateForOutput};

static void test_stream_clock(const WCHAR *file)
{
    IWMReader *reader = new_reader();
    IWMReaderStreamClock *clock = NULL;
    IWMReaderAdvanced6 *adv = NULL;
    struct callback cb;
    QWORD now;
    DWORD id, id2, r;
    HRESULT hr;

    if (!reader) return;
    IWMReader_QueryInterface(reader, &IID_IWMReaderStreamClock, (void **)&clock);
    IWMReader_QueryInterface(reader, &IID_IWMReaderAdvanced6, (void **)&adv);

    /* not open yet */
    now = 99; hr = IWMReaderStreamClock_GetTime(clock, &now);
    CHECKF(hr == S_OK && now == 0, "an idle stream clock reads 0 (%08lx %I64u)", hr, now);
    CHECKF(IWMReaderStreamClock_GetTime(clock, NULL) == E_INVALIDARG, "GetTime(NULL) = E_INVALIDARG");
    hr = IWMReaderStreamClock_SetTimer(clock, 1000000, (void *)0x1234, &id);
    CHECKF(hr == NS_E_INVALID_REQUEST, "SetTimer before Open = NS_E_INVALID_REQUEST (%08lx)", hr);
    hr = IWMReaderStreamClock_KillTimer(clock, 77);
    CHECKF(hr == E_INVALIDARG, "KillTimer of an unknown timer = E_INVALIDARG (%08lx)", hr);

    if (!file)
    {
        printf("SKIP  no sample file: open reader clock tests\n");
        goto done;
    }
    memset(&cb, 0, sizeof(cb));
    cb.IWMReaderCallback_iface.lpVtbl = (IWMReaderCallbackVtbl *)&cb_vtbl;
    cb.IWMReaderCallbackAdvanced_iface.lpVtbl = (IWMReaderCallbackAdvancedVtbl *)&cba_vtbl;
    cb.ref = 1;
    cb.opened = CreateEventW(NULL, FALSE, FALSE, NULL);
    cb.ontime = CreateEventW(NULL, FALSE, FALSE, NULL);

    {
        /* the first open in a fresh prefix can fail while the media framework starts */
        int attempt;
        for (attempt = 0; attempt < 5; attempt++)
        {
            hr = IWMReader_Open(reader, file, &cb.IWMReaderCallback_iface, (void *)0xdeadbeef);
            if (hr == S_OK && WaitForSingleObject(cb.opened, 5000) == WAIT_OBJECT_0)
                break;
            if (hr == S_OK)
                IWMReader_Close(reader);
            hr = hr == S_OK ? E_FAIL : hr;
            Sleep(1500);
        }
    }
    if (FAILED(hr))
    {
        printf("SKIP  the sample file could not be opened here (%08lx)\n", hr);
        goto done;
    }
    CHECKF(hr == S_OK, "Open (%08lx)", hr);

    /* the program's clock: nothing is due until DeliverTime reaches it */
    IWMReaderAdvanced6_SetUserProvidedClock(adv, TRUE);
    hr = IWMReaderStreamClock_SetTimer(clock, 5000000, (void *)0xcafe, &id);
    CHECKF(hr == S_OK && id, "SetTimer with the program's clock (%08lx %lu)", hr, id);
    hr = IWMReaderStreamClock_SetTimer(clock, 9000000, (void *)0xbeef, &id2);
    CHECKF(hr == S_OK && id2 && id2 != id, "a second timer has its own id (%08lx)", hr);
    CHECKF(IWMReaderStreamClock_SetTimer(clock, 1, NULL, NULL) == E_INVALIDARG, "SetTimer(NULL id) = E_INVALIDARG");
    Sleep(300);
    CHECKF(cb.ontime_count == 0, "no OnTime before the clock moves (%ld)", cb.ontime_count);
    now = 99; IWMReaderStreamClock_GetTime(clock, &now);
    CHECKF(now == 0, "the program's clock reads what was delivered: 0 (%I64u)", now);
    hr = IWMReaderAdvanced6_DeliverTime(adv, 4000000);
    CHECKF(hr == S_OK, "DeliverTime (%08lx)", hr);
    now = 99; IWMReaderStreamClock_GetTime(clock, &now);
    CHECKF(now == 4000000, "the stream clock follows DeliverTime (%I64u)", now);
    Sleep(200);
    CHECKF(cb.ontime_count == 0, "still no OnTime at 0.4 s (%ld)", cb.ontime_count);
    hr = IWMReaderAdvanced6_DeliverTime(adv, 6000000);
    r = WaitForSingleObject(cb.ontime, 2000);
    CHECKF(r == WAIT_OBJECT_0 && cb.last_time == 5000000 && cb.last_context == (void *)0xcafe,
           "OnTime for the first timer at 0.6 s with its parameter (%lu, %I64u, %p)", r, cb.last_time, cb.last_context);
    Sleep(100);
    CHECKF(cb.ontime_count == 1, "only the first timer is due (%ld)", cb.ontime_count);
    hr = IWMReaderStreamClock_KillTimer(clock, id2);
    CHECKF(hr == S_OK, "KillTimer (%08lx)", hr);
    hr = IWMReaderAdvanced6_DeliverTime(adv, 20000000);
    Sleep(200);
    CHECKF(cb.ontime_count == 1, "a killed timer never fires (%ld)", cb.ontime_count);
    hr = IWMReaderStreamClock_KillTimer(clock, id2);
    CHECKF(hr == E_INVALIDARG, "KillTimer twice = E_INVALIDARG (%08lx)", hr);
    hr = IWMReaderStreamClock_KillTimer(clock, id);
    CHECKF(hr == S_OK, "KillTimer of a timer that has fired (%08lx)", hr);

    /* a timer that is already due fires at once */
    hr = IWMReaderStreamClock_SetTimer(clock, 1000, (void *)0x77, &id);
    r = WaitForSingleObject(cb.ontime, 2000);
    CHECKF(hr == S_OK && r == WAIT_OBJECT_0 && cb.last_context == (void *)0x77, "a timer in the past fires at once (%08lx %lu)", hr, r);

    /* the reader's own clock: not running before Start, so a timer is a real delay */
    IWMReaderAdvanced6_SetUserProvidedClock(adv, FALSE);
    cb.ontime_count = 0;
    hr = IWMReaderStreamClock_SetTimer(clock, 3000000, (void *)0x88, &id);
    CHECKF(hr == S_OK, "SetTimer with the reader's clock (%08lx)", hr);
    r = WaitForSingleObject(cb.ontime, 100);
    CHECKF(r == WAIT_TIMEOUT, "not at once");
    r = WaitForSingleObject(cb.ontime, 3000);
    CHECKF(r == WAIT_OBJECT_0 && cb.last_context == (void *)0x88 && cb.last_time == 3000000, "fires after its delay (%lu)", r);
    hr = IWMReaderStreamClock_SetTimer(clock, 600000000, (void *)0x99, &id);
    CHECKF(hr == S_OK, "SetTimer a minute away (%08lx)", hr);

    /* Close kills what is left */
    hr = IWMReader_Close(reader);
    CHECKF(hr == S_OK, "Close (%08lx)", hr);
    hr = IWMReaderStreamClock_KillTimer(clock, id);
    CHECKF(hr == E_INVALIDARG, "Close removed the pending timers (%08lx)", hr);
    CloseHandle(cb.opened); CloseHandle(cb.ontime);
done:
    IWMReaderAdvanced6_Release(adv);
    IWMReaderStreamClock_Release(clock);
    IWMReader_Release(reader);
}

int main(int argc, char **argv)
{
    IWMReader *reader;
    WCHAR file[MAX_PATH];

    CoInitialize(NULL);
    reader = new_reader();
    if (!reader) goto out;
    test_interfaces(reader);
    test_network(reader);
    test_advanced_state(reader);
    test_reference_clock(reader);
    IWMReader_Release(reader);

    if (argc > 1 && MultiByteToWideChar(CP_ACP, 0, argv[1], -1, file, MAX_PATH))
        test_stream_clock(file);
    else
        test_stream_clock(NULL);
out:
    CoUninitialize();
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures ? 1 : 0;
}
