/* wsnetpipe-probe.c: WWSAPI's named pipe binding (WS_NAMEDPIPE_CHANNEL_BINDING, wine-sg 7852).
 * A net.pipe service (WsCreateListener/WsOpenListener/WsAcceptChannel) and a client
 * (WsCreateChannel/WsOpenChannel) on one machine: a duplex session both ways. The service is
 * found as WCF's NetNamedPipeBinding finds one -- checked here from the scheme itself (the probe
 * builds the shared memory name, reads the GUID, finds the pipe) -- and the bytes on the pipe are
 * .NET message framing: a raw client's preamble is answered with PreambleAck (0x0b).
 * Prints PASS/FAIL lines and RESULT. */
#include <stdio.h>
#include <string.h>
#include <windows.h>
#include <webservices.h>

static int failures;
#define CHECK(c, ...) do { if (!(c)) { failures++; printf("FAIL  " __VA_ARGS__); printf("\n"); } \
                           else { printf("PASS  " __VA_ARGS__); printf("\n"); } } while (0)

static WS_XML_STRING action = {6, (BYTE *)"action"}, localname = {9, (BYTE *)"localname"}, ns = {2, (BYTE *)"ns"};
static WS_ELEMENT_DESCRIPTION body = { &localname, &ns, WS_INT32_TYPE, NULL };
static WS_MESSAGE_DESCRIPTION desc = { &action, &body };
static HANDLE ready, done;
static INT32 got[2];
static HRESULT server_hr[4];

static const WCHAR url_service[] = L"net.pipe://localhost/sg/echo";

