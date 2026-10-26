/* Calls on one RPC context handle (patches/sg/6123). On Windows a serialized context handle is a
 * reader/writer lock: calls that take it [in] only share it, a call that takes it [in, out] has it
 * alone. Wine locked it exclusively for every call: two [in] calls on one handle waited for each
 * other, and CCleaner's uninstaller (Icarus) hung at "Starting setup" -- a call held the handle
 * while waiting for something that came back as a second call on the same handle.
 *   probe server        ncalrpc endpoint "sg_ctxshare"
 *   probe client        SHARED 1 (Poke reached Hold while Hold held the handle) + its time,
 *                       EXCL 1 (Poke waited for HoldX, which holds the handle [in, out]) */
#include <windows.h>
#include <stdio.h>
#include "ctxshare.h"

void *__RPC_USER MIDL_user_allocate(SIZE_T n) { return HeapAlloc(GetProcessHeap(), 0, n); }
void __RPC_USER MIDL_user_free(void *p) { HeapFree(GetProcessHeap(), 0, p); }

static HANDLE poked;
static int dummy;

long s_Open(handle_t h, ctx_t *ctx) { *ctx = &dummy; return 0; }
long s_Hold(ctx_t ctx, long ms) { return WaitForSingleObject(poked, ms) == WAIT_OBJECT_0; }
long s_Poke(ctx_t ctx) { SetEvent(poked); return 1; }
long s_HoldX(ctx_t *ctx, long ms) { Sleep(ms); return 1; }
void s_Close(ctx_t *ctx) { *ctx = NULL; }
void s_Stop(handle_t h) { RpcMgmtStopServerListening(NULL); }
void __RPC_USER ctx_t_rundown(ctx_t ctx) { }

static ctx_t ctx;
static long hold_result, holdx_result;
static DWORD WINAPI hold_thread(void *arg) { hold_result = Hold(ctx, 5000); return 0; }
static DWORD WINAPI holdx_thread(void *arg) { holdx_result = HoldX(&ctx, 1500); return 0; }

static LONG WINAPI filter(EXCEPTION_POINTERS *e)
{
    printf("EXCEPTION %lx\n", e->ExceptionRecord->ExceptionCode);
    fflush(stdout);
    ExitProcess(3);
}

int main(int argc, char **argv)
{
    RPC_BINDING_HANDLE b;
    RPC_WSTR str;
    HANDLE t;
    DWORD start, took;

    SetUnhandledExceptionFilter(filter);
    if (argc > 1 && !strcmp(argv[1], "server"))
    {
        poked = CreateEventW(NULL, FALSE, FALSE, NULL);
        RpcServerUseProtseqEpW((RPC_WSTR)L"ncalrpc", 20, (RPC_WSTR)L"sg_ctxshare", NULL);
        RpcServerRegisterIf(s_CtxShare_v1_0_s_ifspec, NULL, NULL);
        printf("READY\n");
        fflush(stdout);
        RpcServerListen(1, 20, FALSE);
        return 0;
    }
    RpcStringBindingComposeW(NULL, (RPC_WSTR)L"ncalrpc", NULL, (RPC_WSTR)L"sg_ctxshare", NULL, &str);
    RpcBindingFromStringBindingW(str, &b);
    Open(b, &ctx);

    /* two [in] calls: Poke must reach the server while Hold waits on the handle */
    t = CreateThread(NULL, 0, hold_thread, NULL, 0, NULL);
    Sleep(300);
    start = GetTickCount();
    Poke(ctx);
    took = GetTickCount() - start;
    WaitForSingleObject(t, 10000);
    printf("SHARED %d\nPOKE_MS %lu\n", hold_result == 1 && took < 2000, took);

    /* an [in, out] call has the handle alone: Poke waits for it */
    t = CreateThread(NULL, 0, holdx_thread, NULL, 0, NULL);
    Sleep(300);
    start = GetTickCount();
    Poke(ctx);
    took = GetTickCount() - start;
    WaitForSingleObject(t, 10000);
    printf("EXCL %d\nPOKEX_MS %lu\n", holdx_result == 1 && took >= 900, took);

    Close(&ctx);
    Stop(b);
    return 0;
}
