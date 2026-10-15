/* RpcMgmtInqIfIds with a NULL binding (this application's interfaces) and RpcIfIdVectorFree: the vector lists the
 * interfaces registered by RpcServerRegisterIf*, a server that is not listening fails with RPC_S_NOT_LISTENING,
 * a server binding is the wrong kind.  RpcMgmtInqIfIds answered RPC_S_INVALID_BINDING always and RpcIfIdVectorFree
 * was an unimplemented export. */
#include <windows.h>
#include <rpc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "rpcrt4-unregifex.h"

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

ctx_t __cdecl s_Open(void) { return NULL; }
void __cdecl s_Use(ctx_t c, LONG *n) { *n = 0; }
void __cdecl s_Slow(LONG ms) { }
void __cdecl s_Close(ctx_t *c) { }
void __RPC_USER ctx_t_rundown(ctx_t c) { }
void *__RPC_USER MIDL_user_allocate(size_t size) { return malloc(size); }
void __RPC_USER MIDL_user_free(void *p) { free(p); }

/* a second interface that is only registered, never called */
static const RPC_SERVER_INTERFACE second_if =
{
    sizeof(RPC_SERVER_INTERFACE),
    {{0x5ee0e9e2, 0x8002, 0x4c02, {1, 2, 3, 4, 5, 6, 7, 8}}, {3, 4}},
    {{0x8a885d04, 0x1ceb, 0x11c9, {0x9f, 0xe8, 0x08, 0x00, 0x2b, 0x10, 0x48, 0x60}}, {2, 0}},
    NULL, 0, NULL, NULL, NULL, 0
};

static int has_id(RPC_IF_ID_VECTOR *v, const UUID *uuid, unsigned short major, unsigned short minor)
{
    ULONG i;
    for (i = 0; i < v->Count; i++)
        if (IsEqualGUID(&v->IfId[i]->Uuid, uuid) && v->IfId[i]->VersMajor == major && v->IfId[i]->VersMinor == minor)
            return 1;
    return 0;
}

int main(void)
{
    RPC_IF_ID_VECTOR *vector;
    RPC_STATUS status;
    RPC_BINDING_HANDLE server_binding, client;
    RPC_CSTR sb;
    RPC_BINDING_VECTOR *bv;
    char text[160];

    status = RpcServerUseProtseqEpA((RPC_CSTR)"ncalrpc", RPC_C_PROTSEQ_MAX_REQS_DEFAULT, (RPC_CSTR)"sg_ifids_ep", NULL);
    check(status == RPC_S_OK, "RpcServerUseProtseqEp");

    /* not listening yet */
    vector = (RPC_IF_ID_VECTOR *)0xdeadbeef;
    status = RpcMgmtInqIfIds(NULL, &vector);
    sprintf(text, "not listening: RPC_S_NOT_LISTENING (status %ld)", status);
    check(status == RPC_S_NOT_LISTENING && !vector, text);

    status = RpcServerRegisterIfEx(s_CtxTest_v1_0_s_ifspec, NULL, NULL, RPC_IF_AUTOLISTEN, RPC_C_LISTEN_MAX_CALLS_DEFAULT, NULL);
    check(status == RPC_S_OK, "register the first interface (autolisten)");
    status = RpcServerRegisterIf((RPC_IF_HANDLE)&second_if, NULL, NULL);
    check(status == RPC_S_OK, "register the second interface");

    vector = NULL;
    status = RpcMgmtInqIfIds(NULL, &vector);
    sprintf(text, "listening: RPC_S_OK, %lu interfaces (status %ld)", vector ? vector->Count : 0, status);
    check(status == RPC_S_OK && vector && vector->Count == 2, text);
    if (vector)
    {
        UUID first = {0x5ee0e9e1, 0x8001, 0x4c01, {0x90, 0x01, 0x01, 0x23, 0x45, 0x67, 0x89, 0xab}};
        UUID second = {0x5ee0e9e2, 0x8002, 0x4c02, {1, 2, 3, 4, 5, 6, 7, 8}};
        check(has_id(vector, &first, 1, 0), "the first interface with its version 1.0");
        check(has_id(vector, &second, 3, 4), "the second interface with its version 3.4");
        status = RpcIfIdVectorFree(&vector);
        check(status == RPC_S_OK && !vector, "RpcIfIdVectorFree frees it and clears the pointer");
        status = RpcIfIdVectorFree(&vector);
        check(status == RPC_S_OK, "freeing a NULL vector is fine");
    }

    /* unregistering shows */
    RpcServerUnregisterIf((RPC_IF_HANDLE)&second_if, NULL, TRUE);
    vector = NULL;
    status = RpcMgmtInqIfIds(NULL, &vector);
    check(status == RPC_S_OK && vector && vector->Count == 1, "after unregistering one, one is left");
    if (vector) RpcIfIdVectorFree(&vector);

    /* bindings */
    status = RpcServerInqBindings(&bv);
    if (status == RPC_S_OK && bv->Count)
    {
        server_binding = bv->BindingH[0];
        vector = (RPC_IF_ID_VECTOR *)0xdeadbeef;
        status = RpcMgmtInqIfIds(server_binding, &vector);
        check(status == RPC_S_WRONG_KIND_OF_BINDING && !vector, "a server binding is the wrong kind");
        RpcBindingVectorFree(&bv);
    }
    else check(0, "RpcServerInqBindings");
    check(RpcMgmtInqIfIds(NULL, NULL) == ERROR_INVALID_PARAMETER, "a NULL result pointer is an invalid argument");
    check(RpcIfIdVectorFree(NULL) == ERROR_INVALID_PARAMETER, "RpcIfIdVectorFree(NULL) is an invalid argument");

    RpcServerUnregisterIf(s_CtxTest_v1_0_s_ifspec, NULL, TRUE);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
