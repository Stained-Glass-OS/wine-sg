/* Winsock catalog, connects and cancellable lookups (patches/sg/1655), run
 * by test/wscatalog-gate.sh:
 *  - WSAConnectByName to a local listener (and to "localhost"), with the
 *    addresses it gives back; WSAConnectByList skipping a closed port;
 *    WSAConnectByName with a timeout to a listener whose queue is full:
 *    WSAETIMEDOUT in time;
 *  - GetAddrInfoExW: an asynchronous lookup cancelled with
 *    GetAddrInfoExCancel or stopped by its timeout completes with
 *    WSA_E_CANCELLED / WSAETIMEDOUT and leaves the result alone;
 *  - the catalog: a name space provider installed, disabled, reordered and
 *    removed; a service class installed, read by id and by provider,
 *    removed; an application category set and read; WSAProviderConfigChange
 *    completing when the catalog changes; a transport provider recorded and
 *    its path read. These were stubs (the catalog calls returned success
 *    and did nothing, or failed). */
#include <winsock2.h>
#include <ws2tcpip.h>
#include <ws2spi.h>
#include <mswsock.h>
#include <stdio.h>

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) failures++;
}

typedef int (WINAPI *GetAddrInfoExW_t)(const WCHAR *, const WCHAR *, DWORD, GUID *, const ADDRINFOEXW *,
                                       ADDRINFOEXW **, struct timeval *, OVERLAPPED *,
                                       LPLOOKUPSERVICE_COMPLETION_ROUTINE, HANDLE *);
typedef int (WINAPI *GetAddrInfoExCancel_t)(HANDLE *);
typedef BOOL (WINAPI *WSAConnectByList_t)(SOCKET, SOCKET_ADDRESS_LIST *, DWORD *, SOCKADDR *, DWORD *, SOCKADDR *,
                                          const struct timeval *, WSAOVERLAPPED *);
typedef int (WINAPI *WSCWriteNameSpaceOrder_t)(GUID *, DWORD);

static SOCKET listener(int backlog, unsigned short *port)
{
    SOCKADDR_IN sin = {0};
    int len = sizeof(sin);
    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    bind(s, (SOCKADDR *)&sin, sizeof(sin));
    listen(s, backlog);
    getsockname(s, (SOCKADDR *)&sin, &len);
    *port = ntohs(sin.sin_port);
    return s;
}

static void test_connect(void)
{
    WSAConnectByList_t pWSAConnectByList = (void *)GetProcAddress(GetModuleHandleA("ws2_32.dll"), "WSAConnectByList");
    unsigned short port, closed_port;
    SOCKADDR_IN remote, local, addrs[2];
    SOCKET_ADDRESS_LIST *list;
    DWORD rlen, llen, start;
    SOCKET l, s, fill[8];
    struct timeval tv;
    char service[16];
    BOOL ret;
    int i;

    l = listener(5, &port);
    sprintf(service, "%u", port);
    s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    rlen = llen = sizeof(remote);
    tv.tv_sec = 5; tv.tv_usec = 0;
    ret = WSAConnectByNameA(s, "127.0.0.1", service, &llen, (SOCKADDR *)&local, &rlen, (SOCKADDR *)&remote, &tv, NULL);
    printf("  connect by name: %d err %d remote port %u\n", ret, ret ? 0 : WSAGetLastError(), ntohs(remote.sin_port));
    check(ret && ntohs(remote.sin_port) == port && remote.sin_addr.s_addr == htonl(INADDR_LOOPBACK),
          "WSAConnectByName with a timeout connects and gives the address");
    {
        SOCKADDR_IN peer; int plen = sizeof(peer);
        check(!getpeername(s, (SOCKADDR *)&peer, &plen) && ntohs(peer.sin_port) == port,
              "... and the socket knows its peer");
    }
    closesocket(s);

    s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    ret = WSAConnectByNameW(s, L"localhost", L"0" , NULL, NULL, NULL, NULL, NULL, NULL);
    closesocket(s);
    s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    {
        WCHAR wservice[16];
        swprintf(wservice, 16, L"%u", port);
        ret = WSAConnectByNameW(s, L"localhost", wservice, NULL, NULL, NULL, NULL, NULL, NULL);
    }
    check(ret, "WSAConnectByNameW(localhost) connects");
    closesocket(s);

    /* a closed port first, then the listener */
    {
        unsigned short p;
        SOCKET t = listener(1, &p);
        closesocket(t);
        closed_port = p;
    }
    memset(addrs, 0, sizeof(addrs));
    for (i = 0; i < 2; i++)
    {
        addrs[i].sin_family = AF_INET;
        addrs[i].sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    }
    addrs[0].sin_port = htons(closed_port);
    addrs[1].sin_port = htons(port);
    list = malloc(sizeof(*list) + sizeof(SOCKET_ADDRESS));
    list->iAddressCount = 2;
    list->Address[0].lpSockaddr = (SOCKADDR *)&addrs[0];
    list->Address[0].iSockaddrLength = sizeof(addrs[0]);
    list->Address[1].lpSockaddr = (SOCKADDR *)&addrs[1];
    list->Address[1].iSockaddrLength = sizeof(addrs[1]);
    if (pWSAConnectByList)
    {
        s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        rlen = sizeof(remote);
        memset(&remote, 0, sizeof(remote));
        ret = pWSAConnectByList(s, list, NULL, NULL, &rlen, (SOCKADDR *)&remote, &tv, NULL);
        printf("  connect by list: %d err %d port %u\n", ret, ret ? 0 : WSAGetLastError(), ntohs(remote.sin_port));
        check(ret && ntohs(remote.sin_port) == port, "WSAConnectByList skips a closed port and connects to the next");
        closesocket(s);
    }
    else check(0, "WSAConnectByList is exported");
    free(list);
    closesocket(l);

    /* a listener whose queue is full does not answer: the timeout ends it */
    l = listener(0, &port);
    for (i = 0; i < 8; i++)
    {
        u_long nb = 1;
        SOCKADDR_IN sin = {0};
        fill[i] = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        ioctlsocket(fill[i], FIONBIO, &nb);
        sin.sin_family = AF_INET;
        sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        sin.sin_port = htons(port);
        connect(fill[i], (SOCKADDR *)&sin, sizeof(sin));
    }
    Sleep(300);
    sprintf(service, "%u", port);
    s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    tv.tv_sec = 0; tv.tv_usec = 700000;
    start = GetTickCount();
    ret = WSAConnectByNameA(s, "127.0.0.1", service, NULL, NULL, NULL, NULL, &tv, NULL);
    start = GetTickCount() - start;
    printf("  full queue: %d err %d after %lu ms\n", ret, ret ? 0 : WSAGetLastError(), start);
    check(!ret && WSAGetLastError() == WSAETIMEDOUT && start >= 600 && start < 3000,
          "WSAConnectByName to a listener that does not answer: WSAETIMEDOUT at the timeout");
    closesocket(s);
    for (i = 0; i < 8; i++) closesocket(fill[i]);
    closesocket(l);
}

