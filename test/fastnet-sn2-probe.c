/* mscoree strong name signature generation and assembly comparison, over assemblies built with Wine
 * Mono's mcs (test/net/sgmeta.cs; keys sgtest.snk 1024 bit, sgtest2.snk 2048 bit). Fixtures in the cwd. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <wincrypt.h>

static int failures, checks;
#define ok(cond, ...) do { checks++; if (!(cond)) { failures++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); } } while (0)

#define CORSEC_E_INVALID_STRONGNAME ((HRESULT)0x8013141a)
#define CORSEC_E_MISSING_STRONGNAME ((HRESULT)0x8013141b)
#define CORSEC_E_SIGNATURE_MISMATCH ((HRESULT)0x80131420)

typedef BOOLEAN (WINAPI *gen_t)(const WCHAR *, const WCHAR *, BYTE *, ULONG, BYTE **, ULONG *, DWORD);
static gen_t pGenEx;
static BOOLEAN (WINAPI *pGen)(const WCHAR *, const WCHAR *, BYTE *, ULONG, BYTE **, ULONG *);
static BOOLEAN (WINAPI *pVerifyEx)(const WCHAR *, BOOLEAN, BOOLEAN *);
static BOOLEAN (WINAPI *pCompare)(const WCHAR *, const WCHAR *, DWORD *);
static BOOLEAN (WINAPI *pKeyInstall)(const WCHAR *, BYTE *, ULONG);
static BOOLEAN (WINAPI *pKeyDelete)(const WCHAR *);
static DWORD (WINAPI *pErrorInfo)(void);
static void (WINAPI *pFree)(BYTE *);

static BYTE *load(const WCHAR *name, DWORD *size)
{
    HANDLE f = CreateFileW(name, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    BYTE *b;
    DWORD n;
    if (f == INVALID_HANDLE_VALUE) return NULL;
    *size = GetFileSize(f, NULL);
    b = malloc(*size + 1);
    ReadFile(f, b, *size, &n, NULL);
    CloseHandle(f);
    return b;
}

static DWORD u32(const BYTE *p) { return p[0] | p[1] << 8 | p[2] << 16 | (DWORD)p[3] << 24; }

/* file offsets of the CLR header and the signature */
static void find(const BYTE *b, DWORD *cor, DWORD *sig, DWORD *siglen)
{
    DWORD pe = u32(b + 0x3c), nsect = b[pe + 6], opt = pe + 24, sect = opt + b[pe + 20], i;
    DWORD cor_rva = u32(b + opt + 96 + 14 * 8), sig_rva = 0;
    *cor = 0;
    for (i = 0; i < nsect; i++)
    {
        const BYTE *s = b + sect + 40 * i;
        DWORD va = u32(s + 12), rs = u32(s + 16), raw = u32(s + 20);
        if (cor_rva >= va && cor_rva < va + rs) { *cor = cor_rva - va + raw; sig_rva = u32(b + *cor + 32); *siglen = u32(b + *cor + 36); }
    }
    for (i = 0; i < nsect; i++)
    {
        const BYTE *s = b + sect + 40 * i;
        DWORD va = u32(s + 12), rs = u32(s + 16), raw = u32(s + 20);
        if (sig_rva >= va && sig_rva < va + rs) *sig = sig_rva - va + raw;
    }
}

static BOOL same_file(const WCHAR *a, const BYTE *orig, DWORD size)
{
    DWORD n;
    BYTE *b = load(a, &n);
    BOOL r = b && n == size && !memcmp(b, orig, size);
    free(b);
    return r;
}

