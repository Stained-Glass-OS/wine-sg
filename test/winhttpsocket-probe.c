/* Probe for patches/sg/2444: the web socket buffer size options belong to the request, not to the upgraded socket handle.
 * A local server answers the upgrade request with 101. */
#include <winsock2.h>
#include <windows.h>
#include <winhttp.h>
#include <bcrypt.h>
#include <wincrypt.h>
#include <stdio.h>
#include <string.h>

static int fails;
static void check(const char *name, int ok) { printf("      %s  %s\n", ok ? "PASS" : "FAIL", name); if (!ok) fails++; }

static SOCKET listener;
static unsigned short port;

static DWORD WINAPI server(void *arg)
{
    SOCKET c = accept(listener, NULL, NULL);
    char req[4096], reply[512], key[64] = "", accept_b64[64];
    int n, total = 0;
    char *p;
    BCRYPT_ALG_HANDLE alg = NULL;
    BCRYPT_HASH_HANDLE h = NULL;
    UCHAR digest[20];
    char in[128];
    DWORD len = sizeof(accept_b64);

    if (c == INVALID_SOCKET) return 1;
    while (total < (int)sizeof(req) - 1 && (n = recv(c, req + total, sizeof(req) - 1 - total, 0)) > 0)
    {
        total += n;
        req[total] = 0;
        if (strstr(req, "\r\n\r\n")) break;
    }
    p = strstr(req, "Sec-WebSocket-Key: ");
    if (p) sscanf(p + 19, "%63[^\r]", key);
    snprintf(in, sizeof(in), "%s258EAFA5-E914-47DA-95CA-C5AB0DC85B11", key);
    BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA1_ALGORITHM, NULL, 0);
    BCryptCreateHash(alg, &h, NULL, 0, NULL, 0, 0);
    BCryptHashData(h, (UCHAR *)in, strlen(in), 0);
    BCryptFinishHash(h, digest, sizeof(digest), 0);
    BCryptDestroyHash(h);
    BCryptCloseAlgorithmProvider(alg, 0);
    CryptBinaryToStringA(digest, sizeof(digest), CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, accept_b64, &len);
    snprintf(reply, sizeof(reply), "HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
             "Sec-WebSocket-Accept: %s\r\n\r\n", accept_b64);
    send(c, reply, strlen(reply), 0);
    Sleep(1500);
    closesocket(c);
    return 0;
}

int main(void)
{
    WSADATA wsa;
    struct sockaddr_in addr;
    int alen = sizeof(addr);
    HANDLE thread;
    HINTERNET ses, con, req, sock;
    DWORD value, size;
    BOOL ret;

    WSAStartup(MAKEWORD(2, 2), &wsa);
    listener = socket(AF_INET, SOCK_STREAM, 0);
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    bind(listener, (struct sockaddr *)&addr, sizeof(addr));
    listen(listener, 1);
    getsockname(listener, (struct sockaddr *)&addr, &alen);
    port = ntohs(addr.sin_port);
    thread = CreateThread(NULL, 0, server, NULL, 0, NULL);

    ses = WinHttpOpen(L"sgprobe", WINHTTP_ACCESS_TYPE_NO_PROXY, NULL, NULL, 0);
    con = WinHttpConnect(ses, L"127.0.0.1", port, 0);
    req = WinHttpOpenRequest(con, L"GET", L"/", NULL, NULL, NULL, 0);
    ret = WinHttpSetOption(req, WINHTTP_OPTION_UPGRADE_TO_WEB_SOCKET, NULL, 0);
    check("upgrade option", ret);
    ret = WinHttpSendRequest(req, NULL, 0, NULL, 0, 0, 0);
    check("send request", ret);
    ret = ret && WinHttpReceiveResponse(req, NULL);
    check("receive the 101", ret);
    if (!ret) { puts("RESULT: FAIL"); return 1; }
    sock = WinHttpWebSocketCompleteUpgrade(req, 0);
    check("complete upgrade", sock != NULL);
    if (sock)
    {
        size = sizeof(value);
        SetLastError(0xdeadbeef);
        ret = WinHttpQueryOption(sock, WINHTTP_OPTION_WEB_SOCKET_RECEIVE_BUFFER_SIZE, &value, &size);
        check("query receive buffer size on the socket: incorrect handle type", !ret && GetLastError() == ERROR_WINHTTP_INCORRECT_HANDLE_TYPE);
        size = sizeof(value);
        SetLastError(0xdeadbeef);
        ret = WinHttpQueryOption(sock, WINHTTP_OPTION_WEB_SOCKET_SEND_BUFFER_SIZE, &value, &size);
        check("query send buffer size on the socket: incorrect handle type", !ret && GetLastError() == ERROR_WINHTTP_INCORRECT_HANDLE_TYPE);
        value = 65535;
        SetLastError(0xdeadbeef);
        ret = WinHttpSetOption(sock, WINHTTP_OPTION_WEB_SOCKET_RECEIVE_BUFFER_SIZE, &value, sizeof(DWORD));
        check("set receive buffer size on the socket: incorrect handle type", !ret && GetLastError() == ERROR_WINHTTP_INCORRECT_HANDLE_TYPE);
        SetLastError(0xdeadbeef);
        ret = WinHttpSetOption(sock, WINHTTP_OPTION_WEB_SOCKET_SEND_BUFFER_SIZE, &value, sizeof(DWORD));
        check("set send buffer size on the socket: incorrect handle type", !ret && GetLastError() == ERROR_WINHTTP_INCORRECT_HANDLE_TYPE);
        value = 20000;
        SetLastError(0xdeadbeef);
        ret = WinHttpSetOption(sock, WINHTTP_OPTION_WEB_SOCKET_KEEPALIVE_INTERVAL, &value, 2);
        check("keepalive with a bad size: invalid parameter", !ret && GetLastError() == ERROR_INVALID_PARAMETER);
        ret = WinHttpSetOption(sock, WINHTTP_OPTION_WEB_SOCKET_KEEPALIVE_INTERVAL, &value, sizeof(DWORD));
        check("keepalive still settable", ret);
        WinHttpCloseHandle(sock);
    }
    WinHttpCloseHandle(req);
    WinHttpCloseHandle(con);
    WinHttpCloseHandle(ses);
    WaitForSingleObject(thread, 5000);
    closesocket(listener);
    puts(fails ? "RESULT: FAIL" : "RESULT: PASS");
    return fails != 0;
}
