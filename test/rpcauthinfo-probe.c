/* rpcauthinfo gate probe (wine-sg 7742): the rest of RPC's security calls.
 * - the authorization service and quality of service set with RpcBindingSetAuthInfoEx come back from
 *   RpcBindingInqAuthInfoEx (Wine answered "not implemented": RPC_C_AUTHZ_NONE, and refused QOS);
 * - RpcGetAuthorizationContextForClient gives a server an Authz context of its caller, which
 *   RpcFreeAuthorizationContext frees (both stubs);
 * - I_RpcBindingInqSecurityContext answers (a stub): no security context for an unauthenticated call.
 * One process is server and client over ncalrpc; raw RPC_MESSAGEs. */
#define _WIN32_WINNT 0x0A00
#include <windows.h>
#include <rpc.h>
#include <rpcasync.h>
#include <rpcndr.h>
#include <authz.h>
#include <stdio.h>
#include <string.h>

__declspec(dllimport) RPC_STATUS WINAPI I_RpcBindingInqSecurityContext(RPC_BINDING_HANDLE, void **);

static int failures;
static void check(int ok, const char *what, long v)
{
    printf("%s %s (%ld)\n", ok ? "ok  " : "FAIL", what, v);
    if (!ok) failures++;
}

static const GUID if_guid = {0x7c1c0e1c, 0x5b2d, 0x4a8e, {0x9f, 0x10, 0x3a, 0x51, 0x6e, 0x22, 0x91, 0x07}};
#define NDR_SYNTAX {{0x8a885d04, 0x1ceb, 0x11c9, {0x9f, 0xe8, 0x08, 0x00, 0x2b, 0x10, 0x48, 0x60}}, {2, 0}}

static RPC_STATUS authz_status = 12345, free_status = 12345, ctx_status = 12345, ctx_null_status = 12345;
static BOOL authz_user_ok, authz_freed;
static void *ctx_handle = (void *)1;

static void reply(PRPC_MESSAGE msg, ULONG value)
{
    msg->BufferLength = sizeof(ULONG);
    if (I_RpcGetBuffer(msg) == RPC_S_OK) *(ULONG *)msg->Buffer = value;
}

static void __RPC_STUB proc0(PRPC_MESSAGE msg)
{
    LUID none = {0, 0};
    void *authz = NULL;

    authz_status = RpcGetAuthorizationContextForClient(msg->Handle, FALSE, NULL, NULL, none, 0, NULL, &authz);
    if (!authz_status && authz)
    {
        BYTE buf[256], mine[256];
        DWORD size = 0, len = 0;
        HANDLE token;

        if (AuthzGetInformationFromContext(authz, AuthzContextInfoUserSid, sizeof(buf), &size, buf) &&
            OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token))
        {
            if (GetTokenInformation(token, TokenUser, mine, sizeof(mine), &len))
                authz_user_ok = EqualSid(((TOKEN_USER *)buf)->User.Sid, ((TOKEN_USER *)mine)->User.Sid);
            CloseHandle(token);
        }
        free_status = RpcFreeAuthorizationContext(&authz);
        authz_freed = authz == NULL;
    }
    ctx_status = I_RpcBindingInqSecurityContext(msg->Handle, &ctx_handle);
    ctx_null_status = I_RpcBindingInqSecurityContext(msg->Handle, NULL);
    reply(msg, 0x5a5a);
}

static RPC_DISPATCH_FUNCTION table[] = { proc0 };
static RPC_DISPATCH_TABLE dispatch = { 1, table, 0 };
static RPC_SERVER_INTERFACE server_if = { sizeof(RPC_SERVER_INTERFACE), {if_guid, {1, 0}}, NDR_SYNTAX, &dispatch, 0, 0, 0, 0, 0 };
static RPC_CLIENT_INTERFACE client_if = { sizeof(RPC_CLIENT_INTERFACE), {if_guid, {1, 0}}, NDR_SYNTAX, 0, 0, 0, 0, 0, 0 };

