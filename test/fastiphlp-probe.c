/* iphlpapi lease/state/forwarding/TCP/proxy-ARP calls (patch 3179), run by
 * test/fastiphlp-gate.sh against a stand-in sg-netctl that logs its requests.
 *   fastiphlp-probe.exe admin      every call, as an administrator, answered OK
 *   fastiphlp-probe.exe nonadmin   the same from a thread without Administrators
 *   fastiphlp-probe.exe reply N    one call (SetIpTTL), expected to give error N */
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iphlpapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;
static void expect(DWORD got, DWORD want, const char *what)
{
    int ok = got == want;
    printf("%s  %s (%lu, wanted %lu)\n", ok ? "PASS" : "FAIL", what, got, want);
    fflush(stdout);
    if (!ok) failures++;
}

static HANDLE restricted_thread_token(void)
{
    HANDLE proc, restricted, imp = NULL;
    SID_IDENTIFIER_AUTHORITY nt = { SECURITY_NT_AUTHORITY };
    PSID admins;
    SID_AND_ATTRIBUTES sa;

    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ALL_ACCESS, &proc)) return NULL;
    AllocateAndInitializeSid(&nt, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &admins);
    sa.Sid = admins; sa.Attributes = 0;
    if (CreateRestrictedToken(proc, DISABLE_MAX_PRIVILEGE, 1, &sa, 0, NULL, 0, NULL, &restricted))
    {
        DuplicateTokenEx(restricted, TOKEN_ALL_ACCESS, NULL, SecurityImpersonation, TokenImpersonation, &imp);
        CloseHandle(restricted);
    }
    CloseHandle(proc);
    FreeSid(admins);
    return imp;
}

