/* SIO_TCP_INITIAL_RTO (wine-sg 8900; test/tcprto-gate.sh). Three TCP sockets get MaxSynRetransmissions
 * 3, TCP_INITIAL_RTO_NO_SYN_RETRANSMISSIONS and 9; the gate reads Linux's TCP_SYNCNT of the process's
 * sockets while the probe waits (READY), then creates the file named by argv[1] to let it finish. */
#include <winsock2.h>
#include <windows.h>
#include <mstcpip.h>
#include <stdio.h>

#ifndef SIO_TCP_INITIAL_RTO
#define SIO_TCP_INITIAL_RTO _WSAIOW(IOC_VENDOR, 17)
#endif

static int failures;
static void check(int ok, const char *what)
{
    printf("%s %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) failures++;
}

static int set_rto(SOCKET s, USHORT rtt, UCHAR syn, DWORD size)
{
    TCP_INITIAL_RTO_PARAMETERS p = { rtt, syn };
    DWORD ret = 0;
    return WSAIoctl(s, SIO_TCP_INITIAL_RTO, &p, size, NULL, 0, &ret, NULL, NULL);
}

int main(int argc, char **argv)
{
    WSADATA wsa;
    SOCKET s[3], u;
    int i;

    setvbuf(stdout, NULL, _IONBF, 0);
    WSAStartup(MAKEWORD(2, 2), &wsa);
    for (i = 0; i < 3; i++) s[i] = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    check(!set_rto(s[0], TCP_INITIAL_RTO_UNSPECIFIED_RTT, 3, sizeof(TCP_INITIAL_RTO_PARAMETERS)),
          "SIO_TCP_INITIAL_RTO MaxSynRetransmissions 3 on a TCP socket succeeds");
    check(!set_rto(s[1], 0, TCP_INITIAL_RTO_NO_SYN_RETRANSMISSIONS,
                   sizeof(TCP_INITIAL_RTO_PARAMETERS)),
          "SIO_TCP_INITIAL_RTO no SYN retransmissions (what libuv sets for loopback connects) succeeds");
    check(!set_rto(s[2], 3000, 9, sizeof(TCP_INITIAL_RTO_PARAMETERS)), "SIO_TCP_INITIAL_RTO Rtt 3000 ms, 9 SYNs succeeds");
    check(!set_rto(s[2], TCP_INITIAL_RTO_UNSPECIFIED_RTT, TCP_INITIAL_RTO_UNSPECIFIED_MAX_SYN_RETRANSMISSIONS,
                   sizeof(TCP_INITIAL_RTO_PARAMETERS)), "unspecified values succeed (and change nothing)");
    check(set_rto(s[2], 0, 3, 2) == SOCKET_ERROR, "a buffer smaller than TCP_INITIAL_RTO_PARAMETERS fails");
    u = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    check(set_rto(u, TCP_INITIAL_RTO_UNSPECIFIED_RTT, 3, sizeof(TCP_INITIAL_RTO_PARAMETERS)) == SOCKET_ERROR,
          "a UDP socket refuses SIO_TCP_INITIAL_RTO");
    printf("READY\n");
    if (argc > 1)
        for (i = 0; i < 600 && GetFileAttributesA(argv[1]) == INVALID_FILE_ATTRIBUTES; i++) Sleep(100);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
