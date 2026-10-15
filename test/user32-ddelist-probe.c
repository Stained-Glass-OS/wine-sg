/* Probe for patches/sg/5207: DdeConnectList / DdeQueryNextServer / DdeDisconnectList.
 * Four DDEML servers run in this process (services SgSvcA, SgSvcB, SgSvcC (started later) and
 * SgSvcD, topic "Topic" except D's "Elsewhere"); a client instance connects to all that support a
 * service/topic pair, walks the list, refreshes it, and disconnects it.
 * Table driven; last line "RESULT: PASS|FAIL". */
#include <windows.h>
#include <ddeml.h>
#include <stdio.h>
#include <string.h>

static int failures;
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

#define NSERVERS 4
static const char *services[NSERVERS] = { "SgSvcA", "SgSvcB", "SgSvcC", "SgSvcD" };
static const char *topics[NSERVERS] = { "Topic", "Topic", "Topic", "Elsewhere" };
static DWORD server_inst[NSERVERS];
static int server_disconnects[NSERVERS];
static DWORD client_inst;
static int client_disconnects;

static void query(DWORD inst, HSZ hsz, char *buf, int size)
{
    buf[0] = 0;
    if (hsz) DdeQueryStringA(inst, hsz, buf, size, CP_WINANSI);
}

static HDDEDATA server_cb(int idx, UINT type, HSZ hsz1, HSZ hsz2)
{
    char s1[64], s2[64];

    if (type == XTYP_CONNECT)
    {
        query(server_inst[idx], hsz1, s1, sizeof(s1));
        query(server_inst[idx], hsz2, s2, sizeof(s2));
        return (HDDEDATA)(ULONG_PTR)(!strcmp(s1, topics[idx]) && !strcmp(s2, services[idx]));
    }
    if (type == XTYP_WILDCONNECT)
    {
        HSZPAIR pairs[2];
        query(server_inst[idx], hsz1, s1, sizeof(s1));   /* topic, or none */
        query(server_inst[idx], hsz2, s2, sizeof(s2));   /* service, or none */
        if ((s2[0] && strcmp(s2, services[idx])) || (s1[0] && strcmp(s1, topics[idx])))
            return DdeCreateDataHandle(server_inst[idx], (BYTE *)"\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0", 2 * sizeof(HSZ), 0, 0, 0, 0);
        pairs[0].hszSvc = DdeCreateStringHandleA(server_inst[idx], services[idx], CP_WINANSI);
        pairs[0].hszTopic = DdeCreateStringHandleA(server_inst[idx], topics[idx], CP_WINANSI);
        pairs[1].hszSvc = pairs[1].hszTopic = 0;
        return DdeCreateDataHandle(server_inst[idx], (BYTE *)pairs, sizeof(pairs), 0, 0, 0, 0);
    }
    if (type == XTYP_DISCONNECT) server_disconnects[idx]++;
    return 0;
}

#define CB(n) static HDDEDATA CALLBACK cb##n(UINT t, UINT f, HCONV c, HSZ h1, HSZ h2, HDDEDATA d, ULONG_PTR a, ULONG_PTR b) { return server_cb(n, t, h1, h2); }
CB(0) CB(1) CB(2) CB(3)
static PFNCALLBACK callbacks[NSERVERS] = { cb0, cb1, cb2, cb3 };

static HDDEDATA CALLBACK client_cb(UINT t, UINT f, HCONV c, HSZ h1, HSZ h2, HDDEDATA d, ULONG_PTR a, ULONG_PTR b)
{
    if (t == XTYP_DISCONNECT) client_disconnects++;
    return 0;
}

static void start_server(int i)
{
    HSZ svc;
    UINT ret = DdeInitializeA(&server_inst[i], callbacks[i], APPCLASS_STANDARD, 0);
    check(ret == DMLERR_NO_ERROR, "server %s initialized", services[i]);
    svc = DdeCreateStringHandleA(server_inst[i], services[i], CP_WINANSI);
    check(DdeNameService(server_inst[i], svc, 0, DNS_REGISTER) != 0, "server %s registered", services[i]);
}

static void pump(void)
{
    MSG msg;
    int i;
    for (i = 0; i < 20; i++)
    {
        while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) DispatchMessageA(&msg);
        Sleep(10);
    }
}

/* the service names a list holds, in order, into out[] (comma separated) */
static int list_services(HCONVLIST list, char *out, int size, HCONV *handles, int max)
{
    HCONV conv = 0;
    int n = 0;

    out[0] = 0;
    while ((conv = DdeQueryNextServer(list, conv)) != 0)
    {
        CONVINFO ci = { sizeof(ci) };
        char name[64];
        if (n < max) handles[n] = conv;
        n++;
        if (DdeQueryConvInfo(conv, QID_SYNC, &ci))
        {
            query(client_inst, ci.hszSvcPartner, name, sizeof(name));
            if (out[0]) strncat(out, ",", size - strlen(out) - 1);
            strncat(out, name, size - strlen(out) - 1);
        }
        if (n > 20) break;
    }
    return n;
}

