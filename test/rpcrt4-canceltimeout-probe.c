/* The cancel timeout (RpcMgmtSetCancelTimeout, RpcCancelThread, RpcCancelThreadEx, RpcAsyncCancelCall) between two processes:
 * a widl-generated client and server over ncalrpc and ncacn_ip_tcp on 127.0.0.1.
 *   probe server PROTSEQ ENDPOINT    Slow(ms, poll): waits ms; with poll it returns 77 when the client cancelled
 *   probe client PROTSEQ ENDPOINT    runs the checks, prints RESULT: PASS or FAIL */
#include <windows.h>
#include <setjmp.h>
#include <rpc.h>
#include <rpcndr.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "rpcrt4-canceltimeout.h"

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

void *__RPC_USER MIDL_user_allocate(size_t size) { return malloc(size); }
void __RPC_USER MIDL_user_free(void *p) { free(p); }

static LONG slow(handle_t h, LONG ms, LONG poll)
{
    LONG i;
    for (i = 0; i < ms / 10; i++)
    {
        Sleep(10);
        if (poll && RpcServerTestCancel(h) == RPC_S_CALL_CANCELLED) return 77;
    }
    return 0;
}

LONG s_Slow(handle_t h, LONG ms, LONG poll) { return slow(h, ms, poll); }

void s_ASlow(PRPC_ASYNC_STATE a, handle_t h, LONG ms, LONG poll, LONG *ret)
{
    *ret = slow(h, ms, poll);
    RpcAsyncCompleteCall(a, NULL);
}

void s_Stop(handle_t h) { RpcMgmtStopServerListening(NULL); }

static int run_server(const char *protseq, const char *endpoint)
{
    RPC_STATUS status;

    status = RpcServerUseProtseqEpA((RPC_CSTR)protseq, RPC_C_PROTSEQ_MAX_REQS_DEFAULT, (RPC_CSTR)endpoint, NULL);
    if (status) { printf("server: RpcServerUseProtseqEp failed %lu\n", status); return 1; }
    status = RpcServerRegisterIf(s_CanT_v1_0_s_ifspec, NULL, NULL);
    if (status) { printf("server: RpcServerRegisterIf failed %lu\n", status); return 1; }
    printf("READY\n");
    fflush(stdout);
    status = RpcServerListen(1, RPC_C_LISTEN_MAX_CALLS_DEFAULT, FALSE);
    printf("server: done (%lu)\n", status);
    return 0;
}

/* ---- client: a thread makes a call, the main thread cancels it ---- */

static DWORD tls;
struct call
{
    jmp_buf jb;
    volatile int armed;
    handle_t h;
    LONG ms, poll;
    LONG timeout;            /* RpcMgmtSetCancelTimeout in the calling thread, or -2: leave the default */
    BOOL async;
    HANDLE ready;
    HANDLE thread;
    DWORD took;
    LONG ret;
    ULONG exception;
};

/* the exception of a failed call ends it: the thread jumps back to where the call was made */
static LONG CALLBACK catch_exception(EXCEPTION_POINTERS *ep)
{
    struct call *c = TlsGetValue(tls);

    if (!c || !c->armed) return EXCEPTION_CONTINUE_SEARCH;
    c->armed = 0;
    c->exception = ep->ExceptionRecord->ExceptionCode;
    longjmp(c->jb, 1);
    return EXCEPTION_CONTINUE_SEARCH;
}

static DWORD WINAPI call_thread(void *arg)
{
    struct call *c = arg;
    DWORD t0;

    if (c->timeout != -2) RpcMgmtSetCancelTimeout(c->timeout);
    TlsSetValue(tls, c);
    SetEvent(c->ready);
    t0 = GetTickCount();
    if (!setjmp(c->jb))
    {
        c->armed = 1;
        c->ret = Slow(c->h, c->ms, c->poll);
        c->armed = 0;
    }
    c->took = GetTickCount() - t0;
    return 0;
}

static void start_call(struct call *c, handle_t h, LONG ms, LONG poll, LONG timeout)
{
    HANDLE self;

    memset(c, 0, sizeof(*c));
    c->h = h; c->ms = ms; c->poll = poll; c->timeout = timeout;
    c->ready = CreateEventA(NULL, FALSE, FALSE, NULL);
    c->thread = CreateThread(NULL, 0, call_thread, c, 0, NULL);
    WaitForSingleObject(c->ready, 5000);
    Sleep(400);             /* in the call */
    (void)self;
}

