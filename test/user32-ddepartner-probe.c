/* Probe for patches/sg/5211: DdeQueryConvInfo on the partner handle of CONVINFO.hConvPartner
 * gives the information of the other side of the conversation: the conversation of the other
 * DDEML instance when it is in this process, the same windows seen from the other side when
 * it is another process.  Table driven; last line "RESULT: PASS|FAIL". */
#include <windows.h>
#include <ddeml.h>
#include <stdio.h>
#include <string.h>

static int failures;
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

static HCONV server_conv;
static DWORD inst_server, inst_client;

static HDDEDATA CALLBACK server_cb(UINT type, UINT fmt, HCONV hconv, HSZ hsz1, HSZ hsz2, HDDEDATA data, ULONG_PTR d1, ULONG_PTR d2)
{
    switch (type)
    {
    case XTYP_CONNECT: return (HDDEDATA)TRUE;
    case XTYP_CONNECT_CONFIRM: server_conv = hconv; return NULL;
    }
    return NULL;
}

static HDDEDATA CALLBACK client_cb(UINT type, UINT fmt, HCONV hconv, HSZ hsz1, HSZ hsz2, HDDEDATA data, ULONG_PTR d1, ULONG_PTR d2)
{
    return NULL;
}

static const WCHAR *svc_in = L"SgPartnerIn", *svc_out = L"SgPartnerOut", *topic = L"Topic";

static int server_process(const char *evname_ready, const char *evname_done)
{
    DWORD inst = 0;
    HSZ hsvc;
    HANDLE ready = OpenEventA(EVENT_MODIFY_STATE, FALSE, evname_ready), done = OpenEventA(SYNCHRONIZE, FALSE, evname_done);
    MSG msg;

    DdeInitializeW(&inst, server_cb, APPCLASS_STANDARD, 0);
    hsvc = DdeCreateStringHandleW(inst, svc_out, CP_WINUNICODE);
    DdeNameService(inst, hsvc, 0, DNS_REGISTER);
    SetEvent(ready);
    while (MsgWaitForMultipleObjects(1, &done, FALSE, 20000, QS_ALLINPUT) == WAIT_OBJECT_0 + 1)
        while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) { TranslateMessage(&msg); DispatchMessageA(&msg); }
    DdeNameService(inst, 0, 0, DNS_UNREGISTER);
    DdeUninitialize(inst);
    return 0;
}

static void test_conv(const char *what, HCONV client, BOOL partner_is_dde)
{
    CONVINFO ci = {.cb = sizeof(ci)}, pi = {.cb = sizeof(pi)};
    UINT ret;

    ret = DdeQueryConvInfo(client, QID_SYNC, &ci);
    check(ret == sizeof(ci), "%s: client info (%u)", what, ret);
    check((ci.wStatus & ST_CLIENT) && ci.hConvPartner, "%s: client side with a partner handle %p", what, ci.hConvPartner);
    if (!ci.hConvPartner) return;
    ret = DdeQueryConvInfo(ci.hConvPartner, QID_SYNC, &pi);
    check(ret == sizeof(pi), "%s: partner info (%u, error %x)", what, ret, DdeGetLastError(inst_client));
    check(!(pi.wStatus & ST_CLIENT), "%s: partner is the server side (status %#x)", what, pi.wStatus);
    check((pi.wStatus & ST_CONNECTED) != 0, "%s: partner connected (status %#x)", what, pi.wStatus);
    check(pi.hwnd == ci.hwndPartner && pi.hwndPartner == ci.hwnd, "%s: windows are swapped (%p/%p vs %p/%p)", what,
          pi.hwnd, pi.hwndPartner, ci.hwnd, ci.hwndPartner);
    check(pi.hszTopic == ci.hszTopic || pi.hszTopic != 0, "%s: topic %#lx", what, (ULONG_PTR)pi.hszTopic);
    /* no change of the client's own data */
    ret = DdeQueryConvInfo(client, QID_SYNC, &ci);
    check(ret && (ci.wStatus & ST_CLIENT), "%s: client info unchanged", what);
    (void)partner_is_dde;
}

