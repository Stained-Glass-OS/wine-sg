/* rpcifsec gate probe (wine-sg 7728): RPC interface security.
 * - a security callback (RpcServerRegisterIf2) is called for each call and
 *   can refuse it (the client gets RPC_S_ACCESS_DENIED, the manager never runs);
 * - RPC_IF_ALLOW_LOCAL_ONLY turns away ncacn_ip_tcp;
 * - RpcServerInqCallAttributesW answers inside a call (protocol, caller PID,
 *   locality, opnum, interface, the caller's account, buffer sizing);
 * - a procedure number past the table is a fault, not a call.
 * One process is server and client; the interface is driven with raw
 * RPC_MESSAGEs (no MIDL stubs). */
#define _WIN32_WINNT 0x0A00
#include <windows.h>
#include <rpc.h>
#include <rpcasync.h>
#include <rpcndr.h>
#include <stdio.h>
#include <string.h>

/* not in every SDK's rpcasync.h */
#ifndef RPC_QUERY_CLIENT_PID
#define RPC_QUERY_CLIENT_PID 0x10
#endif
#ifndef RPC_QUERY_IS_CLIENT_LOCAL
#define RPC_QUERY_IS_CLIENT_LOCAL 0x20
#endif
#ifndef RPC_PROTSEQ_LRPC
#define RPC_PROTSEQ_LRPC 0x3
#endif

static int failures;
static void check(int ok, const char *what, long v)
{
    printf("%s %s (%ld)\n", ok ? "ok  " : "FAIL", what, v);
    if (!ok) failures++;
}

static const GUID if_guid = {0x7c1c0e1a, 0x5b2d, 0x4a8e, {0x9f, 0x10, 0x3a, 0x51, 0x6e, 0x22, 0x91, 0x07}};
static const GUID if2_guid = {0x7c1c0e1b, 0x5b2d, 0x4a8e, {0x9f, 0x10, 0x3a, 0x51, 0x6e, 0x22, 0x91, 0x07}};
#define NDR_SYNTAX {{0x8a885d04, 0x1ceb, 0x11c9, {0x9f, 0xe8, 0x08, 0x00, 0x2b, 0x10, 0x48, 0x60}}, {2, 0}}

static LONG callback_calls, manager_calls;
static int callback_refuse_opnum = 1;

/* manager results, read by the client side after the call */
static RPC_STATUS inq_status, inq_short_status;
static ULONG inq_short_len;
static RPC_CALL_ATTRIBUTES_V2_W seen;
static WCHAR seen_client[256];

static void reply(PRPC_MESSAGE msg, ULONG value)
{
    msg->BufferLength = sizeof(ULONG);
    if (I_RpcGetBuffer(msg) == RPC_S_OK) *(ULONG *)msg->Buffer = value;
}

static void __RPC_STUB proc0(PRPC_MESSAGE msg)
{
    RPC_CALL_ATTRIBUTES_V2_W attr;

    InterlockedIncrement(&manager_calls);
    memset(&attr, 0, sizeof(attr));
    attr.Version = 2;
    attr.Flags = RPC_QUERY_CLIENT_PRINCIPAL_NAME | RPC_QUERY_CLIENT_PID | RPC_QUERY_IS_CLIENT_LOCAL;
    inq_short_status = RpcServerInqCallAttributesW(NULL, &attr);   /* no name buffer: the size */
    inq_short_len = attr.ClientPrincipalNameBufferLength;
    attr.ClientPrincipalName = (unsigned short *)seen_client;
    attr.ClientPrincipalNameBufferLength = sizeof(seen_client);
    inq_status = RpcServerInqCallAttributesW(NULL, &attr);
    seen = attr;
    reply(msg, 0x5a5a);
}

static void __RPC_STUB proc1(PRPC_MESSAGE msg)
{
    InterlockedIncrement(&manager_calls);
    reply(msg, 0x1111);
}

static RPC_DISPATCH_FUNCTION table[] = { proc0, proc1 };
static RPC_DISPATCH_TABLE dispatch = { 2, table, 0 };

