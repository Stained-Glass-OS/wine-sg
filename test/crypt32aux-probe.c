/* crypt32 async handles and the Unicode wrappers of the registry and CryptoAPI
 * calls (patches/sg/2408), run by test/crypt32aux-gate.sh. All of
 * Crypt{Create,Set,Get,Close}AsyncHandle/Param, Reg{Open,Create}KeyExU,
 * Reg{Query,Set}ValueExU, RegEnumValueU, RegDeleteValueU, RegQueryInfoKeyU,
 * CryptEnumProvidersU, CryptSetProviderU, CryptSignHashU and
 * CryptVerifySignatureU were stubs that did nothing. */
#include <windows.h>
#include <wincrypt.h>
#include <stdio.h>
#include <string.h>

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

typedef void (WINAPI *freefn)(LPSTR, LPVOID);
typedef BOOL (WINAPI *create_fn)(DWORD, HANDLE *);
typedef BOOL (WINAPI *set_fn)(HANDLE, LPSTR, LPVOID, freefn);
typedef BOOL (WINAPI *get_fn)(HANDLE, LPSTR, LPVOID *, freefn *);
typedef BOOL (WINAPI *close_fn)(HANDLE);

static HMODULE c32;
static void *fn(const char *name)
{
    void *p = GetProcAddress( c32, name );
    if (!p) { printf("FAIL  %s is not exported\n", name); failures++; }
    return p;
}

static int freed;
static char freed_name[8][32];
static void *freed_val[8];
static void WINAPI free_cb(LPSTR oid, LPVOID value)
{
    if (freed < 8)
    {
        if (IS_INTRESOURCE(oid)) sprintf(freed_name[freed], "#%u", (unsigned)(ULONG_PTR)oid);
        else lstrcpynA(freed_name[freed], oid, sizeof(freed_name[0]));
        freed_val[freed] = value;
    }
    freed++;
}

static void test_async(void)
{
    create_fn create = fn("CryptCreateAsyncHandle");
    set_fn set = fn("CryptSetAsyncParam");
    get_fn get = fn("CryptGetAsyncParam");
    close_fn close = fn("CryptCloseAsyncHandle");
    HANDLE h = NULL, h2 = NULL;
    void *v;
    freefn f;
    char a[4], b[4], c[4];
    BOOL ret;

    if (!create || !set || !get || !close) return;

    SetLastError(0xdeadbeef);
    check(!create(0, NULL) && GetLastError() == ERROR_INVALID_PARAMETER, "create with NULL out");
    check(create(0, &h) && h != NULL, "create a handle");
    check(create(0, &h2) && h2 != NULL && h2 != h, "a second handle is distinct");

    check(set(h, "1.2.3", a, free_cb), "set a string-named parameter");
    check(set(h, (LPSTR)(ULONG_PTR)7, b, free_cb), "set an integer-named parameter");
    check(set(h, "7", c, NULL), "set a string parameter that looks like the integer");
    check(set(h, (LPSTR)(ULONG_PTR)8, a, NULL), "set a second integer-named parameter");
    v = NULL;
    check(get(h, (LPSTR)(ULONG_PTR)8, &v, NULL) && v == a, "integer names are told apart");
    v = NULL; f = NULL;
    check(get(h, "1.2.3", &v, &f) && v == a && f == free_cb, "get returns the value and free function");
    v = NULL;
    check(get(h, (LPSTR)(ULONG_PTR)7, &v, NULL) && v == b, "get by integer name");
    v = NULL; f = (freefn)1;
    check(get(h, "7", &v, &f) && v == c && f == NULL, "string \"7\" is not the integer 7");
    v = (void *)1;
    SetLastError(0xdeadbeef);
    ret = get(h, "no.such", &v, NULL);
    check(!ret && GetLastError() == ERROR_NOT_FOUND, "an unknown name is not found");
    SetLastError(0xdeadbeef);
    ret = get(h2, "1.2.3", &v, NULL);
    check(!ret && GetLastError() == ERROR_NOT_FOUND, "parameters belong to their handle");
    SetLastError(0xdeadbeef);
    check(!get(h, "1.2.3", NULL, NULL) && GetLastError() == ERROR_INVALID_PARAMETER, "get with NULL out");
    SetLastError(0xdeadbeef);
    check(!set(h, NULL, a, NULL) && GetLastError() == ERROR_INVALID_PARAMETER, "set with NULL name");

    freed = 0;
    check(set(h, "1.2.3", c, free_cb), "set the same name again");
    check(freed == 1 && !strcmp(freed_name[0], "1.2.3") && freed_val[0] == a, "the replaced value is freed with its name");
    v = NULL;
    check(get(h, "1.2.3", &v, NULL) && v == c, "the new value is returned");

    freed = 0;
    check(close(h), "close");
    check(freed == 2, "close frees the two parameters that have a function");
    check(freed == 2 && ((!strcmp(freed_name[0], "1.2.3") && !strcmp(freed_name[1], "#7")) ||
                         (!strcmp(freed_name[1], "1.2.3") && !strcmp(freed_name[0], "#7"))), "freed with their names");
    SetLastError(0xdeadbeef);
    check(!close(h) && GetLastError() == ERROR_INVALID_HANDLE, "closing twice is an invalid handle");
    SetLastError(0xdeadbeef);
    check(!get(h, "1.2.3", &v, NULL) && GetLastError() == ERROR_INVALID_HANDLE, "get on a closed handle");
    SetLastError(0xdeadbeef);
    check(!set(h, "x", a, NULL) && GetLastError() == ERROR_INVALID_HANDLE, "set on a closed handle");
    SetLastError(0xdeadbeef);
    check(!close(NULL) && GetLastError() == ERROR_INVALID_HANDLE, "closing NULL");
    freed = 0;
    check(close(h2) && freed == 0, "an empty handle closes");
}