int main(int argc, char **argv)
{
    HSZ hsvc, htopic;
    HCONV client;
    CONVINFO si = {.cb = sizeof(si)}, ci = {.cb = sizeof(ci)}, bad = {.cb = sizeof(bad)};
    UINT ret;

    setvbuf(stdout, NULL, _IONBF, 0);
    if (argc >= 4 && !strcmp(argv[1], "server")) return server_process(argv[2], argv[3]);

    /* in this process */
    DdeInitializeW(&inst_server, server_cb, APPCLASS_STANDARD, 0);
    DdeInitializeW(&inst_client, client_cb, APPCMD_CLIENTONLY, 0);
    hsvc = DdeCreateStringHandleW(inst_server, svc_in, CP_WINUNICODE);
    DdeNameService(inst_server, hsvc, 0, DNS_REGISTER);
    hsvc = DdeCreateStringHandleW(inst_client, svc_in, CP_WINUNICODE);
    htopic = DdeCreateStringHandleW(inst_client, topic, CP_WINUNICODE);
    client = DdeConnect(inst_client, hsvc, htopic, NULL);
    check(client != NULL, "connected in this process (error %x)", DdeGetLastError(inst_client));
    if (client)
    {
        test_conv("same process", client, TRUE);
        DdeQueryConvInfo(client, QID_SYNC, &ci);
        ret = DdeQueryConvInfo(ci.hConvPartner, QID_SYNC, &si);
        check(ret && server_conv, "server conversation recorded %p", server_conv);
        /* the partner's CONVINFO is that of the server conversation itself */
        {
            CONVINFO direct = {.cb = sizeof(direct)};
            DdeQueryConvInfo(server_conv, QID_SYNC, &direct);
            check(direct.wStatus == si.wStatus && direct.hwnd == si.hwnd && direct.hwndPartner == si.hwndPartner,
                  "partner info equals the server conversation's (status %#x vs %#x)", si.wStatus, direct.wStatus);
        }
        DdeDisconnect(client);
    }
    DdeNameService(inst_server, 0, 0, DNS_UNREGISTER);

    /* a handle that is not a conversation, with the partner bit */
    SetLastError(0);
    ret = DdeQueryConvInfo((HCONV)0x1235, QID_SYNC, &bad);
    check(ret == 0, "bad partner handle: %u", ret);
    ret = DdeQueryConvInfo((HCONV)1, QID_SYNC, &bad);
    check(ret == 0, "partner bit alone: %u", ret);

    /* another process */
    {
        char cmd[MAX_PATH + 64], path[MAX_PATH];
        STARTUPINFOA sti = {sizeof(sti)};
        PROCESS_INFORMATION pi;
        HANDLE ready = CreateEventA(NULL, TRUE, FALSE, "SgPartnerReady"), done = CreateEventA(NULL, TRUE, FALSE, "SgPartnerDone");

        GetModuleFileNameA(NULL, path, sizeof(path));
        snprintf(cmd, sizeof(cmd), "\"%s\" server SgPartnerReady SgPartnerDone", path);
        if (CreateProcessA(NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &sti, &pi))
        {
            check(WaitForSingleObject(ready, 15000) == WAIT_OBJECT_0, "server process ready");
            hsvc = DdeCreateStringHandleW(inst_client, svc_out, CP_WINUNICODE);
            client = DdeConnect(inst_client, hsvc, htopic, NULL);
            check(client != NULL, "connected to the other process (error %x)", DdeGetLastError(inst_client));
            if (client)
            {
                test_conv("other process", client, TRUE);
                DdeDisconnect(client);
            }
            SetEvent(done);
            WaitForSingleObject(pi.hProcess, 10000);
            CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
        }
        else check(0, "server process started (%lu)", GetLastError());
    }

    DdeUninitialize(inst_client);
    DdeUninitialize(inst_server);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
