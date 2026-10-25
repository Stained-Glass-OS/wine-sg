/* rpcrt4 (6107): what Avast's Icarus installer engine (CCleaner's) needs of RPC between its two
 * processes over ncalrpc, each a widl-generated client and server here:
 *  - asynchronous calls from stubs that say NDR 6.2 (MIDL_STUB_DESC.Version 0x60002, what current
 *    MIDL writes): the client refused them with RPC_X_WRONG_STUB_VERSION (1829);
 *  - an asynchronous call's explicit handle_t parameter: the manager routine got NULL, and asking it
 *    for its string binding crashed the call (fault 0x3e6);
 *  - a context handle made over one connection of a client association and used over another:
 *    the second connection, opened while the first was busy, bound with no association group, the
 *    server made it an association of its own and did not know the handle (0x1c00001a).
 *   probe server ENDPOINT    serves until the client calls Stop
 *   probe client ENDPOINT    runs the checks, prints RESULT: PASS or FAIL */
#include <windows.h>
#include <rpc.h>
#include <rpcndr.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "rpcicarus.h"

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

void *__RPC_USER MIDL_user_allocate(size_t size) { return malloc(size); }
void __RPC_USER MIDL_user_free(void *p) { free(p); }

/* ---- server ---- */

static LONG magic = 0x1c1c;

void s_Who(PRPC_ASYNC_STATE async, handle_t h, LONG *ok)
{
    RPC_WSTR str = NULL;
    *ok = h != NULL && RpcBindingToStringBindingW(h, &str) == RPC_S_OK && str && *str;
    if (str) RpcStringFreeW(&str);
    RpcAsyncCompleteCall(async, NULL);
}

void s_Open(handle_t h, ctx_t *ctx)
{
    *ctx = &magic;
}

LONG s_Use(ctx_t ctx)
{
    return ctx == &magic ? 42 : -1;
}

struct work { RPC_ASYNC_STATE *async; LONG ms; };

static DWORD WINAPI hang_thread(void *arg)
{
    struct work *w = arg;
    Sleep(w->ms);
    RpcAsyncCompleteCall(w->async, NULL);
    free(w);
    return 0;
}

void s_Hang(PRPC_ASYNC_STATE async, handle_t h, LONG ms)
{
    struct work *w = calloc(1, sizeof(*w));
    w->async = async; w->ms = ms;
    CloseHandle(CreateThread(NULL, 0, hang_thread, w, 0, NULL));
}

void s_Close(ctx_t *ctx)
{
    *ctx = NULL;
}

void s_Stop(handle_t h)
{
    RpcMgmtStopServerListening(NULL);
}

void __RPC_USER ctx_t_rundown(ctx_t ctx)
{
}

static int server(const char *ep)
{
    RPC_STATUS status;

    status = RpcServerUseProtseqEpA((RPC_CSTR)"ncalrpc", 20, (RPC_CSTR)ep, NULL);
    if (status) { printf("protseq %lu\n", status); return 1; }
    status = RpcServerRegisterIf(s_Icr_v1_0_s_ifspec, NULL, NULL);
    if (status) { printf("registerif %lu\n", status); return 1; }
    status = RpcServerListen(1, 20, TRUE);
    if (status) { printf("listen %lu\n", status); return 1; }
    printf("READY\n");
    fflush(stdout);
    RpcMgmtWaitServerListen();
    return 0;
}

/* ---- client ---- */

static handle_t bind_to(const char *ep)
{
    RPC_CSTR str;
    handle_t h = NULL;
    RpcStringBindingComposeA(NULL, (RPC_CSTR)"ncalrpc", NULL, (RPC_CSTR)ep, NULL, &str);
    RpcBindingFromStringBindingA(str, &h);
    RpcStringFreeA(&str);
    return h;
}

static RPC_STATUS wait_async(RPC_ASYNC_STATE *async, void *reply)
{
    WaitForSingleObject(async->u.hEvent, 10000);
    return RpcAsyncCompleteCall(async, reply);
}

