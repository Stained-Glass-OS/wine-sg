/* iphlpapi lookups, small tables, initialisers, IP error strings,
 * compartments, owner modules and SendARP (patches/sg/2406), run by
 * test/iphlpapient-gate.sh. All of GetIpForwardEntry2, GetIpNetEntry2,
 * Get{Multicast,Anycast}IpAddressEntry, GetMulticastIpAddressTable,
 * GetIpPathTable/Entry, Get{,Inverted}IfStackTable, InitializeIpForwardEntry,
 * InitializeUnicastIpAddressEntry, GetIpErrorString,
 * Get/SetSessionCompartmentId, GetOwnerModuleFrom*Entry, ...PidAndInfo and
 * SendARP were unimplemented. */
#include <winsock2.h>
#include <ws2ipdef.h>
#include <windows.h>
#include <iphlpapi.h>
#include <netioapi.h>
#include <ipifcons.h>
#include <stdio.h>

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

typedef DWORD (WINAPI *row_fn)(void *);
typedef DWORD (WINAPI *tbl_fn)(ADDRESS_FAMILY, void **);
typedef DWORD (WINAPI *stack_fn)(void **);
typedef void (WINAPI *init_fn)(void *);
typedef DWORD (WINAPI *errstr_fn)(DWORD, WCHAR *, DWORD *);
typedef DWORD (WINAPI *owner_fn)(void *, int, void *, DWORD *);
typedef DWORD (WINAPI *ownpid_fn)(ULONG, ULONGLONG *, int, void *, DWORD *);
typedef DWORD (WINAPI *getsess_fn)(ULONG);
typedef DWORD (WINAPI *setsess_fn)(ULONG, ULONG);
typedef DWORD (WINAPI *arp_fn)(IPAddr, IPAddr, ULONG *, ULONG *);
typedef void (WINAPI *free_fn)(void *);

static HMODULE iph;
static void *fn(const char *name)
{
    void *p = GetProcAddress( iph, name );
    if (!p) { printf("FAIL  %s is not exported\n", name); failures++; }
    return p;
}

static void test_params(void)
{
    static const struct { const char *name; int kind; } t[] =
    {
        { "GetIpForwardEntry2", 0 }, { "GetIpNetEntry2", 0 }, { "GetMulticastIpAddressEntry", 0 },
        { "GetAnycastIpAddressEntry", 0 }, { "GetIpPathEntry", 0 },
        { "GetMulticastIpAddressTable", 1 }, { "GetIpPathTable", 1 },
        { "GetIfStackTable", 2 }, { "GetInvertedIfStackTable", 2 },
    };
    BYTE row[256];
    unsigned i;
    char msg[128];

    for (i = 0; i < sizeof(t) / sizeof(t[0]); i++)
    {
        void *p = fn( t[i].name ), *out = NULL;
        DWORD ret;
        if (!p) continue;
        if (t[i].kind == 0)
        {
            ret = ((row_fn)p)( NULL );
            sprintf( msg, "%s(NULL) is invalid", t[i].name );
            check(ret == ERROR_INVALID_PARAMETER, msg);
            memset( row, 0, sizeof(row) );
            ret = ((row_fn)p)( row );
            sprintf( msg, "%s with no family is invalid", t[i].name );
            check(ret == ERROR_INVALID_PARAMETER, msg);
        }
        else if (t[i].kind == 1)
        {
            ret = ((tbl_fn)p)( AF_INET, NULL );
            sprintf( msg, "%s with no table pointer is invalid", t[i].name );
            check(ret == ERROR_INVALID_PARAMETER, msg);
            ret = ((tbl_fn)p)( 99, &out );
            sprintf( msg, "%s with family 99 is invalid", t[i].name );
            check(ret == ERROR_INVALID_PARAMETER, msg);
        }
        else
        {
            ret = ((stack_fn)p)( NULL );
            sprintf( msg, "%s(NULL) is invalid", t[i].name );
            check(ret == ERROR_INVALID_PARAMETER, msg);
        }
    }
}

static int count_family( ADDRESS_FAMILY fam )
{
    MIB_IPINTERFACE_TABLE *t;
    int n = -1;
    if (!GetIpInterfaceTable( fam, &t )) { n = t->NumEntries; FreeMibTable( t ); }
    return n;
}

