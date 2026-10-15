/* iphlpapi packet filtering (Pf*) and ICMPv6 echo (patch 3187), run by test/fastpf-gate.sh.
 *   fastpf-probe.exe api        every Pf* call fails with the documented error (120)
 *   fastpf-probe.exe icmp6      Icmp6 echo to ::1 */
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iphlpapi.h>
#include <icmpapi.h>
#include <fltdefs.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* the DLL exports these as _Name@N (as Windows' does); mingw's import library has none */
#undef PfCreateInterface
#undef PfDeleteInterface
#undef PfAddFiltersToInterface
#undef PfRemoveFiltersFromInterface
#undef PfRemoveFilterHandles
#undef PfUnBindInterface
#undef PfBindInterfaceToIndex
#undef PfBindInterfaceToIPAddress
#undef PfRebindFilters
#undef PfAddGlobalFilterToInterface
#undef PfRemoveGlobalFilterFromInterface
#undef PfMakeLog
#undef PfSetLogBuffer
#undef PfDeleteLog
#undef PfGetInterfaceStatistics
#undef PfTestPacket
static DWORD (WINAPI *p_PfCreateInterface)(DWORD,PFFORWARD_ACTION,PFFORWARD_ACTION,BOOL,BOOL,INTERFACE_HANDLE*);
#define PfCreateInterface p_PfCreateInterface
static DWORD (WINAPI *p_PfDeleteInterface)(INTERFACE_HANDLE);
#define PfDeleteInterface p_PfDeleteInterface
static DWORD (WINAPI *p_PfAddFiltersToInterface)(INTERFACE_HANDLE,DWORD,PPF_FILTER_DESCRIPTOR,DWORD,PPF_FILTER_DESCRIPTOR,PFILTER_HANDLE);
#define PfAddFiltersToInterface p_PfAddFiltersToInterface
static DWORD (WINAPI *p_PfRemoveFiltersFromInterface)(INTERFACE_HANDLE,DWORD,PPF_FILTER_DESCRIPTOR,DWORD,PPF_FILTER_DESCRIPTOR);
#define PfRemoveFiltersFromInterface p_PfRemoveFiltersFromInterface
static DWORD (WINAPI *p_PfRemoveFilterHandles)(INTERFACE_HANDLE,DWORD,PFILTER_HANDLE);
#define PfRemoveFilterHandles p_PfRemoveFilterHandles
static DWORD (WINAPI *p_PfUnBindInterface)(INTERFACE_HANDLE);
#define PfUnBindInterface p_PfUnBindInterface
static DWORD (WINAPI *p_PfBindInterfaceToIndex)(INTERFACE_HANDLE,DWORD,PFADDRESSTYPE,PBYTE);
#define PfBindInterfaceToIndex p_PfBindInterfaceToIndex
static DWORD (WINAPI *p_PfBindInterfaceToIPAddress)(INTERFACE_HANDLE,PFADDRESSTYPE,PBYTE);
#define PfBindInterfaceToIPAddress p_PfBindInterfaceToIPAddress
static DWORD (WINAPI *p_PfRebindFilters)(INTERFACE_HANDLE,PPF_LATEBIND_INFO);
#define PfRebindFilters p_PfRebindFilters
static DWORD (WINAPI *p_PfAddGlobalFilterToInterface)(INTERFACE_HANDLE,GLOBAL_FILTER);
#define PfAddGlobalFilterToInterface p_PfAddGlobalFilterToInterface
static DWORD (WINAPI *p_PfRemoveGlobalFilterFromInterface)(INTERFACE_HANDLE,GLOBAL_FILTER);
#define PfRemoveGlobalFilterFromInterface p_PfRemoveGlobalFilterFromInterface
static DWORD (WINAPI *p_PfMakeLog)(HANDLE);
#define PfMakeLog p_PfMakeLog
static DWORD (WINAPI *p_PfSetLogBuffer)(PBYTE,DWORD,DWORD,DWORD,PDWORD,PDWORD,PDWORD);
#define PfSetLogBuffer p_PfSetLogBuffer
static DWORD (WINAPI *p_PfDeleteLog)(void);
#define PfDeleteLog p_PfDeleteLog
static DWORD (WINAPI *p_PfGetInterfaceStatistics)(INTERFACE_HANDLE,PPF_INTERFACE_STATS,PDWORD,BOOL);
#define PfGetInterfaceStatistics p_PfGetInterfaceStatistics
static DWORD (WINAPI *p_PfTestPacket)(INTERFACE_HANDLE,INTERFACE_HANDLE,DWORD,PBYTE,PPFFORWARD_ACTION);
#define PfTestPacket p_PfTestPacket
static void load_pf(void)
{
    HMODULE m = LoadLibraryA("iphlpapi.dll");
    char buf[80];
    sprintf(buf, "_PfCreateInterface@24"); p_PfCreateInterface = (void *)GetProcAddress(m, buf);
    sprintf(buf, "_PfDeleteInterface@4"); p_PfDeleteInterface = (void *)GetProcAddress(m, buf);
    sprintf(buf, "_PfAddFiltersToInterface@24"); p_PfAddFiltersToInterface = (void *)GetProcAddress(m, buf);
    sprintf(buf, "_PfRemoveFiltersFromInterface@20"); p_PfRemoveFiltersFromInterface = (void *)GetProcAddress(m, buf);
    sprintf(buf, "_PfRemoveFilterHandles@12"); p_PfRemoveFilterHandles = (void *)GetProcAddress(m, buf);
    sprintf(buf, "_PfUnBindInterface@4"); p_PfUnBindInterface = (void *)GetProcAddress(m, buf);
    sprintf(buf, "_PfBindInterfaceToIndex@16"); p_PfBindInterfaceToIndex = (void *)GetProcAddress(m, buf);
    sprintf(buf, "_PfBindInterfaceToIPAddress@12"); p_PfBindInterfaceToIPAddress = (void *)GetProcAddress(m, buf);
    sprintf(buf, "_PfRebindFilters@8"); p_PfRebindFilters = (void *)GetProcAddress(m, buf);
    sprintf(buf, "_PfAddGlobalFilterToInterface@8"); p_PfAddGlobalFilterToInterface = (void *)GetProcAddress(m, buf);
    sprintf(buf, "_PfRemoveGlobalFilterFromInterface@8"); p_PfRemoveGlobalFilterFromInterface = (void *)GetProcAddress(m, buf);
    sprintf(buf, "_PfMakeLog@4"); p_PfMakeLog = (void *)GetProcAddress(m, buf);
    sprintf(buf, "_PfSetLogBuffer@28"); p_PfSetLogBuffer = (void *)GetProcAddress(m, buf);
    sprintf(buf, "_PfDeleteLog@0"); p_PfDeleteLog = (void *)GetProcAddress(m, buf);
    sprintf(buf, "_PfGetInterfaceStatistics@16"); p_PfGetInterfaceStatistics = (void *)GetProcAddress(m, buf);
    sprintf(buf, "_PfTestPacket@20"); p_PfTestPacket = (void *)GetProcAddress(m, buf);
}

