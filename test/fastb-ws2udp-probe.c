#include <winsock2.h>
#include <ws2tcpip.h>
#include <mswsock.h>
#include <stdio.h>
static int failures;
#define CHECK(c, ...) do { if (!(c)) { failures++; printf("FAIL line %d: %s: ", __LINE__, #c); printf(__VA_ARGS__); printf("\n"); } } while (0)

static unsigned short closed_port(void)
{
    struct sockaddr_in a = {AF_INET, 0, {{{127,0,0,1}}}};
    int len = sizeof(a);
    SOCKET s = socket(AF_INET, SOCK_DGRAM, 0);
    bind(s, (struct sockaddr *)&a, sizeof(a));
    getsockname(s, (struct sockaddr *)&a, &len);
    closesocket(s);
    return a.sin_port;
}
static SOCKET udp(void)
{
    struct sockaddr_in a = {AF_INET, 0, {{{127,0,0,1}}}};
    SOCKET s = socket(AF_INET, SOCK_DGRAM, 0);
    bind(s, (struct sockaddr *)&a, sizeof(a));
    return s;
}
static void set_to(SOCKET s, int ms) { setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (char *)&ms, sizeof(ms)); }
static int ioc(SOCKET s, BOOL v, DWORD *err)
{
    DWORD ret = 0;
    int r = WSAIoctl(s, SIO_UDP_CONNRESET, &v, sizeof(v), NULL, 0, &ret, NULL, NULL);
    *err = r ? WSAGetLastError() : 0;
    return r;
}
static int readable(SOCKET s, int ms)
{
    fd_set fds; struct timeval tv = {0, ms * 1000};
    FD_ZERO(&fds); FD_SET(s, &fds);
    return select(0, &fds, NULL, NULL, &tv);
}
static void drain_to(SOCKET s, struct sockaddr_in *to, int n)
{
    int i;
    for (i = 0; i < n; i++) sendto(s, "x", 1, 0, (struct sockaddr *)to, sizeof(*to));
}