static void init_async(RPC_ASYNC_STATE *async)
{
    RpcAsyncInitializeHandle(async, sizeof(*async));
    async->NotificationType = RpcNotificationTypeEvent;
    async->u.hEvent = CreateEventW(NULL, FALSE, FALSE, NULL);
}

/* a call that may raise: in a thread of its own; the exception ends that thread with its code */
static DWORD guarded_thread;
static LONG WINAPI filter(EXCEPTION_POINTERS *ep)
{
    if (GetCurrentThreadId() == guarded_thread) ExitThread(ep->ExceptionRecord->ExceptionCode);
    printf("FAIL  exception %08lx\n", ep->ExceptionRecord->ExceptionCode);
    fflush(stdout);
    ExitProcess(1);
}

static DWORD guarded(LPTHREAD_START_ROUTINE fn, void *arg)
{
    DWORD code = 0;
    HANDLE t = CreateThread(NULL, 0, fn, arg, CREATE_SUSPENDED, &guarded_thread);
    ResumeThread(t);
    WaitForSingleObject(t, 20000);
    GetExitCodeThread(t, &code);
    CloseHandle(t);
    return code;
}

struct args { handle_t h; LONG ok; ctx_t ctx; LONG r; RPC_ASYNC_STATE *async; LONG ms; };

static DWORD WINAPI who_thread(void *p)
{
    struct args *a = p;
    RPC_ASYNC_STATE async;
    DWORD status;
    init_async(&async);
    Who(&async, a->h, &a->ok);
    status = wait_async(&async, NULL);
    CloseHandle(async.u.hEvent);
    return status;
}

static DWORD WINAPI open_thread(void *p) { struct args *a = p; Open(a->h, &a->ctx); return 0; }
static DWORD WINAPI use_thread(void *p) { struct args *a = p; a->r = Use(a->ctx); return 0; }
static DWORD WINAPI close_thread(void *p) { struct args *a = p; Close(&a->ctx); return 0; }
static DWORD WINAPI stop_thread(void *p) { struct args *a = p; Stop(a->h); return 0; }
static DWORD WINAPI hang_start_thread(void *p) { struct args *a = p; init_async(a->async); Hang(a->async, a->h, a->ms); return 0; }

static int client(const char *ep)
{
    struct args a = {0}, b = {0};
    RPC_ASYNC_STATE hang;
    DWORD status;
    char buf[160];
    int i, used = 0;

    /* a client association whose first connection is busy when the second is opened: the
       association's first call is this one */
    b.h = bind_to(ep);
    b.async = &hang; b.ms = 700;
    guarded(hang_start_thread, &b);
    Sleep(200);
    status = guarded(open_thread, &b);
    check(status == 0 && b.ctx, "a context handle is made while another call holds the first connection");
    wait_async(&hang, NULL);
    CloseHandle(hang.u.hEvent);
    for (i = 0; i < 4; i++)
    {
        /* every other time one connection is busy and the call takes the other one: the pool's
           first connection, then its second */
        if (i & 1)
        {
            b.ms = 300;
            guarded(hang_start_thread, &b);
            Sleep(50);
        }
        b.r = 0;
        status = guarded(use_thread, &b);
        if (!status && b.r == 42) used++;
        else printf("      round %d: r %ld, exception %08lx\n", i, b.r, status);
        if (i & 1)
        {
            wait_async(&hang, NULL);
            CloseHandle(hang.u.hEvent);
        }
    }
    sprintf(buf, "the context handle is known over either connection (%d of 4 calls)", used);
    check(used == 4, buf);
    a.h = bind_to(ep);
    status = guarded(who_thread, &a);
    sprintf(buf, "an asynchronous call from NDR 6.2 stubs completes (status %lu)", status);
    check(status == RPC_S_OK, buf);
    check(a.ok == 1, "its manager routine gets the call's binding as its handle_t parameter");
    guarded(close_thread, &b);
    guarded(stop_thread, &a);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}

int main(int argc, char **argv)
{
    if (argc < 3) return 2;
    SetUnhandledExceptionFilter(filter);
    if (!strcmp(argv[1], "server")) return server(argv[2]);
    return client(argv[2]);
}
