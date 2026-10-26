/* rpcrt4 (6117): a client binds "<object>@ncalrpc:" with no endpoint; the server registered its
 * dynamic endpoint with RpcEpRegister under that object UUID (NVDA: nvda.exe's controller, which
 * its in-process helper calls). RpcEpResolveBinding asked the endpoint mapper for the nil object
 * only and found nothing ("couldn't find an endpoint"). An endpoint registered with no object
 * still answers a bound object.
 *   probe server     registers one interface twice: under OBJECT_A, and with no object (Stop)
 *   probe client */
#include <windows.h>
#include <rpc.h>
#include <stdio.h>
#include <string.h>
#include "epobject.h"

void *__RPC_USER MIDL_user_allocate(size_t size) { return malloc(size); }
void __RPC_USER MIDL_user_free(void *p) { free(p); }

#define OBJECT_A "8504771c-f06e-4dcd-8c97-190559fdbf39"

LONG s_Ping(handle_t h, LONG x) { return x + 1; }
void s_Stop(handle_t h) { RpcMgmtStopServerListening(NULL); }

static int server(void)
{
    RPC_BINDING_VECTOR *bv;
    UUID obj;
    UUID_VECTOR uv = { 1, { &obj } };
    RPC_STATUS st;

    UuidFromStringA((RPC_CSTR)OBJECT_A, &obj);
    if ((st = RpcServerUseProtseqA((RPC_CSTR)"ncalrpc", 20, NULL))) { printf("protseq %lu\n", st); return 1; }
    if ((st = RpcServerRegisterIf(s_EpObj_v1_0_s_ifspec, NULL, NULL))) { printf("registerif %lu\n", st); return 1; }
    if ((st = RpcServerInqBindings(&bv))) { printf("inqbindings %lu\n", st); return 1; }
    if ((st = RpcEpRegisterA(s_EpObj_v1_0_s_ifspec, bv, &uv, (RPC_CSTR)"sg epobject"))) { printf("epregister %lu\n", st); return 1; }
    if ((st = RpcServerListen(1, 10, TRUE))) { printf("listen %lu\n", st); return 1; }
    printf("READY\n");
    fflush(stdout);
    RpcMgmtWaitServerListen();
    RpcEpUnregister(s_EpObj_v1_0_s_ifspec, bv, &uv);
    return 0;
}

static LONG WINAPI filter(EXCEPTION_POINTERS *ep)
{
    printf("EXCEPTION %08lx\n", ep->ExceptionRecord->ExceptionCode);
    fflush(stdout);
    ExitProcess(1);
}

static int client(void)
{
    RPC_CSTR str;
    handle_t h;
    LONG r;

    SetUnhandledExceptionFilter(filter);
    RpcStringBindingComposeA((RPC_CSTR)OBJECT_A, (RPC_CSTR)"ncalrpc", NULL, NULL, NULL, &str);
    RpcBindingFromStringBindingA(str, &h);
    RpcStringFreeA(&str);
    printf("RESOLVE %lu\n", RpcEpResolveBinding(h, EpObj_v1_0_c_ifspec));
    r = Ping(h, 41);
    printf("PING %ld\n", r);
    Stop(h);
    return 0;
}

int main(int argc, char **argv)
{
    if (argc > 1 && !strcmp(argv[1], "server")) return server();
    return client();
}
