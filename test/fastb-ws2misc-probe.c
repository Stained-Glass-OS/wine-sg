#include <winsock2.h>
#include <ws2tcpip.h>
#include <ws2spi.h>
#include <stdio.h>
static int failures;
#define CHECK(c, ...) do { if (!(c)) { failures++; printf("FAIL line %d: %s: ", __LINE__, #c); printf(__VA_ARGS__); printf("\n"); } } while (0)
#define WM_Q (WM_USER + 7)

static int hits[0x10000];
static LRESULT CALLBACK wp(HWND h, UINT m, WPARAM w, LPARAM l)
{
    if (m == WM_Q) { hits[LOWORD(w)]++; return 0; }
    return DefWindowProcA(h, m, w, l);
}
static void pump(int ms)
{
    DWORD end = GetTickCount() + ms; MSG msg;
    while (GetTickCount() < end)
    {
        while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) DispatchMessageA(&msg);
        Sleep(2);
    }
}
static int post_calls;
static BOOL WINAPI my_post(HWND h, UINT m, WPARAM w, LPARAM l) { post_calls++; return PostMessageA(h, m, w, l); }

typedef int (WINAPI *setpost_t)(BOOL (WINAPI *)(HWND,UINT,WPARAM,LPARAM));
static setpost_t pWSApSetPostRoutine;
int main(void)
{
    WSADATA wd; WNDCLASSA wc = {0}; HWND hwnd; int i, r, cancelled = 0, already = 0, invalid = 0;
    char sbuf[MAXGETHOSTSTRUCT]; HANDLE hs[200]; int cr[200];
    WSAStartup(MAKEWORD(2,2), &wd);
    pWSApSetPostRoutine = (setpost_t)GetProcAddress(GetModuleHandleA("ws2_32.dll"), "WSApSetPostRoutine");
    wc.lpfnWndProc = wp; wc.lpszClassName = "q"; wc.hInstance = GetModuleHandleA(NULL);
    RegisterClassA(&wc);
    hwnd = CreateWindowA("q", "q", 0, 0, 0, 10, 10, HWND_MESSAGE, NULL, wc.hInstance, NULL);

    /* cancel: zero and unknown handles */
    r = WSACancelAsyncRequest(NULL);
    CHECK(r == SOCKET_ERROR && WSAGetLastError() == WSAEINVAL, "cancel NULL r=%d err=%d", r, WSAGetLastError());
    r = WSACancelAsyncRequest((HANDLE)0x7777);
    CHECK(r == SOCKET_ERROR && WSAGetLastError() == WSAEINVAL, "cancel unknown r=%d err=%d", r, WSAGetLastError());

    /* cancelled requests never post; those that already ran report WSAEALREADY */
    for (i = 0; i < 200; i++)
    {
        hs[i] = WSAAsyncGetServByName(hwnd, WM_Q, "http", "tcp", sbuf, sizeof(sbuf));
        CHECK(hs[i] != NULL, "request %d", i);
        cr[i] = WSACancelAsyncRequest(hs[i]) ? WSAGetLastError() : 0;
        if (cr[i] == 0) cancelled++; else if (cr[i] == WSAEALREADY) already++; else invalid++;
    }
    pump(400);
    for (i = 0; i < 200; i++)
    {
        if (cr[i] == 0) CHECK(hits[LOWORD((ULONG_PTR)hs[i])] == 0, "cancelled request %d (%p) still posted", i, hs[i]);
        else CHECK(hits[LOWORD((ULONG_PTR)hs[i])] == 1, "request %d (%p) cancel err %d hits %d", i, hs[i], cr[i], hits[LOWORD((ULONG_PTR)hs[i])]);
    }
    printf("note: cancelled %d already %d other %d\n", cancelled, already, invalid);
    CHECK(cancelled > 0, "no request was cancelled");
    CHECK(invalid == 0, "unexpected cancel errors %d", invalid);
    /* a second cancel of a cancelled request is not a success */
    r = WSACancelAsyncRequest(hs[0]);
    CHECK(r == SOCKET_ERROR, "second cancel r=%d", r);
    /* a finished request: WSAEALREADY */
    {
        HANDLE h = WSAAsyncGetServByName(hwnd, WM_Q, "http", "tcp", sbuf, sizeof(sbuf));
        pump(300);
        r = WSACancelAsyncRequest(h);
        CHECK(r == SOCKET_ERROR && WSAGetLastError() == WSAEALREADY, "finished r=%d err=%d", r, WSAGetLastError());
        CHECK(hits[LOWORD((ULONG_PTR)h)] >= 1, "finished request posted");
    }

    /* WSApSetPostRoutine routes the completion message */
    {
        HANDLE h;
        r = pWSApSetPostRoutine(my_post);
        CHECK(r == 0, "set post routine %d", r);
        h = WSAAsyncGetServByName(hwnd, WM_Q, "http", "tcp", sbuf, sizeof(sbuf));
        pump(300);
        CHECK(post_calls == 1, "post routine calls %d", post_calls);
        CHECK(hits[LOWORD((ULONG_PTR)h)] >= 1, "message arrived through the routine");
        pWSApSetPostRoutine(NULL);
        post_calls = 0;
        h = WSAAsyncGetServByName(hwnd, WM_Q, "http", "tcp", sbuf, sizeof(sbuf));
        pump(300);
        CHECK(post_calls == 0, "post routine reset %d", post_calls);
    }

    /* WPUCompleteOverlappedRequest: no WPUCreateSocketHandle sockets exist */
    {
        SOCKET s = socket(AF_INET, SOCK_DGRAM, 0); WSAOVERLAPPED ov = {0}; int en = 0;
        r = WPUCompleteOverlappedRequest(s, &ov, 0, 0, &en);
        CHECK(r == SOCKET_ERROR && en == WSAEINVAL, "wpucomplete r=%d en=%d", r, en);
        closesocket(s);
    }

    /* WSAGetQOSByName */
    {
        SOCKET s = socket(AF_INET, SOCK_DGRAM, 0); WSABUF nm; char nb[64] = "G711"; QOS q; BOOL b;
        nm.buf = nb; nm.len = sizeof(nb);
        b = WSAGetQOSByName(INVALID_SOCKET, &nm, &q);
        CHECK(!b && WSAGetLastError() == WSAENOTSOCK, "qos notsock b=%d err=%d", b, WSAGetLastError());
        b = WSAGetQOSByName(s, NULL, &q);
        CHECK(!b && WSAGetLastError() == WSAEFAULT, "qos null name b=%d err=%d", b, WSAGetLastError());
        b = WSAGetQOSByName(s, &nm, NULL);
        CHECK(!b && WSAGetLastError() == WSAEFAULT, "qos null qos b=%d err=%d", b, WSAGetLastError());
        nb[0] = 0;
        b = WSAGetQOSByName(s, &nm, NULL);
        CHECK(b, "qos enumerate b=%d err=%d", b, WSAGetLastError());
        closesocket(s);
    }

    /* WSAJoinLeaf */
    {
        SOCKET s, t, leaf; struct sockaddr_in grp = {AF_INET, htons(0), {{{239,255,42,99}}}}, a = {AF_INET}, bad = grp;
        struct sockaddr_in6 g6 = {AF_INET6};
        WSABUF cd = {4, (char *)"abcd"};
        s = socket(AF_INET, SOCK_DGRAM, 0);
        leaf = WSAJoinLeaf(INVALID_SOCKET, (struct sockaddr *)&grp, sizeof(grp), NULL, NULL, NULL, NULL, JL_BOTH);
        CHECK(leaf == INVALID_SOCKET && WSAGetLastError() == WSAENOTSOCK, "join notsock err=%d", WSAGetLastError());
        leaf = WSAJoinLeaf(s, NULL, sizeof(grp), NULL, NULL, NULL, NULL, JL_BOTH);
        CHECK(leaf == INVALID_SOCKET && WSAGetLastError() == WSAEFAULT, "join null addr err=%d", WSAGetLastError());
        leaf = WSAJoinLeaf(s, (struct sockaddr *)&grp, 4, NULL, NULL, NULL, NULL, JL_BOTH);
        CHECK(leaf == INVALID_SOCKET && WSAGetLastError() == WSAEFAULT, "join short addr err=%d", WSAGetLastError());
        leaf = WSAJoinLeaf(s, (struct sockaddr *)&g6, sizeof(g6), NULL, NULL, NULL, NULL, JL_BOTH);
        CHECK(leaf == INVALID_SOCKET && WSAGetLastError() == WSAEAFNOSUPPORT, "join family err=%d", WSAGetLastError());
        bad.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        leaf = WSAJoinLeaf(s, (struct sockaddr *)&bad, sizeof(bad), NULL, NULL, NULL, NULL, JL_BOTH);
        CHECK(leaf == INVALID_SOCKET && WSAGetLastError() == WSAEADDRNOTAVAIL, "join not multicast err=%d", WSAGetLastError());
        leaf = WSAJoinLeaf(s, (struct sockaddr *)&grp, sizeof(grp), NULL, NULL, NULL, NULL, 0x55);
        CHECK(leaf == INVALID_SOCKET && WSAGetLastError() == WSAEINVAL, "join bad flags err=%d", WSAGetLastError());
        leaf = WSAJoinLeaf(s, (struct sockaddr *)&grp, sizeof(grp), &cd, NULL, NULL, NULL, JL_BOTH);
        CHECK(leaf == INVALID_SOCKET && WSAGetLastError() == WSAEPROTONOSUPPORT, "join caller data err=%d", WSAGetLastError());
        t = socket(AF_INET, SOCK_STREAM, 0);
        leaf = WSAJoinLeaf(t, (struct sockaddr *)&grp, sizeof(grp), NULL, NULL, NULL, NULL, JL_BOTH);
        CHECK(leaf == INVALID_SOCKET && WSAGetLastError() == WSAEOPNOTSUPP, "join tcp err=%d", WSAGetLastError());
        closesocket(t);

        /* a sender-only leaf is a distinct bound socket; a receiver leaf joins the group */
        a.sin_addr.s_addr = htonl(INADDR_ANY);
        bind(s, (struct sockaddr *)&a, sizeof(a));
        { int l = sizeof(a); getsockname(s, (struct sockaddr *)&a, &l); grp.sin_port = a.sin_port; }
        leaf = WSAJoinLeaf(s, (struct sockaddr *)&grp, sizeof(grp), NULL, NULL, NULL, NULL, JL_SENDER_ONLY);
        CHECK(leaf != INVALID_SOCKET && leaf != s, "sender leaf %Id err=%d", (INT_PTR)leaf, WSAGetLastError());
        if (leaf != INVALID_SOCKET)
        {
            struct sockaddr_in la; int l = sizeof(la);
            CHECK(!getsockname(leaf, (struct sockaddr *)&la, &l) && la.sin_port == a.sin_port, "leaf port %d vs %d", ntohs(la.sin_port), ntohs(a.sin_port));
            closesocket(leaf);
        }
        {
            /* control: is multicast delivery to a plain joined socket possible in this environment? */
            SOCKET ctl = socket(AF_INET, SOCK_DGRAM, 0), snd = socket(AF_INET, SOCK_DGRAM, 0), c2 = socket(AF_INET, SOCK_DGRAM, 0);
            struct sockaddr_in any = {AF_INET}; struct ip_mreq mr; char b[16]; int n, ctl_ok, one = 1, ln = sizeof(any);
            DWORD to = 800; unsigned char ttl = 1; BOOL loop = TRUE;
            struct sockaddr_in cgrp = grp;

            /* a fresh port for the control group so the leaf test below starts from scratch */
            bind(c2, (struct sockaddr *)&any, sizeof(any)); getsockname(c2, (struct sockaddr *)&any, &ln); cgrp.sin_port = any.sin_port;
            closesocket(c2);
            setsockopt(ctl, SOL_SOCKET, SO_REUSEADDR, (char *)&one, sizeof(one));
            bind(ctl, (struct sockaddr *)&any, sizeof(any));
            memset(&mr, 0, sizeof(mr)); mr.imr_multiaddr = cgrp.sin_addr;
            setsockopt(ctl, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *)&mr, sizeof(mr));
            setsockopt(snd, IPPROTO_IP, IP_MULTICAST_TTL, (char *)&ttl, 1);
            setsockopt(snd, IPPROTO_IP, IP_MULTICAST_LOOP, (char *)&loop, sizeof(loop));
            setsockopt(ctl, SOL_SOCKET, SO_RCVTIMEO, (char *)&to, sizeof(to));
            sendto(snd, "ctl", 3, 0, (struct sockaddr *)&cgrp, sizeof(cgrp));
            ctl_ok = recv(ctl, b, sizeof(b), 0) == 3;
            closesocket(ctl);

            /* a socket that did not join gets nothing; the leaf of a receiving join does */
            leaf = WSAJoinLeaf(s, (struct sockaddr *)&grp, sizeof(grp), NULL, NULL, NULL, NULL, JL_RECEIVER_ONLY);
            CHECK(leaf != INVALID_SOCKET, "receiver join failed err=%d", WSAGetLastError());
            if (!ctl_ok) printf("note: multicast does not loop back here, delivery not checked\n");
            else if (leaf != INVALID_SOCKET)
            {
                setsockopt(leaf, SOL_SOCKET, SO_RCVTIMEO, (char *)&to, sizeof(to));
                sendto(snd, "mcast", 5, 0, (struct sockaddr *)&grp, sizeof(grp));
                n = recv(leaf, b, sizeof(b), 0);
                CHECK(n == 5 && !memcmp(b, "mcast", 5), "no multicast delivery on the leaf (n=%d err=%d)", n, WSAGetLastError());
            }
            if (leaf != INVALID_SOCKET) closesocket(leaf);
            closesocket(snd);
        }
        closesocket(s);
    }
    printf("%s\n", failures ? "RESULT: FAIL" : "RESULT: PASS");
    return failures != 0;
}
