/* Probe for patches/sg/5027: CERT_RDN_UNIVERSAL_STRING name values, whose
 * value is "an array of INT4 elements" (32-bit code points): X509_NAME_VALUE
 * encodes them as an ASN.1 UniversalString (tag 0x1c, 4 big-endian bytes each)
 * and decodes them back, a name with one decodes, and CertRDNValueToStrW /
 * CertNameToStrW give the text (UTF-16, astral code points as surrogates).
 * Table driven; lines "ok"/"FAIL", last "RESULT: PASS|FAIL". */
#include <windows.h>
#include <wincrypt.h>
#include <stdio.h>

static int failures;
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

int main(void)
{
    static const struct { DWORD chars[4]; DWORD n; const WCHAR *text; } tests[] =
    {
        {{'A', 'b', '1'}, 3, L"Ab1"},
        {{0xe9, 0x4e2d}, 2, L"\x00e9\x4e2d"},
        {{'x', 0x1f600, 'y'}, 3, L"x\xd83d\xde00y"},
        {{0}, 0, L""},
    };
    unsigned int i, j;

    for (i = 0; i < ARRAYSIZE(tests); i++)
    {
        CERT_NAME_VALUE value = {CERT_RDN_UNIVERSAL_STRING, {tests[i].n * 4, (BYTE *)tests[i].chars}};
        BYTE enc[64], want[64];
        DWORD size = sizeof(enc), n = 0;
        CERT_NAME_VALUE *dec = NULL;
        WCHAR str[32];
        BOOL ret, same;

        want[n++] = 0x1c; want[n++] = tests[i].n * 4;
        for (j = 0; j < tests[i].n; j++)
        {
            want[n++] = tests[i].chars[j] >> 24; want[n++] = tests[i].chars[j] >> 16;
            want[n++] = tests[i].chars[j] >> 8; want[n++] = tests[i].chars[j];
        }
        ret = CryptEncodeObjectEx(X509_ASN_ENCODING, X509_NAME_VALUE, &value, 0, NULL, enc, &size);
        check(ret && size == n && !memcmp(enc, want, n), "value %u: encoded as a UniversalString (%d, %lu bytes, %#lx)",
              i, ret, size, ret ? 0 : GetLastError());
        if (!ret) continue;
        size = 0;
        ret = CryptDecodeObjectEx(X509_ASN_ENCODING, X509_NAME_VALUE, want, n, CRYPT_DECODE_ALLOC_FLAG, NULL, &dec, &size);
        same = ret && dec->dwValueType == CERT_RDN_UNIVERSAL_STRING && dec->Value.cbData == tests[i].n * 4
               && (!tests[i].n || !memcmp(dec->Value.pbData, tests[i].chars, tests[i].n * 4));
        check(same, "value %u: decoded back (%d, %#lx)", i, ret, ret ? 0 : GetLastError());
        if (ret) LocalFree(dec);
        memset(str, 0x55, sizeof(str));
        size = CertRDNValueToStrW(CERT_RDN_UNIVERSAL_STRING, &value.Value, str, ARRAYSIZE(str));
        check(size == lstrlenW(tests[i].text) + 1 && !lstrcmpW(str, tests[i].text), "value %u: as text \"%ls\" (%lu)", i, str, size);
    }

    /* a name: CN as a UniversalString */
    {
        static const BYTE name[] =
        {
            0x30, 0x17, 0x31, 0x15, 0x30, 0x13, 0x06, 0x03, 0x55, 0x04, 0x03, 0x1c, 0x0c,
            0, 0, 0, 'W', 0, 0, 0, 'i', 0, 0, 0, 'n'
        };
        CERT_NAME_BLOB blob = {sizeof(name), (BYTE *)name};
        WCHAR str[64];
        DWORD size;

        size = CertNameToStrW(X509_ASN_ENCODING, &blob, CERT_X500_NAME_STR, str, ARRAYSIZE(str));
        check(size && !lstrcmpW(str, L"CN=Win"), "a name with a UniversalString CN: \"%ls\" (%lu)", str, size);
    }
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