static void test_multicast(void)
{
    tbl_fn get = (tbl_fn)fn( "GetMulticastIpAddressTable" );
    row_fn getrow = (row_fn)fn( "GetMulticastIpAddressEntry" );
    static const ADDRESS_FAMILY fams[] = { AF_INET, AF_INET6, AF_UNSPEC };
    int n4 = count_family( AF_INET ), n6 = count_family( AF_INET6 );
    unsigned f;
    ULONG i;

    if (!get || !getrow) return;
    check(n4 >= 1, "the host has an IPv4 interface");
    for (f = 0; f < 3; f++)
    {
        MIB_MULTICASTIPADDRESS_TABLE *t = NULL;
        int want = fams[f] == AF_INET ? n4 : fams[f] == AF_INET6 ? 2 * n6 : n4 + 2 * n6;
        int v4 = 0, v6 = 0, ok = 1;
        char msg[128];

        check(!get( fams[f], (void **)&t ) && t, "the multicast table is returned");
        if (!t) continue;
        sprintf( msg, "family %u has %d groups (got %lu)", fams[f], want, t->NumEntries );
        check(t->NumEntries == (ULONG)want, msg);
        for (i = 0; i < t->NumEntries; i++)
        {
            MIB_MULTICASTIPADDRESS_ROW *r = t->Table + i, copy;
            if (!r->InterfaceIndex || !r->InterfaceLuid.Value) ok = 0;
            if (r->Address.si_family == AF_INET)
            {
                v4++;
                if (r->Address.Ipv4.sin_addr.s_addr != htonl( 0xe0000001 )) ok = 0;
            }
            else if (r->Address.si_family == AF_INET6)
            {
                const BYTE *a = r->Address.Ipv6.sin6_addr.s6_addr;
                BYTE zero[14] = {0};
                v6++;
                if (a[0] != 0xff || (a[1] != 1 && a[1] != 2) || a[15] != 1 || memcmp( a + 2, zero, 13 )) ok = 0;
                if (r->ScopeId.Level != (a[1] == 1 ? 1 : 2) || r->ScopeId.Zone != r->InterfaceIndex) ok = 0;
            }
            else ok = 0;
            /* look each one up by index, and by LUID alone */
            memset( &copy, 0, sizeof(copy) );
            copy.Address = r->Address;
            copy.InterfaceIndex = r->InterfaceIndex;
            if (getrow( &copy ) || copy.InterfaceLuid.Value != r->InterfaceLuid.Value ||
                copy.ScopeId.Value != r->ScopeId.Value) ok = 0;
            memset( &copy, 0, sizeof(copy) );
            copy.Address = r->Address;
            copy.InterfaceLuid = r->InterfaceLuid;
            if (getrow( &copy ) || copy.InterfaceIndex != r->InterfaceIndex) ok = 0;
            /* not on another interface */
            memset( &copy, 0, sizeof(copy) );
            copy.Address = r->Address;
            copy.InterfaceIndex = r->InterfaceIndex + 1000;
            if (getrow( &copy ) != ERROR_NOT_FOUND) ok = 0;
        }
        check(ok, "multicast rows are well-formed and found by index and by LUID but not on a wrong interface");
        sprintf( msg, "the families split as %d IPv4 and %d IPv6", fams[f] == AF_INET6 ? 0 : n4,
                 fams[f] == AF_INET ? 0 : 2 * n6 );
        check(v4 == (fams[f] == AF_INET6 ? 0 : n4) && v6 == (fams[f] == AF_INET ? 0 : 2 * n6), msg);
        FreeMibTable( t );
    }

    {
        MIB_MULTICASTIPADDRESS_ROW r;
        memset( &r, 0, sizeof(r) );
        r.Address.si_family = AF_INET;
        r.Address.Ipv4.sin_addr.s_addr = htonl( 0xe0000001 );
        check(getrow( &r ) == ERROR_INVALID_PARAMETER, "a multicast row naming no interface is invalid");
        r.InterfaceIndex = 1;
        r.Address.Ipv4.sin_addr.s_addr = htonl( 0xe00000fb );
        check(getrow( &r ) == ERROR_NOT_FOUND, "a group the interface did not join is not found");
    }
}

