/* winhttp gap-A batch (patch 3221), run by test/fastwinhttp-gate.sh, 64- and 32-bit.
 * An in-process HTTP server on 127.0.0.1 (also used as a proxy) answers; nothing leaves the machine.
 *   options kept and handed down from the session, the NULL-handle options, WINHTTP_OPTION_PROXY on a
 *   session / request / the system, WinHttpQueryHeaders names, skipped 1xx answers, response header and
 *   drain bounds, EXPIRE_CONNECTION, MAX_CONNS_PER_SERVER, automatic decompression, request times and
 *   statistics, extended error, user agent, code page, shared credentials, the proxy's CONNECT answer,
 *   IWinHttpRequest::Option, SetClientCertificate and the response stream. */
#define COBJMACROS
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <winhttp.h>
#include <wincrypt.h>
#include <ole2.h>
#include <oaidl.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define O_HANDLE_TYPE 9
#define O_CALLBACK 1
#define O_CONNECT_RETRIES 4
#define O_EXTENDED_ERROR 24
#define O_PROXY 38
#define O_USER_AGENT 41
#define O_CODEPAGE 68
#define O_MAX_CONNS_PER_SERVER 73
#define O_MAX_CONNS_PER_1_0_SERVER 74
#define O_SECURE_PROTOCOLS 84
#define O_ENABLETRACING 85
#define O_MAX_REDIRECTS 89
#define O_MAX_STATUS_CONTINUE 90
#define O_MAX_HEADER_SIZE 91
#define O_MAX_DRAIN_SIZE 92
#define O_GLOBAL_PROXY_CREDS 97
#define O_GLOBAL_SERVER_CREDS 98
#define O_REJECT_USERPWD 100
#define O_USE_GLOBAL_SERVER_CREDS 101
#define O_RECEIVE_PROXY_CONNECT_RESPONSE 103
#define O_IS_PROXY_CONNECT_RESPONSE 104
#define O_DECOMPRESSION 118
#define O_ENABLE_HTTP_PROTOCOL 133
#define O_HTTP_PROTOCOL_USED 134
#define O_ENCODE_EXTRA 138
#define O_REQUEST_TIMES 142
#define O_EXPIRE_CONNECTION 143
#define O_REQUEST_STATS 146
#define O_SPN 96
#define O_HTTP_VERSION 59
#define O_WORKER_THREAD_COUNT 80
#define O_IPV6_FAST_FALLBACK 140
#define O_ASSURED_NON_BLOCKING_CALLBACKS 111
#define O_DISABLE_FEATURE 63
#define O_ENABLE_FEATURE 79
#define O_PARENT_HANDLE 21
#define O_PASSPORT_COBRANDING_TEXT 81

#define MY_PROXY_ACCESS_NAMED 3
#define TIMES_LAST 36
#define STATS_LAST 16

/* the layout of winhttp.h: packed at 4 on i386, 8 on x64 */
#ifdef _WIN64
#pragma pack(push, 8)
#else
#pragma pack(push, 4)
#endif
struct my_times { ULONG cTimes; unsigned long long t[64]; };
struct my_stats { unsigned long long flags; ULONG index; ULONG cStats; unsigned long long s[32]; };
#pragma pack(pop)
struct my_creds { char *user, *pass, *realm; DWORD scheme; char *host; DWORD port; char *url; };

#include "fastwinhttp-data.inc"

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}
#define ERR_IS(e) (GetLastError() == (e))

/* ---- the server ---- */
static unsigned short srv_port;
static SOCKET lsock = INVALID_SOCKET;
static volatile LONG accepts, active_conns, max_active;
static CRITICAL_SECTION srv_lock;
static char last_req[16384];
static HANDLE release_event;

static void send_all(SOCKET s, const void *buf, int len)
{
    const char *p = buf;
    while (len > 0)
    {
        int n = send(s, p, len, 0);
        if (n <= 0) return;
        p += n; len -= n;
    }
}

static void send_response(SOCKET s, const char *status, const char *headers, const void *body, int len)
{
    char head[20000];
    int n = sprintf(head, "HTTP/1.1 %s\r\n%sContent-Length: %d\r\n\r\n", status, headers, len);
    send_all(s, head, n);
    if (len) send_all(s, body, len);
}

static BOOL handle_request(SOCKET s, char *req)
{
    char path[512], *p;
    const char *q;
    char body[8192];
    int i, n;

    EnterCriticalSection(&srv_lock);
    strncpy(last_req, req, sizeof(last_req) - 1);
    LeaveCriticalSection(&srv_lock);

    if (!strncmp(req, "CONNECT ", 8))
    {
        send_response(s, "407 Proxy Authentication Required", "Proxy-Authenticate: Basic realm=\"p\"\r\nConnection: close\r\n", "proxy", 5);
        return FALSE;
    }
    q = strchr(req, ' ');
    if (!q) return FALSE;
    q++;
    p = path;
    while (*q && *q != ' ' && p < path + sizeof(path) - 1) *p++ = *q++;
    *p = 0;
    if (!strncmp(path, "http://", 7)) { p = strchr(path + 7, '/'); memmove(path, p ? p : "/", strlen(p ? p : "/") + 1); }

    if (strstr(path, "/echo"))
    {
        send_response(s, "200 OK", "", req, (int)strlen(req));
    }
    else if (strstr(path, "/plain"))
    {
        for (n = i = 0; i < 12; i++) n += sprintf(body + n, "%s", PLAIN);
        send_response(s, "200 OK", "", body, n);
    }
    else if (strstr(path, "/gzip")) send_response(s, "200 OK", "Content-Encoding: gzip\r\n", gz_data, sizeof(gz_data));
    else if (strstr(path, "/rawdeflate")) send_response(s, "200 OK", "Content-Encoding: deflate\r\n", raw_data, sizeof(raw_data));
    else if (strstr(path, "/deflate")) send_response(s, "200 OK", "Content-Encoding: deflate\r\n", zl_data, sizeof(zl_data));
    else if (strstr(path, "/hdrs"))
        send_response(s, "200 OK", "Content-Description: desc\r\nLink: <a>; rel=x\r\nForwarded: for=1.2.3.4\r\nFrom: me@example.org\r\n"
                      "Refresh: 5; url=/z\r\nContent-Disposition: attachment; filename=f\r\nProxy-Support: Session-Based-Authentication\r\n"
                      "Authentication-Info: nextnonce=\"n\"\r\n", "ok", 2);
    else if (strstr(path, "/cont"))
    {
        const char *r = "HTTP/1.1 100 Continue\r\n\r\nHTTP/1.1 102 Processing\r\nX-Info: skipped\r\n\r\n"
                        "HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nok";
        send_all(s, r, (int)strlen(r));
    }
    else if (strstr(path, "/big"))
    {
        char hdr[16384];
        for (n = i = 0; i < 100; i++) n += sprintf(hdr + n, "X-Pad-%d: aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\r\n", i);
        send_response(s, "200 OK", hdr, "ok", 2);
    }
    else if (strstr(path, "/drain"))
    {
        static char big[200000];
        memset(big, 'd', sizeof(big));
        send_response(s, "200 OK", "", big, sizeof(big));
    }
    else if (strstr(path, "/hold"))
    {
        send_response(s, "200 OK", "", "0123456789", 10);
    }
    else if (strstr(path, "/close")) { send_response(s, "200 OK", "Connection: close\r\n", "ok", 2); return FALSE; }
    else if (strstr(path, "/auth"))
    {
        if (strstr(req, "Authorization: Basic ")) send_response(s, "200 OK", "", "authed", 6);
        else send_response(s, "401 Unauthorized", "WWW-Authenticate: Basic realm=\"x\"\r\n", "", 0);
    }
    else if (strstr(path, "/ok")) send_response(s, "200 OK", "", "ok", 2);
    else send_response(s, "404 Not Found", "", "", 0);
    return TRUE;
}

static DWORD WINAPI conn_thread(void *arg)
{
    SOCKET s = (SOCKET)(ULONG_PTR)arg;
    char buf[16384];
    int len = 0, n;
    LONG a = InterlockedIncrement(&active_conns);
    LONG m;

    while ((m = max_active) < a) InterlockedCompareExchange(&max_active, a, m);
    for (;;)
    {
        char *end;
        buf[len] = 0;
        while (!(end = strstr(buf, "\r\n\r\n")))
        {
            if (len >= (int)sizeof(buf) - 1) goto done;
            n = recv(s, buf + len, sizeof(buf) - 1 - len, 0);
            if (n <= 0) goto done;
            len += n;
            buf[len] = 0;
        }
        end += 4;
        {
            char saved = *end;
            *end = 0;
            if (!handle_request(s, buf)) goto done;
            *end = saved;
        }
        len -= (int)(end - buf);
        memmove(buf, end, len + 1);
    }
done:
    closesocket(s);
    InterlockedDecrement(&active_conns);
    return 0;
}

static DWORD WINAPI accept_thread(void *arg)
{
    SOCKET ls = (SOCKET)(ULONG_PTR)arg;
    for (;;)
    {
        SOCKET c = accept(ls, NULL, NULL);
        if (c == INVALID_SOCKET) return 0;
        InterlockedIncrement(&accepts);
        CloseHandle(CreateThread(NULL, 0, conn_thread, (void *)(ULONG_PTR)c, 0, NULL));
    }
}

static SOCKET listen_on(int family, unsigned short port, unsigned short *ret_port)
{
    SOCKET s = socket(family, SOCK_STREAM, 0);
    struct sockaddr_storage ss;
    int len;
    memset(&ss, 0, sizeof(ss));
    if (family == AF_INET)
    {
        struct sockaddr_in *a = (struct sockaddr_in *)&ss;
        a->sin_family = AF_INET; a->sin_addr.s_addr = htonl(INADDR_LOOPBACK); a->sin_port = htons(port);
        len = sizeof(*a);
    }
    else
    {
        struct sockaddr_in6 *a = (struct sockaddr_in6 *)&ss;
        a->sin6_family = AF_INET6; a->sin6_addr = in6addr_loopback; a->sin6_port = htons(port);
        len = sizeof(*a);
    }
    if (s == INVALID_SOCKET || bind(s, (struct sockaddr *)&ss, len) || listen(s, 16))
    {
        if (s != INVALID_SOCKET) closesocket(s);
        return INVALID_SOCKET;
    }
    len = sizeof(ss);
    getsockname(s, (struct sockaddr *)&ss, &len);
    *ret_port = ntohs(family == AF_INET ? ((struct sockaddr_in *)&ss)->sin_port : ((struct sockaddr_in6 *)&ss)->sin6_port);
    return s;
}

