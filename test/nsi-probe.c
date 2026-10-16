/* nsiproxy.sys (patches/sg/5501): the IP helper tables and statistics Windows
 * programs read, printed one line per fact for test/nsi-gate.sh.
 *
 *   nsi-probe.exe stats    counters (after traffic of its own) as key=value
 *   nsi-probe.exe neigh4   IPv4 neighbours
 *   nsi-probe.exe neigh6   IPv6 neighbours
 *   nsi-probe.exe routes4  IPv4 routes
 */
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <icmpapi.h>
#include <netioapi.h>
#include <stdio.h>
#include <string.h>

static void traffic(void)
{
    SOCKET l, c, a, u1, u2;
    struct sockaddr_in addr = {0};
    int len = sizeof(addr);
    char buf[16];
    WSADATA wsa;

    WSAStartup(MAKEWORD(2, 2), &wsa);
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    l = socket(AF_INET, SOCK_STREAM, 0);
    bind(l, (struct sockaddr *)&addr, sizeof(addr));
    listen(l, 1);
    getsockname(l, (struct sockaddr *)&addr, &len);
    c = socket(AF_INET, SOCK_STREAM, 0);
    connect(c, (struct sockaddr *)&addr, sizeof(addr));
    a = accept(l, NULL, NULL);
    send(c, "hello", 5, 0);
    recv(a, buf, sizeof(buf), 0);
    closesocket(a);
    closesocket(c);
    closesocket(l);

    u1 = socket(AF_INET, SOCK_DGRAM, 0);
    u2 = socket(AF_INET, SOCK_DGRAM, 0);
    addr.sin_port = 0;
    bind(u2, (struct sockaddr *)&addr, sizeof(addr));
    len = sizeof(addr);
    getsockname(u2, (struct sockaddr *)&addr, &len);
    sendto(u1, "dgram", 5, 0, (struct sockaddr *)&addr, sizeof(addr));
    Sleep(100);
    recv(u2, buf, sizeof(buf), 0);
    closesocket(u1);
    closesocket(u2);

    /* an echo to ourselves: the ICMP counters */
    {
        HANDLE icmp = IcmpCreateFile();
        char reply[256];
        DWORD n = icmp == INVALID_HANDLE_VALUE ? 0 :
                IcmpSendEcho(icmp, htonl(INADDR_LOOPBACK), "ping", 4, NULL, reply, sizeof(reply), 2000);

        printf("icmp.echo=%lu\n", n);
        if (icmp != INVALID_HANDLE_VALUE) IcmpCloseHandle(icmp);
    }
}

static void print_mac(const BYTE *mac, ULONG len)
{
    ULONG i;

    for (i = 0; i < len; i++) printf("%s%02x", i ? ":" : "", mac[i]);
}

static int stats(void)
{
    MIB_IPSTATS ip;
    MIB_ICMP_EX icmp;
    MIB_TCPSTATS tcp;
    MIB_UDPSTATS udp;
    DWORD ret;

    traffic();
    ret = GetIpStatisticsEx(&ip, AF_INET);
    printf("ip.ret=%lu\n", ret);
    printf("ip.forwarding=%lu\nip.defaultttl=%lu\nip.inreceives=%lu\nip.indelivers=%lu\nip.outrequests=%lu\n",
           ip.dwForwarding, ip.dwDefaultTTL, ip.dwInReceives, ip.dwInDelivers, ip.dwOutRequests);
    printf("ip.numif=%lu\nip.numaddr=%lu\nip.numroutes=%lu\n", ip.dwNumIf, ip.dwNumAddr, ip.dwNumRoutes);
    ret = GetIcmpStatisticsEx(&icmp, AF_INET);
    printf("icmp.ret=%lu\nicmp.inmsgs=%lu\nicmp.outmsgs=%lu\n", ret, icmp.icmpInStats.dwMsgs, icmp.icmpOutStats.dwMsgs);
    ret = GetTcpStatisticsEx(&tcp, AF_INET);
    printf("tcp.ret=%lu\ntcp.activeopens=%lu\ntcp.passiveopens=%lu\ntcp.insegs=%lu\ntcp.outsegs=%lu\n",
           ret, tcp.dwActiveOpens, tcp.dwPassiveOpens, tcp.dwInSegs, tcp.dwOutSegs);
    ret = GetUdpStatisticsEx(&udp, AF_INET);
    printf("udp.ret=%lu\nudp.indatagrams=%lu\nudp.outdatagrams=%lu\n", ret, udp.dwInDatagrams, udp.dwOutDatagrams);
    return 0;
}

static int neigh(ADDRESS_FAMILY family)
{
    MIB_IPNET_TABLE2 *table;
    ULONG i;
    DWORD ret = GetIpNetTable2(family, &table);

    printf("neigh.ret=%lu\n", ret);
    if (ret) return 1;
    for (i = 0; i < table->NumEntries; i++)
    {
        MIB_IPNET_ROW2 *row = &table->Table[i];
        char addr[64] = "";

        if (family == AF_INET) inet_ntop(AF_INET, &row->Address.Ipv4.sin_addr, addr, sizeof(addr));
        else inet_ntop(AF_INET6, &row->Address.Ipv6.sin6_addr, addr, sizeof(addr));
        printf("n%d %s if=%lu state=%d router=%d unreach=%d mac=", family == AF_INET ? 4 : 6, addr,
               row->InterfaceIndex, row->State, row->IsRouter, row->IsUnreachable);
        print_mac(row->PhysicalAddress, row->PhysicalAddressLength);
        printf("\n");
    }
    FreeMibTable(table);
    return 0;
}

static int routes4(void)
{
    MIB_IPFORWARD_TABLE2 *table;
    ULONG i;
    DWORD ret = GetIpForwardTable2(AF_INET, &table);

    printf("routes.ret=%lu\n", ret);
    if (ret) return 1;
    for (i = 0; i < table->NumEntries; i++)
    {
        MIB_IPFORWARD_ROW2 *row = &table->Table[i];
        char prefix[64], hop[64];

        inet_ntop(AF_INET, &row->DestinationPrefix.Prefix.Ipv4.sin_addr, prefix, sizeof(prefix));
        inet_ntop(AF_INET, &row->NextHop.Ipv4.sin_addr, hop, sizeof(hop));
        printf("r4 %s/%u via %s if=%lu metric=%lu\n", prefix, row->DestinationPrefix.PrefixLength, hop,
               row->InterfaceIndex, row->Metric);
    }
    FreeMibTable(table);
    return 0;
}

int main(int argc, char **argv)
{
    if (argc > 1 && !strcmp(argv[1], "stats")) return stats();
    if (argc > 1 && !strcmp(argv[1], "neigh4")) return neigh(AF_INET);
    if (argc > 1 && !strcmp(argv[1], "neigh6")) return neigh(AF_INET6);
    if (argc > 1 && !strcmp(argv[1], "routes4")) return routes4();
    fprintf(stderr, "usage: nsi-probe stats|neigh4|neigh6|routes4\n");
    return 2;
}
