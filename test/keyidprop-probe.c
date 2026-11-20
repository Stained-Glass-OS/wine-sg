/* CryptSetKeyIdentifierProperty / CryptGetKeyIdentifierProperty / CryptEnumKeyIdentifierProperties keep
 * properties for a key identifier (patches/sg/8214). Set was a stub returning FALSE; Get and Enum were not
 * exported at all. */
#include <windows.h>
#include <wincrypt.h>
#include <stdio.h>

static int failures;
#define CHECK(c, ...) do { if (!(c)) { failures++; printf("FAIL  "); } else printf("ok    "); \
                           printf(__VA_ARGS__); printf("\n"); } while (0)

typedef BOOL (WINAPI *pSet)(const CRYPT_HASH_BLOB *, DWORD, DWORD, LPCWSTR, void *, const void *);
typedef BOOL (WINAPI *pGet)(const CRYPT_HASH_BLOB *, DWORD, DWORD, LPCWSTR, void *, void *, DWORD *);
typedef BOOL (WINAPI *pEnum)(const CRYPT_HASH_BLOB *, DWORD, DWORD, LPCWSTR, void *, void *, PFN_CRYPT_ENUM_KEYID_PROP);

static BYTE id_bytes[] = { 0xde, 0xad, 0xbe, 0xef, 0x01, 0x02, 0x03, 0x04 };
static CRYPT_HASH_BLOB id = { sizeof(id_bytes), id_bytes };

struct enum_state { int seen, props, has_prov, has_name; };

static BOOL WINAPI enum_cb( const CRYPT_HASH_BLOB *key, DWORD flags, void *reserved, void *arg, DWORD count,
                            DWORD *ids, void **values, DWORD *sizes )
{
    struct enum_state *st = arg;
    DWORD i;

    if (key->cbData != id.cbData || memcmp( key->pbData, id.pbData, id.cbData )) return TRUE;
    st->seen++;
    st->props = count;
    for (i = 0; i < count; i++)
    {
        if (ids[i] == CERT_KEY_PROV_INFO_PROP_ID &&
            !lstrcmpW( ((CRYPT_KEY_PROV_INFO *)values[i])->pwszContainerName, L"sg-container" )) st->has_prov = 1;
        if (ids[i] == CERT_FRIENDLY_NAME_PROP_ID && sizes[i] == 5 && !memcmp( values[i], "hello", 5 )) st->has_name = 1;
    }
    return TRUE;
}

