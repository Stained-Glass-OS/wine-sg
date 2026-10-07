/* fwlisten-gate.sh's probe: a Windows program that listens (TCP, IPv4 and
 * IPv6), binds UDP, and does both on the loopback too -- the firewall is
 * told about the first three, not the loopback ones.
 *   fwlisten-probe.exe TCPPORT UDPPORT
 * Copyright 2026 Stained Glass OS contributors
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h>
#include <stdlib.h>

static SOCKET make(int family, int type, const char *addr, int port, int do_listen)
{
    struct sockaddr_storage ss;
    int len;
    SOCKET s = socket(family, type, 0);
    memset(&ss, 0, sizeof(ss));
    if (family == AF_INET)
    {
        struct sockaddr_in *a = (struct sockaddr_in *)&ss;
        a->sin_family = AF_INET;
        a->sin_port = htons(port);
        inet_pton(AF_INET, addr, &a->sin_addr);
        len = sizeof(*a);
    }
    else
    {
        struct sockaddr_in6 *a = (struct sockaddr_in6 *)&ss;
        a->sin6_family = AF_INET6;
        a->sin6_port = htons(port);
        inet_pton(AF_INET6, addr, &a->sin6_addr);
        len = sizeof(*a);
    }
    if (s == INVALID_SOCKET || bind(s, (struct sockaddr *)&ss, len) || (do_listen && listen(s, 4)))
    {
        printf("FAILED %s %d %d\n", addr, port, WSAGetLastError());
        return INVALID_SOCKET;
    }
    printf("%s %s %d\n", do_listen ? "LISTEN" : "BOUND", addr, port);
    return s;
}

int main(int argc, char **argv)
{
    WSADATA wsa;
    int tport, uport;
    if (argc < 3) return 2;
    tport = atoi(argv[1]);
    uport = atoi(argv[2]);
    WSAStartup(MAKEWORD(2, 2), &wsa);
    make(AF_INET, SOCK_STREAM, "0.0.0.0", tport, 1);
    make(AF_INET, SOCK_STREAM, "127.0.0.1", tport + 1, 1);
    make(AF_INET6, SOCK_STREAM, "::", tport + 2, 1);
    make(AF_INET, SOCK_DGRAM, "0.0.0.0", uport, 0);
    make(AF_INET, SOCK_DGRAM, "127.0.0.1", uport + 1, 0);
    /* a TCP socket bound but not listening: nothing to tell */
    make(AF_INET, SOCK_STREAM, "0.0.0.0", tport + 3, 0);
    fflush(stdout);
    Sleep(1500);
    printf("DONE\n");
    return 0;
}