static void test_anycast_path_stack(void)
{
    row_fn getany = (row_fn)fn( "GetAnycastIpAddressEntry" );
    row_fn getpath = (row_fn)fn( "GetIpPathEntry" );
    tbl_fn pathtbl = (tbl_fn)fn( "GetIpPathTable" );
    stack_fn stack = (stack_fn)fn( "GetIfStackTable" );
    stack_fn inv = (stack_fn)fn( "GetInvertedIfStackTable" );
    MIB_ANYCASTIPADDRESS_ROW a;
    MIB_IPPATH_ROW p;
    MIB_IPPATH_TABLE *pt = NULL;
    MIB_IF_TABLE2 *ifs = NULL;
    MIB_IFSTACK_TABLE *st = NULL;
    MIB_INVERTEDIFSTACK_TABLE *it = NULL;
    ULONG i, j;
    int ok;

    if (getany)
    {
        memset( &a, 0, sizeof(a) );
        a.Address.si_family = AF_INET6;
        a.InterfaceIndex = 1;
        check(getany( &a ) == ERROR_NOT_FOUND, "no anycast address is configured");
        a.InterfaceIndex = 0;
        check(getany( &a ) == ERROR_INVALID_PARAMETER, "an anycast row naming no interface is invalid");
    }
    if (pathtbl && getpath)
    {
        check(!pathtbl( AF_UNSPEC, (void **)&pt ) && pt && pt->NumEntries == 0, "the path table is empty");
        if (pt) FreeMibTable( pt );
        memset( &p, 0, sizeof(p) );
        p.Destination.si_family = AF_INET;
        p.Destination.Ipv4.sin_addr.s_addr = htonl( 0x7f000001 );
        check(getpath( &p ) == ERROR_NOT_FOUND, "an uncached path is not found");
        p.Source.si_family = AF_INET6;
        check(getpath( &p ) == ERROR_INVALID_PARAMETER, "a path from IPv6 to IPv4 is invalid");
    }
    if (!stack || !inv) return;
    check(!GetIfTable2( &ifs ) && ifs, "GetIfTable2");
    check(!stack( (void **)&st ) && st, "GetIfStackTable");
    check(!inv( (void **)&it ) && it, "GetInvertedIfStackTable");
    if (!ifs || !st || !it) return;
    check(st->NumEntries == 2 * ifs->NumEntries && it->NumEntries == st->NumEntries,
          "both stack tables have two rows per interface");
    ok = 1;
    for (i = 0; i < ifs->NumEntries; i++)
    {
        NET_IFINDEX n = ifs->Table[i].InterfaceIndex;
        int a1 = 0, a2 = 0, b1 = 0, b2 = 0;
        for (j = 0; j < st->NumEntries; j++)
        {
            if (st->Table[j].HigherLayerInterfaceIndex == 0 && st->Table[j].LowerLayerInterfaceIndex == n) a1++;
            if (st->Table[j].HigherLayerInterfaceIndex == n && st->Table[j].LowerLayerInterfaceIndex == 0) a2++;
            if (it->Table[j].LowerLayerInterfaceIndex == 0 && it->Table[j].HigherLayerInterfaceIndex == n) b1++;
            if (it->Table[j].LowerLayerInterfaceIndex == n && it->Table[j].HigherLayerInterfaceIndex == 0) b2++;
        }
        if (a1 != 1 || a2 != 1 || b1 != 1 || b2 != 1) ok = 0;
    }
    check(ok, "each interface is a leaf in both directions, in both tables");
    ok = 1;
    for (j = 1; j < st->NumEntries; j++)
    {
        ULONG h0 = st->Table[j - 1].HigherLayerInterfaceIndex, l0 = st->Table[j - 1].LowerLayerInterfaceIndex;
        ULONG h1 = st->Table[j].HigherLayerInterfaceIndex, l1 = st->Table[j].LowerLayerInterfaceIndex;
        if (h0 > h1 || (h0 == h1 && l0 > l1)) ok = 0;
        h0 = it->Table[j - 1].LowerLayerInterfaceIndex; l0 = it->Table[j - 1].HigherLayerInterfaceIndex;
        h1 = it->Table[j].LowerLayerInterfaceIndex; l1 = it->Table[j].HigherLayerInterfaceIndex;
        if (h0 > h1 || (h0 == h1 && l0 > l1)) ok = 0;
    }
    check(ok, "both stack tables are sorted");
    FreeMibTable( ifs ); FreeMibTable( st ); FreeMibTable( it );
}

