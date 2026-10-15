/* RpcServerUnregisterIfEx: the interface is removed after the calls on it complete, and the context handles
 * its calls made are run down (their rundown routines called) when RundownContextHandles is non-zero, simply
 * forgotten when it is zero.  It used to do nothing and say success.  A real client and server over ncalrpc in
 * this process, stubs made by widl from rpcrt4-unregifex.idl. */
#include <windows.h>
#include <rpc.h>
#include <stdio.h>
#include <stdlib.h>
#include "rpcrt4-unregifex.h"

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

static LONG rundown_calls, slow_done;

ctx_t __cdecl s_Open(void) { return calloc(1, sizeof(LONG)); }
void __cdecl s_Use(ctx_t c, LONG *n) { *n = ++*(LONG *)c; }
void __cdecl s_Slow(LONG ms) { Sleep(ms); InterlockedIncrement(&slow_done); }
void __cdecl s_Close(ctx_t *c) { free(*c); *c = NULL; }
void __RPC_USER ctx_t_rundown(ctx_t c) { InterlockedIncrement(&rundown_calls); free(c); }

void *__RPC_USER MIDL_user_allocate(size_t size) { return malloc(size); }
void __RPC_USER MIDL_user_free(void *p) { free(p); }

static RPC_BINDING_HANDLE connect_client(void)
{
    RPC_BINDING_HANDLE h = NULL;
    RPC_CSTR sb = NULL;
    RpcStringBindingComposeA(NULL, (RPC_CSTR)"ncalrpc", NULL, (RPC_CSTR)"sg_unregifex_ep", NULL, &sb);
    RpcBindingFromStringBindingA(sb, &h);
    RpcStringFreeA(&sb);
    return h;
}

static DWORD WINAPI slow_thread(void *arg)
{
    Slow(1200);
    return 0;
}

static void register_if(void)
{
    RPC_STATUS status = RpcServerRegisterIfEx(s_CtxTest_v1_0_s_ifspec, NULL, NULL,
                                              RPC_IF_AUTOLISTEN, RPC_C_LISTEN_MAX_CALLS_DEFAULT, NULL);
    check(status == RPC_S_OK, "register the interface");
}

int main(void)
{
    RPC_STATUS status;
    ctx_t ctx;
    LONG n;
    HANDLE thread;
    DWORD start, took;

    status = RpcServerUseProtseqEpA((RPC_CSTR)"ncalrpc", RPC_C_PROTSEQ_MAX_REQS_DEFAULT, (RPC_CSTR)"sg_unregifex_ep", NULL);
    check(status == RPC_S_OK, "RpcServerUseProtseqEp");

    /* 1. rundown asked for: the handle's rundown routine runs, the server's part is gone */
    register_if();
    ctx_bind = connect_client();
    ctx = Open(); n = 0; Use(ctx, &n);
    check(ctx && n == 1, "a context handle is made and used");
    rundown_calls = 0;
    status = RpcServerUnregisterIfEx(s_CtxTest_v1_0_s_ifspec, NULL, TRUE);
    check(status == RPC_S_OK, "RpcServerUnregisterIfEx(rundown)");
    check(rundown_calls == 1, "the rundown routine was called for the handle");
    RpcBindingFree(&ctx_bind);
    Sleep(300);
    check(rundown_calls == 1, "and only once, closing the connection adds nothing");

    /* 2. no rundown: the handle is forgotten and the routine never runs */
    register_if();
    ctx_bind = connect_client();
    ctx = Open(); n = 0; Use(ctx, &n);
    check(ctx && n == 1, "another context handle");
    rundown_calls = 0;
    status = RpcServerUnregisterIfEx(s_CtxTest_v1_0_s_ifspec, NULL, FALSE);
    check(status == RPC_S_OK, "RpcServerUnregisterIfEx(no rundown)");
    check(rundown_calls == 0, "no rundown routine is called");
    RpcBindingFree(&ctx_bind);
    Sleep(300);
    check(rundown_calls == 0, "not even when the connection closes");

    /* 3. the plain RpcServerUnregisterIf leaves the handle to the connection: rundown at its end */
    register_if();
    ctx_bind = connect_client();
    ctx = Open();
    rundown_calls = 0;
    status = RpcServerUnregisterIf(s_CtxTest_v1_0_s_ifspec, NULL, TRUE);
    check(status == RPC_S_OK, "RpcServerUnregisterIf");
    check(rundown_calls == 0, "RpcServerUnregisterIf calls no rundown");
    RpcBindingFree(&ctx_bind);
    for (n = 0; n < 30 && rundown_calls != 1; n++) Sleep(100);
    printf("      (rundown %s after the binding was freed: %ld calls)\n", rundown_calls ? "came" : "did not come", rundown_calls);

    /* 4. a call in progress is waited for */
    register_if();
    ctx_bind = connect_client();
    slow_done = 0;
    thread = CreateThread(NULL, 0, slow_thread, NULL, 0, NULL);
    Sleep(300);  /* the call is in the server now */
    start = GetTickCount();
    status = RpcServerUnregisterIfEx(s_CtxTest_v1_0_s_ifspec, NULL, TRUE);
    took = GetTickCount() - start;
    check(status == RPC_S_OK, "RpcServerUnregisterIfEx with a call in progress");
    printf("      waited %lu ms\n", took);
    check(slow_done == 1 && took >= 500, "it waited for the call to complete");
    WaitForSingleObject(thread, 5000);
    CloseHandle(thread);
    RpcBindingFree(&ctx_bind);

    /* 5. an interface that is not registered */
    status = RpcServerUnregisterIfEx(s_CtxTest_v1_0_s_ifspec, NULL, TRUE);
    check(status == RPC_S_UNKNOWN_IF, "an unregistered interface is RPC_S_UNKNOWN_IF");

    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