/* ---- client helpers ---- */
static HINTERNET mkses(DWORD access, const WCHAR *proxy)
{
    return WinHttpOpen(L"sgwh", access, proxy, NULL, 0);
}

static BOOL run(HINTERNET req, DWORD *status, char *body, DWORD size, DWORD *len)
{
    DWORD sz = sizeof(*status), got, total = 0;

    if (!WinHttpSendRequest(req, NULL, 0, NULL, 0, 0, 0)) return FALSE;
    if (!WinHttpReceiveResponse(req, NULL)) return FALSE;
    if (status && !WinHttpQueryHeaders(req, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, NULL, status, &sz, NULL)) return FALSE;
    if (body)
    {
        while (total < size - 1 && WinHttpReadData(req, body + total, size - 1 - total, &got) && got) total += got;
        body[total] = 0;
        if (len) *len = total;
    }
    return TRUE;
}

static BOOL query_hdr(HINTERNET req, DWORD level, WCHAR *buf, DWORD size)
{
    DWORD sz = size;
    return WinHttpQueryHeaders(req, level, NULL, buf, &sz, NULL);
}

static BOOL qdw(HINTERNET h, DWORD opt, DWORD *value)
{
    DWORD size = sizeof(*value);
    return WinHttpQueryOption(h, opt, value, &size);
}
static BOOL sdw(HINTERNET h, DWORD opt, DWORD value)
{
    return WinHttpSetOption(h, opt, &value, sizeof(value));
}

static void test_options(void)
{
    HINTERNET ses = mkses(WINHTTP_ACCESS_TYPE_NO_PROXY, NULL), con, req;
    DWORD v, size;
    static const struct { DWORD opt; DWORD def; const char *name; } defaults[] =
    {
        { O_CONNECT_RETRIES, 5, "CONNECT_RETRIES" }, { O_MAX_REDIRECTS, 10, "MAX_HTTP_AUTOMATIC_REDIRECTS" },
        { O_MAX_STATUS_CONTINUE, 10, "MAX_HTTP_STATUS_CONTINUE" }, { O_MAX_HEADER_SIZE, 65536, "MAX_RESPONSE_HEADER_SIZE" },
        { O_MAX_DRAIN_SIZE, 1048576, "MAX_RESPONSE_DRAIN_SIZE" }, { O_MAX_CONNS_PER_SERVER, 0xffffffff, "MAX_CONNS_PER_SERVER" },
        { O_MAX_CONNS_PER_1_0_SERVER, 0xffffffff, "MAX_CONNS_PER_1_0_SERVER" },
    };
    unsigned int i;
    WCHAR str[64];
    HINTERNET parent;

    con = WinHttpConnect(ses, L"localhost", srv_port, 0);
    req = WinHttpOpenRequest(con, NULL, L"/ok", NULL, NULL, NULL, 0);

    for (i = 0; i < sizeof(defaults) / sizeof(defaults[0]); i++)
    {
        char what[100];
        v = 0x1234;
        sprintf(what, "session default %s", defaults[i].name);
        check(qdw(ses, defaults[i].opt, &v) && v == defaults[i].def, what);
        v = 0x1234;
        sprintf(what, "a connection answers %s as its session", defaults[i].name);
        check(qdw(con, defaults[i].opt, &v) && v == defaults[i].def, what);
    }
    /* requests that are not for the session handle */
    v = 0; size = sizeof(v);
    SetLastError(0);
    check(!qdw(req, O_MAX_CONNS_PER_SERVER, &v) && ERR_IS(ERROR_INVALID_PARAMETER), "MAX_CONNS_PER_SERVER is not a request option to query");
    check(!qdw(ses, O_ENCODE_EXTRA, &v) && ERR_IS(ERROR_INVALID_PARAMETER), "ENCODE_EXTRA cannot be queried");
    check(!qdw(req, O_DECOMPRESSION, &v) && ERR_IS(ERROR_INVALID_PARAMETER), "DECOMPRESSION cannot be queried");

    /* handle type, callback, parent */
    check(qdw(ses, O_HANDLE_TYPE, &v) && v == WINHTTP_HANDLE_TYPE_SESSION, "HANDLE_TYPE of a session");
    check(qdw(con, O_HANDLE_TYPE, &v) && v == WINHTTP_HANDLE_TYPE_CONNECT, "HANDLE_TYPE of a connection");
    check(qdw(req, O_HANDLE_TYPE, &v) && v == WINHTTP_HANDLE_TYPE_REQUEST, "HANDLE_TYPE of a request");
    check(!sdw(req, O_HANDLE_TYPE, 1), "HANDLE_TYPE cannot be set");
    {
        WINHTTP_STATUS_CALLBACK cb = (WINHTTP_STATUS_CALLBACK)0x1234;
        size = sizeof(cb);
        check(WinHttpQueryOption(ses, O_CALLBACK, &cb, &size) && cb == NULL, "no callback on a new session");
        cb = (WINHTTP_STATUS_CALLBACK)(ULONG_PTR)0x4321;
        check(WinHttpSetOption(ses, O_CALLBACK, &cb, sizeof(cb)), "set the callback with the option");
        cb = NULL;
        check(WinHttpQueryOption(ses, O_CALLBACK, &cb, &size) && cb == (WINHTTP_STATUS_CALLBACK)(ULONG_PTR)0x4321, "and read it back");
        cb = NULL;
        WinHttpSetOption(ses, O_CALLBACK, &cb, sizeof(cb));
    }
    parent = (HINTERNET)0x1234;
    size = sizeof(parent);
    check(WinHttpQueryOption(ses, O_PARENT_HANDLE, &parent, &size) && parent == NULL, "a session has no parent handle");
    parent = NULL;
    check(WinHttpQueryOption(req, O_PARENT_HANDLE, &parent, &size) && parent == con, "the parent of a request is its connection");

    /* session values reach requests opened afterwards, not those opened before */
    check(sdw(ses, O_MAX_REDIRECTS, 3), "set MAX_HTTP_AUTOMATIC_REDIRECTS on the session");
    check(qdw(ses, O_MAX_REDIRECTS, &v) && v == 3, "read back");
    check(qdw(req, O_MAX_REDIRECTS, &v) && v == 10, "a request opened before keeps 10");
    {
        HINTERNET req2 = WinHttpOpenRequest(con, NULL, L"/ok", NULL, NULL, NULL, 0);
        check(qdw(req2, O_MAX_REDIRECTS, &v) && v == 3, "a request opened after has 3");
        check(sdw(req2, O_MAX_REDIRECTS, 7) && qdw(req2, O_MAX_REDIRECTS, &v) && v == 7, "the request's own value");
        check(qdw(ses, O_MAX_REDIRECTS, &v) && v == 3, "does not change the session");
        WinHttpCloseHandle(req2);
    }
    check(sdw(ses, O_CONNECT_RETRIES, 9) && qdw(ses, O_CONNECT_RETRIES, &v) && v == 9, "CONNECT_RETRIES round trip");
    check(sdw(ses, O_MAX_STATUS_CONTINUE, 2) && qdw(ses, O_MAX_STATUS_CONTINUE, &v) && v == 2, "MAX_HTTP_STATUS_CONTINUE round trip");
    check(sdw(ses, O_MAX_CONNS_PER_SERVER, 0) && qdw(ses, O_MAX_CONNS_PER_SERVER, &v) && v == 2, "MAX_CONNS_PER_SERVER 0 becomes 2");
    check(sdw(ses, O_MAX_CONNS_PER_SERVER, 4) && qdw(ses, O_MAX_CONNS_PER_SERVER, &v) && v == 4, "MAX_CONNS_PER_SERVER 4");
    check(sdw(ses, O_MAX_CONNS_PER_1_0_SERVER, 6) && qdw(ses, O_MAX_CONNS_PER_1_0_SERVER, &v) && v == 6, "MAX_CONNS_PER_1_0_SERVER 6");
    SetLastError(0);
    check(!WinHttpSetOption(ses, O_CONNECT_RETRIES, &v, 2) && ERR_IS(ERROR_INSUFFICIENT_BUFFER), "a value of the wrong size");
    SetLastError(0);
    check(!sdw(con, O_CONNECT_RETRIES, 3) && ERR_IS(ERROR_WINHTTP_INCORRECT_HANDLE_TYPE), "a connection takes no option to set");
    SetLastError(0);
    check(!sdw(req, O_MAX_CONNS_PER_SERVER, 3) && ERR_IS(ERROR_WINHTTP_INCORRECT_HANDLE_TYPE), "a request has no MAX_CONNS_PER_SERVER");
    SetLastError(0);
    check(!sdw(ses, O_SPN, 1) && ERR_IS(ERROR_WINHTTP_INCORRECT_HANDLE_TYPE), "SPN is a request option");
    check(sdw(req, O_SPN, WINHTTP_ENABLE_SPN_SERVER_PORT), "SPN with the server port");
    SetLastError(0);
    check(!sdw(req, O_SPN, 2) && ERR_IS(ERROR_INVALID_PARAMETER), "SPN with an unknown bit");
    check(sdw(ses, O_DECOMPRESSION, WINHTTP_DECOMPRESSION_FLAG_ALL), "DECOMPRESSION all");
    SetLastError(0);
    check(!sdw(ses, O_DECOMPRESSION, 4) && ERR_IS(ERROR_INVALID_PARAMETER), "DECOMPRESSION with an unknown bit");
    SetLastError(0);
    check(!sdw(ses, O_ENABLE_HTTP_PROTOCOL, 8) && ERR_IS(ERROR_INVALID_PARAMETER), "ENABLE_HTTP_PROTOCOL with an unknown bit");
    check(sdw(ses, O_ENABLE_HTTP_PROTOCOL, WINHTTP_PROTOCOL_FLAG_HTTP2), "ENABLE_HTTP_PROTOCOL HTTP/2");
    check(sdw(req, O_ENCODE_EXTRA, 5), "ENCODE_EXTRA 5 (a BOOL)");
    check(sdw(ses, O_IPV6_FAST_FALLBACK, 2) && sdw(ses, O_IPV6_FAST_FALLBACK, 0), "IPV6_FAST_FALLBACK");
    check(sdw(ses, O_ASSURED_NON_BLOCKING_CALLBACKS, 1), "ASSURED_NON_BLOCKING_CALLBACKS");
    check(qdw(req, O_HTTP_PROTOCOL_USED, &v) && v == 0, "HTTP_PROTOCOL_USED is 0 for HTTP/1.1");
    check(sdw(req, 12, 4096) && qdw(req, 12, &v) && v == 4096, "READ_BUFFER_SIZE (no effect) round trip");
    check(sdw(req, 58, 2) && qdw(req, 58, &v) && v == 2, "REQUEST_PRIORITY round trip");

    /* secure protocols */
    check(qdw(ses, O_SECURE_PROTOCOLS, &v) && (v & WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2), "SECURE_PROTOCOLS default includes TLS 1.2");
    check(sdw(ses, O_SECURE_PROTOCOLS, WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2) && qdw(ses, O_SECURE_PROTOCOLS, &v) && v == WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2,
          "SECURE_PROTOCOLS set and read");

    /* user agent */
    size = sizeof(str);
    check(WinHttpQueryOption(ses, O_USER_AGENT, str, &size) && !wcscmp(str, L"sgwh"), "USER_AGENT of the session");
    check(WinHttpSetOption(ses, O_USER_AGENT, (void *)L"newagent/2", 10), "set USER_AGENT");
    size = sizeof(str);
    check(WinHttpQueryOption(ses, O_USER_AGENT, str, &size) && !wcscmp(str, L"newagent/2") && size == 10 * sizeof(WCHAR), "read it back");
    {
        char body[8192];
        HINTERNET r = WinHttpOpenRequest(con, NULL, L"/echo", NULL, NULL, NULL, 0);
        check(run(r, NULL, body, sizeof(body), NULL) && strstr(body, "User-Agent: newagent/2"), "the server sees the new user agent");
        WinHttpCloseHandle(r);
    }

    /* the request-only proxy credential options and others are untouched */
    check(!qdw(ses, O_PASSPORT_COBRANDING_TEXT, &v), "cobranding text of a session");

    WinHttpCloseHandle(req);
    WinHttpCloseHandle(con);
    WinHttpCloseHandle(ses);
}

