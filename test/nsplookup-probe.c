/* Winsock name space services (patches/sg/1654), run by
 * test/nsplookup-gate.sh: the provider list, host and service lookups
 * through WSALookupService* (names, addresses, HOSTENT and SERVENT blobs,
 * buffer sizes, ANSI), the NLA network list with its blobs,
 * SIO_NSP_NOTIFY_CHANGE (polled, and an event completed when the lookup
 * ends), WSCGetProviderPath and WSCGetProviderInfo. These were stubs. */
#include <winsock2.h>
#include <ws2tcpip.h>
#include <ws2spi.h>
#include <mswsock.h>
#include <svcguid.h>
#include <stdio.h>

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) failures++;
}

static GUID hostname_class = SVCID_HOSTNAME, byname_class = SVCID_INET_HOSTADDRBYNAME,
            bystr_class = SVCID_INET_HOSTADDRBYINETSTRING, service_class = SVCID_INET_SERVICEBYNAME,
            nla_class = NLA_SERVICE_CLASS_GUID;
static const GUID tcp_provider = {0xe70f1aa0, 0xab8b, 0x11cf, {0x8c, 0xa3, 0x00, 0x80, 0x5f, 0x48, 0xa1, 0x92}};

static int begin(const WCHAR *name, GUID *class, DWORD ns, DWORD flags, HANDLE *h)
{
    WSAQUERYSETW q;
    memset(&q, 0, sizeof(q));
    q.dwSize = sizeof(q);
    q.lpszServiceInstanceName = (WCHAR *)name;
    q.lpServiceClassId = class;
    q.dwNameSpace = ns;
    return WSALookupServiceBeginW(&q, flags, h) ? WSAGetLastError() : 0;
}