static void end_call(struct call *c)
{
    WaitForSingleObject(c->thread, 20000);
    CloseHandle(c->thread);
    CloseHandle(c->ready);
}

static void run_checks(handle_t h)
{
    struct call c;
    char what[200];
    RPC_ASYNC_STATE a;
    HANDLE ev;
    LONG ret;
    RPC_STATUS status;
    DWORD t0, took;

    /* RpcCancelThreadEx with no time: the call is abandoned now */
    start_call(&c, h, 6000, 0, -2);
    RpcCancelThreadEx(c.thread, 0);
    end_call(&c);
    sprintf(what, "RpcCancelThreadEx(0): the call is abandoned at once (took %lu ms, exception %lu)", c.took, c.exception);
    check(c.exception == RPC_S_CALL_CANCELLED && c.took < 3000, what);

    /* RpcCancelThreadEx with 1 second against a manager which does not look: the call fails when the second is over */
    start_call(&c, h, 6000, 0, -2);
    RpcCancelThreadEx(c.thread, 1);
    end_call(&c);
    sprintf(what, "RpcCancelThreadEx(1): the call fails with RPC_S_CALL_CANCELLED after about a second (took %lu ms, exception %lu)", c.took, c.exception);
    check(c.exception == RPC_S_CALL_CANCELLED && c.took >= 1200 && c.took < 4000, what);

    /* ... against a manager which looks: it ends the call before the time is over */
    start_call(&c, h, 6000, 1, -2);
    RpcCancelThreadEx(c.thread, 4);
    end_call(&c);
    sprintf(what, "RpcCancelThreadEx(4): the server sees the cancel and answers (ret %ld, exception %lu, took %lu ms)", c.ret, c.exception, c.took);
    check(c.ret == 77 && !c.exception && c.took < 3500, what);

    /* the cancel timeout of the thread making the call, default (for ever): RpcCancelThread forwards the cancel and waits */
    start_call(&c, h, 6000, 1, -2);
    RpcCancelThread(c.thread);
    end_call(&c);
    sprintf(what, "RpcCancelThread, default timeout: the server answers the cancel (ret %ld, exception %lu, took %lu ms)", c.ret, c.exception, c.took);
    check(c.ret == 77 && !c.exception && c.took < 3500, what);

    /* RpcMgmtSetCancelTimeout(1) in the calling thread: the call fails with RPC_S_CALL_FAILED after about a second */
    start_call(&c, h, 6000, 0, 1);
    RpcCancelThread(c.thread);
    end_call(&c);
    sprintf(what, "RpcMgmtSetCancelTimeout(1) + RpcCancelThread: the call fails after about a second (took %lu ms, exception %lu)", c.took, c.exception);
    check(c.exception == RPC_S_CALL_FAILED && c.took >= 1200 && c.took < 4000, what);

    /* RpcMgmtSetCancelTimeout(0): RpcCancelThread abandons the call now */
    start_call(&c, h, 6000, 0, 0);
    RpcCancelThread(c.thread);
    end_call(&c);
    sprintf(what, "RpcMgmtSetCancelTimeout(0) + RpcCancelThread: the call is abandoned at once (took %lu ms, exception %lu)", c.took, c.exception);
    check(c.exception == RPC_S_CALL_CANCELLED && c.took < 3000, what);

    ret = Slow(h, 50, 0);
    check(ret == 0, "the binding is usable afterwards");

    /* asynchronous calls: RpcAsyncCancelCall(FALSE) with the cancel timeout of the thread */
    RpcMgmtSetCancelTimeout(RPC_C_CANCEL_INFINITE_TIMEOUT);
    ev = CreateEventA(NULL, TRUE, FALSE, NULL);
    RpcAsyncInitializeHandle(&a, sizeof(a)); a.NotificationType = RpcNotificationTypeEvent; a.u.hEvent = ev;
    ret = 0;
    ASlow(&a, h, 6000, 1, &ret);
    Sleep(400);
    RpcAsyncCancelCall(&a, FALSE);
    t0 = GetTickCount();
    check(WaitForSingleObject(ev, 8000) == WAIT_OBJECT_0, "async, default timeout: the polling server ends the call");
    took = GetTickCount() - t0;
    status = RpcAsyncCompleteCall(&a, NULL);
    sprintf(what, "... with the result of the server (status %lu, ret %ld, %lu ms)", status, ret, took);
    check(!status && ret == 77 && took < 3500, what);
    CloseHandle(ev);

    RpcMgmtSetCancelTimeout(1);
    ev = CreateEventA(NULL, TRUE, FALSE, NULL);
    RpcAsyncInitializeHandle(&a, sizeof(a)); a.NotificationType = RpcNotificationTypeEvent; a.u.hEvent = ev;
    ASlow(&a, h, 6000, 0, &ret);
    Sleep(400);
    RpcAsyncCancelCall(&a, FALSE);
    t0 = GetTickCount();
    check(WaitForSingleObject(ev, 8000) == WAIT_OBJECT_0, "async, timeout of 1 second, a manager which does not look: the call ends");
    took = GetTickCount() - t0;
    status = RpcAsyncCompleteCall(&a, NULL);
    sprintf(what, "... after about a second, cancelled (status %lu, %lu ms)", status, took);
    check(status == RPC_S_CALL_CANCELLED && took >= 400 && took < 4000, what);
    CloseHandle(ev);

    RpcMgmtSetCancelTimeout(0);
    ev = CreateEventA(NULL, TRUE, FALSE, NULL);
    RpcAsyncInitializeHandle(&a, sizeof(a)); a.NotificationType = RpcNotificationTypeEvent; a.u.hEvent = ev;
    ASlow(&a, h, 6000, 0, &ret);
    Sleep(400);
    t0 = GetTickCount();
    RpcAsyncCancelCall(&a, FALSE);
    check(WaitForSingleObject(ev, 8000) == WAIT_OBJECT_0, "async, timeout 0: the call is abandoned at once");
    took = GetTickCount() - t0;
    status = RpcAsyncCompleteCall(&a, NULL);
    sprintf(what, "... cancelled (status %lu, %lu ms)", status, took);
    check(status == RPC_S_CALL_CANCELLED && took < 1500, what);
    CloseHandle(ev);

    /* the call completes before the timer: nothing happens to it afterwards */
    RpcMgmtSetCancelTimeout(2);
    ev = CreateEventA(NULL, TRUE, FALSE, NULL);
    RpcAsyncInitializeHandle(&a, sizeof(a)); a.NotificationType = RpcNotificationTypeEvent; a.u.hEvent = ev;
    ASlow(&a, h, 6000, 1, &ret);
    Sleep(400);
    RpcAsyncCancelCall(&a, FALSE);
    WaitForSingleObject(ev, 8000);
    status = RpcAsyncCompleteCall(&a, NULL);
    CloseHandle(ev);
    Sleep(2500);        /* the timer runs out with the call gone */
    ret = Slow(h, 50, 0);
    check(!status && ret == 0, "a call which ended first is not touched when its timer runs out");
}

