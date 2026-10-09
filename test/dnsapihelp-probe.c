/* dnsapi record type names, question writing, context handles, the flush
 * calls and the cache table (patches/sg/2407), run by
 * test/dnsapihelp-gate.sh. DnsRecordStringForType,
 * DnsRecordStringForWritableType and DnsRecordTypeForName were stubs that
 * were not even callable; DnsWriteQuestionToBuffer_*, DnsGetCacheDataTable
 * and the context handle calls returned FALSE or nothing. */
#include <windows.h>
#include <windns.h>
#include <stdio.h>
#include <string.h>

/* not in every SDK's windns.h */
typedef struct _SG_CACHE_ENTRY
{
    struct _SG_CACHE_ENTRY *Next;
    const WCHAR *Name;
    WORD Type;
    WORD DataLength;
    ULONG Flags;
} DNS_CACHE_ENTRY;

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

typedef const char *(WINAPI *strtype_fn)(WORD);
typedef WORD (WINAPI *typename_fn)(const char *, int);
typedef BOOL (WINAPI *wq8_fn)(DNS_MESSAGE_BUFFER *, DWORD *, const char *, WORD, WORD, BOOL);
typedef BOOL (WINAPI *wqw_fn)(DNS_MESSAGE_BUFFER *, DWORD *, const WCHAR *, WORD, WORD, BOOL);
typedef DWORD (WINAPI *ctx_fn)(DWORD, void *, HANDLE *);
typedef void (WINAPI *rel_fn)(HANDLE);
typedef void (WINAPI *flush_fn)(void);
typedef BOOL (WINAPI *flushe_fn)(const void *);
typedef BOOL (WINAPI *cache_fn)(DNS_CACHE_ENTRY **);
typedef void (WINAPI *free_fn)(void *, DNS_FREE_TYPE);

static HMODULE dns;
static void *fn(const char *name)
{
    void *p = GetProcAddress( dns, name );
    if (!p) { printf("FAIL  %s is not exported\n", name); failures++; }
    return p;
}

static void test_types(void)
{
    static const struct { const char *name; WORD type; const char *canon; } t[] =
    {
        { "A", 1, "A" }, { "NS", 2, "NS" }, { "CNAME", 5, "CNAME" }, { "SOA", 6, "SOA" },
        { "PTR", 12, "PTR" }, { "HINFO", 13, "HINFO" }, { "MX", 15, "MX" }, { "TXT", 16, "TXT" },
        { "AAAA", 28, "AAAA" }, { "SRV", 33, "SRV" }, { "NAPTR", 35, "NAPTR" }, { "DNAME", 39, "DNAME" },
        { "OPT", 41, "OPT" }, { "WINS", 0xff01, "WINS" }, { "WINSR", 0xff02, "WINSR" },
        { "ALL", 255, "ALL" }, { "AXFR", 252, "AXFR" }, { "KEY", 25, "KEY" }, { "SIG", 24, "SIG" },
    };
    strtype_fn str = fn( "DnsRecordStringForType" ), wstr = fn( "DnsRecordStringForWritableType" );
    typename_fn byname = fn( "DnsRecordTypeForName" );
    char msg[128];
    unsigned i;

    if (!str || !wstr || !byname) return;
    for (i = 0; i < sizeof(t) / sizeof(t[0]); i++)
    {
        const char *s = str( t[i].type );
        sprintf( msg, "StringForType(%u) is %s", t[i].type, t[i].canon );
        check( s && !strcmp( s, t[i].canon ), msg );
        sprintf( msg, "TypeForName(%s) is %u", t[i].name, t[i].type );
        check( byname( t[i].name, 0 ) == t[i].type, msg );
        sprintf( msg, "TypeForName(%s, strlen) is %u", t[i].name, t[i].type );
        check( byname( t[i].name, (int)strlen( t[i].name ) ) == t[i].type, msg );
    }
    check( byname( "aaaa", 0 ) == 28, "TypeForName ignores case" );
    check( byname( "Mx", 0 ) == 15, "TypeForName mixed case" );
    check( byname( "AAAAXX", 4 ) == 28, "TypeForName honours the length" );
    check( byname( "AAAA", 3 ) == 0, "TypeForName with a short length is not AAAA" );
    check( byname( "NOSUCHTYPE", 0 ) == 0, "TypeForName unknown is 0" );
    check( byname( "", 0 ) == 0, "TypeForName empty is 0" );
    check( byname( NULL, 0 ) == 0, "TypeForName NULL is 0" );
    check( str( 0 ) == NULL, "StringForType(0) is NULL" );
    check( str( 9999 ) == NULL, "StringForType unknown is NULL" );
    check( wstr( 1 ) && !strcmp( wstr( 1 ), "A" ), "WritableType(A) is A" );
    check( wstr( 28 ) && !strcmp( wstr( 28 ), "AAAA" ), "WritableType(AAAA) is AAAA" );
    check( wstr( 255 ) == NULL, "WritableType(ALL) is NULL" );
    check( wstr( 252 ) == NULL, "WritableType(AXFR) is NULL" );
    check( wstr( 41 ) == NULL, "WritableType(OPT) is NULL" );
    check( wstr( 0 ) == NULL, "WritableType(0) is NULL" );
}

