/* wine-sg 9920: CryptImportPKCS8 / CryptExportPKCS8 / CryptExportPKCS8Ex, run by test/pkcs8-gate.sh.
 *   pkcs8-probe.exe openssl-key.der modulus-hex-file exported.der
 * Imports an RSA PKCS #8 key that openssl made, checks the modulus, exports it again (must be the same
 * bytes), and covers encrypted keys (callbacks), the default provider, signature-only keys (key usage
 * attribute), an unsupported algorithm, ERROR_MORE_DATA and a non-exportable key. */
#include <windows.h>
#include <wincrypt.h>
#include <stdio.h>
#include <string.h>

typedef BOOL (WINAPI *export_ex_t)(CRYPT_PKCS8_EXPORT_PARAMS *, DWORD, void *, BYTE *, DWORD *);
static export_ex_t pCryptExportPKCS8Ex;
static int failures;
#define CHECK(c, ...) do { if (c) printf("PASS  "); else { printf("FAIL  "); failures++; } printf(__VA_ARGS__); printf("\n"); fflush(stdout); } while (0)

static BYTE *readfile(const char *name, DWORD *size)
{
    FILE *f = fopen(name, "rb");
    BYTE *buf;
    long n;
    if (!f) return NULL;
    fseek(f, 0, SEEK_END); n = ftell(f); fseek(f, 0, SEEK_SET);
    buf = malloc(n + 1);
    *size = fread(buf, 1, n, f);
    buf[*size] = 0;
    fclose(f);
    return buf;
}

static HCRYPTPROV container(const char *name)
{
    HCRYPTPROV prov = 0;
    CryptAcquireContextA(&prov, name, MS_ENHANCED_PROV_A, PROV_RSA_FULL, CRYPT_DELETEKEYSET);
    prov = 0;
    if (!CryptAcquireContextA(&prov, name, MS_ENHANCED_PROV_A, PROV_RSA_FULL, CRYPT_NEWKEYSET)) return 0;
    return prov;
}

static int resolved;
static BOOL CALLBACK resolve(CRYPT_PRIVATE_KEY_INFO *info, HCRYPTPROV *prov, void *ctx)
{
    resolved++;
    if (!info || strcmp(info->Algorithm.pszObjId, szOID_RSA_RSA)) return FALSE;
    *prov = container(ctx);
    return *prov != 0;
}

/* a modulus in big-endian hex, from a PUBLICKEYBLOB of the provider's key */
static void modulus_hex(HCRYPTPROV prov, DWORD spec, char *hex)
{
    HCRYPTKEY key;
    BYTE blob[2048];
    DWORD size = sizeof(blob), bits, i, n;
    *hex = 0;
    if (!CryptGetUserKey(prov, spec, &key)) return;
    if (CryptExportKey(key, 0, PUBLICKEYBLOB, 0, blob, &size))
    {
        bits = ((RSAPUBKEY *)(blob + sizeof(BLOBHEADER)))->bitlen;
        n = bits / 8;
        for (i = 0; i < n; i++)
            sprintf(hex + i * 2, "%02X", blob[sizeof(BLOBHEADER) + sizeof(RSAPUBKEY) + n - 1 - i]);
    }
    CryptDestroyKey(key);
}

static BOOL CALLBACK xor_encrypt(CRYPT_ALGORITHM_IDENTIFIER *alg, CRYPT_DATA_BLOB *clear, BYTE *out, DWORD *size, void *ctx)
{
    DWORD i;
    alg->pszObjId = (char *)"1.3.6.1.4.1.99999.1";
    alg->Parameters.cbData = 0;
    if (!out) { *size = clear->cbData; return TRUE; }
    if (*size < clear->cbData) return FALSE;
    for (i = 0; i < clear->cbData; i++) out[i] = clear->pbData[i] ^ 0x5a;
    *size = clear->cbData;
    return TRUE;
}

static int decrypted;
static BOOL CALLBACK xor_decrypt(CRYPT_ALGORITHM_IDENTIFIER alg, CRYPT_DATA_BLOB enc, BYTE *out, DWORD *size, void *ctx)
{
    DWORD i;
    if (strcmp(alg.pszObjId, "1.3.6.1.4.1.99999.1")) return FALSE;
    if (!out) { *size = enc.cbData; return TRUE; }
    for (i = 0; i < enc.cbData; i++) out[i] = enc.pbData[i] ^ 0x5a;
    *size = enc.cbData;
    decrypted++;
    return TRUE;
}