static void test_entries(void)
{
    row_fn getfwd = (row_fn)fn( "GetIpForwardEntry2" );
    row_fn getnet = (row_fn)fn( "GetIpNetEntry2" );
    static const ADDRESS_FAMILY fams[] = { AF_INET, AF_INET6 };
    unsigned f;
    ULONG i;

    if (!getfwd || !getnet) return;
    for (f = 0; f < 2; f++)
    {
        MIB_IPFORWARD_TABLE2 *ft = NULL;
        MIB_IPNET_TABLE2 *nt = NULL;
        int ok = 1, tested = 0;

        if (!GetIpForwardTable2( fams[f], &ft ))
        {
            for (i = 0; i < ft->NumEntries; i++)
            {
                MIB_IPFORWARD_ROW2 k, *r = ft->Table + i;
                memset( &k, 0, sizeof(k) );
                k.DestinationPrefix = r->DestinationPrefix;
                k.NextHop = r->NextHop;
                k.InterfaceIndex = r->InterfaceIndex;
                if (getfwd( &k ) || k.Metric != r->Metric || k.InterfaceLuid.Value != r->InterfaceLuid.Value ||
                    k.Protocol != r->Protocol) ok = 0;
                memset( &k, 0, sizeof(k) );
                k.DestinationPrefix = r->DestinationPrefix;
                k.NextHop = r->NextHop;
                k.InterfaceLuid = r->InterfaceLuid;
                if (getfwd( &k ) || k.InterfaceIndex != r->InterfaceIndex) ok = 0;
                memset( &k, 0, sizeof(k) );
                k.DestinationPrefix = r->DestinationPrefix;
                k.NextHop = r->NextHop;
                k.InterfaceIndex = r->InterfaceIndex + 1000;
                if (getfwd( &k ) != ERROR_NOT_FOUND) ok = 0;
                memset( &k, 0, sizeof(k) );
                k.DestinationPrefix = r->DestinationPrefix;
                k.NextHop = r->NextHop;
                k.InterfaceIndex = r->InterfaceIndex;
                k.DestinationPrefix.PrefixLength = r->DestinationPrefix.PrefixLength ? 0 : 1;
                if (getfwd( &k ) == NO_ERROR) ok = 0;
                tested++;
            }
            FreeMibTable( ft );
        }
        printf("      %u forward rows for family %u\n", tested, fams[f]);
        check(ok, "every forward row is found by interface index or LUID, not on another interface or prefix");

        ok = 1; tested = 0;
        if (!GetIpNetTable2( fams[f], &nt ))
        {
            for (i = 0; i < nt->NumEntries; i++)
            {
                MIB_IPNET_ROW2 k, *r = nt->Table + i;
                memset( &k, 0, sizeof(k) );
                k.Address = r->Address;
                k.InterfaceIndex = r->InterfaceIndex;
                if (getnet( &k ) || k.State != r->State || k.PhysicalAddressLength != r->PhysicalAddressLength ||
                    memcmp( k.PhysicalAddress, r->PhysicalAddress, sizeof(k.PhysicalAddress) )) ok = 0;
                memset( &k, 0, sizeof(k) );
                k.Address = r->Address;
                k.InterfaceIndex = r->InterfaceIndex + 1000;
                if (getnet( &k ) != ERROR_NOT_FOUND) ok = 0;
                tested++;
            }
            FreeMibTable( nt );
        }
        printf("      %u neighbour rows for family %u\n", tested, fams[f]);
        check(ok, "every neighbour row is found by interface index, not on another interface");
    }

    {
        MIB_IPFORWARD_ROW2 k;
        MIB_IPNET_ROW2 n;
        memset( &k, 0, sizeof(k) );
        k.DestinationPrefix.Prefix.si_family = AF_INET;
        check(getfwd( &k ) == ERROR_INVALID_PARAMETER, "a route naming no interface is invalid");
        k.InterfaceIndex = 1;
        k.DestinationPrefix.Prefix.Ipv4.sin_addr.s_addr = htonl( 0x0a0b0c00 );
        k.DestinationPrefix.PrefixLength = 24;
        check(getfwd( &k ) == ERROR_NOT_FOUND, "a route that is not there is not found");
        memset( &n, 0, sizeof(n) );
        n.Address.si_family = AF_INET;
        check(getnet( &n ) == ERROR_INVALID_PARAMETER, "a neighbour naming no interface is invalid");
        n.InterfaceIndex = 1;
        n.Address.Ipv4.sin_addr.s_addr = htonl( 0x0a0b0c0d );
        check(getnet( &n ) == ERROR_NOT_FOUND, "a neighbour that is not there is not found");
    }
}