static int failures;
static void expect(DWORD got, DWORD want, const char *what)
{
    int ok = got == want;
    printf("%s  %s (%lu, wanted %lu)\n", ok ? "PASS" : "FAIL", what, got, want);
    fflush(stdout);
    if (!ok) failures++;
}

static BYTE any[4] = {0, 0, 0, 0}, host[4] = {255, 255, 255, 255}, lo[4] = {127, 0, 0, 1}, net10[4] = {10, 0, 0, 0}, m8[4] = {255, 0, 0, 0};

static PF_FILTER_DESCRIPTOR filt(DWORD proto, WORD dport, WORD dhigh, BYTE *src, BYTE *smask)
{
    PF_FILTER_DESCRIPTOR d = {0};
    d.pfatType = PF_IPV4;
    d.SrcAddr = src; d.SrcMask = smask;
    d.DstAddr = any; d.DstMask = any;
    d.dwProtocol = proto;
    d.wDstPort = dport; d.wDstPortHighRange = dhigh;
    return d;
}

static int rule_count(void)
{
    HKEY key;
    DWORD n = 0, i, len;
    WCHAR name[128];
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"System\\CurrentControlSet\\Services\\SharedAccess\\Parameters\\FirewallPolicy\\FirewallRules", 0, KEY_READ, &key)) return 0;
    for (i = 0; len = ARRAYSIZE(name), !RegEnumValueW(key, i, name, &len, NULL, NULL, NULL, NULL); i++)
        if (!wcsncmp(name, L"SGPF-", 5)) n++;
    RegCloseKey(key);
    return n;
}

