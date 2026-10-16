#include <winsock2.h>
#include <windows.h>
#include <webservices.h>
#include <stdio.h>
#include <string.h>
static int bad, total;
#define EXPECT(c) do { total++; if (!(c)) { bad++; printf("MISMATCH line %d: %s\n", __LINE__, #c); } } while (0)

#define PORT 47231
static WS_XML_STRING s_req = {3, (BYTE *)"req"}, s_resp = {4, (BYTE *)"resp"}, s_val = {3, (BYTE *)"val"},
                     s_str = {3, (BYTE *)"str"}, s_ns = {2, (BYTE *)"ns"},
                     s_act_in = {7, (BYTE *)"act_req"}, s_act_out = {7, (BYTE *)"act_rsp"};
struct in { INT32 val; };
struct out { WCHAR *str; };

static WS_FIELD_DESCRIPTION f_in, f_out;
static WS_FIELD_DESCRIPTION *fl_in[] = {&f_in}, *fl_out[] = {&f_out};
static WS_STRUCT_DESCRIPTION st_in = {sizeof(struct in), 4, fl_in, 1, &s_req, &s_ns};
static WS_STRUCT_DESCRIPTION st_out = {sizeof(struct out), sizeof(void *), fl_out, 1, &s_resp, &s_ns};
static WS_ELEMENT_DESCRIPTION e_in = {&s_req, &s_ns, WS_STRUCT_TYPE, &st_in};
static WS_ELEMENT_DESCRIPTION e_out = {&s_resp, &s_ns, WS_STRUCT_TYPE, &st_out};
static WS_MESSAGE_DESCRIPTION m_in = {&s_act_in, &e_in};
static WS_MESSAGE_DESCRIPTION m_out = {&s_act_out, &e_out};

static void init_descs(void)
{
    f_in.mapping = WS_ELEMENT_FIELD_MAPPING; f_in.localName = &s_val; f_in.ns = &s_ns; f_in.type = WS_INT32_TYPE;
    f_out.mapping = WS_ELEMENT_FIELD_MAPPING; f_out.localName = &s_str; f_out.ns = &s_ns; f_out.type = WS_WSZ_TYPE;
}

static HANDLE server_thread;
static int server_count;
static int server_seen[8];
static HRESULT server_hr[8];

static DWORD WINAPI server_proc(void *arg)
{
    WS_LISTENER *listener;
    WS_CHANNEL *channel;
    WS_HEAP *heap;
    WS_MESSAGE *req, *rsp;
    WS_STRING url = {0};
    WCHAR buf[64];
    int i;
    HRESULT hr;

    url.length = wsprintfW(buf, L"http://127.0.0.1:%u/svc", PORT);
    url.chars = buf;
    hr = WsCreateListener(WS_CHANNEL_TYPE_REPLY, WS_HTTP_CHANNEL_BINDING, NULL, 0, NULL, &listener, NULL);
    if (hr != S_OK) { server_hr[0] = hr; return 1; }
    hr = WsOpenListener(listener, &url, NULL, NULL);
    if (hr != S_OK) { server_hr[0] = hr; return 1; }
    hr = WsCreateChannelForListener(listener, NULL, 0, &channel, NULL);
    if (hr != S_OK) { server_hr[0] = hr; return 1; }
    WsCreateHeap(1 << 16, 0, NULL, 0, &heap, NULL);
    for (i = 0; i < server_count; i++)
    {
        struct in in = {0};
        struct out out;
        WCHAR text[32];
        WsResetHeap(heap, NULL);
        hr = WsAcceptChannel(listener, channel, NULL, NULL);
        if (hr != S_OK) { server_hr[i] = hr; break; }
        WsCreateMessageForChannel(channel, NULL, 0, &req, NULL);
        WsCreateMessageForChannel(channel, NULL, 0, &rsp, NULL);
        hr = WsReadMessageStart(channel, req, NULL, NULL);
        if (hr == S_OK) hr = WsReadBody(req, &e_in, WS_READ_REQUIRED_VALUE, heap, &in, sizeof(in), NULL);
        if (hr == S_OK) hr = WsReadMessageEnd(channel, req, NULL, NULL);
        server_seen[i] = in.val;
        if (hr == S_OK)
        {
            wsprintfW(text, L"got %d", in.val);
            out.str = text;
            hr = WsSendReplyMessage(channel, rsp, &m_out, WS_WRITE_REQUIRED_VALUE, &out, sizeof(out), req, NULL, NULL);
        }
        server_hr[i] = hr;
        WsFreeMessage(req);
        WsFreeMessage(rsp);
        WsResetChannel(channel, NULL);
    }
    WsFreeHeap(heap);
    WsFreeChannel(channel);
    WsCloseListener(listener, NULL, NULL);
    WsFreeListener(listener);
    return 0;
}