static void sequence(int admin)
{
    DWORD ok = admin ? NO_ERROR : ERROR_ACCESS_DENIED;
    IP_ADAPTER_INDEX_MAP map = { 1 };
    MIB_IFROW ifrow = { .dwIndex = 1 };
    MIB_IPSTATS stats = { .dwForwarding = MIB_IP_FORWARDING, .dwDefaultTTL = 77 };
    MIB_TCPROW tcp = { .dwState = MIB_TCP_STATE_DELETE_TCB };
    OVERLAPPED o1 = {0}, o2 = {0};
    HANDLE h1 = NULL, h2 = NULL;
    DWORD count = 99;
    BYTE rw[4] = {0};

    expect(IpReleaseAddress(&map), ok, "IpReleaseAddress");
    expect(IpRenewAddress(&map), ok, "IpRenewAddress");
    expect(IpReleaseAddress(NULL), ERROR_INVALID_PARAMETER, "IpReleaseAddress(NULL)");
    ifrow.dwAdminStatus = MIB_IF_ADMIN_STATUS_DOWN;
    expect(SetIfEntry(&ifrow), ok, "SetIfEntry down");
    ifrow.dwAdminStatus = MIB_IF_ADMIN_STATUS_UP;
    expect(SetIfEntry(&ifrow), ok, "SetIfEntry up");
    ifrow.dwAdminStatus = 9;
    expect(SetIfEntry(&ifrow), ERROR_INVALID_PARAMETER, "SetIfEntry bad status");
    ifrow.dwAdminStatus = MIB_IF_ADMIN_STATUS_TESTING;
    expect(SetIfEntry(&ifrow), ERROR_NOT_SUPPORTED, "SetIfEntry testing");

    o1.hEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
    o2.hEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
    expect(EnableRouter(NULL, &o1), ERROR_INVALID_PARAMETER, "EnableRouter(NULL handle)");
    expect(EnableRouter(&h1, &o1), admin ? ERROR_IO_PENDING : ERROR_ACCESS_DENIED, "EnableRouter");
    expect(EnableRouter(&h2, &o2), admin ? ERROR_IO_PENDING : ERROR_ACCESS_DENIED, "EnableRouter again");
    expect(EnableRouter(&h2, &o2), admin ? ERROR_INVALID_PARAMETER : ERROR_ACCESS_DENIED, "EnableRouter, same structure twice");
    if (admin)
    {
        expect(UnenableRouter(&o1, &count), NO_ERROR, "UnenableRouter (one left)");
        expect(count, 1, "enable count");
        expect(WaitForSingleObject(o1.hEvent, 0), WAIT_OBJECT_0, "the pending request completes");
        expect(UnenableRouter(&o1, &count), ERROR_INVALID_PARAMETER, "UnenableRouter, unknown structure");
        expect(UnenableRouter(&o2, &count), NO_ERROR, "UnenableRouter (last)");
        expect(count, 0, "enable count 0");
    }
    else expect(UnenableRouter(&o1, &count), ERROR_ACCESS_DENIED, "UnenableRouter");

    expect(SetIpStatistics(&stats), ok, "SetIpStatistics");
    stats.dwDefaultTTL = 0;
    expect(SetIpStatistics(&stats), ERROR_INVALID_PARAMETER, "SetIpStatistics TTL 0");
    expect(SetIpTTL(64), ok, "SetIpTTL");
    expect(SetIpTTL(0), ERROR_INVALID_PARAMETER, "SetIpTTL(0)");
    expect(SetIpTTL(256), ERROR_INVALID_PARAMETER, "SetIpTTL(256)");

    {
        SOCKET ls = socket(AF_INET, SOCK_STREAM, 0), cs = socket(AF_INET, SOCK_STREAM, 0), as;
        struct sockaddr_in a = { AF_INET }, b = { AF_INET };
        int len = sizeof(a), len2 = sizeof(b);
        MIB_TCPROW none = { .dwState = MIB_TCP_STATE_DELETE_TCB };
        MIB_TCPROW zero = {0};

        a.sin_addr.s_addr = inet_addr("127.0.0.1");
        bind(ls, (struct sockaddr *)&a, sizeof(a)); listen(ls, 1); getsockname(ls, (struct sockaddr *)&a, &len);
        connect(cs, (struct sockaddr *)&a, sizeof(a)); as = accept(ls, NULL, NULL);
        getsockname(cs, (struct sockaddr *)&b, &len2);
        tcp.dwLocalAddr = b.sin_addr.s_addr; tcp.dwLocalPort = b.sin_port;
        tcp.dwRemoteAddr = a.sin_addr.s_addr; tcp.dwRemotePort = a.sin_port;
        expect(SetTcpEntry(&tcp), admin ? ok : ERROR_NETWORK_ACCESS_DENIED, "SetTcpEntry delete TCB of a live connection");
        expect(SetTcpEntry(&none), admin ? ERROR_MR_MID_NOT_FOUND : ERROR_NETWORK_ACCESS_DENIED, "SetTcpEntry of no connection");
        expect(SetTcpEntry(&zero), admin ? ERROR_INVALID_PARAMETER : ERROR_NETWORK_ACCESS_DENIED, "SetTcpEntry, state 0");
        tcp.dwState = MIB_TCP_STATE_ESTAB;
        expect(SetTcpEntry(&tcp), admin ? ERROR_INVALID_PARAMETER : ERROR_NETWORK_ACCESS_DENIED, "SetTcpEntry other state");
        closesocket(as); closesocket(cs); closesocket(ls);
    }
    expect(SetPerTcpConnectionEStats(&tcp, TcpConnectionEstatsSynOpts, rw, 0, sizeof(rw), 0),
           admin ? ERROR_NOT_SUPPORTED : ERROR_ACCESS_DENIED, "SetPerTcpConnectionEStats");
    expect(GetPerTcpConnectionEStats(&tcp, TcpConnectionEstatsSynOpts, NULL, 0, 0, NULL, 0, 0, NULL, 0, 0),
           admin ? ERROR_NOT_SUPPORTED : ERROR_ACCESS_DENIED, "GetPerTcpConnectionEStats");
    expect(SetPerTcpConnectionEStats(&tcp, 99, rw, 0, sizeof(rw), 0), ERROR_INVALID_PARAMETER, "SetPerTcpConnectionEStats bad type");

    expect(CreateProxyArpEntry(inet_addr("10.9.8.20"), inet_addr("255.255.255.0"), 1), ok, "CreateProxyArpEntry");
    expect(DeleteProxyArpEntry(inet_addr("10.9.8.20"), inet_addr("255.255.255.0"), 1), ok, "DeleteProxyArpEntry");
    expect(CreateProxyArpEntry(0, 0, 1), ERROR_INVALID_PARAMETER, "CreateProxyArpEntry(0)");
}

