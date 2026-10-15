#include <winsock2.h>
#include <windows.h>
#include <http.h>
#include <stdio.h>

static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

int main(void)
{
    HTTPAPI_VERSION ver = {1, 0};
    HANDLE queue, file;
    WSADATA wsa;
    SOCKET s;
    struct sockaddr_in sa = {.sin_family = AF_INET, .sin_port = htons(50233)};
    char reqbuf[4096] = {0}, resp[16384], text[5000], path[MAX_PATH], tmp[MAX_PATH];
    HTTP_REQUEST *req = (HTTP_REQUEST *)reqbuf;
    HTTP_RESPONSE response = {0};
    HTTP_DATA_CHUNK chunk = {0};
    OVERLAPPED ovl = {0};
    ULONG ret, written, got = 0;
    int n, total = 0, i;
    static const char get[] = "GET /f HTTP/1.1\r\nHost: localhost:50233\r\n\r\n";
    char *body;

    WSAStartup(MAKEWORD(2, 2), &wsa);
    ret = HttpInitialize(ver, HTTP_INITIALIZE_SERVER, NULL); CHECK(ret == 0, "init %lu", ret);
    ret = HttpCreateHttpHandle(&queue, 0); CHECK(ret == 0, "handle %lu", ret);
    ret = HttpAddUrl(queue, L"http://localhost:50233/", NULL); CHECK(ret == 0, "addurl %lu", ret);

    for (i = 0; i < 5000; i++) text[i] = 'a' + i % 26;
    GetTempPathA(sizeof(tmp), tmp);
    GetTempFileNameA(tmp, "fh", 0, path);
    file = CreateFileA(path, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE, NULL);
    WriteFile(file, text, sizeof(text), &written, NULL);

    s = socket(AF_INET, SOCK_STREAM, 0);
    sa.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    CHECK(connect(s, (struct sockaddr *)&sa, sizeof(sa)) == 0, "connect");
    send(s, get, sizeof(get) - 1, 0);

    ovl.hEvent = CreateEventA(NULL, TRUE, FALSE, NULL);
    ret = HttpReceiveHttpRequest(queue, HTTP_NULL_ID, 0, req, sizeof(reqbuf), &got, &ovl);
    if (ret == ERROR_IO_PENDING) { WaitForSingleObject(ovl.hEvent, 10000); ret = 0; }
    CHECK(ret == 0 && req->Verb == HttpVerbGET, "receive %lu", ret);

    response.StatusCode = 200;
    response.pReason = "OK";
    response.ReasonLength = 2;
    chunk.DataChunkType = HttpDataChunkFromFileHandle;
    chunk.FromFileHandle.FileHandle = file;
    chunk.FromFileHandle.ByteRange.StartingOffset.QuadPart = 10;
    chunk.FromFileHandle.ByteRange.Length.QuadPart = HTTP_BYTE_RANGE_TO_EOF;
    response.EntityChunkCount = 1;
    response.pEntityChunks = &chunk;
    ResetEvent(ovl.hEvent);
    ret = HttpSendHttpResponse(queue, req->RequestId, HTTP_SEND_RESPONSE_FLAG_DISCONNECT, &response, NULL, &written, NULL, 0, &ovl, NULL);
    if (ret == ERROR_IO_PENDING) { WaitForSingleObject(ovl.hEvent, 10000); ret = 0; }
    CHECK(ret == 0, "send response %lu", ret);

    /* the server closes the connection after the response */
    while ((n = recv(s, resp + total, sizeof(resp) - total, 0)) > 0) total += n;
    CHECK(n == 0, "connection not closed (%d)", n);
    body = strstr(resp, "\r\n\r\n");
    CHECK(body && total - (body + 4 - resp) == 4990 && !memcmp(body + 4, text + 10, 4990), "file body, total %d", total);

    /* binding follows the URL host: localhost is loopback only, "+" and "*" are all interfaces,
     * an IP address is that address. A bind to 127.0.0.2 tells the three apart. */
    {
        static const struct { const WCHAR *url; int port; int other_free; } cases[] =
        {
            { L"http://localhost:50240/", 50240, 1 },
            { L"http://+:50241/", 50241, 0 },
            { L"http://*:50242/", 50242, 0 },
            { L"http://127.0.0.1:50243/", 50243, 1 },
            { L"http://127.0.0.3:50244/", 50244, 1 },
        };
        int k;

        for (k = 0; k < 5; k++)
        {
            struct sockaddr_in other = {.sin_family = AF_INET, .sin_port = htons(cases[k].port)};
            SOCKET t = socket(AF_INET, SOCK_STREAM, 0);
            int bound;

            ret = HttpAddUrl(queue, cases[k].url, NULL);
            CHECK(ret == 0, "addurl %ls: %lu", cases[k].url, ret);
            other.sin_addr.s_addr = inet_addr("127.0.0.2");
            bound = bind(t, (struct sockaddr *)&other, sizeof(other)) == 0;
            CHECK(bound == cases[k].other_free, "%ls: bind to 127.0.0.2 gave %d", cases[k].url, bound);
            closesocket(t);
            HttpRemoveUrl(queue, cases[k].url);
        }
    }

    /* extra URLs in the service configuration store */
    {
        struct { WCHAR *prefix; WCHAR *sddl; } set = {L"http://+:50234/probe/", L"D:(A;;GX;;;WD)"};
        ret = HttpSetServiceConfiguration(NULL, HttpServiceConfigUrlAclInfo, &set, sizeof(set), NULL);
        CHECK(ret == 0 || ret == ERROR_ALREADY_EXISTS, "set urlacl %lu", ret);
        ret = HttpDeleteServiceConfiguration(NULL, HttpServiceConfigUrlAclInfo, &set, sizeof(set), NULL);
        CHECK(ret == 0, "delete urlacl %lu", ret);
    }

    closesocket(s);
    CloseHandle(file);
    HttpCloseRequestQueue(queue);
    HttpTerminate(HTTP_INITIALIZE_SERVER, NULL);
    printf(fails ? "RESULT: FAIL (%d)\n" : "RESULT: PASS\n", fails);
    return fails != 0;
}
