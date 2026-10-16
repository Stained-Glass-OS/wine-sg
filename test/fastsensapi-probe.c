/* sensapi network state (patch 3292), run by test/fastsensapi-gate.sh.
 * IsNetworkAlive is compared with the interface list read independently here; IsDestinationReachable
 * is checked for loopback, name resolution failures and argument errors. No packets leave the machine. */
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iphlpapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct { DWORD dwSize, dwFlags, dwInSpeed, dwOutSpeed; } QOC;
typedef BOOL (WINAPI *PAlive)(LPDWORD);
typedef BOOL (WINAPI *PReachA)(const char *, QOC *);
typedef BOOL (WINAPI *PReachW)(const WCHAR *, QOC *);
static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

/* expected: bit0 LAN, bit1 WAN from the adapter list */
static DWORD expected_flags(void)
{
    ULONG size = 32768;
    IP_ADAPTER_ADDRESSES *list = malloc(size), *a;
    DWORD flags = 0;
    if (!list || GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER, NULL, list, &size)) return 0;
    for (a = list; a; a = a->Next)
    {
        IP_ADAPTER_UNICAST_ADDRESS *u;
        BOOL usable = FALSE;
        if (a->OperStatus != IfOperStatusUp || a->IfType == IF_TYPE_SOFTWARE_LOOPBACK) continue;
        for (u = a->FirstUnicastAddress; u; u = u->Next)
        {
            SOCKADDR *sa = u->Address.lpSockaddr;
            if (sa->sa_family == AF_INET)
            {
                BYTE *b = (BYTE *)&((SOCKADDR_IN *)sa)->sin_addr;
                if (b[0] && !(b[0] == 169 && b[1] == 254)) usable = TRUE;
            }
            else if (sa->sa_family == AF_INET6)
            {
                BYTE *b = ((struct sockaddr_in6 *)sa)->sin6_addr.s6_addr;
                if (!(b[0] == 0xfe && (b[1] & 0xc0) == 0x80)) usable = TRUE;
            }
        }
        if (!usable) continue;
        flags |= (a->IfType == IF_TYPE_PPP || a->IfType == IF_TYPE_SLIP) ? 2 : 1;
    }
    free(list);
    return flags;
}

int main(void)
{
    HMODULE m = LoadLibraryA("sensapi.dll");
    PAlive alive = (PAlive)GetProcAddress(m, "IsNetworkAlive");
    PReachA reach_a = (PReachA)GetProcAddress(m, "IsDestinationReachableA");
    PReachW reach_w = (PReachW)GetProcAddress(m, "IsDestinationReachableW");
    DWORD flags = 0xdead, want = expected_flags();
    QOC q;
    BOOL r;

    check(alive && reach_a && reach_w, "exports");
    r = alive(&flags);
    printf("note: expected flags %lu from the interface list\n", want);
    check(r == (want != 0), "IsNetworkAlive result follows the interfaces");
    check(flags == want, "IsNetworkAlive flags are LAN/WAN as the interface types say");
    SetLastError(0xdeadbeef);
    r = alive(NULL);
    check(r == (want != 0) && GetLastError() == ERROR_SUCCESS, "IsNetworkAlive(NULL) works and clears the last error");

    memset(&q, 0, sizeof(q)); q.dwSize = sizeof(q);
    r = reach_a("127.0.0.1", &q);
    check(r, "127.0.0.1 is reachable");
    check(q.dwFlags == 1, "loopback is reported as a LAN connection");
    memset(&q, 0, sizeof(q)); q.dwSize = sizeof(q);
    check(reach_a("localhost", &q), "localhost is reachable");
    check(reach_w(L"127.0.0.1", NULL), "wide form, no QOCINFO");

    SetLastError(0);
    check(!reach_a(NULL, NULL) && GetLastError() == ERROR_INVALID_PARAMETER, "NULL destination is an invalid parameter");
    SetLastError(0);
    check(!reach_w(L"", NULL) && GetLastError() == ERROR_INVALID_PARAMETER, "empty destination is an invalid parameter");
    memset(&q, 0, sizeof(q));
    SetLastError(0);
    check(!reach_a("127.0.0.1", &q) && GetLastError() == ERROR_INVALID_PARAMETER, "QOCINFO with a wrong size is refused");
    SetLastError(0);
    check(!reach_a("no-such-host.invalid", NULL) && GetLastError() != 0, "an unresolvable name is not reachable");

    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
