/* wsdprovider-probe.c: IWSDiscoveryProvider (WS-Discovery client side) against test/wsdprovider-device.py and
 * Wine's own IWSDiscoveryPublisher, for test/wsdprovider-gate.sh.
 *   wsdprovider-probe.exe HTTP_PORT CONTROL_PORT FLAGDIR
 */
#define COBJMACROS
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
/* mingw's wsdapi.h includes wsdtypes.h twice; this order builds */
#include <wsdxml.h>
#include <wsdbase.h>
typedef interface IWSDServiceMessaging IWSDServiceMessaging;
#include <wsddisco.h>
#include <stdio.h>

static const WCHAR device_epr[] = L"urn:uuid:5e1d2c3b-0000-4000-8000-00000000c0de";
static const WCHAR types_ns[] = L"http://sg.test/wsd";
static const WCHAR wine_epr[] = L"urn:uuid:5e1d2c3b-0000-4000-8000-0000000041e5";

struct event
{
    char kind;      /* A(dd) R(emove) C(omplete) F(ailed) */
    WCHAR tag[32];
    WCHAR address[128];
    HRESULT hr;
    int types, scopes, xaddrs;
    WCHAR first_type[64], first_xaddr[128], remote[64];
    ULONGLONG metadata, instance;
    BOOL type_ns_ok;
};

static CRITICAL_SECTION cs;
static struct event events[256];
static int event_count;

static void record(char kind, IWSDiscoveredService *service, HRESULT hr, LPCWSTR tag)
{
    struct event *e;

    EnterCriticalSection(&cs);
    if (event_count == ARRAYSIZE(events)) { LeaveCriticalSection(&cs); return; }
    e = &events[event_count];
    memset(e, 0, sizeof(*e));
    e->kind = kind;
    e->hr = hr;
    if (tag) lstrcpynW(e->tag, tag, ARRAYSIZE(e->tag));
    if (service)
    {
        WSD_ENDPOINT_REFERENCE *epr = NULL;
        WSD_NAME_LIST *types = NULL, *t;
        WSD_URI_LIST *scopes = NULL, *xaddrs = NULL, *u;
        LPCWSTR s = NULL;

        if (SUCCEEDED(IWSDiscoveredService_GetEndpointReference(service, &epr)) && epr && epr->Address)
            lstrcpynW(e->address, epr->Address, ARRAYSIZE(e->address));
        if (SUCCEEDED(IWSDiscoveredService_GetTypes(service, &types)))
        {
            for (t = types; t; t = t->Next) e->types++;
            if (types && types->Element)
            {
                lstrcpynW(e->first_type, types->Element->LocalName, ARRAYSIZE(e->first_type));
                e->type_ns_ok = types->Element->Space && types->Element->Space->Uri &&
                        !lstrcmpW(types->Element->Space->Uri, types_ns);
            }
        }
        if (SUCCEEDED(IWSDiscoveredService_GetScopes(service, &scopes))) for (u = scopes; u; u = u->Next) e->scopes++;
        if (SUCCEEDED(IWSDiscoveredService_GetXAddrs(service, &xaddrs)))
        {
            for (u = xaddrs; u; u = u->Next) e->xaddrs++;
            if (xaddrs) lstrcpynW(e->first_xaddr, xaddrs->Element, ARRAYSIZE(e->first_xaddr));
        }
        IWSDiscoveredService_GetMetadataVersion(service, &e->metadata);
        IWSDiscoveredService_GetInstanceId(service, &e->instance);
        if (SUCCEEDED(IWSDiscoveredService_GetProbeResolveTag(service, &s)) && s && !e->tag[0])
            lstrcpynW(e->tag, s, ARRAYSIZE(e->tag));
        s = NULL;
        if (SUCCEEDED(IWSDiscoveredService_GetRemoteTransportAddress(service, &s)) && s)
            lstrcpynW(e->remote, s, ARRAYSIZE(e->remote));
    }
    event_count++;
    LeaveCriticalSection(&cs);
}

