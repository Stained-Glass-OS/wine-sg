/* The Network List Manager (patches/sg/1689), run by test/netlist-gate.sh in
 * a network namespace of its own (a dummy interface with an address and no
 * default route). It reads a network's name, description, times and
 * category, sets its name and category (kept in its profile: a new manager
 * reads them back), calls the manager through IDispatch, counts connection
 * points and sinks, sees a fixed (metered) cost set in DefaultMediaCost, and
 * waits for ConnectivityChanged to say "internet" when the gate adds a
 * default route. These were stubs: E_NOTIMPL, FIXMEs, no events.
 *
 *   netlist-probe.exe READYFILE */
#define COBJMACROS
#include <windows.h>
#include <ole2.h>
#include <olectl.h>
#include <netlistmgr.h>
#include <stdio.h>

static const CLSID clsid_nlm = { 0xdcb00c01, 0x570f, 0x4a9b, { 0x8d, 0x69, 0x19, 0x9f, 0xdb, 0xa5, 0x72, 0x3b } };
static const IID iid_nlm = { 0xdcb00000, 0x570f, 0x4a9b, { 0x8d, 0x69, 0x19, 0x9f, 0xdb, 0xa5, 0x72, 0x3b } };
static const IID iid_nlm_events = { 0xdcb00001, 0x570f, 0x4a9b, { 0x8d, 0x69, 0x19, 0x9f, 0xdb, 0xa5, 0x72, 0x3b } };
static const IID iid_cost_mgr = { 0xdcb00008, 0x570f, 0x4a9b, { 0x8d, 0x69, 0x19, 0x9f, 0xdb, 0xa5, 0x72, 0x3b } };

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

static HANDLE changed_event;
static LONG last_connectivity = -1;

static HRESULT WINAPI sink_QueryInterface(INetworkListManagerEvents *iface, REFIID riid, void **ppv)
{
    if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &iid_nlm_events))
    {
        *ppv = iface;
        return S_OK;
    }
    *ppv = NULL;
    return E_NOINTERFACE;
}
static ULONG WINAPI sink_AddRef(INetworkListManagerEvents *iface) { return 2; }
static ULONG WINAPI sink_Release(INetworkListManagerEvents *iface) { return 1; }
static HRESULT WINAPI sink_ConnectivityChanged(INetworkListManagerEvents *iface, NLM_CONNECTIVITY conn)
{
    last_connectivity = conn;
    SetEvent(changed_event);
    return S_OK;
}
static INetworkListManagerEventsVtbl sink_vtbl = { sink_QueryInterface, sink_AddRef, sink_Release,
                                                   sink_ConnectivityChanged };
static INetworkListManagerEvents sink = { &sink_vtbl };

static INetwork *first_network(INetworkListManager *mgr)
{
    IEnumNetworks *en = NULL;
    INetwork *net = NULL;
    ULONG got = 0;

    if (FAILED(INetworkListManager_GetNetworks(mgr, NLM_ENUM_NETWORK_CONNECTED, &en)) || !en) return NULL;
    IEnumNetworks_Next(en, 1, &net, &got);
    IEnumNetworks_Release(en);
    return got ? net : NULL;
}