static void test_null_handle(void)
{
    DWORD v, size;
    HTTP_VERSION_INFO info;
    WINHTTP_PROXY_INFO pi;

    size = sizeof(info);
    check(WinHttpQueryOption(NULL, O_HTTP_VERSION, &info, &size) && info.dwMajorVersion == 1 && info.dwMinorVersion == 1, "HTTP_VERSION defaults to 1.1");
    info.dwMajorVersion = 1; info.dwMinorVersion = 0;
    check(WinHttpSetOption(NULL, O_HTTP_VERSION, &info, sizeof(info)), "set HTTP_VERSION 1.0");
    info.dwMajorVersion = 5;
    check(WinHttpQueryOption(NULL, O_HTTP_VERSION, &info, &size) && info.dwMajorVersion == 1 && info.dwMinorVersion == 0, "read back");
    info.dwMajorVersion = 1; info.dwMinorVersion = 1;
    WinHttpSetOption(NULL, O_HTTP_VERSION, &info, sizeof(info));

    check(qdw(NULL, O_ENABLETRACING, &v) && v == 0, "tracing is off");
    check(sdw(NULL, O_ENABLETRACING, 1) && qdw(NULL, O_ENABLETRACING, &v) && v == 1, "tracing on");
    sdw(NULL, O_ENABLETRACING, 0);

    check(qdw(NULL, O_WORKER_THREAD_COUNT, &v) && v == 0, "worker thread count 0");
    check(sdw(NULL, O_WORKER_THREAD_COUNT, 0), "set once");
    SetLastError(0);
    check(!sdw(NULL, O_WORKER_THREAD_COUNT, 3), "a second time is refused");

    SetLastError(0);
    check(!sdw(NULL, O_MAX_REDIRECTS, 3) && ERR_IS(ERROR_INVALID_HANDLE), "other options need a handle");
    SetLastError(0);
    check(!qdw(NULL, O_CONNECT_RETRIES, &v) && ERR_IS(ERROR_INVALID_HANDLE), "queries too");

    size = sizeof(pi);
    memset(&pi, 0xcc, sizeof(pi));
    check(WinHttpQueryOption(NULL, O_PROXY, &pi, &size) && (pi.dwAccessType == WINHTTP_ACCESS_TYPE_NO_PROXY || pi.dwAccessType == MY_PROXY_ACCESS_NAMED),
          "the default proxy, with a NULL handle");
    if (pi.lpszProxy) GlobalFree(pi.lpszProxy);
    if (pi.lpszProxyBypass) GlobalFree(pi.lpszProxyBypass);

    /* the Winsock error behind a failure: a connection refused */
    {
        HINTERNET ses = mkses(WINHTTP_ACCESS_TYPE_NO_PROXY, NULL), con, req;
        unsigned short port;
        SOCKET s = listen_on(AF_INET, 0, &port);
        closesocket(s);
        con = WinHttpConnect(ses, L"127.0.0.1", port, 0);
        req = WinHttpOpenRequest(con, NULL, L"/ok", NULL, NULL, NULL, 0);
        SetLastError(0);
        check(!WinHttpSendRequest(req, NULL, 0, NULL, 0, 0, 0) && ERR_IS(ERROR_WINHTTP_CANNOT_CONNECT), "a refused connection is ERROR_WINHTTP_CANNOT_CONNECT");
        check(qdw(NULL, O_EXTENDED_ERROR, &v) && v == WSAECONNREFUSED, "EXTENDED_ERROR is WSAECONNREFUSED");
        v = 0;
        check(qdw(req, O_EXTENDED_ERROR, &v) && v == WSAECONNREFUSED, "also asked with a request handle");
        WinHttpCloseHandle(req);
        WinHttpCloseHandle(con);
        WinHttpCloseHandle(ses);
    }
}

static void test_query_headers(void)
{
    static const struct { DWORD level; const WCHAR *value; const char *name; } hdrs[] =
    {
        { WINHTTP_QUERY_CONTENT_DESCRIPTION, L"desc", "CONTENT_DESCRIPTION" },
        { WINHTTP_QUERY_LINK, L"<a>; rel=x", "LINK" },
        { WINHTTP_QUERY_FORWARDED, L"for=1.2.3.4", "FORWARDED" },
        { WINHTTP_QUERY_FROM, L"me@example.org", "FROM" },
        { WINHTTP_QUERY_REFRESH, L"5; url=/z", "REFRESH" },
        { WINHTTP_QUERY_CONTENT_DISPOSITION, L"attachment; filename=f", "CONTENT_DISPOSITION" },
        { WINHTTP_QUERY_PROXY_SUPPORT, L"Session-Based-Authentication", "PROXY_SUPPORT" },
        { WINHTTP_QUERY_AUTHENTICATION_INFO, L"nextnonce=\"n\"", "AUTHENTICATION_INFO" },
    };
    HINTERNET ses = mkses(WINHTTP_ACCESS_TYPE_NO_PROXY, NULL), con, req;
    WCHAR buf[128];
    unsigned int i;
    DWORD status;

    con = WinHttpConnect(ses, L"127.0.0.1", srv_port, 0);
    req = WinHttpOpenRequest(con, NULL, L"/hdrs", NULL, NULL, NULL, 0);
    check(run(req, &status, NULL, 0, NULL) && status == 200, "request with the legacy headers");
    for (i = 0; i < sizeof(hdrs) / sizeof(hdrs[0]); i++)
    {
        char what[64];
        memset(buf, 0, sizeof(buf));
        sprintf(what, "WINHTTP_QUERY_%s", hdrs[i].name);
        check(query_hdr(req, hdrs[i].level, buf, sizeof(buf)) && !wcscmp(buf, hdrs[i].value), what);
    }
    SetLastError(0);
    check(!query_hdr(req, WINHTTP_QUERY_COST, buf, sizeof(buf)) && ERR_IS(ERROR_WINHTTP_HEADER_NOT_FOUND), "COST is not supported");
    SetLastError(0);
    check(!query_hdr(req, WINHTTP_QUERY_MESSAGE_ID, buf, sizeof(buf)) && ERR_IS(ERROR_WINHTTP_HEADER_NOT_FOUND), "MESSAGE_ID is not supported");
    SetLastError(0);
    check(!query_hdr(req, WINHTTP_QUERY_PASSPORT_CONFIG, buf, sizeof(buf)) && ERR_IS(ERROR_WINHTTP_HEADER_NOT_FOUND), "PASSPORT_CONFIG finds nothing");
    SetLastError(0);
    check(!query_hdr(req, 79, buf, sizeof(buf)) && ERR_IS(ERROR_INVALID_PARAMETER), "79 is past the table");
    check(query_hdr(req, WINHTTP_QUERY_SERVER, buf, sizeof(buf)) == FALSE, "no Server header");
    WinHttpCloseHandle(req);
    WinHttpCloseHandle(con);
    WinHttpCloseHandle(ses);
}