static int run_client(const char *protseq, const char *endpoint)
{
    RPC_STATUS status;
    RPC_CSTR str = NULL;
    handle_t h = NULL;

    status = RpcStringBindingComposeA(NULL, (RPC_CSTR)protseq, (RPC_CSTR)(strcmp(protseq, "ncalrpc") ? "127.0.0.1" : NULL),
                                      (RPC_CSTR)endpoint, NULL, &str);
    if (!status) status = RpcBindingFromStringBindingA(str, &h);
    check(!status, "binding");
    if (status) return 1;
    RpcStringFreeA(&str);
    run_checks(h);
    Stop(h);
    RpcBindingFree(&h);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}

static LONG WINAPI filter(EXCEPTION_POINTERS *ep)
{
    printf("FAIL  exception %08lx\n", ep->ExceptionRecord->ExceptionCode);
    printf("RESULT: FAIL\n");
    fflush(stdout);
    ExitProcess(3);
    return EXCEPTION_EXECUTE_HANDLER;
}

int main(int argc, char **argv)
{
    SetUnhandledExceptionFilter(filter);
    tls = TlsAlloc();
    AddVectoredExceptionHandler(1, catch_exception);
    if (argc < 4) return 2;
    if (!strcmp(argv[1], "server")) return run_server(argv[2], argv[3]);
    return run_client(argv[2], argv[3]);
}