static RPC_SERVER_INTERFACE server_if = { sizeof(RPC_SERVER_INTERFACE), {if_guid, {1, 0}}, NDR_SYNTAX, &dispatch, 0, 0, 0, 0, 0 };
static RPC_CLIENT_INTERFACE client_if = { sizeof(RPC_CLIENT_INTERFACE), {if_guid, {1, 0}}, NDR_SYNTAX, 0, 0, 0, 0, 0, 0 };
static RPC_SERVER_INTERFACE server_if2 = { sizeof(RPC_SERVER_INTERFACE), {if2_guid, {1, 0}}, NDR_SYNTAX, &dispatch, 0, 0, 0, 0, 0 };
static RPC_CLIENT_INTERFACE client_if2 = { sizeof(RPC_CLIENT_INTERFACE), {if2_guid, {1, 0}}, NDR_SYNTAX, 0, 0, 0, 0, 0, 0 };

static RPC_STATUS CALLBACK security_callback(RPC_IF_HANDLE ifh, void *ctx)
{
    RPC_CALL_ATTRIBUTES_V2_W attr;

    InterlockedIncrement(&callback_calls);
    memset(&attr, 0, sizeof(attr));
    attr.Version = 2;
    if (RpcServerInqCallAttributesW(ctx, &attr) != RPC_S_OK) return ERROR_ACCESS_DENIED;
    return attr.OpNum == callback_refuse_opnum ? ERROR_ACCESS_DENIED : RPC_S_OK;
}

/* one raw call: 4 bytes in, 4 bytes out; the status, the exception or the reply */
static RPC_STATUS call(RPC_BINDING_HANDLE b, RPC_CLIENT_INTERFACE *cif, unsigned int opnum, ULONG *out)
{
    RPC_MESSAGE msg;
    RPC_STATUS st = RPC_S_OK;

    memset(&msg, 0, sizeof(msg));
    msg.Handle = b;
    msg.ProcNum = opnum;
    msg.RpcInterfaceInformation = cif;
    msg.BufferLength = sizeof(ULONG);
    if ((st = I_RpcGetBuffer(&msg))) return st;
    *(ULONG *)msg.Buffer = 7;
    st = I_RpcSendReceive(&msg);
    if (!st)
    {
        if (msg.BufferLength >= sizeof(ULONG) && out) *out = *(ULONG *)msg.Buffer;
        I_RpcFreeBuffer(&msg);
    }
    return st;
}

static RPC_BINDING_HANDLE bind_to(const char *protseq, const char *endpoint)
{
    RPC_CSTR str;
    RPC_BINDING_HANDLE b = NULL;
    RpcStringBindingComposeA(NULL, (RPC_CSTR)protseq, (RPC_CSTR)(strcmp(protseq, "ncalrpc") ? "127.0.0.1" : NULL),
                             (RPC_CSTR)endpoint, NULL, &str);
    RpcBindingFromStringBindingA(str, &b);
    RpcStringFreeA(&str);
    return b;
}

