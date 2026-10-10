/* RpcStringBindingParse (A and W) and MesEncodeFixedBufferHandleCreate
 * (patches/sg/2219): a string binding without a protocol sequence is
 * RPC_S_INVALID_STRING_BINDING and returns nothing; options are an empty string
 * when the brackets hold none; a fixed encode buffer of size 0 is
 * RPC_S_INVALID_ARG. */
#include <windows.h>
#include <rpc.h>
#include <rpcndr.h>
#include <midles.h>
#include <stdio.h>
#include <string.h>

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

int main(void)
{
    RPC_STATUS st;
    unsigned char *uuid, *protseq, *addr, *ep, *opt;
    RPC_WSTR wuuid, wprotseq, waddr, wep, wopt;
    ULONG enc;
    handle_t h;
    char *buffer = (char *)(ULONG_PTR)((0xdeadbeef + 7) & ~7);

    /* a good binding: options come back empty, not NULL */
    st = RpcStringBindingParseA((unsigned char *)"00000000-0000-0000-c000-000000000046@ncacn_np:.[endpoint=\\pipe\\test]",
                                &uuid, &protseq, &addr, &ep, &opt);
    check(st == RPC_S_OK, "a valid binding parses");
    check(st == RPC_S_OK && !strcmp((char *)protseq, "ncacn_np") && !strcmp((char *)addr, ".") && !strcmp((char *)ep, "pipetest"),
          "...protseq, network address and endpoint");
    check(st == RPC_S_OK && opt && !strcmp((char *)opt, ""), "...options are an empty string, not NULL");
    if (st == RPC_S_OK) { RpcStringFreeA(&uuid); RpcStringFreeA(&protseq); RpcStringFreeA(&addr); RpcStringFreeA(&ep); RpcStringFreeA(&opt); }

    st = RpcStringBindingParseW((RPC_WSTR)L"00000000-0000-0000-c000-000000000046@ncacn_np:.[endpoint=\\pipe\\test]",
                                &wuuid, &wprotseq, &waddr, &wep, &wopt);
    check(st == RPC_S_OK && wopt && !*wopt, "the wide version too: options are an empty string");
    if (st == RPC_S_OK) { RpcStringFreeW(&wuuid); RpcStringFreeW(&wprotseq); RpcStringFreeW(&waddr); RpcStringFreeW(&wep); RpcStringFreeW(&wopt); }

    /* real options stay */
    st = RpcStringBindingParseA((unsigned char *)"ncacn_np:srv[endpoint=\\pipe\\x,Security=Identification]", NULL, &protseq, &addr, &ep, &opt);
    check(st == RPC_S_OK && opt && !strcmp((char *)opt, "Security=Identification"), "a real option is returned");
    if (st == RPC_S_OK) { RpcStringFreeA(&protseq); RpcStringFreeA(&addr); RpcStringFreeA(&ep); RpcStringFreeA(&opt); }

    /* a binding with no protocol sequence */
    uuid = protseq = addr = ep = opt = (unsigned char *)(ULONG_PTR)0xdeadbeef;
    st = RpcStringBindingParseA((unsigned char *)"00000000-0000-0000-c000-000000000046@ncacn_np", &uuid, &protseq, &addr, &ep, &opt);
    check(st == RPC_S_INVALID_STRING_BINDING, "no protocol sequence: RPC_S_INVALID_STRING_BINDING");
    check(!uuid && !protseq && !addr && !ep && !opt, "...and nothing is returned");
    wuuid = wprotseq = waddr = wep = wopt = (RPC_WSTR)(ULONG_PTR)0xdeadbeef;
    st = RpcStringBindingParseW((RPC_WSTR)L"00000000-0000-0000-c000-000000000046@ncacn_np", &wuuid, &wprotseq, &waddr, &wep, &wopt);
    check(st == RPC_S_INVALID_STRING_BINDING && !wuuid && !wprotseq && !waddr && !wep && !wopt, "the wide version too");

    /* the other failures keep their codes */
    st = RpcStringBindingParseA((unsigned char *)"{00000000-0000-0000-c000-000000000046}@ncacn_np:.[endpoint=\\pipe\\test]", NULL, &protseq, NULL, NULL, NULL);
    check(st == RPC_S_INVALID_STRING_UUID && !protseq, "a bad object uuid is RPC_S_INVALID_STRING_UUID");
    st = RpcStringBindingParseA((unsigned char *)"ncacn_np:.[endpoint=a,endpoint=b]", NULL, &protseq, NULL, NULL, NULL);
    check(st == RPC_S_INVALID_STRING_BINDING, "two endpoints are RPC_S_INVALID_STRING_BINDING");

    /* the encode buffer */
    st = MesEncodeFixedBufferHandleCreate(buffer, 0, &enc, &h);
    check(st == RPC_S_INVALID_ARG, "MesEncodeFixedBufferHandleCreate with a zero size is RPC_S_INVALID_ARG");
    st = MesEncodeFixedBufferHandleCreate(buffer, 0, NULL, &h);
    check(st == RPC_S_INVALID_ARG, "...also without the size output");
    {
        void *real = HeapAlloc(GetProcessHeap(), 0, 64);
        char *aligned = (char *)(((ULONG_PTR)real + 7) & ~(ULONG_PTR)7);
        st = MesEncodeFixedBufferHandleCreate(aligned, 32, &enc, &h);
        check(st == RPC_S_OK, "a real buffer of 32 bytes works");
        if (st == RPC_S_OK) MesHandleFree(h);
        HeapFree(GetProcessHeap(), 0, real);
    }

    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures ? 1 : 0;
}