static void test_response_limits(void)
{
    HINTERNET ses = mkses(WINHTTP_ACCESS_TYPE_NO_PROXY, NULL), con, req;
    DWORD status = 0;
    char body[512];
    LONG before;

    con = WinHttpConnect(ses, L"127.0.0.1", srv_port, 0);

    /* informational answers are skipped */
    req = WinHttpOpenRequest(con, NULL, L"/cont", NULL, NULL, NULL, 0);
    check(run(req, &status, body, sizeof(body), NULL) && status == 200 && !strcmp(body, "ok"), "100 and 102 answers are skipped");
    WinHttpCloseHandle(req);
    check(sdw(ses, O_MAX_STATUS_CONTINUE, 1), "ignore only one 1xx answer");
    req = WinHttpOpenRequest(con, NULL, L"/cont", NULL, NULL, NULL, 0);
    status = 0;
    check(run(req, &status, NULL, 0, NULL) && status == 102, "the second one is then the answer");
    WinHttpCloseHandle(req);
    check(sdw(ses, O_MAX_STATUS_CONTINUE, 0), "ignore none");
    req = WinHttpOpenRequest(con, NULL, L"/cont", NULL, NULL, NULL, 0);
    status = 0;
    check(run(req, &status, NULL, 0, NULL) && status == 100, "the first one is the answer");
    WinHttpCloseHandle(req);
    sdw(ses, O_MAX_STATUS_CONTINUE, 10);

    /* the header size bound */
    req = WinHttpOpenRequest(con, NULL, L"/big", NULL, NULL, NULL, 0);
    check(run(req, &status, NULL, 0, NULL) && status == 200, "a large header block passes by default");
    WinHttpCloseHandle(req);
    req = WinHttpOpenRequest(con, NULL, L"/big", NULL, NULL, NULL, 0);
    check(sdw(req, O_MAX_HEADER_SIZE, 2000) && qdw(req, O_MAX_HEADER_SIZE, &status) && status == 2000, "bound the header size at 2000");
    SetLastError(0);
    check(!run(req, &status, NULL, 0, NULL) && ERR_IS(ERROR_WINHTTP_HEADER_SIZE_OVERFLOW), "ERROR_WINHTTP_HEADER_SIZE_OVERFLOW");
    WinHttpCloseHandle(req);

    /* the drain bound: a body that is not read is thrown away with its connection */
    WinHttpCloseHandle(con);
    WinHttpCloseHandle(ses);
    ses = mkses(WINHTTP_ACCESS_TYPE_NO_PROXY, NULL);
    con = WinHttpConnect(ses, L"127.0.0.1", srv_port, 0);
    req = WinHttpOpenRequest(con, NULL, L"/drain", NULL, NULL, NULL, 0);
    check(WinHttpSendRequest(req, NULL, 0, NULL, 0, 0, 0) && WinHttpReceiveResponse(req, NULL), "an answer with a 200000 byte body");
    check(sdw(req, O_MAX_DRAIN_SIZE, 1000), "bound the drain at 1000");
    before = accepts;
    check(WinHttpSendRequest(req, NULL, 0, NULL, 0, 0, 0) && WinHttpReceiveResponse(req, NULL), "the same request again");
    check(accepts == before + 1, "it needed a new connection");
    WinHttpCloseHandle(req);

    req = WinHttpOpenRequest(con, NULL, L"/drain", NULL, NULL, NULL, 0);
    check(WinHttpSendRequest(req, NULL, 0, NULL, 0, 0, 0) && WinHttpReceiveResponse(req, NULL), "again, with the default bound");
    before = accepts;
    check(WinHttpSendRequest(req, NULL, 0, NULL, 0, 0, 0) && WinHttpReceiveResponse(req, NULL), "the same request again");
    check(accepts == before, "the body was drained and the connection reused");
    WinHttpCloseHandle(req);

    /* EXPIRE_CONNECTION */
    req = WinHttpOpenRequest(con, NULL, L"/ok", NULL, NULL, NULL, 0);
    check(run(req, &status, body, sizeof(body), NULL), "an ok request");
    WinHttpCloseHandle(req);
    req = WinHttpOpenRequest(con, NULL, L"/ok", NULL, NULL, NULL, 0);
    before = accepts;
    check(run(req, &status, body, sizeof(body), NULL) && accepts == before, "the next one reuses the connection");
    WinHttpCloseHandle(req);
    req = WinHttpOpenRequest(con, NULL, L"/ok", NULL, NULL, NULL, 0);
    check(WinHttpSetOption(req, O_EXPIRE_CONNECTION, NULL, 0), "EXPIRE_CONNECTION");
    check(run(req, &status, body, sizeof(body), NULL), "request with the connection expired");
    WinHttpCloseHandle(req);
    req = WinHttpOpenRequest(con, NULL, L"/ok", NULL, NULL, NULL, 0);
    before = accepts;
    check(run(req, &status, body, sizeof(body), NULL) && accepts == before + 1, "the next one needs a new connection");
    WinHttpCloseHandle(req);
    SetLastError(0);
    check(!WinHttpSetOption(ses, O_EXPIRE_CONNECTION, NULL, 0) && ERR_IS(ERROR_WINHTTP_INCORRECT_HANDLE_TYPE), "EXPIRE_CONNECTION is a request option");

    WinHttpCloseHandle(con);
    WinHttpCloseHandle(ses);
}

static DWORD WINAPI send_thread(void *arg)
{
    HINTERNET req = arg;
    return WinHttpSendRequest(req, NULL, 0, NULL, 0, 0, 0) ? 0 : GetLastError();
}

static void test_max_conns(void)
{
    HINTERNET ses = mkses(WINHTTP_ACCESS_TYPE_NO_PROXY, NULL), con, req1, req2, req3;
    DWORD status, err;
    char body[64];
    ULONGLONG t;

    con = WinHttpConnect(ses, L"127.0.0.1", srv_port, 0);
    check(sdw(ses, O_MAX_CONNS_PER_SERVER, 1), "one connection per server");
    check(WinHttpSetTimeouts(ses, 0, 1500, 5000, 5000), "connect timeout of 1.5 s");
    req1 = WinHttpOpenRequest(con, NULL, L"/hold", NULL, NULL, NULL, 0);
    req2 = WinHttpOpenRequest(con, NULL, L"/ok", NULL, NULL, NULL, 0);
    check(WinHttpSendRequest(req1, NULL, 0, NULL, 0, 0, 0) && WinHttpReceiveResponse(req1, NULL), "the first request is in use");
    t = GetTickCount64();
    SetLastError(0);
    err = send_thread(req2);
    check(err == ERROR_WINHTTP_TIMEOUT && GetTickCount64() - t >= 1000, "the second waits for a free connection and times out");
    WinHttpCloseHandle(req1);
    req3 = WinHttpOpenRequest(con, NULL, L"/ok", NULL, NULL, NULL, 0);
    check(run(req3, &status, body, sizeof(body), NULL) && status == 200, "after the first is closed another request goes");
    WinHttpCloseHandle(req3);

    /* two connections: two requests in use, the third waits */
    check(sdw(ses, O_MAX_CONNS_PER_SERVER, 2), "two connections per server");
    {
        HINTERNET a, b, c;
        a = WinHttpOpenRequest(con, NULL, L"/hold", NULL, NULL, NULL, 0);
        b = WinHttpOpenRequest(con, NULL, L"/hold", NULL, NULL, NULL, 0);
        c = WinHttpOpenRequest(con, NULL, L"/ok", NULL, NULL, NULL, 0);
        check(WinHttpSendRequest(a, NULL, 0, NULL, 0, 0, 0) && WinHttpReceiveResponse(a, NULL), "first in use");
        check(WinHttpSendRequest(b, NULL, 0, NULL, 0, 0, 0) && WinHttpReceiveResponse(b, NULL), "second in use");
        check(send_thread(c) == ERROR_WINHTTP_TIMEOUT, "the third waits");
        WinHttpCloseHandle(a);
        check(send_thread(c) == 0, "and goes once one is free");
        WinHttpCloseHandle(b);
        WinHttpCloseHandle(c);
    }
    WinHttpCloseHandle(req2);
    WinHttpCloseHandle(con);
    WinHttpCloseHandle(ses);
}

static void test_proxy(void)
{
    HINTERNET ses = mkses(WINHTTP_ACCESS_TYPE_NO_PROXY, NULL), con, req;
    WINHTTP_PROXY_INFO pi, got;
    WCHAR proxy[64];
    char body[8192];
    DWORD status, size;

    swprintf(proxy, 64, L"127.0.0.1:%u", srv_port);
    size = sizeof(got);
    memset(&got, 0xcc, sizeof(got));
    check(WinHttpQueryOption(ses, O_PROXY, &got, &size) && got.dwAccessType == WINHTTP_ACCESS_TYPE_NO_PROXY && !got.lpszProxy,
          "a session without proxy");

    pi.dwAccessType = MY_PROXY_ACCESS_NAMED;
    pi.lpszProxy = proxy;
    pi.lpszProxyBypass = L"*.local";
    check(WinHttpSetOption(ses, O_PROXY, &pi, sizeof(pi)), "set a named proxy on the session");
    size = sizeof(got);
    memset(&got, 0xcc, sizeof(got));
    check(WinHttpQueryOption(ses, O_PROXY, &got, &size) && got.dwAccessType == MY_PROXY_ACCESS_NAMED && got.lpszProxy && !wcscmp(got.lpszProxy, proxy) &&
          got.lpszProxyBypass && !wcscmp(got.lpszProxyBypass, L"*.local"), "query it");
    if (got.lpszProxy) GlobalFree(got.lpszProxy);
    if (got.lpszProxyBypass) GlobalFree(got.lpszProxyBypass);

    con = WinHttpConnect(ses, L"sgwh.invalid", 80, 0);
    req = WinHttpOpenRequest(con, NULL, L"/echo", NULL, NULL, NULL, 0);
    check(run(req, &status, body, sizeof(body), NULL) && status == 200, "a request through the proxy");
    check(strstr(body, "GET http://sgwh.invalid:80/echo HTTP/1.1") != NULL, "which has the absolute URL");
    check(strstr(body, "Host: sgwh.invalid") != NULL, "and the Host of the real server");
    WinHttpCloseHandle(req);
    WinHttpCloseHandle(con);

    /* a request with a proxy of its own */
    pi.dwAccessType = WINHTTP_ACCESS_TYPE_NO_PROXY;
    pi.lpszProxy = pi.lpszProxyBypass = NULL;
    check(WinHttpSetOption(ses, O_PROXY, &pi, sizeof(pi)), "the session goes direct again");
    con = WinHttpConnect(ses, L"sgwh.invalid", 80, 0);
    req = WinHttpOpenRequest(con, NULL, L"/echo", NULL, NULL, NULL, 0);
    size = sizeof(got);
    check(WinHttpQueryOption(req, O_PROXY, &got, &size) && got.dwAccessType == WINHTTP_ACCESS_TYPE_NO_PROXY, "a request inherits that");
    pi.dwAccessType = MY_PROXY_ACCESS_NAMED;
    pi.lpszProxy = proxy;
    pi.lpszProxyBypass = NULL;
    check(WinHttpSetOption(req, O_PROXY, &pi, sizeof(pi)), "set a proxy on the request");
    size = sizeof(got);
    check(WinHttpQueryOption(req, O_PROXY, &got, &size) && got.dwAccessType == MY_PROXY_ACCESS_NAMED && got.lpszProxy && !wcscmp(got.lpszProxy, proxy),
          "the request reports it");
    if (got.lpszProxy) GlobalFree(got.lpszProxy);
    if (got.lpszProxyBypass) GlobalFree(got.lpszProxyBypass);
    check(run(req, &status, body, sizeof(body), NULL) && status == 200 && strstr(body, "GET http://sgwh.invalid:80/echo HTTP/1.1"), "and its request goes through it");
    WinHttpCloseHandle(req);
    size = sizeof(got);
    {
        HINTERNET sesq = ses;
        check(WinHttpQueryOption(sesq, O_PROXY, &got, &size) && got.dwAccessType == WINHTTP_ACCESS_TYPE_NO_PROXY, "the session is unchanged");
    }
    SetLastError(0);
    pi.dwAccessType = MY_PROXY_ACCESS_NAMED;
    pi.lpszProxy = NULL;
    check(!WinHttpSetOption(ses, O_PROXY, &pi, sizeof(pi)) && ERR_IS(ERROR_INVALID_PARAMETER), "a named proxy needs a name");
    SetLastError(0);
    pi.dwAccessType = 99;
    check(!WinHttpSetOption(ses, O_PROXY, &pi, sizeof(pi)) && ERR_IS(ERROR_INVALID_PARAMETER), "an unknown access type");
    SetLastError(0);
    check(!WinHttpSetOption(ses, O_PROXY, &pi, 4) && ERR_IS(ERROR_INSUFFICIENT_BUFFER), "a short buffer");
    WinHttpCloseHandle(con);
    WinHttpCloseHandle(ses);
}