int main(void)
{
    RPC_BINDING_HANDLE lrpc, tcp;
    RPC_STATUS st;
    ULONG out = 0;
    LONG before;
    RPC_CALL_ATTRIBUTES_V2_W attr;
    WCHAR user[256];
    DWORD ulen = ARRAYSIZE(user);

    st = RpcServerUseProtseqEpA((RPC_CSTR)"ncalrpc", 20, (RPC_CSTR)"sgrpcifsec", NULL);
    check(!st, "ncalrpc endpoint", st);
    st = RpcServerUseProtseqEpA((RPC_CSTR)"ncacn_ip_tcp", 20, (RPC_CSTR)"49731", NULL);
    check(!st, "ncacn_ip_tcp endpoint", st);
    st = RpcServerRegisterIf2(&server_if, NULL, NULL, 0, RPC_C_LISTEN_MAX_CALLS_DEFAULT, (UINT)-1, security_callback);
    check(!st, "interface with a security callback", st);
    st = RpcServerRegisterIf2(&server_if2, NULL, NULL, RPC_IF_ALLOW_LOCAL_ONLY, RPC_C_LISTEN_MAX_CALLS_DEFAULT, (UINT)-1, NULL);
    check(!st, "local-only interface", st);
    st = RpcServerListen(1, 20, TRUE);
    check(!st, "listening", st);

    memset(&attr, 0, sizeof(attr));
    attr.Version = 2;
    st = RpcServerInqCallAttributesW(NULL, &attr);
    check(st == RPC_S_NO_CALL_ACTIVE, "RpcServerInqCallAttributes outside a call: RPC_S_NO_CALL_ACTIVE", st);

    lrpc = bind_to("ncalrpc", "sgrpcifsec");
    tcp = bind_to("ncacn_ip_tcp", "49731");

    /* the callback is asked and lets opnum 0 through */
    out = 0;
    st = call(lrpc, &client_if, 0, &out);
    check(!st && out == 0x5a5a, "a call the callback allows reaches the manager", st);
    check(callback_calls == 1, "the security callback was called for it", callback_calls);

    /* call attributes, read inside that call */
    check(inq_short_status == ERROR_MORE_DATA && inq_short_len > sizeof(WCHAR), "no name buffer: ERROR_MORE_DATA and the size", inq_short_status);
    check(inq_status == RPC_S_OK, "RpcServerInqCallAttributesW in a call", inq_status);
    check(seen.ProtocolSequence == RPC_PROTSEQ_LRPC, "ProtocolSequence is LRPC", seen.ProtocolSequence);
    check(HandleToULong(seen.ClientPID) == GetCurrentProcessId(), "ClientPID is the caller", HandleToULong(seen.ClientPID));
    check(seen.IsClientLocal == rcclLocal, "the caller is local", seen.IsClientLocal);
    check(seen.OpNum == 0, "OpNum", seen.OpNum);
    check(IsEqualGUID(&seen.InterfaceUuid, &if_guid), "InterfaceUuid", 0);
    check(seen.AuthenticationLevel == RPC_C_AUTHN_LEVEL_PKT_PRIVACY, "ncalrpc is PKT_PRIVACY", seen.AuthenticationLevel);
    GetUserNameW(user, &ulen);
    check(wcsstr(seen_client, L"\\") && !_wcsicmp(wcsrchr(seen_client, L'\\') + 1, user),
          "the client principal is DOMAIN\\<this user>", (long)wcslen(seen_client));

    /* the callback refuses opnum 1: access denied, the manager never runs */
    before = manager_calls;
    st = call(lrpc, &client_if, 1, &out);  /* I_RpcSendReceive returns the fault */
    check(st == RPC_S_ACCESS_DENIED, "a call the callback refuses fails with access denied", st);
    check(manager_calls == before, "the refused call never reached its manager", manager_calls - before);

    /* local-only: ncalrpc yes, TCP no */
    st = call(lrpc, &client_if2, 1, &out);
    check(!st && out == 0x1111, "a local-only interface answers over ncalrpc", st);
    before = manager_calls;
    st = call(tcp, &client_if2, 1, &out);  /* I_RpcSendReceive returns the fault */
    check(st == RPC_S_ACCESS_DENIED && manager_calls == before, "a local-only interface turns away ncacn_ip_tcp", st);

    /* a procedure past the table is a fault; the server lives on */
    st = call(lrpc, &client_if2, 9, &out);  /* I_RpcSendReceive returns the fault */
    check(st == RPC_S_PROCNUM_OUT_OF_RANGE, "a procedure number past the table: RPC_S_PROCNUM_OUT_OF_RANGE", st);
    out = 0;
    st = call(lrpc, &client_if2, 1, &out);
    check(!st && out == 0x1111, "the server still answers afterwards", st);

    RpcBindingFree(&lrpc);
    RpcBindingFree(&tcp);
    RpcMgmtStopServerListening(NULL);
    RpcServerUnregisterIf(NULL, NULL, FALSE);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