static RPC_STATUS call(RPC_BINDING_HANDLE b, ULONG *out)
{
    RPC_MESSAGE msg;
    RPC_STATUS st;

    memset(&msg, 0, sizeof(msg));
    msg.Handle = b;
    msg.RpcInterfaceInformation = &client_if;
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

int main(void)
{
    static const ULONG services[] = { RPC_C_AUTHN_WINNT, RPC_C_AUTHN_GSS_NEGOTIATE, RPC_C_AUTHN_GSS_KERBEROS, RPC_C_AUTHN_GSS_SCHANNEL };
    RPC_BINDING_HANDLE b = NULL, b2 = NULL;
    RPC_SECURITY_QOS qos, got;
    RPC_SECURITY_QOS_V2_W got2;
    RPC_CSTR str;
    RPC_STATUS st;
    ULONG out = 0, level = 0, svc = 0, authz = 99;
    unsigned int i;
    LUID none = {0, 0};
    void *ctx = (void *)1;

    st = RpcServerUseProtseqEpA((RPC_CSTR)"ncalrpc", 20, (RPC_CSTR)"sgrpcauthinfo", NULL);
    check(!st, "ncalrpc endpoint", st);
    st = RpcServerRegisterIf(&server_if, NULL, NULL);
    check(!st, "interface", st);
    st = RpcServerListen(1, 20, TRUE);
    check(!st, "listening", st);

    RpcStringBindingComposeA(NULL, (RPC_CSTR)"ncalrpc", NULL, (RPC_CSTR)"sgrpcauthinfo", NULL, &str);
    RpcBindingFromStringBindingA(str, &b);
    RpcBindingFromStringBindingA(str, &b2);
    RpcStringFreeA(&str);

    /* the authorization service and the QOS the client set come back */
    memset(&qos, 0, sizeof(qos));
    qos.Version = RPC_C_SECURITY_QOS_VERSION_1;
    qos.Capabilities = RPC_C_QOS_CAPABILITIES_DEFAULT;
    qos.IdentityTracking = RPC_C_QOS_IDENTITY_DYNAMIC;
    qos.ImpersonationType = RPC_C_IMP_LEVEL_IDENTIFY;
    st = RPC_S_UNKNOWN_AUTHN_SERVICE;
    for (i = 0; i < ARRAYSIZE(services) && st; i++)
        st = RpcBindingSetAuthInfoExW(b, NULL, RPC_C_AUTHN_LEVEL_PKT_PRIVACY, services[i], NULL, RPC_C_AUTHZ_NAME, &qos);
    if (st)
        printf("skip no authentication service can be set here (%ld): authorization service and QOS not checked\n", st);
    else
    {
        printf("info authentication service %lu\n", services[i - 1]);
        memset(&got, 0xcc, sizeof(got));
        st = RpcBindingInqAuthInfoExW(b, NULL, &level, &svc, NULL, &authz, RPC_C_SECURITY_QOS_VERSION_1, &got);
        check(!st, "RpcBindingInqAuthInfoExW with a QOS", st);
        check(level == RPC_C_AUTHN_LEVEL_PKT_PRIVACY && svc == services[i - 1], "level and service", level);
        check(authz == RPC_C_AUTHZ_NAME, "the authorization service is the one set (RPC_C_AUTHZ_NAME)", authz);
        check(got.Version == 1 && got.IdentityTracking == RPC_C_QOS_IDENTITY_DYNAMIC &&
              got.ImpersonationType == RPC_C_IMP_LEVEL_IDENTIFY && got.Capabilities == RPC_C_QOS_CAPABILITIES_DEFAULT,
              "the QOS is the one set", got.ImpersonationType);
        memset(&got2, 0xcc, sizeof(got2));
        st = RpcBindingInqAuthInfoExW(b, NULL, NULL, NULL, NULL, NULL, RPC_C_SECURITY_QOS_VERSION_2, (RPC_SECURITY_QOS *)&got2);
        check(!st && got2.Version == 2 && got2.AdditionalSecurityInfoType == 0 && !got2.u.HttpCredentials,
              "a version 2 QOS: no additional security information", st);
        st = RpcBindingInqAuthInfoExW(b, NULL, NULL, NULL, NULL, &authz, 0, &got);
        check(st == RPC_S_INVALID_ARG, "QOS version 0: RPC_S_INVALID_ARG", st);
    }
    st = RpcBindingInqAuthInfoExW(b2, NULL, &level, NULL, NULL, &authz, 0, NULL);
    check(st == RPC_S_BINDING_HAS_NO_AUTH, "a binding without authentication: RPC_S_BINDING_HAS_NO_AUTH", st);

    /* the server side: an Authz context of the caller, freed; no security context unauthenticated */
    out = 0;
    st = call(b2, &out);
    check(!st && out == 0x5a5a, "the call", st);
    check(authz_status == RPC_S_OK, "RpcGetAuthorizationContextForClient in the call", authz_status);
    check(authz_user_ok, "the context's user is the caller", authz_user_ok);
    check(free_status == RPC_S_OK && authz_freed, "RpcFreeAuthorizationContext frees and clears it", free_status);
    check(ctx_status == RPC_S_BINDING_HAS_NO_AUTH && !ctx_handle, "I_RpcBindingInqSecurityContext, unauthenticated: no context", ctx_status);
    check(ctx_null_status == RPC_S_INVALID_ARG, "I_RpcBindingInqSecurityContext without a pointer: RPC_S_INVALID_ARG", ctx_null_status);
    st = RpcGetAuthorizationContextForClient(NULL, FALSE, (void *)1, NULL, none, 0, NULL, &ctx);
    check(st == RPC_S_INVALID_ARG, "a reserved argument set: RPC_S_INVALID_ARG", st);
    st = RpcFreeAuthorizationContext(NULL);
    check(st == RPC_S_INVALID_ARG, "RpcFreeAuthorizationContext(NULL): RPC_S_INVALID_ARG", st);

    RpcBindingFree(&b);
    RpcBindingFree(&b2);
    RpcMgmtStopServerListening(NULL);
    RpcServerUnregisterIf(NULL, NULL, FALSE);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