static void test_decompression(void)
{
    HINTERNET ses = mkses(WINHTTP_ACCESS_TYPE_NO_PROXY, NULL), con, req;
    char body[2048], expected[1024];
    DWORD status, len, avail, i;
    int n;

    for (n = i = 0; i < 12; i++) n += sprintf(expected + n, "%s", PLAIN);
    con = WinHttpConnect(ses, L"127.0.0.1", srv_port, 0);

    req = WinHttpOpenRequest(con, NULL, L"/gzip", NULL, NULL, NULL, 0);
    check(run(req, &status, body, sizeof(body), &len) && len == sizeof(gz_data) && !memcmp(body, gz_data, len), "without the option the gzip body comes as it is");
    WinHttpCloseHandle(req);

    check(sdw(ses, O_DECOMPRESSION, WINHTTP_DECOMPRESSION_FLAG_ALL), "decompress everything");
    req = WinHttpOpenRequest(con, NULL, L"/gzip", NULL, NULL, NULL, 0);
    check(run(req, &status, body, sizeof(body), &len) && len == (DWORD)n && !memcmp(body, expected, len), "a gzip body is inflated");
    WinHttpCloseHandle(req);
    req = WinHttpOpenRequest(con, NULL, L"/deflate", NULL, NULL, NULL, 0);
    check(run(req, &status, body, sizeof(body), &len) && len == (DWORD)n && !memcmp(body, expected, len), "a zlib deflate body is inflated");
    WinHttpCloseHandle(req);
    req = WinHttpOpenRequest(con, NULL, L"/rawdeflate", NULL, NULL, NULL, 0);
    check(run(req, &status, body, sizeof(body), &len) && len == (DWORD)n && !memcmp(body, expected, len), "so is a raw deflate body");
    WinHttpCloseHandle(req);
    req = WinHttpOpenRequest(con, NULL, L"/plain", NULL, NULL, NULL, 0);
    check(run(req, &status, body, sizeof(body), &len) && len == (DWORD)n && !memcmp(body, expected, len), "an unencoded body is untouched");
    WinHttpCloseHandle(req);

    /* the size that can be read at once is the inflated size */
    req = WinHttpOpenRequest(con, NULL, L"/gzip", NULL, NULL, NULL, 0);
    check(WinHttpSendRequest(req, NULL, 0, NULL, 0, 0, 0) && WinHttpReceiveResponse(req, NULL), "again");
    avail = 0;
    check(WinHttpQueryDataAvailable(req, &avail) && avail == (DWORD)n, "WinHttpQueryDataAvailable gives the inflated size");
    {
        char small[100];
        DWORD got = 0, total = 0;
        check(WinHttpReadData(req, small, sizeof(small), &got) && got == sizeof(small) && !memcmp(small, expected, got), "a read of 100 bytes");
        total = got;
        check(WinHttpQueryDataAvailable(req, &avail) && avail == (DWORD)n - total, "the rest is what remains");
        while (WinHttpReadData(req, body, sizeof(body), &got) && got) total += got;
        check(total == (DWORD)n, "all of it in the end");
        check(WinHttpQueryDataAvailable(req, &avail) && avail == 0, "nothing after");
    }
    WinHttpCloseHandle(req);

    /* the request asks for what is enabled */
    req = WinHttpOpenRequest(con, NULL, L"/echo", NULL, NULL, NULL, 0);
    check(WinHttpAddRequestHeaders(req, L"Accept-Encoding: br", ~0u, WINHTTP_ADDREQ_FLAG_ADD), "the caller asked for br");
    check(run(req, &status, body, sizeof(body), &len) && strstr(body, "Accept-Encoding: gzip, deflate\r\n") && !strstr(body, "br"), "WinHTTP replaces it with gzip, deflate");
    WinHttpCloseHandle(req);

    check(sdw(ses, O_DECOMPRESSION, WINHTTP_DECOMPRESSION_FLAG_GZIP), "gzip only");
    req = WinHttpOpenRequest(con, NULL, L"/echo", NULL, NULL, NULL, 0);
    check(run(req, &status, body, sizeof(body), &len) && strstr(body, "Accept-Encoding: gzip\r\n"), "Accept-Encoding: gzip");
    WinHttpCloseHandle(req);
    req = WinHttpOpenRequest(con, NULL, L"/deflate", NULL, NULL, NULL, 0);
    check(run(req, &status, body, sizeof(body), &len) && len == sizeof(zl_data) && !memcmp(body, zl_data, len), "a deflate body then stays compressed");
    WinHttpCloseHandle(req);

    /* on the request alone */
    check(sdw(ses, O_DECOMPRESSION, 0), "none on the session");
    req = WinHttpOpenRequest(con, NULL, L"/deflate", NULL, NULL, NULL, 0);
    check(sdw(req, O_DECOMPRESSION, WINHTTP_DECOMPRESSION_FLAG_DEFLATE), "deflate on the request");
    check(run(req, &status, body, sizeof(body), &len) && len == (DWORD)n && !memcmp(body, expected, len), "which inflates it");
    WinHttpCloseHandle(req);
    WinHttpCloseHandle(con);
    WinHttpCloseHandle(ses);
}

static void test_times_stats(void)
{
    HINTERNET ses = mkses(WINHTTP_ACCESS_TYPE_NO_PROXY, NULL), con, req;
    struct my_times times;
    struct my_stats stats;
    DWORD size, status;
    char body[1024];
    LARGE_INTEGER now;
    ULONGLONG before;

    con = WinHttpConnect(ses, L"localhost", srv_port, 0);
    /* throw away any connection to this server that an earlier test left */
    req = WinHttpOpenRequest(con, NULL, L"/ok", NULL, NULL, NULL, 0);
    WinHttpSetOption(req, O_EXPIRE_CONNECTION, NULL, 0);
    run(req, &status, body, sizeof(body), NULL);
    WinHttpCloseHandle(req);
    req = WinHttpOpenRequest(con, NULL, L"/plain", NULL, NULL, NULL, 0);
    QueryPerformanceCounter(&now);
    before = now.QuadPart;
    check(run(req, &status, body, sizeof(body), NULL), "a request");
    QueryPerformanceCounter(&now);

    memset(&times, 0, sizeof(times));
    times.cTimes = TIMES_LAST;
    size = sizeof(times);
    check(WinHttpQueryOption(req, O_REQUEST_TIMES, &times, &size) && times.cTimes == TIMES_LAST, "REQUEST_TIMES");
    check(times.t[17] >= before && times.t[17] <= (ULONGLONG)now.QuadPart, "SendRequestStart is a performance counter value of the call");
    check(times.t[21] >= times.t[17], "SendRequestEnd is later");
    check(times.t[22] >= times.t[21], "ReceiveResponseStart after it");
    check(times.t[25] >= times.t[22], "ReceiveResponseHeadersEnd after it");
    check(times.t[27] >= times.t[25] && times.t[27] <= (ULONGLONG)now.QuadPart, "ReceiveResponseEnd after the headers");
    check(times.t[2] && times.t[4] >= times.t[2], "connection acquisition is timed");
    check(times.t[7] && times.t[8] >= times.t[7], "connection establishment is timed");

    memset(&stats, 0, sizeof(stats));
    stats.cStats = STATS_LAST;
    size = sizeof(stats);
    check(WinHttpQueryOption(req, O_REQUEST_STATS, &stats, &size) && stats.cStats == STATS_LAST, "REQUEST_STATS");
    check(stats.s[6] > 20, "WinHttpRequestHeadersSize counts the request's headers");
    check(stats.s[8] > 20, "WinHttpResponseHeadersSize counts the response's headers");
    check(stats.s[10] == 540, "WinHttpResponseBodySize is the body read, 540");
    check(stats.index == 0 && (stats.flags & WINHTTP_REQUEST_STAT_FLAG_FIRST_REQUEST), "the first request on its connection: index 0 and the first-request flag");
    WinHttpCloseHandle(req);

    /* the second request on a connection */
    req = WinHttpOpenRequest(con, NULL, L"/plain", NULL, NULL, NULL, 0);
    check(run(req, &status, body, sizeof(body), NULL), "another request");
    size = sizeof(stats);
    stats.cStats = STATS_LAST;
    check(WinHttpQueryOption(req, O_REQUEST_STATS, &stats, &size) && stats.index >= 1 && !(stats.flags & WINHTTP_REQUEST_STAT_FLAG_FIRST_REQUEST),
          "it is not the first on its connection");
    WinHttpCloseHandle(req);
    SetLastError(0);
    check(!WinHttpQueryOption(ses, O_REQUEST_TIMES, &times, &size) && ERR_IS(ERROR_INVALID_PARAMETER), "a session has no times");
    WinHttpCloseHandle(con);
    WinHttpCloseHandle(ses);
}

