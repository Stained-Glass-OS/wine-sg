/* test/nlmproxy-gate.sh's probe (7758): the network list manager's event interfaces cross
 * apartments. netprofm calls its sinks through the global interface table (SG 1689), which needs a
 * proxy for each: INetworkCostManagerEvents had none ("CoMarshalInterface failed" at every Office
 * start, and cost events never reached their sinks).
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#define COBJMACROS
#include <windows.h>
#include <ocidl.h>
#include <initguid.h>
#include <netlistmgr.h>
#include <stdio.h>

static DWORD main_tid, call_tid;
static LONG calls;

static HRESULT WINAPI sink_QueryInterface(INetworkCostManagerEvents *iface, REFIID iid, void **out)
{
    if (IsEqualGUID(iid, &IID_IUnknown) || IsEqualGUID(iid, &IID_INetworkCostManagerEvents)) { *out = iface; return S_OK; }
    *out = NULL;
    return E_NOINTERFACE;
}
static ULONG WINAPI sink_AddRef(INetworkCostManagerEvents *iface) { return 2; }
static ULONG WINAPI sink_Release(INetworkCostManagerEvents *iface) { return 1; }
static HRESULT WINAPI sink_CostChanged(INetworkCostManagerEvents *iface, DWORD cost, NLM_SOCKADDR *addr)
{
    call_tid = GetCurrentThreadId();
    InterlockedIncrement(&calls);
    printf("CostChanged=%lu\n", cost);
    return S_OK;
}
static HRESULT WINAPI sink_DataPlanStatusChanged(INetworkCostManagerEvents *iface, NLM_SOCKADDR *addr) { return S_OK; }
static INetworkCostManagerEventsVtbl sink_vtbl = { sink_QueryInterface, sink_AddRef, sink_Release, sink_CostChanged, sink_DataPlanStatusChanged };
static INetworkCostManagerEvents sink = { &sink_vtbl };

static IStream *sink_stream, *nlm_stream;

static DWORD WINAPI mta_thread(void *arg)
{
    INetworkCostManagerEvents *proxy = NULL;
    INetworkListManager *nlm = NULL;
    NLM_CONNECTIVITY connectivity;
    HRESULT hr;

    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (sink_stream)
    {
        hr = CoGetInterfaceAndReleaseStream(sink_stream, &IID_INetworkCostManagerEvents, (void **)&proxy);
        printf("UnmarshalSink=0x%08lx\n", hr);
        if (SUCCEEDED(hr))
        {
            printf("CallSink=0x%08lx\n", INetworkCostManagerEvents_CostChanged(proxy, 3, NULL));
            INetworkCostManagerEvents_Release(proxy);
        }
    }
    if (nlm_stream)
    {
        hr = CoGetInterfaceAndReleaseStream(nlm_stream, &IID_INetworkListManager, (void **)&nlm);
        printf("UnmarshalNlm=0x%08lx\n", hr);
        if (SUCCEEDED(hr))
        {
            printf("GetConnectivity=0x%08lx\n", INetworkListManager_GetConnectivity(nlm, &connectivity));
            INetworkListManager_Release(nlm);
        }
    }
    CoUninitialize();
    return 0;
}

int main(void)
{
    INetworkListManager *nlm = NULL;
    IConnectionPointContainer *cpc;
    IConnectionPoint *cp;
    HANDLE thread;
    DWORD cookie = 0;
    HRESULT hr;
    MSG msg;

    main_tid = GetCurrentThreadId();
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    hr = CoMarshalInterThreadInterfaceInStream(&IID_INetworkCostManagerEvents, (IUnknown *)&sink, &sink_stream);
    printf("MarshalSink=0x%08lx\n", hr);
    if (FAILED(hr)) sink_stream = NULL;
    hr = CoCreateInstance(&CLSID_NetworkListManager, NULL, CLSCTX_INPROC_SERVER, &IID_INetworkListManager, (void **)&nlm);
    printf("Create=0x%08lx\n", hr);
    if (SUCCEEDED(hr))
    {
        hr = CoMarshalInterThreadInterfaceInStream(&IID_INetworkListManager, (IUnknown *)nlm, &nlm_stream);
        printf("MarshalNlm=0x%08lx\n", hr);
        if (FAILED(hr)) nlm_stream = NULL;
        if (SUCCEEDED(INetworkListManager_QueryInterface(nlm, &IID_IConnectionPointContainer, (void **)&cpc)))
        {
            if (SUCCEEDED(IConnectionPointContainer_FindConnectionPoint(cpc, &IID_INetworkCostManagerEvents, &cp)))
            {
                printf("Advise=0x%08lx\n", IConnectionPoint_Advise(cp, (IUnknown *)&sink, &cookie));
                IConnectionPoint_Unadvise(cp, cookie);
                IConnectionPoint_Release(cp);
            }
            IConnectionPointContainer_Release(cpc);
        }
    }
    thread = CreateThread(NULL, 0, mta_thread, NULL, 0, NULL);
    while (MsgWaitForMultipleObjects(1, &thread, FALSE, 10000, QS_ALLINPUT) == WAIT_OBJECT_0 + 1)
        while (PeekMessageW(&msg, 0, 0, 0, PM_REMOVE)) DispatchMessageW(&msg);
    printf("SinkCalls=%ld\n", calls);
    printf("SinkOnOwnThread=%d\n", calls && call_tid == main_tid);
    if (nlm) INetworkListManager_Release(nlm);
    CoUninitialize();
    return 0;
}