static void api(void)
{
    /* Windows Vista and later: every Pf* call fails with the documented error (120) */
    INTERFACE_HANDLE h = (INTERFACE_HANDLE)0x1234;
    PF_FILTER_DESCRIPTOR f = filt(6, 8080, 8080, any, any);
    BYTE pkt[40] = {0x45};
    PFFORWARD_ACTION act;
    PF_LATEBIND_INFO late = {0};
    DWORD size = 0, a, b, c;
    PF_INTERFACE_STATS st;
    const DWORD NS = ERROR_CALL_NOT_IMPLEMENTED;

    expect(PfCreateInterface(0, PF_ACTION_FORWARD, PF_ACTION_FORWARD, FALSE, FALSE, &h), NS, "PfCreateInterface");
    expect(PfCreateInterface(7, PF_ACTION_DROP, PF_ACTION_DROP, TRUE, TRUE, &h), NS, "PfCreateInterface unique/log");
    expect(PfDeleteInterface(h), NS, "PfDeleteInterface");
    expect(PfBindInterfaceToIPAddress(h, PF_IPV4, lo), NS, "PfBindInterfaceToIPAddress");
    expect(PfBindInterfaceToIndex(h, 1, PF_IPV4, lo), NS, "PfBindInterfaceToIndex");
    expect(PfUnBindInterface(h), NS, "PfUnBindInterface");
    expect(PfAddFiltersToInterface(h, 1, &f, 0, NULL, NULL), NS, "PfAddFiltersToInterface in");
    expect(PfAddFiltersToInterface(h, 0, NULL, 1, &f, NULL), NS, "PfAddFiltersToInterface out");
    expect(PfAddFiltersToInterface(h, 0, NULL, 0, NULL, NULL), PFERROR_NO_FILTERS_GIVEN, "PfAddFiltersToInterface none given");
    expect(PfRemoveFiltersFromInterface(h, 1, &f, 0, NULL), NS, "PfRemoveFiltersFromInterface");
    expect(PfRemoveFilterHandles(h, 0, NULL), NS, "PfRemoveFilterHandles");
    expect(PfRebindFilters(h, &late), NS, "PfRebindFilters");
    expect(PfAddGlobalFilterToInterface(h, GF_FRAGMENTS), NS, "PfAddGlobalFilterToInterface");
    expect(PfRemoveGlobalFilterFromInterface(h, GF_FRAGMENTS), NS, "PfRemoveGlobalFilterFromInterface");
    expect(PfGetInterfaceStatistics(h, &st, &size, FALSE), NS, "PfGetInterfaceStatistics");
    expect(PfTestPacket(h, NULL, sizeof(pkt), pkt, &act), NS, "PfTestPacket");
    expect(PfMakeLog(NULL), NS, "PfMakeLog");
    expect(PfSetLogBuffer(pkt, sizeof(pkt), 1, 1, &a, &b, &c), NS, "PfSetLogBuffer");
    expect(PfDeleteLog(), NS, "PfDeleteLog");
    /* nothing was created or enforced: no firewall rule, no request to the firewall */
    expect(rule_count(), 0, "no firewall rule exists");
}

static void icmp6(void)
{
    struct sockaddr_in6 src = {0}, dst = {0};
    HANDLE h = Icmp6CreateFile();
    BYTE reply[256], data[32] = "stained glass echo";
    DWORD n, status, i;
    WSADATA wsa;

    WSAStartup(MAKEWORD(2, 2), &wsa);
    expect(h != INVALID_HANDLE_VALUE, 1, "Icmp6CreateFile");
    src.sin6_family = AF_INET6;
    dst.sin6_family = AF_INET6;
    dst.sin6_addr.s6_addr[15] = 1;   /* ::1 */
    SetLastError(0);
    expect(Icmp6SendEcho2(h, NULL, NULL, NULL, &src, &dst, data, sizeof(data), NULL, reply, 10, 1000), 0, "a reply buffer too small");
    expect(GetLastError(), IP_BUF_TOO_SMALL, "...says IP_BUF_TOO_SMALL");
    expect(Icmp6SendEcho2(h, NULL, NULL, NULL, &src, NULL, data, sizeof(data), NULL, reply, sizeof(reply), 1000), 0, "no destination");
    memset(&dst.sin6_addr, 0, 16);
    SetLastError(0);
    expect(Icmp6SendEcho2(h, NULL, NULL, NULL, &src, &dst, data, sizeof(data), NULL, reply, sizeof(reply), 1000), 0, "the unspecified address");
    dst.sin6_addr.s6_addr[15] = 1;
    memset(reply, 0xcc, sizeof(reply));
    n = Icmp6SendEcho2(h, NULL, NULL, NULL, &src, &dst, data, sizeof(data), NULL, reply, sizeof(reply), 2000);
    if (!n && (GetLastError() == ERROR_ACCESS_DENIED || GetLastError() == ERROR_INVALID_PARAMETER || GetLastError() == IP_GENERAL_FAILURE))
        printf("SKIP  no ICMPv6 loopback here (%lu)\n", GetLastError());
    else
    {
        expect(n, 1, "ping ::1");
        memcpy(&status, reply + 28, 4);
        expect(status, IP_SUCCESS, "status IP_SUCCESS");
        for (i = 0; i < 26 && !reply[i]; i++) ;
        expect(i < 26, 1, "the sender is in the reply");
        expect(reply[6 + 15], 1, "the sender is ::1");
        expect(memcmp(reply + 36, data, sizeof(data)) == 0, 1, "the echoed data follows the structure");
    }
    /* an address nobody has: 2001:db8::1 is documentation space, no route here or it times out */
    IcmpCloseHandle(h);
}

int main(int argc, char **argv)
{
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
    load_pf();
    if (argc > 1 && !strcmp(argv[1], "api")) api();
    else if (argc > 1 && !strcmp(argv[1], "icmp6")) icmp6();
    else return 2;
    printf(failures ? "RESULT: FAIL\n" : "RESULT: PASS\n");
    return failures != 0;
}