static void test_codepage_encode(void)
{
    HINTERNET ses = mkses(WINHTTP_ACCESS_TYPE_NO_PROXY, NULL), con, req;
    char body[8192];
    DWORD status;

    con = WinHttpConnect(ses, L"127.0.0.1", srv_port, 0);
    req = WinHttpOpenRequest(con, NULL, L"/echo?q=\x00e9", NULL, NULL, NULL, 0);
    check(run(req, &status, body, sizeof(body), NULL) && strstr(body, "/echo?q=%C3%A9 "), "the default code page is UTF-8");
    WinHttpCloseHandle(req);

    check(sdw(ses, O_CODEPAGE, 1252), "code page 1252");
    SetLastError(0);
    check(!sdw(con, O_CODEPAGE, 1252), "the code page is a session option");
    req = WinHttpOpenRequest(con, NULL, L"/echo?q=\x00e9", NULL, NULL, NULL, 0);
    check(run(req, &status, body, sizeof(body), NULL) && strstr(body, "/echo?q=%E9 "), "the query is converted with it");
    WinHttpCloseHandle(req);
    sdw(ses, O_CODEPAGE, CP_UTF8);

    req = WinHttpOpenRequest(con, NULL, L"/echo?a=50%", NULL, NULL, NULL, 0);
    check(run(req, &status, body, sizeof(body), NULL) && strstr(body, "/echo?a=50% "), "a percent sign is left alone");
    WinHttpCloseHandle(req);
    req = WinHttpOpenRequest(con, NULL, L"/echo?a=50%", NULL, NULL, NULL, 0);
    check(sdw(req, O_ENCODE_EXTRA, 1), "ENCODE_EXTRA");
    check(run(req, &status, body, sizeof(body), NULL) && strstr(body, "/echo?a=50%25 "), "then it is encoded");
    WinHttpCloseHandle(req);

    req = WinHttpOpenRequest(con, NULL, L"/echo", NULL, NULL, NULL, 0);
    check(sdw(req, O_REJECT_USERPWD, 1), "REJECT_USERPWD_IN_URL");
    WinHttpCloseHandle(req);
    {
        HINTERNET con2 = WinHttpConnect(ses, L"user:pw@127.0.0.1", srv_port, 0), req2;
        req2 = WinHttpOpenRequest(con2, NULL, L"/echo", NULL, NULL, NULL, 0);
        sdw(req2, O_REJECT_USERPWD, 1);
        SetLastError(0);
        check(!WinHttpSendRequest(req2, NULL, 0, NULL, 0, 0, 0) && ERR_IS(ERROR_WINHTTP_INVALID_URL), "a server name with user:password@ is refused");
        WinHttpCloseHandle(req2);
        WinHttpCloseHandle(con2);
    }
    WinHttpCloseHandle(con);
    WinHttpCloseHandle(ses);
}

static void test_global_creds(void)
{
    HINTERNET ses = mkses(WINHTTP_ACCESS_TYPE_NO_PROXY, NULL), con = NULL, req;
    struct my_creds creds;
    DWORD status;
    char body[256];
    HKEY key;

    memset(&creds, 0, sizeof(creds));
    creds.user = (char *)"gu";
    creds.pass = (char *)"gp";
    creds.realm = (char *)"x";
    creds.scheme = WINHTTP_AUTH_SCHEME_BASIC;
    creds.host = (char *)"127.0.0.1";
    creds.port = srv_port;
    RegDeleteKeyValueW(HKEY_LOCAL_MACHINE, L"Software\\Microsoft\\Windows\\CurrentVersion\\Internet Settings", L"ShareCredsWithWinHttp");
    SetLastError(0);
    check(!WinHttpSetOption(NULL, O_GLOBAL_SERVER_CREDS, &creds, sizeof(creds)) && ERR_IS(ERROR_WINHTTP_INVALID_OPTION), "without the ShareCredsWithWinHttp policy: ERROR_WINHTTP_INVALID_OPTION");
    SetLastError(0);
    check(!sdw(ses, O_USE_GLOBAL_SERVER_CREDS, 1) && ERR_IS(ERROR_WINHTTP_INVALID_OPTION), "USE_GLOBAL_SERVER_CREDENTIALS too");

    if (RegCreateKeyExW(HKEY_LOCAL_MACHINE, L"Software\\Microsoft\\Windows\\CurrentVersion\\Internet Settings", 0, NULL, 0, KEY_WRITE, NULL, &key, NULL))
    {
        check(0, "could not make the policy key");
        goto done;
    }
    {
        DWORD one = 1;
        RegSetValueExW(key, L"ShareCredsWithWinHttp", 0, REG_DWORD, (BYTE *)&one, sizeof(one));
        RegCloseKey(key);
    }
    check(WinHttpSetOption(NULL, O_GLOBAL_SERVER_CREDS, &creds, sizeof(creds)), "with the policy: the shared credentials are taken");
    check(sdw(ses, O_USE_GLOBAL_SERVER_CREDS, 1), "USE_GLOBAL_SERVER_CREDENTIALS");
    con = WinHttpConnect(ses, L"127.0.0.1", srv_port, 0);
    req = WinHttpOpenRequest(con, NULL, L"/auth", NULL, NULL, NULL, 0);
    status = 0;
    check(run(req, &status, body, sizeof(body), NULL) && status == 200 && !strcmp(body, "authed"), "a 401 is answered with them");
    WinHttpCloseHandle(req);
    WinHttpCloseHandle(con);
    WinHttpCloseHandle(ses);

    ses = mkses(WINHTTP_ACCESS_TYPE_NO_PROXY, NULL);
    con = WinHttpConnect(ses, L"127.0.0.1", srv_port, 0);
    req = WinHttpOpenRequest(con, NULL, L"/auth", NULL, NULL, NULL, 0);
    check(run(req, &status, body, sizeof(body), NULL) && status == 401, "not without the USE option");
    WinHttpCloseHandle(req);
    RegDeleteKeyValueW(HKEY_LOCAL_MACHINE, L"Software\\Microsoft\\Windows\\CurrentVersion\\Internet Settings", L"ShareCredsWithWinHttp");
done:
    if (con) WinHttpCloseHandle(con);
    WinHttpCloseHandle(ses);
}

static void test_proxy_connect(void)
{
    HINTERNET ses = mkses(WINHTTP_ACCESS_TYPE_NO_PROXY, NULL), con, req;
    WINHTTP_PROXY_INFO pi;
    WCHAR proxy[64];
    DWORD status, v, len;
    char body[256];

    swprintf(proxy, 64, L"127.0.0.1:%u", srv_port);
    pi.dwAccessType = MY_PROXY_ACCESS_NAMED;
    pi.lpszProxy = proxy;
    pi.lpszProxyBypass = NULL;
    check(WinHttpSetOption(ses, O_PROXY, &pi, sizeof(pi)), "a proxy that refuses every tunnel");
    con = WinHttpConnect(ses, L"sgwh.invalid", 443, 0);
    req = WinHttpOpenRequest(con, NULL, L"/x", NULL, NULL, NULL, WINHTTP_FLAG_SECURE);
    check(qdw(req, O_IS_PROXY_CONNECT_RESPONSE, &v) && v == 0, "IS_PROXY_CONNECT_RESPONSE is 0 at first");
    check(sdw(req, O_RECEIVE_PROXY_CONNECT_RESPONSE, 1), "RECEIVE_PROXY_CONNECT_RESPONSE");
    status = 0;
    check(run(req, &status, body, sizeof(body), &len), "the request completes");
    check(status == 407, "with the proxy's 407");
    check(qdw(req, O_IS_PROXY_CONNECT_RESPONSE, &v) && v == 1, "IS_PROXY_CONNECT_RESPONSE is 1");
    check(len == 5 && !strcmp(body, "proxy"), "and its entity can be read");
    WinHttpCloseHandle(req);

    req = WinHttpOpenRequest(con, NULL, L"/x", NULL, NULL, NULL, WINHTTP_FLAG_SECURE);
    SetLastError(0);
    check(!WinHttpSendRequest(req, NULL, 0, NULL, 0, 0, 0), "without the option the request fails");
    check(qdw(req, O_IS_PROXY_CONNECT_RESPONSE, &v) && v == 0, "and IS_PROXY_CONNECT_RESPONSE stays 0");
    WinHttpCloseHandle(req);
    WinHttpCloseHandle(con);
    WinHttpCloseHandle(ses);
}

static void test_retries(void)
{
    HINTERNET ses, con, req;
    unsigned short port6 = 0;
    SOCKET s6;
    ADDRINFOA hints, *res, *p;
    BOOL has4 = FALSE, has6 = FALSE;
    DWORD status;
    char body[64];

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    if (getaddrinfo("localhost", NULL, &hints, &res)) { printf("SKIP  localhost does not resolve\n"); return; }
    for (p = res; p; p = p->ai_next) { if (p->ai_family == AF_INET) has4 = TRUE; if (p->ai_family == AF_INET6) has6 = TRUE; }
    freeaddrinfo(res);
    if (!has4 || !has6) { printf("SKIP  localhost does not have both address families\n"); return; }
    s6 = listen_on(AF_INET6, 0, &port6);
    if (s6 == INVALID_SOCKET) { printf("SKIP  no IPv6 loopback\n"); return; }
    CloseHandle(CreateThread(NULL, 0, accept_thread, (void *)(ULONG_PTR)s6, 0, NULL));

    ses = mkses(WINHTTP_ACCESS_TYPE_NO_PROXY, NULL);
    con = WinHttpConnect(ses, L"localhost", port6, 0);

    check(sdw(ses, O_CONNECT_RETRIES, 1), "one connection attempt");
    req = WinHttpOpenRequest(con, NULL, L"/ok", NULL, NULL, NULL, 0);
    SetLastError(0);
    check(!WinHttpSendRequest(req, NULL, 0, NULL, 0, 0, 0), "localhost is tried at its IPv4 address only, which refuses");
    WinHttpCloseHandle(req);
    WinHttpCloseHandle(con);
    WinHttpCloseHandle(ses);

    ses = mkses(WINHTTP_ACCESS_TYPE_NO_PROXY, NULL);
    con = WinHttpConnect(ses, L"localhost", port6, 0);
    req = WinHttpOpenRequest(con, NULL, L"/ok", NULL, NULL, NULL, 0);
    check(run(req, &status, body, sizeof(body), NULL) && status == 200, "with the default five attempts the IPv6 address is reached");
    WinHttpCloseHandle(req);
    WinHttpCloseHandle(con);
    WinHttpCloseHandle(ses);

    ses = mkses(WINHTTP_ACCESS_TYPE_NO_PROXY, NULL);
    check(sdw(ses, O_IPV6_FAST_FALLBACK, 1), "IPv6 fast fallback");
    con = WinHttpConnect(ses, L"localhost", port6, 0);
    req = WinHttpOpenRequest(con, NULL, L"/ok", NULL, NULL, NULL, 0);
    check(run(req, &status, body, sizeof(body), NULL) && status == 200, "it connects over IPv6 first");
    WinHttpCloseHandle(req);
    WinHttpCloseHandle(con);
    WinHttpCloseHandle(ses);
    closesocket(s6);
}

