/* dnssd-probe: dnsapi DNS-SD over multicast DNS (Stained Glass OS wine-sg 6068), for test/dnssd-gate.sh.
 *   dnssd-probe INSTANCE TYPE   (the gate publishes INSTANCE.TYPE.local with avahi-publish)
 * Writes C:\found.flag once the browse found the instance and waits for its removal.
 * Prints KEY=VALUE lines. */
#define WIN32_LEAN_AND_MEAN
#define UNICODE
#include <windows.h>
#include <windns.h>
#include <stdio.h>
#include <string.h>

static WCHAR instance[256], type[64], full[400];
static HANDLE found_event, removed_event, cancelled_event, v2_event, resolved_event;
static int found_ok, srv_port, txt_ok, addr_ok, v2_ok, removed_ttl0;
static DWORD resolve_status, cancel_status;
static DNS_SERVICE_INSTANCE *resolved;

static void WINAPI browse_cb( DWORD status, void *ctx, DNS_RECORD *rec )
{
    DNS_RECORD *r;
    BOOL ours = FALSE;

    if (status == ERROR_CANCELLED) { cancel_status = status; SetEvent( cancelled_event ); return; }
    for (r = rec; r; r = r->pNext)
        if (r->wType == DNS_TYPE_PTR && !wcsicmp( r->Data.PTR.pNameHost, full ))
        {
            ours = TRUE;
            if (!r->dwTtl) { removed_ttl0 = 1; SetEvent( removed_event ); }
        }
    if (ours && !removed_ttl0 && !found_ok)
    {
        for (r = rec; r; r = r->pNext)
        {
            if (r->wType == DNS_TYPE_SRV) srv_port = r->Data.SRV.wPort;
            if (r->wType == DNS_TYPE_TEXT)
            {
                DWORD i;
                for (i = 0; i < r->Data.TXT.dwStringCount; i++)
                    if (!wcscmp( r->Data.TXT.pStringArray[i], L"k1=v1" )) txt_ok = 1;
            }
            if (r->wType == DNS_TYPE_A || r->wType == DNS_TYPE_AAAA) addr_ok = 1;
        }
        found_ok = 1;
        SetEvent( found_event );
    }
    DnsRecordListFree( rec, DnsFreeRecordList );
}

static void WINAPI browse_cb2( void *ctx, DNS_QUERY_RESULT *result )
{
    DNS_RECORD *r;
    if (result->QueryStatus) return;
    for (r = result->pQueryRecords; r; r = r->pNext)
        if (r->wType == DNS_TYPE_PTR && !wcsicmp( r->Data.PTR.pNameHost, full ) && r->dwTtl) { v2_ok = 1; SetEvent( v2_event ); }
    DnsRecordListFree( result->pQueryRecords, DnsFreeRecordList );
}

static HANDLE reg_event;
static DWORD reg_status;
static DNS_SERVICE_INSTANCE *reg_inst;
static void WINAPI register_cb( DWORD status, void *ctx, DNS_SERVICE_INSTANCE *inst )
{
    reg_status = status;
    reg_inst = inst;
    SetEvent( reg_event );
}

static void flag( const WCHAR *name )
{
    CloseHandle( CreateFileW( name, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL ) );
}

static void wait_flag( const WCHAR *name )
{
    int i;
    for (i = 0; i < 300 && GetFileAttributesW( name ) == INVALID_FILE_ATTRIBUTES; i++) Sleep( 100 );
}

/* dnssd-probe register NAME TYPE: advertises NAME.TYPE.local while the gate looks */
static int do_register( void )
{
    DNS_SERVICE_REGISTER_REQUEST rq = { 0 };
    DNS_SERVICE_CANCEL c = { 0 };
    const WCHAR *keys[1] = { L"sg" }, *values[1] = { L"yes" };
    DNS_SERVICE_INSTANCE *inst = DnsServiceConstructInstance( full, NULL, NULL, NULL, 4343, 0, 0, 1, keys, values );
    DWORD st, ok;

    reg_event = CreateEventW( NULL, FALSE, FALSE, NULL );
    printf( "RegisterNull=%lu\n", DnsServiceRegister( NULL, &c ) );
    rq.Version = DNS_QUERY_REQUEST_VERSION1;
    rq.pServiceInstance = inst;
    rq.pRegisterCompletionCallback = register_cb;
    st = DnsServiceRegister( &rq, &c );
    ok = WaitForSingleObject( reg_event, 15000 ) == WAIT_OBJECT_0;
    printf( "Register=%lu %lu %lu %d\n", st, ok, reg_status, reg_inst && !wcsicmp( reg_inst->pszInstanceName, full ) );
    DnsServiceFreeInstance( reg_inst );
    printf( "RegisterAgain=%lu\n", DnsServiceRegister( &rq, &c ) );
    flag( L"C:\\registered.flag" );
    wait_flag( L"C:\\checked.flag" );
    reg_inst = NULL;
    st = DnsServiceDeRegister( &rq, NULL );
    ok = WaitForSingleObject( reg_event, 15000 ) == WAIT_OBJECT_0;
    printf( "DeRegister=%lu %lu %lu %d\n", st, ok, reg_status, reg_inst != NULL );
    DnsServiceFreeInstance( reg_inst );
    printf( "DeRegisterAgain=%lu\n", DnsServiceDeRegister( &rq, NULL ) );
    flag( L"C:\\deregistered.flag" );
    wait_flag( L"C:\\checked2.flag" );
    /* registered again, then the process ends without deregistering */
    st = DnsServiceRegister( &rq, &c );
    ok = WaitForSingleObject( reg_event, 15000 ) == WAIT_OBJECT_0;
    printf( "RegisterExit=%lu %lu %lu\n", st, ok, reg_status );
    flag( L"C:\\registered2.flag" );
    wait_flag( L"C:\\checked3.flag" );
    printf( "done=1\n" );
    ExitProcess( 0 );
}

