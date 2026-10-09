/* GetBestRoute2 (patches/sg/1690), run by test/bestroute-gate.sh in a
 * network namespace: d0 10.9.9.2/24 with the default route via 10.9.9.1,
 * d1 10.8.0.2/16 with 10.8.5.0/24 via 10.8.0.1. The longest prefix wins,
 * the source address is the route's interface's, an interface or a source
 * address limits the routes. It was a stub (ERROR_NOT_SUPPORTED). */
#include <winsock2.h>
#include <ws2ipdef.h>
#include <iphlpapi.h>
#include <netioapi.h>
#include <stdio.h>

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

static SOCKADDR_INET v4(const char *s)
{
    SOCKADDR_INET a;
    memset(&a, 0, sizeof(a));
    a.Ipv4.sin_family = AF_INET;
    a.Ipv4.sin_addr.s_addr = inet_addr(s);
    return a;
}

static const char *str(const SOCKADDR_INET *a)
{
    return inet_ntoa(a->Ipv4.sin_addr);
}

int main(void)
{
    MIB_IPFORWARD_ROW2 route;
    SOCKADDR_INET dst, src, best;
    NET_IFINDEX d1 = 0;
    DWORD ret;
    char hop[32];

    dst = v4("10.9.9.7");
    ret = GetBestRoute2(NULL, 0, NULL, &dst, 0, &route, &best);
    check(!ret && route.DestinationPrefix.PrefixLength == 24 && !strcmp(str(&best), "10.9.9.2"),
          "10.9.9.7: the /24 on d0, from 10.9.9.2");
    if (ret) printf("      ret %lu\n", ret);
    dst = v4("8.8.8.8");
    ret = GetBestRoute2(NULL, 0, NULL, &dst, 0, &route, &best);
    strcpy(hop, str(&route.NextHop));
    check(!ret && route.DestinationPrefix.PrefixLength == 0 && !strcmp(hop, "10.9.9.1") &&
          !strcmp(str(&best), "10.9.9.2"), "8.8.8.8: the default route via 10.9.9.1");
    dst = v4("10.8.5.5");
    ret = GetBestRoute2(NULL, 0, NULL, &dst, 0, &route, &best);
    strcpy(hop, str(&route.NextHop));
    check(!ret && route.DestinationPrefix.PrefixLength == 24 && !strcmp(hop, "10.8.0.1") &&
          !strcmp(str(&best), "10.8.0.2"), "10.8.5.5: the /24 over the /16, from 10.8.0.2");
    d1 = route.InterfaceIndex;
    dst = v4("8.8.8.8");
    ret = GetBestRoute2(NULL, d1, NULL, &dst, 0, &route, &best);
    check(ret == ERROR_NETWORK_UNREACHABLE, "8.8.8.8 on d1 only: ERROR_NETWORK_UNREACHABLE");
    src = v4("10.8.0.2");
    dst = v4("10.8.200.1");
    ret = GetBestRoute2(NULL, 0, &src, &dst, 0, &route, &best);
    check(!ret && route.DestinationPrefix.PrefixLength == 16 && !strcmp(str(&best), "10.8.0.2"),
          "from 10.8.0.2: d1's /16");
    check(GetBestRoute2(NULL, 0, NULL, NULL, 0, &route, &best) == ERROR_INVALID_PARAMETER,
          "no destination: ERROR_INVALID_PARAMETER");
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
