/* wsock32 GetTypeByName (patch 3656): service type GUIDs of the TCP/UDP services */
#include <winsock2.h>
#include <windows.h>
#include <stdio.h>
#include <string.h>

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

static INT (WINAPI *pGetTypeByNameA)(LPSTR, LPGUID);
static INT (WINAPI *pGetTypeByNameW)(LPWSTR, LPGUID);

static int is_svcid(const GUID *g, WORD kind, WORD port)
{
    static const BYTE tail[8] = { 0xc0, 0, 0, 0, 0, 0, 0, 0x46 };
    return g->Data1 == (((DWORD)kind << 16) | port) && !g->Data2 && !g->Data3 && !memcmp(g->Data4, tail, 8);
}

int main(void)
{
    HMODULE wsock = LoadLibraryA("wsock32.dll");
    WSADATA wsa;
    GUID guid;
    struct servent *tcp, *udp;
    INT ret;

    if (!wsock) { printf("FAIL  wsock32.dll\nRESULT: FAIL\n"); return 1; }
    pGetTypeByNameA = (void *)GetProcAddress(wsock, "GetTypeByNameA");
    pGetTypeByNameW = (void *)GetProcAddress(wsock, "GetTypeByNameW");
    check(pGetTypeByNameA && pGetTypeByNameW, "exports");
    WSAStartup(MAKEWORD(1, 1), &wsa);

    /* services with fixed, well known ports */
    memset(&guid, 0xcc, sizeof(guid));
    ret = pGetTypeByNameA((LPSTR)"http", &guid);
    check(ret == 0 && is_svcid(&guid, 0x0009, 80), "http is the TCP service on port 80");
    memset(&guid, 0xcc, sizeof(guid));
    ret = pGetTypeByNameA((LPSTR)"ftp", &guid);
    check(ret == 0 && is_svcid(&guid, 0x0009, 21), "ftp is the TCP service on port 21");
    memset(&guid, 0xcc, sizeof(guid));
    ret = pGetTypeByNameW((LPWSTR)L"smtp", &guid);
    check(ret == 0 && is_svcid(&guid, 0x0009, 25), "smtp (wide) is the TCP service on port 25");

    /* the TCP entry wins when both exist */
    tcp = getservbyname("domain", "tcp");
    udp = getservbyname("domain", "udp");
    if (tcp && udp)
    {
        ret = pGetTypeByNameA((LPSTR)"domain", &guid);
        check(ret == 0 && is_svcid(&guid, 0x0009, 53), "domain exists for TCP and UDP: TCP is returned");
    }

    /* a UDP-only service */
    tcp = getservbyname("ntp", "tcp");
    udp = getservbyname("ntp", "udp");
    if (!tcp && udp)
    {
        ret = pGetTypeByNameA((LPSTR)"ntp", &guid);
        check(ret == 0 && is_svcid(&guid, 0x000a, ntohs(udp->s_port)), "a UDP-only service gets the UDP GUID");
    }

    /* failures */
    memset(&guid, 0xcc, sizeof(guid));
    SetLastError(0);
    ret = pGetTypeByNameA((LPSTR)"no-such-service-zz", &guid);
    check(ret == SOCKET_ERROR && GetLastError() == ERROR_SERVICE_DOES_NOT_EXIST, "unknown service");
    check(guid.Data1 == 0xcccccccc, "the GUID is not touched on failure");
    SetLastError(0);
    ret = pGetTypeByNameA(NULL, &guid);
    check(ret == SOCKET_ERROR && GetLastError() == ERROR_INVALID_PARAMETER, "NULL name");
    SetLastError(0);
    ret = pGetTypeByNameA((LPSTR)"http", NULL);
    check(ret == SOCKET_ERROR && GetLastError() == ERROR_INVALID_PARAMETER, "NULL destination");
    SetLastError(0);
    ret = pGetTypeByNameW((LPWSTR)L"no-such-service-zz", &guid);
    check(ret == SOCKET_ERROR && GetLastError() == ERROR_SERVICE_DOES_NOT_EXIST, "unknown service (wide)");

    WSACleanup();
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures ? 1 : 0;
}