typedef LONG (WINAPI *openex_fn)(HKEY, const WCHAR *, DWORD, DWORD, HKEY *);
typedef LONG (WINAPI *createex_fn)(HKEY, const WCHAR *, DWORD, WCHAR *, DWORD, DWORD, void *, HKEY *, DWORD *);
typedef LONG (WINAPI *setex_fn)(HKEY, const WCHAR *, DWORD, DWORD, const BYTE *, DWORD);
typedef LONG (WINAPI *queryex_fn)(HKEY, const WCHAR *, DWORD *, DWORD *, BYTE *, DWORD *);
typedef LONG (WINAPI *enumv_fn)(HKEY, DWORD, WCHAR *, DWORD *, DWORD *, DWORD *, BYTE *, DWORD *);
typedef LONG (WINAPI *delv_fn)(HKEY, const WCHAR *);
typedef LONG (WINAPI *qinfo_fn)(HKEY, WCHAR *, DWORD *, DWORD *, DWORD *, DWORD *, DWORD *, DWORD *, DWORD *,
                                DWORD *, DWORD *, FILETIME *);

static void test_registry(void)
{
    static const WCHAR keyname[] = L"Software\\SGCrypt32AuxProbe";
    static const WCHAR euro[] = { 'v', 0x20ac, 0 };
    static const WCHAR text[] = { 'h', 0xe9, 'l', 'l', 'o', 0x20ac, 0 };
    openex_fn openex = fn("RegOpenKeyExU");
    createex_fn createex = fn("RegCreateKeyExU");
    setex_fn setex = fn("RegSetValueExU");
    queryex_fn queryex = fn("RegQueryValueExU");
    enumv_fn enumv = fn("RegEnumValueU");
    delv_fn delv = fn("RegDeleteValueU");
    qinfo_fn qinfo = fn("RegQueryInfoKeyU");
    HKEY key = NULL, key2 = NULL;
    DWORD disp = 0, type, size, nvals, maxname, maxdata, i, n;
    WCHAR buf[64], name[64];
    BYTE data[64];
    LONG ret;
    int found_euro = 0, found_a = 0;

    if (!openex || !createex || !setex || !queryex || !enumv || !delv || !qinfo) return;

    RegDeleteKeyW(HKEY_CURRENT_USER, keyname);
    ret = createex(HKEY_CURRENT_USER, keyname, 0, NULL, 0, KEY_ALL_ACCESS, NULL, &key, &disp);
    check(ret == ERROR_SUCCESS && key && disp == REG_CREATED_NEW_KEY, "RegCreateKeyExU creates a key");
    if (ret) return;
    ret = createex(HKEY_CURRENT_USER, keyname, 0, NULL, 0, KEY_ALL_ACCESS, NULL, &key2, &disp);
    check(ret == ERROR_SUCCESS && disp == REG_OPENED_EXISTING_KEY, "RegCreateKeyExU opens it again");
    if (key2) RegCloseKey(key2);

    ret = setex(key, euro, 0, REG_SZ, (const BYTE *)text, sizeof(text));
    check(ret == ERROR_SUCCESS, "RegSetValueExU with a Unicode name");
    ret = setex(key, L"a", 0, REG_DWORD, (const BYTE *)&(DWORD){ 0x1234 }, 4);
    check(ret == ERROR_SUCCESS, "RegSetValueExU REG_DWORD");

    size = sizeof(buf); type = 0;
    ret = queryex(key, euro, NULL, &type, (BYTE *)buf, &size);
    check(ret == ERROR_SUCCESS && type == REG_SZ && size == sizeof(text) && !lstrcmpW(buf, text), "RegQueryValueExU returns the wide text");
    size = 0;
    ret = queryex(key, euro, NULL, &type, NULL, &size);
    check(ret == ERROR_SUCCESS && size == sizeof(text), "RegQueryValueExU size query");
    size = 2;
    ret = queryex(key, euro, NULL, &type, (BYTE *)buf, &size);
    check(ret == ERROR_MORE_DATA && size == sizeof(text), "RegQueryValueExU small buffer");
    ret = queryex(key, L"missing", NULL, &type, NULL, &size);
    check(ret == ERROR_FILE_NOT_FOUND, "RegQueryValueExU missing value");

    ret = qinfo(key, NULL, NULL, NULL, NULL, NULL, NULL, &nvals, &maxname, &maxdata, NULL, NULL);
    check(ret == ERROR_SUCCESS && nvals == 2 && maxname == 2 && maxdata == sizeof(text), "RegQueryInfoKeyU counts values");

    {
        static const WCHAR cls[] = { 'C', 0x20ac, 'l', 0 };
        static const WCHAR ckey[] = L"Software\\SGCrypt32AuxProbe\\cls";
        WCHAR clsbuf[16] = { 0 };
        DWORD clslen = 16;
        HKEY ck = NULL;

        ret = createex(HKEY_CURRENT_USER, ckey, 0, (WCHAR *)cls, 0, KEY_ALL_ACCESS, NULL, &ck, &disp);
        check(ret == ERROR_SUCCESS && ck, "RegCreateKeyExU with a class");
        if (ck)
        {
            ret = qinfo(ck, clsbuf, &clslen, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
            check(ret == ERROR_SUCCESS && clslen == 3 && !lstrcmpW(clsbuf, cls), "RegQueryInfoKeyU returns the wide class");
            RegCloseKey(ck);
            RegDeleteKeyW(HKEY_CURRENT_USER, ckey);
        }
    }

    for (i = 0; i < 2; i++)
    {
        n = 64; size = sizeof(data); type = 0;
        ret = enumv(key, i, name, &n, NULL, &type, data, &size);
        if (ret != ERROR_SUCCESS) break;
        if (!lstrcmpW(name, euro)) found_euro = (type == REG_SZ && n == 2 && size == sizeof(text));
        if (!lstrcmpW(name, L"a")) found_a = (type == REG_DWORD && *(DWORD *)data == 0x1234);
    }
    check(found_euro && found_a, "RegEnumValueU lists both values");
    n = 64; size = sizeof(data);
    ret = enumv(key, 2, name, &n, NULL, &type, data, &size);
    check(ret == ERROR_NO_MORE_ITEMS, "RegEnumValueU ends with ERROR_NO_MORE_ITEMS");

    ret = delv(key, euro);
    check(ret == ERROR_SUCCESS, "RegDeleteValueU");
    ret = queryex(key, euro, NULL, &type, NULL, &size);
    check(ret == ERROR_FILE_NOT_FOUND, "the deleted value is gone");
    ret = delv(key, euro);
    check(ret == ERROR_FILE_NOT_FOUND, "RegDeleteValueU again");
    RegCloseKey(key);

    key = NULL;
    ret = openex(HKEY_CURRENT_USER, keyname, 0, KEY_READ, &key);
    check(ret == ERROR_SUCCESS && key, "RegOpenKeyExU");
    if (key) RegCloseKey(key);
    ret = openex(HKEY_CURRENT_USER, L"Software\\SGCrypt32AuxProbe\\nope", 0, KEY_READ, &key);
    check(ret == ERROR_FILE_NOT_FOUND, "RegOpenKeyExU missing key");
    RegDeleteKeyW(HKEY_CURRENT_USER, keyname);
}

static void test_crypto(void)
{
    typedef BOOL (WINAPI *enum_fn)(DWORD, DWORD *, DWORD, DWORD *, WCHAR *, DWORD *);
    typedef BOOL (WINAPI *setp_fn)(const WCHAR *, DWORD);
    typedef BOOL (WINAPI *sign_fn)(HCRYPTHASH, DWORD, const WCHAR *, DWORD, BYTE *, DWORD *);
    typedef BOOL (WINAPI *verify_fn)(HCRYPTHASH, const BYTE *, DWORD, HCRYPTKEY, const WCHAR *, DWORD);
    enum_fn enumu = fn("CryptEnumProvidersU");
    setp_fn setp = fn("CryptSetProviderU");
    sign_fn sign = fn("CryptSignHashU");
    verify_fn verify = fn("CryptVerifySignatureU");
    WCHAR nameu[256], namew[256];
    DWORD i, lenu, lenw, type, typew, matched = 0, count = 0;
    BOOL ru, rw;
    HCRYPTPROV prov = 0;
    HCRYPTKEY key = 0;
    HCRYPTHASH hash = 0;
    BYTE sig[256];
    DWORD siglen;

    if (!enumu || !setp || !sign || !verify) return;

    for (i = 0; i < 64; i++)
    {
        lenu = lenw = 256;
        ru = enumu(i, NULL, 0, &type, nameu, &lenu);
        rw = CryptEnumProvidersW(i, NULL, 0, &typew, namew, &lenw);
        if (ru != rw) break;
        if (!ru) break;
        count++;
        if (type == typew && lenu == lenw && !lstrcmpW(nameu, namew)) matched++;
    }
    check(count >= 1 && matched == count, "CryptEnumProvidersU lists what CryptEnumProvidersW lists");
    lenu = 0;
    ru = enumu(0, NULL, 0, &type, NULL, &lenu);
    check(ru && lenu > 1, "CryptEnumProvidersU name length query");

    SetLastError(0xdeadbeef);
    ru = setp(L"No Such Provider", PROV_RSA_FULL);
    check(!ru, "CryptSetProviderU of an unknown provider fails");

    if (!CryptAcquireContextW(&prov, NULL, MS_DEF_PROV_W, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT))
    { printf("FAIL  CryptAcquireContext %lu\n", GetLastError()); failures++; return; }
    if (!CryptGenKey(prov, AT_SIGNATURE, (1024 << 16), &key))
    { printf("FAIL  CryptGenKey %lu\n", GetLastError()); failures++; goto done; }
    CryptCreateHash(prov, CALG_MD5, 0, 0, &hash);
    CryptHashData(hash, (const BYTE *)"hello", 5, 0);
    siglen = 0;
    ru = sign(hash, AT_SIGNATURE, NULL, 0, NULL, &siglen);
    check(ru && siglen == 128, "CryptSignHashU size query");
    siglen = sizeof(sig);
    ru = sign(hash, AT_SIGNATURE, NULL, 0, sig, &siglen);
    check(ru && siglen == 128, "CryptSignHashU signs");
    ru = verify(hash, sig, siglen, key, NULL, 0);
    check(ru, "CryptVerifySignatureU verifies it");
    sig[10] ^= 0x55;
    SetLastError(0xdeadbeef);
    ru = verify(hash, sig, siglen, key, NULL, 0);
    check(!ru && GetLastError() == NTE_BAD_SIGNATURE, "a changed signature does not verify");
    sig[10] ^= 0x55;
    CryptDestroyHash(hash);
    CryptCreateHash(prov, CALG_MD5, 0, 0, &hash);
    CryptHashData(hash, (const BYTE *)"world", 5, 0);
    ru = verify(hash, sig, siglen, key, NULL, 0);
    check(!ru, "the signature of another hash does not verify");
done:
    if (hash) CryptDestroyHash(hash);
    if (key) CryptDestroyKey(key);
    CryptReleaseContext(prov, 0);
}

int main(void)
{
    c32 = LoadLibraryA("crypt32.dll");
    if (!c32) { printf("FAIL  crypt32.dll did not load\n"); return 1; }
    test_async();
    test_registry();
    test_crypto();
    printf("%d failures\n", failures);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