int main(int argc, char **argv)
{
    INetworkListManager *mgr = NULL, *mgr2 = NULL;
    IConnectionPointContainer *cpc;
    IConnectionPoint *cp;
    IEnumConnectionPoints *ecp;
    IEnumConnections *ec;
    INetworkCostManager *cost_mgr;
    INetwork *net, *net2;
    IDispatch *disp;
    CONNECTDATA cd;
    DWORD cookie = 0, cost = 0, lo, hi, lo2, hi2, value = 2;
    NLM_NETWORK_CATEGORY cat = -1;
    NLM_CONNECTIVITY conn = -1;
    DISPPARAMS params = { 0 };
    VARIANT result;
    OLECHAR *name = (OLECHAR *)L"IsConnected";
    DISPID dispid = 0;
    ULONG got = 0;
    UINT count = 0;
    HKEY key;
    BSTR s = NULL;
    HRESULT hr;

    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    changed_event = CreateEventW(NULL, FALSE, FALSE, NULL);

    /* metered: DefaultMediaCost, as Windows keeps it */
    RegCreateKeyExW(HKEY_LOCAL_MACHINE, L"Software\\Microsoft\\Windows NT\\CurrentVersion\\NetworkList\\DefaultMediaCost",
                    0, NULL, 0, KEY_ALL_ACCESS, NULL, &key, NULL);
    RegSetValueExW(key, L"Ethernet", 0, REG_DWORD, (BYTE *)&value, sizeof(value));
    RegSetValueExW(key, L"Default", 0, REG_DWORD, (BYTE *)&value, sizeof(value));
    RegCloseKey(key);

    hr = CoCreateInstance(&clsid_nlm, NULL, CLSCTX_INPROC_SERVER, &iid_nlm, (void **)&mgr);
    check(hr == S_OK, "the Network List Manager");
    if (!mgr) goto done;
    INetworkListManager_GetConnectivity(mgr, &conn);
    check(conn == NLM_CONNECTIVITY_IPV4_LOCALNETWORK, "an address, no default route: IPv4 local network");
    if (conn != NLM_CONNECTIVITY_IPV4_LOCALNETWORK) printf("      connectivity %#x\n", conn);

    net = first_network(mgr);
    check(net != NULL, "a connected network");
    if (net)
    {
        check(INetwork_GetName(net, &s) == S_OK && s && *s, "GetName (was E_NOTIMPL)");
        SysFreeString(s); s = NULL;
        check(INetwork_GetDescription(net, &s) == S_OK && s, "GetDescription");
        SysFreeString(s); s = NULL;
        check(INetwork_GetTimeCreatedAndConnected(net, &lo, &hi, &lo2, &hi2) == S_OK && (hi || lo) && (hi2 || lo2),
              "GetTimeCreatedAndConnected");
        check(INetwork_SetCategory(net, NLM_NETWORK_CATEGORY_DOMAIN_AUTHENTICATED) == E_INVALIDARG,
              "SetCategory(domain): E_INVALIDARG");
        check(INetwork_SetCategory(net, NLM_NETWORK_CATEGORY_PRIVATE) == S_OK &&
              INetwork_GetCategory(net, &cat) == S_OK && cat == NLM_NETWORK_CATEGORY_PRIVATE, "SetCategory(private)");
        check(INetwork_SetName(net, (BSTR)L"SG probe network") == S_OK, "SetName");
        CoCreateInstance(&clsid_nlm, NULL, CLSCTX_INPROC_SERVER, &iid_nlm, (void **)&mgr2);
        net2 = mgr2 ? first_network(mgr2) : NULL;
        cat = -1;
        if (net2)
        {
            INetwork_GetName(net2, &s);
            INetwork_GetCategory(net2, &cat);
        }
        check(s && !lstrcmpW(s, L"SG probe network") && cat == NLM_NETWORK_CATEGORY_PRIVATE,
              "a new manager reads the name and category from the profile");
        SysFreeString(s); s = NULL;
        if (net2) INetwork_Release(net2);
        if (mgr2) INetworkListManager_Release(mgr2);
        INetwork_Release(net);
    }

    /* IDispatch */
    check(INetworkListManager_GetTypeInfoCount(mgr, &count) == S_OK && count == 1, "GetTypeInfoCount: 1");
    disp = (IDispatch *)mgr;
    hr = IDispatch_GetIDsOfNames(disp, &IID_NULL, &name, 1, 0, &dispid);
    check(hr == S_OK, "GetIDsOfNames(IsConnected)");
    VariantInit(&result);
    hr = IDispatch_Invoke(disp, dispid, &IID_NULL, 0, DISPATCH_PROPERTYGET, &params, &result, NULL, NULL);
    if (hr != S_OK || V_VT(&result) != VT_BOOL) printf("      hr %08lx vt %d dispid %ld\n", hr, V_VT(&result), dispid);
    check(hr == S_OK && V_VT(&result) == VT_BOOL && V_BOOL(&result) == VARIANT_TRUE, "Invoke: IsConnected is true");

    /* cost */
    if (SUCCEEDED(INetworkListManager_QueryInterface(mgr, &iid_cost_mgr, (void **)&cost_mgr)))
    {
        check(INetworkCostManager_GetCost(cost_mgr, &cost, NULL) == S_OK, "INetworkCostManager::GetCost");
        INetworkCostManager_Release(cost_mgr);
    }

    /* connection points and the event */
    INetworkListManager_QueryInterface(mgr, &IID_IConnectionPointContainer, (void **)&cpc);
    check(IConnectionPointContainer_EnumConnectionPoints(cpc, &ecp) == S_OK, "EnumConnectionPoints (was E_NOTIMPL)");
    if (ecp)
    {
        IConnectionPoint *cps[8];
        IEnumConnectionPoints_Next(ecp, 8, cps, &got);
        check(got == 4, "four connection points");
        while (got) IConnectionPoint_Release(cps[--got]);
        IEnumConnectionPoints_Release(ecp);
    }
    IConnectionPointContainer_FindConnectionPoint(cpc, &iid_nlm_events, &cp);
    check(IConnectionPoint_Advise(cp, (IUnknown *)&sink, &cookie) == S_OK, "Advise");
    check(IConnectionPoint_EnumConnections(cp, &ec) == S_OK && IEnumConnections_Next(ec, 1, &cd, &got) == S_OK &&
          cd.dwCookie == cookie, "EnumConnections: the sink (was E_NOTIMPL)");
    if (got) IUnknown_Release(cd.pUnk);
    if (ec) IEnumConnections_Release(ec);

    if (argc > 1)
    {
        FILE *f = fopen(argv[1], "w");
        if (f) fclose(f);
        /* the gate adds a default route now */
        check(WaitForSingleObject(changed_event, 20000) == WAIT_OBJECT_0 &&
              (last_connectivity & NLM_CONNECTIVITY_IPV4_INTERNET), "ConnectivityChanged: IPv4 internet");
        Sleep(500);
        if (SUCCEEDED(INetworkListManager_QueryInterface(mgr, &iid_cost_mgr, (void **)&cost_mgr)))
        {
            INetworkCostManager_GetCost(cost_mgr, &cost, NULL);
            check(cost == NLM_CONNECTION_COST_FIXED, "with the internet through a metered medium: NLM_CONNECTION_COST_FIXED");
            INetworkCostManager_Release(cost_mgr);
        }
    }
    check(IConnectionPoint_Unadvise(cp, cookie) == S_OK, "Unadvise");
    IConnectionPoint_Release(cp);
    IConnectionPointContainer_Release(cpc);
    INetworkListManager_Release(mgr);

done:
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    fflush(stdout);
    return failures != 0;
}
