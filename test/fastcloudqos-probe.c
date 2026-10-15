#include <winsock2.h>
#include <ws2tcpip.h>
#include <qos2.h>
#include <stdio.h>
static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)
static int tos(SOCKET s) { int v = -1, l = sizeof(v); getsockopt(s, IPPROTO_IP, IP_TOS, (char *)&v, &l); return v; }
int main(void)
{
    WSADATA wsa; QOS_VERSION ver = {1, 0}; HANDLE h = NULL;
    SOCKET a, b, c; QOS_FLOWID f1 = 0, f2 = 0, f3 = 0; struct sockaddr_in dst = {0};
    QOS_TRAFFIC_TYPE tt; DWORD d; BOOL ok;
    WSAStartup(MAKEWORD(2, 2), &wsa);
    a = socket(AF_INET, SOCK_DGRAM, 0); b = socket(AF_INET, SOCK_DGRAM, 0); c = socket(AF_INET, SOCK_DGRAM, 0);
    dst.sin_family = AF_INET; dst.sin_addr.s_addr = htonl(INADDR_LOOPBACK); dst.sin_port = htons(9);
    CHECK(QOSCreateHandle(&ver, &h) && h, "create");
    SetLastError(0);
    CHECK(!QOSAddSocketToFlow((HANDLE)0x1234, a, (PSOCKADDR)&dst, QOSTrafficTypeVoice, 0, &f1) && GetLastError() == ERROR_INVALID_HANDLE, "bad handle %lu", GetLastError());
    SetLastError(0);
    CHECK(!QOSAddSocketToFlow(h, a, (PSOCKADDR)&dst, QOSTrafficTypeVoice, 0, NULL) && GetLastError() == ERROR_INVALID_PARAMETER, "NULL flow id %lu", GetLastError());
    SetLastError(0);
    CHECK(!QOSAddSocketToFlow(h, a, (PSOCKADDR)&dst, (QOS_TRAFFIC_TYPE)9, 0, &f1) && GetLastError() == ERROR_INVALID_PARAMETER, "bad traffic type %lu", GetLastError());
    SetLastError(0);
    CHECK(!QOSAddSocketToFlow(h, INVALID_SOCKET, (PSOCKADDR)&dst, QOSTrafficTypeVoice, 0, &f1) && GetLastError() == ERROR_INVALID_PARAMETER, "bad socket %lu", GetLastError());

    /* adaptive: the traffic is not touched by the add */
    CHECK(QOSAddSocketToFlow(h, a, (PSOCKADDR)&dst, QOSTrafficTypeVoice, 0, &f1) && f1, "add adaptive flow %lu", GetLastError());
    CHECK(tos(a) == 0, "adaptive add leaves TOS: %#x", tos(a));
    tt = QOSTrafficTypeVoice;
    CHECK(QOSSetFlow(h, f1, QOSSetTrafficType, sizeof(tt), &tt, 0, NULL), "set traffic type %lu", GetLastError());
    CHECK(tos(a) == (0x38 << 2), "voice marks 0x38: TOS %#x", tos(a));
    tt = QOSTrafficTypeBackground;
    CHECK(QOSSetFlow(h, f1, QOSSetTrafficType, sizeof(tt), &tt, 0, NULL) && tos(a) == (0x08 << 2), "background: TOS %#x", tos(a));
    d = 46;
    CHECK(QOSSetFlow(h, f1, QOSSetOutgoingDSCPValue, sizeof(d), &d, 0, NULL) && tos(a) == (46 << 2), "explicit DSCP: TOS %#x", tos(a));
    d = 99; SetLastError(0);
    CHECK(!QOSSetFlow(h, f1, QOSSetOutgoingDSCPValue, sizeof(d), &d, 0, NULL) && GetLastError() == ERROR_INVALID_PARAMETER, "DSCP out of range %lu", GetLastError());
    SetLastError(0);
    CHECK(!QOSSetFlow(h, 777, QOSSetTrafficType, sizeof(tt), &tt, 0, NULL) && GetLastError() == ERROR_NOT_FOUND, "unknown flow %lu", GetLastError());
    CHECK(!QOSAddSocketToFlow(h, b, (PSOCKADDR)&dst, QOSTrafficTypeVoice, 0, &f1) , "adaptive flow takes one socket");

    /* non-adaptive: marked at once, several sockets share a flow */
    CHECK(QOSAddSocketToFlow(h, b, (PSOCKADDR)&dst, QOSTrafficTypeAudioVideo, QOS_NON_ADAPTIVE_FLOW, &f2) && f2 && f2 != f1, "add non-adaptive %lu", GetLastError());
    CHECK(tos(b) == (0x28 << 2), "A/V marks 0x28: TOS %#x", tos(b));
    CHECK(QOSAddSocketToFlow(h, c, (PSOCKADDR)&dst, QOSTrafficTypeAudioVideo, QOS_NON_ADAPTIVE_FLOW, &f2) && tos(c) == (0x28 << 2), "second socket in the flow %lu", GetLastError());
    tt = QOSTrafficTypeBackground;
    CHECK(QOSSetFlow(h, f2, QOSSetTrafficType, sizeof(tt), &tt, 0, NULL) && tos(b) == 0x20 && tos(c) == 0x20, "both sockets re-marked: %#x %#x", tos(b), tos(c));

    CHECK(QOSRemoveSocketFromFlow(h, b, f2, 0), "remove one socket %lu", GetLastError());
    SetLastError(0);
    CHECK(!QOSRemoveSocketFromFlow(h, b, f2, 0) && GetLastError() == ERROR_NOT_FOUND, "socket no longer in the flow %lu", GetLastError());
    CHECK(QOSRemoveSocketFromFlow(h, 0, f2, 0), "remove the flow %lu", GetLastError());
    SetLastError(0);
    CHECK(!QOSRemoveSocketFromFlow(h, 0, f2, 0) && GetLastError() == ERROR_NOT_FOUND, "flow gone %lu", GetLastError());

    CHECK(QOSCloseHandle(h), "close");
    SetLastError(0);
    CHECK(!QOSCloseHandle(h) && GetLastError() == ERROR_INVALID_HANDLE, "close twice %lu", GetLastError());
    SetLastError(0);
    CHECK(!QOSAddSocketToFlow(h, a, (PSOCKADDR)&dst, QOSTrafficTypeVoice, 0, &f3) && GetLastError() == ERROR_INVALID_HANDLE, "closed handle %lu", GetLastError());
    ok = TRUE; (void)ok;
    printf(fails ? "RESULT: FAIL\n" : "RESULT: PASS\n");
    return fails != 0;
}
