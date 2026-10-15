/* RpcMgmtInqIfIds for this process' server, RpcIfIdVectorFree (patch 3743) */
#include <windows.h>
#include <rpc.h>
#include <rpcdce.h>
#include <stdio.h>
#include <string.h>

static int f;
#define CHECK(c, ...) do { if (!(c)) { f++; printf("FAIL line %d: %s: ", __LINE__, #c); printf(__VA_ARGS__); printf("\n"); } } while (0)

static RPC_SERVER_INTERFACE make_if(unsigned long d1, unsigned short major, unsigned short minor)
{
    RPC_SERVER_INTERFACE i;
    memset(&i, 0, sizeof(i));
    i.Length = sizeof(i);
    i.InterfaceId.SyntaxGUID.Data1 = d1;
    i.InterfaceId.SyntaxGUID.Data2 = 0x1111;
    i.InterfaceId.SyntaxGUID.Data3 = 0x2222;
    memset(i.InterfaceId.SyntaxGUID.Data4, 0x33, 8);
    i.InterfaceId.SyntaxVersion.MajorVersion = major;
    i.InterfaceId.SyntaxVersion.MinorVersion = minor;
    i.TransferSyntax.SyntaxGUID.Data1 = 0x8a885d04;
    i.TransferSyntax.SyntaxGUID.Data2 = 0x1ceb;
    i.TransferSyntax.SyntaxGUID.Data3 = 0x11c9;
    i.TransferSyntax.SyntaxGUID.Data4[0] = 0x9f; i.TransferSyntax.SyntaxGUID.Data4[1] = 0xe8;
    i.TransferSyntax.SyntaxGUID.Data4[2] = 0x08; i.TransferSyntax.SyntaxGUID.Data4[5] = 0x2b;
    i.TransferSyntax.SyntaxGUID.Data4[6] = 0x10; i.TransferSyntax.SyntaxGUID.Data4[7] = 0x48;
    i.TransferSyntax.SyntaxVersion.MajorVersion = 2;
    return i;
}

static int has(RPC_IF_ID_VECTOR *v, unsigned long d1, unsigned maj, unsigned min)
{
    unsigned long i;
    for (i = 0; i < v->Count; i++)
        if (v->IfId[i]->Uuid.Data1 == d1 && v->IfId[i]->VersMajor == maj && v->IfId[i]->VersMinor == min) return 1;
    return 0;
}

int main(void)
{
    RPC_SERVER_INTERFACE a = make_if(0xfa57b001, 3, 7), b = make_if(0xfa57b002, 1, 0);
    RPC_IF_ID_VECTOR *vec;
    RPC_STATUS st;
    unsigned long base;

    st = RpcMgmtInqIfIds(NULL, NULL); CHECK(st == RPC_S_INVALID_ARG, "NULL vector: %ld", st);
    /* "no interface yet" before RpcServerListen (RPC_S_OK): removed at
     * integration, superseded by 5114 (documented: RpcMgmtInqIfIds answers
     * RPC_S_NOT_LISTENING until the server listens). The listing checks below
     * therefore run with the server listening. */
    st = RpcServerUseProtseqEpA((unsigned char *)"ncalrpc", RPC_C_PROTSEQ_MAX_REQS_DEFAULT,
                                (unsigned char *)"fastb-rpcifids", NULL);
    CHECK(st == RPC_S_OK, "protseq: %ld", st);
    st = RpcServerListen(1, RPC_C_LISTEN_MAX_CALLS_DEFAULT, TRUE);
    CHECK(st == RPC_S_OK, "listen: %ld", st);
    st = RpcMgmtInqIfIds(NULL, &vec);
    CHECK(st == RPC_S_OK && vec, "listening, none of ours yet: %ld", st);
    base = vec ? vec->Count : 0;
    if (vec) { st = RpcIfIdVectorFree(&vec); CHECK(st == RPC_S_OK && vec == NULL, "free: %ld %p", st, vec); }

    st = RpcServerRegisterIf(&a, NULL, NULL); CHECK(st == RPC_S_OK, "register a: %ld", st);
    st = RpcMgmtInqIfIds(NULL, &vec);
    CHECK(st == RPC_S_OK && vec && vec->Count == base + 1 && has(vec, 0xfa57b001, 3, 7), "one registered: %ld count %lu", st, vec ? vec->Count : 0);
    if (vec) RpcIfIdVectorFree(&vec);

    st = RpcServerRegisterIf(&b, NULL, NULL); CHECK(st == RPC_S_OK, "register b: %ld", st);
    st = RpcMgmtInqIfIds(NULL, &vec);
    CHECK(st == RPC_S_OK && vec && vec->Count == base + 2 && has(vec, 0xfa57b001, 3, 7) && has(vec, 0xfa57b002, 1, 0),
          "two registered: %ld count %lu", st, vec ? vec->Count : 0);
    if (vec)
    {
        unsigned long i;
        for (i = 0; i < vec->Count; i++) CHECK(vec->IfId[i] != NULL, "entry %lu is a pointer", i);
        RpcIfIdVectorFree(&vec);
    }

    st = RpcServerUnregisterIf(&a, NULL, 0); CHECK(st == RPC_S_OK, "unregister a: %ld", st);
    st = RpcMgmtInqIfIds(NULL, &vec);
    CHECK(st == RPC_S_OK && vec && vec->Count == base + 1 && !has(vec, 0xfa57b001, 3, 7) && has(vec, 0xfa57b002, 1, 0),
          "after unregister: %ld count %lu", st, vec ? vec->Count : 0);
    if (vec) RpcIfIdVectorFree(&vec);
    RpcServerUnregisterIf(&b, NULL, 0);

    /* another server's management interface is not called: a binding is refused (a limit, not Windows' answer) */
    {
        RPC_BINDING_HANDLE binding = NULL;
        unsigned char *str = (unsigned char *)"ncalrpc:[fastb-nowhere]";
        st = RpcBindingFromStringBindingA(str, &binding);
        CHECK(st == RPC_S_OK, "binding: %ld", st);
        if (st == RPC_S_OK)
        {
            /* the error code (RPC_S_INVALID_BINDING): removed at integration,
             * superseded by 5116 (RPC_S_CANNOT_SUPPORT, the documented answer
             * for an unsupported request); the call still fails and returns no vector */
            vec = (RPC_IF_ID_VECTOR *)0x1234;
            st = RpcMgmtInqIfIds(binding, &vec);
            CHECK(st != RPC_S_OK && vec == NULL, "remote binding: %ld %p", st, vec);
            RpcBindingFree(&binding);
        }
    }

    RpcMgmtStopServerListening(NULL);
    RpcMgmtWaitServerListen();
    st = RpcIfIdVectorFree(NULL); CHECK(st == RPC_S_INVALID_ARG, "free NULL: %ld", st);
    printf("%s\n", f ? "RESULT: FAIL" : "RESULT: PASS");
    return f != 0;
}
