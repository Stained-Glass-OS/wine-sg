/* mscoree strong name signature verification and tokens over assemblies built with Wine Mono's mcs
 * (test/net/sgmeta.cs). Usage: probe  (fixtures in the current directory) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <wincrypt.h>

static int failures, checks;
#define ok(cond, ...) do { checks++; if (!(cond)) { failures++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); } } while (0)

#define CORSEC_E_INVALID_STRONGNAME ((HRESULT)0x8013141a)
#define CORSEC_E_MISSING_STRONGNAME ((HRESULT)0x8013141b)
#define CORSEC_E_INVALID_IMAGE_FORMAT ((HRESULT)0x8013141d)

static BOOLEAN (WINAPI *pVerify)(const WCHAR *, DWORD, DWORD *);
static BOOLEAN (WINAPI *pVerifyEx)(const WCHAR *, BOOLEAN, BOOLEAN *);
static BOOLEAN (WINAPI *pToken)(const WCHAR *, BYTE **, ULONG *);
static BOOLEAN (WINAPI *pTokenEx)(const WCHAR *, BYTE **, ULONG *, BYTE **, ULONG *);
static BOOLEAN (WINAPI *pTokenFromKey)(BYTE *, ULONG, BYTE **, ULONG *);
static DWORD (WINAPI *pErrorInfo)(void);
static void (WINAPI *pFree)(BYTE *);

static BYTE *load(const WCHAR *name, DWORD *size)
{
    HANDLE f = CreateFileW(name, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    BYTE *b;
    DWORD n;
    if (f == INVALID_HANDLE_VALUE) return NULL;
    *size = GetFileSize(f, NULL);
    b = malloc(*size + 64);
    ReadFile(f, b, *size, &n, NULL);
    CloseHandle(f);
    memset(b + *size, 0, 64);
    return b;
}

static void save(const WCHAR *name, const BYTE *b, DWORD size)
{
    HANDLE f = CreateFileW(name, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    DWORD n;
    WriteFile(f, b, size, &n, NULL);
    CloseHandle(f);
}

struct layout { DWORD pe, opt, sect, nsect, sig, siglen, cor, text_raw, last_raw; };

static DWORD u32(const BYTE *p) { return p[0] | p[1] << 8 | p[2] << 16 | (DWORD)p[3] << 24; }

static void get_layout(const BYTE *b, struct layout *l)
{
    DWORD i, cor_rva, sig_rva;
    l->pe = u32(b + 0x3c);
    l->nsect = b[l->pe + 6] | b[l->pe + 7] << 8;
    l->opt = l->pe + 24;
    l->sect = l->opt + (b[l->pe + 20] | b[l->pe + 21] << 8);
    cor_rva = u32(b + l->opt + 96 + 14 * 8);
    l->cor = 0;
    for (i = 0; i < l->nsect; i++)
    {
        const BYTE *s = b + l->sect + 40 * i;
        DWORD va = u32(s + 12), rs = u32(s + 16), raw = u32(s + 20);
        if (!i) l->text_raw = raw;
        l->last_raw = raw + rs - 1;
        if (cor_rva >= va && cor_rva < va + rs) l->cor = cor_rva - va + raw;
    }
    sig_rva = u32(b + l->cor + 32);
    l->siglen = u32(b + l->cor + 36);
    for (i = 0; i < l->nsect; i++)
    {
        const BYTE *s = b + l->sect + 40 * i;
        DWORD va = u32(s + 12), rs = u32(s + 16), raw = u32(s + 20);
        if (sig_rva >= va && sig_rva < va + rs) l->sig = sig_rva - va + raw;
    }
}

static BOOLEAN verify_bytes(const BYTE *b, DWORD size, BOOLEAN force, BOOLEAN *verified, DWORD *err)
{
    BOOLEAN ret;
    save(L"tamper.dll", b, size);
    *verified = 0xee;
    ret = pVerifyEx(L"tamper.dll", force, verified);
    *err = pErrorInfo();
    return ret;
}

static void expect_reject(const BYTE *orig, DWORD size, const char *what, DWORD off, BYTE xor)
{
    BYTE *c = malloc(size);
    BOOLEAN verified;
    DWORD err;
    memcpy(c, orig, size);
    c[off] ^= xor;
    ok(!verify_bytes(c, size, FALSE, &verified, &err), "%s: tampered image verified\n", what);
    ok(!verified, "%s: was-verified set\n", what);
    ok(err == CORSEC_E_INVALID_STRONGNAME, "%s: error %lx\n", what, err);
    free(c);
}

static void expect_accept(const BYTE *c, DWORD size, const char *what)
{
    BOOLEAN verified;
    DWORD err;
    ok(verify_bytes(c, size, FALSE, &verified, &err), "%s: rejected (%lx)\n", what, err);
    ok(verified == TRUE, "%s: verified %d\n", what, verified);
}

static const BYTE tok1[8] = {0x6e,0xe6,0x6e,0xe7,0x8d,0x66,0x05,0x91};
static const BYTE tok2[8] = {0xf5,0xc1,0x8f,0xee,0x44,0x0c,0xe8,0x2d};

static void test_tokens(const WCHAR *file, const BYTE *tok)
{
    BYTE *token = NULL, *key = NULL, *token2 = NULL;
    ULONG len = 0, klen = 0, len2 = 0;
    BOOLEAN ret;

    ret = pToken(file, &token, &len);
    ok(ret && len == 8 && token && !memcmp(token, tok, 8), "token from %ls: %d len %lu\n", file, ret, len);
    if (ret) pFree(token);

    token = NULL;
    ret = pTokenEx(file, &token, &len, &key, &klen);
    ok(ret && len == 8 && !memcmp(token, tok, 8), "token Ex from %ls\n", file);
    ok(ret && klen >= 160 && key && key[0] == 0x00 && key[1] == 0x24, "key from %ls: len %lu\n", file, klen);
    if (ret && pTokenFromKey(key, klen, &token2, &len2))
    {
        ok(len2 == 8 && !memcmp(token2, tok, 8), "token of returned key differs\n");
        pFree(token2);
    }
    else if (ret) ok(0, "TokenFromPublicKey on the returned key failed\n");
    if (ret) { pFree(token); pFree(key); }
}

int main(void)
{
    HMODULE mod = LoadLibraryA("mscoree.dll");
    BYTE *img, *img2, *unsigned_img, *c;
    DWORD size, size2, usize, i, err, fl;
    struct layout l, l2;
    BOOLEAN ret, verified;
    BYTE *token;
    ULONG len;
    HKEY key;

#define GET(n) p##n = (void *)GetProcAddress(mod, "StrongName" #n)
    GET(ErrorInfo); GET(Free);
    pVerify = (void *)GetProcAddress(mod, "StrongNameSignatureVerification");
    pVerifyEx = (void *)GetProcAddress(mod, "StrongNameSignatureVerificationEx");
    pToken = (void *)GetProcAddress(mod, "StrongNameTokenFromAssembly");
    pTokenEx = (void *)GetProcAddress(mod, "StrongNameTokenFromAssemblyEx");
    pTokenFromKey = (void *)GetProcAddress(mod, "StrongNameTokenFromPublicKey");
    pFree = (void *)GetProcAddress(mod, "StrongNameFreeBuffer");
    ok(pVerify && pVerifyEx && pToken && pTokenEx && pErrorInfo && pFree, "exports missing\n");
    if (!pTokenEx) goto done;

    img = load(L"sgmeta-signed.dll", &size);
    img2 = load(L"sgmeta-signed2.dll", &size2);
    unsigned_img = load(L"sgmeta.dll", &usize);
    ok(img && img2 && unsigned_img, "fixtures\n");
    if (!img || !img2 || !unsigned_img) goto done;
    get_layout(img, &l);
    get_layout(img2, &l2);
    ok(l.siglen == 128 && l2.siglen == 256, "signature sizes %lu %lu\n", l.siglen, l2.siglen);

    /* good files */
    verified = 0xee;
    ret = pVerifyEx(L"sgmeta-signed.dll", FALSE, &verified);
    ok(ret && verified == TRUE, "1024-bit: %d %d err %lx\n", ret, verified, pErrorInfo());
    verified = 0xee;
    ret = pVerifyEx(L"sgmeta-signed2.dll", FALSE, &verified);
    ok(ret && verified == TRUE, "2048-bit: %d %d err %lx\n", ret, verified, pErrorInfo());
    fl = 0xdeadbeef;
    ret = pVerify(L"sgmeta-signed.dll", 0, &fl);
    ok(ret && fl == 1, "SignatureVerification %d flags %lx\n", ret, fl);
    fl = 0xdeadbeef;
    ret = pVerify(L"sgmeta-signed.dll", 1, &fl);
    ok(ret && fl == 1, "forced SignatureVerification %d flags %lx\n", ret, fl);

    /* no signature, missing file, not a managed image */
    verified = 0xee;
    ret = pVerifyEx(L"sgmeta.dll", FALSE, &verified);
    ok(!ret && !verified && pErrorInfo() == CORSEC_E_MISSING_STRONGNAME, "unsigned: %d %d err %lx\n", ret, verified, pErrorInfo());
    fl = 0xdeadbeef;
    ret = pVerify(L"sgmeta.dll", 0, &fl);
    ok(!ret && fl == 0, "unsigned SignatureVerification %d flags %lx\n", ret, fl);
    ret = pVerifyEx(L"nonexistent-file.dll", FALSE, &verified);
    ok(!ret && pErrorInfo() == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND), "missing: %d err %lx\n", ret, pErrorInfo());
    ret = pVerifyEx(L"kernel32.dll", FALSE, &verified);
    ok(!ret, "native dll verified\n");
    {
        BYTE junk[300] = "MZ garbage";
        save(L"junk.dll", junk, sizeof(junk));
        ret = pVerifyEx(L"junk.dll", FALSE, &verified);
        ok(!ret && pErrorInfo() == CORSEC_E_INVALID_IMAGE_FORMAT, "junk: %d err %lx\n", ret, pErrorInfo());
    }

    /* every covered part: a flipped bit makes the signature invalid */
    expect_reject(img, size, "code/metadata", l.text_raw + 0x30, 1);
    expect_reject(img, size, "last section", l.last_raw - 2, 1);
    expect_reject(img, size, "pe timestamp", l.pe + 8, 1);
    expect_reject(img, size, "dos stub", 0x45, 1);
    expect_reject(img, size, "section header", l.sect + 36, 1);
    expect_reject(img, size, "optional header", l.opt + 8, 1);
    expect_reject(img, size, "signature", l.sig, 1);
    expect_reject(img, size, "signature end", l.sig + l.siglen - 1, 0x80);

    /* a signature made with another key does not verify (swap in the 1024-bit file's signature) */
    c = malloc(size);
    memcpy(c, img, size);
    memset(c + l.sig, 0, l.siglen);
    {
        BOOLEAN v;
        ret = verify_bytes(c, size, FALSE, &v, &err);
        ok(!ret && err == CORSEC_E_INVALID_STRONGNAME, "zeroed (delay-signed) signature: %d %lx\n", ret, err);
    }
    memcpy(c, img, size);
    ok(l.siglen == 128, "siglen\n");
    for (i = 0; i < 128; i++) c[l.sig + i] = img[l.sig + 127 - i];
    {
        BOOLEAN v;
        ret = verify_bytes(c, size, FALSE, &v, &err);
        ok(!ret, "byte-reversed signature verified\n");
    }
    /* strong name flag cleared: not signed */
    memcpy(c, img, size);
    c[l.cor + 16] &= ~8;
    {
        BOOLEAN v;
        ret = verify_bytes(c, size, FALSE, &v, &err);
        ok(!ret && err == CORSEC_E_INVALID_STRONGNAME, "flag cleared: %d %lx\n", ret, err);
    }
    /* truncated file */
    {
        BOOLEAN v;
        ret = verify_bytes(img, size - 1, FALSE, &v, &err);
        ok(!ret, "truncated image verified\n");
    }
    free(c);

    /* the checksum, the certificate table entry and data after the sections are not covered */
    c = malloc(size + 64);
    memcpy(c, img, size);
    c[l.opt + 64] ^= 0x55;
    expect_accept(c, size, "checksum");
    memcpy(c, img, size);
    c[l.opt + 96 + 4 * 8] = 0x10;
    c[l.opt + 96 + 4 * 8 + 4] = 8;
    expect_accept(c, size, "certificate entry");
    memcpy(c, img, size);
    memset(c + size, 0xaa, 64);
    expect_accept(c, size + 64, "appended data");
    free(c);

    /* skip-verification registry entries */
    c = malloc(size);
    memcpy(c, img, size);
    c[l.text_raw + 0x30] ^= 1;
    if (!RegCreateKeyExW(HKEY_LOCAL_MACHINE, L"Software\\Microsoft\\StrongName\\Verification\\*,*", 0, NULL, 0, KEY_ALL_ACCESS, NULL, &key, NULL))
    {
        BOOLEAN v;
        RegCloseKey(key);
        ret = verify_bytes(c, size, FALSE, &v, &err);
        ok(ret && v == FALSE, "registered skip: %d %d\n", ret, v);
        ret = verify_bytes(c, size, TRUE, &v, &err);
        ok(!ret && !v, "forced past skip: %d %d\n", ret, v);
        save(L"tamper.dll", c, size);
        fl = 0xdeadbeef;
        ret = pVerify(L"tamper.dll", 0, &fl);
        ok(ret && fl == 0, "SignatureVerification skip: %d %lx\n", ret, fl);
        ret = pVerify(L"tamper.dll", 1, &fl);
        ok(!ret, "SignatureVerification forced past skip\n");
        RegDeleteKeyW(HKEY_LOCAL_MACHINE, L"Software\\Microsoft\\StrongName\\Verification\\*,*");
        ret = verify_bytes(c, size, FALSE, &v, &err);
        ok(!ret, "still skipped after deleting the key\n");
    }
    else ok(0, "cannot create registry key\n");
    if (!RegCreateKeyExW(HKEY_LOCAL_MACHINE, L"Software\\Microsoft\\StrongName\\Verification\\*,f5c18fee440ce82d", 0, NULL, 0, KEY_ALL_ACCESS, NULL, &key, NULL))
    {
        BOOLEAN v;
        RegCloseKey(key);
        ret = verify_bytes(c, size, FALSE, &v, &err);   /* the 1024-bit assembly: token differs */
        ok(!ret, "skip entry for another token applied\n");
        RegDeleteKeyW(HKEY_LOCAL_MACHINE, L"Software\\Microsoft\\StrongName\\Verification\\*,f5c18fee440ce82d");
    }
    free(c);

    /* tokens */
    test_tokens(L"sgmeta-signed.dll", tok1);
    test_tokens(L"sgmeta-signed2.dll", tok2);
    token = NULL;
    ret = pToken(L"sgmeta.dll", &token, &len);
    ok(!ret && pErrorInfo() == CORSEC_E_MISSING_STRONGNAME, "token from unsigned: %d %lx\n", ret, pErrorInfo());
    ret = pToken(L"nonexistent-file.dll", &token, &len);
    ok(!ret && pErrorInfo() == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND), "token from missing: %d %lx\n", ret, pErrorInfo());
    ret = pToken(NULL, &token, &len);
    ok(!ret, "token from NULL\n");

done:
    printf("%d checks, %d failures\n", checks, failures);
    printf(failures ? "RESULT: FAIL\n" : "RESULT: PASS\n");
    return failures != 0;
}