static HANDLE query_event;
static LONG query_calls;
static int query_srv, query_a;
static void WINAPI query_cb( void *ctx, MDNS_QUERY_HANDLE *handle, DNS_QUERY_RESULT *result )
{
    DNS_RECORD *r;

    /* a slow callback: it is still running when the probe stops the query */
    SetEvent( query_event );
    Sleep( 500 );
    if (result->QueryStatus) { InterlockedIncrement( &query_calls ); return; }
    for (r = result->pQueryRecords; r; r = r->pNext)
    {
        if (r->wType == DNS_TYPE_SRV && r->Data.SRV.wPort == 4242) query_srv = 1;
        if (r->wType == DNS_TYPE_A) query_a = 1;
    }
    DnsRecordListFree( result->pQueryRecords, DnsFreeRecordList );
    InterlockedIncrement( &query_calls );
}

static void WINAPI resolve_cb( DWORD status, void *ctx, DNS_SERVICE_INSTANCE *inst )
{
    resolve_status = status;
    resolved = inst;
    SetEvent( resolved_event );
}

int main( int argc, char **argv )
{
    DNS_SERVICE_BROWSE_REQUEST br = { 0 };
    DNS_SERVICE_RESOLVE_REQUEST rr = { 0 };
    DNS_SERVICE_CANCEL c1 = { 0 }, c2 = { 0 }, c3 = { 0 }, c4 = { 0 };
    DNS_SERVICE_INSTANCE *inst, *copy;
    WCHAR svc[128], missing[400];
    const WCHAR *keys[2] = { L"a", L"b" }, *values[2] = { L"1", L"2" };
    IP4_ADDRESS ip = 0x0100007f;
    DNS_STATUS st;
    DWORD i;

    setvbuf( stdout, NULL, _IONBF, 0 );
    if (argc < 3) return 1;
    if (argc == 4 && !strcmp( argv[1], "register" ))
    {
        MultiByteToWideChar( CP_ACP, 0, argv[2], -1, instance, 256 );
        MultiByteToWideChar( CP_ACP, 0, argv[3], -1, type, 64 );
        swprintf( full, 400, L"%ls.%ls.local", instance, type );
        return do_register();
    }
    MultiByteToWideChar( CP_ACP, 0, argv[1], -1, instance, 256 );
    MultiByteToWideChar( CP_ACP, 0, argv[2], -1, type, 64 );
    swprintf( full, 400, L"%ls.%ls.local", instance, type );
    swprintf( svc, 128, L"%ls.local", type );
    found_event = CreateEventW( NULL, TRUE, FALSE, NULL );
    removed_event = CreateEventW( NULL, TRUE, FALSE, NULL );
    cancelled_event = CreateEventW( NULL, TRUE, FALSE, NULL );
    v2_event = CreateEventW( NULL, TRUE, FALSE, NULL );
    resolved_event = CreateEventW( NULL, TRUE, FALSE, NULL );

    printf( "BrowseNull=%ld\n", DnsServiceBrowse( NULL, &c1 ) );
    br.Version = DNS_QUERY_REQUEST_VERSION1;
    br.QueryName = L"notaservice";
    br.pBrowseCallback = browse_cb;
    printf( "BrowseBadName=%ld\n", DnsServiceBrowse( &br, &c1 ) );
    br.QueryName = svc;
    st = DnsServiceBrowse( &br, &c1 );
    printf( "Browse=%ld\n", st );
    printf( "Found=%d\n", WaitForSingleObject( found_event, 10000 ) == WAIT_OBJECT_0 );
    printf( "Records=%d %d %d\n", srv_port, txt_ok, addr_ok );

    /* the V2 callback */
    br.Version = DNS_QUERY_REQUEST_VERSION2;
    br.pBrowseCallbackV2 = browse_cb2;
    st = DnsServiceBrowse( &br, &c2 );
    i = WaitForSingleObject( v2_event, 10000 ) == WAIT_OBJECT_0 && v2_ok;
    printf( "BrowseV2=%ld %lu\n", st, i );
    printf( "CancelV2=%ld\n", DnsServiceBrowseCancel( &c2 ) );

    /* resolving the instance */
    rr.Version = DNS_QUERY_REQUEST_VERSION1;
    rr.QueryName = full;
    rr.pResolveCompletionCallback = resolve_cb;
    st = DnsServiceResolve( &rr, &c3 );
    WaitForSingleObject( resolved_event, 15000 );
    printf( "Resolve=%ld %lu %d\n", st, resolve_status, resolved != NULL );
    if (resolved)
    {
        BOOL k1 = FALSE, flag = FALSE;
        for (i = 0; i < resolved->dwPropertyCount; i++)
        {
            if (!wcscmp( resolved->keys[i], L"k1" ) && !wcscmp( resolved->values[i], L"v1" )) k1 = TRUE;
            if (!wcscmp( resolved->keys[i], L"flag" ) && !resolved->values[i][0]) flag = TRUE;
        }
        printf( "Instance=%d %u %d %d %d\n", !wcsicmp( resolved->pszInstanceName, full ), resolved->wPort,
                resolved->pszHostName && wcschr( resolved->pszHostName, '.' ) != NULL,
                resolved->ip4Address || resolved->ip6Address, k1 && flag );
        DnsServiceFreeInstance( resolved );
    }
    /* an instance nobody publishes */
    ResetEvent( resolved_event );
    resolved = NULL;
    resolve_status = 0;
    swprintf( missing, 400, L"Nobody %ls.%ls.local", instance, type );
    rr.QueryName = missing;
    st = DnsServiceResolve( &rr, &c4 );
    WaitForSingleObject( resolved_event, 30000 );
    printf( "ResolveMissing=%ld %d %d\n", st, resolve_status != 0, resolved == NULL );
    rr.QueryName = (WCHAR *)L"_x._tcp.local";
    printf( "ResolveNoInstance=%ld\n", DnsServiceResolve( &rr, &c4 ) );

    /* the instances */
    inst = DnsServiceConstructInstance( L"x._y._tcp.local", L"h.local", &ip, NULL, 80, 1, 2, 2, keys, values );
    copy = DnsServiceCopyInstance( inst );
    printf( "Construct=%d %d %u %lu %d\n", inst != NULL, copy != NULL, copy ? copy->wPort : 0,
            copy ? copy->dwPropertyCount : 0, copy && !wcscmp( copy->values[1], L"2" ) && *copy->ip4Address == ip );
    DnsServiceFreeInstance( inst );
    DnsServiceFreeInstance( copy );

    /* multicast queries */
    {
        MDNS_QUERY_REQUEST q = { 0 };
        MDNS_QUERY_HANDLE h, h2;
        WCHAR host[256];
        DWORD len = 200;
        LONG calls;

        query_event = CreateEventW( NULL, FALSE, FALSE, NULL );
        q.Version = DNS_QUERY_REQUEST_VERSION1;
        q.Query = full;
        q.QueryType = DNS_TYPE_SRV;
        q.pQueryCallback = query_cb;
        st = DnsStartMulticastQuery( &q, &h );
        i = WaitForSingleObject( query_event, 10000 ) == WAIT_OBJECT_0;
        /* stopped while the callback runs: it returns once the callback is done */
        len = DnsStopMulticastQuery( &h );
        calls = query_calls;
        printf( "QuerySrv=%ld %lu %d %d\n", st, i, query_srv, h.wType == DNS_TYPE_SRV && !wcsicmp( h.nameBuf, full ) );
        printf( "QueryStop=%lu %d\n", len, calls >= 1 );
        Sleep( 1500 );
        printf( "QueryQuiet=%d\n", calls == query_calls );
        len = 200;
        GetComputerNameExW( ComputerNameDnsHostname, host, &len );
        wcscat( host, L".local" );
        q.Query = host;
        q.QueryType = DNS_TYPE_A;
        ResetEvent( query_event );
        st = DnsStartMulticastQuery( &q, &h2 );
        for (len = 0; len < 100 && !query_a; len++) Sleep( 100 );
        i = query_a;
        printf( "QueryA=%ld %lu %d\n", st, i, query_a );
        DnsStopMulticastQuery( &h2 );
        printf( "QueryStopAgain=%ld\n", DnsStopMulticastQuery( &h2 ) );
        printf( "QueryNull=%ld\n", DnsStartMulticastQuery( NULL, &h2 ) );
    }

    /* the instance goes away: the first browse sees a PTR with no time to live */
    CloseHandle( CreateFileW( L"C:\\found.flag", GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL ) );
    i = WaitForSingleObject( removed_event, 20000 ) == WAIT_OBJECT_0;
    printf( "Removed=%d\n", i && removed_ttl0 );
    st = DnsServiceBrowseCancel( &c1 );
    i = WaitForSingleObject( cancelled_event, 5000 ) == WAIT_OBJECT_0;
    Sleep( 200 );   /* the browse's thread finishes after its last callback */
    printf( "Cancel=%ld %lu %lu\n", st, i, cancel_status );
    printf( "CancelAgain=%ld\n", DnsServiceBrowseCancel( &c1 ) );
    printf( "done=1\n" );
    return 0;
}