static void test_init(void)
{
    init_fn fwd = (init_fn)fn( "InitializeIpForwardEntry" );
    init_fn uni = (init_fn)fn( "InitializeUnicastIpAddressEntry" );
    MIB_IPFORWARD_ROW2 f;
    MIB_UNICASTIPADDRESS_ROW u;

    if (fwd)
    {
        memset( &f, 0x5a, sizeof(f) );
        fwd( &f );
        check(f.InterfaceLuid.Value == 0 && f.InterfaceIndex == 0 && f.DestinationPrefix.PrefixLength == 0 &&
              f.DestinationPrefix.Prefix.si_family == 0 && f.NextHop.si_family == 0, "route keys are cleared");
        check(f.SitePrefixLength == 0xff && f.ValidLifetime == ~0u && f.PreferredLifetime == ~0u,
              "route prefix length and lifetimes are unspecified");
        check(f.Metric == ~0u && f.Protocol == (NL_ROUTE_PROTOCOL)~0u && f.Origin == (NL_ROUTE_ORIGIN)~0u,
              "route metric, protocol and origin are unspecified");
        check(!f.Publish && !f.Immortal, "route flags are cleared");
    }
    if (uni)
    {
        memset( &u, 0x5a, sizeof(u) );
        uni( &u );
        check(u.InterfaceLuid.Value == 0 && u.InterfaceIndex == 0 && u.Address.si_family == 0 &&
              u.ScopeId.Value == 0 && u.SkipAsSource == 0 && u.CreationTimeStamp.QuadPart == 0 &&
              u.DadState == 0, "address keys and state are cleared");
        check(u.PrefixOrigin == IpPrefixOriginUnchanged && u.SuffixOrigin == IpSuffixOriginUnchanged,
              "address origins are unchanged");
        check(u.ValidLifetime == ~0u && u.PreferredLifetime == ~0u && u.OnLinkPrefixLength == 0xff,
              "address lifetimes and prefix length are unspecified");
    }
}

static void test_errstr(void)
{
    static const struct { DWORD code; const WCHAR *text; } t[] =
    {
        { 11001, L"Buffer too small." },
        { 11002, L"Destination net unreachable." },
        { 11003, L"Destination host unreachable." },
        { 11004, L"Destination protocol unreachable." },
        { 11005, L"Destination port unreachable." },
        { 11006, L"No resources." },
        { 11007, L"Bad option." },
        { 11008, L"Hardware error." },
        { 11009, L"Packet needs to be fragmented but DF set." },
        { 11010, L"Request timed out." },
        { 11011, L"Bad request." },
        { 11012, L"Bad route." },
        { 11013, L"TTL expired in transit." },
        { 11014, L"TTL expired during fragment reassembly." },
        { 11015, L"Parameter problem." },
        { 11016, L"Source quench." },
        { 11017, L"Option too big." },
        { 11018, L"Bad destination." },
        { 11050, L"General failure." },
    };
    errstr_fn get = (errstr_fn)fn( "GetIpErrorString" );
    WCHAR buf[256];
    DWORD size, ret;
    unsigned i;
    int ok = 1;

    if (!get) return;
    for (i = 0; i < sizeof(t) / sizeof(t[0]); i++)
    {
        size = 256;
        memset( buf, 0, sizeof(buf) );
        ret = get( t[i].code, buf, &size );
        if (ret || lstrcmpW( buf, t[i].text ) || size != (DWORD)lstrlenW( t[i].text ) + 1)
        {
            printf("      code %lu: ret %lu size %lu\n", t[i].code, ret, size);
            ok = 0;
        }
    }
    check(ok, "every IP status code has its text, and the size counts the NUL");

    size = 3;
    ret = get( 11003, buf, &size );
    check(ret == ERROR_INSUFFICIENT_BUFFER && size == (DWORD)lstrlenW( t[2].text ) + 1,
          "a small buffer reports the size it needs");
    size = 0;
    ret = get( 11003, NULL, &size );
    check(ret == ERROR_INSUFFICIENT_BUFFER && size == (DWORD)lstrlenW( t[2].text ) + 1,
          "and so does no buffer at all");
    size = (DWORD)lstrlenW( t[2].text );
    ret = get( 11003, buf, &size );
    check(ret == ERROR_INSUFFICIENT_BUFFER, "a buffer one short for the NUL is too small");
    check(get( 11003, buf, NULL ) == ERROR_INVALID_PARAMETER, "no size is invalid");
}

