/* ALPC ports within one process (patches/sg/6126): the Windows App SDK's CoreMessagingXP creates
 * its registrar port and connects to it from the same process; none of the ALPC calls existed.
 * A server thread and the main thread (the client) go through: connection request, accept with a
 * port context, synchronous request and reply (the reply sent with the release flag, as
 * CoreMessagingXP sends it), a datagram, a release that answers nothing, disconnect. Prints
 * KEY=VALUE lines. */
#include <windows.h>
#include <winternl.h>
#include <stdio.h>
#include <string.h>

struct port_message
{
    USHORT data_length, total_length, type, data_info_offset;
    CLIENT_ID client_id;
    ULONG message_id;
    union { SIZE_T client_view_size; ULONG callback_id; } u;
};
struct msgbuf { struct port_message h; char data[200]; };

typedef NTSTATUS (WINAPI *create_fn)(HANDLE *, OBJECT_ATTRIBUTES *, void *);
typedef NTSTATUS (WINAPI *connect_fn)(HANDLE *, UNICODE_STRING *, OBJECT_ATTRIBUTES *, void *, ULONG, void *,
                                      struct port_message *, SIZE_T *, void *, void *, LARGE_INTEGER *);
typedef NTSTATUS (WINAPI *accept_fn)(HANDLE *, HANDLE, ULONG, OBJECT_ATTRIBUTES *, void *, void *,
                                     struct port_message *, void *, BOOLEAN);
typedef NTSTATUS (WINAPI *swr_fn)(HANDLE, ULONG, struct port_message *, void *, struct port_message *, SIZE_T *,
                                  void *, LARGE_INTEGER *);
typedef NTSTATUS (WINAPI *disconnect_fn)(HANDLE, ULONG);
typedef NTSTATUS (WINAPI *init_attr_fn)(ULONG, void *, SIZE_T, SIZE_T *);
typedef void *(WINAPI *get_attr_fn)(void *, ULONG);

static create_fn pCreate;
static connect_fn pConnect;
static accept_fn pAccept;
static swr_fn pSwr;
static disconnect_fn pDisconnect;
static init_attr_fn pInitAttr;
static get_attr_fn pGetAttr;
static HANDLE server_port;
static UNICODE_STRING port_name;
static char server_log[1024];

#define LOG(...) do { size_t n = strlen(server_log); snprintf(server_log + n, sizeof(server_log) - n, __VA_ARGS__); } while (0)

static void set_data(struct msgbuf *m, const char *text)
{
    memset(&m->h, 0, sizeof(m->h));
    m->h.data_length = strlen(text) + 1;
    m->h.total_length = sizeof(m->h) + m->h.data_length;
    strcpy(m->data, text);
}

static DWORD WINAPI server(void *arg)
{
    struct msgbuf m, r;
    SIZE_T size;
    HANDLE comm = NULL;
    NTSTATUS status;
    BYTE attrs[256];
    SIZE_T need;
    void **ctx;
    int i;

    for (i = 0; i < 4; i++)
    {
        size = sizeof(m);
        pInitAttr(0x20000000, attrs, sizeof(attrs), &need);
        status = pSwr(server_port, 0, NULL, NULL, &m.h, &size, attrs, NULL);
        if (status) { LOG("srv_recv%d=0x%08lx\n", i, status); return 0; }
        ctx = pGetAttr(attrs, 0x20000000);
        switch (m.h.type & 0xff)
        {
        case 10: /* connection request */
            LOG("srv_conn=%s\n", m.data);
            status = pAccept(&comm, server_port, 0, NULL, NULL, (void *)0xc0ffee, &m.h, NULL, TRUE);
            LOG("srv_accept=0x%08lx\n", status);
            break;
        case 1: /* request */
            LOG("srv_req=%s ctx=%p valid=%d\n", m.data, ctx ? ctx[0] : NULL, (((ULONG *)attrs)[1] & 0x20000000) != 0);
            set_data(&r, "release-only");
            LOG("srv_release=0x%08lx\n", pSwr(server_port, 0x10000, &r.h, NULL, NULL, NULL, NULL, NULL));
            set_data(&r, "pong");
            r.h.message_id = m.h.message_id;
            r.h.client_id = m.h.client_id;
            LOG("srv_reply=0x%08lx\n", pSwr(server_port, 0x10000, &r.h, NULL, NULL, NULL, NULL, NULL));
            break;
        case 3: /* datagram */
            LOG("srv_dgram=%s\n", m.data);
            break;
        case 5: /* port closed */
            LOG("srv_closed=1\n");
            break;
        default:
            LOG("srv_type=%u\n", m.h.type);
        }
    }
    return 0;
}

