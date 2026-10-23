/* dnsupdate-probe: dnsapi dynamic update (Stained Glass OS wine-sg 6070), for test/dnsupdate-gate.sh, which runs it
 * against its own DNS server for sg.test and locked.test. Prints KEY=VALUE lines. */
#define UNICODE
#include <windows.h>
#include <windns.h>
#include <stdio.h>

static DNS_RECORDW *a_record( const WCHAR *name, const char *ip )
{
    DNS_RECORDW *r = calloc( 1, sizeof(*r) );
    r->pName = (WCHAR *)name;
    r->wType = DNS_TYPE_A;
    r->wDataLength = sizeof(DNS_A_DATA);
    r->dwTtl = 300;
    r->Flags.S.CharSet = DnsCharSetUnicode;
    r->Data.A.IpAddress = inet_addr( ip );
    return r;
}

/* the A records of the name, as "a.b.c.d,..." sorted by the server's order */
static void addresses( const WCHAR *name, char *out )
{
    DNS_RECORDW *list = NULL, *r;
    out[0] = 0;
    if (DnsQuery_W( name, DNS_TYPE_A, DNS_QUERY_BYPASS_CACHE, NULL, (DNS_RECORD **)&list, NULL )) { strcpy( out, "none" ); return; }
    for (r = list; r; r = r->pNext)
    {
        IN_ADDR a;
        if (r->wType != DNS_TYPE_A) continue;
        a.s_addr = r->Data.A.IpAddress;
        if (out[0]) strcat( out, "," );
        strcat( out, inet_ntoa( a ) );
    }
    DnsRecordListFree( (DNS_RECORD *)list, DnsFreeRecordList );
}

int main( void )
{
    const WCHAR *host = L"host.sg.test";
    DNS_RECORDW *a1 = a_record( host, "10.1.2.3" ), *a2 = a_record( host, "10.1.2.4" ), *a3 = a_record( host, "10.1.2.5" );
    DNS_RECORDW *a9 = a_record( host, "10.9.9.9" ), *other = a_record( L"other.sg.test", "10.0.0.1" );
    DNS_RECORDW *locked = a_record( L"x.locked.test", "10.0.0.2" ), *nowhere = a_record( L"a.nowhere.test", "10.0.0.3" );
    DNS_RECORDW srv = { 0 }, *list = NULL, *r;
    DNS_RECORDA ansi = { 0 };
    char buf[256];
    DNS_STATUS st;

    setvbuf( stdout, NULL, _IONBF, 0 );
    st = DnsModifyRecordsInSet_W( a1, NULL, DNS_UPDATE_SECURITY_OFF, NULL, NULL, NULL );
    addresses( host, buf );
    printf( "Add=%ld %s\n", st, buf );
    st = DnsModifyRecordsInSet_W( a2, NULL, 0, NULL, NULL, NULL );
    addresses( host, buf );
    printf( "AddSecond=%ld %s\n", st, buf );
    st = DnsModifyRecordsInSet_W( a3, a1, 0, NULL, NULL, NULL );
    addresses( host, buf );
    printf( "Modify=%ld %s\n", st, buf );
    st = DnsReplaceRecordSetW( a9, 0, NULL, NULL, NULL );
    addresses( host, buf );
    printf( "Replace=%ld %s\n", st, buf );
    st = DnsModifyRecordsInSet_W( NULL, a9, 0, NULL, NULL, NULL );
    addresses( host, buf );
    printf( "Delete=%ld %s\n", st, buf );

    srv.pName = (WCHAR *)L"_svc._tcp.sg.test";
    srv.wType = DNS_TYPE_SRV;
    srv.wDataLength = sizeof(DNS_SRV_DATAW);
    srv.dwTtl = 300;
    srv.Data.SRV.pNameTarget = (WCHAR *)L"host.sg.test";
    srv.Data.SRV.wPort = 8443;
    srv.Data.SRV.wPriority = 1;
    srv.Data.SRV.wWeight = 5;
    st = DnsModifyRecordsInSet_W( &srv, NULL, 0, NULL, NULL, NULL );
    printf( "AddSrv=%ld", st );
    if (!DnsQuery_W( srv.pName, DNS_TYPE_SRV, DNS_QUERY_BYPASS_CACHE, NULL, (DNS_RECORD **)&list, NULL ))
    {
        for (r = list; r; r = r->pNext)
            if (r->wType == DNS_TYPE_SRV)
                printf( " %u %u %u %ls", r->Data.SRV.wPort, r->Data.SRV.wPriority, r->Data.SRV.wWeight, r->Data.SRV.pNameTarget );
        DnsRecordListFree( (DNS_RECORD *)list, DnsFreeRecordList );
    }
    printf( "\n" );

    ansi.pName = (char *)"ansi.sg.test";
    ansi.wType = DNS_TYPE_A;
    ansi.wDataLength = sizeof(DNS_A_DATA);
    ansi.dwTtl = 300;
    ansi.Flags.S.CharSet = DnsCharSetAnsi;
    ansi.Data.A.IpAddress = inet_addr( "10.7.7.7" );
    st = DnsModifyRecordsInSet_A( (void *)&ansi, NULL, 0, NULL, NULL, NULL );
    addresses( L"ansi.sg.test", buf );
    printf( "AddAnsi=%ld %s\n", st, buf );

    a1->pNext = other;
    printf( "MixedNames=%ld\n", DnsModifyRecordsInSet_W( a1, NULL, 0, NULL, NULL, NULL ) );
    a1->pNext = NULL;
    printf( "Nothing=%ld\n", DnsModifyRecordsInSet_W( NULL, NULL, 0, NULL, NULL, NULL ) );
    printf( "Refused=%ld\n", DnsModifyRecordsInSet_W( locked, NULL, 0, NULL, NULL, NULL ) );
    printf( "NoZone=%ld\n", DnsModifyRecordsInSet_W( nowhere, NULL, 0, NULL, NULL, NULL ) );
    printf( "SecureOnly=%ld\n", DnsModifyRecordsInSet_W( a1, NULL, DNS_UPDATE_SECURITY_ONLY, NULL, NULL, NULL ) );
    printf( "done=1\n" );
    return 0;
}
