/* ncsi-probe: the Network List Manager's internet connectivity follows the NCSI probes (patches/sg/9661).
 * The gate (test/ncsi-gate.sh) serves the probe page and changes what it answers; between phases this
 * probe touches DIR\ready-N and the gate answers DIR\go-N once the page and the network are changed.
 * One manager lives through all phases: its connectivity and its ConnectivityChanged events must follow.
 *   ncsi-probe Z:\path\to\dir */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <netlistmgr.h>
#include <ocidl.h>
#include <stdio.h>

static const IID iid_nlm_events = { 0xdcb00001, 0x570f, 0x4a9b, { 0x8d, 0x69, 0x19, 0x9f, 0xdb, 0xa5, 0x72, 0x3b } };
static int failures;
static HANDLE changed_event;
static volatile LONG last = -1, events;
static HRESULT WINAPI sink_QueryInterface(INetworkListManagerEvents *iface, REFIID riid, void **ppv)
{
    if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &iid_nlm_events)) { *ppv = iface; return S_OK; }
    *ppv = NULL;
    return E_NOINTERFACE;
}
static ULONG WINAPI sink_AddRef(INetworkListManagerEvents *iface) { return 2; }
static ULONG WINAPI sink_Release(INetworkListManagerEvents *iface) { return 1; }
static HRESULT WINAPI sink_ConnectivityChanged(INetworkListManagerEvents *iface, NLM_CONNECTIVITY conn)
{
    last = conn; InterlockedIncrement(&events); SetEvent(changed_event);
    return S_OK;
}
static INetworkListManagerEventsVtbl sink_vtbl = { sink_QueryInterface, sink_AddRef, sink_Release, sink_ConnectivityChanged };
static INetworkListManagerEvents sink = { &sink_vtbl };
static char dir[MAX_PATH];

static void step(int n)
{
    char path[MAX_PATH];
    FILE *f;
    int i;
    snprintf(path, sizeof(path), "%s\\ready-%d", dir, n);
    if ((f = fopen(path, "w"))) fclose(f);
    snprintf(path, sizeof(path), "%s\\go-%d", dir, n);
    for (i = 0; i < 600 && GetFileAttributesA(path) == INVALID_FILE_ATTRIBUTES; i++) Sleep(100);
}

/* waits up to 40 s for the manager to say internet (want) or not; prints the outcome */
static void expect(INetworkListManager *mgr, BOOL want, const char *what)
{
    NLM_CONNECTIVITY c = 0;
    DWORD start = GetTickCount();
    LONG ev = events;
    BOOL ok = FALSE;
    while (GetTickCount() - start < 40000)
    {
        INetworkListManager_GetConnectivity(mgr, &c);
        if (!!(c & NLM_CONNECTIVITY_IPV4_INTERNET) == want && (want || (c & NLM_CONNECTIVITY_IPV4_LOCALNETWORK))) { ok = TRUE; break; }
        WaitForSingleObject(changed_event, 500);
    }
    printf("%s  %s (connectivity %#x)\n", ok ? "PASS" : "FAIL", what, c);
    if (!ok) failures++;
    if (ok && events == ev) { printf("FAIL  ... but no ConnectivityChanged event told it\n"); failures++; }
    fflush(stdout);
}

int main(int argc, char **argv)
{
    INetworkListManager *mgr;
    IConnectionPointContainer *cpc;
    IConnectionPoint *cp;
    NLM_CONNECTIVITY c;
    DWORD cookie;

    if (argc < 2) return 2;
    lstrcpynA(dir, argv[1], sizeof(dir));
    changed_event = CreateEventW(NULL, FALSE, FALSE, NULL);
    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (CoCreateInstance(&CLSID_NetworkListManager, NULL, CLSCTX_ALL, &IID_INetworkListManager, (void **)&mgr))
    {
        printf("FAIL  no network list manager\n");
        return 1;
    }
    INetworkListManager_QueryInterface(mgr, &IID_IConnectionPointContainer, (void **)&cpc);
    IConnectionPointContainer_FindConnectionPoint(cpc, &iid_nlm_events, &cp);
    IConnectionPoint_Advise(cp, (IUnknown *)&sink, &cookie);

    /* phase 1: the probe page answers as it should */
    INetworkListManager_GetConnectivity(mgr, &c);
    events = 0; step(1);
    {
        NLM_CONNECTIVITY c1 = 0; DWORD start = GetTickCount();
        while (GetTickCount() - start < 40000 && !(c1 & NLM_CONNECTIVITY_IPV4_INTERNET))
        { INetworkListManager_GetConnectivity(mgr, &c1); Sleep(300); }
        printf("%s  the probe page answers: IPv4 internet (connectivity %#x)\n", c1 & NLM_CONNECTIVITY_IPV4_INTERNET ? "PASS" : "FAIL", c1);
        if (!(c1 & NLM_CONNECTIVITY_IPV4_INTERNET)) failures++;
    }
    step(2);  /* a captive portal: another page */
    expect(mgr, FALSE, "a captive portal's page: the local network, no internet");
    step(3);  /* the portal signed in to: the page again */
    expect(mgr, TRUE, "the page again: the internet again");
    step(4);  /* nothing answers */
    expect(mgr, FALSE, "nothing answers: no internet");
    step(5);  /* probing turned off (EnableActiveProbing 0): the default route decides */
    expect(mgr, TRUE, "probing off: the default route is the internet, as before");
    step(6);
    IConnectionPoint_Unadvise(cp, cookie);
    INetworkListManager_Release(mgr);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