int main(void)
{
    HMODULE crypt32 = LoadLibraryA( "crypt32.dll" );
    pSet set = (pSet)GetProcAddress( crypt32, "CryptSetKeyIdentifierProperty" );
    pGet get = (pGet)GetProcAddress( crypt32, "CryptGetKeyIdentifierProperty" );
    pEnum enumerate = (pEnum)GetProcAddress( crypt32, "CryptEnumKeyIdentifierProperties" );
    BYTE param_data[] = { 9, 8, 7 };
    CRYPT_KEY_PROV_PARAM param = { 42, param_data, sizeof(param_data), 0 };
    CRYPT_KEY_PROV_INFO info = { (WCHAR *)L"sg-container", (WCHAR *)L"Microsoft Enhanced Cryptographic Provider v1.0",
                                 PROV_RSA_FULL, 0, 1, &param, AT_KEYEXCHANGE };
    CRYPT_DATA_BLOB name = { 5, (BYTE *)"hello" };
    struct enum_state st;
    BYTE buf[1024], small[4];
    CRYPT_KEY_PROV_INFO *got = (CRYPT_KEY_PROV_INFO *)buf, *alloc = NULL;
    DWORD size;
    BOOL ret;

    CHECK( set && get && enumerate, "exports: set %p get %p enum %p", set, get, enumerate );
    if (!set || !get || !enumerate) goto done;
    set( &id, 0, CRYPT_KEYID_DELETE_FLAG, NULL, NULL, NULL );  /* a clean start */

    ret = set( &id, CERT_KEY_PROV_INFO_PROP_ID, 0, NULL, NULL, &info );
    CHECK( ret, "set CERT_KEY_PROV_INFO_PROP_ID: %lu", ret ? 0 : GetLastError() );
    ret = set( &id, CERT_FRIENDLY_NAME_PROP_ID, 0, NULL, NULL, &name );
    CHECK( ret, "set CERT_FRIENDLY_NAME_PROP_ID: %lu", ret ? 0 : GetLastError() );

    size = 0;
    ret = get( &id, CERT_KEY_PROV_INFO_PROP_ID, 0, NULL, NULL, NULL, &size );
    CHECK( ret && size > sizeof(CRYPT_KEY_PROV_INFO), "get size: %d %lu", ret, size );
    size = sizeof(buf);
    memset( buf, 0, sizeof(buf) );
    ret = get( &id, CERT_KEY_PROV_INFO_PROP_ID, 0, NULL, NULL, buf, &size );
    CHECK( ret && got->pwszContainerName && !lstrcmpW( got->pwszContainerName, L"sg-container" ) &&
           got->pwszProvName && !lstrcmpW( got->pwszProvName, info.pwszProvName ) && got->dwProvType == PROV_RSA_FULL &&
           got->dwKeySpec == AT_KEYEXCHANGE && got->cProvParam == 1 && got->rgProvParam[0].dwParam == 42 &&
           got->rgProvParam[0].cbData == 3 && !memcmp( got->rgProvParam[0].pbData, param_data, 3 ),
           "get CRYPT_KEY_PROV_INFO back: %d", ret );
    CHECK( ret && (BYTE *)got->pwszContainerName > buf && (BYTE *)got->pwszContainerName < buf + size,
           "the strings live in the caller's buffer" );

    size = sizeof(small);
    SetLastError( 0xdeadbeef );
    ret = get( &id, CERT_FRIENDLY_NAME_PROP_ID, 0, NULL, NULL, small, &size );
    CHECK( !ret && GetLastError() == ERROR_MORE_DATA && size == 5, "small buffer: %d %lu %lu", ret, GetLastError(), size );
    size = sizeof(buf);
    ret = get( &id, CERT_FRIENDLY_NAME_PROP_ID, 0, NULL, NULL, buf, &size );
    CHECK( ret && size == 5 && !memcmp( buf, "hello", 5 ), "get the blob back: %d %lu", ret, size );

    ret = get( &id, CERT_KEY_PROV_INFO_PROP_ID, CRYPT_KEYID_ALLOC_FLAG, NULL, NULL, &alloc, &size );
    CHECK( ret && alloc && !lstrcmpW( alloc->pwszContainerName, L"sg-container" ), "CRYPT_KEYID_ALLOC_FLAG: %d", ret );
    if (alloc) LocalFree( alloc );

    SetLastError( 0xdeadbeef );
    ret = set( &id, CERT_FRIENDLY_NAME_PROP_ID, CRYPT_KEYID_SET_NEW_FLAG, NULL, NULL, &name );
    CHECK( !ret && GetLastError() == (DWORD)CRYPT_E_EXISTS, "CRYPT_KEYID_SET_NEW_FLAG on an existing one: %d %#lx",
           ret, GetLastError() );

    memset( &st, 0, sizeof(st) );
    ret = enumerate( NULL, 0, 0, NULL, NULL, &st, enum_cb );
    CHECK( ret && st.seen == 1 && st.props == 2 && st.has_prov && st.has_name,
           "enumerate all: %d seen %d props %d prov %d name %d", ret, st.seen, st.props, st.has_prov, st.has_name );
    memset( &st, 0, sizeof(st) );
    ret = enumerate( &id, CERT_FRIENDLY_NAME_PROP_ID, 0, NULL, NULL, &st, enum_cb );
    CHECK( ret && st.seen == 1 && st.props == 1 && st.has_name && !st.has_prov,
           "enumerate one property of one key id: %d seen %d props %d", ret, st.seen, st.props );

    ret = set( &id, CERT_FRIENDLY_NAME_PROP_ID, 0, NULL, NULL, NULL );
    size = sizeof(buf);
    SetLastError( 0xdeadbeef );
    CHECK( ret && !get( &id, CERT_FRIENDLY_NAME_PROP_ID, 0, NULL, NULL, buf, &size ) &&
           GetLastError() == (DWORD)CRYPT_E_NOT_FOUND, "a NULL value removes the property: %#lx", GetLastError() );

    ret = set( &id, 0, CRYPT_KEYID_DELETE_FLAG, NULL, NULL, NULL );
    memset( &st, 0, sizeof(st) );
    enumerate( NULL, 0, 0, NULL, NULL, &st, enum_cb );
    CHECK( ret && !st.seen, "CRYPT_KEYID_DELETE_FLAG removes the key id: %d seen %d", ret, st.seen );

    size = sizeof(buf);
    SetLastError( 0xdeadbeef );
    ret = get( &id, CERT_KEY_PROV_INFO_PROP_ID, 0, NULL, NULL, buf, &size );
    CHECK( !ret && GetLastError() == (DWORD)CRYPT_E_NOT_FOUND, "an unknown key id: %d %#lx", ret, GetLastError() );

done:
    printf( "RESULT: %s\n", failures ? "FAIL" : "PASS" );
    return failures != 0;
}