static void test_lookup_cancel(void)
{
    HMODULE ws2 = GetModuleHandleA("ws2_32.dll");
    GetAddrInfoExW_t pGetAddrInfoExW = (void *)GetProcAddress(ws2, "GetAddrInfoExW");
    GetAddrInfoExCancel_t pGetAddrInfoExCancel = (void *)GetProcAddress(ws2, "GetAddrInfoExCancel");
    int i, cancelled = 0, consistent = 1, timed_out = 0, ret;

    for (i = 0; i < 40 && !cancelled; i++)
    {
        ADDRINFOEXW *result = (ADDRINFOEXW *)0xdeadbeef;
        OVERLAPPED ov = {0};
        HANDLE h = NULL;

        ov.hEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
        ret = pGetAddrInfoExW(L"sg-cancel-test.invalid", NULL, NS_DNS, NULL, NULL, &result, NULL, &ov, NULL, &h);
        if (ret != ERROR_IO_PENDING) { consistent = 0; break; }
        ret = pGetAddrInfoExCancel(&h);
        if (!ret)
        {
            cancelled = 1;
            if (WaitForSingleObject(ov.hEvent, 1000) || ov.Internal != WSA_E_CANCELLED) consistent = 0;
        }
        else if (ret != WSA_INVALID_HANDLE) consistent = 0;
        WaitForSingleObject(ov.hEvent, 10000);
        Sleep(50);  /* let the lookup finish in the background before the next */
        CloseHandle(ov.hEvent);
        if (!ret && result && result != (ADDRINFOEXW *)0xdeadbeef) consistent = 0;
    }
    printf("  cancelled after %d tries\n", i);
    check(cancelled && consistent, "GetAddrInfoExCancel stops a lookup: WSA_E_CANCELLED, result left alone");

    {
        HANDLE h = (HANDLE)0x1234;
        check(pGetAddrInfoExCancel(&h) == WSA_INVALID_HANDLE, "GetAddrInfoExCancel of no lookup: WSA_INVALID_HANDLE");
    }

    for (i = 0; i < 40 && !timed_out; i++)
    {
        ADDRINFOEXW *result = NULL;
        OVERLAPPED ov = {0};
        struct timeval tv = {0, 1};
        HANDLE h;

        ov.hEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
        ret = pGetAddrInfoExW(L"sg-timeout-test.invalid", NULL, NS_DNS, NULL, NULL, &result, &tv, &ov, NULL, &h);
        if (ret == ERROR_IO_PENDING && !WaitForSingleObject(ov.hEvent, 10000) && ov.Internal == WSAETIMEDOUT)
            timed_out = 1;
        Sleep(50);
        CloseHandle(ov.hEvent);
    }
    printf("  timed out after %d tries\n", i);
    check(timed_out, "an asynchronous lookup's timeout completes it with WSAETIMEDOUT");
}

