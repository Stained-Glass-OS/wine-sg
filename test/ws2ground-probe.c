/* ws2_32 behaviours Wine's own tests record from Windows (patches/sg/2434), run by
 * test/ws2ground-gate.sh: a lookup with neither name nor service, WSACleanup's last
 * error, and the IPv6 addresses of a host name on a machine with no IPv6 to use. */
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <stdio.h>
#include <string.h>

static int failures;
#define CHECK(c, ...) do { if (!(c)) { failures++; printf("FAIL  " __VA_ARGS__); printf("\n"); } } while (0)

typedef INT (WINAPI *GetAddrInfoExW_t)(PCWSTR, PCWSTR, DWORD, LPGUID, const ADDRINFOEXW *, ADDRINFOEXW **, struct timeval *, LPOVERLAPPED, LPLOOKUPSERVICE_COMPLETION_ROUTINE, LPHANDLE);

static BOOL machine_has_ipv6(void)
{
    ULONG size = 0;
    IP_ADAPTER_ADDRESSES *buf, *a;
    BOOL ret = FALSE;

    if (GetAdaptersAddresses(AF_INET6, GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER, NULL, NULL, &size) != ERROR_BUFFER_OVERFLOW) return FALSE;
    buf = malloc(size);
    if (GetAdaptersAddresses(AF_INET6, GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER, NULL, buf, &size) == ERROR_SUCCESS)
        for (a = buf; a; a = a->Next)
        {
            IP_ADAPTER_UNICAST_ADDRESS *u;
            for (u = a->FirstUnicastAddress; u; u = u->Next)
            {
                const IN6_ADDR *addr = &((SOCKADDR_IN6 *)u->Address.lpSockaddr)->sin6_addr;
                if (u->Address.lpSockaddr->sa_family == AF_INET6 && !IN6_IS_ADDR_LINKLOCAL(addr) && !IN6_IS_ADDR_LOOPBACK(addr) &&
                    !IN6_IS_ADDR_UNSPECIFIED(addr)) ret = TRUE;
            }
        }
    free(buf);
    return ret;
}

static BOOL has_family(ADDRINFOA *list, int family)
{
    for (; list; list = list->ai_next) if (list->ai_family == family) return TRUE;
    return FALSE;
}

int main(void)
{
    WSADATA data;
    GetAddrInfoExW_t pGetAddrInfoExW;
    OVERLAPPED ov;
    ADDRINFOEXW *result = (void *)0xdeadbeef;
    ADDRINFOA *res, hint;
    HANDLE event;
    int ret;

    WSAStartup(MAKEWORD(2, 2), &data);
    pGetAddrInfoExW = (void *)GetProcAddress(GetModuleHandleA("ws2_32.dll"), "GetAddrInfoExW");

    /* neither a name nor a service: no lookup is started */
    event = CreateEventW(NULL, TRUE, FALSE, NULL);
    memset(&ov, 0xcc, sizeof(ov));
    ov.hEvent = event;
    WSASetLastError(0);
    ret = pGetAddrInfoExW(NULL, NULL, NS_DNS, NULL, NULL, &result, NULL, &ov, NULL, NULL);
    CHECK(ret == WSAHOST_NOT_FOUND, "no name, no service: %d", ret);
    CHECK(WSAGetLastError() == WSAHOST_NOT_FOUND, "no name, no service, last error %d", WSAGetLastError());
    CHECK(result == (void *)0xdeadbeef || result == NULL, "result touched: %p", result);
    CHECK(WaitForSingleObject(event, 100) == WAIT_TIMEOUT, "event signalled for a request that was not started");

    /* a clean-up that works clears the error */
    WSAStartup(MAKEWORD(2, 2), &data);
    WSASetLastError(0xdeadbeef);
    CHECK(!WSACleanup() && WSAGetLastError() == 0, "WSACleanup last error %d", WSAGetLastError());

    /* a name that has IPv6 addresses but a machine that cannot use them */
    memset(&hint, 0, sizeof(hint));
    hint.ai_family = AF_INET6;
    res = NULL;
    ret = getaddrinfo("localhost", NULL, &hint, &res);
    CHECK(!ret && has_family(res, AF_INET6), "localhost keeps its IPv6 loopback: %d", ret);
    if (res) freeaddrinfo(res);

    res = NULL;
    ret = getaddrinfo("www.kernel.org", NULL, NULL, &res);
    if (ret)
        printf("note  no name resolution here: the IPv6 rows are skipped\n");
    else
    {
        BOOL v6 = machine_has_ipv6();
        CHECK(v6 || !has_family(res, AF_INET6), "IPv6 address returned to a machine without IPv6");
        CHECK(has_family(res, AF_INET), "no IPv4 address left");
        freeaddrinfo(res);
        if (!v6)
        {
            memset(&hint, 0, sizeof(hint));
            hint.ai_family = AF_INET6;
            res = NULL;
            ret = getaddrinfo("www.kernel.org", NULL, &hint, &res);
            CHECK(ret == WSANO_DATA, "only IPv6 asked for: %d", ret);
            if (!ret) freeaddrinfo(res);
        }
    }
    WSACleanup();
    printf(failures ? "RESULT: FAIL\n" : "RESULT: PASS\n");
    return failures != 0;
}