/* the notification sink */
static HRESULT WINAPI sink_QueryInterface(IWSDiscoveryProviderNotify *iface, REFIID riid, void **out)
{
    *out = iface;
    return S_OK;
}
static ULONG WINAPI sink_AddRef(IWSDiscoveryProviderNotify *iface) { return 2; }
static ULONG WINAPI sink_Release(IWSDiscoveryProviderNotify *iface) { return 1; }
static HRESULT WINAPI sink_Add(IWSDiscoveryProviderNotify *iface, IWSDiscoveredService *service)
{
    record('A', service, S_OK, NULL);
    return S_OK;
}
static HRESULT WINAPI sink_Remove(IWSDiscoveryProviderNotify *iface, IWSDiscoveredService *service)
{
    record('R', service, S_OK, NULL);
    return S_OK;
}
static HRESULT WINAPI sink_SearchFailed(IWSDiscoveryProviderNotify *iface, HRESULT hr, LPCWSTR tag)
{
    record('F', NULL, hr, tag);
    return S_OK;
}
static HRESULT WINAPI sink_SearchComplete(IWSDiscoveryProviderNotify *iface, LPCWSTR tag)
{
    record('C', NULL, S_OK, tag);
    return S_OK;
}
static IWSDiscoveryProviderNotifyVtbl sink_vtbl =
{
    sink_QueryInterface, sink_AddRef, sink_Release, sink_Add, sink_Remove, sink_SearchFailed, sink_SearchComplete
};
static IWSDiscoveryProviderNotify sink = { &sink_vtbl };

/* the Wine publisher's sink: answers probes for its types */
static IWSDiscoveryPublisher *publisher;
static WSD_NAME_LIST camera_types;
static LONG untyped_probes;

static HRESULT WINAPI pub_QueryInterface(IWSDiscoveryPublisherNotify *iface, REFIID riid, void **out)
{
    *out = iface;
    return S_OK;
}
static ULONG WINAPI pub_AddRef(IWSDiscoveryPublisherNotify *iface) { return 2; }
static ULONG WINAPI pub_Release(IWSDiscoveryPublisherNotify *iface) { return 1; }
static HRESULT WINAPI pub_ProbeHandler(IWSDiscoveryPublisherNotify *iface, const WSD_SOAP_MESSAGE *msg,
    IWSDMessageParameters *params)
{
    const WSD_PROBE *probe = msg->Body;

    if (probe && !probe->Types) InterlockedIncrement(&untyped_probes);
    if (probe && probe->Types && probe->Types->Element &&
        !lstrcmpW(probe->Types->Element->LocalName, L"Camera"))
        IWSDiscoveryPublisher_MatchProbe(publisher, msg, params, wine_epr, 3, 1, 1, NULL, &camera_types, NULL, NULL);
    return S_OK;
}
static HRESULT WINAPI pub_ResolveHandler(IWSDiscoveryPublisherNotify *iface, const WSD_SOAP_MESSAGE *msg,
    IWSDMessageParameters *params)
{
    return S_OK;
}
static IWSDiscoveryPublisherNotifyVtbl pub_vtbl =
{
    pub_QueryInterface, pub_AddRef, pub_Release, pub_ProbeHandler, pub_ResolveHandler
};
static IWSDiscoveryPublisherNotify pub_sink = { &pub_vtbl };

static const struct event *find_event(char kind, LPCWSTR tag, LPCWSTR address)
{
    const struct event *ret = NULL;
    int i;

    EnterCriticalSection(&cs);
    for (i = 0; i < event_count && !ret; i++)
        if (events[i].kind == kind && (!tag || !lstrcmpW(events[i].tag, tag)) &&
            (!address || !lstrcmpW(events[i].address, address)))
            ret = &events[i];
    LeaveCriticalSection(&cs);
    return ret;
}

static int count_events(char kind, LPCWSTR tag)
{
    int i, n = 0;

    EnterCriticalSection(&cs);
    for (i = 0; i < event_count; i++)
        if (events[i].kind == kind && (!tag || !lstrcmpW(events[i].tag, tag))) n++;
    LeaveCriticalSection(&cs);
    return n;
}

/* waits for the search with this tag to end, in ms (or -1) */
static int wait_end(LPCWSTR tag, DWORD timeout)
{
    DWORD start = GetTickCount();

    while (GetTickCount() - start < timeout)
    {
        if (find_event('C', tag, NULL) || find_event('F', tag, NULL)) return GetTickCount() - start;
        Sleep(50);
    }
    return -1;
}

static const struct event *wait_event(char kind, LPCWSTR tag, LPCWSTR address, DWORD timeout)
{
    DWORD start = GetTickCount();
    const struct event *e;

    while (!(e = find_event(kind, tag, address)) && GetTickCount() - start < timeout) Sleep(50);
    return e;
}

