/* tsretrieve-probe.c: CryptRetrieveTimeStamp / CryptVerifyTimeStampSignature (wine-sg 7855) against a
 * local RFC 3161 time-stamping authority (tsretrieve-gate.sh: openssl ts behind a small HTTP server).
 *   tsretrieve-probe URL OUTDIR     (writes OUTDIR\token.der and OUTDIR\data.bin for openssl ts -verify)
 * Prints PASS/FAIL and RESULT. */
#include <stdio.h>
#include <string.h>
#include <windows.h>
#include <wincrypt.h>
#include <bcrypt.h>

#ifndef BCRYPT_SHA256_ALG_HANDLE
#define BCRYPT_SHA256_ALG_HANDLE ((BCRYPT_ALG_HANDLE)0x00000041)
#endif

static int failures;
#define CHECK(c, ...) do { if (!(c)) { failures++; printf("FAIL  " __VA_ARGS__); printf("\n"); } \
                           else { printf("PASS  " __VA_ARGS__); printf("\n"); } } while (0)

static void sha256(const BYTE *data, DWORD len, BYTE *out)
{
    BCryptHash(BCRYPT_SHA256_ALG_HANDLE, NULL, 0, (BYTE *)data, len, out, 32);
}

static void save(const WCHAR *dir, const WCHAR *name, const BYTE *p, DWORD n)
{
    WCHAR path[MAX_PATH];
    HANDLE f;
    DWORD w;
    swprintf(path, MAX_PATH, L"%ls\\%ls", dir, name);
    f = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    WriteFile(f, p, n, &w, NULL);
    CloseHandle(f);
}

