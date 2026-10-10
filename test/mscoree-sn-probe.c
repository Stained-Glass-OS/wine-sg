/* mscoree strong name key functions (patches/sg/2428), run by test/mscoree-sn-gate.sh:
 * public key tokens against a vector computed outside Wine, key sizes, generated
 * key pairs, public keys from private blobs and from containers. */
#include <windows.h>
#include <stdio.h>
#include <string.h>

static const BYTE fixed_key[] = {
    0x00, 0x24, 0x00, 0x00, 0x04, 0x80, 0x00, 0x00, 0x94, 0x00, 0x00, 0x00, 0x06, 0x02, 0x00, 0x00, 0x00, 0x24, 0x00, 0x00, 0x52, 0x53, 0x41, 0x31, 0x00, 0x04, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x08, 0x0f, 0x16, 0x1d, 0x24, 0x2b, 0x32, 0x39, 0x40, 0x47, 0x4e, 0x55, 0x5c, 0x63, 0x6a, 0x71, 0x78, 0x7f, 0x86, 0x8d, 0x94, 0x9b, 0xa2, 0xa9, 0xb0, 0xb7, 0xbe, 0xc5, 0xcc, 0xd3, 0xda, 0xe1, 0xe8, 0xef, 0xf6, 0xfd, 0x04, 0x0b, 0x12, 0x19, 0x20, 0x27, 0x2e, 0x35, 0x3c, 0x43, 0x4a, 0x51, 0x58, 0x5f, 0x66, 0x6d, 0x74, 0x7b, 0x82, 0x89, 0x90, 0x97, 0x9e, 0xa5, 0xac, 0xb3, 0xba, 0xc1, 0xc8, 0xcf, 0xd6, 0xdd, 0xe4, 0xeb, 0xf2, 0xf9, 0x00, 0x07, 0x0e, 0x15, 0x1c, 0x23, 0x2a, 0x31, 0x38, 0x3f, 0x46, 0x4d, 0x54, 0x5b, 0x62, 0x69, 0x70, 0x77, 0x7e, 0x85, 0x8c, 0x93, 0x9a, 0xa1, 0xa8, 0xaf, 0xb6, 0xbd, 0xc4, 0xcb, 0xd2, 0xd9, 0xe0, 0xe7, 0xee, 0xf5, 0xfc, 0x03, 0x0a, 0x11, 0x18, 0x1f, 0x26, 0x2d, 0x34, 0x3b, 0x42, 0x49, 0x50, 0x57, 0x5e, 0x65, 0x6c, 0x73, 0xfa
};
static const BYTE fixed_token[8] = { 0xc0, 0x47, 0xbe, 0x6c, 0x76, 0xff, 0x91, 0x7e };

#define CORSEC_E_INVALID_PUBLICKEY ((HRESULT)0x8013141e)
#define SN_LEAVE_KEY 1

static BOOLEAN (WINAPI *StrongNameKeyGen)(const WCHAR *, DWORD, BYTE **, ULONG *);
static BOOLEAN (WINAPI *StrongNameKeyGenEx)(const WCHAR *, DWORD, DWORD, BYTE **, ULONG *);
static BOOLEAN (WINAPI *StrongNameKeyDelete)(const WCHAR *);
static BOOLEAN (WINAPI *StrongNameKeyInstall)(const WCHAR *, BYTE *, ULONG);
static BOOLEAN (WINAPI *StrongNameGetPublicKey)(const WCHAR *, BYTE *, ULONG, BYTE **, ULONG *);
static BOOLEAN (WINAPI *StrongNameSignatureSize)(BYTE *, ULONG, ULONG *);
static BOOLEAN (WINAPI *StrongNameTokenFromPublicKey)(BYTE *, ULONG, BYTE **, ULONG *);
static BOOLEAN (WINAPI *StrongNameHashSize)(ULONG, DWORD *);
static DWORD (WINAPI *StrongNameErrorInfo)(void);
static void (WINAPI *StrongNameFreeBuffer)(BYTE *);

static int failures;
#define CHECK(c, ...) do { if (!(c)) { failures++; printf("FAIL  " __VA_ARGS__); printf("\n"); } } while (0)