static void control(int port, const char *what)
{
    SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    struct sockaddr_in addr = { AF_INET };

    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    sendto(s, what, strlen(what), 0, (struct sockaddr *)&addr, sizeof(addr));
    closesocket(s);
}

/* tells the gate where we are and waits for it to have looked */
static void flag_and_wait(const char *dir, const char *name)
{
    char path[MAX_PATH], ack[MAX_PATH];
    FILE *f;
    int i;

    sprintf(path, "%s\\%s.flag", dir, name);
    sprintf(ack, "%s\\%s.ack", dir, name);
    if ((f = fopen(path, "w"))) fclose(f);
    for (i = 0; i < 200 && GetFileAttributesA(ack) == INVALID_FILE_ATTRIBUTES; i++) Sleep(50);
}

static void print_add(const char *key, const struct event *e)
{
    if (!e) { printf("%s=none\n", key); return; }
    printf("%s=%ls %d %ls %d %d %ls %u %u %d\n", key, e->address, e->types, e->first_type, e->type_ns_ok,
           e->scopes, e->first_xaddr, (unsigned)e->metadata, (unsigned)e->instance, e->remote[0] != 0);
}

int main(int argc, char **argv)
{
    IWSDiscoveryProvider *provider = NULL, *provider2 = NULL;
    IWSDXMLContext *context = NULL, *context2 = NULL;
    WSDXML_NAME *printer = NULL, *fax = NULL, *camera = NULL;
    WSD_NAME_LIST printer_list = { 0 }, fax_list = { 0 };
    WSD_URI_LIST scope_prefix = { 0 }, scope_exact = { 0 }, scope_partial = { 0 };
    WCHAR url[128], url404[128], longtag[8200];
    const struct event *e;
    int http_port, control_port, ms, i;
    const char *flagdir;
    WSADATA wsa;
    HRESULT hr, hr2;

    if (argc < 4) return 2;
    http_port = atoi(argv[1]);
    control_port = atoi(argv[2]);
    flagdir = argv[3];
    InitializeCriticalSection(&cs);
    WSAStartup(MAKEWORD(2, 2), &wsa);
    setvbuf(stdout, NULL, _IONBF, 0);
    CoInitializeEx(NULL, COINIT_MULTITHREADED);

    hr = WSDCreateDiscoveryProvider(NULL, NULL);
    printf("CreateNull=0x%08lx\n", hr);
    hr = WSDCreateDiscoveryProvider(NULL, &provider);
    printf("Create=0x%08lx %d\n", hr, provider != NULL);
    if (!provider) return 1;
    hr = IWSDiscoveryProvider_GetXMLContext(provider, &context);
    printf("Context=0x%08lx %d\n", hr, context != NULL);

    IWSDXMLContext_AddNameToNamespace(context, types_ns, L"Printer", &printer);
    IWSDXMLContext_AddNameToNamespace(context, types_ns, L"Fax", &fax);
    IWSDXMLContext_AddNameToNamespace(context, types_ns, L"Camera", &camera);
    printer_list.Element = printer;
    fax_list.Element = fax;
    camera_types.Element = camera;
    scope_prefix.Element = L"http://sg.test/scope";
    scope_exact.Element = L"http://sg.test/scope/lobby";
    scope_partial.Element = L"http://sg.test/scope";

    hr = IWSDiscoveryProvider_SearchByType(provider, &printer_list, NULL, NULL, L"early");
    printf("SearchUnattached=0x%08lx\n", hr);
    hr = IWSDiscoveryProvider_Attach(provider, NULL);
    printf("AttachNull=0x%08lx\n", hr);
    hr = IWSDiscoveryProvider_SetAddressFamily(provider, 7);
    printf("FamilyBad=0x%08lx\n", hr);
    hr = IWSDiscoveryProvider_SetAddressFamily(provider, WSDAPI_ADDRESSFAMILY_IPV4);
    printf("Family=0x%08lx\n", hr);
    hr = IWSDiscoveryProvider_Attach(provider, &sink);
    hr2 = IWSDiscoveryProvider_Attach(provider, &sink);
    printf("Attach=0x%08lx 0x%08lx\n", hr, hr2);
    hr = IWSDiscoveryProvider_SetAddressFamily(provider, WSDAPI_ADDRESSFAMILY_IPV4);
    printf("FamilyAttached=0x%08lx\n", hr);

    /* bad arguments */
    for (i = 0; i < 8199; i++) longtag[i] = 'x';
    longtag[8199] = 0;
    hr = IWSDiscoveryProvider_SearchById(provider, NULL, NULL);
    hr2 = IWSDiscoveryProvider_SearchByType(provider, &printer_list, NULL, NULL, longtag);
    printf("BadArgs=0x%08lx 0x%08lx", hr, hr2);
    hr = IWSDiscoveryProvider_SearchByAddress(provider, L"ftp://127.0.0.1/dev", NULL);
    hr2 = IWSDiscoveryProvider_SearchByType(provider, NULL, NULL, L"http://schemas.xmlsoap.org/ws/2005/04/discovery/strcmp0", NULL);
    printf(" 0x%08lx 0x%08lx\n", hr, hr2);

    /* the searches run at once, told apart by their tags */
    swprintf(url, ARRAYSIZE(url), L"http://127.0.0.1:%d/dev", http_port);
    swprintf(url404, ARRAYSIZE(url404), L"http://127.0.0.1:%d/none", http_port);
    hr = IWSDiscoveryProvider_SearchByType(provider, &printer_list, NULL, NULL, L"type");
    printf("SearchType=0x%08lx\n", hr);
    hr = IWSDiscoveryProvider_SearchByType(provider, &fax_list, NULL, NULL, L"nofax");
    printf("SearchNoMatch=0x%08lx\n", hr);
    hr = IWSDiscoveryProvider_SearchByType(provider, NULL, &scope_prefix, NULL, L"scope");
    hr2 = IWSDiscoveryProvider_SearchByType(provider, NULL, &scope_partial,
            L"http://schemas.xmlsoap.org/ws/2005/04/discovery/strcmp0", L"strcmp-partial");
    printf("SearchScopes=0x%08lx 0x%08lx", hr, hr2);
    hr = IWSDiscoveryProvider_SearchByType(provider, NULL, &scope_exact,
            L"http://schemas.xmlsoap.org/ws/2005/04/discovery/strcmp0", L"strcmp-exact");
    printf(" 0x%08lx\n", hr);
    hr = IWSDiscoveryProvider_SearchById(provider, device_epr, L"resolve");
    hr2 = IWSDiscoveryProvider_SearchById(provider, L"urn:uuid:5e1d2c3b-0000-4000-8000-0000000000ff", L"resolve-none");
    printf("SearchId=0x%08lx 0x%08lx\n", hr, hr2);
    hr = IWSDiscoveryProvider_SearchByAddress(provider, url, L"directed");
    hr2 = IWSDiscoveryProvider_SearchByAddress(provider, url404, L"directed-404");
    printf("SearchAddress=0x%08lx 0x%08lx\n", hr, hr2);

    ms = wait_end(L"resolve", 9000);
    printf("ResolveFast=%d\n", ms >= 0 && ms < 5000);
    print_add("ResolveAdd", find_event('A', L"resolve", NULL));
    printf("ResolveEnd=%d %d\n", find_event('C', L"resolve", NULL) != NULL, count_events('A', L"resolve"));
    ms = wait_end(L"directed", 9000);
    printf("DirectedFast=%d\n", ms >= 0 && ms < 5000);
    print_add("DirectedAdd", find_event('A', L"directed", NULL));
    printf("DirectedEnd=%d %d\n", find_event('C', L"directed", NULL) != NULL, count_events('A', L"directed"));
    wait_end(L"directed-404", 25000);
    e = find_event('F', L"directed-404", NULL);
    printf("Directed404=%d 0x%08lx %d\n", e != NULL, e ? e->hr : 0, count_events('A', L"directed-404"));

    ms = wait_end(L"type", 15000);
    printf("TypeTime=%d\n", ms >= 8000 && ms < 13000);
    print_add("TypeAdd", find_event('A', L"type", NULL));
    printf("TypeEnd=%d %d %d\n", find_event('C', L"type", NULL) != NULL, count_events('A', L"type"),
           count_events('F', L"type"));
    printf("Decoy=%d\n", find_event('A', NULL, L"urn:uuid:not-for-you") != NULL);
    wait_end(L"nofax", 15000);
    e = find_event('F', L"nofax", NULL);
    printf("NoMatch=%d 0x%08lx %d %d\n", e != NULL, e ? e->hr : 0, count_events('A', L"nofax"),
           count_events('C', L"nofax"));
    wait_end(L"scope", 15000);
    wait_end(L"strcmp-partial", 15000);
    wait_end(L"strcmp-exact", 15000);
    printf("Scopes=%d %d %d\n", find_event('C', L"scope", NULL) != NULL,
           find_event('F', L"strcmp-partial", NULL) != NULL, find_event('C', L"strcmp-exact", NULL) != NULL);
    wait_end(L"resolve-none", 15000);
    e = find_event('F', L"resolve-none", NULL);
    printf("ResolveNone=%d 0x%08lx\n", e != NULL, e ? e->hr : 0);

    /* announcements */
    control(control_port, "hello");
    e = wait_event('A', L"", device_epr, 5000);   /* an untagged Add is the Hello */
    print_add("Hello", e);
    control(control_port, "bye");
    e = wait_event('R', NULL, device_epr, 5000);
    printf("Bye=%d %d\n", e != NULL, e ? e->types : -1);

    /* Wine's own publisher: its Hello is heard, and it answers a search */
    hr = WSDCreateDiscoveryPublisher(context, &publisher);
    hr2 = publisher ? IWSDiscoveryPublisher_SetAddressFamily(publisher, WSDAPI_ADDRESSFAMILY_IPV4) : E_FAIL;
    if (publisher) IWSDiscoveryPublisher_RegisterNotificationSink(publisher, &pub_sink);
    if (publisher) hr2 = IWSDiscoveryPublisher_Publish(publisher, wine_epr, 3, 1, 1, NULL, &camera_types, NULL, NULL);
    e = wait_event('A', NULL, wine_epr, 6000);
    printf("WineHello=%d %ls\n", e != NULL, e ? e->first_type : L"-");
    hr = IWSDiscoveryProvider_SearchByType(provider, &camera_types, NULL, NULL, L"wine");
    wait_end(L"wine", 15000);
    e = find_event('A', L"wine", wine_epr);
    printf("WineSearch=0x%08lx %d %d\n", hr, e != NULL, find_event('C', L"wine", NULL) != NULL);
    /* a Probe without Types (scopes only) reaches the publisher's handler too */
    IWSDiscoveryProvider_SearchByType(provider, NULL, &scope_exact, NULL, L"wine-untyped");
    for (i = 0; i < 60 && !untyped_probes; i++) Sleep(50);
    printf("WineUntyped=%d\n", untyped_probes > 0);
    if (publisher)
    {
        IWSDiscoveryPublisher_UnPublish(publisher, wine_epr, 1, 2, NULL, NULL);
        e = wait_event('R', NULL, wine_epr, 6000);
        printf("WineBye=%d\n", e != NULL);
        IWSDiscoveryPublisher_UnRegisterNotificationSink(publisher, &pub_sink);
        IWSDiscoveryPublisher_Release(publisher);
    }

    flag_and_wait(flagdir, "attached");

    /* Detach: no more announcements, the listener goes */
    hr = IWSDiscoveryProvider_Detach(provider);
    hr2 = IWSDiscoveryProvider_Detach(provider);
    printf("Detach=0x%08lx 0x%08lx\n", hr, hr2);
    i = count_events('A', NULL);
    control(control_port, "hello");
    Sleep(1500);
    printf("QuietAfterDetach=%d\n", count_events('A', NULL) == i);
    hr = IWSDiscoveryProvider_SearchByType(provider, &printer_list, NULL, NULL, L"late");
    printf("SearchDetached=0x%08lx\n", hr);

    /* a provider released while attached stops listening too */
    WSDCreateDiscoveryProvider(NULL, &provider2);
    IWSDiscoveryProvider_SetAddressFamily(provider2, WSDAPI_ADDRESSFAMILY_IPV4);
    hr = IWSDiscoveryProvider_Attach(provider2, &sink);
    IWSDiscoveryProvider_GetXMLContext(provider2, &context2);
    printf("Attach2=0x%08lx\n", hr);
    IWSDiscoveryProvider_Release(provider2);
    if (context2) IWSDXMLContext_Release(context2);
    flag_and_wait(flagdir, "detached");

    /* reattaching works */
    hr = IWSDiscoveryProvider_Attach(provider, &sink);
    hr2 = IWSDiscoveryProvider_SearchById(provider, device_epr, L"again");
    ms = wait_end(L"again", 9000);
    printf("Reattach=0x%08lx 0x%08lx %d\n", hr, hr2, find_event('C', L"again", NULL) != NULL);
    IWSDiscoveryProvider_Detach(provider);

    IWSDiscoveryProvider_Release(provider);
    IWSDXMLContext_Release(context);
    printf("done=1\n");
    return 0;
}