int main(void)
{
    HMODULE mod = LoadLibraryA("mscoree.dll");
    BYTE *key1, *key2, *signed1, *delay, *out, *sigfile, *c, *sig;
    DWORD k1len, k2len, slen, dlen, cor, sigoff, siglen, n, res, cor2, sig2off, sig2len;
    ULONG outlen;
    BOOLEAN ret, verified;

    pGenEx = (void *)GetProcAddress(mod, "StrongNameSignatureGenerationEx");
    pGen = (void *)GetProcAddress(mod, "StrongNameSignatureGeneration");
    pVerifyEx = (void *)GetProcAddress(mod, "StrongNameSignatureVerificationEx");
    pCompare = (void *)GetProcAddress(mod, "StrongNameCompareAssemblies");
    pKeyInstall = (void *)GetProcAddress(mod, "StrongNameKeyInstall");
    pKeyDelete = (void *)GetProcAddress(mod, "StrongNameKeyDelete");
    pErrorInfo = (void *)GetProcAddress(mod, "StrongNameErrorInfo");
    pFree = (void *)GetProcAddress(mod, "StrongNameFreeBuffer");
    ok(pGenEx && pGen && pCompare && pVerifyEx, "exports missing\n");
    if (!pCompare) goto done;

    key1 = load(L"sgtest.snk", &k1len);
    key2 = load(L"sgtest2.snk", &k2len);
    signed1 = load(L"sgmeta-signed.dll", &slen);
    delay = load(L"sgmeta-delay.dll", &dlen);
    ok(key1 && key2 && signed1 && delay, "fixtures\n");
    if (!key1 || !key2 || !signed1 || !delay) goto done;
    find(signed1, &cor, &sigoff, &siglen);

    /* the size of the signature, no file */
    outlen = 0;
    ret = pGenEx(NULL, NULL, key1, k1len, NULL, &outlen, 0);
    ok(ret && outlen == 128, "size 1024: %d %lu\n", ret, outlen);
    ret = pGenEx(NULL, NULL, key2, k2len, NULL, &outlen, 0);
    ok(ret && outlen == 256, "size 2048: %d %lu\n", ret, outlen);

    /* RSA PKCS#1 signing is deterministic: signing the signed file again gives the signature mcs made */
    out = NULL; outlen = 0;
    ret = pGen(L"sgmeta-signed.dll", NULL, key1, k1len, &out, &outlen);
    ok(ret && outlen == 128 && out && !memcmp(out, signed1 + sigoff, 128), "re-signing reproduces the signature: %d %lu err %lx\n", ret, outlen, pErrorInfo());
    if (ret) pFree(out);
    ok(same_file(L"sgmeta-signed.dll", signed1, slen), "file changed when the signature was returned\n");

    /* tampered code is detected, and re-signing makes it valid again */
    CopyFileW(L"sgmeta-signed.dll", L"re.dll", FALSE);
    c = malloc(slen);
    memcpy(c, signed1, slen);
    c[sigoff - 0x10] ^= 0x40;     /* inside the signature's section, outside the signature */
    {
        HANDLE f = CreateFileW(L"re.dll", GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
        WriteFile(f, c, slen, &n, NULL);
        CloseHandle(f);
    }
    ret = pVerifyEx(L"re.dll", FALSE, &verified);
    ok(!ret, "tampered file verified\n");
    ret = pGenEx(L"re.dll", NULL, key1, k1len, NULL, &outlen, 0);
    ok(ret && outlen == 128, "sign in place: %d err %lx\n", ret, pErrorInfo());
    ret = pVerifyEx(L"re.dll", FALSE, &verified);
    ok(ret && verified, "re-signed file does not verify: err %lx\n", pErrorInfo());
    free(c);

    /* delay-signed: signing fills the signature and sets the flag */
    CopyFileW(L"sgmeta-delay.dll", L"delay.dll", FALSE);
    ret = pVerifyEx(L"delay.dll", FALSE, &verified);
    ok(!ret && pErrorInfo() == CORSEC_E_INVALID_STRONGNAME, "delay-signed verified: %d %lx\n", ret, pErrorInfo());
    ret = pGenEx(L"delay.dll", NULL, key1, k1len, NULL, &outlen, 0);
    ok(ret, "sign delay: err %lx\n", pErrorInfo());
    ret = pVerifyEx(L"delay.dll", FALSE, &verified);
    ok(ret && verified, "delay-signed file after signing: err %lx\n", pErrorInfo());
    sigfile = load(L"delay.dll", &n);
    ok(sigfile && (sigfile[cor + 16] & 8), "strong name flag not set\n");
    free(sigfile);

    /* test signing leaves the flag alone */
    CopyFileW(L"sgmeta-delay.dll", L"test.dll", FALSE);
    ret = pGenEx(L"test.dll", NULL, key1, k1len, NULL, &outlen, 2);
    ok(ret, "test sign: %lx\n", pErrorInfo());
    sigfile = load(L"test.dll", &n);
    find(sigfile, &cor2, &sig2off, &sig2len);
    ok(!(sigfile[cor2 + 16] & 8), "test sign set the flag\n");
    {
        BOOL nonzero = FALSE;
        DWORD i;
        for (i = 0; i < sig2len; i++) if (sigfile[sig2off + i]) nonzero = TRUE;
        ok(nonzero, "test sign wrote no signature\n");
    }
    free(sigfile);

    /* another key: the public keys differ, nothing is written */
    CopyFileW(L"sgmeta-delay.dll", L"wrong.dll", FALSE);
    ret = pGenEx(L"wrong.dll", NULL, key2, k2len, NULL, &outlen, 0);
    ok(!ret && pErrorInfo() == CORSEC_E_SIGNATURE_MISMATCH, "wrong key: %d %lx\n", ret, pErrorInfo());
    ok(same_file(L"wrong.dll", delay, dlen), "wrong key modified the file\n");
    {
        DWORD k3len;
        BYTE *key3 = load(L"sgtest3.snk", &k3len);   /* another 1024-bit key: same signature size */
        ret = key3 && pGenEx(L"wrong.dll", NULL, key3, k3len, NULL, &outlen, 0);
        ok(!ret && pErrorInfo() == CORSEC_E_SIGNATURE_MISMATCH, "wrong key, same size: %d %lx\n", ret, pErrorInfo());
        ok(same_file(L"wrong.dll", delay, dlen), "wrong key (same size) modified the file\n");
    }

    /* an assembly without a public key */
    CopyFileW(L"sgmeta.dll", L"nokey.dll", FALSE);
    ret = pGenEx(L"nokey.dll", NULL, key1, k1len, NULL, &outlen, 0);
    ok(!ret && pErrorInfo() == CORSEC_E_MISSING_STRONGNAME, "no public key: %d %lx\n", ret, pErrorInfo());

    /* a public key cannot sign; nothing is written */
    ret = pGenEx(L"wrong.dll", NULL, (BYTE *)"\x06\x02\x00\x00\x00\x24\x00\x00RSA1", 20, NULL, &outlen, 0);
    ok(!ret, "garbage key signed\n");
    ok(same_file(L"wrong.dll", delay, dlen), "garbage key modified the file\n");

    /* a key container */
    if (pKeyInstall && pKeyDelete)
    {
        CopyFileW(L"sgmeta-delay.dll", L"cont.dll", FALSE);
        ret = pKeyInstall(L"SgNetSn2Test", key1, k1len);
        ok(ret, "KeyInstall %lx\n", pErrorInfo());
        ret = pGenEx(L"cont.dll", L"SgNetSn2Test", NULL, 0, NULL, &outlen, 0);
        ok(ret && outlen == 128, "sign from container: %d %lx\n", ret, pErrorInfo());
        ret = pVerifyEx(L"cont.dll", FALSE, &verified);
        ok(ret && verified, "container-signed file: %lx\n", pErrorInfo());
        ret = pGenEx(L"cont.dll", L"SgNoSuchContainer", NULL, 0, NULL, &outlen, 0);
        ok(!ret, "missing container signed\n");
        pKeyDelete(L"SgNetSn2Test");
    }

    /* comparison */
    CopyFileW(L"sgmeta-signed.dll", L"same.dll", FALSE);
    res = 99;
    ret = pCompare(L"sgmeta-signed.dll", L"same.dll", &res);
    ok(ret && res == 1, "identical: %d %lu\n", ret, res);
    c = malloc(slen);
    memcpy(c, signed1, slen);
    c[sigoff + 5] ^= 0x10;
    c[cor + 16] &= ~8;      /* as for a delay-signed copy */
    {
        HANDLE f = CreateFileW(L"sigdiff.dll", GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
        WriteFile(f, c, slen, &n, NULL);
        CloseHandle(f);
    }
    res = 99;
    ret = pCompare(L"sgmeta-signed.dll", L"sigdiff.dll", &res);
    ok(ret && res == 2, "signature only: %d %lu\n", ret, res);
    c[sigoff - 0x10] ^= 1;
    {
        HANDLE f = CreateFileW(L"codediff.dll", GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
        WriteFile(f, c, slen, &n, NULL);
        CloseHandle(f);
    }
    res = 99;
    ret = pCompare(L"sgmeta-signed.dll", L"codediff.dll", &res);
    ok(ret && res == 0, "different: %d %lu\n", ret, res);
    res = 99;
    ret = pCompare(L"sgmeta-signed.dll", L"sgmeta.dll", &res);
    ok(ret && res == 0, "rebuilt assembly: %d %lu\n", ret, res);
    res = 99;
    ret = pCompare(L"sgmeta-signed.dll", L"nonexistent-file.dll", &res);
    ok(!ret, "missing file compared\n");
    free(c);
    (void)sig;

done:
    printf("%d checks, %d failures\n", checks, failures);
    printf(failures ? "RESULT: FAIL\n" : "RESULT: PASS\n");
    return failures != 0;
}
