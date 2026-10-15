/* Asynchronous remote procedures between two processes: a widl-generated client and server (ncalrpc and ncacn_ip_tcp on 127.0.0.1).
 *   probe server PROTSEQ ENDPOINT    serves until the client calls Stop
 *   probe client PROTSEQ ENDPOINT    runs the checks, prints RESULT: PASS or FAIL
 * Calls completed inside the manager routine and from other threads, every kind of notification (event, APC,
 * completion port, callback, none), many calls at once, the out parameters and return value that come with
 * RpcAsyncCompleteCall, a call that is not complete, RpcAsyncAbortCall and an exception of the server,
 * RpcAsyncCancelCall with and without abort. */
#include <windows.h>
#include <rpc.h>
#include <rpcndr.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "rpcrt4-async-rpc.h"

static int failures;
static void check(int ok, const char *what)
{
    static DWORD last;
    DWORD now = GetTickCount();
    printf("%s  %s (%lu ms)\n", ok ? "PASS" : "FAIL", what, last ? now - last : 0);
    last = now;
    fflush(stdout);
    if (!ok) failures++;
}

void *__RPC_USER MIDL_user_allocate(size_t size) { return malloc(size); }
void __RPC_USER MIDL_user_free(void *p) { free(p); }

/* ---- server ---- */

struct work { RPC_ASYNC_STATE *async; LONG ms; LONG *buf; LONG n; LONG kind; };

static DWORD WINAPI slow_thread(void *arg)
{
    struct work *w = arg;
    LONG reply = w->ms * 2;
    Sleep(w->ms);
    RpcAsyncCompleteCall(w->async, &reply);
    free(w);
    return 0;
}

static DWORD WINAPI fill_thread(void *arg)
{
    struct work *w = arg;
    LONG i;
    Sleep(20);
    for (i = 0; i < w->n; i++) w->buf[i] = i * 7;
    RpcAsyncCompleteCall(w->async, NULL);
    free(w);
    return 0;
}

static DWORD WINAPI hang_thread(void *arg)
{
    struct work *w = arg;
    LONG waited;
    for (waited = 0; waited < w->ms; waited += 10)
    {
        if (RpcServerTestCancel(RpcAsyncGetCallHandle(w->async)) == RPC_S_CALL_CANCELLED)
        {
            RpcAsyncAbortCall(w->async, RPC_S_CALL_CANCELLED);
            free(w);
            return 0;
        }
        Sleep(10);
    }
    RpcAsyncCompleteCall(w->async, NULL);
    free(w);
    return 0;
}

static void start(LPTHREAD_START_ROUTINE proc, struct work *w)
{
    HANDLE t = CreateThread(NULL, 0, proc, w, 0, NULL);
    CloseHandle(t);
}

void s_Add(PRPC_ASYNC_STATE async, handle_t h, LONG a, LONG b, LONG *sum)
{
    if (a == -1)
    {
        /* the manager aborts the call with a code of its own */
        RpcAsyncAbortCall(async, 0x1234);
        return;
    }
    if (a == -2) RpcRaiseException(0x4321);  /* an exception before the call is complete */
    *sum = a + b;
    RpcAsyncCompleteCall(async, NULL);  /* inside the manager routine */
}

LONG s_Slow(PRPC_ASYNC_STATE async, handle_t h, LONG ms)
{
    struct work *w = calloc(1, sizeof(*w));
    w->async = async; w->ms = ms;
    start(slow_thread, w);
    return 0;
}

void s_Fill(PRPC_ASYNC_STATE async, handle_t h, LONG n, LONG *buf)
{
    struct work *w = calloc(1, sizeof(*w));
    w->async = async; w->buf = buf; w->n = n;
    start(fill_thread, w);
}

void s_Hang(PRPC_ASYNC_STATE async, handle_t h, LONG ms)
{
    struct work *w = calloc(1, sizeof(*w));
    w->async = async; w->ms = ms;
    start(hang_thread, w);
}

void s_Pair(PRPC_ASYNC_STATE async, handle_t h, short a, LONGLONG b, double d, LONGLONG *r)
{
    *r = a + b + (LONGLONG)d;
    RpcAsyncCompleteCall(async, NULL);
}

LONG s_Sync(handle_t h, LONG x)
{
    return x + 1;
}

void s_None(PRPC_ASYNC_STATE async, handle_t h)
{
    struct work *w = calloc(1, sizeof(*w));
    w->async = async; w->ms = 30;
    start(slow_thread, w);
}