static void check_generated(const char *name, DWORD bits, BYTE *priv, ULONG priv_len)
{
    BYTE *pub = NULL, *tok = NULL;
    ULONG pub_len = 0, tok_len = 0, sigsize = 0;
    DWORD mod = bits / 8;

    CHECK(priv[0] == 7, "%s: blob type %u", name, priv[0]);
    CHECK(*(DWORD *)(priv + 4) == 0x2400, "%s: key algorithm %#lx", name, *(DWORD *)(priv + 4));
    CHECK(*(DWORD *)(priv + 12) == bits, "%s: bit length %lu", name, *(DWORD *)(priv + 12));
    /* 9 half-size numbers beyond the header: modulus (2), p, q, dp, dq, iq (5), d (2) */
    CHECK(priv_len == 20 + mod * 9 / 2, "%s: private blob size %lu", name, priv_len);

    CHECK(StrongNameGetPublicKey(NULL, priv, priv_len, &pub, &pub_len), "%s: public key from blob (%#lx)", name, StrongNameErrorInfo());
    if (!pub) return;
    CHECK(*(DWORD *)pub == 0x2400 && *(DWORD *)(pub + 4) == 0x8004, "%s: algorithms %#lx %#lx", name, *(DWORD *)pub, *(DWORD *)(pub + 4));
    CHECK(*(DWORD *)(pub + 8) == pub_len - 12, "%s: length field %lu", name, *(DWORD *)(pub + 8));
    CHECK(pub_len == 12 + 20 + mod, "%s: public key size %lu", name, pub_len);
    CHECK(pub[12] == 6, "%s: public blob type %u", name, pub[12]);
    CHECK(!memcmp(pub + 12 + 20, priv + 20, mod), "%s: modulus differs", name);
    CHECK(StrongNameSignatureSize(pub, pub_len, &sigsize) && sigsize == mod, "%s: signature size %lu", name, sigsize);
    CHECK(StrongNameTokenFromPublicKey(pub, pub_len, &tok, &tok_len) && tok_len == 8, "%s: token", name);
    if (tok) StrongNameFreeBuffer(tok);
    StrongNameFreeBuffer(pub);
}