/* ---- IWinHttpRequest ---- */
static DISPID getid(IDispatch *d, const WCHAR *name)
{
    DISPID id = -1;
    LPOLESTR n = (LPOLESTR)name;
    if (FAILED(IDispatch_GetIDsOfNames(d, &IID_NULL, &n, 1, 0, &id))) return -1;
    return id;
}

static HRESULT call(IDispatch *d, const WCHAR *name, WORD flags, VARIANT *args, int nargs, VARIANT *res)
{
    DISPPARAMS dp;
    DISPID named = DISPID_PROPERTYPUT, id = getid(d, name);
    HRESULT hr;
    EXCEPINFO ei;
    UINT err;
    if (id == -1) return DISP_E_UNKNOWNNAME;
    dp.rgvarg = args;
    dp.cArgs = nargs;
    dp.rgdispidNamedArgs = (flags & DISPATCH_PROPERTYPUT) ? &named : NULL;
    dp.cNamedArgs = (flags & DISPATCH_PROPERTYPUT) ? 1 : 0;
    if (res) VariantInit(res);
    memset(&ei, 0, sizeof(ei));
    hr = IDispatch_Invoke(d, id, &IID_NULL, 0, flags, &dp, res, &ei, &err);
    if (hr == DISP_E_EXCEPTION) hr = ei.scode;
    return hr;
}

static HRESULT opt_put(IDispatch *d, LONG opt, VARIANT v)
{
    VARIANT a[2];

    /* rgvarg is in reverse order: the value of the property put comes first, then the option */
    a[0] = v;
    VariantInit(&a[1]);
    V_VT(&a[1]) = VT_I4;
    V_I4(&a[1]) = opt;
    return call(d, L"Option", DISPATCH_PROPERTYPUT, a, 2, NULL);
}

static HRESULT opt_get(IDispatch *d, LONG opt, VARIANT *res)
{
    VARIANT arg;
    V_VT(&arg) = VT_I4;
    V_I4(&arg) = opt;
    return call(d, L"Option", DISPATCH_PROPERTYGET, &arg, 1, res);
}

static VARIANT vbool(BOOL b) { VARIANT v; VariantInit(&v); V_VT(&v) = VT_BOOL; V_BOOL(&v) = b ? VARIANT_TRUE : VARIANT_FALSE; return v; }
static VARIANT vi4(LONG l) { VARIANT v; VariantInit(&v); V_VT(&v) = VT_I4; V_I4(&v) = l; return v; }
static VARIANT vstr(const WCHAR *s) { VARIANT v; VariantInit(&v); V_VT(&v) = VT_BSTR; V_BSTR(&v) = SysAllocString(s); return v; }

#define CHECK_BOOL_OPT(d, opt, def, name) do { \
        VARIANT r; \
        check(SUCCEEDED(opt_get(d, opt, &r)) && V_VT(&r) == VT_BOOL && !!V_BOOL(&r) == (def), name " default"); \
        check(SUCCEEDED(opt_put(d, opt, vbool(!(def)))) && SUCCEEDED(opt_get(d, opt, &r)) && !!V_BOOL(&r) == !(def), name " set"); \
    } while (0)

static void test_com_options(void)
{
    IDispatch *d = NULL;
    HRESULT hr;
    VARIANT r, args[4], async;
    CLSID clsid;
    WCHAR url[128];
    IStream *stream, *clone, *mem;
    STATSTG st;
    ULARGE_INTEGER n, rd, wr;
    LARGE_INTEGER zero;
    char buf[1024];
    ULONG got;

    CoInitialize(NULL);
    hr = CLSIDFromProgID(L"WinHttp.WinHttpRequest.5.1", &clsid);
    check(hr == S_OK, "the WinHttpRequest class");
    hr = CoCreateInstance(&clsid, NULL, CLSCTX_INPROC_SERVER, &IID_IDispatch, (void **)&d);
    check(hr == S_OK && d, "a WinHttpRequest object");
    if (!d) return;

    /* options before Open */
    check(SUCCEEDED(opt_get(d, 0, &r)) && V_VT(&r) == VT_BSTR && wcsstr(V_BSTR(&r), L"WinHttp.WinHttpRequest.5"), "UserAgentString default");
    VariantClear(&r);
    check(SUCCEEDED(opt_put(d, 0, vstr(L"gate-agent"))) && SUCCEEDED(opt_get(d, 0, &r)) && !wcscmp(V_BSTR(&r), L"gate-agent"), "UserAgentString set");
    VariantClear(&r);
    hr = opt_get(d, 1, &r);
    check(hr == HRESULT_FROM_WIN32(ERROR_WINHTTP_CANNOT_CALL_BEFORE_OPEN), "URL before Open");
    check(SUCCEEDED(opt_get(d, 2, &r)) && V_I4(&r) == CP_UTF8, "URLCodePage default UTF-8");
    check(SUCCEEDED(opt_get(d, 4, &r)) && V_I4(&r) == 0, "SslErrorIgnoreFlags default 0");
    CHECK_BOOL_OPT(d, 3, TRUE, "EscapePercentInURL");
    CHECK_BOOL_OPT(d, 7, TRUE, "UrlEscapeDisable");
    CHECK_BOOL_OPT(d, 8, TRUE, "UrlEscapeDisableQuery");
    CHECK_BOOL_OPT(d, 6, TRUE, "EnableRedirects");
    check(SUCCEEDED(opt_get(d, 9, &r)) && V_I4(&r) == 0x28, "SecureProtocols default 0x28");
    check(SUCCEEDED(opt_put(d, 9, vi4(0x800))) && SUCCEEDED(opt_get(d, 9, &r)) && V_I4(&r) == 0x800, "SecureProtocols set");
    CHECK_BOOL_OPT(d, 10, FALSE, "EnableTracing");
    opt_put(d, 10, vbool(FALSE));
    CHECK_BOOL_OPT(d, 11, TRUE, "RevertImpersonationOverSsl");
    CHECK_BOOL_OPT(d, 12, FALSE, "EnableHttpsToHttpRedirects");
    CHECK_BOOL_OPT(d, 13, FALSE, "EnablePassportAuthentication");
    check(SUCCEEDED(opt_get(d, 14, &r)) && V_I4(&r) == 10, "MaxAutomaticRedirects default 10");
    check(SUCCEEDED(opt_put(d, 14, vi4(3))) && SUCCEEDED(opt_get(d, 14, &r)) && V_I4(&r) == 3, "MaxAutomaticRedirects set");
    check(SUCCEEDED(opt_get(d, 15, &r)) && V_I4(&r) == 65536, "MaxResponseHeaderSize default 64K");
    check(SUCCEEDED(opt_put(d, 15, vi4(8000))) && SUCCEEDED(opt_get(d, 15, &r)) && V_I4(&r) == 8000, "MaxResponseHeaderSize set");
    check(SUCCEEDED(opt_get(d, 16, &r)) && V_I4(&r) == 1048576, "MaxResponseDrainSize default 1M");
    check(SUCCEEDED(opt_put(d, 16, vi4(5000))) && SUCCEEDED(opt_get(d, 16, &r)) && V_I4(&r) == 5000, "MaxResponseDrainSize set");
    CHECK_BOOL_OPT(d, 17, TRUE, "EnableHttp1_1");
    opt_put(d, 17, vbool(TRUE));
    CHECK_BOOL_OPT(d, 18, FALSE, "EnableCertificateRevocationCheck");
    hr = opt_get(d, 5, &r);
    check(FAILED(hr), "SelectCertificate cannot be read");
    hr = opt_get(d, 99, &r);
    check(hr == HRESULT_FROM_WIN32(ERROR_WINHTTP_INVALID_OPTION), "an unknown option");
    opt_put(d, 12, vbool(FALSE));
    opt_put(d, 14, vi4(10));
    opt_put(d, 15, vi4(65536));
    opt_put(d, 16, vi4(1048576));
    opt_put(d, 9, vi4(0x800));

    /* after Open: URL, the user agent on the wire, the response stream */
    swprintf(url, 128, L"http://127.0.0.1:%u/echo", srv_port);
    VariantInit(&async);
    V_VT(&async) = VT_BOOL; V_BOOL(&async) = VARIANT_FALSE;
    args[1] = vstr(L"GET"); args[0] = vstr(url);
    {
        /* Open(Method, Url, Async): arguments in reverse order */
        VARIANT a[3];
        a[2] = args[1]; a[1] = args[0]; a[0] = async;
        hr = call(d, L"Open", DISPATCH_METHOD, a, 3, NULL);
        check(hr == S_OK, "Open");
    }
    check(SUCCEEDED(opt_get(d, 1, &r)) && V_VT(&r) == VT_BSTR && !wcscmp(V_BSTR(&r), url), "URL after Open");
    VariantClear(&r);
    hr = opt_put(d, 1, vstr(L"http://x/"));
    check(hr == HRESULT_FROM_WIN32(ERROR_WINHTTP_OPTION_NOT_SETTABLE), "URL cannot be set");
    hr = call(d, L"Send", DISPATCH_METHOD, NULL, 0, NULL);
    check(hr == S_OK, "Send");
    hr = call(d, L"ResponseText", DISPATCH_PROPERTYGET, NULL, 0, &r);
    check(hr == S_OK && V_VT(&r) == VT_BSTR && wcsstr(V_BSTR(&r), L"User-Agent: gate-agent"), "the user agent set before Open is sent");
    VariantClear(&r);

    hr = call(d, L"ResponseStream", DISPATCH_PROPERTYGET, NULL, 0, &r);
    check(hr == S_OK && V_VT(&r) == VT_UNKNOWN, "ResponseStream");
    stream = NULL;
    if (hr == S_OK) IUnknown_QueryInterface(V_UNKNOWN(&r), &IID_IStream, (void **)&stream);
    check(stream != NULL, "an IStream");
    if (stream)
    {
        ISequentialStream *seq = NULL;
        memset(&st, 0xcc, sizeof(st));
        check(IStream_Stat(stream, &st, STATFLAG_NONAME) == S_OK && st.type == STGTY_STREAM && st.cbSize.QuadPart > 100, "Stat gives the size");
        n.QuadPart = 5;
        check(IStream_Write(stream, "x", 1, &got) == STG_E_ACCESSDENIED, "Write is refused");
        check(IStream_SetSize(stream, n) == STG_E_ACCESSDENIED, "SetSize is refused");
        check(IStream_LockRegion(stream, n, n, LOCK_WRITE) == STG_E_INVALIDFUNCTION, "LockRegion is not supported");
        check(IStream_UnlockRegion(stream, n, n, LOCK_WRITE) == STG_E_INVALIDFUNCTION, "nor UnlockRegion");
        check(IStream_Commit(stream, 0) == S_OK && IStream_Revert(stream) == S_OK, "Commit and Revert do nothing");
        check(IStream_QueryInterface(stream, &IID_ISequentialStream, (void **)&seq) == S_OK && seq, "ISequentialStream");
        if (seq) ISequentialStream_Release(seq);
        zero.QuadPart = 0;
        IStream_Seek(stream, zero, STREAM_SEEK_SET, NULL);
        mem = NULL;
        CreateStreamOnHGlobal(NULL, TRUE, &mem);
        n.QuadPart = 10;
        check(IStream_CopyTo(stream, mem, n, &rd, &wr) == S_OK && rd.QuadPart == 10 && wr.QuadPart == 10, "CopyTo of 10 bytes");
        check(IStream_Clone(stream, &clone) == S_OK && clone, "Clone");
        if (clone)
        {
            got = 0;
            memset(buf, 0, sizeof(buf));
            check(IStream_Read(clone, buf, 4, &got) == S_OK && got == 4 && !strncmp(buf, "HTTP", 4), "the clone reads from the same position");
            got = 0;
            check(IStream_Read(stream, buf, 4, &got) == S_OK && got == 4 && !strncmp(buf, "HTTP", 4), "the original is at 10 and is unaffected");
            IStream_Release(clone);
        }
        n.QuadPart = ~0ull;
        zero.QuadPart = 0;
        IStream_Seek(mem, zero, STREAM_SEEK_SET, NULL);
        got = 0;
        memset(buf, 0, sizeof(buf));
        check(IStream_Read(mem, buf, 10, &got) == S_OK && got == 10 && !strncmp(buf, "GET /echo ", 10), "what was copied is the start of the body");
        IStream_Release(mem);
        IStream_Release(stream);
    }
    VariantClear(&r);
    IDispatch_Release(d);
}

