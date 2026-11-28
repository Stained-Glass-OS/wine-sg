/* nlmmetered gate probe: the connection cost a program is told follows the network's metered
 * setting (NetworkManager's, through sg-netctl's "cost"). Usage: probe STATEFILE NEXTSTATE...
 * prints COST <manager cost> <connection cost> after writing each next state (and 13 s, past
 * netprofm's 10-second cache). */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <netlistmgr.h>
#include <stdio.h>

static void report(INetworkListManager *mgr)
{
    INetworkCostManager *cm;
    IEnumNetworkConnections *conns;
    INetworkConnection *conn;
    INetworkConnectionCost *cc;
    DWORD mcost = 0xdead, ccost = 0xdead;
    ULONG got = 0;

    if (SUCCEEDED(INetworkListManager_QueryInterface(mgr, &IID_INetworkCostManager, (void **)&cm)))
    {
        INetworkCostManager_GetCost(cm, &mcost, NULL);
        INetworkCostManager_Release(cm);
    }
    if (SUCCEEDED(INetworkListManager_GetNetworkConnections(mgr, &conns)))
    {
        while (IEnumNetworkConnections_Next(conns, 1, &conn, &got) == S_OK && got)
        {
            VARIANT_BOOL inet = VARIANT_FALSE;
            INetworkConnection_get_IsConnectedToInternet(conn, &inet);
            if (inet && SUCCEEDED(INetworkConnection_QueryInterface(conn, &IID_INetworkConnectionCost, (void **)&cc)))
            {
                INetworkConnectionCost_GetCost(cc, &ccost);
                INetworkConnectionCost_Release(cc);
            }
            INetworkConnection_Release(conn);
        }
        IEnumNetworkConnections_Release(conns);
    }
    printf("COST %lu %lu\n", mcost, ccost);
    fflush(stdout);
}

int main(int argc, char **argv)
{
    INetworkListManager *mgr;
    int i;

    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (FAILED(CoCreateInstance(&CLSID_NetworkListManager, NULL, CLSCTX_INPROC_SERVER, &IID_INetworkListManager, (void **)&mgr)))
    {
        printf("NO NLM\n");
        return 1;
    }
    report(mgr);
    for (i = 2; i < argc; i++)
    {
        FILE *f = fopen(argv[1], "w");
        if (f) { fputs(argv[i], f); fclose(f); }
        Sleep(13000);
        report(mgr);
    }
    INetworkListManager_Release(mgr);
    return 0;
}
