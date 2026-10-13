/* wsock32 (patch 3589): WSARecvEx reports a truncated datagram as partial; GetAddressByName resolves through the
 * Windows Sockets 2 name-space calls. */
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <nspapi.h>
#include <stdio.h>
#include <string.h>

static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL  line %d  ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

int main(void)
{
    WSADATA wd;
    HMODULE m = LoadLibraryA("wsock32.dll");
    int (WINAPI *recvex)(SOCKET, char *, int, int *) = (void *)GetProcAddress(m, "WSARecvEx");
    int (WINAPI *getaddr)(DWORD, GUID *, char *, int *, DWORD, void *, void *, DWORD *, char *, DWORD *) =
        (void *)GetProcAddress(m, "GetAddressByNameA");
    SOCKET a, b;
    struct sockaddr_in addr = {0};
    int len = sizeof(addr), flags, ret;
    char buf[200], data[100];
    DWORD size, alias_len = 0;
    INT protocols[2] = { IPPROTO_TCP, 0 };

    WSAStartup(MAKEWORD(2, 2), &wd);
    CHECK(recvex && getaddr, "exports");

    a = socket(AF_INET, SOCK_DGRAM, 0);
    b = socket(AF_INET, SOCK_DGRAM, 0);
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    CHECK(!bind(a, (struct sockaddr *)&addr, sizeof(addr)), "bind");
    getsockname(a, (struct sockaddr *)&addr, &len);
    memset(data, 'x', sizeof(data));
    CHECK(sendto(b, data, 100, 0, (struct sockaddr *)&addr, sizeof(addr)) == 100, "sendto");

    flags = 0;
    ret = recvex(a, buf, 10, &flags);
    CHECK(ret == 10 && (flags & MSG_PARTIAL), "truncated datagram: ret %d flags %#x err %d", ret, flags, WSAGetLastError());

    CHECK(sendto(b, data, 100, 0, (struct sockaddr *)&addr, sizeof(addr)) == 100, "sendto 2");
    flags = 0;
    ret = recvex(a, buf, 200, &flags);
    CHECK(ret == 100 && !(flags & MSG_PARTIAL), "whole datagram: ret %d flags %#x", ret, flags);
    CHECK(recvex(a, buf, 10, NULL) == SOCKET_ERROR, "NULL flags");

    size = 0;
    ret = getaddr(NS_DNS, NULL, "localhost", protocols, RES_FIND_MULTIPLE, NULL, NULL, &size, NULL, &alias_len);
    CHECK(ret == SOCKET_ERROR && GetLastError() == ERROR_INSUFFICIENT_BUFFER && size >= sizeof(CSADDR_INFO), "size query: %d err %lu size %lu", ret, GetLastError(), size);
    if (size >= sizeof(CSADDR_INFO))
    {
        CSADDR_INFO *csa = calloc(1, size);
        ret = getaddr(NS_DNS, NULL, "localhost", protocols, RES_FIND_MULTIPLE, NULL, csa, &size, NULL, &alias_len);
        CHECK(ret >= 1, "lookup: %d err %lu", ret, GetLastError());
        if (ret >= 1)
        {
            int found = 0, i;
            for (i = 0; i < ret; i++)
                if (csa[i].RemoteAddr.lpSockaddr && csa[i].RemoteAddr.lpSockaddr->sa_family == AF_INET)
                    found = ((struct sockaddr_in *)csa[i].RemoteAddr.lpSockaddr)->sin_addr.s_addr == htonl(INADDR_LOOPBACK);
            CHECK(found, "127.0.0.1 among the answers");
            CHECK((BYTE *)csa[0].RemoteAddr.lpSockaddr >= (BYTE *)(csa + ret), "addresses are packed behind the structures");
        }
        free(csa);
    }
    CHECK(getaddr(NS_DNS, NULL, "no-such-host.invalid", protocols, 0, NULL, buf, &(DWORD){200}, NULL, &alias_len) == SOCKET_ERROR, "unknown host");

    printf("RESULT: %s\n", fails ? "FAIL" : "PASS");
    return fails != 0;
}
