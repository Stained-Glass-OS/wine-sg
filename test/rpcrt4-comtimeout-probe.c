/* RpcMgmtSetComTimeout / RpcMgmtInqComTimeout: the communications time-out hint in a client binding, 0 to 10
 * (RPC_S_INVALID_TIMEOUT beyond), the default is RPC_C_BINDING_DEFAULT_TIMEOUT, a copy of a binding keeps it,
 * a server binding is the wrong kind and a NULL one is invalid.  Set was a stub that answered success whatever
 * it was given, Inq was an unimplemented export. */
#include <windows.h>
#include <rpc.h>
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
    static const struct { unsigned int timeout; RPC_STATUS status; const char *name; } sets[] =
    {
        {0, RPC_S_OK, "0, the lowest"},
        {1, RPC_S_OK, "1"},
        {5, RPC_S_OK, "5, the default"},
        {9, RPC_S_OK, "9, the highest finite"},
        {10, RPC_S_OK, "10, infinite"},
        {11, RPC_S_INVALID_TIMEOUT, "11, out of range"},
        {100, RPC_S_INVALID_TIMEOUT, "100"},
        {0xffffffff, RPC_S_INVALID_TIMEOUT, "-1"},
    };
    RPC_BINDING_HANDLE client, copy, server;
    RPC_CSTR string_binding = NULL;
    unsigned int got, expect;
    RPC_STATUS status;
    unsigned int i;
    char text[120];

    status = RpcBindingFromStringBindingA((RPC_CSTR)"ncalrpc:[sg_comtimeout_probe]", &client);
    check(status == RPC_S_OK, "a client binding");

    got = 0xdeadbeef;
    status = RpcMgmtInqComTimeout(client, &got);
    sprintf(text, "the default is 5 (status %ld, got %u)", status, got);
    check(status == RPC_S_OK && got == RPC_C_BINDING_DEFAULT_TIMEOUT, text);

    expect = RPC_C_BINDING_DEFAULT_TIMEOUT;
    for (i = 0; i < sizeof(sets) / sizeof(sets[0]); i++)
    {
        status = RpcMgmtSetComTimeout(client, sets[i].timeout);
        sprintf(text, "set %s -> status %ld", sets[i].name, status);
        check(status == sets[i].status, text);
        if (status == RPC_S_OK) expect = sets[i].timeout;
        got = 0xdeadbeef;
        status = RpcMgmtInqComTimeout(client, &got);
        sprintf(text, "  inquire after it: %u (expected %u, status %ld)", got, expect, status);
        check(status == RPC_S_OK && got == expect, text);
    }

    /* a copy has the value of its origin */
    RpcMgmtSetComTimeout(client, 7);
    status = RpcBindingCopy(client, &copy);
    check(status == RPC_S_OK, "RpcBindingCopy");
    got = 0;
    RpcMgmtInqComTimeout(copy, &got);
    sprintf(text, "the copy keeps 7 (got %u)", got);
    check(got == 7, text);
    RpcMgmtSetComTimeout(copy, 2);
    got = 0;
    RpcMgmtInqComTimeout(client, &got);
    check(got == 7, "changing the copy leaves the original alone");
    RpcBindingFree(&copy);

    /* arguments */
    check(RpcMgmtSetComTimeout(NULL, 3) == RPC_S_INVALID_BINDING, "Set on a NULL binding is RPC_S_INVALID_BINDING");
    check(RpcMgmtInqComTimeout(NULL, &got) == RPC_S_INVALID_BINDING, "Inq on a NULL binding is RPC_S_INVALID_BINDING");
    check(RpcMgmtInqComTimeout(client, NULL) == ERROR_INVALID_PARAMETER, "Inq into a NULL pointer is an invalid argument");

    /* a server binding is the wrong kind */
    status = RpcServerUseProtseqEpA((RPC_CSTR)"ncalrpc", RPC_C_PROTSEQ_MAX_REQS_DEFAULT, (RPC_CSTR)"sg_comtimeout_srv", NULL);
    if (status == RPC_S_OK)
    {
        RPC_BINDING_VECTOR *vector = NULL;
        status = RpcServerInqBindings(&vector);
        if (status == RPC_S_OK && vector && vector->Count)
        {
            server = vector->BindingH[0];
            sprintf(text, "Set on a server binding (status %ld)", RpcMgmtSetComTimeout(server, 3));
            check(RpcMgmtSetComTimeout(server, 3) == RPC_S_WRONG_KIND_OF_BINDING, text);
            check(RpcMgmtInqComTimeout(server, &got) == RPC_S_WRONG_KIND_OF_BINDING, "Inq on a server binding is the wrong kind");
            RpcBindingVectorFree(&vector);
        }
        else check(0, "RpcServerInqBindings");
    }
    else check(0, "RpcServerUseProtseqEp");

    if (string_binding) RpcStringFreeA(&string_binding);
    RpcBindingFree(&client);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
