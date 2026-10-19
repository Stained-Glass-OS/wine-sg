/* rpcselfunreg-probe: an RPC call that unregisters its own interface, with
 * WaitForCallsToComplete, must return (patches/sg/5938). COM does this when
 * the call that ends an object releases its last stub; the Click-to-Run
 * service froze there and Office setup stayed at 1%.
 *
 * Raw RPC, no MIDL: a server interface whose one procedure calls
 * RpcServerUnregisterIf(itself, NULL, TRUE); a client thread calls it over
 * ncalrpc. Prints "RETURNED" or "HUNG". */
#include <windows.h>
#include <rpc.h>
#include <rpcdcep.h>
#include <stdio.h>

static RPC_SERVER_INTERFACE server_if;

static void __RPC_STUB self_unregister(PRPC_MESSAGE msg)
{
    RPC_STATUS s = RpcServerUnregisterIf(&server_if, NULL, TRUE);
    printf("unregister in call: %ld\n", s);
    msg->BufferLength = 0;
    I_RpcGetBuffer(msg);
}

static RPC_DISPATCH_FUNCTION dispatch_fns[] = { self_unregister };
static RPC_DISPATCH_TABLE dispatch_table = { 1, dispatch_fns, 0 };
static const RPC_SYNTAX_IDENTIFIER ndr_syntax =
    { {0x8a885d04,0x1ceb,0x11c9,{0x9f,0xe8,0x08,0x00,0x2b,0x10,0x48,0x60}}, {2,0} };
static const RPC_SYNTAX_IDENTIFIER if_id =
    { {0x5a6e1f2b,0x3c4d,0x4e5f,{0x80,0x91,0xa2,0xb3,0xc4,0xd5,0xe6,0xf7}}, {1,0} };
static RPC_CLIENT_INTERFACE client_if;

static DWORD WINAPI client(void *arg)
{
    RPC_BINDING_HANDLE b;
    RPC_MESSAGE msg = {0};
    RPC_STATUS s = RpcBindingFromStringBindingA((RPC_CSTR)"ncalrpc:[sg-rpcselfunreg]", &b);
    if (s) { printf("bind: %ld\n", s); return 1; }
    msg.Handle = b;
    msg.RpcInterfaceInformation = &client_if;
    msg.ProcNum = 0 | RPC_FLAGS_VALID_BIT;
    msg.BufferLength = 0;
    if ((s = I_RpcGetBuffer(&msg))) { printf("getbuffer: %ld\n", s); return 1; }
    s = I_RpcSendReceive(&msg);
    printf("sendreceive: %ld\n", s);
    if (!s) I_RpcFreeBuffer(&msg);
    return 0;
}

int main(void)
{
    HANDLE t;
    RPC_STATUS s;
    server_if.Length = sizeof(server_if);
    server_if.InterfaceId = if_id;
    server_if.TransferSyntax = ndr_syntax;
    server_if.DispatchTable = &dispatch_table;
    client_if.Length = sizeof(client_if);
    client_if.InterfaceId = if_id;
    client_if.TransferSyntax = ndr_syntax;
    if ((s = RpcServerUseProtseqEpA((RPC_CSTR)"ncalrpc", 20, (RPC_CSTR)"sg-rpcselfunreg", NULL))) { printf("useprotseq: %ld\n", s); return 2; }
    if ((s = RpcServerRegisterIfEx(&server_if, NULL, NULL, RPC_IF_AUTOLISTEN, 20, NULL))) { printf("register: %ld\n", s); return 2; }
    t = CreateThread(NULL, 0, client, NULL, 0, NULL);
    if (WaitForSingleObject(t, 10000) == WAIT_OBJECT_0) { printf("RETURNED\n"); fflush(stdout); ExitProcess(0); }
    printf("HUNG\n"); fflush(stdout);
    ExitProcess(1);
}
