/* getsockopt whose answer the unix side fills in without a server call: the length it returns must be the
 * size of the answer, in 32-bit as in 64-bit programs (wow64 copies the status block back only when the
 * status was written). */
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h>
#include <string.h>

static int failures;
#define CHECK(c, ...) do { if (!(c)) { failures++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

int main(void)
{
    WSADATA wsa;
    SOCKET s, s6;
    struct linger lg;
    int val, size, ret;
    DWORD dv;

    WSAStartup(MAKEWORD(2, 2), &wsa);
    s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    CHECK(s != INVALID_SOCKET, "socket");

    /* SO_LINGER: struct linger is 4 bytes on Windows; ask with room for 8 and with exactly 4 */
    lg.l_onoff = 1; lg.l_linger = 7;
    ret = setsockopt(s, SOL_SOCKET, SO_LINGER, (char *)&lg, sizeof(lg));
    CHECK(!ret, "setsockopt linger %d", WSAGetLastError());
    {
        char buf[8];
        memset(buf, 0xcc, sizeof(buf));
        size = sizeof(buf);
        ret = getsockopt(s, SOL_SOCKET, SO_LINGER, buf, &size);
        CHECK(!ret, "getsockopt linger %d", WSAGetLastError());
        CHECK(size == 4, "linger size %d", size);
        CHECK(((struct linger *)buf)->l_onoff == 1 && ((struct linger *)buf)->l_linger == 7, "linger value");
        size = sizeof(lg);
        memset(&lg, 0, sizeof(lg));
        ret = getsockopt(s, SOL_SOCKET, SO_LINGER, (char *)&lg, &size);
        CHECK(!ret && size == 4 && lg.l_onoff == 1 && lg.l_linger == 7, "linger exact %d size %d", ret, size);
    }

    /* IP_DONTFRAGMENT and IPV6_DONTFRAG: the length is what the option occupies */
    s6 = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
    if (s6 != INVALID_SOCKET)
    {
        dv = 1;
        ret = setsockopt(s6, IPPROTO_IPV6, IPV6_DONTFRAG, (char *)&dv, sizeof(dv));
        CHECK(!ret, "set IPV6_DONTFRAG %d", WSAGetLastError());
        val = 0x55555555; size = 8;
        ret = getsockopt(s6, IPPROTO_IPV6, IPV6_DONTFRAG, (char *)&val, &size);
        CHECK(!ret, "get IPV6_DONTFRAG %d", WSAGetLastError());
        CHECK(size == 4, "IPV6_DONTFRAG size %d", size);
        closesocket(s6);
    }
    else printf("note: no IPv6 socket\n");
    {
        SOCKET su = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        dv = 1;
        ret = setsockopt(su, IPPROTO_IP, IP_DONTFRAGMENT, (char *)&dv, sizeof(dv));
        CHECK(!ret, "set IP_DONTFRAGMENT %d", WSAGetLastError());
        val = 0x55555555; size = 8;
        ret = getsockopt(su, IPPROTO_IP, IP_DONTFRAGMENT, (char *)&val, &size);
        CHECK(!ret, "get IP_DONTFRAGMENT %d", WSAGetLastError());
        CHECK(size == 4, "IP_DONTFRAGMENT size %d", size);
        closesocket(su);
    }
    closesocket(s);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