int main(void)
{
    HMODULE m = LoadLibraryA("mscoree.dll");
    static const WCHAR container[] = L"SgProbeContainer", container2[] = L"SgProbeContainer2";
    BYTE *buf = NULL, *tok = NULL, *pub1 = NULL, *pub2 = NULL, *priv = NULL;
    ULONG len = 0, tok_len = 0, pub1_len = 0, pub2_len = 0, priv_len = 0, size;
    DWORD hash;
    BYTE bad[sizeof(fixed_key)];

#define L(f) f = (void *)GetProcAddress(m, #f)
    L(StrongNameKeyGen); L(StrongNameKeyGenEx); L(StrongNameKeyDelete); L(StrongNameKeyInstall);
    L(StrongNameGetPublicKey); L(StrongNameSignatureSize); L(StrongNameTokenFromPublicKey);
    L(StrongNameHashSize); L(StrongNameErrorInfo); L(StrongNameFreeBuffer);
#undef L
    if (!StrongNameKeyGen || !StrongNameFreeBuffer) { printf("FAIL  exports missing\n"); return 1; }

    /* the token: last eight bytes of the SHA-1 of the whole blob, reversed */
    CHECK(StrongNameTokenFromPublicKey((BYTE *)fixed_key, sizeof(fixed_key), &tok, &tok_len), "token call");
    CHECK(tok_len == 8 && tok && !memcmp(tok, fixed_token, 8), "token value");
    if (tok) StrongNameFreeBuffer(tok);

    size = 0;
    CHECK(StrongNameSignatureSize((BYTE *)fixed_key, sizeof(fixed_key), &size) && size == 128, "signature size %lu", size);

    /* malformed keys */
    CHECK(!StrongNameSignatureSize((BYTE *)fixed_key, sizeof(fixed_key) - 1, &size), "short key accepted");
    CHECK(StrongNameErrorInfo() == (DWORD)CORSEC_E_INVALID_PUBLICKEY, "short key error %#lx", StrongNameErrorInfo());
    memcpy(bad, fixed_key, sizeof(bad)); bad[12] = 7;
    CHECK(!StrongNameTokenFromPublicKey(bad, sizeof(bad), &tok, &tok_len), "private type accepted");
    CHECK(!StrongNameTokenFromPublicKey(NULL, 4, &tok, &tok_len), "NULL key accepted");
    CHECK(StrongNameErrorInfo() == (DWORD)E_POINTER, "NULL key error %#lx", StrongNameErrorInfo());

    /* hash sizes */
    CHECK(StrongNameHashSize(0, &hash) && hash == 20, "default hash size %lu", hash);
    CHECK(StrongNameHashSize(0x8004, &hash) && hash == 20, "sha1 %lu", hash);
    CHECK(StrongNameHashSize(0x8003, &hash) && hash == 16, "md5 %lu", hash);
    CHECK(StrongNameHashSize(0x800c, &hash) && hash == 32, "sha256 %lu", hash);
    CHECK(!StrongNameHashSize(0xdead, &hash), "bad algorithm accepted");
    CHECK(!StrongNameHashSize(0, NULL) && StrongNameErrorInfo() == (DWORD)E_POINTER, "NULL size");

    /* generated pairs */
    CHECK(StrongNameKeyGen(NULL, 0, &priv, &priv_len), "key gen (%#lx)", StrongNameErrorInfo());
    if (priv) { check_generated("1024", 1024, priv, priv_len); StrongNameFreeBuffer(priv); priv = NULL; }
    CHECK(StrongNameKeyGenEx(NULL, 0, 512, &priv, &priv_len), "key gen 512 (%#lx)", StrongNameErrorInfo());
    if (priv) { check_generated("512", 512, priv, priv_len); StrongNameFreeBuffer(priv); priv = NULL; }
    CHECK(StrongNameKeyGenEx(NULL, 0, 2048, &priv, &priv_len), "key gen 2048 (%#lx)", StrongNameErrorInfo());
    if (priv) { check_generated("2048", 2048, priv, priv_len); StrongNameFreeBuffer(priv); priv = NULL; }
    CHECK(!StrongNameKeyGen(NULL, 0, NULL, &priv_len), "NULL output accepted");

    /* containers: install, read back, delete */
    CHECK(StrongNameKeyGen(NULL, 0, &priv, &priv_len), "key gen for container");
    CHECK(StrongNameKeyInstall(container, priv, priv_len), "install (%#lx)", StrongNameErrorInfo());
    CHECK(StrongNameGetPublicKey(NULL, priv, priv_len, &pub1, &pub1_len), "public from blob");
    CHECK(StrongNameGetPublicKey(container, NULL, 0, &pub2, &pub2_len), "public from container (%#lx)", StrongNameErrorInfo());
    CHECK(pub1 && pub2 && pub1_len == pub2_len && !memcmp(pub1, pub2, pub1_len), "container key differs");
    if (pub1) StrongNameFreeBuffer(pub1);
    if (pub2) StrongNameFreeBuffer(pub2);
    CHECK(StrongNameKeyDelete(container), "delete (%#lx)", StrongNameErrorInfo());
    CHECK(!StrongNameGetPublicKey(container, NULL, 0, &pub2, &pub2_len), "deleted container still answers");
    CHECK(!StrongNameKeyDelete(container), "second delete succeeded");
    StrongNameFreeBuffer(priv); priv = NULL;

    /* a named container keeps the key only when asked to */
    CHECK(StrongNameKeyGen(container2, 0, &priv, &priv_len), "key gen into container");
    if (priv) StrongNameFreeBuffer(priv);
    CHECK(!StrongNameGetPublicKey(container2, NULL, 0, &pub2, &pub2_len), "key kept without SN_LEAVE_KEY");
    CHECK(StrongNameKeyGen(container2, SN_LEAVE_KEY, &priv, &priv_len), "key gen leaving the key");
    if (priv) StrongNameFreeBuffer(priv);
    CHECK(StrongNameGetPublicKey(container2, NULL, 0, &pub2, &pub2_len), "key gone despite SN_LEAVE_KEY");
    if (pub2) StrongNameFreeBuffer(pub2);
    StrongNameKeyDelete(container2);

    printf(failures ? "RESULT: FAIL\n" : "RESULT: PASS\n");
    return failures != 0;
}
