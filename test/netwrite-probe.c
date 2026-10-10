/* iphlpapi's calls that change the network (patches/sg/2416), run by
 * test/netwrite-gate.sh against a stand-in sg-netctl that logs what it is asked
 * and answers as the gate says (SG_NETCTL_MODE).
 *
 *   netwrite-probe.exe admin        every call, as an administrator, answered OK
 *   netwrite-probe.exe nonadmin     the same from a thread without the Administrators group
 *   netwrite-probe.exe reply NEW OLD
 *                                   one new-style and one old-style call, expected
 *                                   to give the errors NEW and OLD (the stand-in's answer) */
#include <winsock2.h>
#include <ws2ipdef.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iphlpapi.h>
#include <netioapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}
static void expect(DWORD got, DWORD want, const char *what)
{
    char msg[200];
    sprintf(msg, "%s (%lu, wanted %lu)", what, got, want);
    check(got == want, msg);
}

static void v4(SOCKADDR_INET *a, const char *text)
{
    memset(a, 0, sizeof(*a));
    a->Ipv4.sin_family = AF_INET;
    InetPtonA(AF_INET, text, &a->Ipv4.sin_addr);
}
static void v6(SOCKADDR_INET *a, const char *text)
{
    memset(a, 0, sizeof(*a));
    a->Ipv6.sin6_family = AF_INET6;
    InetPtonA(AF_INET6, text, &a->Ipv6.sin6_addr);
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

static MIB_UNICASTIPADDRESS_ROW unicast4(const char *addr, UINT8 prefix)
{
    MIB_UNICASTIPADDRESS_ROW r;
    InitializeUnicastIpAddressEntry(&r);
    r.InterfaceIndex = 1;
    v4(&r.Address, addr);
    r.OnLinkPrefixLength = prefix;
    return r;
}

static MIB_IPFORWARD_ROW2 route4(const char *dest, UINT8 prefix, const char *hop, ULONG metric)
{
    MIB_IPFORWARD_ROW2 r;
    InitializeIpForwardEntry(&r);
    r.InterfaceIndex = 1;
    v4(&r.DestinationPrefix.Prefix, dest);
    r.DestinationPrefix.PrefixLength = prefix;
    if (hop) v4(&r.NextHop, hop);
    r.Metric = metric;
    return r;
}

static MIB_IPNET_ROW2 neigh4(const char *addr, int permanent)
{
    MIB_IPNET_ROW2 r;
    static const BYTE mac[6] = { 0, 0x11, 0x22, 0x33, 0x44, 0x55 };
    memset(&r, 0, sizeof(r));
    r.InterfaceIndex = 1;
    v4(&r.Address, addr);
    memcpy(r.PhysicalAddress, mac, 6);
    r.PhysicalAddressLength = 6;
    r.State = permanent ? NlnsPermanent : NlnsReachable;
    return r;
}

static void sequence(int admin)
{
    MIB_UNICASTIPADDRESS_ROW u;
    MIB_IPFORWARD_ROW2 f;
    MIB_IPNET_ROW2 n;
    MIB_ANYCASTIPADDRESS_ROW any;
    MIB_IPFORWARDROW old;
    MIB_IPNETROW oldn;
    ULONG ctx = 0, inst = 0;
    DWORD ok = admin ? NO_ERROR : ERROR_ACCESS_DENIED;
    DWORD want_new = admin ? NO_ERROR : ERROR_ACCESS_DENIED;

    u = unicast4("10.9.8.7", 24);
    expect(CreateUnicastIpAddressEntry(&u), want_new, "CreateUnicastIpAddressEntry 10.9.8.7/24");
    u = unicast4("10.9.8.7", 255);
    expect(CreateUnicastIpAddressEntry(&u), want_new, "with the default prefix: the class's");
    { MIB_UNICASTIPADDRESS_ROW r6; InitializeUnicastIpAddressEntry(&r6); r6.InterfaceIndex = 1; v6(&r6.Address, "fd00::5");
      r6.OnLinkPrefixLength = 64; expect(CreateUnicastIpAddressEntry(&r6), want_new, "CreateUnicastIpAddressEntry fd00::5/64"); }
    u = unicast4("10.9.8.7", 24);
    expect(DeleteUnicastIpAddressEntry(&u), want_new, "DeleteUnicastIpAddressEntry");
    u = unicast4("10.9.8.250", 24);
    expect(SetUnicastIpAddressEntry(&u), admin ? ERROR_NOT_FOUND : ERROR_ACCESS_DENIED, "SetUnicastIpAddressEntry on an address that is not there");

    f = route4("10.20.0.0", 16, "10.9.8.1", 5);
    expect(CreateIpForwardEntry2(&f), want_new, "CreateIpForwardEntry2");
    expect(DeleteIpForwardEntry2(&f), want_new, "DeleteIpForwardEntry2");
    f.Metric = 7;
    expect(SetIpForwardEntry2(&f), want_new, "SetIpForwardEntry2");
    memset(&old, 0, sizeof(old));
    old.dwForwardDest = inet_addr("10.30.0.0"); old.dwForwardMask = inet_addr("255.255.0.0");
    old.dwForwardNextHop = inet_addr("10.9.8.1"); old.dwForwardIfIndex = 1; old.dwForwardMetric1 = 3;
    expect(CreateIpForwardEntry(&old), ok, "CreateIpForwardEntry");
    expect(DeleteIpForwardEntry(&old), ok, "DeleteIpForwardEntry");
    expect(SetIpForwardEntry(&old), ok, "SetIpForwardEntry");

    n = neigh4("10.9.8.50", 1);
    expect(CreateIpNetEntry2(&n), want_new, "CreateIpNetEntry2 (permanent)");
    n = neigh4("10.9.8.52", 0);
    expect(SetIpNetEntry2(&n), want_new, "SetIpNetEntry2");
    expect(DeleteIpNetEntry2(&n), want_new, "DeleteIpNetEntry2");
    expect(FlushIpNetTable2(AF_INET, 1), want_new, "FlushIpNetTable2 (IPv4, lo)");
    expect(FlushIpNetTable2(AF_UNSPEC, 0), want_new, "FlushIpNetTable2 (all)");
    expect(FlushIpNetTable(1), ok, "FlushIpNetTable");
    memset(&oldn, 0, sizeof(oldn));
    oldn.dwIndex = 1; oldn.dwPhysAddrLen = 6; memcpy(oldn.bPhysAddr, "\xaa\xbb\xcc\xdd\xee\xff", 6);
    oldn.dwAddr = inet_addr("10.9.8.51"); oldn.dwType = MIB_IPNET_TYPE_STATIC;
    expect(CreateIpNetEntry(&oldn), ok, "CreateIpNetEntry");
    expect(DeleteIpNetEntry(&oldn), ok, "DeleteIpNetEntry");

    expect(AddIPAddress(inet_addr("10.9.8.9"), inet_addr("255.255.255.0"), 1, &ctx, &inst), ok, "AddIPAddress");
    if (admin)
    {
        check(ctx != 0, "and it hands out a context");
        expect(DeleteIPAddress(ctx), NO_ERROR, "DeleteIPAddress with that context");
        expect(DeleteIPAddress(ctx), ERROR_INVALID_PARAMETER, "and a second time: unknown context");
    }
    memset(&any, 0, sizeof(any));
    any.InterfaceIndex = 1;
    v4(&any.Address, "10.9.8.77");
    expect(CreateAnycastIpAddressEntry(&any), admin ? ERROR_NOT_SUPPORTED : ERROR_ACCESS_DENIED, "CreateAnycastIpAddressEntry");
    expect(FlushIpPathTable(AF_INET), ok, "FlushIpPathTable");

    if (admin)
    {
        /* bad arguments are refused before sg-netctl is asked */
        u = unicast4("10.9.8.7", 33);
        expect(CreateUnicastIpAddressEntry(&u), ERROR_INVALID_PARAMETER, "a prefix of 33 bits");
        u = unicast4("224.0.0.9", 24);
        expect(CreateUnicastIpAddressEntry(&u), ERROR_INVALID_PARAMETER, "a multicast address");
        u = unicast4("0.0.0.0", 24);
        expect(CreateUnicastIpAddressEntry(&u), ERROR_INVALID_PARAMETER, "an unspecified address");
        u = unicast4("10.9.8.7", 24);
        u.InterfaceIndex = 0;
        expect(CreateUnicastIpAddressEntry(&u), ERROR_INVALID_PARAMETER, "no interface");
        u.InterfaceIndex = 4242;
        check(CreateUnicastIpAddressEntry(&u) != NO_ERROR, "an interface that does not exist");
        f = route4("10.20.0.0", 33, NULL, 1);
        expect(CreateIpForwardEntry2(&f), ERROR_INVALID_PARAMETER, "a route prefix of 33 bits");
        f = route4("10.20.0.0", 16, NULL, 1);
        v6(&f.NextHop, "fe80::1");
        expect(CreateIpForwardEntry2(&f), ERROR_INVALID_PARAMETER, "a next hop of the other family");
        n = neigh4("10.9.8.50", 1);
        n.PhysicalAddressLength = 3;
        expect(CreateIpNetEntry2(&n), ERROR_INVALID_PARAMETER, "a hardware address of 3 bytes");
        expect(CreateUnicastIpAddressEntry(NULL), ERROR_INVALID_PARAMETER, "NULL");
        expect(FlushIpNetTable(0), ERROR_INVALID_PARAMETER, "FlushIpNetTable(0)");
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
        check(imp && SetThreadToken(NULL, imp), "a thread without the Administrators group");
        sequence(0);
    }
    else if (argc > 3 && !strcmp(argv[1], "reply"))
    {
        MIB_UNICASTIPADDRESS_ROW u = unicast4("10.9.8.7", 24);
        MIB_IPFORWARDROW old = {0};
        old.dwForwardDest = inet_addr("10.30.0.0"); old.dwForwardMask = inet_addr("255.255.0.0");
        old.dwForwardIfIndex = 1;
        expect(CreateUnicastIpAddressEntry(&u), atoi(argv[2]), "a new-style call");
        expect(CreateIpForwardEntry(&old), atoi(argv[3]), "an old-style call");
    }
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
