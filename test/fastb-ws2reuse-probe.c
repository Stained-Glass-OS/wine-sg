#include <winsock2.h>
#include <ws2tcpip.h>
#include <mswsock.h>
#include <stdio.h>
static int failures;
#define CHECK(c, ...) do { if (!(c)) { failures++; printf("FAIL line %d: %s: ", __LINE__, #c); printf(__VA_ARGS__); printf("\n"); } } while (0)

static LPFN_DISCONNECTEX pDisconnectEx;
static LPFN_ACCEPTEX pAcceptEx;
static LPFN_CONNECTEX pConnectEx;

int main(void)
{
    WSADATA wd; SOCKET lst, cl, srv, srv2; struct sockaddr_in a = {AF_INET, 0, {{{127,0,0,1}}}}, la;
    GUID g1 = WSAID_DISCONNECTEX, g2 = WSAID_ACCEPTEX, g3 = WSAID_CONNECTEX; DWORD n; int r, len, i;
    char buf[64]; OVERLAPPED ov; BOOL b;
    setvbuf(stdout, NULL, _IONBF, 0);
    WSAStartup(MAKEWORD(2,2), &wd);
    lst = socket(AF_INET, SOCK_STREAM, 0);
    bind(lst, (struct sockaddr *)&a, sizeof(a)); listen(lst, 5);
    len = sizeof(la); getsockname(lst, (struct sockaddr *)&la, &len);
    WSAIoctl(lst, SIO_GET_EXTENSION_FUNCTION_POINTER, &g1, sizeof(g1), &pDisconnectEx, sizeof(void*), &n, NULL, NULL);
    WSAIoctl(lst, SIO_GET_EXTENSION_FUNCTION_POINTER, &g2, sizeof(g2), &pAcceptEx, sizeof(void*), &n, NULL, NULL);
    WSAIoctl(lst, SIO_GET_EXTENSION_FUNCTION_POINTER, &g3, sizeof(g3), &pConnectEx, sizeof(void*), &n, NULL, NULL);

    /* unconnected socket: not connected */
    cl = socket(AF_INET, SOCK_STREAM, 0);
    b = pDisconnectEx(cl, NULL, TF_REUSE_SOCKET, 0);
    CHECK(!b && WSAGetLastError() == WSAENOTCONN, "unconnected reuse b=%d err=%d", b, WSAGetLastError());

    for (i = 0; i < 3; i++)
    {
        printf("iter %d\n", i);
        /* connect, reuse, reconnect on the same handle */
        printf("connect\n"); r = connect(cl, (struct sockaddr *)&la, sizeof(la));
        CHECK(!r, "connect %d err=%d", i, WSAGetLastError());
        printf("accept\n"); srv = accept(lst, NULL, NULL);
        CHECK(srv != INVALID_SOCKET, "accept");
        { u_long nb = 0; ioctlsocket(srv, FIONBIO, &nb); }
        printf("send\n"); r = send(cl, "ping", 4, 0); printf("sent %d err %d\n", r, WSAGetLastError());
        printf("srv recv\n"); r = recv(srv, buf, sizeof(buf), 0); CHECK(r == 4, "srv recv %d", r);

        memset(&ov, 0, sizeof(ov)); ov.hEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
        b = pDisconnectEx(cl, &ov, TF_REUSE_SOCKET, 0);
        CHECK(b || WSAGetLastError() == ERROR_IO_PENDING, "disconnectex b=%d err=%d", b, WSAGetLastError());
        r = WaitForSingleObject(ov.hEvent, 2000); CHECK(r == WAIT_OBJECT_0, "wait %d", r);
        CloseHandle(ov.hEvent);
        printf("recv close\n"); r = recv(srv, buf, sizeof(buf), 0);
        CHECK(r == 0, "peer sees close, got %d err=%d", r, WSAGetLastError());
        closesocket(srv);

        len = sizeof(a);
        r = getpeername(cl, (struct sockaddr *)&a, &len);
        CHECK(r == -1 && WSAGetLastError() == WSAENOTCONN, "getpeername r=%d err=%d", r, WSAGetLastError());
        len = sizeof(n); n = 0;
        r = getsockopt(cl, SOL_SOCKET, SO_CONNECT_TIME, (char *)&n, &len);
        CHECK(!r && n == ~0u, "connect time r=%d %lx", r, n);
        r = send(cl, "x", 1, 0);
        CHECK(r == -1, "send after reuse r=%d err=%d", r, WSAGetLastError());
    }

    /* the reused handle can be the target of AcceptEx */
    {
        SOCKET l2 = socket(AF_INET, SOCK_STREAM, 0), c2 = socket(AF_INET, SOCK_STREAM, 0);
        char abuf[2 * (sizeof(struct sockaddr_in) + 16)];
        struct sockaddr_in la2 = {AF_INET, 0, {{{127,0,0,1}}}};
        OVERLAPPED ao = {0};
        bind(l2, (struct sockaddr *)&la2, sizeof(la2)); listen(l2, 5);
        len = sizeof(la2); getsockname(l2, (struct sockaddr *)&la2, &len);
        srv2 = socket(AF_INET, SOCK_STREAM, 0);
        /* first accept into srv2, then reuse it and accept again */
        for (i = 0; i < 2; i++)
        {
            ao.hEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
            b = pAcceptEx(l2, srv2, abuf, 0, sizeof(struct sockaddr_in) + 16, sizeof(struct sockaddr_in) + 16, &n, &ao);
            CHECK(b || WSAGetLastError() == ERROR_IO_PENDING, "acceptex %d b=%d err=%d", i, b, WSAGetLastError());
            c2 = socket(AF_INET, SOCK_STREAM, 0);
            r = connect(c2, (struct sockaddr *)&la2, sizeof(la2)); CHECK(!r, "connect c2");
            r = WaitForSingleObject(ao.hEvent, 2000); CHECK(r == WAIT_OBJECT_0, "accept wait %d", r);
            CloseHandle(ao.hEvent);
            setsockopt(srv2, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT, (char *)&l2, sizeof(l2));
            send(c2, "hey", 3, 0);
            r = recv(srv2, buf, sizeof(buf), 0); CHECK(r == 3, "srv2 recv %d err=%d", r, WSAGetLastError());
            memset(&ov, 0, sizeof(ov));
            b = pDisconnectEx(srv2, NULL, TF_REUSE_SOCKET, 0);
            CHECK(b, "reuse accepted socket %d err=%d", i, WSAGetLastError());
            r = recv(c2, buf, sizeof(buf), 0); CHECK(r == 0, "c2 sees close %d", r);
            closesocket(c2);
        }
        closesocket(srv2); closesocket(l2);
    }

    /* options survive; a pending recv fails */
    {
        DWORD to = 300; u_long nb = 1;
        setsockopt(cl, SOL_SOCKET, SO_RCVTIMEO, (char *)&to, sizeof(to));
        r = connect(cl, (struct sockaddr *)&la, sizeof(la)); CHECK(!r, "reconnect");
        srv = accept(lst, NULL, NULL);
        pDisconnectEx(cl, NULL, TF_REUSE_SOCKET, 0);
        closesocket(srv);
        (void)nb;
    }
    closesocket(cl); closesocket(lst);
    printf("%s\n", failures ? "RESULT: FAIL" : "RESULT: PASS");
    return failures != 0;
}