int main(void)
{
    WSADATA wd; SOCKET s, s2; struct sockaddr_in to = {AF_INET, 0, {{{127,0,0,1}}}};
    char buf[16]; int r, len; DWORD err;
    OVERLAPPED ov; WSABUF wb; DWORD got, fl; struct sockaddr_in from; int fromlen;
    WSAStartup(MAKEWORD(2,2), &wd);

    /* A: default on, unconnected: sendto a closed port, recvfrom fails once with WSAECONNRESET */
    s = udp(); set_to(s, 700); to.sin_port = closed_port();
    sendto(s, "x", 1, 0, (struct sockaddr *)&to, sizeof(to));
    r = readable(s, 700);
    CHECK(r == 1, "A select readable r=%d", r);
    fromlen = sizeof(from);
    r = recvfrom(s, buf, sizeof(buf), 0, (struct sockaddr *)&from, &fromlen);
    CHECK(r == -1 && WSAGetLastError() == WSAECONNRESET, "A recvfrom r=%d err=%d", r, WSAGetLastError());
    r = readable(s, 100);
    CHECK(r == 0, "A select after consume r=%d", r);
    r = recvfrom(s, buf, sizeof(buf), 0, NULL, NULL);
    CHECK(r == -1 && WSAGetLastError() == WSAETIMEDOUT, "A second recvfrom r=%d err=%d", r, WSAGetLastError());
    /* the socket still works */
    s2 = udp(); len = sizeof(to);
    { struct sockaddr_in me; len = sizeof(me); getsockname(s, (struct sockaddr *)&me, &len);
      sendto(s2, "hello", 5, 0, (struct sockaddr *)&me, sizeof(me)); }
    r = recvfrom(s, buf, sizeof(buf), 0, NULL, NULL);
    CHECK(r == 5, "A data after reset r=%d err=%d", r, WSAGetLastError());
    closesocket(s2); closesocket(s);

    /* B: SIO_UDP_CONNRESET FALSE: nothing is reported */
    s = udp(); set_to(s, 400);
    r = ioc(s, FALSE, &err); CHECK(r == 0, "B ioctl off err=%lu", err);
    sendto(s, "x", 1, 0, (struct sockaddr *)&to, sizeof(to));
    r = readable(s, 300);
    CHECK(r == 0, "B select r=%d", r);
    r = recvfrom(s, buf, sizeof(buf), 0, NULL, NULL);
    CHECK(r == -1 && WSAGetLastError() == WSAETIMEDOUT, "B recvfrom r=%d err=%d", r, WSAGetLastError());
    /* ... and turning it on again reports a later error */
    r = ioc(s, TRUE, &err); CHECK(r == 0, "B ioctl on err=%lu", err);
    sendto(s, "x", 1, 0, (struct sockaddr *)&to, sizeof(to));
    r = recvfrom(s, buf, sizeof(buf), 0, NULL, NULL);
    CHECK(r == -1 && WSAGetLastError() == WSAECONNRESET, "B re-enabled r=%d err=%d", r, WSAGetLastError());
    closesocket(s);

    /* C: connected UDP socket */
    s = udp(); set_to(s, 700);
    connect(s, (struct sockaddr *)&to, sizeof(to));
    r = send(s, "x", 1, 0); CHECK(r == 1, "C send r=%d", r);
    r = recv(s, buf, sizeof(buf), 0);
    CHECK(r == -1 && WSAGetLastError() == WSAECONNRESET, "C recv r=%d err=%d", r, WSAGetLastError());
    closesocket(s);
    s = udp(); set_to(s, 400); ioc(s, FALSE, &err);
    connect(s, (struct sockaddr *)&to, sizeof(to));
    send(s, "x", 1, 0);
    r = recv(s, buf, sizeof(buf), 0);
    CHECK(r == -1 && WSAGetLastError() == WSAETIMEDOUT, "C off recv r=%d err=%d", r, WSAGetLastError());
    closesocket(s);

    /* D: a pending overlapped receive completes with WSAECONNRESET / ERROR_PORT_UNREACHABLE */
    s = udp();
    memset(&ov, 0, sizeof(ov)); ov.hEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
    wb.buf = buf; wb.len = sizeof(buf); fl = 0; fromlen = sizeof(from);
    r = WSARecvFrom(s, &wb, 1, &got, &fl, (struct sockaddr *)&from, &fromlen, &ov, NULL);
    CHECK(r == -1 && WSAGetLastError() == WSA_IO_PENDING, "D pending r=%d err=%d", r, WSAGetLastError());
    sendto(s, "x", 1, 0, (struct sockaddr *)&to, sizeof(to));
    r = WaitForSingleObject(ov.hEvent, 1000);
    CHECK(r == WAIT_OBJECT_0, "D wait %d", r);
    r = WSAGetOverlappedResult(s, &ov, &got, FALSE, &fl);
    CHECK(!r && WSAGetLastError() == WSAECONNRESET, "D result r=%d err=%d", r, WSAGetLastError());
    CHECK((NTSTATUS)ov.Internal == 0xC000023F /* STATUS_PORT_UNREACHABLE */,
          "D status %lx", (unsigned long)ov.Internal);
    closesocket(s);

    /* E: a socket that never sent anything is not affected; a different socket's error does not leak */
    s = udp(); s2 = udp(); set_to(s, 300);
    sendto(s2, "x", 1, 0, (struct sockaddr *)&to, sizeof(to));
    r = recvfrom(s, buf, sizeof(buf), 0, NULL, NULL);
    CHECK(r == -1 && WSAGetLastError() == WSAETIMEDOUT, "E other r=%d err=%d", r, WSAGetLastError());
    closesocket(s2); closesocket(s);

    /* F: several errors queue, each consumed in turn, and data after them still arrives */
    s = udp(); set_to(s, 400);
    drain_to(s, &to, 3);
    Sleep(100);
    got = 0;
    for (;;) { r = recvfrom(s, buf, sizeof(buf), 0, NULL, NULL);
        if (r == -1 && WSAGetLastError() == WSAECONNRESET) got++; else break; if (got > 5) break; }
    CHECK(got >= 1 && got <= 3, "F error count %lu", got);
    CHECK(r == -1 && WSAGetLastError() == WSAETIMEDOUT, "F end r=%d err=%d", r, WSAGetLastError());
    closesocket(s);


    /* H: non-blocking socket: select says readable, recv fails once */
    s = udp(); { u_long nb = 1; ioctlsocket(s, FIONBIO, &nb); }
    sendto(s, "x", 1, 0, (struct sockaddr *)&to, sizeof(to));
    r = readable(s, 500);
    CHECK(r == 1, "H select r=%d", r);
    r = recvfrom(s, buf, sizeof(buf), 0, NULL, NULL);
    CHECK(r == -1 && WSAGetLastError() == WSAECONNRESET, "H recv r=%d err=%d", r, WSAGetLastError());
    r = recvfrom(s, buf, sizeof(buf), 0, NULL, NULL);
    CHECK(r == -1 && WSAGetLastError() == WSAEWOULDBLOCK, "H recv2 r=%d err=%d", r, WSAGetLastError());
    /* and an immediate recv right after the send (no select) */
    sendto(s, "x", 1, 0, (struct sockaddr *)&to, sizeof(to));
    r = recvfrom(s, buf, sizeof(buf), 0, NULL, NULL);
    CHECK(r == -1 && WSAGetLastError() == WSAECONNRESET, "H immediate r=%d err=%d", r, WSAGetLastError());
    closesocket(s);

    /* I: WSAEventSelect FD_READ is signalled */
    s = udp();
    { WSAEVENT ev = WSACreateEvent(); WSANETWORKEVENTS ne;
      WSAEventSelect(s, ev, FD_READ);
      sendto(s, "x", 1, 0, (struct sockaddr *)&to, sizeof(to));
      r = WSAWaitForMultipleEvents(1, &ev, FALSE, 1000, FALSE);
      CHECK(r == WSA_WAIT_EVENT_0, "I event wait %d", r);
      memset(&ne, 0, sizeof(ne));
      WSAEnumNetworkEvents(s, ev, &ne);
      CHECK(ne.lNetworkEvents & FD_READ, "I events %lx", ne.lNetworkEvents);
      WSACloseEvent(ev); }
    closesocket(s);

    /* J: two pending receives: one send fails only the first */
    {
      OVERLAPPED ov2; char buf2[16]; WSABUF wb2;
      s = udp();
      memset(&ov, 0, sizeof(ov)); ov.hEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
      memset(&ov2, 0, sizeof(ov2)); ov2.hEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
      wb.buf = buf; wb.len = sizeof(buf); wb2.buf = buf2; wb2.len = sizeof(buf2);
      fl = 0; WSARecvFrom(s, &wb, 1, &got, &fl, NULL, NULL, &ov, NULL);
      fl = 0; WSARecvFrom(s, &wb2, 1, &got, &fl, NULL, NULL, &ov2, NULL);
      sendto(s, "x", 1, 0, (struct sockaddr *)&to, sizeof(to));
      r = WaitForSingleObject(ov.hEvent, 1000);
      CHECK(r == WAIT_OBJECT_0, "J first wait %d", r);
      r = WSAGetOverlappedResult(s, &ov, &got, FALSE, &fl);
      CHECK(!r && WSAGetLastError() == WSAECONNRESET, "J first result r=%d err=%d", r, WSAGetLastError());
      r = WaitForSingleObject(ov2.hEvent, 200);
      CHECK(r == WAIT_TIMEOUT, "J second still pending %d", r);
      { struct sockaddr_in me; int l = sizeof(me); getsockname(s, (struct sockaddr *)&me, &l);
        s2 = udp(); sendto(s2, "hi", 2, 0, (struct sockaddr *)&me, sizeof(me)); closesocket(s2); }
      r = WaitForSingleObject(ov2.hEvent, 1000);
      CHECK(r == WAIT_OBJECT_0, "J second wait %d", r);
      r = WSAGetOverlappedResult(s, &ov2, &got, FALSE, &fl);
      CHECK(r && got == 2, "J second result r=%d got=%lu err=%d", r, got, WSAGetLastError());
      closesocket(s);
    }

    /* K: IPv6 */
    {
      struct sockaddr_in6 a6 = {0}, c6 = {0};
      int l6 = sizeof(c6);
      s = socket(AF_INET6, SOCK_DGRAM, 0);
      a6.sin6_family = AF_INET6; a6.sin6_addr.s6_addr[15] = 1;
      if (s != INVALID_SOCKET && !bind(s, (struct sockaddr *)&a6, sizeof(a6)))
      {
          s2 = socket(AF_INET6, SOCK_DGRAM, 0);
          bind(s2, (struct sockaddr *)&a6, sizeof(a6));
          getsockname(s2, (struct sockaddr *)&c6, &l6);
          closesocket(s2);
          set_to(s, 700);
          sendto(s, "x", 1, 0, (struct sockaddr *)&c6, sizeof(c6));
          r = recvfrom(s, buf, sizeof(buf), 0, NULL, NULL);
          CHECK(r == -1 && WSAGetLastError() == WSAECONNRESET, "K v6 r=%d err=%d", r, WSAGetLastError());
          r = ioc(s, FALSE, &err); CHECK(!r, "K ioctl %lu", err);
          sendto(s, "x", 1, 0, (struct sockaddr *)&c6, sizeof(c6));
          r = recvfrom(s, buf, sizeof(buf), 0, NULL, NULL);
          CHECK(r == -1 && WSAGetLastError() == WSAETIMEDOUT, "K v6 off r=%d err=%d", r, WSAGetLastError());
      }
      else printf("note: no IPv6 loopback\n");
      if (s != INVALID_SOCKET) closesocket(s);
    }

    /* L: the ioctl completes asynchronously with an OVERLAPPED too */
    s = udp();
    { BOOL v = FALSE; DWORD ret = 0; OVERLAPPED o = {0};
      o.hEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
      r = WSAIoctl(s, SIO_UDP_CONNRESET, &v, sizeof(v), NULL, 0, &ret, &o, NULL);
      CHECK(r == 0 || WSAGetLastError() == WSA_IO_PENDING, "L overlapped r=%d err=%d", r, WSAGetLastError());
      r = WaitForSingleObject(o.hEvent, 1000);
      CHECK(r == WAIT_OBJECT_0, "L wait %d", r); }
    closesocket(s);

    /* G: ioctl on a TCP socket and with a short buffer */
    s = socket(AF_INET, SOCK_STREAM, 0);
    { BOOL v = FALSE; DWORD ret;
      r = WSAIoctl(s, SIO_UDP_CONNRESET, &v, sizeof(v), NULL, 0, &ret, NULL, NULL);
      printf("note: TCP SIO_UDP_CONNRESET r=%d err=%d\n", r, r ? WSAGetLastError() : 0); }
    closesocket(s);
    s = udp();
    { BOOL v = FALSE; DWORD ret;
      r = WSAIoctl(s, SIO_UDP_CONNRESET, &v, 1, NULL, 0, &ret, NULL, NULL);
      printf("note: short buffer r=%d err=%d\n", r, r ? WSAGetLastError() : 0); }
    closesocket(s);

    printf("%s\n", failures ? "RESULT: FAIL" : "RESULT: PASS");
    return failures != 0;
}
