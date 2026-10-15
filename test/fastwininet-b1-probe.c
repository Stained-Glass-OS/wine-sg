/* wininet batch 1 probe: runs under Wine, prints "ok"/"FAIL" lines and a RESULT line.
 * It starts its own tiny HTTP and FTP servers on 127.0.0.1. */
#define __USE_MINGW_ANSI_STDIO 0
#include <winsock2.h>
#include <windows.h>
#include <wininet.h>
#include <wincred.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures, checks;

#define CHECK(cond, ...) do { checks++; if (cond) printf("ok   %s\n", #cond); \
    else { failures++; printf("FAIL %s:%d %s: ", __FILE__, __LINE__, #cond); printf(__VA_ARGS__); printf("\n"); } } while (0)

/* ------------------------------------------------------------ servers */
static volatile LONG stop_servers;
static SOCKET http_ls, ftp_ls;
static int http_port, ftp_port;
static char ftp_last_user[128];
static volatile LONG ftp_size_supported = 1, ftp_150_hint = 1;

static SOCKET listen_on(int *port)
{
    struct sockaddr_in sa;
    int len = sizeof(sa);
    SOCKET s = socket(AF_INET, SOCK_STREAM, 0);

    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    bind(s, (struct sockaddr *)&sa, sizeof(sa));
    listen(s, 8);
    getsockname(s, (struct sockaddr *)&sa, &len);
    *port = ntohs(sa.sin_port);
    return s;
}

static SOCKET accept_to(SOCKET ls, int ms)
{
    fd_set set;
    struct timeval tv;

    FD_ZERO(&set);
    FD_SET(ls, &set);
    tv.tv_sec = ms / 1000;
    tv.tv_usec = (ms % 1000) * 1000;
    if (select(0, &set, NULL, NULL, &tv) <= 0) return INVALID_SOCKET;
    return accept(ls, NULL, NULL);
}

static int recv_line(SOCKET s, char *buf, int size)
{
    int n = 0;
    char c;

    while (n < size - 1)
    {
        fd_set set;
        struct timeval tv = { 10, 0 };
        FD_ZERO(&set);
        FD_SET(s, &set);
        if (select(0, &set, NULL, NULL, &tv) <= 0) return -1;
        if (recv(s, &c, 1, 0) <= 0) return -1;
        if (c == '\n') break;
        if (c != '\r') buf[n++] = c;
    }
    buf[n] = 0;
    return n;
}

static void send_str(SOCKET s, const char *str) { send(s, str, strlen(str), 0); }

static DWORD WINAPI http_conn(void *arg)
{
    SOCKET s = (SOCKET)(ULONG_PTR)arg;
    char line[512], path[128];
    char resp[512];

    for (;;)
    {
        int n;
        path[0] = 0;
        /* read the request headers, up to the empty line */
        if ((n = recv_line(s, line, sizeof(line))) < 0) break;
        do
        {
            if (!strncmp(line, "GET ", 4)) sscanf(line + 4, "%127s", path);
            if (!n) break;
        } while ((n = recv_line(s, line, sizeof(line))) >= 0);
        if (n < 0 || !path[0]) break;
        sprintf(resp, "HTTP/1.1 200 OK\r\nContent-Length: 20\r\n%sConnection: keep-alive\r\n\r\n",
                !strcmp(path, "/ranges") ? "Accept-Ranges: bytes\r\n" : "");
        send_str(s, resp);
        send_str(s, "0123456789abcdefghij");
    }
    closesocket(s);
    return 0;
}

static DWORD WINAPI http_thread(void *arg)
{
    while (!stop_servers)
    {
        SOCKET c = accept_to(http_ls, 200);
        if (c != INVALID_SOCKET) CloseHandle(CreateThread(NULL, 0, http_conn, (void *)(ULONG_PTR)c, 0, NULL));
    }
    return 0;
}

#define FTP_BODY_SIZE 1234

static void ftp_data(SOCKET pasv, SOCKET ctrl, const char *what, int is_list, int hint)
{
    SOCKET d = accept_to(pasv, 5000);
    char msg[160];

    if (d == INVALID_SOCKET) { send_str(ctrl, "425 no data connection\r\n"); return; }
    if (hint) sprintf(msg, "150 Opening data connection for %s (%d bytes).\r\n", what, FTP_BODY_SIZE);
    else sprintf(msg, "150 Opening data connection for %s.\r\n", what);
    send_str(ctrl, msg);
    if (is_list)
        send_str(d, "+i1.2,m1000000000,/,\tsubdir\r\n+i1.3,m1000000000,r,s321,up644,\tfile with space.txt\r\n");
    else
    {
        char buf[FTP_BODY_SIZE];
        memset(buf, 'x', sizeof(buf));
        send(d, buf, sizeof(buf), 0);
    }
    closesocket(d);
    send_str(ctrl, "226 Transfer complete.\r\n");
}

static DWORD WINAPI ftp_conn(void *arg)
{
    SOCKET s = (SOCKET)(ULONG_PTR)arg, pasv = INVALID_SOCKET;
    char line[512], msg[160];

    send_str(s, "220 test ftp ready\r\n");
    while (recv_line(s, line, sizeof(line)) >= 0)
    {
        if (!strncmp(line, "USER ", 5))
        {
            strncpy(ftp_last_user, line + 5, sizeof(ftp_last_user) - 1);
            send_str(s, "331 password please\r\n");
        }
        else if (!strncmp(line, "PASS ", 5)) send_str(s, "230 logged in\r\n");
        else if (!strncmp(line, "TYPE ", 5)) send_str(s, "200 type set\r\n");
        else if (!strncmp(line, "SIZE ", 5))
        {
            if (ftp_size_supported)
            {
                sprintf(msg, "213 %d\r\n", FTP_BODY_SIZE);
                send_str(s, msg);
            }
            else send_str(s, "500 SIZE not understood\r\n");
        }
        else if (!strncmp(line, "PASV", 4))
        {
            int port;
            if (pasv != INVALID_SOCKET) closesocket(pasv);
            pasv = listen_on(&port);
            sprintf(msg, "227 Entering Passive Mode (127,0,0,1,%d,%d)\r\n", port >> 8, port & 255);
            send_str(s, msg);
        }
        else if (!strncmp(line, "RETR ", 5)) ftp_data(pasv, s, line + 5, 0, ftp_150_hint);
        else if (!strncmp(line, "STOR ", 5)) ftp_data(pasv, s, line + 5, 0, 0);
        else if (!strncmp(line, "LIST", 4)) ftp_data(pasv, s, "list", 1, 0);
        else if (!strncmp(line, "QUIT", 4)) { send_str(s, "221 bye\r\n"); break; }
        else send_str(s, "200 ok\r\n");
    }
    if (pasv != INVALID_SOCKET) closesocket(pasv);
    closesocket(s);
    return 0;
}

static DWORD WINAPI ftp_thread(void *arg)
{
    while (!stop_servers)
    {
        SOCKET c = accept_to(ftp_ls, 200);
        if (c != INVALID_SOCKET) CloseHandle(CreateThread(NULL, 0, ftp_conn, (void *)(ULONG_PTR)c, 0, NULL));
    }
    return 0;
}

/* ------------------------------------------------------------ helpers */
typedef DWORD (WINAPI *PrivSet)(DWORD, DWORD, DWORD, LPCWSTR);
typedef DWORD (WINAPI *PrivGet)(DWORD, DWORD, LPDWORD, LPWSTR, LPDWORD);
typedef BOOL (WINAPI *InitAutoProxy)(DWORD);
typedef DWORD (WINAPI *DialW)(HWND, LPWSTR, DWORD, DWORD_PTR *, DWORD);
typedef BOOL (WINAPI *GoOnlineW)(LPWSTR, HWND, DWORD);
typedef DWORD (WINAPI *HangUp)(DWORD_PTR, DWORD);
typedef BOOL (WINAPI *MD5Hash)(PWSTR, PWSTR, PWSTR, PBYTE);
typedef BOOL (WINAPI *Resume)(HINTERNET, DWORD);
typedef BOOL (WINAPI *ShowSecW)(LPCWSTR, HWND);
typedef BOOL (WINAPI *ShowSecA)(LPCSTR, HWND);
typedef BOOL (WINAPI *GopherGetType)(LPCSTR, LPDWORD);
typedef DWORD (WINAPI *Attempt)(DWORD);

static HMODULE wi;
#define PROC(name) ((void *)GetProcAddress(wi, name))

static HINTERNET open_request(HINTERNET con, const char *path, DWORD flags)
{
    HINTERNET req = HttpOpenRequestA(con, "GET", path, NULL, NULL, NULL, flags, 0);
    BOOL ret;
    if (!req) return NULL;
    ret = HttpSendRequestA(req, NULL, 0, NULL, 0);
    if (!ret) { InternetCloseHandle(req); return NULL; }
    return req;
}

static void test_privacy(void)
{
    PrivSet set = PROC("PrivacySetZonePreferenceW");
    PrivGet get = PROC("PrivacyGetZonePreferenceW");
    WCHAR buf[128];
    DWORD ret, tmpl, len;

    CHECK(set && get, "exports");
    /* defaults and plain templates */
    tmpl = 99;
    ret = get(3, 0, &tmpl, NULL, NULL);
    CHECK(ret == 0 && tmpl <= 5, "default ret %lu tmpl %lu", ret, tmpl);
    ret = set(3, 0, PRIVACY_TEMPLATE_HIGH, NULL);
    CHECK(ret == 0, "set high %lu", ret);
    ret = set(3, 1, PRIVACY_TEMPLATE_LOW, NULL);
    CHECK(ret == 0, "set low 3rd party %lu", ret);
    tmpl = 99;
    ret = get(3, 0, &tmpl, NULL, NULL);
    CHECK(ret == 0 && tmpl == PRIVACY_TEMPLATE_HIGH, "first party kept ret %lu tmpl %lu", ret, tmpl);
    tmpl = 99;
    ret = get(3, 1, &tmpl, NULL, NULL);
    CHECK(ret == 0 && tmpl == PRIVACY_TEMPLATE_LOW, "third party kept ret %lu tmpl %lu", ret, tmpl);
    tmpl = 99;
    ret = get(1, 0, &tmpl, NULL, NULL);
    CHECK(ret == 0 && tmpl != PRIVACY_TEMPLATE_HIGH, "other zone not changed ret %lu tmpl %lu", ret, tmpl);

    /* validation */
    CHECK(set(9, 0, 1, NULL) == ERROR_INVALID_PARAMETER, "bad zone");
    CHECK(set(3, 2, 1, NULL) == ERROR_INVALID_PARAMETER, "bad type");
    CHECK(set(3, 0, 6, NULL) == ERROR_INVALID_PARAMETER, "bad template");
    CHECK(get(9, 0, &tmpl, NULL, NULL) == ERROR_INVALID_PARAMETER, "get bad zone");
    CHECK(set(3, 0, PRIVACY_TEMPLATE_CUSTOM, NULL) == ERROR_INVALID_PARAMETER, "custom without string");
    CHECK(set(3, 0, PRIVACY_TEMPLATE_CUSTOM, L"/=a/") == ERROR_INVALID_PARAMETER, "custom without signature");

    /* custom string */
    ret = set(3, 0, PRIVACY_TEMPLATE_CUSTOM, L"IE6-P3PSettings/V1: /DEM=d/ /=a/");
    CHECK(ret == 0, "custom set %lu", ret);
    tmpl = 0;
    len = 0;
    ret = get(3, 0, &tmpl, NULL, &len);
    CHECK(ret == ERROR_MORE_DATA && len == 33, "size query ret %lu len %lu", ret, len);
    len = 5;
    ret = get(3, 0, &tmpl, buf, &len);
    CHECK(ret == ERROR_MORE_DATA && len == 33, "small buffer ret %lu len %lu", ret, len);
    len = 128;
    memset(buf, 0, sizeof(buf));
    ret = get(3, 0, &tmpl, buf, &len);
    CHECK(ret == 0 && tmpl == PRIVACY_TEMPLATE_CUSTOM && !lstrcmpW(buf, L"IE6-P3PSettings/V1: /DEM=d/ /=a/"),
          "custom get ret %lu tmpl %lu", ret, tmpl);
    CHECK(len == 32, "length %lu", len);
    /* back to a built-in template drops the string */
    set(3, 0, PRIVACY_TEMPLATE_MEDIUM, NULL);
    len = 128;
    ret = get(3, 0, &tmpl, buf, &len);
    CHECK(ret == 0 && tmpl == PRIVACY_TEMPLATE_MEDIUM && !buf[0], "string dropped ret %lu", ret);
}

static void test_dial_and_misc(void)
{
    InitAutoProxy init = PROC("InternetInitializeAutoProxyDll");
    Attempt attempt = PROC("InternetAttemptConnect");
    DialW dial = PROC("InternetDialW");
    GoOnlineW goonline = PROC("InternetGoOnlineW");
    HangUp hangup = PROC("InternetHangUp");
    Resume resume = PROC("ResumeSuspendedDownload");
    DWORD_PTR conn = 55;
    DWORD ret, status;
    BOOL connected = InternetGetConnectedState(&status, 0);
    HINTERNET ses, con, req;

    CHECK(init(1) == FALSE && GetLastError() == ERROR_INVALID_PARAMETER, "autoproxy reserved");
    CHECK(init(0) == TRUE, "autoproxy");
    CHECK(attempt(1) == ERROR_INVALID_PARAMETER, "attempt reserved");
    ret = attempt(0);
    CHECK(connected ? ret == ERROR_SUCCESS : ret == ERROR_NOT_CONNECTED, "attempt %lu connected %d", ret, connected);

    CHECK(dial(NULL, NULL, 0, NULL, 0) == ERROR_INVALID_PARAMETER, "dial no connection pointer");
    CHECK(dial(NULL, NULL, 0, &conn, 1) == ERROR_INVALID_PARAMETER, "dial reserved");
    conn = 55;
    ret = dial(NULL, NULL, 0, &conn, 0);
    CHECK(connected ? ret == ERROR_SUCCESS : ret == 668, "dial autodial %lu", ret);
    CHECK(conn == 0, "connection %Iu", conn);
    conn = 55;
    ret = dial(NULL, (LPWSTR)L"No such connection", 0, &conn, 0);
    CHECK(ret == 668, "dial named %lu", ret);

    CHECK(goonline(NULL, NULL, 2) == FALSE && GetLastError() == ERROR_INVALID_PARAMETER, "goonline flags");
    CHECK(goonline((LPWSTR)L"http://127.0.0.1/", NULL, 0) == connected, "goonline");
    CHECK(hangup(0, 1) == ERROR_INVALID_PARAMETER, "hangup reserved");
    CHECK(hangup(77, 0) == ERROR_INVALID_PARAMETER, "hangup unknown connection");
    CHECK(hangup(0, 0) == ERROR_SUCCESS, "hangup");

    /* ResumeSuspendedDownload */
    SetLastError(0);
    CHECK(resume(NULL, 0) == FALSE && GetLastError() == ERROR_INVALID_HANDLE, "resume null %lu", GetLastError());
    ses = InternetOpenA("t", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
    SetLastError(0);
    CHECK(resume(ses, 0) == FALSE && GetLastError() == ERROR_INTERNET_INCORRECT_HANDLE_TYPE, "resume session %lu", GetLastError());
    con = InternetConnectA(ses, "127.0.0.1", http_port, NULL, NULL, INTERNET_SERVICE_HTTP, 0, 0);
    req = open_request(con, "/ranges", INTERNET_FLAG_RELOAD);
    CHECK(req != NULL, "request %lu", GetLastError());
    CHECK(resume(req, 0) == TRUE, "resume request %lu", GetLastError());
    if (req) InternetCloseHandle(req);
    InternetCloseHandle(con);
    InternetCloseHandle(ses);
}

static void test_options(void)
{
    ULONG v;
    DWORD size;
    BOOL ret;
    HKEY key;
    HINTERNET ses;
    char buf[512];
    INTERNET_PROXY_INFO *pi = (INTERNET_PROXY_INFO *)buf;
    const char *names[3] = { "proxya:1", "proxyb:2", "proxyc:3" };
    int i;

    /* security flags without a handle */
    v = 0xdeadbeef;
    size = sizeof(v);
    ret = InternetQueryOptionA(NULL, INTERNET_OPTION_SECURITY_FLAGS, &v, &size);
    CHECK(ret && v == 0 && size == sizeof(v), "security flags %d %lx", ret, v);
    size = 1;
    SetLastError(0);
    ret = InternetQueryOptionA(NULL, INTERNET_OPTION_SECURITY_FLAGS, &v, &size);
    CHECK(!ret && GetLastError() == ERROR_INSUFFICIENT_BUFFER, "security flags small %lu", GetLastError());

    /* suppress behaviour */
    v = 3;
    ret = InternetSetOptionA(NULL, 81, &v, sizeof(v));
    CHECK(ret, "persist suppress %lu", GetLastError());
    v = 4;
    ret = InternetSetOptionA(NULL, 81, &v, sizeof(v));
    CHECK(ret, "persist reset %lu", GetLastError());
    v = 1;
    CHECK(InternetSetOptionA(NULL, 81, &v, sizeof(v)), "policy");
    v = 2;
    CHECK(InternetSetOptionA(NULL, 81, &v, sizeof(v)), "policy reset");
    v = 0;
    CHECK(InternetSetOptionA(NULL, 81, &v, sizeof(v)), "reset all");
    v = 9;
    SetLastError(0);
    CHECK(!InternetSetOptionA(NULL, 81, &v, sizeof(v)) && GetLastError() == ERROR_INVALID_PARAMETER, "bad behaviour %lu", GetLastError());
    v = 0;
    SetLastError(0);
    CHECK(!InternetSetOptionA(NULL, 81, &v, 2) && GetLastError() == ERROR_INTERNET_BAD_OPTION_LENGTH, "bad length %lu", GetLastError());

    /* proxy option on a non session handle */
    {
        INTERNET_PROXY_INFO info = { INTERNET_OPEN_TYPE_PROXY, "px:1", "" };
        HINTERNET con;
        ses = InternetOpenA("t", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
        con = InternetConnectA(ses, "127.0.0.1", http_port, NULL, NULL, INTERNET_SERVICE_HTTP, 0, 0);
        SetLastError(0);
        CHECK(!InternetSetOptionA(con, INTERNET_OPTION_PROXY, &info, sizeof(info)) &&
              GetLastError() == ERROR_INTERNET_INVALID_OPTION, "proxy on connect handle %lu", GetLastError());
        CHECK(InternetSetOptionA(ses, INTERNET_OPTION_PROXY, &info, sizeof(info)), "proxy on session %lu", GetLastError());
        InternetCloseHandle(con);
        InternetCloseHandle(ses);
    }

    /* settings changed reloads the proxy configuration */
    if (RegCreateKeyExA(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Internet Settings",
                        0, NULL, 0, KEY_ALL_ACCESS, NULL, &key, NULL))
    {
        CHECK(0, "registry key");
        return;
    }
    v = 1;
    RegSetValueExA(key, "ProxyEnable", 0, REG_DWORD, (BYTE *)&v, sizeof(v));
    for (i = 0; i < 3; i++)
    {
        RegSetValueExA(key, "ProxyServer", 0, REG_SZ, (const BYTE *)names[i], strlen(names[i]) + 1);
        if (i == 2) CHECK(((BOOL (WINAPI *)(DWORD))PROC("InternetInitializeAutoProxyDll"))(0), "reload call");
        else InternetSetOptionA(NULL, INTERNET_OPTION_SETTINGS_CHANGED, NULL, 0);
        ses = InternetOpenA("t", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
        size = sizeof(buf);
        ret = InternetQueryOptionA(ses, INTERNET_OPTION_PROXY, buf, &size);
        CHECK(ret && pi->lpszProxy && !strcmp(pi->lpszProxy, names[i]), "proxy %d: %s", i, ret && pi->lpszProxy ? pi->lpszProxy : "(none)");
        InternetCloseHandle(ses);
    }
    RegDeleteValueA(key, "ProxyServer");
    RegDeleteValueA(key, "ProxyEnable");
    RegCloseKey(key);
    InternetSetOptionA(NULL, INTERNET_OPTION_SETTINGS_CHANGED, NULL, 0);
}

static int cookie_cached(const char *host)
{
    char buf[4096];
    DWORD size = sizeof(buf);
    HANDLE h = FindFirstUrlCacheEntryA(NULL, (INTERNET_CACHE_ENTRY_INFOA *)buf, &size);
    int found = 0;

    if (!h) return 0;
    do
    {
        if (!strncmp(((INTERNET_CACHE_ENTRY_INFOA *)buf)->lpszSourceUrlName, "Cookie:", 7) && strstr(((INTERNET_CACHE_ENTRY_INFOA *)buf)->lpszSourceUrlName, host)) found = 1;
        size = sizeof(buf);
    } while (FindNextUrlCacheEntryA(h, (INTERNET_CACHE_ENTRY_INFOA *)buf, &size));
    FindCloseUrlCache(h);
    return found;
}

static void test_cookie_persist(void)
{
    char data[256];
    DWORD size, v;
    BOOL ret;

    ret = InternetSetCookieA("http://persist-a.example/", NULL, "a=1; expires=Mon, 01-Jan-2035 00:00:00 GMT");
    CHECK(ret, "set cookie a %lu", GetLastError());
    CHECK(cookie_cached("persist-a.example"), "cookie a is persistent");

    v = 3;
    InternetSetOptionA(NULL, 81, &v, sizeof(v));
    ret = InternetSetCookieA("http://persist-b.example/", NULL, "b=2; expires=Mon, 01-Jan-2035 00:00:00 GMT");
    CHECK(ret, "set cookie b %lu", GetLastError());
    size = sizeof(data);
    ret = InternetGetCookieA("http://persist-b.example/", NULL, data, &size);
    CHECK(ret && strstr(data, "b=2"), "cookie b is usable");
    CHECK(!cookie_cached("persist-b.example"), "cookie b is not persistent");
    CHECK(cookie_cached("persist-a.example"), "cookie a stays");

    v = 4;
    InternetSetOptionA(NULL, 81, &v, sizeof(v));
    ret = InternetSetCookieA("http://persist-c.example/", NULL, "c=3; expires=Mon, 01-Jan-2035 00:00:00 GMT");
    CHECK(ret, "set cookie c %lu", GetLastError());
    CHECK(cookie_cached("persist-c.example"), "cookie c is persistent again");
    CHECK(!cookie_cached("persist-b.example"), "cookie b still not persistent");
}

static void test_http_file(void)
{
    HINTERNET ses, con, req;
    DWORD pos, high, err;
    INTERNET_DIAGNOSTIC_SOCKET_INFO info;
    DWORD size;
    BOOL ret;

    ses = InternetOpenA("t", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
    con = InternetConnectA(ses, "127.0.0.1", http_port, NULL, NULL, INTERNET_SERVICE_HTTP, 0, 0);
    req = open_request(con, "/ranges", INTERNET_FLAG_RELOAD);
    CHECK(req != NULL, "request %lu", GetLastError());
    if (!req) return;

    /* FILE_END */
    SetLastError(0xdeadbeef);
    pos = InternetSetFilePointer(req, 0, NULL, FILE_END, 0);
    CHECK(pos == 20 && GetLastError() == 0, "end %lu err %lu", pos, GetLastError());
    pos = InternetSetFilePointer(req, 3, NULL, FILE_END, 0);
    CHECK(pos == 23, "end+3 %lu", pos);
    SetLastError(0xdeadbeef);
    pos = InternetSetFilePointer(req, -1, NULL, FILE_END, 0);
    CHECK(pos == INVALID_SET_FILE_POINTER && GetLastError() == ERROR_NEGATIVE_SEEK, "end-1 %lu err %lu", pos, GetLastError());

    /* the 64-bit form */
    high = 0;
    SetLastError(0xdeadbeef);
    pos = InternetSetFilePointer(req, 5, (PVOID)&high, FILE_BEGIN, 0);
    CHECK(pos == 5 && high == 0 && GetLastError() == 0, "wide begin %lu high %lu", pos, high);
    high = 1;
    pos = InternetSetFilePointer(req, 7, (PVOID)&high, FILE_BEGIN, 0);
    CHECK(pos == 7 && high == 1, "wide begin high %lu high %lu", pos, high);
    high = 0;
    pos = InternetSetFilePointer(req, 1, (PVOID)&high, FILE_CURRENT, 0);
    CHECK(pos == 8 && high == 1, "wide current %lu high %lu", pos, high);
    high = 0;
    InternetSetFilePointer(req, 3, (PVOID)&high, FILE_BEGIN, 0);
    high = -1;
    SetLastError(0xdeadbeef);
    pos = InternetSetFilePointer(req, -100, (PVOID)&high, FILE_CURRENT, 0);
    CHECK(pos == INVALID_SET_FILE_POINTER && GetLastError() == ERROR_NEGATIVE_SEEK, "wide negative %lu err %lu", pos, GetLastError());
    high = 0;
    pos = InternetSetFilePointer(req, 3, (PVOID)&high, FILE_BEGIN, 0);
    CHECK(pos == 3, "pointer not moved by the failed seek %lu", pos);

    /* diagnostics */
    memset(&info, 0, sizeof(info));
    size = sizeof(info);
    ret = InternetQueryOptionA(req, INTERNET_OPTION_DIAGNOSTIC_SOCKET_INFO, &info, &size);
    CHECK(ret && size == sizeof(info), "diag %lu", GetLastError());
    CHECK(info.DestPort == http_port, "dest port %lu", info.DestPort);
    CHECK(info.SourcePort != 0 && info.SourcePort != http_port, "source port %lu", info.SourcePort);
    CHECK(info.Socket != 0, "socket %Iu", (size_t)info.Socket);
    CHECK(!(info.Flags & IDSI_FLAG_PROXY) && !(info.Flags & IDSI_FLAG_SECURE), "flags %lx", info.Flags);
    {
        struct sockaddr_in sa;
        int len = sizeof(sa);
        CHECK(getpeername(info.Socket, (struct sockaddr *)&sa, &len) == 0 && ntohs(sa.sin_port) == http_port,
              "the socket is the connection");
    }
    InternetCloseHandle(req);

    /* no ranges: FILE_END is refused */
    req = open_request(con, "/noranges", INTERNET_FLAG_RELOAD);
    CHECK(req != NULL, "request2 %lu", GetLastError());
    if (req)
    {
        SetLastError(0xdeadbeef);
        pos = InternetSetFilePointer(req, 0, NULL, FILE_END, 0);
        err = GetLastError();
        CHECK(pos == INVALID_SET_FILE_POINTER && err == ERROR_INTERNET_INVALID_OPERATION, "end without ranges %lu err %lu", pos, err);
        pos = InternetSetFilePointer(req, 4, NULL, FILE_BEGIN, 0);
        CHECK(pos == 4, "begin without ranges %lu", pos);
        InternetCloseHandle(req);
    }
    InternetCloseHandle(con);
    InternetCloseHandle(ses);
}

static void test_ftp(void)
{
    HINTERNET ses, con, file, find;
    DWORD size, high, err;
    char buf[2000];
    WIN32_FIND_DATAA fd;
    SYSTEMTIME st;
    char proxy[128];
    BOOL ret;

    ses = InternetOpenA("t", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
    con = InternetConnectA(ses, "127.0.0.1", ftp_port, "joe", "pw", INTERNET_SERVICE_FTP, INTERNET_FLAG_PASSIVE, 0);
    CHECK(con != NULL, "ftp connect %lu", GetLastError());
    if (!con) return;
    CHECK(!strcmp(ftp_last_user, "joe"), "user %s", ftp_last_user);

    /* FtpGetFileSize with SIZE */
    ftp_size_supported = 1; ftp_150_hint = 0;
    file = FtpOpenFileA(con, "/sized.bin", GENERIC_READ, FTP_TRANSFER_TYPE_BINARY, 0);
    CHECK(file != NULL, "open %lu", GetLastError());
    if (file)
    {
        high = 77;
        SetLastError(0xdeadbeef);
        size = FtpGetFileSize(file, &high);
        CHECK(size == FTP_BODY_SIZE && high == 0, "size %lu high %lu", size, high);
        size = FtpGetFileSize(file, NULL);
        CHECK(size == FTP_BODY_SIZE, "size without high %lu", size);
        InternetReadFile(file, buf, sizeof(buf), &size);
        InternetCloseHandle(file);
    }
    /* size from the 150 reply when SIZE is refused */
    ftp_size_supported = 0; ftp_150_hint = 1;
    file = FtpOpenFileA(con, "/hint.bin", GENERIC_READ, FTP_TRANSFER_TYPE_BINARY, 0);
    CHECK(file != NULL, "open2 %lu", GetLastError());
    if (file)
    {
        size = FtpGetFileSize(file, NULL);
        CHECK(size == FTP_BODY_SIZE, "size from 150 %lu", size);
        InternetReadFile(file, buf, sizeof(buf), &size);
        InternetCloseHandle(file);
    }
    /* unknown size */
    ftp_size_supported = 0; ftp_150_hint = 0;
    file = FtpOpenFileA(con, "/none.bin", GENERIC_READ, FTP_TRANSFER_TYPE_BINARY, 0);
    CHECK(file != NULL, "open3 %lu", GetLastError());
    if (file)
    {
        SetLastError(0xdeadbeef);
        size = FtpGetFileSize(file, NULL);
        err = GetLastError();
        CHECK(size == INVALID_FILE_SIZE && err == ERROR_INTERNET_INVALID_OPERATION, "unknown size %lu err %lu", size, err);
        InternetReadFile(file, buf, sizeof(buf), &size);
        InternetCloseHandle(file);
    }
    ftp_size_supported = 1;
    SetLastError(0);
    CHECK(FtpGetFileSize(NULL, NULL) == INVALID_FILE_SIZE && GetLastError() == ERROR_INVALID_HANDLE, "size null handle");
    SetLastError(0);
    CHECK(FtpGetFileSize(con, NULL) == INVALID_FILE_SIZE && GetLastError() == ERROR_INTERNET_INCORRECT_HANDLE_TYPE, "size session handle");

    /* EPLF directory listing */
    find = FtpFindFirstFileA(con, "*", &fd, 0, 0);
    CHECK(find != NULL, "find %lu", GetLastError());
    if (find)
    {
        CHECK(!strcmp(fd.cFileName, "subdir") && (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && fd.nFileSizeLow == 0,
              "first '%s' attr %lx size %lu", fd.cFileName, fd.dwFileAttributes, fd.nFileSizeLow);
        FileTimeToSystemTime(&fd.ftLastWriteTime, &st);
        CHECK(st.wYear == 2001 && st.wMonth == 9 && st.wDay == 9 && st.wHour == 1 && st.wMinute == 46 && st.wSecond == 40,
              "time %04d-%02d-%02d %02d:%02d:%02d", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
        ret = InternetFindNextFileA(find, &fd);
        CHECK(ret && !strcmp(fd.cFileName, "file with space.txt") && !(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) &&
              fd.nFileSizeLow == 321, "second ret %d '%s' size %lu", ret, ret ? fd.cFileName : "", fd.nFileSizeLow);
        ret = InternetFindNextFileA(find, &fd);
        CHECK(!ret && GetLastError() == ERROR_NO_MORE_FILES, "end of list %d %lu", ret, GetLastError());
        InternetCloseHandle(find);
    }
    InternetCloseHandle(con);
    InternetCloseHandle(ses);

    /* a proxy named for FTP: connect to it and log in as user@server */
    sprintf(proxy, "http=http://127.0.0.1:8 ftp=ftp://127.0.0.1:%d", ftp_port);
    ses = InternetOpenA("t", INTERNET_OPEN_TYPE_PROXY, proxy, NULL, 0);
    ftp_last_user[0] = 0;
    con = InternetConnectA(ses, "ftp.example.invalid", INTERNET_INVALID_PORT_NUMBER, "joe", "pw", INTERNET_SERVICE_FTP,
                           INTERNET_FLAG_PASSIVE, 0);
    CHECK(con != NULL, "proxy connect %lu", GetLastError());
    CHECK(!strcmp(ftp_last_user, "joe@ftp.example.invalid"), "proxy user '%s'", ftp_last_user);
    if (con) InternetCloseHandle(con);
    ftp_last_user[0] = 0;
    con = InternetConnectA(ses, "ftp.example.invalid", 2121, "joe", "pw", INTERNET_SERVICE_FTP, INTERNET_FLAG_PASSIVE, 0);
    CHECK(con != NULL && !strcmp(ftp_last_user, "joe@ftp.example.invalid:2121"), "proxy user with port '%s'", ftp_last_user);
    if (con) InternetCloseHandle(con);
    InternetCloseHandle(ses);

    /* the host is on the bypass list: no proxy, the (unresolvable) host is used directly */
    ses = InternetOpenA("t", INTERNET_OPEN_TYPE_PROXY, proxy, "ftp.example.invalid", 0);
    ftp_last_user[0] = 0;
    con = InternetConnectA(ses, "ftp.example.invalid", INTERNET_INVALID_PORT_NUMBER, "joe", "pw", INTERNET_SERVICE_FTP,
                           INTERNET_FLAG_PASSIVE, 0);
    CHECK(con == NULL && !ftp_last_user[0], "bypassed host does not use the proxy (con %p)", con);
    if (con) InternetCloseHandle(con);
    InternetCloseHandle(ses);

    /* localhost bypasses the proxy by default; "<-loopback>" removes that */
    sprintf(proxy, "ftp=ftp://127.0.0.1:1");
    ses = InternetOpenA("t", INTERNET_OPEN_TYPE_PROXY, proxy, NULL, 0);
    ftp_last_user[0] = 0;
    con = InternetConnectA(ses, "localhost", ftp_port, "joe", "pw", INTERNET_SERVICE_FTP, INTERNET_FLAG_PASSIVE, 0);
    CHECK(con != NULL && !strcmp(ftp_last_user, "joe"), "loopback bypasses the proxy '%s'", ftp_last_user);
    if (con) InternetCloseHandle(con);
    InternetCloseHandle(ses);
    ses = InternetOpenA("t", INTERNET_OPEN_TYPE_PROXY, proxy, "<-loopback>", 0);
    con = InternetConnectA(ses, "localhost", ftp_port, "joe", "pw", INTERNET_SERVICE_FTP, INTERNET_FLAG_PASSIVE, 0);
    CHECK(con == NULL, "<-loopback> uses the proxy");
    if (con) InternetCloseHandle(con);
    InternetCloseHandle(ses);

    /* a proxy that is not named for FTP (CERN) is not used for FTP */
    sprintf(proxy, "127.0.0.1:%d", ftp_port);
    ses = InternetOpenA("t", INTERNET_OPEN_TYPE_PROXY, proxy, NULL, 0);
    ftp_last_user[0] = 0;
    con = InternetConnectA(ses, "127.0.0.1", ftp_port, "joe", "pw", INTERNET_SERVICE_FTP, INTERNET_FLAG_PASSIVE, 0);
    CHECK(con != NULL && !strcmp(ftp_last_user, "joe"), "cern proxy ignored for ftp '%s'", ftp_last_user);
    if (con) InternetCloseHandle(con);
    InternetCloseHandle(ses);
}

static void test_gopher(void)
{
    char loc[512];
    WCHAR locw[512];
    DWORD len, type, i;
    HINTERNET ses;
    struct { DWORD type; DWORD want; } cases[] = {
        { 0, GOPHER_TYPE_DIRECTORY }, { GOPHER_TYPE_DIRECTORY, GOPHER_TYPE_DIRECTORY },
        { GOPHER_TYPE_TEXT_FILE, GOPHER_TYPE_TEXT_FILE }, { GOPHER_TYPE_BINARY, GOPHER_TYPE_BINARY },
        { GOPHER_TYPE_INDEX_SERVER, GOPHER_TYPE_INDEX_SERVER }, { GOPHER_TYPE_GIF, GOPHER_TYPE_GIF },
        { GOPHER_TYPE_HTML, GOPHER_TYPE_HTML }, { GOPHER_TYPE_TEXT_FILE | GOPHER_TYPE_GOPHER_PLUS,
          GOPHER_TYPE_TEXT_FILE | GOPHER_TYPE_GOPHER_PLUS },
    };
    GopherGetType gettype = PROC("GopherGetLocatorTypeA");

    for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
    {
        BOOL ret;
        len = sizeof(loc);
        ret = GopherCreateLocatorA("gopher.example", INTERNET_INVALID_PORT_NUMBER, "Display", "/sel", cases[i].type, loc, &len);
        CHECK(ret, "create %lu: %lu", i, GetLastError());
        if (!ret) continue;
        CHECK(len == strlen(loc), "length %lu vs %u", len, (unsigned)strlen(loc));
        type = 0;
        ret = gettype(loc, &type);
        CHECK(ret && type == cases[i].want, "type %lu: ret %d got %lx want %lx", i, ret, type, cases[i].want);
        if (i == 0) CHECK(strstr(loc, "\t/sel\tgopher.example\t70"), "locator content '%s'", loc);
    }
    /* explicit port and unicode */
    len = 512;
    CHECK(GopherCreateLocatorW(L"h.example", 7070, NULL, NULL, GOPHER_TYPE_TEXT_FILE, locw, &len), "create W");
    CHECK(wcsstr(locw, L"\t7070") != NULL && locw[0] == L'0', "W locator");
    /* sizing */
    len = 0;
    SetLastError(0);
    CHECK(!GopherCreateLocatorA("h.example", 70, "d", "s", 0, NULL, &len) && GetLastError() == ERROR_INSUFFICIENT_BUFFER && len > 0, "sizing %lu", len);
    {
        DWORD need = len;
        len = need - 1;
        SetLastError(0);
        CHECK(!GopherCreateLocatorA("h.example", 70, "d", "s", 0, loc, &len) && GetLastError() == ERROR_INSUFFICIENT_BUFFER && len == need, "small buffer");
        len = need;
        CHECK(GopherCreateLocatorA("h.example", 70, "d", "s", 0, loc, &len) && len == need - 1, "exact buffer");
    }
    SetLastError(0);
    len = 512;
    CHECK(!GopherCreateLocatorA(NULL, 70, "d", "s", 0, loc, &len) && GetLastError() == ERROR_INVALID_PARAMETER, "no host");
    /* parsing errors */
    SetLastError(0);
    CHECK(!gettype("garbage", &type) && GetLastError() == ERROR_GOPHER_INVALID_LOCATOR, "invalid locator %lu", GetLastError());
    SetLastError(0);
    CHECK(!gettype(NULL, &type) && GetLastError() == ERROR_INVALID_PARAMETER, "null locator");
    CHECK(gettype("zname\tsel\thost\t70", &type) && type == GOPHER_TYPE_UNKNOWN, "unknown type char %lx", type);

    ses = InternetOpenA("t", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
    SetLastError(0);
    CHECK(InternetConnectA(ses, "127.0.0.1", 70, NULL, NULL, INTERNET_SERVICE_GOPHER, 0, 0) == NULL &&
          GetLastError() == ERROR_INTERNET_INVALID_OPERATION, "gopher connect %lu", GetLastError());
    InternetCloseHandle(ses);

    /* the network functions need a gopher session, which cannot exist */
    SetLastError(0);
    CHECK(GopherFindFirstFileA(NULL, NULL, NULL, NULL, 0, 0) == NULL && GetLastError() == ERROR_INVALID_HANDLE, "find null");
    SetLastError(0);
    CHECK(GopherOpenFileA(NULL, "x", NULL, 0, 0) == NULL && GetLastError() == ERROR_INVALID_HANDLE, "open null");
    SetLastError(0);
    CHECK(!GopherGetAttributeA(NULL, "x", NULL, NULL, 0, NULL, NULL, 0) && GetLastError() == ERROR_INVALID_HANDLE, "attr null");
    ses = InternetOpenA("t", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
    SetLastError(0);
    CHECK(GopherFindFirstFileA(ses, NULL, NULL, NULL, 0, 0) == NULL && GetLastError() == ERROR_INTERNET_INCORRECT_HANDLE_TYPE, "find session");
    SetLastError(0);
    CHECK(GopherOpenFileW(ses, L"x", NULL, 0, 0) == NULL && GetLastError() == ERROR_INTERNET_INCORRECT_HANDLE_TYPE, "open session");
    InternetCloseHandle(ses);
}

static void test_md5_and_security(void)
{
    MD5Hash md5 = PROC("CreateMD5SSOHash");
    ShowSecW showw = PROC("InternetShowSecurityInfoByURLW");
    ShowSecA showa = PROC("InternetShowSecurityInfoByURLA");
    CREDENTIALW cred;
    WCHAR pass[] = L"pass";
    BYTE hash[40];
    BOOL ret;

    memset(hash, 0x55, sizeof(hash));
    SetLastError(0);
    ret = md5((PWSTR)L"abc", NULL, NULL, hash);
    CHECK(!ret, "no stored password");
    SetLastError(0);
    CHECK(!md5(NULL, NULL, NULL, hash) && GetLastError() == ERROR_INVALID_PARAMETER, "null challenge");
    SetLastError(0);
    CHECK(!md5((PWSTR)L"abc", NULL, NULL, NULL) && GetLastError() == ERROR_INVALID_PARAMETER, "null output");

    memset(&cred, 0, sizeof(cred));
    cred.Type = CRED_TYPE_GENERIC;
    cred.TargetName = (WCHAR *)L"Passport.Net\\*";
    cred.CredentialBlob = (BYTE *)pass;
    cred.CredentialBlobSize = 4 * sizeof(WCHAR);
    cred.Persist = CRED_PERSIST_SESSION;
    if (CredWriteW(&cred, 0))
    {
        memset(hash, 0x55, sizeof(hash));
        ret = md5((PWSTR)L"abc", NULL, NULL, hash);
        CHECK(ret && !strcmp((char *)hash, "67834713e21404b16d9abf3229660972"), "hash %s", ret ? (char *)hash : "(failed)");
        CredDeleteW(L"Passport.Net\\*", CRED_TYPE_GENERIC, 0);
    }
    else printf("note CredWriteW unavailable (%lu); hash value not checked\n", GetLastError());

    SetLastError(0);
    CHECK(!showw(NULL, NULL) && GetLastError() == ERROR_INVALID_PARAMETER, "show null");
    SetLastError(0);
    CHECK(!showa(NULL, NULL) && GetLastError() == ERROR_INVALID_PARAMETER, "show null A");
    CHECK(!showw(L"https://never-connected.example/", NULL), "show unknown server");
}

static void test_urlcache_synctime(void)
{
    char path[MAX_PATH];
    char buf[2048];
    DWORD size = sizeof(buf);
    INTERNET_CACHE_ENTRY_INFOA info;
    FILETIME ft;
    SYSTEMTIME st = { 2005, 3, 0, 4, 12, 30, 0, 0 };
    BOOL ret;
    const char *url = "http://synctime.example/file.txt";
    HANDLE h;

    ret = CreateUrlCacheEntryA(url, 0, "txt", path, 0);
    CHECK(ret, "create entry %lu", GetLastError());
    h = CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    CloseHandle(h);
    memset(&ft, 0, sizeof(ft));
    ret = CommitUrlCacheEntryA(url, path, ft, ft, NORMAL_CACHE_ENTRY, NULL, 0, NULL, NULL);
    CHECK(ret, "commit %lu", GetLastError());

    memset(&info, 0, sizeof(info));
    info.dwStructSize = sizeof(info);
    SystemTimeToFileTime(&st, &info.LastSyncTime);
    /* the access time is different, so the sync time cannot come from it */
    st.wYear = 2010;
    SystemTimeToFileTime(&st, &info.LastAccessTime);
    ret = SetUrlCacheEntryInfoA(url, &info, CACHE_ENTRY_SYNCTIME_FC);
    CHECK(ret, "set info %lu", GetLastError());
    ret = GetUrlCacheEntryInfoA(url, (INTERNET_CACHE_ENTRY_INFOA *)buf, &size);
    CHECK(ret, "get info %lu", GetLastError());
    if (ret)
    {
        FileTimeToSystemTime(&((INTERNET_CACHE_ENTRY_INFOA *)buf)->LastSyncTime, &st);
        CHECK(st.wYear == 2005 && st.wMonth == 3 && st.wDay == 4, "sync time %04d-%02d-%02d", st.wYear, st.wMonth, st.wDay);
    }
    DeleteUrlCacheEntryA(url);
}

int main(void)
{
    WSADATA wsa;
    HANDLE th[2];

    WSAStartup(MAKEWORD(2, 2), &wsa);
    wi = LoadLibraryA("wininet.dll");
    http_ls = listen_on(&http_port);
    ftp_ls = listen_on(&ftp_port);
    th[0] = CreateThread(NULL, 0, http_thread, NULL, 0, NULL);
    th[1] = CreateThread(NULL, 0, ftp_thread, NULL, 0, NULL);

    test_privacy();
    test_dial_and_misc();
    test_options();
    test_cookie_persist();
    test_http_file();
    test_ftp();
    test_gopher();
    test_md5_and_security();
    test_urlcache_synctime();

    stop_servers = 1;
    WaitForMultipleObjects(2, th, TRUE, 3000);
    printf("%d checks, %d failures\n", checks, failures);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    fflush(stdout);
    ExitProcess(failures ? 1 : 0);
}