#include <ws2ipdef.h>
typedef struct { int Format; union { struct { WCHAR Address[256]; WCHAR Port[6]; } n; SOCKADDR_IN v4; SOCKADDR_IN6 v6; } u; } NET_ADDRESS_INFO;
#ifndef NET_STRING_IPV4_NETWORK
#define NET_STRING_IPV4_NETWORK 4
#define NET_STRING_IPV6_NETWORK 0x80
#define NET_STRING_IPV6_ADDRESS_NO_SCOPE 0x10
#define NET_STRING_NAMED_ADDRESS 0x100
#define NET_STRING_NAMED_SERVICE 0x200
#endif
#define NET_ADDRESS_IPV4 2
#define NET_ADDRESS_IPV6 3
#define NET_ADDRESS_DNS_NAME 1
static void parse(void)
{
    typedef DWORD (WINAPI *parse_t)(const WCHAR *, DWORD, NET_ADDRESS_INFO *, USHORT *, BYTE *);
    parse_t pParse = (parse_t)GetProcAddress(LoadLibraryA("iphlpapi.dll"), "ParseNetworkString");
    NET_ADDRESS_INFO info;
    USHORT port = 99;
    BYTE plen = 0;
    static const struct { const WCHAR *s; DWORD type; DWORD ret; int format; int port; int plen; } t[] = {
        { L"10.1.0.0/16", NET_STRING_IPV4_NETWORK, 0, NET_ADDRESS_IPV4, 0, 16 },
        { L"10.1.0.0/33", NET_STRING_IPV4_NETWORK, ERROR_INVALID_PARAMETER, 0, 0, 0 },
        { L"10.1.0.0",    NET_STRING_IPV4_NETWORK, ERROR_INVALID_PARAMETER, 0, 0, 0 },
        { L"fe80::/10",   NET_STRING_IPV6_NETWORK, 0, NET_ADDRESS_IPV6, 0, 10 },
        { L"fe80::/129",  NET_STRING_IPV6_NETWORK, ERROR_INVALID_PARAMETER, 0, 0, 0 },
        { L"::1",         NET_STRING_IPV6_ADDRESS_NO_SCOPE, 0, NET_ADDRESS_IPV6, 0, 255 },
        { L"fe80::1%3",   NET_STRING_IPV6_ADDRESS_NO_SCOPE, ERROR_INVALID_PARAMETER, 0, 0, 0 },
        { L"winehq.org",  NET_STRING_NAMED_ADDRESS, 0, NET_ADDRESS_DNS_NAME, 0, 255 },
        { L"winehq.org:443", NET_STRING_NAMED_SERVICE, 0, NET_ADDRESS_DNS_NAME, 443, 255 },
        { L"winehq.org:443", NET_STRING_NAMED_ADDRESS, ERROR_INVALID_PARAMETER, 0, 0, 0 },
        { L"winehq.org",  NET_STRING_NAMED_SERVICE, ERROR_INVALID_PARAMETER, 0, 0, 0 },
        { L"bad name",    NET_STRING_NAMED_ADDRESS, ERROR_INVALID_PARAMETER, 0, 0, 0 },
        { L"a..b",        NET_STRING_NAMED_ADDRESS, ERROR_INVALID_PARAMETER, 0, 0, 0 },
        { L"host:70000",  NET_STRING_NAMED_SERVICE, ERROR_INVALID_PARAMETER, 0, 0, 0 },
    };
    unsigned i;
    for (i = 0; i < ARRAYSIZE(t); i++)
    {
        char msg[100];
        DWORD ret;
        memset(&info, 0, sizeof(info)); port = 99; plen = 0;
        ret = pParse(t[i].s, t[i].type, &info, &port, &plen);
        sprintf(msg, "ParseNetworkString %ls type %lx", t[i].s, t[i].type);
        expect(ret, t[i].ret, msg);
        if (!t[i].ret)
        {
            expect(info.Format, t[i].format, "format");
            expect(port, t[i].port, "port");
            expect(plen, t[i].plen, "prefix length");
        }
    }
}

int main(int argc, char **argv)
{
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
    if (argc > 1 && !strcmp(argv[1], "admin")) sequence(1);
    else if (argc > 1 && !strcmp(argv[1], "nonadmin"))
    {
        HANDLE imp = restricted_thread_token();
        expect(imp && SetThreadToken(NULL, imp), 1, "a thread without the Administrators group");
        sequence(0);
    }
    else if (argc > 1 && !strcmp(argv[1], "parse")) parse();
    else if (argc > 2 && !strcmp(argv[1], "reply")) expect(SetIpTTL(64), atoi(argv[2]), "SetIpTTL answered by sg-netctl");
    else return 2;
    printf(failures ? "RESULT: FAIL\n" : "RESULT: PASS\n");
    return failures != 0;
}