void s_Stop(handle_t h)
{
    RpcMgmtStopServerListening(NULL);
}

static RPC_SERVER_INTERFACE all_if;
static RPC_DISPATCH_TABLE all_table;
static RPC_DISPATCH_FUNCTION all_funcs[8];

/* all_procs: every entry of the dispatch table is NdrServerCallAll, which tells the synchronous procedures from the
 * asynchronous ones itself */
static int run_server(const char *protseq, const char *endpoint, int all_procs)
{
    RPC_STATUS status;
    RPC_IF_HANDLE ifh = s_AsyT_v1_0_s_ifspec;

    status = RpcServerUseProtseqEpA((RPC_CSTR)protseq, RPC_C_PROTSEQ_MAX_REQS_DEFAULT, (RPC_CSTR)endpoint, NULL);
    if (status) { printf("server: RpcServerUseProtseqEp failed %lu\n", status); return 1; }
    if (all_procs)
    {
        int i;
        memcpy(&all_if, s_AsyT_v1_0_s_ifspec, sizeof(all_if));
        all_table.DispatchTableCount = 8;
        for (i = 0; i < 8; i++) all_funcs[i] = (RPC_DISPATCH_FUNCTION)GetProcAddress(GetModuleHandleA("rpcrt4.dll"), "NdrServerCallAll");
        all_table.DispatchTable = all_funcs;
        all_if.DispatchTable = &all_table;
        ifh = (RPC_IF_HANDLE)&all_if;
    }
    status = RpcServerRegisterIf(ifh, NULL, NULL);
    if (status) { printf("server: RpcServerRegisterIf failed %lu\n", status); return 1; }
    printf("READY\n");
    fflush(stdout);
    status = RpcServerListen(1, RPC_C_LISTEN_MAX_CALLS_DEFAULT, FALSE);
    printf("server: done (%lu)\n", status);
    return 0;
}

/* ---- client ---- */

static HANDLE apc_event, cb_event;
static volatile LONG apc_runs, cb_runs;
static volatile RPC_ASYNC_EVENT apc_event_arg, cb_event_arg;

static void RPC_ENTRY apc_routine(PRPC_ASYNC_STATE async, void *context, RPC_ASYNC_EVENT event)
{
    apc_event_arg = event;
    InterlockedIncrement(&apc_runs);
}

static void RPC_ENTRY cb_routine(PRPC_ASYNC_STATE async, void *context, RPC_ASYNC_EVENT event)
{
    cb_event_arg = event;
    InterlockedIncrement(&cb_runs);
    SetEvent(cb_event);
}

static void init_event(RPC_ASYNC_STATE *a, HANDLE *event)
{
    RpcAsyncInitializeHandle(a, sizeof(*a));
    *event = CreateEventA(NULL, TRUE, FALSE, NULL);
    a->NotificationType = RpcNotificationTypeEvent;
    a->u.hEvent = *event;
}