static const GUID test_ns = {0x5ca1ab1e, 0x1655, 0x4e57, {0x8a, 0x10, 0x53, 0x47, 0x4f, 0x53, 0x00, 0x01}};
static const GUID test_class = {0x5ca1ab1e, 0x1655, 0x4e57, {0x8a, 0x10, 0x53, 0x47, 0x4f, 0x53, 0x00, 0x02}};
static const GUID test_provider = {0x5ca1ab1e, 0x1655, 0x4e57, {0x8a, 0x10, 0x53, 0x47, 0x4f, 0x53, 0x00, 0x03}};
static const GUID dns_provider = {0x22059d40, 0x7e9e, 0x11cf, {0xae, 0x5a, 0x00, 0xaa, 0x00, 0xa7, 0x11, 0x2b}};

static int find_ns(BOOL *active, int *pos)
{
    WSANAMESPACE_INFOW *ns;
    DWORD len = 0;
    int ret, i, found = 0;

    WSAEnumNameSpaceProvidersW(&len, NULL);
    ns = malloc(len);
    ret = WSAEnumNameSpaceProvidersW(&len, ns);
    for (i = 0; i < ret; i++)
        if (IsEqualGUID(&ns[i].NSProviderId, &test_ns))
        {
            found = 1;
            if (active) *active = ns[i].fActive;
            if (pos) *pos = i;
        }
    free(ns);
    return found;
}

