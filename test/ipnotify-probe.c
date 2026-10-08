/* IP interface table and change notifications (patches/sg/1623). Run in a
 * network namespace of its own: after it prints READY, the gate adds an
 * address, a route and a link, and the callbacks must hear of each.
 *
 *  - GetIpInterfaceTable failed with ERROR_NOT_SUPPORTED; GetIpInterfaceEntry
 *    and InitializeIpInterfaceEntry were missing;
 *  - NotifyIpInterfaceChange and NotifyRouteChange2 never called back;
 *    NotifyUnicastIpAddressChange gave only the initial notification;
 *    CancelMibChangeNotify2 did nothing; NotifyRouteChange failed;
 *    NotifyStableUnicastIpAddressTable was missing.
 */
#include <winsock2.h>
#include <ws2ipdef.h>
#include <iphlpapi.h>
#include <netioapi.h>
#include <stdio.h>

typedef DWORD (WINAPI *get_if_table_t)(ADDRESS_FAMILY, MIB_IPINTERFACE_TABLE **);
typedef DWORD (WINAPI *get_if_entry_t)(MIB_IPINTERFACE_ROW *);
typedef void (WINAPI *init_if_entry_t)(MIB_IPINTERFACE_ROW *);
typedef DWORD (WINAPI *notify_t)(ADDRESS_FAMILY, void *, void *, BOOLEAN, HANDLE *);
typedef DWORD (WINAPI *stable_t)(ADDRESS_FAMILY, MIB_UNICASTIPADDRESS_TABLE **, void *, void *, HANDLE *);

static int failures;
static HANDLE addr_added, route_added, if_added;
static LONG initial_calls;

static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

static void WINAPI addr_cb(void *ctx, MIB_UNICASTIPADDRESS_ROW *row, MIB_NOTIFICATION_TYPE type)
{
    if (row) printf("note: address %d family %u %08lx\n", type, row->Address.si_family,
                    (unsigned long)ntohl(row->Address.Ipv4.sin_addr.s_addr));
    if (type == MibInitialNotification) { InterlockedIncrement(&initial_calls); return; }
    if (type == MibAddInstance && row && row->Address.si_family == AF_INET &&
        row->Address.Ipv4.sin_addr.s_addr == htonl(0x0a090807))
        SetEvent(addr_added);
}

static void WINAPI route_cb(void *ctx, MIB_IPFORWARD_ROW2 *row, MIB_NOTIFICATION_TYPE type)
{
    if (row) printf("note: route %d %08lx/%u\n", type,
                    (unsigned long)ntohl(row->DestinationPrefix.Prefix.Ipv4.sin_addr.s_addr), row->DestinationPrefix.PrefixLength);
    if (type == MibInitialNotification) { InterlockedIncrement(&initial_calls); return; }
    if (type == MibAddInstance && row && row->DestinationPrefix.PrefixLength == 16 &&
        row->DestinationPrefix.Prefix.Ipv4.sin_addr.s_addr == htonl(0x0a4d0000))
        SetEvent(route_added);
}

static void WINAPI if_cb(void *ctx, MIB_IPINTERFACE_ROW *row, MIB_NOTIFICATION_TYPE type)
{
    if (row) printf("note: interface %d family %u index %lu\n", type, row->Family, row->InterfaceIndex);
    if (type == MibInitialNotification) { InterlockedIncrement(&initial_calls); return; }
    if (type == MibAddInstance && row && row->Family == AF_INET) SetEvent(if_added);
}

