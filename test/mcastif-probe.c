/* IP_MULTICAST_IF given an interface index (patches/sg/0639): Windows takes
 * an address in 0.x.x.x as an interface index in network byte order, as
 * Zeroconf's mDNS passes it (DYMO Connect's printer discovery); Wine gave
 * "Invalid argument" and the DYMO web service stopped. Prints key=value
 * lines for test/mcastif-gate.sh:
 *   index=<the loopback interface's index>
 *   byindex=<0, or the WSA error, setting it by index>
 *   byaddress=<setting it by 127.0.0.1>
 *   nosuch=<setting it to an index no interface has>
 */
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <stdio.h>
#include <stdlib.h>

static int set_if(SOCKET s, DWORD value)
{
    return setsockopt(s, IPPROTO_IP, IP_MULTICAST_IF, (const char *)&value, sizeof(value)) ? WSAGetLastError() : 0;
}

int main(void)
{
    WSADATA wsa;
    MIB_IPADDRTABLE *table;
    ULONG size = 0, i, index = 0;
    SOCKET s;

    WSAStartup(MAKEWORD(2, 2), &wsa);
    GetIpAddrTable(NULL, &size, FALSE);
    table = malloc(size);
    if (!GetIpAddrTable(table, &size, FALSE))
        for (i = 0; i < table->dwNumEntries; i++)
            if (table->table[i].dwAddr == htonl(INADDR_LOOPBACK)) index = table->table[i].dwIndex;
    printf("index=%lu\n", index);
    s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    printf("byindex=%d\n", set_if(s, htonl(index)));
    printf("byaddress=%d\n", set_if(s, htonl(INADDR_LOOPBACK)));
    printf("nosuch=%d\n", set_if(s, htonl(0x00fffff0)));
    closesocket(s);
    return 0;
}
