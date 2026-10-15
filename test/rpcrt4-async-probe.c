/* RpcAsyncGetCallStatus on a real asynchronous client call: RPC_S_ASYNC_CALL_PENDING while the server works,
 * RPC_S_OK once the reply has arrived (the event of the async state is signalled), the out parameters come
 * with RpcAsyncCompleteCall.  A handle with no call is RPC_S_INVALID_BINDING (the Windows result recorded in
 * tests/rpc_async.c).  The procedures are widl's synchronous ones; the probe patches copies of their format strings
 * to take the RPC_ASYNC_STATE as the first argument (stack size and every stack offset grow by a pointer), which
 * is what an [async] procedure compiled by MIDL looks like, and calls them with NdrAsyncClientCall. */
#include <windows.h>
#include <rpc.h>
#include <rpcndr.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "rpcrt4-async_c.c"


static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

void *__RPC_USER MIDL_user_allocate(size_t size) { return malloc(size); }
void __RPC_USER MIDL_user_free(void *p) { free(p); }

void __cdecl s_Add(LONG a, LONG b, LONG *sum) { *sum = a + b; }
void __cdecl s_Slow(LONG ms) { Sleep(ms); }

/* the procedure format strings, copied and given the asynchronous handle slot */
static unsigned char async_proc[sizeof(__MIDL_ProcFormatString.Format)];

static void make_async_proc(void)
{
    unsigned char *p = async_proc;
    size_t proc;

    memcpy(async_proc, __MIDL_ProcFormatString.Format, sizeof(async_proc));
    for (proc = 0; proc < sizeof(async_proc); )
    {
        unsigned char ext, nparams, i;
        unsigned short stack_size;

        if (p[proc] != 0x32) break;  /* FC_BIND_PRIMITIVE: a procedure */
        stack_size = *(unsigned short *)(p + proc + 8);
        *(unsigned short *)(p + proc + 8) = stack_size + sizeof(void *);
        nparams = p[proc + 15];
        ext = p[proc + 16];
        for (i = 0; i < nparams; i++)
        {
            unsigned char *param = p + proc + 16 + ext + 6 * i;
            *(unsigned short *)(param + 2) += sizeof(void *);
        }
        proc += 16 + ext + 6 * nparams;
    }
}

/* 64-bit stubs pass the arguments themselves, 32-bit ones a pointer to the first (widl and MIDL style) */
#ifdef _WIN64
# define ASYNC_CALL(fmt, async, ...) NdrAsyncClientCall(&AsyncT_StubDesc, fmt, async, __VA_ARGS__)
#else
# define ASYNC_CALL(fmt, async, ...) do { struct { void *a; LONG v[3]; } args = { async, { __VA_ARGS__ } }; \
        NdrAsyncClientCall(&AsyncT_StubDesc, fmt, &args); } while (0)
#endif

static RPC_ASYNC_STATE *new_async(HANDLE *event)
{
    RPC_ASYNC_STATE *a = calloc(1, sizeof(*a));
    RpcAsyncInitializeHandle(a, sizeof(*a));
    *event = CreateEventA(NULL, TRUE, FALSE, NULL);
    a->NotificationType = RpcNotificationTypeEvent;
    a->u.hEvent = *event;
    return a;
}

int main(void)
{
    RPC_STATUS status;
    RPC_ASYNC_STATE *async;
    HANDLE event;
    DWORD start, took, wait;
    LONG sum = 0;
    int i, pending_seen = 0;

    make_async_proc();

    /* handles with no call behind them */
    {
        RPC_ASYNC_STATE fresh;
        RpcAsyncInitializeHandle(&fresh, sizeof(fresh));
        check(RpcAsyncGetCallStatus(&fresh) == RPC_S_INVALID_BINDING, "a fresh handle: RPC_S_INVALID_BINDING");
        memset(&fresh, 0, sizeof(fresh));
        check(RpcAsyncGetCallStatus(&fresh) == RPC_S_INVALID_BINDING, "a zeroed handle: RPC_S_INVALID_BINDING");
        check(RpcAsyncGetCallStatus(NULL) == RPC_S_INVALID_ASYNC_HANDLE, "NULL: RPC_S_INVALID_ASYNC_HANDLE");
    }

    status = RpcServerUseProtseqEpA((RPC_CSTR)"ncalrpc", RPC_C_PROTSEQ_MAX_REQS_DEFAULT, (RPC_CSTR)"sg_async_ep", NULL);
    check(status == RPC_S_OK, "RpcServerUseProtseqEp");
    status = RpcServerRegisterIfEx(s_AsyncT_v1_0_s_ifspec, NULL, NULL, RPC_IF_AUTOLISTEN, RPC_C_LISTEN_MAX_CALLS_DEFAULT, NULL);
    check(status == RPC_S_OK, "register the interface");
    {
        RPC_CSTR sb = NULL;
        RpcStringBindingComposeA(NULL, (RPC_CSTR)"ncalrpc", NULL, (RPC_CSTR)"sg_async_ep", NULL, &sb);
        RpcBindingFromStringBindingA(sb, &as_bind);
        RpcStringFreeA(&sb);
    }

    /* a call that takes 900 ms */
    async = new_async(&event);
    ASYNC_CALL(async_proc + (sizeof(void *) == 8 ? 44 : 42), async, 900);
    start = GetTickCount();
    status = RpcAsyncGetCallStatus(async);
    printf("      status right after the call: %ld\n", status);
    check(status == RPC_S_ASYNC_CALL_PENDING, "right after the call the status is RPC_S_ASYNC_CALL_PENDING");
    for (i = 0; i < 3; i++)
    {
        Sleep(100);
        if (RpcAsyncGetCallStatus(async) == RPC_S_ASYNC_CALL_PENDING) pending_seen++;
    }
    check(pending_seen == 3, "still pending 100, 200 and 300 ms later");
    wait = WaitForSingleObject(event, 5000);
    took = GetTickCount() - start;
    check(wait == WAIT_OBJECT_0 && took >= 700, "the event is signalled when the server is done");
    printf("      event after %lu ms\n", took);
    status = RpcAsyncGetCallStatus(async);
    check(status == RPC_S_OK, "after the reply arrived the status is RPC_S_OK");
    check(RpcAsyncGetCallStatus(async) == RPC_S_OK, "and stays RPC_S_OK");
    status = RpcAsyncCompleteCall(async, NULL);
    check(status == RPC_S_OK, "RpcAsyncCompleteCall");
    CloseHandle(event);
    free(async);

    /* a call with an out parameter */
    async = new_async(&event);
#ifdef _WIN64
    NdrAsyncClientCall(&AsyncT_StubDesc, async_proc, async, 20, 22, &sum);
#else
    { struct { void *a; LONG x, y; LONG *sum; } args = { async, 20, 22, &sum }; NdrAsyncClientCall(&AsyncT_StubDesc, async_proc, &args); }
#endif
    wait = WaitForSingleObject(event, 5000);
    check(wait == WAIT_OBJECT_0, "the quick call completes");
    check(RpcAsyncGetCallStatus(async) == RPC_S_OK, "its status is RPC_S_OK");
    status = RpcAsyncCompleteCall(async, NULL);
    check(status == RPC_S_OK && sum == 42, "the out parameter comes with RpcAsyncCompleteCall (20 + 22 = 42)");
    CloseHandle(event);
    free(async);

    RpcBindingFree(&as_bind);
    RpcServerUnregisterIf(s_AsyncT_v1_0_s_ifspec, NULL, TRUE);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