static HRESULT call(WS_ENVELOPE_VERSION env, WS_ADDRESSING_VERSION addr, const WCHAR *path, INT32 val, WCHAR **result, WS_HEAP *heap)
{
    WS_CHANNEL_PROPERTY prop[2] = {
        {WS_CHANNEL_PROPERTY_ENVELOPE_VERSION, &env, sizeof(env)},
        {WS_CHANNEL_PROPERTY_ADDRESSING_VERSION, &addr, sizeof(addr)}};
    WS_PARAMETER_DESCRIPTION param[2] = {{WS_PARAMETER_TYPE_NORMAL, 0, 0xffff}, {WS_PARAMETER_TYPE_NORMAL, 0xffff, 0}};
    WS_OPERATION_DESCRIPTION op = {1, &m_in, &m_out, 0, 0, 2, param, NULL, WS_NON_RPC_LITERAL_OPERATION};
    WS_ENDPOINT_ADDRESS ep;
    WS_SERVICE_PROXY *proxy;
    WCHAR url[96];
    WCHAR *str = NULL, **pstr = &str;
    const void *args[2];
    HRESULT hr;

    hr = WsCreateServiceProxy(WS_CHANNEL_TYPE_REQUEST, WS_HTTP_CHANNEL_BINDING, NULL, NULL, 0, prop, 2, &proxy, NULL);
    if (hr != S_OK) return hr;
    memset(&ep, 0, sizeof(ep));
    ep.url.length = wsprintfW(url, L"http://127.0.0.1:%u%s", PORT, path);
    ep.url.chars = url;
    hr = WsOpenServiceProxy(proxy, &ep, NULL, NULL);
    if (hr == S_OK)
    {
        args[0] = &val;
        args[1] = &pstr;
        hr = WsCall(proxy, &op, args, heap, NULL, 0, NULL, NULL);
        *result = str;
        WsCloseServiceProxy(proxy, NULL, NULL);
    }
    WsFreeServiceProxy(proxy);
    return hr;
}

static char *raw(const char *request, int len, const char *tail, int taillen)
{
    static char reply[4096];
    struct sockaddr_in a;
    SOCKET s = socket(AF_INET, SOCK_STREAM, 0);
    int n, total_n = 0;

    memset(&a, 0, sizeof(a));
    a.sin_family = AF_INET;
    a.sin_port = htons(PORT);
    a.sin_addr.s_addr = inet_addr("127.0.0.1");
    reply[0] = 0;
    if (connect(s, (struct sockaddr *)&a, sizeof(a))) { closesocket(s); return reply; }
    send(s, request, len, 0);
    if (tail) { Sleep(200); send(s, tail, taillen, 0); }
    while ((n = recv(s, reply + total_n, sizeof(reply) - 1 - total_n, 0)) > 0) total_n += n;
    reply[total_n] = 0;
    closesocket(s);
    return reply;
}

static const char env11[] = "<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\"><s:Body><req xmlns=\"ns\"><val>%d</val></req></s:Body></s:Envelope>";

int main(void)
{
    WSADATA wsa;
    WS_HEAP *heap;
    WCHAR *str;
    HRESULT hr;
    char body[256], request[1024], *r;
    int n, len;

    init_descs();
    WSAStartup(MAKEWORD(2, 2), &wsa);
    WsCreateHeap(1 << 16, 0, NULL, 0, &heap, NULL);
    server_count = 6;
    server_thread = CreateThread(NULL, 0, server_proc, NULL, 0, NULL);
    Sleep(500);
    EXPECT(server_hr[0] == S_OK);

    /* refused requests do not reach the application and do not stop the accept loop */
    r = raw("GET /svc HTTP/1.1\r\nHost: x\r\n\r\n", 31, NULL, 0);
    EXPECT(strstr(r, "405 Method Not Allowed") != NULL);
    n = sprintf(body, env11, 7);
    len = sprintf(request, "POST /other HTTP/1.1\r\nContent-Type: text/xml\r\nContent-Length: %d\r\n\r\n%s", n, body);
    r = raw(request, len, NULL, 0);
    EXPECT(strstr(r, "404 Not Found") != NULL);
    r = raw("POST /svc HTTP/1.1\r\nContent-Type: text/xml\r\n\r\n", 46, NULL, 0);
    EXPECT(strstr(r, "411 Length Required") != NULL);

    /* chunked request, split over two sends */
    len = sprintf(request, "POST /svc HTTP/1.1\r\nContent-Type: text/xml; charset=utf-8\r\nTransfer-Encoding: chunked\r\n\r\n"
                  "%x\r\n%.*s\r\n", 20, 20, body);
    n = sprintf(request + len + 0, "%x\r\n%s\r\n0\r\n\r\n", (int)strlen(body) - 20, body + 20);
    r = raw(request, len, request + len, n);
    EXPECT(strstr(r, "HTTP/1.1 200 OK") != NULL);
    EXPECT(strstr(r, "Content-Type: application/soap+xml; charset=utf-8") != NULL);
    EXPECT(strstr(r, "got 7") != NULL);
    EXPECT(strstr(r, "Connection: close") != NULL);

    /* plain request with Content-Length and an Expect header; the lower case prefix still matches */
    n = sprintf(body, env11, 8);
    len = sprintf(request, "POST /SVC HTTP/1.1\r\nExpect: 100-continue\r\ncontent-length: %d\r\nContent-Type: text/xml\r\n\r\n%s", n, body);
    r = raw(request, len, NULL, 0);
    EXPECT(strstr(r, "100 Continue") != NULL && strstr(r, "200 OK") != NULL && strstr(r, "got 8") != NULL);

    /* clients of this library */
    str = NULL;
    hr = call(WS_ENVELOPE_VERSION_SOAP_1_1, WS_ADDRESSING_VERSION_TRANSPORT, L"/svc", 21, &str, heap);
    EXPECT(hr == S_OK);
    EXPECT(str && !wcscmp(str, L"got 21"));
    str = NULL;
    hr = call(WS_ENVELOPE_VERSION_SOAP_1_2, WS_ADDRESSING_VERSION_0_9, L"/svc", 22, &str, heap);
    EXPECT(hr == S_OK);
    EXPECT(str && !wcscmp(str, L"got 22"));

    WaitForSingleObject(server_thread, 10000);
    EXPECT(server_hr[0] == S_OK && server_hr[1] == S_OK && server_hr[2] == S_OK && server_hr[3] == S_OK);
    EXPECT(server_seen[0] == 7 && server_seen[1] == 8 && server_seen[2] == 21 && server_seen[3] == 22);
    printf("checks %d, mismatches %d\n", total, bad);
    printf("RESULT: %s\n", bad ? "FAIL" : "PASS");
    return bad != 0;
}