static void test_question(void)
{
    wq8_fn w8 = fn( "DnsWriteQuestionToBuffer_UTF8" );
    wqw_fn ww = fn( "DnsWriteQuestionToBuffer_W" );
    static const BYTE body[] = { 3,'w','w','w', 7,'e','x','a','m','p','l','e', 3,'c','o','m', 0, 0,28, 0,1 };
    union { DNS_MESSAGE_BUFFER msg; BYTE raw[512]; } b;
    DWORD size, err;
    BOOL ret;
    char name[300];
    int i;

    if (!w8 || !ww) return;

    memset( &b, 0xcc, sizeof(b) );
    size = sizeof(b);
    ret = w8( &b.msg, &size, "www.example.com", DNS_TYPE_AAAA, 0x1234, TRUE );
    check( ret, "write question" );
    check( size == 12 + sizeof(body), "size is the message length" );
    check( !memcmp( b.raw + 12, body, sizeof(body) ), "name, type and class in wire format" );
    check( b.msg.MessageHead.Xid == 0x1234, "Xid is in host order" );
    check( b.msg.MessageHead.RecursionDesired == 1, "recursion desired" );
    check( b.msg.MessageHead.QuestionCount == 1, "one question (host order)" );
    check( b.msg.MessageHead.AnswerCount == 0 && b.msg.MessageHead.NameServerCount == 0 &&
           b.msg.MessageHead.AdditionalCount == 0, "no other records" );
    check( b.msg.MessageHead.IsResponse == 0 && b.msg.MessageHead.Opcode == 0 &&
           b.msg.MessageHead.ResponseCode == 0 && b.msg.MessageHead.Truncation == 0, "other header bits clear" );

    memset( &b, 0xcc, sizeof(b) );
    size = sizeof(b);
    ret = w8( &b.msg, &size, "www.example.com.", DNS_TYPE_AAAA, 7, FALSE );
    check( ret && size == 12 + sizeof(body) && !memcmp( b.raw + 12, body, sizeof(body) ), "trailing dot is the same name" );
    check( b.msg.MessageHead.RecursionDesired == 0 && b.msg.MessageHead.Xid == 7, "no recursion, Xid 7" );

    memset( &b, 0xcc, sizeof(b) );
    size = sizeof(b);
    ret = ww( &b.msg, &size, L"www.example.com", DNS_TYPE_AAAA, 0x1234, TRUE );
    check( ret && size == 12 + sizeof(body) && !memcmp( b.raw + 12, body, sizeof(body) ), "wide name" );

    memset( &b, 0xcc, sizeof(b) );
    size = sizeof(b);
    ret = w8( &b.msg, &size, "a", DNS_TYPE_A, 0, TRUE );
    check( ret && size == 12 + 3 + 4 && b.raw[12] == 1 && b.raw[13] == 'a' && b.raw[14] == 0 &&
           b.raw[15] == 0 && b.raw[16] == 1 && b.raw[17] == 0 && b.raw[18] == 1, "one-label name" );

    memset( &b, 0xcc, sizeof(b) );
    size = sizeof(b);
    ret = w8( &b.msg, &size, ".", DNS_TYPE_NS, 0, TRUE );
    check( ret && size == 12 + 1 + 4 && b.raw[12] == 0 && b.raw[14] == 2, "root name" );

    /* too small */
    size = 12 + sizeof(body) - 1;
    SetLastError( 0xdeadbeef );
    ret = w8( &b.msg, &size, "www.example.com", DNS_TYPE_AAAA, 1, TRUE );
    err = GetLastError();
    check( !ret && err == ERROR_MORE_DATA, "too small gives ERROR_MORE_DATA" );
    check( size == 12 + sizeof(body), "too small reports the needed size" );
    size = 12 + sizeof(body);
    ret = w8( &b.msg, &size, "www.example.com", DNS_TYPE_AAAA, 1, TRUE );
    check( ret && size == 12 + sizeof(body), "exact size works" );
    size = 5;
    SetLastError( 0xdeadbeef );
    ret = w8( &b.msg, &size, "a", DNS_TYPE_A, 1, TRUE );
    check( !ret && GetLastError() == ERROR_MORE_DATA && size == 19, "tiny buffer" );

    /* invalid names */
    {
        static const char *bad[] = { "a..b", "..", ".a", "a...", "b..c" };
        for (i = 0; i < 5; i++)
        {
            char m[64];
            size = sizeof(b);
            SetLastError( 0xdeadbeef );
            ret = w8( &b.msg, &size, bad[i], DNS_TYPE_A, 1, TRUE );
            sprintf( m, "invalid name '%s' is rejected", bad[i] );
            check( !ret && GetLastError() == ERROR_INVALID_NAME, m );
        }
    }
    memset( name, 'a', 64 ); name[64] = 0;
    size = sizeof(b);
    SetLastError( 0xdeadbeef );
    ret = w8( &b.msg, &size, name, DNS_TYPE_A, 1, TRUE );
    check( !ret && GetLastError() == ERROR_INVALID_NAME, "64-byte label is rejected" );
    memset( name, 'a', 63 ); name[63] = 0;
    size = sizeof(b);
    ret = w8( &b.msg, &size, name, DNS_TYPE_A, 1, TRUE );
    check( ret && size == 12 + 1 + 63 + 1 + 4, "63-byte label is accepted" );
    name[0] = 0;
    for (i = 0; i < 5; i++) strcat( name, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa." );
    size = sizeof(b);
    SetLastError( 0xdeadbeef );
    ret = w8( &b.msg, &size, name, DNS_TYPE_A, 1, TRUE );
    check( !ret && GetLastError() == ERROR_INVALID_NAME, "name over 255 bytes is rejected" );

    /* parameters */
    size = sizeof(b);
    SetLastError( 0xdeadbeef );
    check( !w8( NULL, &size, "a", 1, 1, 1 ) && GetLastError() == ERROR_INVALID_PARAMETER, "NULL buffer" );
    SetLastError( 0xdeadbeef );
    check( !w8( &b.msg, NULL, "a", 1, 1, 1 ) && GetLastError() == ERROR_INVALID_PARAMETER, "NULL size" );
    SetLastError( 0xdeadbeef );
    check( !w8( &b.msg, &size, NULL, 1, 1, 1 ) && GetLastError() == ERROR_INVALID_PARAMETER, "NULL name" );
    SetLastError( 0xdeadbeef );
    check( !ww( &b.msg, &size, NULL, 1, 1, 1 ) && GetLastError() == ERROR_INVALID_PARAMETER, "NULL wide name" );
}

static void test_context(void)
{
    ctx_fn a = fn( "DnsAcquireContextHandle_A" ), u = fn( "DnsAcquireContextHandle_UTF8" ),
           w = fn( "DnsAcquireContextHandle_W" );
    rel_fn rel = fn( "DnsReleaseContextHandle" );
    flush_fn flush = fn( "DnsFlushResolverCache" );
    flushe_fn fa = fn( "DnsFlushResolverCacheEntry_A" ), fu = fn( "DnsFlushResolverCacheEntry_UTF8" ),
              fw = fn( "DnsFlushResolverCacheEntry_W" );
    HANDLE h1 = NULL, h2 = NULL;
    DWORD ret;

    if (!a || !u || !w || !rel) return;
    ret = a( 0, NULL, &h1 );
    check( ret == ERROR_SUCCESS && h1 != NULL, "AcquireContextHandle_A" );
    ret = w( 0, NULL, &h2 );
    check( ret == ERROR_SUCCESS && h2 != NULL && h2 != h1, "AcquireContextHandle_W gives a distinct handle" );
    rel( h1 );
    rel( h2 );
    h1 = NULL;
    ret = u( 0, NULL, &h1 );
    check( ret == ERROR_SUCCESS && h1 != NULL, "AcquireContextHandle_UTF8" );
    rel( h1 );
    rel( NULL );
    check( 1, "ReleaseContextHandle(NULL) is harmless" );
    check( a( 0, NULL, NULL ) == ERROR_INVALID_PARAMETER, "A with NULL context" );
    check( u( 0, NULL, NULL ) == ERROR_INVALID_PARAMETER, "UTF8 with NULL context" );
    check( w( 0, NULL, NULL ) == ERROR_INVALID_PARAMETER, "W with NULL context" );

    if (flush) { flush(); check( 1, "DnsFlushResolverCache returns" ); }
    if (fa && fu && fw)
    {
        check( fa( "www.example.com" ) && fu( "www.example.com" ) && fw( L"www.example.com" ), "FlushResolverCacheEntry" );
        check( !fa( NULL ) && !fu( NULL ) && !fw( NULL ), "FlushResolverCacheEntry(NULL) fails" );
    }
}

static void write_hosts( const char *text )
{
    char path[MAX_PATH];
    HANDLE f;
    DWORD n;

    GetSystemDirectoryA( path, MAX_PATH );
    strcat( path, "\\drivers" ); CreateDirectoryA( path, NULL );
    strcat( path, "\\etc" ); CreateDirectoryA( path, NULL );
    strcat( path, "\\hosts" );
    f = CreateFileA( path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL );
    if (f == INVALID_HANDLE_VALUE) { printf( "FAIL  cannot write hosts\n" ); failures++; return; }
    WriteFile( f, text, strlen( text ), &n, NULL );
    CloseHandle( f );
}

static DNS_CACHE_ENTRY *find( DNS_CACHE_ENTRY *e, const WCHAR *name, WORD type )
{
    for (; e; e = e->Next) if (e->Type == type && !lstrcmpiW( e->Name, name )) return e;
    return NULL;
}

static void test_cache(void)
{
    cache_fn get = fn( "DnsGetCacheDataTable" );
    free_fn dfree = fn( "DnsFree" );
    DNS_CACHE_ENTRY *t = (void *)1, *e;
    int count = 0;
    BOOL ret;

    if (!get || !dfree) return;

    write_hosts( "# comment\r\n127.0.0.1 localhost loopback # trailing\r\n::1\tip6-localhost\r\n\r\n"
                 "10.1.2.3 foo.example.com\r\n#10.9.9.9 hidden\r\n" );
    ret = get( &t );
    check( ret && t != NULL, "cache table from the hosts file" );
    for (e = t; e; e = e->Next) count++;
    check( count == 5, "one entry per name, plus the AAAA entry for localhost" );
    check( find( t, L"localhost", DNS_TYPE_AAAA ) != NULL, "localhost AAAA is added" );
    check( find( t, L"localhost", DNS_TYPE_A ) != NULL, "localhost A" );
    check( find( t, L"loopback", DNS_TYPE_A ) != NULL, "loopback A (second name on the line)" );
    check( find( t, L"ip6-localhost", DNS_TYPE_AAAA ) != NULL, "ip6-localhost AAAA" );
    check( find( t, L"foo.example.com", DNS_TYPE_A ) != NULL, "foo.example.com A" );
    check( find( t, L"hidden", DNS_TYPE_A ) == NULL, "commented line is skipped" );
    e = find( t, L"localhost", DNS_TYPE_A );
    check( e && e->DataLength == 4, "A data length is 4" );
    e = find( t, L"ip6-localhost", DNS_TYPE_AAAA );
    check( e && e->DataLength == 16, "AAAA data length is 16" );
    while (t)
    {
        e = t->Next;
        dfree( (void *)t->Name, DnsFreeFlat );
        dfree( t, DnsFreeFlat );
        t = e;
    }
    check( 1, "entries free with DnsFree" );

    write_hosts( "# nothing here\r\n" );
    t = (void *)1;
    ret = get( &t );
    count = 0;
    for (e = t; e; e = e->Next) count++;
    check( ret && count == 2 && find( t, L"localhost", DNS_TYPE_A ) && find( t, L"localhost", DNS_TYPE_AAAA ),
           "empty hosts file gives only localhost" );
    while (t) { e = t->Next; dfree( (void *)t->Name, DnsFreeFlat ); dfree( t, DnsFreeFlat ); t = e; }
    SetLastError( 0xdeadbeef );
    check( !get( NULL ) && GetLastError() == ERROR_INVALID_PARAMETER, "NULL table" );
}

int main(void)
{
    dns = LoadLibraryA( "dnsapi.dll" );
    if (!dns) { printf( "FAIL  dnsapi.dll did not load\n" ); return 1; }
    test_types();
    test_question();
    test_context();
    test_cache();
    printf( "%d failures\n", failures );
    printf( "RESULT: %s\n", failures ? "FAIL" : "PASS" );
    return failures != 0;
}