static void test_client_cert(void)
{
    IDispatch *d = NULL;
    CLSID clsid;
    HRESULT hr;
    VARIANT args[3], async, r;
    HCERTSTORE store;
    PCCERT_CONTEXT cert;
    CERT_NAME_BLOB subject;
    BYTE enc[256];
    DWORD enclen = sizeof(enc);
    WCHAR url[128];
    HCRYPTPROV prov;
    HCRYPTKEY key;
    CRYPT_KEY_PROV_INFO kpi;
    WCHAR cont[] = L"sgwh-container";

    CLSIDFromProgID(L"WinHttp.WinHttpRequest.5.1", &clsid);
    hr = CoCreateInstance(&clsid, NULL, CLSCTX_INPROC_SERVER, &IID_IDispatch, (void **)&d);
    if (!d) { check(0, "a WinHttpRequest object"); return; }

    hr = call(d, L"SetClientCertificate", DISPATCH_METHOD, &(VARIANT){ .vt = VT_BSTR, .bstrVal = SysAllocString(L"CURRENT_USER\\MY\\x") }, 1, NULL);
    check(hr == HRESULT_FROM_WIN32(ERROR_WINHTTP_CANNOT_CALL_BEFORE_OPEN), "SetClientCertificate before Open");

    swprintf(url, 128, L"https://127.0.0.1:%u/echo", srv_port);
    VariantInit(&async);
    V_VT(&async) = VT_BOOL; V_BOOL(&async) = VARIANT_FALSE;
    args[2] = vstr(L"GET"); args[1] = vstr(url); args[0] = async;
    check(call(d, L"Open", DISPATCH_METHOD, args, 3, NULL) == S_OK, "Open an https request");

    /* a self-signed certificate "sgwh-test-subject" in the current user's MY store */
    store = CertOpenStore(CERT_STORE_PROV_SYSTEM_W, 0, 0, CERT_SYSTEM_STORE_CURRENT_USER, L"MY");
    check(store != NULL, "the MY store");
    CryptAcquireContextW(&prov, cont, NULL, PROV_RSA_FULL, CRYPT_DELETEKEYSET);
    check(CryptAcquireContextW(&prov, cont, NULL, PROV_RSA_FULL, CRYPT_NEWKEYSET), "a key container");
    CryptGenKey(prov, AT_KEYEXCHANGE, 0x4000000 /* 1024 bits */, &key);
    CryptDestroyKey(key);
    enclen = sizeof(enc);
    check(CertStrToNameW(X509_ASN_ENCODING, L"CN=sgwh-test-subject", 0, NULL, enc, &enclen, NULL), "a subject name");
    subject.cbData = enclen;
    subject.pbData = enc;
    memset(&kpi, 0, sizeof(kpi));
    kpi.pwszContainerName = cont;
    kpi.dwProvType = PROV_RSA_FULL;
    kpi.dwKeySpec = AT_KEYEXCHANGE;
    cert = CertCreateSelfSignCertificate(prov, &subject, 0, &kpi, NULL, NULL, NULL, NULL);
    check(cert != NULL, "a self-signed certificate");
    if (cert)
    {
        check(CertAddCertificateContextToStore(store, cert, CERT_STORE_ADD_ALWAYS, NULL), "added to the store");
        CertFreeCertificateContext(cert);
    }
    hr = call(d, L"SetClientCertificate", DISPATCH_METHOD, &(VARIANT){ .vt = VT_BSTR, .bstrVal = SysAllocString(L"CURRENT_USER\\MY\\sgwh-test-subject") }, 1, NULL);
    check(hr == S_OK, "SetClientCertificate with location, store and subject");
    hr = call(d, L"SetClientCertificate", DISPATCH_METHOD, &(VARIANT){ .vt = VT_BSTR, .bstrVal = SysAllocString(L"CURRENT_USER\\Personal\\sgwh-test-subject") }, 1, NULL);
    check(hr == S_OK, "the store may be called Personal");
    hr = call(d, L"SetClientCertificate", DISPATCH_METHOD, &(VARIANT){ .vt = VT_BSTR, .bstrVal = SysAllocString(L"sgwh-test-subject") }, 1, NULL);
    check(hr == S_OK, "a subject alone: CURRENT_USER and MY");
    hr = call(d, L"SetClientCertificate", DISPATCH_METHOD, &(VARIANT){ .vt = VT_BSTR, .bstrVal = SysAllocString(L"CURRENT_USER\\MY\\") }, 1, NULL);
    check(hr == S_OK, "a blank subject takes the first certificate");
    hr = call(d, L"SetClientCertificate", DISPATCH_METHOD, &(VARIANT){ .vt = VT_BSTR, .bstrVal = SysAllocString(L"CURRENT_USER\\MY\\no such subject") }, 1, NULL);
    check(hr == CRYPT_E_NOT_FOUND, "a subject that is not there: CRYPT_E_NOT_FOUND");
    hr = call(d, L"SetClientCertificate", DISPATCH_METHOD, &(VARIANT){ .vt = VT_BSTR, .bstrVal = SysAllocString(L"NOWHERE\\MY\\sgwh-test-subject") }, 1, NULL);
    check(hr == E_INVALIDARG, "an unknown location");
    hr = opt_put(d, 5, vstr(L"CURRENT_USER\\MY\\sgwh-test-subject"));
    check(hr == S_OK, "the SelectCertificate option does the same");

    /* clean up the store and the container */
    cert = NULL;
    while ((cert = CertFindCertificateInStore(store, X509_ASN_ENCODING, 0, CERT_FIND_SUBJECT_STR_W, L"sgwh-test-subject", NULL)))
        CertDeleteCertificateFromStore(cert);
    CertCloseStore(store, 0);
    CryptReleaseContext(prov, 0);
    CryptAcquireContextW(&prov, cont, NULL, PROV_RSA_FULL, CRYPT_DELETEKEYSET);
    IDispatch_Release(d);
}

#define RUN(n) do { if (!only || !strcmp(only, #n)) test_##n(); } while (0)

int main(int argc, char **argv)
{
    const char *only = argc > 1 ? argv[1] : NULL;
    WSADATA wsa;
    HANDLE th;

    WSAStartup(MAKEWORD(2, 2), &wsa);
    CoInitialize(NULL);
    InitializeCriticalSection(&srv_lock);
    release_event = CreateEventW(NULL, TRUE, FALSE, NULL);
    lsock = listen_on(AF_INET, 0, &srv_port);
    if (lsock == INVALID_SOCKET) { printf("FAIL  no server\n"); return 1; }
    th = CreateThread(NULL, 0, accept_thread, (void *)(ULONG_PTR)lsock, 0, NULL);
    CloseHandle(th);

    RUN(options);
    RUN(null_handle);
    RUN(query_headers);
    RUN(response_limits);
    RUN(max_conns);
    RUN(proxy);
    RUN(decompression);
    RUN(times_stats);
    RUN(codepage_encode);
    RUN(global_creds);
    RUN(proxy_connect);
    RUN(retries);
    RUN(com_options);
    RUN(client_cert);

    printf("DONE\n");
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    fflush(stdout);
    return failures != 0;
}