int main(void)
{
    HMODULE ip = LoadLibraryA("iphlpapi.dll");
    get_if_table_t pGetIpInterfaceTable = (void *)GetProcAddress(ip, "GetIpInterfaceTable");
    get_if_entry_t pGetIpInterfaceEntry = (void *)GetProcAddress(ip, "GetIpInterfaceEntry");
    init_if_entry_t pInitializeIpInterfaceEntry = (void *)GetProcAddress(ip, "InitializeIpInterfaceEntry");
    notify_t pNotifyIpInterfaceChange = (void *)GetProcAddress(ip, "NotifyIpInterfaceChange");
    notify_t pNotifyRouteChange2 = (void *)GetProcAddress(ip, "NotifyRouteChange2");
    notify_t pNotifyUnicastIpAddressChange = (void *)GetProcAddress(ip, "NotifyUnicastIpAddressChange");
    stable_t pNotifyStableUnicastIpAddressTable = (void *)GetProcAddress(ip, "NotifyStableUnicastIpAddressTable");
    DWORD (WINAPI *pCancelMibChangeNotify2)(HANDLE) = (void *)GetProcAddress(ip, "CancelMibChangeNotify2");
    MIB_IPINTERFACE_TABLE *table = NULL;
    MIB_UNICASTIPADDRESS_TABLE *stable = NULL;
    MIB_IPINTERFACE_ROW row;
    HANDLE hn_addr = NULL, hn_route = NULL, hn_if = NULL, h_stable = (HANDLE)1, h_v1 = NULL;
    OVERLAPPED ovr;
    DWORD err, i, loopback = 0;

    if (!pGetIpInterfaceTable || !pGetIpInterfaceEntry || !pInitializeIpInterfaceEntry ||
        !pNotifyStableUnicastIpAddressTable)
    {
        check(0, "iphlpapi exports GetIpInterfaceTable/Entry, InitializeIpInterfaceEntry, NotifyStableUnicastIpAddressTable");
        printf("RESULT: FAIL\n");
        return 1;
    }

    /* the table */
    err = pGetIpInterfaceTable(AF_INET, &table);
    printf("GetIpInterfaceTable(AF_INET): %lu, %lu rows\n", err, table ? table->NumEntries : 0);
    check(!err && table && table->NumEntries >= 1, "GetIpInterfaceTable lists the IPv4 interfaces");
    if (table)
    {
        for (i = 0; i < table->NumEntries; i++)
        {
            MIB_IPINTERFACE_ROW *r = table->Table + i;
            printf("  if %lu family %u metric %lu mtu %lu connected %d\n", r->InterfaceIndex, r->Family,
                   r->Metric, r->NlMtu, r->Connected);
            if (r->Metric == 75) loopback = r->InterfaceIndex;
        }
        check(loopback != 0, "the loopback interface has metric 75");
        FreeMibTable(table);
    }
    err = pGetIpInterfaceTable(7, &table);
    check(err == ERROR_INVALID_PARAMETER, "a bad family is refused");

    memset(&row, 0, sizeof(row));
    row.Family = AF_INET;
    row.InterfaceIndex = loopback;
    err = pGetIpInterfaceEntry(&row);
    printf("GetIpInterfaceEntry(loopback): %lu metric %lu connected %d mtu %lu\n", err, row.Metric, row.Connected, row.NlMtu);
    check(!err && row.Metric == 75 && row.Connected && row.NlMtu > 0 && row.InterfaceLuid.Value,
          "GetIpInterfaceEntry by index fills the row");
    memset(&row, 0, sizeof(row));
    row.Family = AF_INET;
    row.InterfaceIndex = 12345;
    check(pGetIpInterfaceEntry(&row) == ERROR_NOT_FOUND, "... an unknown interface is ERROR_NOT_FOUND");

    pInitializeIpInterfaceEntry(&row);
    check(row.Family == AF_UNSPEC && row.Metric == ~0u && row.NlMtu == ~0u && row.SitePrefixLength == ~0u &&
          row.RouterDiscoveryBehavior == RouterDiscoveryUnchanged && row.UseAutomaticMetric == 0xff,
          "InitializeIpInterfaceEntry marks everything unchanged");

    err = pNotifyStableUnicastIpAddressTable(AF_UNSPEC, &stable, addr_cb, NULL, &h_stable);
    check(!err && stable && stable->NumEntries >= 1 && !h_stable, "NotifyStableUnicastIpAddressTable gives the table at once");
    if (stable) FreeMibTable(stable);

    /* notifications */
    addr_added = CreateEventA(NULL, TRUE, FALSE, NULL);
    route_added = CreateEventA(NULL, TRUE, FALSE, NULL);
    if_added = CreateEventA(NULL, TRUE, FALSE, NULL);
    err = pNotifyUnicastIpAddressChange(AF_INET, addr_cb, NULL, TRUE, &hn_addr);
    check(!err && hn_addr, "NotifyUnicastIpAddressChange");
    err = pNotifyRouteChange2(AF_INET, route_cb, NULL, TRUE, &hn_route);
    check(!err && hn_route, "NotifyRouteChange2");
    err = pNotifyIpInterfaceChange(AF_UNSPEC, if_cb, NULL, TRUE, &hn_if);
    check(!err && hn_if, "NotifyIpInterfaceChange");
    check(initial_calls == 3, "each gave its initial notification before returning");
    memset(&ovr, 0, sizeof(ovr));
    ovr.hEvent = CreateEventA(NULL, TRUE, FALSE, NULL);
    err = NotifyRouteChange(&h_v1, &ovr);
    check(err == ERROR_IO_PENDING, "NotifyRouteChange is pending");

    printf("READY\n");
    fflush(stdout);

    check(WaitForSingleObject(addr_added, 20000) == WAIT_OBJECT_0, "the new address 10.9.8.7 was notified");
    check(WaitForSingleObject(route_added, 20000) == WAIT_OBJECT_0, "the new route 10.77.0.0/16 was notified");
    check(WaitForSingleObject(if_added, 20000) == WAIT_OBJECT_0, "the new interface was notified");
    check(WaitForSingleObject(ovr.hEvent, 5000) == WAIT_OBJECT_0, "NotifyRouteChange completed");

    check(!pCancelMibChangeNotify2(hn_addr) && !pCancelMibChangeNotify2(hn_route) && !pCancelMibChangeNotify2(hn_if),
          "CancelMibChangeNotify2");

    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    fflush(stdout);
    return failures != 0;
}