int wmain(int argc, WCHAR **argv)
{
    static const BYTE data[] = "Stained Glass OS: a document to time-stamp";
    BYTE nonce[8] = { 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x08 }, hash[32];
    CRYPT_TIMESTAMP_PARA para = { NULL, TRUE, { sizeof(nonce), nonce }, 0, NULL };
    PCRYPT_TIMESTAMP_CONTEXT ctx = NULL, ctx2 = NULL;
    PCCERT_CONTEXT signer = NULL, signer2 = NULL;
    HCERTSTORE store = NULL;
    WCHAR bad[512], name[256] = L"";
    FILETIME now;
    LONGLONG delta;
    BOOL ok;
    DWORD err;

    if (argc < 3) return 2;
    sha256(data, sizeof(data) - 1, hash);

    ok = CryptRetrieveTimeStamp(argv[1], TIMESTAMP_VERIFY_CONTEXT_SIGNATURE, 20000, szOID_NIST_sha256, &para,
                                data, sizeof(data) - 1, &ctx, &signer, &store);
    err = GetLastError();
    CHECK(ok && ctx && ctx->pTimeStamp, "CryptRetrieveTimeStamp gets a verified time stamp from the authority (%#lx)", ok ? 0 : err);
    if (!ok || !ctx) { puts("RESULT: FAIL"); return 1; }
    CHECK(!strcmp(ctx->pTimeStamp->HashAlgorithm.pszObjId, szOID_NIST_sha256), "its hash algorithm is SHA-256 (%s)",
          ctx->pTimeStamp->HashAlgorithm.pszObjId);
    CHECK(ctx->pTimeStamp->HashedMessage.cbData == 32 && !memcmp(ctx->pTimeStamp->HashedMessage.pbData, hash, 32),
          "its message imprint is the data's hash");
    CHECK(ctx->pTimeStamp->Nonce.cbData == sizeof(nonce) && !memcmp(ctx->pTimeStamp->Nonce.pbData, nonce, sizeof(nonce)),
          "the nonce asked for comes back (%lu bytes)", ctx->pTimeStamp->Nonce.cbData);
    CHECK(ctx->pTimeStamp->SerialNumber.cbData > 0 && ctx->pTimeStamp->dwVersion == 1, "with a serial number, version 1");
    GetSystemTimeAsFileTime(&now);
    delta = (((LONGLONG)now.dwHighDateTime << 32 | now.dwLowDateTime) -
             ((LONGLONG)ctx->pTimeStamp->ftTime.dwHighDateTime << 32 | ctx->pTimeStamp->ftTime.dwLowDateTime)) / 10000000;
    CHECK(delta > -300 && delta < 300, "its time is now (%lld s off)", delta);
    CHECK(ctx->pTimeStamp->pszTSAPolicyId && !strcmp(ctx->pTimeStamp->pszTSAPolicyId, "1.3.6.1.4.1.65535.7855.1"),
          "the authority's policy (%s)", ctx->pTimeStamp->pszTSAPolicyId ? ctx->pTimeStamp->pszTSAPolicyId : "none");
    if (signer) CertGetNameStringW(signer, CERT_NAME_SIMPLE_DISPLAY_TYPE, 0, NULL, name, ARRAYSIZE(name));
    CHECK(signer && !wcscmp(name, L"SG Test TSA"), "the signer is the authority's certificate (%ls)", name);
    CHECK(store && CertEnumCertificatesInStore(store, NULL), "and the token's certificates come in a store");
    save(argv[2], L"token.der", ctx->pbEncoded, ctx->cbEncoded);
    save(argv[2], L"data.bin", data, sizeof(data) - 1);

    /* the token checked again later, as a signature's time stamp is */
    ok = CryptVerifyTimeStampSignature(ctx->pbEncoded, ctx->cbEncoded, data, sizeof(data) - 1, NULL, &ctx2, &signer2, NULL);
    CHECK(ok && ctx2 && ctx2->pTimeStamp->SerialNumber.cbData == ctx->pTimeStamp->SerialNumber.cbData &&
          !memcmp(ctx2->pTimeStamp->SerialNumber.pbData, ctx->pTimeStamp->SerialNumber.pbData, ctx->pTimeStamp->SerialNumber.cbData),
          "CryptVerifyTimeStampSignature verifies the token for its data (%#lx)", ok ? 0 : GetLastError());
    if (ctx2) CryptMemFree(ctx2);
    if (signer2) CertFreeCertificateContext(signer2);
    ctx2 = NULL;
    ok = CryptVerifyTimeStampSignature(ctx->pbEncoded, ctx->cbEncoded, (const BYTE *)"other data", 10, NULL, &ctx2, NULL, NULL);
    err = GetLastError();
    CHECK(!ok && !ctx2 && err == NTE_BAD_HASH, "other data does not match its imprint: NTE_BAD_HASH (%#lx)", err);
    {
        BYTE *copy = malloc(ctx->cbEncoded);
        memcpy(copy, ctx->pbEncoded, ctx->cbEncoded);
        copy[ctx->cbEncoded - 5] ^= 0x5a;   /* in the signature, the end of the SignerInfo */
        ok = CryptVerifyTimeStampSignature(copy, ctx->cbEncoded, data, sizeof(data) - 1, NULL, &ctx2, NULL, NULL);
        CHECK(!ok && !ctx2, "a token with a changed signature does not verify (%#lx)", GetLastError());
        free(copy);
    }

    /* the hash given instead of the data */
    if (signer) CertFreeCertificateContext(signer);
    if (store) CertCloseStore(store, 0);
    CryptMemFree(ctx);
    ctx = NULL; signer = NULL; store = NULL;
    para.Nonce.cbData = 0;   /* certificates asked for (the signer is in the token), no nonce */
    ok = CryptRetrieveTimeStamp(argv[1], TIMESTAMP_DONT_HASH_DATA | TIMESTAMP_VERIFY_CONTEXT_SIGNATURE, 20000, szOID_NIST_sha256,
                                &para, hash, sizeof(hash), &ctx, NULL, NULL);
    err = GetLastError();
    CHECK(ok && ctx && !memcmp(ctx->pTimeStamp->HashedMessage.pbData, hash, 32), "TIMESTAMP_DONT_HASH_DATA: the hash given is stamped (%#lx)", ok ? 0 : err);
    if (ctx) CryptMemFree(ctx);

    /* an authority that refuses */
    swprintf(bad, ARRAYSIZE(bad), L"%lsbad", argv[1]);
    ctx = (void *)1;
    ok = CryptRetrieveTimeStamp(bad, 0, 20000, szOID_NIST_sha256, NULL, data, sizeof(data) - 1, &ctx, NULL, NULL);
    CHECK(!ok && !ctx, "an authority's refusal fails the call (%#lx)", GetLastError());

    puts(failures ? "RESULT: FAIL" : "RESULT: PASS");
    return failures != 0;
}