int main(int argc, char **argv)
{
    CRYPT_PKCS8_IMPORT_PARAMS ip = { { 0 } };
    CRYPT_PKCS8_EXPORT_PARAMS ep = { 0 };
    HCRYPTPROV prov = 0, prov2 = 0, prov3 = 0;
    HCRYPTKEY key;
    BYTE *der, *out = NULL, *enc = NULL, small[8];
    DWORD der_size, size = 0, enc_size = 0, small_size;
    char want[1100] = "", got[1100], *expect;
    BOOL ret;
    FILE *f;

    if (argc < 4) { printf("usage\n"); return 2; }
    pCryptExportPKCS8Ex = (export_ex_t)GetProcAddress(GetModuleHandleA("crypt32"), "CryptExportPKCS8Ex");
    CHECK(pCryptExportPKCS8Ex != NULL, "CryptExportPKCS8Ex is exported");
    if (!pCryptExportPKCS8Ex) goto end;
    der = readfile(argv[1], &der_size);
    expect = (char *)readfile(argv[2], &size);
    CHECK(der && expect, "inputs read (%lu bytes of key)", der_size);
    if (!der || !expect) goto end;
    sscanf(expect, "%1099s", want);

    /* import into the container the resolve callback names */
    ip.PrivateKey.cbData = der_size; ip.PrivateKey.pbData = der;
    ip.pResolvehCryptProvFunc = resolve; ip.pVoidResolveFunc = (void *)"sg-pkcs8-a";
    SetLastError(0xdead);
    ret = CryptImportPKCS8(ip, CRYPT_EXPORTABLE, &prov, NULL);
    CHECK(ret && prov && resolved == 1, "CryptImportPKCS8 with a resolve callback (ret %d, error %lu, resolved %d)", ret, GetLastError(), resolved);
    if (!ret) goto end;
    modulus_hex(prov, AT_KEYEXCHANGE, got);
    CHECK(!strcmp(got, want), "the imported AT_KEYEXCHANGE key has openssl's modulus (%.16s... want %.16s...)", got, want);

    /* export it again: the same PrivateKeyInfo bytes openssl wrote */
    size = 0;
    ret = CryptExportPKCS8(prov, AT_KEYEXCHANGE, (char *)szOID_RSA_RSA, 0, NULL, NULL, &size);
    CHECK(ret && size, "CryptExportPKCS8 size query (ret %d, size %lu, error %lu)", ret, size, GetLastError());
    small_size = sizeof(small);
    SetLastError(0xdead);
    ret = CryptExportPKCS8(prov, AT_KEYEXCHANGE, (char *)szOID_RSA_RSA, 0, NULL, small, &small_size);
    CHECK(!ret && GetLastError() == ERROR_MORE_DATA && small_size == size, "a small buffer: ERROR_MORE_DATA and the size (ret %d, error %lu, size %lu)", ret, GetLastError(), small_size);
    out = malloc(size);
    ret = CryptExportPKCS8(prov, AT_KEYEXCHANGE, (char *)szOID_RSA_RSA, 0, NULL, out, &size);
    CHECK(ret, "CryptExportPKCS8 (error %lu)", GetLastError());
    CHECK(ret && size == der_size && !memcmp(out, der, size), "the export is openssl's PrivateKeyInfo, byte for byte (%lu vs %lu bytes)", size, der_size);
    if (ret && (f = fopen(argv[3], "wb"))) { fwrite(out, 1, size, f); fclose(f); }

    /* encrypted: CryptExportPKCS8Ex with an encrypt callback, CryptImportPKCS8 with a decrypt callback */
    ep.hCryptProv = prov; ep.dwKeySpec = AT_KEYEXCHANGE; ep.pszPrivateKeyObjId = (char *)szOID_RSA_RSA;
    ep.pEncryptPrivateKeyFunc = xor_encrypt;
    ret = pCryptExportPKCS8Ex(&ep, 0, NULL, NULL, &enc_size);
    if (ret) { enc = malloc(enc_size); ret = pCryptExportPKCS8Ex(&ep, 0, NULL, enc, &enc_size); }
    CHECK(ret && enc[0] == 0x30, "CryptExportPKCS8Ex with an encrypt callback (ret %d, error %lu, %lu bytes)", ret, GetLastError(), enc_size);
    if (ret)
    {
        CRYPT_ENCRYPTED_PRIVATE_KEY_INFO *e = NULL;
        DWORD es;
        ret = CryptDecodeObjectEx(X509_ASN_ENCODING, PKCS_ENCRYPTED_PRIVATE_KEY_INFO, enc, enc_size, CRYPT_DECODE_ALLOC_FLAG, NULL, &e, &es);
        CHECK(ret && !strcmp(e->EncryptionAlgorithm.pszObjId, "1.3.6.1.4.1.99999.1") && e->EncryptedPrivateKey.cbData == der_size &&
              (e->EncryptedPrivateKey.pbData[0] ^ 0x5a) == 0x30, "it is an EncryptedPrivateKeyInfo with the callback's algorithm and data");
        if (ret) LocalFree(e);
        ip.PrivateKey.cbData = enc_size; ip.PrivateKey.pbData = enc;
        ip.pVoidResolveFunc = (void *)"sg-pkcs8-b";
        ret = CryptImportPKCS8(ip, 0, &prov2, NULL);
        CHECK(!ret, "an encrypted key without a decrypt callback is refused (ret %d, error %08lx)", ret, GetLastError());
        if (ret) CryptReleaseContext(prov2, 0);
        prov2 = 0;
        ip.pDecryptPrivateKeyFunc = xor_decrypt;
        ret = CryptImportPKCS8(ip, 0, &prov2, NULL);
        modulus_hex(prov2, AT_KEYEXCHANGE, got);
        CHECK(ret && decrypted == 1 && !strcmp(got, want), "an encrypted key with a decrypt callback (ret %d, error %lu, decrypted %d)", ret, GetLastError(), decrypted);
        ip.pDecryptPrivateKeyFunc = NULL;
        if (prov2 && CryptGetUserKey(prov2, AT_KEYEXCHANGE, &key))
        {
            size = 0;
            ret = CryptExportPKCS8(prov2, AT_KEYEXCHANGE, (char *)szOID_RSA_RSA, 0, NULL, NULL, &size);
            CHECK(!ret, "a key imported without CRYPT_EXPORTABLE does not export (ret %d, error %08lx)", ret, GetLastError());
            CryptDestroyKey(key);
        }
    }

    /* no resolve callback: the default provider */
    ip.PrivateKey.cbData = der_size; ip.PrivateKey.pbData = der;
    ip.pResolvehCryptProvFunc = NULL; ip.pVoidResolveFunc = NULL;
    ret = CryptImportPKCS8(ip, 0, &prov3, NULL);
    modulus_hex(prov3, AT_KEYEXCHANGE, got);
    CHECK(ret && prov3 && !strcmp(got, want), "no resolve callback: the default provider gets the key (ret %d, error %lu)", ret, GetLastError());
    {
        char name[100] = "";
        DWORD n = sizeof(name);
        CHECK(prov3 && CryptGetProvParam(prov3, PP_NAME, (BYTE *)name, &n, 0) && n > 1, "its provider is %s", name);
    }

    /* a signature key: marked with the key usage attribute, imported as AT_SIGNATURE */
    {
        HCRYPTPROV sp = container("sg-pkcs8-sig"), sp2 = 0;
        CRYPT_PRIVATE_KEY_INFO *pi = NULL;
        BYTE *sder = NULL;
        DWORD ss = 0, ps;
        char sigmod[1100];
        ret = sp && CryptGenKey(sp, AT_SIGNATURE, CRYPT_EXPORTABLE | (1024 << 16), &key);
        if (ret) CryptDestroyKey(key);
        ret = ret && CryptExportPKCS8(sp, AT_SIGNATURE, (char *)szOID_RSA_RSA, 0, NULL, NULL, &ss) &&
              (sder = malloc(ss)) && CryptExportPKCS8(sp, AT_SIGNATURE, (char *)szOID_RSA_RSA, 0, NULL, sder, &ss);
        CHECK(ret, "an AT_SIGNATURE key exports (error %lu)", GetLastError());
        if (ret && CryptDecodeObjectEx(X509_ASN_ENCODING, PKCS_PRIVATE_KEY_INFO, sder, ss, CRYPT_DECODE_ALLOC_FLAG, NULL, &pi, &ps))
        {
            CHECK(pi->pAttributes && pi->pAttributes->cAttr == 1 && !strcmp(pi->pAttributes->rgAttr[0].pszObjId, szOID_KEY_USAGE),
                  "with the key usage attribute (%lu attributes)", pi->pAttributes ? pi->pAttributes->cAttr : 0);
            LocalFree(pi);
        }
        else CHECK(0, "its PrivateKeyInfo decodes");
        modulus_hex(sp, AT_SIGNATURE, sigmod);
        ip.PrivateKey.cbData = ss; ip.PrivateKey.pbData = sder;
        ip.pResolvehCryptProvFunc = resolve; ip.pVoidResolveFunc = (void *)"sg-pkcs8-c";
        ret = sder && CryptImportPKCS8(ip, 0, &sp2, NULL);
        modulus_hex(sp2, AT_SIGNATURE, got);
        CHECK(ret && !strcmp(got, sigmod), "it imports as AT_SIGNATURE (ret %d, error %lu)", ret, GetLastError());
        CHECK(sp2 && !CryptGetUserKey(sp2, AT_KEYEXCHANGE, &key), "and not as AT_KEYEXCHANGE");
        if (sp2) CryptReleaseContext(sp2, 0);
        if (sp) CryptReleaseContext(sp, 0);
        free(sder);
    }

    /* an algorithm with no import function: ERROR_UNSUPPORTED_TYPE */
    {
        static BYTE keybytes[] = { 0x02, 0x01, 0x01 };
        CRYPT_PRIVATE_KEY_INFO pi = { 0 };
        BYTE *dsa = NULL;
        DWORD ds;
        HCRYPTPROV dp = 0;
        pi.Algorithm.pszObjId = (char *)"1.2.840.10040.4.1";
        pi.PrivateKey.cbData = sizeof(keybytes); pi.PrivateKey.pbData = keybytes;
        ret = CryptEncodeObjectEx(X509_ASN_ENCODING, PKCS_PRIVATE_KEY_INFO, &pi, CRYPT_ENCODE_ALLOC_FLAG, NULL, &dsa, &ds);
        ip.PrivateKey.cbData = ds; ip.PrivateKey.pbData = dsa;
        ip.pResolvehCryptProvFunc = NULL;
        SetLastError(0xdead);
        ret = ret && CryptImportPKCS8(ip, 0, &dp, NULL);
        CHECK(!ret && GetLastError() == ERROR_UNSUPPORTED_TYPE && !dp, "a DSA PrivateKeyInfo: ERROR_UNSUPPORTED_TYPE (ret %d, error %lu)", ret, GetLastError());
        SetLastError(0xdead);
        size = 0;
        ret = CryptExportPKCS8(prov, AT_KEYEXCHANGE, (char *)"1.2.840.10040.4.1", 0, NULL, NULL, &size);
        CHECK(!ret && GetLastError() == ERROR_UNSUPPORTED_TYPE, "exporting as DSA: ERROR_UNSUPPORTED_TYPE (ret %d, error %lu)", ret, GetLastError());
        LocalFree(dsa);
    }

    /* garbage is an ASN.1 error */
    {
        BYTE junk[] = { 0x04, 0x03, 1, 2, 3 };
        HCRYPTPROV jp = 0;
        ip.PrivateKey.cbData = sizeof(junk); ip.PrivateKey.pbData = junk;
        SetLastError(0xdead);
        ret = CryptImportPKCS8(ip, 0, &jp, NULL);
        CHECK(!ret && (GetLastError() & 0xffff0000) == 0x80090000, "a non-PKCS #8 blob: an ASN.1 error (ret %d, error %08lx)", ret, GetLastError());
    }

end:
    if (prov) CryptReleaseContext(prov, 0);
    if (prov2) CryptReleaseContext(prov2, 0);
    if (prov3) CryptReleaseContext(prov3, 0);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures ? 1 : 0;
}