int main(void)
{
    char names[256];
    HCONV handles[8] = {0}, handles2[8] = {0}, bogus = (HCONV)0x1234;
    HCONVLIST list1, list2, list3;
    HSZ hTopic, hSvcA, hSvcD, hNone, hElse;
    UINT ret;
    int n, i;
    DWORD err;

    for (i = 0; i < 2; i++) start_server(i);
    start_server(3);
    ret = DdeInitializeA(&client_inst, client_cb, APPCLASS_STANDARD | CBF_SKIP_REGISTRATIONS | CBF_SKIP_UNREGISTRATIONS, 0);
    check(ret == DMLERR_NO_ERROR, "client initialized");
    hTopic = DdeCreateStringHandleA(client_inst, "Topic", CP_WINANSI);
    hElse = DdeCreateStringHandleA(client_inst, "Elsewhere", CP_WINANSI);
    hSvcA = DdeCreateStringHandleA(client_inst, "SgSvcA", CP_WINANSI);
    hSvcD = DdeCreateStringHandleA(client_inst, "SgSvcD", CP_WINANSI);
    hNone = DdeCreateStringHandleA(client_inst, "SgNoSuchService", CP_WINANSI);

    /* every server of a topic */
    list1 = DdeConnectList(client_inst, 0, hTopic, 0, NULL);
    check(list1 != 0, "all services of topic: list %p (error %u)", list1, DdeGetLastError(client_inst));
    n = list1 ? list_services(list1, names, sizeof(names), handles, 8) : 0;
    check(n == 2 && !strcmp(names, "SgSvcA,SgSvcB") || (n == 2 && !strcmp(names, "SgSvcB,SgSvcA")), "two conversations: %d (%s)", n, names);
    if (n == 2)
    {
        check(DdeQueryNextServer(list1, 0) == handles[0], "from the start: the first");
        check(DdeQueryNextServer(list1, handles[0]) == handles[1], "after the first: the second");
        check(DdeQueryNextServer(list1, handles[1]) == 0, "after the last: none");
        check(DdeQueryNextServer(list1, bogus) == 0, "after a handle that is not in the list: none");
    }
    check(DdeQueryNextServer((HCONVLIST)bogus, 0) == 0, "not a list: none");

    /* one service */
    list2 = DdeConnectList(client_inst, hSvcA, hTopic, 0, NULL);
    n = list2 ? list_services(list2, names, sizeof(names), handles2, 8) : 0;
    check(list2 != 0 && n == 1 && !strcmp(names, "SgSvcA"), "one service: %d (%s)", n, names);
    check(DdeDisconnectList(list2), "disconnect that list");

    /* a topic only one server has, wildcard topic */
    list2 = DdeConnectList(client_inst, hSvcD, 0, 0, NULL);
    n = list2 ? list_services(list2, names, sizeof(names), handles2, 8) : 0;
    check(list2 != 0 && n == 1 && !strcmp(names, "SgSvcD"), "any topic of one service: %d (%s)", n, names);
    if (n == 1)
    {
        CONVINFO ci = { sizeof(ci) };
        char topic[64];
        DdeQueryConvInfo(handles2[0], QID_SYNC, &ci);
        query(client_inst, ci.hszTopic, topic, sizeof(topic));
        check(!strcmp(topic, "Elsewhere"), "its topic is %s", topic);
    }
    DdeDisconnectList(list2);

    /* nothing there */
    list2 = DdeConnectList(client_inst, hNone, hTopic, 0, NULL);
    err = DdeGetLastError(client_inst);
    check(list2 == 0 && err == DMLERR_NO_CONV_ESTABLISHED, "no such service: %p, error %lu", list2, err);
    list2 = DdeConnectList(0xdeadbeef, 0, hTopic, 0, NULL);
    check(list2 == 0, "bad instance");
    list2 = DdeConnectList(client_inst, 0, hTopic, (HCONVLIST)bogus, NULL);
    check(list2 == 0 && DdeGetLastError(client_inst) == DMLERR_INVALIDPARAMETER, "bad list handle");

    /* refresh once a third server is there */
    start_server(2);
    list3 = DdeConnectList(client_inst, 0, hTopic, list1, NULL);
    n = list3 ? list_services(list3, names, sizeof(names), handles2, 8) : 0;
    check(list3 != 0 && n == 3, "refreshed list holds %d (%s)", n, names);
    check(strstr(names, "SgSvcA") && strstr(names, "SgSvcB") && strstr(names, "SgSvcC"), "A, B and C are there: %s", names);
    check(handles2[0] == handles[0] && handles2[1] == handles[1], "the old conversations kept their handles");
    check(DdeQueryNextServer(list1, 0) == 0, "the old list handle is gone");

    /* disconnect */
    pump();
    for (i = 0; i < NSERVERS; i++) server_disconnects[i] = 0;
    client_disconnects = 0;
    check(DdeDisconnectList(list3), "DdeDisconnectList");
    pump();
    check(server_disconnects[0] == 1 && server_disconnects[1] == 1 && server_disconnects[2] == 1 && server_disconnects[3] == 0,
          "servers A,B,C saw their conversation end: %d %d %d %d", server_disconnects[0], server_disconnects[1], server_disconnects[2], server_disconnects[3]);
    {
        CONVINFO ci = { sizeof(ci) };
        check(!DdeQueryConvInfo(handles2[0], QID_SYNC, &ci), "a conversation of the list is closed");
    }
    check(DdeQueryNextServer(list3, 0) == 0, "the disconnected list is gone");
    check(!DdeDisconnectList(list3), "a second DdeDisconnectList fails");
    check(!DdeDisconnectList(0), "DdeDisconnectList(0) fails");

    /* uninitialize closes lists that are still open */
    list2 = DdeConnectList(client_inst, 0, hTopic, 0, NULL);
    check(list2 != 0, "a list to leave open");
    check(DdeUninitialize(client_inst), "client uninitialized with a list open");
    for (i = 0; i < NSERVERS; i++) DdeUninitialize(server_inst[i]);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