static void b64(const BYTE *d, int n, char *out)
{
    static const char t[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    int i, o = 0;
    for (i = 0; i < n; i += 3)
    {
        unsigned v = d[i] << 16 | (i + 1 < n ? d[i + 1] << 8 : 0) | (i + 2 < n ? d[i + 2] : 0);
        out[o++] = t[v >> 18 & 63]; out[o++] = t[v >> 12 & 63];
        out[o++] = i + 1 < n ? t[v >> 6 & 63] : '=';
        out[o++] = i + 2 < n ? t[v & 63] : '=';
    }
    out[o] = 0;
}

/* as WCF: "Global\" or "Local\" + "net.pipe:E" + base64(UTF-8 "net.pipe://+" + upper path) */
static WCHAR *find_pipe(const char *canon, WCHAR *pipe_name, int *global)
{
    char enc[256], name[300];
    int g;
    b64((const BYTE *)canon, strlen(canon), enc);
    for (g = 1; g >= 0; g--)
    {
        HANDLE m;
        BYTE *v;
        sprintf(name, "%snet.pipe:E%s", g ? "Global\\" : "Local\\", enc);
        if (!(m = OpenFileMappingA(FILE_MAP_READ, FALSE, name))) continue;
        v = MapViewOfFile(m, FILE_MAP_READ, 0, 0, 20);
        if (v && v[0])
        {
            GUID *guid = (GUID *)(v + 4);
            swprintf(pipe_name, 80, L"\\\\.\\pipe\\%08lx-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x",
                     guid->Data1, guid->Data2, guid->Data3, guid->Data4[0], guid->Data4[1], guid->Data4[2],
                     guid->Data4[3], guid->Data4[4], guid->Data4[5], guid->Data4[6], guid->Data4[7]);
            *global = g;
            UnmapViewOfFile(v);
            CloseHandle(m);
            return pipe_name;
        }
        if (v) UnmapViewOfFile(v);
        CloseHandle(m);
    }
    return NULL;
}

static DWORD CALLBACK server(void *arg)
{
    WS_LISTENER *listener = arg;
    WS_CHANNEL *channel;
    WS_MESSAGE *msg;
    const WS_MESSAGE_DESCRIPTION *descs[1] = { &desc };
    INT32 val;
    int i;

    /* first session: two messages in, one out */
    WsCreateChannelForListener(listener, NULL, 0, &channel, NULL);
    SetEvent(ready);
    server_hr[0] = WsAcceptChannel(listener, channel, NULL, NULL);
    for (i = 0; i < 2 && server_hr[0] == S_OK; i++)
    {
        WsCreateMessageForChannel(channel, NULL, 0, &msg, NULL);
        server_hr[1] = WsReceiveMessage(channel, msg, descs, 1, WS_RECEIVE_REQUIRED_MESSAGE, WS_READ_REQUIRED_VALUE,
                                        NULL, &got[i], sizeof(got[i]), NULL, NULL, NULL);
        WsFreeMessage(msg);
        if (server_hr[1] != S_OK) break;
    }
    if (server_hr[1] == S_OK)
    {
        val = got[0] + got[1];
        WsCreateMessageForChannel(channel, NULL, 0, &msg, NULL);
        server_hr[2] = WsSendMessage(channel, msg, &desc, WS_WRITE_REQUIRED_VALUE, &val, sizeof(val), NULL, NULL);
        WsFreeMessage(msg);
    }
    WaitForSingleObject(done, 10000);
    WsCloseChannel(channel, NULL, NULL);
    WsFreeChannel(channel);

    /* second session: a raw client's preamble (read when a message is asked for) */
    WsCreateChannelForListener(listener, NULL, 0, &channel, NULL);
    SetEvent(ready);
    server_hr[3] = WsAcceptChannel(listener, channel, NULL, NULL);
    if (server_hr[3] == S_OK)
    {
        WsCreateMessageForChannel(channel, NULL, 0, &msg, NULL);
        WsReceiveMessage(channel, msg, descs, 1, WS_RECEIVE_REQUIRED_MESSAGE, WS_READ_REQUIRED_VALUE,
                         NULL, &val, sizeof(val), NULL, NULL, NULL);
        WsFreeMessage(msg);
    }
    WsCloseChannel(channel, NULL, NULL);
    WsFreeChannel(channel);
    return 0;
}

/* net.tcp, the same session code over a socket: a control */
static HRESULT tcp_hr[2];
static INT32 tcp_got;
static HANDLE tcp_received;

static DWORD CALLBACK tcp_server(void *arg)
{
    WS_LISTENER *listener = arg;
    const WS_MESSAGE_DESCRIPTION *descs[1] = { &desc };
    WS_CHANNEL *channel;
    WS_MESSAGE *msg;

    WsCreateChannelForListener(listener, NULL, 0, &channel, NULL);
    SetEvent(ready);
    if ((tcp_hr[0] = WsAcceptChannel(listener, channel, NULL, NULL)) == S_OK)
    {
        WsCreateMessageForChannel(channel, NULL, 0, &msg, NULL);
        tcp_hr[1] = WsReceiveMessage(channel, msg, descs, 1, WS_RECEIVE_REQUIRED_MESSAGE, WS_READ_REQUIRED_VALUE,
                                     NULL, &tcp_got, sizeof(tcp_got), NULL, NULL, NULL);
        WsFreeMessage(msg);
    }
    SetEvent(tcp_received);
    WaitForSingleObject(done, 10000);
    WsCloseChannel(channel, NULL, NULL);
    WsFreeChannel(channel);
    return 0;
}

static void tcp_control(void)
{
    WCHAR buf[64];
    WS_STRING url;
    WS_LISTENER *listener;
    WS_CHANNEL *channel;
    WS_MESSAGE *msg;
    WS_ENDPOINT_ADDRESS addr;
    HANDLE thread;
    INT32 val = 7;
    HRESULT hr;
    unsigned port = 20000 + GetCurrentProcessId() % 20000;

    url.length = swprintf(buf, 64, L"net.tcp://localhost:%u", port);
    url.chars = buf;
    WsCreateListener(WS_CHANNEL_TYPE_DUPLEX_SESSION, WS_TCP_CHANNEL_BINDING, NULL, 0, NULL, &listener, NULL);
    hr = WsOpenListener(listener, &url, NULL, NULL);
    if (hr != S_OK)
    {
        printf("NOTE  net.tcp control skipped: no listener on port %u (%#lx)\n", port, hr);
        WsFreeListener(listener);
        return;
    }
    tcp_received = CreateEventW(NULL, TRUE, FALSE, NULL);
    thread = CreateThread(NULL, 0, tcp_server, listener, 0, NULL);
    WaitForSingleObject(ready, 5000);
    WsCreateChannel(WS_CHANNEL_TYPE_DUPLEX_SESSION, WS_TCP_CHANNEL_BINDING, NULL, 0, NULL, &channel, NULL);
    memset(&addr, 0, sizeof(addr));
    addr.url = url;
    hr = WsOpenChannel(channel, &addr, NULL, NULL);
    if (hr == S_OK)
    {
        WsCreateMessageForChannel(channel, NULL, 0, &msg, NULL);
        hr = WsSendMessage(channel, msg, &desc, WS_WRITE_REQUIRED_VALUE, &val, sizeof(val), NULL, NULL);
        WsFreeMessage(msg);
    }
    WaitForSingleObject(tcp_received, 10000);
    CHECK(hr == S_OK && tcp_hr[0] == S_OK && tcp_hr[1] == S_OK && tcp_got == 7,
          "control: a net.tcp session still works (%#lx %#lx %#lx: %d)", hr, tcp_hr[0], tcp_hr[1], tcp_got);
    WsShutdownSessionChannel(channel, NULL, NULL);
    WsCloseChannel(channel, NULL, NULL);
    WsFreeChannel(channel);
    SetEvent(done);
    WaitForSingleObject(thread, 10000);
    WsCloseListener(listener, NULL, NULL);
    WsFreeListener(listener);
}

int main(void)
{
    const WS_MESSAGE_DESCRIPTION *descs[1] = { &desc };
    WS_STRING url = { (ULONG)(sizeof(url_service) / sizeof(WCHAR) - 1), (WCHAR *)url_service };
    WS_LISTENER *listener, *other;
    WS_CHANNEL *channel;
    WS_MESSAGE *msg;
    WS_ENDPOINT_ADDRESS addr;
    WCHAR pipe_name[80], *found;
    INT32 val, reply = 0;
    HANDLE thread, raw;
    int global = -1;
    HRESULT hr;

    hr = WsCreateListener(WS_CHANNEL_TYPE_DUPLEX, WS_NAMEDPIPE_CHANNEL_BINDING, NULL, 0, NULL, &other, NULL);
    CHECK(hr == E_INVALIDARG, "a named pipe listener is a duplex session one (%#lx)", hr);
    hr = WsCreateChannel(WS_CHANNEL_TYPE_DUPLEX, WS_NAMEDPIPE_CHANNEL_BINDING, NULL, 0, NULL, &channel, NULL);
    CHECK(hr == E_INVALIDARG, "a named pipe channel is a duplex session one (%#lx)", hr);

    hr = WsCreateListener(WS_CHANNEL_TYPE_DUPLEX_SESSION, WS_NAMEDPIPE_CHANNEL_BINDING, NULL, 0, NULL, &listener, NULL);
    CHECK(hr == S_OK, "WsCreateListener (named pipe, duplex session) (%#lx)", hr);
    if (hr != S_OK) { puts("RESULT: FAIL"); return 1; }
    hr = WsOpenListener(listener, &url, NULL, NULL);
    CHECK(hr == S_OK, "WsOpenListener %ls (%#lx)", url_service, hr);

    found = find_pipe("net.pipe://+/SG/ECHO/", pipe_name, &global);
    CHECK(found != NULL, "its pipe is published as WCF's are (%s\\net.pipe:E<base64 of net.pipe://+/SG/ECHO/>)",
          global == 1 ? "Global" : global == 0 ? "Local" : "none");
    CHECK(found && WaitNamedPipeW(pipe_name, 2000), "and the pipe it names waits for a client (%ls)", found ? found : L"-");

    WsCreateListener(WS_CHANNEL_TYPE_DUPLEX_SESSION, WS_NAMEDPIPE_CHANNEL_BINDING, NULL, 0, NULL, &other, NULL);
    hr = WsOpenListener(other, &url, NULL, NULL);
    CHECK(FAILED(hr), "a second service at the same address is refused (%#lx)", hr);
    WsFreeListener(other);

    ready = CreateEventW(NULL, FALSE, FALSE, NULL);
    done = CreateEventW(NULL, FALSE, FALSE, NULL);
    thread = CreateThread(NULL, 0, server, listener, 0, NULL);
    WaitForSingleObject(ready, 5000);

    /* the client, at a path below the service's (found walking up, as WCF's are) */
    hr = WsCreateChannel(WS_CHANNEL_TYPE_DUPLEX_SESSION, WS_NAMEDPIPE_CHANNEL_BINDING, NULL, 0, NULL, &channel, NULL);
    CHECK(hr == S_OK, "WsCreateChannel (named pipe, duplex session) (%#lx)", hr);
    memset(&addr, 0, sizeof(addr));
    addr.url.chars = (WCHAR *)L"net.pipe://localhost/sg/echo/sub";
    addr.url.length = wcslen(addr.url.chars);
    hr = WsOpenChannel(channel, &addr, NULL, NULL);
    CHECK(hr == S_OK, "WsOpenChannel net.pipe://localhost/sg/echo/sub finds the service (%#lx)", hr);
    val = 40;
    WsCreateMessageForChannel(channel, NULL, 0, &msg, NULL);
    hr = WsSendMessage(channel, msg, &desc, WS_WRITE_REQUIRED_VALUE, &val, sizeof(val), NULL, NULL);
    WsFreeMessage(msg);
    CHECK(hr == S_OK, "the first message is sent (%#lx)", hr);
    val = 2;
    WsCreateMessageForChannel(channel, NULL, 0, &msg, NULL);
    hr = WsSendMessage(channel, msg, &desc, WS_WRITE_REQUIRED_VALUE, &val, sizeof(val), NULL, NULL);
    WsFreeMessage(msg);
    CHECK(hr == S_OK, "the second message is sent (%#lx)", hr);
    WsCreateMessageForChannel(channel, NULL, 0, &msg, NULL);
    hr = WsReceiveMessage(channel, msg, descs, 1, WS_RECEIVE_REQUIRED_MESSAGE, WS_READ_REQUIRED_VALUE,
                          NULL, &reply, sizeof(reply), NULL, NULL, NULL);
    WsFreeMessage(msg);
    CHECK(server_hr[0] == S_OK && server_hr[1] == S_OK && got[0] == 40 && got[1] == 2,
          "the service accepted the session and read both (%#lx %#lx: %d %d)", server_hr[0], server_hr[1], got[0], got[1]);
    CHECK(hr == S_OK && server_hr[2] == S_OK && reply == 42, "the service's reply comes back (%#lx %#lx: %d)",
          hr, server_hr[2], reply);
    hr = WsShutdownSessionChannel(channel, NULL, NULL);
    CHECK(hr == S_OK, "WsShutdownSessionChannel (%#lx)", hr);
    WsCloseChannel(channel, NULL, NULL);
    WsFreeChannel(channel);
    SetEvent(done);

    /* a raw client as WCF's: .NET message framing on the pipe */
    WaitForSingleObject(ready, 5000);
    raw = found ? CreateFileW(pipe_name, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL) : INVALID_HANDLE_VALUE;
    {
        static const char via[] = "net.pipe://localhost/sg/echo";
        BYTE pre[64], ack = 0;
        DWORD n = 0, w = 0, mode = PIPE_READMODE_MESSAGE;
        pre[n++] = 0x00; pre[n++] = 1; pre[n++] = 0;          /* version 1.0 */
        pre[n++] = 0x01; pre[n++] = 0x02;                     /* mode: duplex */
        pre[n++] = 0x02; pre[n++] = sizeof(via) - 1;          /* via */
        memcpy(pre + n, via, sizeof(via) - 1); n += sizeof(via) - 1;
        pre[n++] = 0x03; pre[n++] = 0x08;                     /* known encoding: binary session */
        pre[n++] = 0x0c;                                      /* preamble end */
        if (raw != INVALID_HANDLE_VALUE)
        {
            SetNamedPipeHandleState(raw, &mode, NULL, NULL);
            WriteFile(raw, pre, n, &w, NULL);
            ReadFile(raw, &ack, 1, &n, NULL);
        }
        CHECK(raw != INVALID_HANDLE_VALUE && ack == 0x0b, "a raw .NET framing preamble is answered with PreambleAck (%#x)", ack);
        if (raw != INVALID_HANDLE_VALUE)
        {
            BYTE end = 0x07;   /* end record: the session over */
            WriteFile(raw, &end, 1, &w, NULL);
            CloseHandle(raw);
        }
    }
    WaitForSingleObject(thread, 10000);
    CHECK(server_hr[3] == S_OK, "the service accepted the raw client too (%#lx)", server_hr[3]);

    hr = WsCloseListener(listener, NULL, NULL);
    CHECK(hr == S_OK, "WsCloseListener (%#lx)", hr);
    WsFreeListener(listener);
    CHECK(!find_pipe("net.pipe://+/SG/ECHO/", pipe_name, &global), "closed, its name is no longer published");

    hr = WsCreateChannel(WS_CHANNEL_TYPE_DUPLEX_SESSION, WS_NAMEDPIPE_CHANNEL_BINDING, NULL, 0, NULL, &channel, NULL);
    addr.url.chars = (WCHAR *)L"net.pipe://localhost/nothing/here";
    addr.url.length = wcslen(addr.url.chars);
    hr = WsOpenChannel(channel, &addr, NULL, NULL);
    CHECK(FAILED(hr), "no service there: WsOpenChannel fails (%#lx)", hr);
    WsFreeChannel(channel);

    tcp_control();

    puts(failures ? "RESULT: FAIL" : "RESULT: PASS");
    return failures != 0;
}
