/* test/nocode7-gate.sh: httpapi HttpWaitForDisconnect(Ex) and HttpCancelHttpRequest (patches/sg/8021),
 * which .NET's HttpListener calls (connection-based authentication; aborting a request). Spec stubs before. */
#include <winsock2.h>
#include <windows.h>
#include <http.h>
#include <stdio.h>

static int failures;
#define CHECK(c, ...) do { if (c) { printf("ok    "); printf(__VA_ARGS__); printf("\n"); } \
                           else { failures++; printf("FAIL  "); printf(__VA_ARGS__); printf("\n"); } } while (0)

static const char get[] = "GET /x HTTP/1.1\r\nHost: localhost:50277\r\n\r\n";

static SOCKET client(void)
{
    struct sockaddr_in sa = {.sin_family = AF_INET, .sin_port = htons(50277)};
    SOCKET s = socket(AF_INET, SOCK_STREAM, 0);
    sa.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (connect(s, (struct sockaddr *)&sa, sizeof(sa))) return INVALID_SOCKET;
    send(s, get, sizeof(get) - 1, 0);
    return s;
}

static ULONG receive(HANDLE queue, HTTP_REQUEST *req, ULONG size)
{
    OVERLAPPED ovl = {0};
    ULONG ret, got = 0;
    ovl.hEvent = CreateEventA(NULL, TRUE, FALSE, NULL);
    ret = HttpReceiveHttpRequest(queue, HTTP_NULL_ID, 0, req, size, &got, &ovl);
    if (ret == ERROR_IO_PENDING) ret = WaitForSingleObject(ovl.hEvent, 10000) ? WAIT_TIMEOUT : 0;
    CloseHandle(ovl.hEvent);
    return ret;
}

static ULONG (WINAPI *pHttpWaitForDisconnectEx)(HANDLE, HTTP_CONNECTION_ID, ULONG, OVERLAPPED *);

int main(void)
{
    HTTPAPI_VERSION ver = {1, 0};
    HANDLE queue;
    WSADATA wsa;
    SOCKET s;
    char reqbuf[4096], resp[256];
    HTTP_REQUEST *req = (HTTP_REQUEST *)reqbuf;
    HTTP_RESPONSE response = {0};
    OVERLAPPED ovl = {0};
    ULONG ret, written;
    DWORD bytes;
    int n;

    pHttpWaitForDisconnectEx = (void *)GetProcAddress(LoadLibraryA("httpapi.dll"), "HttpWaitForDisconnectEx");
    CHECK(pHttpWaitForDisconnectEx != NULL, "HttpWaitForDisconnectEx is exported");
    if (!pHttpWaitForDisconnectEx) { printf("RESULT: FAIL\n"); return 1; }
    WSAStartup(MAKEWORD(2, 2), &wsa);
    ret = HttpInitialize(ver, HTTP_INITIALIZE_SERVER, NULL);
    CHECK(ret == 0, "HttpInitialize %lu", ret);
    HttpCreateHttpHandle(&queue, 0);
    ret = HttpAddUrl(queue, L"http://localhost:50277/", NULL);
    CHECK(ret == 0, "HttpAddUrl %lu", ret);
    ovl.hEvent = CreateEventA(NULL, TRUE, FALSE, NULL);

    /* the client goes away: the wait for its disconnection completes */
    s = client();
    CHECK(s != INVALID_SOCKET, "a client");
    ret = receive(queue, req, sizeof(reqbuf));
    CHECK(ret == 0 && req->ConnectionId && req->RequestId, "a request on a connection (%lu)", ret);
    ret = HttpWaitForDisconnect(queue, req->ConnectionId, &ovl);
    CHECK(ret == ERROR_IO_PENDING, "HttpWaitForDisconnect: pending (%lu)", ret);
    CHECK(WaitForSingleObject(ovl.hEvent, 300) == WAIT_TIMEOUT, "not complete while the client is there");
    closesocket(s);
    CHECK(WaitForSingleObject(ovl.hEvent, 10000) == 0 && GetOverlappedResult(queue, &ovl, &bytes, FALSE),
          "the client closed: complete (%lu)", GetLastError());
    ret = HttpWaitForDisconnect(queue, req->ConnectionId, NULL);
    CHECK(ret == ERROR_CONNECTION_INVALID, "that connection again: %lu", ret);
    ret = pHttpWaitForDisconnectEx(queue, 0x1234, 0, NULL);
    CHECK(ret == ERROR_CONNECTION_INVALID, "a connection that never was: %lu", ret);
    CHECK(pHttpWaitForDisconnectEx(queue, 0x1234, 1, NULL) == ERROR_INVALID_PARAMETER, "reserved not 0");

    /* the server cancels a request: the connection closes, a wait on it completes, a response cannot go */
    s = client();
    ret = receive(queue, req, sizeof(reqbuf));
    CHECK(ret == 0, "a second request (%lu)", ret);
    ResetEvent(ovl.hEvent);
    ret = pHttpWaitForDisconnectEx(queue, req->ConnectionId, 0, &ovl);
    CHECK(ret == ERROR_IO_PENDING, "waiting for its disconnection (%lu)", ret);
    ret = HttpCancelHttpRequest(queue, req->RequestId, NULL);
    CHECK(ret == 0, "HttpCancelHttpRequest %lu", ret);
    CHECK(WaitForSingleObject(ovl.hEvent, 10000) == 0, "the wait completed");
    n = recv(s, resp, sizeof(resp), 0);
    CHECK(n <= 0, "the client's connection is closed (%d)", n);
    closesocket(s);
    response.StatusCode = 200;
    ret = HttpSendHttpResponse(queue, req->RequestId, 0, &response, NULL, &written, NULL, 0, NULL, NULL);
    CHECK(ret == ERROR_CONNECTION_INVALID, "no response to a cancelled request: %lu", ret);
    ret = HttpCancelHttpRequest(queue, req->RequestId, NULL);
    CHECK(ret == ERROR_CONNECTION_INVALID, "cancelled twice: %lu", ret);
    CHECK(HttpCancelHttpRequest(queue, HTTP_NULL_ID, NULL) == ERROR_INVALID_PARAMETER, "the null request id");

    HttpRemoveUrl(queue, L"http://localhost:50277/");
    CloseHandle(queue);
    HttpTerminate(HTTP_INITIALIZE_SERVER, NULL);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