static void test_catalog(void)
{
    WSCWriteNameSpaceOrder_t pWSCWriteNameSpaceOrder =
        (void *)GetProcAddress(GetModuleHandleA("ws2_32.dll"), "WSCWriteNameSpaceOrder");
    HANDLE change = NULL;
    OVERLAPPED ov = {0};
    BOOL active = TRUE;
    int ret, pos = -1, err;
    GUID id = test_ns, cls = test_class;

    /* catalog changes are announced */
    ret = WSAProviderConfigChange(&change, NULL, NULL);
    check(!ret && change, "WSAProviderConfigChange gives a handle");
    ov.hEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
    ret = WSAProviderConfigChange(&change, &ov, NULL);
    check(ret == SOCKET_ERROR && WSAGetLastError() == WSA_IO_PENDING, "... and waits for a change");

    WSCUnInstallNameSpace(&id);
    ret = WSCInstallNameSpace((WCHAR *)L"SG test name space", (WCHAR *)L"%SystemRoot%\\system32\\sgtestns.dll",
                              NS_NETBT, 3, &id);
    printf("  install name space: %d err %d\n", ret, ret ? WSAGetLastError() : 0);
    check(!ret, "WSCInstallNameSpace");
    check(WaitForSingleObject(ov.hEvent, 3000) == WAIT_OBJECT_0, "WSAProviderConfigChange completes on the change");
    CloseHandle(ov.hEvent);
    CloseHandle(change);

    check(WSCInstallNameSpace((WCHAR *)L"again", (WCHAR *)L"x.dll", NS_NETBT, 3, &id) == SOCKET_ERROR,
          "installing it twice is refused");
    check(find_ns(&active, &pos) && active, "WSAEnumNameSpaceProviders lists the installed provider, active");
    check(!WSCEnableNSProvider(&id, FALSE) && find_ns(&active, NULL) && !active, "WSCEnableNSProvider disables it");

    if (pWSCWriteNameSpaceOrder)
    {
        WSANAMESPACE_INFOW *ns;
        GUID order[16];
        DWORD len = 0;
        int n, i, newpos = -1;

        WSAEnumNameSpaceProvidersW(&len, NULL);
        ns = malloc(len);
        n = WSAEnumNameSpaceProvidersW(&len, ns);
        for (i = 0; i < n; i++) order[i] = ns[n - 1 - i].NSProviderId;
        free(ns);
        ret = pWSCWriteNameSpaceOrder(order, n);
        find_ns(NULL, &newpos);
        printf("  order: %d, position %d -> %d of %d\n", ret, pos, newpos, n);
        check(!ret && newpos == n - 1 - pos, "WSCWriteNameSpaceOrder reorders the providers");
        check(pWSCWriteNameSpaceOrder(order, n - 1) == WSAEINVAL, "... and refuses a partial order");
        for (i = 0; i < n; i++) order[i] = order[n - 1 - i];
        pWSCWriteNameSpaceOrder(order, n);
    }
    else check(0, "WSCWriteNameSpaceOrder is exported");

    check(!WSCUnInstallNameSpace(&id) && !find_ns(NULL, NULL), "WSCUnInstallNameSpace removes it");
    {
        GUID dns = dns_provider;
        check(WSCUnInstallNameSpace(&dns) == SOCKET_ERROR, "a built-in provider cannot be removed");
    }

    /* service classes */
    {
        WSANSCLASSINFOW ci[2];
        WSASERVICECLASSINFOW info;
        DWORD port = 4242, flag = 1, len;
        WCHAR name[64];
        union { WSASERVICECLASSINFOW i; BYTE b[1024]; } out;
        GUID dns = dns_provider;

        WSARemoveServiceClass(&cls);
        ci[0].lpszName = (WCHAR *)L"Port";
        ci[0].dwNameSpace = NS_DNS;
        ci[0].dwValueType = REG_DWORD;
        ci[0].dwValueSize = sizeof(port);
        ci[0].lpValue = &port;
        ci[1].lpszName = (WCHAR *)L"Flag";
        ci[1].dwNameSpace = NS_NETBT;
        ci[1].dwValueType = REG_DWORD;
        ci[1].dwValueSize = sizeof(flag);
        ci[1].lpValue = &flag;
        info.lpServiceClassId = &cls;
        info.lpszServiceClassName = (WCHAR *)L"SG Test Service";
        info.dwCount = 2;
        info.lpClassInfos = ci;
        ret = WSAInstallServiceClassW(&info);
        check(!ret, "WSAInstallServiceClass");
        check(WSAInstallServiceClassW(&info) == SOCKET_ERROR && WSAGetLastError() == WSAEALREADY,
              "... twice: WSAEALREADY");
        len = ARRAYSIZE(name);
        ret = WSAGetServiceClassNameByClassIdW(&cls, name, &len);
        check(!ret && !wcscmp(name, L"SG Test Service"), "WSAGetServiceClassNameByClassId");
        len = sizeof(out);
        ret = WSAGetServiceClassInfoW(&dns, &cls, &len, &out.i);
        printf("  class info: %d count %lu\n", ret, ret ? 0 : out.i.dwCount);
        check(!ret && out.i.dwCount == 1 && !wcscmp(out.i.lpClassInfos[0].lpszName, L"Port") &&
              *(DWORD *)out.i.lpClassInfos[0].lpValue == 4242,
              "WSAGetServiceClassInfo gives the DNS provider's entries");
        check(!WSARemoveServiceClass(&cls), "WSARemoveServiceClass");
        len = ARRAYSIZE(name);
        check(WSAGetServiceClassNameByClassIdW(&cls, name, &len) == SOCKET_ERROR &&
              WSAGetLastError() == WSATYPE_NOT_FOUND, "... then it is gone");
    }

    /* application categories */
    {
        static const WCHAR app[] = L"C:\\Program Files\\SG\\app.exe";
        DWORD cat = 0, prev = 0xdead;
        ret = WSCSetApplicationCategory(app, wcslen(app), NULL, 0, 0x4000 /* LSP_SYSTEM */, &prev, &err);
        check(!ret, "WSCSetApplicationCategory");
        ret = WSCGetApplicationCategory(app, wcslen(app), NULL, 0, &cat, &err);
        check(!ret && cat == 0x4000, "WSCGetApplicationCategory reads it back");
        ret = WSCSetApplicationCategory(app, wcslen(app), NULL, 0, 0, &prev, &err);
        check(!ret && prev == 0x4000, "... and setting again gives the previous one");
    }

    /* a recorded transport provider */
    {
        WSAPROTOCOL_INFOW pi;
        GUID prov = test_provider;
        WCHAR path[MAX_PATH];
        int plen = MAX_PATH;

        WSCDeinstallProvider(&prov, &err);
        memset(&pi, 0, sizeof(pi));
        pi.iAddressFamily = AF_INET;
        pi.iSocketType = SOCK_STREAM;
        pi.ProviderId = prov;
        wcscpy(pi.szProtocol, L"SG layered test");
        ret = WSCInstallProvider(&prov, L"%SystemRoot%\\system32\\sglsp.dll", &pi, 1, &err);
        check(!ret, "WSCInstallProvider");
        ret = WSCGetProviderPath(&prov, path, &plen, &err);
        check(!ret && !wcscmp(path, L"%SystemRoot%\\system32\\sglsp.dll"), "WSCGetProviderPath of the installed provider");
        check(!WSCDeinstallProvider(&prov, &err), "WSCDeinstallProvider");
        plen = MAX_PATH;
        check(WSCGetProviderPath(&prov, path, &plen, &err) == SOCKET_ERROR && err == WSAEINVAL, "... then it is gone");
    }
}

int main(void)
{
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
    test_connect();
    test_lookup_cancel();
    test_catalog();
    WSACleanup();
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