static void test_compartment(void)
{
    getsess_fn gets = (getsess_fn)fn( "GetSessionCompartmentId" );
    setsess_fn sets = (setsess_fn)fn( "SetSessionCompartmentId" );
    typedef ULONG (WINAPI *getcur_fn)(void);
    typedef DWORD (WINAPI *setcur_fn)(ULONG);
    getcur_fn getc = (getcur_fn)fn( "GetCurrentThreadCompartmentId" );
    setcur_fn setc = (setcur_fn)fn( "SetCurrentThreadCompartmentId" );

    if (!gets || !sets || !getc || !setc) return;
    check(getc() == 1, "the thread is in compartment 1");
    check(gets( 0 ) == 1 && gets( 1 ) == 1 && gets( 7 ) == 1, "every session is in compartment 1");
    check(sets( 0, 1 ) == NO_ERROR && gets( 0 ) == 1, "a session can be put in compartment 1");
    check(sets( 0, 2 ) == ERROR_INVALID_PARAMETER && sets( 0, 0 ) == ERROR_INVALID_PARAMETER,
          "but not in a compartment that does not exist");
    check(gets( 0 ) == 1, "and it stays in 1");
    check(setc( 1 ) == NO_ERROR && getc() == 1, "the thread can be put in compartment 1");
    check(setc( 2 ) == ERROR_INVALID_PARAMETER, "but not in compartment 2");
}

static void test_owner(void)
{
    static const struct { const char *name; int off; } t[] =
    {
        { "GetOwnerModuleFromTcpEntry", 20 }, { "GetOwnerModuleFromTcp6Entry", 52 },
        { "GetOwnerModuleFromUdpEntry", 8 }, { "GetOwnerModuleFromUdp6Entry", 24 },
    };
    WCHAR path[MAX_PATH], *base;
    BYTE row[256], buf[2048];
    DWORD size, ret;
    unsigned i;
    ownpid_fn bypid = (ownpid_fn)fn( "GetOwnerModuleFromPidAndInfo" );
    ULONGLONG info[16] = {0};

    GetModuleFileNameW( NULL, path, MAX_PATH );
    base = wcsrchr( path, '\\' ) + 1;

    for (i = 0; i < 5; i++)
    {
        void *p = i < 4 ? fn( t[i].name ) : (void *)bypid;
        const char *name = i < 4 ? t[i].name : "GetOwnerModuleFromPidAndInfo";
        char msg[128];
        void **ptrs;

        if (!p) continue;
        memset( row, 0, sizeof(row) );
        if (i < 4) *(DWORD *)(row + t[i].off) = GetCurrentProcessId();
#define CALL(rowp, pid, cls, b, sz) (i < 4 ? ((owner_fn)p)( (rowp), (cls), (b), (sz) ) : bypid( (pid), info, (cls), (b), (sz) ))
        size = sizeof(buf);
        memset( buf, 0, sizeof(buf) );
        ret = CALL( row, GetCurrentProcessId(), 0, buf, &size );
        sprintf( msg, "%s fills the basic info for this process", name );
        check(ret == NO_ERROR, msg);
        ptrs = (void **)buf;
        if (!ret)
        {
            WCHAR *mn = ptrs[0], *mp = ptrs[1];
            check((BYTE *)mn >= buf && (BYTE *)mn < buf + sizeof(buf) && !lstrcmpiW( mn, base ), "the module name is the executable name");
            check((BYTE *)mp >= buf && (BYTE *)mp < buf + sizeof(buf) && !lstrcmpiW( mp, path ), "the module path is the full path");
            check(size == sizeof(void *) * 2 + (lstrlenW( mn ) + 1 + lstrlenW( mp ) + 1) * sizeof(WCHAR),
                  "the size is the structure and both strings");
        }
        {
            DWORD need = size;
            size = 8;
            ret = CALL( row, GetCurrentProcessId(), 0, buf, &size );
            sprintf( msg, "%s with a small buffer reports the size", name );
            check(ret == ERROR_INSUFFICIENT_BUFFER && size == need, msg);
        }
        size = sizeof(buf);
        ret = CALL( row, GetCurrentProcessId(), 1, buf, &size );
        sprintf( msg, "%s with an unknown class is invalid", name );
        check(ret == ERROR_INVALID_PARAMETER, msg);
        if (i < 4)
        {
            size = sizeof(buf);
            check(((owner_fn)p)( NULL, 0, buf, &size ) == ERROR_INVALID_PARAMETER, "a NULL row is invalid");
            *(DWORD *)(row + t[i].off) = 0x7ffffff0;
        }
        size = sizeof(buf);
        ret = CALL( row, 0x7ffffff0, 0, buf, &size );
        sprintf( msg, "%s for a process that does not exist is not found", name );
        check(ret == ERROR_NOT_FOUND, msg);
    }
    if (bypid)
    {
        size = sizeof(buf);
        check(bypid( GetCurrentProcessId(), NULL, 0, buf, &size ) == ERROR_INVALID_PARAMETER, "no info is invalid");
    }
}

