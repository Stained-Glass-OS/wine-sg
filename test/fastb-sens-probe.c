/* sensapi IsDestinationReachable, connect IsInternetConnected (patch 3741). The results depend on the
 * routing table of the machine, so each is checked against the routing table itself. */
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <stdio.h>
#include <string.h>

static int f;
#define CHECK(c, ...) do { if (!(c)) { f++; printf("FAIL line %d: %s: ", __LINE__, #c); printf(__VA_ARGS__); printf("\n"); } } while (0)

typedef struct { DWORD dwSize, dwFlags, dwInSpeed, dwOutSpeed; } QOC;

int main(int argc, char **argv)
{
    /* argv[1]: "offline" (a network namespace with no route) or "online" (one with a default route): the
     * outcome is then known, not read from the host's own routing table */
    const char *mode = argc > 1 ? argv[1] : "host";
    BOOL (WINAPI *pDestA)(const char *, QOC *), (WINAPI *pDestW)(const WCHAR *, QOC *);
    HRESULT (WINAPI *pInet)(void);
    HMODULE s = LoadLibraryA("sensapi.dll"), c = LoadLibraryA("connect.dll");
    WSADATA wsa; SOCKADDR_IN a = { AF_INET }; DWORD index, err;
    QOC q; BOOL ok;

    WSAStartup(MAKEWORD(2, 2), &wsa);
    pDestA = (void *)GetProcAddress(s, "IsDestinationReachableA");
    pDestW = (void *)GetProcAddress(s, "IsDestinationReachableW");
    pInet = c ? (void *)GetProcAddress(c, "IsInternetConnected") : NULL;
    CHECK(pDestA && pDestW, "sensapi exports");

    memset(&q, 0xcc, sizeof(q)); q.dwSize = sizeof(q);
    ok = pDestA("127.0.0.1", &q);
    CHECK(ok, "the loopback is reachable: %lu", GetLastError());
    CHECK(ok && q.dwSize == sizeof(q) && q.dwFlags != 0xcccccccc && q.dwFlags != 0, "QOC flags %#lx", q.dwFlags);
    ok = pDestW(L"localhost", NULL);
    CHECK(ok, "localhost with no QOC info: %lu", GetLastError());
    SetLastError(0xdeadbeef);
    ok = pDestW(L"no-such-host.fastb.invalid", NULL);
    CHECK(!ok && GetLastError() != 0xdeadbeef, "an unresolvable name: %d %lu", ok, GetLastError());
    SetLastError(0xdeadbeef);
    ok = pDestA(NULL, NULL);
    CHECK(!ok && GetLastError() == ERROR_INVALID_PARAMETER, "NULL destination: %d %lu", ok, GetLastError());
    q.dwSize = 3;
    SetLastError(0xdeadbeef);
    ok = pDestA("127.0.0.1", &q);
    CHECK(!ok && GetLastError() == ERROR_INVALID_PARAMETER, "bad QOC size: %d %lu", ok, GetLastError());

    /* a public address: reachable exactly when the routing table has an interface for it */
    a.sin_addr.s_addr = htonl(0x08080808);
    err = GetBestInterfaceEx((SOCKADDR *)&a, &index);
    if (!strcmp(mode, "offline")) CHECK(err != NO_ERROR, "the namespace was meant to have no route, got %lu", err);
    if (!strcmp(mode, "online")) CHECK(err == NO_ERROR, "the namespace was meant to have a default route, got %lu", err);
    ok = pDestA("8.8.8.8", NULL);
    CHECK(ok == (err == NO_ERROR), "8.8.8.8: reachable %d, routing table says %lu", ok, err);
    if (!strcmp(mode, "offline")) CHECK(!ok, "8.8.8.8 must be unreachable with no route");
    if (!strcmp(mode, "online")) CHECK(ok, "8.8.8.8 must be reachable with a default route");
    if (pInet)
    {
        HRESULT hr = pInet();
        CHECK(hr == (err == NO_ERROR ? S_OK : S_FALSE), "IsInternetConnected %#lx, routing table says %lu", hr, err);
        if (!strcmp(mode, "offline")) CHECK(hr == S_FALSE, "IsInternetConnected %#lx with no route", hr);
        if (!strcmp(mode, "online")) CHECK(hr == S_OK, "IsInternetConnected %#lx with a default route", hr);
    }
    else CHECK(0, "connect.dll has no IsInternetConnected");
    printf("note: default route %s\n", err == NO_ERROR ? "present" : "absent");
    printf("%s\n", f ? "RESULT: FAIL" : "RESULT: PASS");
    return f != 0;
}