int main(void)
{
    union { WSAQUERYSETW q; BYTE b[8192]; } buf;
    union { WSAQUERYSETA q; BYTE b[8192]; } bufA;
    WSANAMESPACE_INFOW *ns;
    WSADATA wsa;
    HANDLE h;
    DWORD len;
    int ret, err, i, dns = 0, nla = 0;

    WSAStartup(MAKEWORD(2, 2), &wsa);

    /* providers */
    len = 0;
    ret = WSAEnumNameSpaceProvidersW(&len, NULL);
    check(ret == SOCKET_ERROR && WSAGetLastError() == WSAEFAULT && len > 0, "WSAEnumNameSpaceProviders gives the size it needs");
    ns = malloc(len);
    ret = WSAEnumNameSpaceProvidersW(&len, ns);
    for (i = 0; i < ret; i++)
    {
        printf("  provider %lu %ls active %d\n", ns[i].dwNameSpace, ns[i].lpszIdentifier, ns[i].fActive);
        if (ns[i].dwNameSpace == NS_DNS && ns[i].fActive) dns = 1;
        if (ns[i].dwNameSpace == NS_NLA && ns[i].fActive) nla = 1;
    }
    check(ret >= 2 && dns && nla, "the DNS and NLA name space providers are listed");

    /* a host name */
    err = begin(L"localhost", &hostname_class, NS_DNS, LUP_RETURN_NAME | LUP_RETURN_ADDR, &h);
    check(!err, "WSALookupServiceBegin(localhost)");
    if (!err)
    {
        len = 16;
        ret = WSALookupServiceNextW(h, 0, &len, &buf.q);
        err = WSAGetLastError();
        check(ret == SOCKET_ERROR && err == WSAEFAULT && len > sizeof(WSAQUERYSETW), "a short buffer: WSAEFAULT and the size");
        len = sizeof(buf);
        ret = WSALookupServiceNextW(h, 0, &len, &buf.q);
        if (!ret)
        {
            int loop = 0;
            DWORD j;
            for (j = 0; j < buf.q.dwNumberOfCsAddrs; j++)
            {
                SOCKADDR *sa = buf.q.lpcsaBuffer[j].RemoteAddr.lpSockaddr;
                if (sa->sa_family == AF_INET && ((SOCKADDR_IN *)sa)->sin_addr.s_addr == htonl(INADDR_LOOPBACK)) loop = 1;
                if (sa->sa_family == AF_INET6 && IN6_IS_ADDR_LOOPBACK(&((SOCKADDR_IN6 *)sa)->sin6_addr)) loop = 1;
            }
            printf("  name %ls, %lu addresses, name space %lu\n", buf.q.lpszServiceInstanceName, buf.q.dwNumberOfCsAddrs, buf.q.dwNameSpace);
            check(buf.q.lpszServiceInstanceName && loop && buf.q.dwNameSpace == NS_DNS, "localhost: its name and the loopback address");
        }
        else check(0, "WSALookupServiceNext(localhost)");
        len = sizeof(buf);
        ret = WSALookupServiceNextW(h, 0, &len, &buf.q);
        check(ret == SOCKET_ERROR && WSAGetLastError() == WSA_E_NO_MORE, "... then no more");
        check(!WSALookupServiceEnd(h), "WSALookupServiceEnd");
    }
    check(WSALookupServiceEnd(h) == SOCKET_ERROR, "an ended lookup's handle is refused");

    /* HOSTENT blob */
    err = begin(L"localhost", &byname_class, NS_DNS, LUP_RETURN_BLOB, &h);
    if (!err)
    {
        len = sizeof(buf);
        ret = WSALookupServiceNextW(h, 0, &len, &buf.q);
        if (!ret && buf.q.lpBlob)
        {
            BYTE *base = buf.q.lpBlob->pBlobData;
            struct hostent *he = (struct hostent *)base;
            char **list = (char **)(base + (ULONG_PTR)he->h_addr_list);
            DWORD a = list[0] ? *(DWORD *)(base + (ULONG_PTR)list[0]) : 0;
            printf("  hostent %s type %d len %d first %#lx\n", base + (ULONG_PTR)he->h_name, he->h_addrtype, he->h_length, ntohl(a));
            check(he->h_addrtype == AF_INET && he->h_length == 4 && a == htonl(INADDR_LOOPBACK),
                  "SVCID_INET_HOSTADDRBYNAME: a HOSTENT blob with offsets");
        }
        else check(0, "SVCID_INET_HOSTADDRBYNAME blob");
        WSALookupServiceEnd(h);
    }
    else check(0, "SVCID_INET_HOSTADDRBYNAME begin");

    /* address literal */
    err = begin(L"10.1.2.3", &bystr_class, NS_DNS, LUP_RETURN_ADDR, &h);
    if (!err)
    {
        len = sizeof(buf);
        ret = WSALookupServiceNextW(h, 0, &len, &buf.q);
        check(!ret && buf.q.dwNumberOfCsAddrs == 1 &&
              ((SOCKADDR_IN *)buf.q.lpcsaBuffer[0].RemoteAddr.lpSockaddr)->sin_addr.s_addr == inet_addr("10.1.2.3"),
              "SVCID_INET_HOSTADDRBYINETSTRING gives the address");
        WSALookupServiceEnd(h);
    }
    else check(0, "SVCID_INET_HOSTADDRBYINETSTRING begin");

    /* service */
    err = begin(L"http/tcp", &service_class, NS_DNS, LUP_RETURN_BLOB | LUP_RETURN_NAME, &h);
    if (!err)
    {
        len = sizeof(buf);
        ret = WSALookupServiceNextW(h, 0, &len, &buf.q);
        if (!ret && buf.q.lpBlob)
        {
            struct servent *se = (struct servent *)buf.q.lpBlob->pBlobData;
            printf("  service %ls port %d\n", buf.q.lpszServiceInstanceName, ntohs(se->s_port));
            check(ntohs(se->s_port) == 80, "SVCID_INET_SERVICEBYNAME: a SERVENT blob with the port");
        }
        else check(0, "service blob");
        WSALookupServiceEnd(h);
    }
    else check(0, "SVCID_INET_SERVICEBYNAME begin");

    err = begin(L"no-such-host.invalid", &hostname_class, NS_DNS, LUP_RETURN_ADDR, &h);
    printf("  unknown host: %d\n", err);
    check(err == WSAHOST_NOT_FOUND || err == WSANO_DATA, "an unknown host: WSAHOST_NOT_FOUND");

    /* ANSI */
    {
        WSAQUERYSETA q;
        memset(&q, 0, sizeof(q));
        q.dwSize = sizeof(q);
        q.lpszServiceInstanceName = (char *)"localhost";
        q.lpServiceClassId = &hostname_class;
        q.dwNameSpace = NS_ALL;
        ret = WSALookupServiceBeginA(&q, LUP_RETURN_NAME, &h);
        if (!ret)
        {
            len = sizeof(bufA);
            ret = WSALookupServiceNextA(h, 0, &len, &bufA.q);
            check(!ret && bufA.q.lpszServiceInstanceName && strlen(bufA.q.lpszServiceInstanceName) > 0,
                  "WSALookupServiceNextA gives an ANSI name");
            WSALookupServiceEnd(h);
        }
        else check(0, "WSALookupServiceBeginA");
    }

    /* networks */
    err = begin(NULL, &nla_class, NS_NLA, LUP_RETURN_ALL, &h);
    check(!err, "NLA lookup begins");
    if (!err)
    {
        int count = 0, good = 1;
        WSAOVERLAPPED ov;
        WSACOMPLETION c;

        for (;;)
        {
            len = sizeof(buf);
            if (WSALookupServiceNextW(h, 0, &len, &buf.q)) break;
            count++;
            if (!buf.q.lpszServiceInstanceName || !buf.q.lpBlob) good = 0;
            else
            {
                NLA_BLOB *b = (NLA_BLOB *)buf.q.lpBlob->pBlobData;
                printf("  network %ls (%ls): ", buf.q.lpszServiceInstanceName, buf.q.lpszComment ? buf.q.lpszComment : L"");
                if (b->header.type != NLA_INTERFACE) good = 0;
                else printf("interface type %lu, adapter %s; ", b->data.interfaceData.dwType, b->data.interfaceData.adapterName);
                if (!b->header.nextOffset) good = 0;
                else
                {
                    b = (NLA_BLOB *)((BYTE *)b + b->header.nextOffset);
                    if (b->header.type != NLA_CONNECTIVITY) good = 0;
                    else printf("connectivity %d internet %d", b->data.connectivity.type, b->data.connectivity.internet);
                }
                printf("\n");
            }
        }
        check(WSAGetLastError() == WSA_E_NO_MORE && good, "each network has a name and interface + connectivity blobs");
        if (!count) printf("  (no connected networks here)\n");

        len = 0;
        ret = WSANSPIoctl(h, SIO_NSP_NOTIFY_CHANGE, NULL, 0, NULL, 0, &len, NULL);
        check(ret == SOCKET_ERROR && WSAGetLastError() == WSAEWOULDBLOCK, "SIO_NSP_NOTIFY_CHANGE polled: no change yet");

        memset(&ov, 0, sizeof(ov));
        ov.hEvent = WSACreateEvent();
        memset(&c, 0, sizeof(c));
        c.Type = NSP_NOTIFY_EVENT;
        c.Parameters.Event.lpOverlapped = &ov;
        ret = WSANSPIoctl(h, SIO_NSP_NOTIFY_CHANGE, NULL, 0, NULL, 0, &len, &c);
        check(ret == SOCKET_ERROR && WSAGetLastError() == WSA_IO_PENDING, "SIO_NSP_NOTIFY_CHANGE with an event: pending");
        check(WaitForSingleObject(ov.hEvent, 200) == WAIT_TIMEOUT, "... and not completed while nothing changes");
        WSALookupServiceEnd(h);
        check(WaitForSingleObject(ov.hEvent, 2000) == WAIT_OBJECT_0 && ov.Internal != 0 && ov.Internal != STATUS_PENDING,
              "ending the lookup completes the wait as cancelled");
        CloseHandle(ov.hEvent);
    }

    err = begin(L"localhost", &hostname_class, NS_DNS, 0, &h);
    if (!err)
    {
        ret = WSANSPIoctl(h, SIO_NSP_NOTIFY_CHANGE, NULL, 0, NULL, 0, &len, NULL);
        check(ret == SOCKET_ERROR && WSAGetLastError() == WSAEOPNOTSUPP, "the DNS provider has no change notifications");
        WSALookupServiceEnd(h);
    }

    /* provider paths and info */
    {
        WCHAR path[MAX_PATH];
        GUID id = tcp_provider, bogus = {1, 2, 3, {4}};
        int plen = MAX_PATH, code = 0;
        DWORD cat = 0xdead;
        size_t clen = sizeof(cat);

        ret = WSCGetProviderPath(&id, path, &plen, &code);
        printf("  TCP provider path: %d %ls (%d)\n", ret, ret ? L"" : path, code);
        check(!ret && !_wcsicmp(path, L"%SystemRoot%\\system32\\mswsock.dll"), "WSCGetProviderPath of the TCP provider");
        plen = MAX_PATH;
        ret = WSCGetProviderPath(&bogus, path, &plen, &code);
        check(ret == SOCKET_ERROR && code == WSAEINVAL, "WSCGetProviderPath of no provider: WSAEINVAL");
        ret = WSCGetProviderInfo(&id, ProviderInfoLspCategories, (BYTE *)&cat, &clen, 0, &code);
        check(!ret && cat == 0, "WSCGetProviderInfo: a base provider has no LSP categories");
    }

    WSACleanup();
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