static void test_arp(void)
{
    arp_fn arp = (arp_fn)fn( "SendARP" );
    MIB_IPADDRTABLE *at;
    ULONG size = 0, mac[2], len, i;
    IPAddr own = 0;
    MIB_IFROW ifrow;

    if (!arp) return;
    len = 6;
    check(arp( 0x0100000a, 0, NULL, &len ) == ERROR_INVALID_PARAMETER, "no MAC buffer is invalid");
    check(arp( 0x0100000a, 0, mac, NULL ) == ERROR_INVALID_PARAMETER, "no length is invalid");
    check(arp( 0, 0, mac, &len ) == ERROR_INVALID_PARAMETER, "address 0 is invalid");

    GetIpAddrTable( NULL, &size, FALSE );
    at = HeapAlloc( GetProcessHeap(), 0, size );
    if (at && !GetIpAddrTable( at, &size, FALSE ))
    {
        for (i = 0; i < at->dwNumEntries && !own; i++)
        {
            if ((at->table[i].dwAddr & 0xff) == 127) continue;
            memset( &ifrow, 0, sizeof(ifrow) );
            ifrow.dwIndex = at->table[i].dwIndex;
            if (!GetIfEntry( &ifrow ) && ifrow.dwPhysAddrLen == 6) own = at->table[i].dwAddr;
        }
    }
    if (!own) { printf("SKIP  no non-loopback IPv4 address with a MAC on this host\n"); return; }
    memset( mac, 0, sizeof(mac) );
    len = 6;
    check(arp( own, 0, mac, &len ) == NO_ERROR && len == 6 && !memcmp( mac, ifrow.bPhysAddr, 6 ),
          "the MAC of one of our own addresses is that of its interface");
    memset( mac, 0, sizeof(mac) );
    len = 8;
    check(arp( own, own, mac, &len ) == NO_ERROR && len == 6, "and a larger buffer is told the real length");
    len = 2;
    memset( mac, 0, sizeof(mac) );
    check(arp( own, 0, mac, &len ) == ERROR_BUFFER_OVERFLOW && len == 6, "a buffer that is too small gets the length");
    check(((BYTE *)mac)[0] == 0 && ((BYTE *)mac)[2] == 0, "and nothing is written to it");
}

int main(void)
{
    WSADATA wsa;
    WSAStartup( MAKEWORD(2, 2), &wsa );
    iph = LoadLibraryA( "iphlpapi.dll" );
    if (!iph) { printf("FAIL  no iphlpapi\n"); return 1; }
    test_params();
    test_multicast();
    test_anycast_path_stack();
    test_entries();
    test_init();
    test_errstr();
    test_compartment();
    test_owner();
    test_arp();
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
