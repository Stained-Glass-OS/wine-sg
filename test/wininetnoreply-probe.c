/* Probe for patches/sg/2458: a server that closes the connection without answering makes HttpSendRequest fail. */
#include <winsock2.h>
#include <windows.h>
#include <wininet.h>
#include <stdio.h>
#include <string.h>

static int fails;
static void check(const char *name, int ok) { printf("      %s  %s\n", ok ? "PASS" : "FAIL", name); if (!ok) fails++; }

static SOCKET listener;

static DWORD WINAPI server(void *arg)
{
    char req[2048];
    SOCKET c = accept(listener, NULL, NULL);
    int n;
    if (c == INVALID_SOCKET) return 1;
    n = recv(c, req, sizeof(req), 0);   /* the request... */
    (void)n;
    closesocket(c);                      /* ...and no answer */
    return 0;
}

int main(void)
{
    WSADATA wsa;
    struct sockaddr_in addr;
    int alen = sizeof(addr);
    HANDLE thread;
    HINTERNET ses, con, req;
    BOOL ret;
    DWORD err;

    WSAStartup(MAKEWORD(2, 2), &wsa);
    listener = socket(AF_INET, SOCK_STREAM, 0);
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    bind(listener, (struct sockaddr *)&addr, sizeof(addr));
    listen(listener, 1);
    getsockname(listener, (struct sockaddr *)&addr, &alen);
    thread = CreateThread(NULL, 0, server, NULL, 0, NULL);

    ses = InternetOpenA("sgprobe", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
    con = InternetConnectA(ses, "127.0.0.1", ntohs(addr.sin_port), NULL, NULL, INTERNET_SERVICE_HTTP, 0, 0);
    req = HttpOpenRequestA(con, "GET", "/premature", NULL, NULL, NULL, INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_RELOAD, 0);
    SetLastError(0xdeadbeef);
    ret = HttpSendRequestA(req, NULL, 0, NULL, 0);
    err = GetLastError();
    check("HttpSendRequest fails when the server hangs up", !ret);
    check("with ERROR_HTTP_INVALID_SERVER_RESPONSE (12152)", err == ERROR_HTTP_INVALID_SERVER_RESPONSE);
    InternetCloseHandle(req);
    InternetCloseHandle(con);
    InternetCloseHandle(ses);
    WaitForSingleObject(thread, 5000);
    closesocket(listener);
    puts(fails ? "RESULT: FAIL" : "RESULT: PASS");
    return fails != 0;
}