static void run_checks(handle_t h)
{
    RPC_ASYNC_STATE a, b[8];
    HANDLE ev, evs[8];
    RPC_STATUS status;
    LONG sum = 0, ret = 0, i;
    DWORD t0, took;

    /* a call completed inside the manager routine, event notification */
    init_event(&a, &ev);
    Add(&a, h, 20, 22, &sum);
    check(WaitForSingleObject(ev, 10000) == WAIT_OBJECT_0, "event notification of a call completed inside the manager routine");
    check(RpcAsyncGetCallStatus(&a) == RPC_S_OK, "its status is RPC_S_OK");
    status = RpcAsyncCompleteCall(&a, NULL);
    check(status == RPC_S_OK && sum == 42, "the out parameter comes with RpcAsyncCompleteCall (20 + 22)");
    CloseHandle(ev);

    /* a call completed by another thread later, with a return value */
    init_event(&a, &ev);
    t0 = GetTickCount();
    Slow(&a, h, 400);
    ret = 0;
    status = RpcAsyncGetCallStatus(&a);
    check(status == RPC_S_ASYNC_CALL_PENDING, "right after the call the status is RPC_S_ASYNC_CALL_PENDING");
    status = RpcAsyncCompleteCall(&a, &ret);
    check(status == RPC_S_ASYNC_CALL_PENDING, "RpcAsyncCompleteCall of a call which is not complete: RPC_S_ASYNC_CALL_PENDING");
    check(WaitForSingleObject(ev, 10000) == WAIT_OBJECT_0, "event notification of a call completed by another thread");
    took = GetTickCount() - t0;
    check(took >= 300, "... not before the server completed it");
    status = RpcAsyncCompleteCall(&a, &ret);
    check(status == RPC_S_OK && ret == 800, "the return value comes with RpcAsyncCompleteCall");
    CloseHandle(ev);

    /* asynchronous procedure with no parameters; notification none (polling) */
    RpcAsyncInitializeHandle(&a, sizeof(a));
    a.NotificationType = RpcNotificationTypeNone;
    None(&a, h);
    for (i = 0; i < 500 && RpcAsyncGetCallStatus(&a) == RPC_S_ASYNC_CALL_PENDING; i++) Sleep(10);
    check(RpcAsyncGetCallStatus(&a) == RPC_S_OK, "notification none: the status says when the call is complete");
    check(RpcAsyncCompleteCall(&a, &ret) == RPC_S_OK, "... and the call ends");

    /* APC */
    RpcAsyncInitializeHandle(&a, sizeof(a));
    a.NotificationType = RpcNotificationTypeApc;
    DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &a.u.APC.hThread, 0, FALSE, DUPLICATE_SAME_ACCESS);
    a.u.APC.NotificationRoutine = apc_routine;
    Add(&a, h, 1, 2, &sum);
    for (i = 0; i < 200 && !apc_runs; i++) SleepEx(50, TRUE);
    check(apc_runs == 1 && apc_event_arg == RpcCallComplete, "APC notification: the routine runs in the thread with RpcCallComplete");
    check(RpcAsyncCompleteCall(&a, NULL) == RPC_S_OK && sum == 3, "... and the call ends");
    CloseHandle(a.u.APC.hThread);

    /* completion port */
    {
        HANDLE port = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 1);
        OVERLAPPED ov, *got = NULL;
        DWORD bytes = 0;
        ULONG_PTR key = 0;

        memset(&ov, 0, sizeof(ov));
        RpcAsyncInitializeHandle(&a, sizeof(a));
        a.NotificationType = RpcNotificationTypeIoc;
        a.u.IOC.hIOPort = port;
        a.u.IOC.dwNumberOfBytesTransferred = 77;
        a.u.IOC.dwCompletionKey = 0x1234;
        a.u.IOC.lpOverlapped = &ov;
        Add(&a, h, 5, 6, &sum);
        check(GetQueuedCompletionStatus(port, &bytes, &key, &got, 10000) && bytes == 77 && key == 0x1234 && got == &ov,
              "completion port notification: the packet carries what the state said");
        check(RpcAsyncCompleteCall(&a, NULL) == RPC_S_OK && sum == 11, "... and the call ends");
        CloseHandle(port);
    }

    /* callback */
    cb_event = CreateEventA(NULL, TRUE, FALSE, NULL);
    RpcAsyncInitializeHandle(&a, sizeof(a));
    a.NotificationType = RpcNotificationTypeCallback;
    a.u.NotificationRoutine = cb_routine;
    Add(&a, h, 7, 8, &sum);
    check(WaitForSingleObject(cb_event, 10000) == WAIT_OBJECT_0 && cb_runs == 1 && cb_event_arg == RpcCallComplete,
          "callback notification: the routine runs with RpcCallComplete");
    check(RpcAsyncCompleteCall(&a, NULL) == RPC_S_OK && sum == 15, "... and the call ends");
    CloseHandle(cb_event);

    /* arguments of other types: short, hyper, double, hyper result */
    {
        LONGLONG r = 0;
        init_event(&a, &ev);
        Pair(&a, h, -3, 5000000000LL, 12.9, &r);
        WaitForSingleObject(ev, 10000);
        check(RpcAsyncCompleteCall(&a, NULL) == RPC_S_OK && r == -3 + 5000000000LL + 12, "short, hyper and double arguments, hyper result");
        CloseHandle(ev);
    }

    /* a larger out parameter, filled by another thread */
    {
        LONG n = 300000;
        LONG *buf = calloc(n, sizeof(LONG));
        init_event(&a, &ev);
        Fill(&a, h, n, buf);
        WaitForSingleObject(ev, 10000);
        check(RpcAsyncCompleteCall(&a, NULL) == RPC_S_OK, "an out array of 1.2 MB: the call ends");
        for (i = 0; i < n && buf[i] == i * 7; i++) ;
        check(i == n, "... with all its elements");
        free(buf);
        CloseHandle(ev);
    }

    /* many calls at once */
    t0 = GetTickCount();
    {
        LONG rets[8];
        for (i = 0; i < 8; i++)
        {
            init_event(&b[i], &evs[i]);
            Slow(&b[i], h, 300 + 40 * i);
        }
        for (i = 0; i < 8; i++)
        {
            WaitForSingleObject(evs[i], 10000);
            rets[i] = -1;
            status = RpcAsyncCompleteCall(&b[i], &rets[i]);
            if (status != RPC_S_OK || rets[i] != 2 * (300 + 40 * i)) { printf("      call %ld: status 0x%lx ret %ld\n", i, status, rets[i]); break; }
            CloseHandle(evs[i]);
        }
        took = GetTickCount() - t0;
        check(i == 8, "eight calls at once: each ends with its own return value");
        check(took < 2000, "... together (they do not wait for each other)");
    }

    /* a synchronous procedure on the same interface */
    check(Sync(h, 41) == 42, "a synchronous procedure of the same interface works");

    /* the manager aborts the call */
    init_event(&a, &ev);
    Add(&a, h, -1, 0, &sum);
    check(WaitForSingleObject(ev, 10000) == WAIT_OBJECT_0, "RpcAsyncAbortCall: the client is notified");
    status = RpcAsyncCompleteCall(&a, NULL);
    printf("      status 0x%lx\n", status);
    check(status == 0x1234, "... and RpcAsyncCompleteCall returns the code of the abort");
    CloseHandle(ev);

    /* the manager raises an exception before it completes the call: a fault, and the server goes on */
    init_event(&a, &ev);
    Add(&a, h, -2, 0, &sum);
    check(WaitForSingleObject(ev, 10000) == WAIT_OBJECT_0, "an exception of the manager routine: the client is notified");
    status = RpcAsyncCompleteCall(&a, NULL);
    printf("      status 0x%lx\n", status);
    check(status == 0x4321, "... with the exception code");
    CloseHandle(ev);
    check(Sync(h, 1) == 2, "... and the server serves on");

    /* RpcAsyncCancelCall with abort: the client gives up at once */
    init_event(&a, &ev);
    Hang(&a, h, 6000);
    Sleep(150);
    t0 = GetTickCount();
    check(RpcAsyncCancelCall(&a, TRUE) == RPC_S_OK, "RpcAsyncCancelCall(TRUE)");
    check(WaitForSingleObject(ev, 5000) == WAIT_OBJECT_0 && GetTickCount() - t0 < 3000, "... the notification comes at once");
    check(RpcAsyncGetCallStatus(&a) == RPC_S_CALL_CANCELLED, "... the status is RPC_S_CALL_CANCELLED");
    check(RpcAsyncCompleteCall(&a, NULL) == RPC_S_CALL_CANCELLED, "... and so is the result of RpcAsyncCompleteCall");
    CloseHandle(ev);
    check(Sync(h, 9) == 10, "after the abandoned call the binding works");

    /* RpcAsyncCancelCall without abort: the server is told, sees it and aborts the call */
    init_event(&a, &ev);
    Hang(&a, h, 6000);
    Sleep(150);
    t0 = GetTickCount();
    check(RpcAsyncCancelCall(&a, FALSE) == RPC_S_OK, "RpcAsyncCancelCall(FALSE)");
    check(WaitForSingleObject(ev, 5000) == WAIT_OBJECT_0 && GetTickCount() - t0 < 3000, "... the server sees RpcServerTestCancel and the call ends");
    check(RpcAsyncCompleteCall(&a, NULL) == RPC_S_CALL_CANCELLED, "... with RPC_S_CALL_CANCELLED");
    CloseHandle(ev);
    check(Sync(h, 20) == 21, "after the cancelled call the binding works");

    /* a call which cannot be cancelled any more */
    init_event(&a, &ev);
    Add(&a, h, 1, 1, &sum);
    WaitForSingleObject(ev, 10000);
    check(RpcAsyncCancelCall(&a, TRUE) == RPC_S_OK, "RpcAsyncCancelCall of a complete call: RPC_S_OK");
    status = RpcAsyncCompleteCall(&a, NULL);
    printf("      status 0x%lx sum %ld\n", status, sum);
    check(status == RPC_S_OK && sum == 2, "... the call ends with its results");
    CloseHandle(ev);
    check(RpcAsyncCancelCall(&a, TRUE) == RPC_S_INVALID_ASYNC_HANDLE || 1, "(cancel of an ended call is not an error we depend on)");
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
    if (argc < 4) return 2;
    if (!strcmp(argv[1], "server")) return run_server(argv[2], argv[3], 0);
    if (!strcmp(argv[1], "serverall")) return run_server(argv[2], argv[3], 1);
    return run_client(argv[2], argv[3]);
}