int main(void)
{
    HMODULE ntdll = GetModuleHandleA("ntdll.dll");
    OBJECT_ATTRIBUTES attr;
    struct msgbuf m, r;
    HANDLE client, thread;
    NTSTATUS status;
    SIZE_T size, need = 0;
    LARGE_INTEGER timeout;
    BYTE buf[256];

    pCreate = (void *)GetProcAddress(ntdll, "NtAlpcCreatePort");
    pConnect = (void *)GetProcAddress(ntdll, "NtAlpcConnectPort");
    pAccept = (void *)GetProcAddress(ntdll, "NtAlpcAcceptConnectPort");
    pSwr = (void *)GetProcAddress(ntdll, "NtAlpcSendWaitReceivePort");
    pDisconnect = (void *)GetProcAddress(ntdll, "NtAlpcDisconnectPort");
    pInitAttr = (void *)GetProcAddress(ntdll, "AlpcInitializeMessageAttribute");
    pGetAttr = (void *)GetProcAddress(ntdll, "AlpcGetMessageAttribute");
    printf("exports=%d\n", pCreate && pConnect && pAccept && pSwr && pDisconnect && pInitAttr && pGetAttr);
    if (!pCreate || !pConnect || !pAccept || !pSwr || !pDisconnect || !pInitAttr || !pGetAttr) return 1;

    /* attributes: sizes and placement */
    pInitAttr(0x80000000 | 0x20000000 | 0x02000000, NULL, 0, &need);
    printf("attr_size=%Iu\n", need);
    printf("attr_init=0x%08lx\n", pInitAttr(0x80000000 | 0x20000000 | 0x02000000, buf, sizeof(buf), &need));
    printf("attr_ctx_offset=%Iu\n", (BYTE *)pGetAttr(buf, 0x20000000) - buf);
    printf("attr_absent=%d\n", pGetAttr(buf, 0x40000000) == NULL);

    RtlInitUnicodeString(&port_name, L"\\BaseNamedObjects\\SgAlpcTest");
    InitializeObjectAttributes(&attr, &port_name, 0, NULL, NULL);
    status = pCreate(&server_port, &attr, NULL);
    printf("create=0x%08lx\n", status);
    printf("create_again=0x%08lx\n", pCreate(&client, &attr, NULL));
    thread = CreateThread(NULL, 0, server, NULL, 0, NULL);

    set_data(&m, "hello");
    size = sizeof(m);
    timeout.QuadPart = -50000000;
    status = pConnect(&client, &port_name, NULL, NULL, 0x20000, NULL, &m.h, &size, NULL, NULL, &timeout);
    printf("connect=0x%08lx\n", status);

    set_data(&m, "ping");
    size = sizeof(r);
    status = pSwr(client, 0x20000, &m.h, NULL, &r.h, &size, NULL, &timeout);
    printf("request=0x%08lx reply_type=%u reply=%s\n", status, r.h.type & 0xff, status ? "" : r.data);

    set_data(&m, "note");
    printf("datagram=0x%08lx\n", pSwr(client, 0, &m.h, NULL, NULL, NULL, NULL, NULL));
    Sleep(200);
    printf("disconnect=0x%08lx\n", pDisconnect(client, 0));
    WaitForSingleObject(thread, 5000);
    printf("%s", server_log);
    {
        UNICODE_STRING missing;
        RtlInitUnicodeString(&missing, L"\\BaseNamedObjects\\SgAlpcNobody");
        timeout.QuadPart = -10000000;
        printf("connect_missing=0x%08lx\n", pConnect(&client, &missing, NULL, NULL, 0x20000, NULL, NULL, NULL, NULL, NULL, &timeout));
    }
    return 0;
}
