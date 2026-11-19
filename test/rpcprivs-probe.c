/* rpcprivs gate probe (wine-sg 8001): RpcBindingInqAuthClient's Privs is the
 * caller's account (DOMAIN\user) -- the W function a wide string, the A
 * function an ANSI one, the same pointer on a second call (it lives with the
 * binding). Wine gave NULL ("privs not implemented"). One process is server and
 * client over ncalrpc; the interface is driven with raw RPC_MESSAGEs. */
#define _WIN32_WINNT 0x0A00
#include <windows.h>
#include <rpc.h>
#include <rpcndr.h>
#include <stdio.h>
#include <string.h>

static int failures;
static void check(int ok, const char *what, long v)
{
    printf("%s %s (%ld)\n", ok ? "ok  " : "FAIL", what, v);
    if (!ok) failures++;
}

static const GUID if_guid = {0x8d2c0e1a, 0x5b2d, 0x4a8e, {0x9f, 0x10, 0x3a, 0x51, 0x6e, 0x22, 0x91, 0x08}};
#define NDR_SYNTAX {{0x8a885d04, 0x1ceb, 0x11c9, {0x9f, 0xe8, 0x08, 0x00, 0x2b, 0x10, 0x48, 0x60}}, {2, 0}}

static RPC_STATUS w_status, a_status, w2_status;
static WCHAR w_name[256];
static char a_name[256];
static BOOL same_pointer;
static ULONG authn_svc;

static void __RPC_STUB proc0(PRPC_MESSAGE msg)
{
    RPC_AUTHZ_HANDLE privs = NULL, privs2 = NULL, privs_a = NULL;

    w_status = RpcBindingInqAuthClientW(NULL, &privs, NULL, NULL, &authn_svc, NULL);
    if (!w_status && privs) lstrcpynW(w_name, privs, ARRAYSIZE(w_name));
    w2_status = RpcBindingInqAuthClientW(msg->Handle, &privs2, NULL, NULL, NULL, NULL);
    same_pointer = privs && privs == privs2;
    a_status = RpcBindingInqAuthClientA(NULL, &privs_a, NULL, NULL, NULL, NULL);
    if (!a_status && privs_a) lstrcpynA(a_name, privs_a, sizeof(a_name));
    msg->BufferLength = sizeof(ULONG);
    if (I_RpcGetBuffer(msg) == RPC_S_OK) *(ULONG *)msg->Buffer = 0x5a5a;
}

static RPC_DISPATCH_FUNCTION table[] = { proc0 };
static RPC_DISPATCH_TABLE dispatch = { 1, table, 0 };
static RPC_SERVER_INTERFACE server_if = { sizeof(RPC_SERVER_INTERFACE), {if_guid, {1, 0}}, NDR_SYNTAX, &dispatch, 0, 0, 0, 0, 0 };
static RPC_CLIENT_INTERFACE client_if = { sizeof(RPC_CLIENT_INTERFACE), {if_guid, {1, 0}}, NDR_SYNTAX, 0, 0, 0, 0, 0, 0 };

int main(void)
{
    RPC_BINDING_HANDLE b = NULL;
    RPC_CSTR str;
    RPC_MESSAGE msg;
    RPC_STATUS st;
    WCHAR user[256], expect[512], domain[256];
    char expect_a[512];
    char sidbuf[SECURITY_MAX_SID_SIZE + sizeof(TOKEN_USER)];
    DWORD ulen = ARRAYSIZE(user), dlen = ARRAYSIZE(domain), len;
    SID_NAME_USE use;
    HANDLE token;

    OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token);
    GetTokenInformation(token, TokenUser, sidbuf, sizeof(sidbuf), &len);
    LookupAccountSidW(NULL, ((TOKEN_USER *)sidbuf)->User.Sid, user, &ulen, domain, &dlen, &use);
    swprintf(expect, ARRAYSIZE(expect), L"%ls\\%ls", domain, user);
    WideCharToMultiByte(CP_ACP, 0, expect, -1, expect_a, sizeof(expect_a), NULL, NULL);

    st = RpcServerUseProtseqEpA((RPC_CSTR)"ncalrpc", 20, (RPC_CSTR)"sgrpcprivs", NULL);
    check(!st, "ncalrpc endpoint", st);
    st = RpcServerRegisterIf2(&server_if, NULL, NULL, 0, RPC_C_LISTEN_MAX_CALLS_DEFAULT, (UINT)-1, NULL);
    check(!st, "interface", st);
    st = RpcServerListen(1, 20, TRUE);
    check(!st, "listening", st);

    RpcStringBindingComposeA(NULL, (RPC_CSTR)"ncalrpc", NULL, (RPC_CSTR)"sgrpcprivs", NULL, &str);
    RpcBindingFromStringBindingA(str, &b);
    RpcStringFreeA(&str);
    memset(&msg, 0, sizeof(msg));
    msg.Handle = b;
    msg.RpcInterfaceInformation = &client_if;
    msg.BufferLength = sizeof(ULONG);
    st = I_RpcGetBuffer(&msg);
    if (!st) { *(ULONG *)msg.Buffer = 7; st = I_RpcSendReceive(&msg); }
    check(!st && *(ULONG *)msg.Buffer == 0x5a5a, "the call", st);
    if (!st) I_RpcFreeBuffer(&msg);

    check(w_status == RPC_S_OK, "RpcBindingInqAuthClientW in the call", w_status);
    check(authn_svc == RPC_C_AUTHN_WINNT, "ncalrpc: RPC_C_AUTHN_WINNT", authn_svc);
    printf("     W Privs \"%ls\", expected \"%ls\"\n", w_name, expect);
    check(!_wcsicmp(w_name, expect), "W: Privs is DOMAIN\\<this user>", (long)wcslen(w_name));
    check(w2_status == RPC_S_OK && same_pointer, "a second call by the binding handle gives the same string", w2_status);
    check(a_status == RPC_S_OK && !_stricmp(a_name, expect_a), "A: Privs is the ANSI DOMAIN\\<this user>", a_status);

    RpcBindingFree(&b);
    RpcMgmtStopServerListening(NULL);
    RpcServerUnregisterIf(NULL, NULL, FALSE);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
