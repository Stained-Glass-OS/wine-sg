/* Probe for patches/sg/5020: DdeDisconnect called by the server of a
 * conversation ends it: it returns TRUE, the client's callback gets
 * XTYP_DISCONNECT ("sent to the DDE callback function of the partner"), and
 * the server's handle is no longer a conversation. A client's DdeDisconnect
 * still works.
 * Table driven; lines "ok"/"FAIL", last "RESULT: PASS|FAIL". */
#include <windows.h>
#include <ddeml.h>
#include <stdio.h>

static int failures;
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

static DWORD server_inst, client_inst;
static HCONV server_conv;
static LONG client_disconnects, server_disconnects;
static HANDLE server_ready, server_done, disconnect_now;
static BOOL server_ret;
static DWORD server_err;

static HDDEDATA CALLBACK server_cb(UINT type, UINT fmt, HCONV conv, HSZ hsz1, HSZ hsz2, HDDEDATA data, ULONG_PTR d1, ULONG_PTR d2)
{
    switch (type)
    {
    case XTYP_CONNECT: return (HDDEDATA)TRUE;
    case XTYP_CONNECT_CONFIRM: server_conv = conv; return 0;
    case XTYP_DISCONNECT: InterlockedIncrement(&server_disconnects); return 0;
    }
    return 0;
}

static HDDEDATA CALLBACK client_cb(UINT type, UINT fmt, HCONV conv, HSZ hsz1, HSZ hsz2, HDDEDATA data, ULONG_PTR d1, ULONG_PTR d2)
{
    if (type == XTYP_DISCONNECT) InterlockedIncrement(&client_disconnects);
    return 0;
}

static void pump(DWORD ms)
{
    DWORD end = GetTickCount() + ms;
    MSG msg;
    while ((LONG)(end - GetTickCount()) > 0)
    {
        while (PeekMessageA(&msg, 0, 0, 0, PM_REMOVE)) DispatchMessageA(&msg);
        Sleep(10);
    }
}

static DWORD WINAPI server_thread(void *arg)
{
    HSZ service;
    MSG msg;

    DdeInitializeA(&server_inst, server_cb, APPCLASS_STANDARD | CBF_SKIP_REGISTRATIONS | CBF_SKIP_UNREGISTRATIONS, 0);
    service = DdeCreateStringHandleA(server_inst, "SgDdeService", CP_WINANSI);
    DdeNameService(server_inst, service, 0, DNS_REGISTER);
    SetEvent(server_ready);
    for (;;)
    {
        DWORD r = MsgWaitForMultipleObjects(1, &disconnect_now, FALSE, 5000, QS_ALLINPUT);
        if (r == WAIT_OBJECT_0)
        {
            server_ret = DdeDisconnect(server_conv);
            server_err = DdeGetLastError(server_inst);
            ResetEvent(disconnect_now);
            SetEvent(server_done);
            continue;
        }
        if (r == WAIT_TIMEOUT) break;
        while (PeekMessageA(&msg, 0, 0, 0, PM_REMOVE)) DispatchMessageA(&msg);
    }
    DdeNameService(server_inst, service, 0, DNS_UNREGISTER);
    DdeFreeStringHandle(server_inst, service);
    DdeUninitialize(server_inst);
    return 0;
}

int main(void)
{
    HSZ service, topic[2];
    HCONV conv;
    HANDLE thread;
    CONVINFO info;
    UINT i;

    server_ready = CreateEventA(NULL, FALSE, FALSE, NULL);
    server_done = CreateEventA(NULL, FALSE, FALSE, NULL);
    disconnect_now = CreateEventA(NULL, TRUE, FALSE, NULL);
    thread = CreateThread(NULL, 0, server_thread, NULL, 0, NULL);
    WaitForSingleObject(server_ready, 5000);

    DdeInitializeA(&client_inst, client_cb, APPCMD_CLIENTONLY, 0);
    service = DdeCreateStringHandleA(client_inst, "SgDdeService", CP_WINANSI);
    topic[0] = DdeCreateStringHandleA(client_inst, "SgTopic", CP_WINANSI);
    topic[1] = DdeCreateStringHandleA(client_inst, "SgTopic2", CP_WINANSI);

    for (i = 0; i < 2; i++)
    {
        const char *who = i ? "client" : "server";

        server_conv = NULL;
        client_disconnects = server_disconnects = 0;
        conv = DdeConnect(client_inst, service, topic[i], NULL);
        check(conv != NULL, "%s round: connected (%u)", who, DdeGetLastError(client_inst));
        if (!conv) continue;
        pump(200);
        check(server_conv != NULL, "%s round: the server has the conversation", who);
        if (!i)
        {
            SetEvent(disconnect_now);
            while (MsgWaitForMultipleObjects(1, &server_done, FALSE, 5000, QS_ALLINPUT) == WAIT_OBJECT_0 + 1) pump(10);
            pump(300);
            check(server_ret, "server DdeDisconnect: %d (error %#lx)", server_ret, server_err);
            check(client_disconnects == 1, "the client got XTYP_DISCONNECT (%ld)", client_disconnects);
            info.cb = sizeof(info);
            check(!DdeQueryConvInfo(conv, QID_SYNC, &info) || !(info.wStatus & ST_CONNECTED),
                  "the client's conversation is no longer connected");
            DdeDisconnect(conv);
        }
        else
        {
            check(DdeDisconnect(conv), "client DdeDisconnect");
            pump(300);
            check(server_disconnects == 1, "the server got XTYP_DISCONNECT (%ld)", server_disconnects);
        }
    }
    DdeFreeStringHandle(client_inst, service);
    DdeFreeStringHandle(client_inst, topic[0]);
    DdeFreeStringHandle(client_inst, topic[1]);
    DdeUninitialize(client_inst);
    WaitForSingleObject(thread, 10000);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
