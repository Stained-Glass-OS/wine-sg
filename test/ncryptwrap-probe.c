/* Key blobs wrapped for another key (patches/sg/2401), run by
 * test/ncryptwrap-gate.sh: NCryptExportKey with an encrypting key and
 * NCryptImportKey with a decrypting key, which both refused to run. An RSA
 * key exported under another RSA key leaves in a blob that does not hold the
 * private key in the clear, needs only ALLOW_EXPORT (not ALLOW_PLAINTEXT_EXPORT),
 * and imports again, with the recipient's private key, into a key that signs
 * the same way. */
#include <windows.h>
#include <bcrypt.h>
#include <ncrypt.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

static int contains(const BYTE *hay, DWORD hlen, const BYTE *needle, DWORD nlen)
{
    DWORD i;
    for (i = 0; i + nlen <= hlen; i++) if (!memcmp(hay + i, needle, nlen)) return 1;
    return 0;
}

static NCRYPT_KEY_HANDLE make_key(NCRYPT_PROV_HANDLE prov, const WCHAR *alg, DWORD len, DWORD policy)
{
    NCRYPT_KEY_HANDLE key;
    if (NCryptCreatePersistedKey(prov, &key, alg, NULL, 0, 0)) return 0;
    if (len) NCryptSetProperty(key, NCRYPT_LENGTH_PROPERTY, (BYTE *)&len, sizeof(len), 0);
    NCryptSetProperty(key, NCRYPT_EXPORT_POLICY_PROPERTY, (BYTE *)&policy, sizeof(policy), 0);
    if (NCryptFinalizeKey(key, 0)) return 0;
    return key;
}

int main(void)
{
    NCRYPT_PROV_HANDLE prov;
    NCRYPT_KEY_HANDLE kek, other_kek, aes, target, restored, ec, ec2;
    BCRYPT_PKCS1_PADDING_INFO pad = { BCRYPT_SHA256_ALGORITHM };
    BYTE hash[32], sig[512], pub1[1024], pub2[1024], priv[4096], *wrapped, *bad;
    DWORD i, sigsize, pub1len, pub2len, privlen, wlen, wlen2;
    SECURITY_STATUS ret;

    for (i = 0; i < sizeof(hash); i++) hash[i] = (BYTE)(i * 7 + 1);

    check(!NCryptOpenStorageProvider(&prov, MS_KEY_STORAGE_PROVIDER, 0), "the software provider");
    kek = make_key(prov, BCRYPT_RSA_ALGORITHM, 2048, (NCRYPT_ALLOW_EXPORT_FLAG | NCRYPT_ALLOW_PLAINTEXT_EXPORT_FLAG));
    other_kek = make_key(prov, BCRYPT_RSA_ALGORITHM, 2048, (NCRYPT_ALLOW_EXPORT_FLAG | NCRYPT_ALLOW_PLAINTEXT_EXPORT_FLAG));
    aes = make_key(prov, BCRYPT_AES_ALGORITHM, 0, (NCRYPT_ALLOW_EXPORT_FLAG | NCRYPT_ALLOW_PLAINTEXT_EXPORT_FLAG));
    check(kek && other_kek && aes, "two RSA wrapping keys and an AES key");

    /* the key to move: exportable, but not in plain text */
    target = make_key(prov, BCRYPT_RSA_ALGORITHM, 2048, NCRYPT_ALLOW_EXPORT_FLAG);
    check(target != 0, "an RSA key with ALLOW_EXPORT only");
    check(!NCryptExportKey(target, 0, BCRYPT_RSAPUBLIC_BLOB, NULL, pub1, sizeof(pub1), &pub1len, 0), "its public blob");
    ret = NCryptExportKey(target, 0, BCRYPT_RSAFULLPRIVATE_BLOB, NULL, priv, sizeof(priv), &privlen, 0);
    check(ret == NTE_NOT_SUPPORTED, "the private key does not leave in the clear");

    wlen = 0;
    ret = NCryptExportKey(target, kek, BCRYPT_RSAFULLPRIVATE_BLOB, NULL, NULL, 0, &wlen, 0);
    check(!ret && wlen > 256, "wrapped export sizes the output");
    wrapped = malloc(wlen);
    ret = NCryptExportKey(target, kek, BCRYPT_RSAFULLPRIVATE_BLOB, NULL, wrapped, wlen - 1, &wlen2, 0);
    check(ret == NTE_BUFFER_TOO_SMALL && wlen2 == wlen, "a short buffer: NTE_BUFFER_TOO_SMALL and the size");
    ret = NCryptExportKey(target, kek, BCRYPT_RSAFULLPRIVATE_BLOB, NULL, wrapped, wlen, &wlen2, 0);
    check(!ret && wlen2 <= wlen && wlen2 > 256, "wrapped export of the private key succeeds");
    wlen = wlen2;

    /* the clear private blob, got through a key that allows it, must not appear inside the wrapped one */
    {
        NCRYPT_KEY_HANDLE open = make_key(prov, BCRYPT_RSA_ALGORITHM, 2048, (NCRYPT_ALLOW_EXPORT_FLAG | NCRYPT_ALLOW_PLAINTEXT_EXPORT_FLAG));
        BYTE clear[4096];
        DWORD clearlen;
        check(!NCryptExportKey(open, 0, BCRYPT_RSAFULLPRIVATE_BLOB, NULL, clear, sizeof(clear), &clearlen, 0), "a plain export, for comparison");
        {
            NCRYPT_KEY_HANDLE kek2 = kek;
            BYTE *w2 = malloc(8192);
            DWORD w2len;
            check(!NCryptExportKey(open, kek2, BCRYPT_RSAFULLPRIVATE_BLOB, NULL, w2, 8192, &w2len, 0), "wrapped export of that key");
            /* the 128-byte prime P sits at offset 539 (header 24 + 3 + 256 + 256 + ...); any 32 of the private bytes will do */
            check(clearlen > 600 && !contains(w2, w2len, clear + clearlen - 64, 32), "no private key bytes in the clear inside the wrapped blob");
            free(w2);
        }
        NCryptFreeObject(open);
    }
    check(!contains(wrapped, wlen, pub1 + 24 + 3, 32), "the wrapped blob is not the plain blob (modulus bytes absent)");

    /* import into a new key, with the recipient's private key */
    ret = NCryptImportKey(prov, kek, BCRYPT_RSAFULLPRIVATE_BLOB, NULL, &restored, wrapped, wlen, 0);
    check(!ret && restored, "import with the decrypting key");
    if (!ret)
    {
        check(!NCryptExportKey(restored, 0, BCRYPT_RSAPUBLIC_BLOB, NULL, pub2, sizeof(pub2), &pub2len, 0) &&
              pub2len == pub1len && !memcmp(pub1, pub2, pub1len), "the imported key has the same public key");
        check(!NCryptSignHash(restored, &pad, hash, sizeof(hash), sig, sizeof(sig), &sigsize, BCRYPT_PAD_PKCS1) && sigsize == 256,
              "it signs");
        {
            /* verify with the original key (same modulus) */
            ret = NCryptVerifySignature(target, &pad, hash, sizeof(hash), sig, sigsize, BCRYPT_PAD_PKCS1);
            check(!ret, "the original key verifies the imported key's signature");
        }
        NCryptFreeObject(restored);
    }

    /* things that must not import */
    restored = 0;
    ret = NCryptImportKey(prov, other_kek, BCRYPT_RSAFULLPRIVATE_BLOB, NULL, &restored, wrapped, wlen, 0);
    check(ret != 0 && !restored, "the wrong decrypting key fails");
    bad = malloc(wlen);
    memcpy(bad, wrapped, wlen);
    bad[0] ^= 0x55;
    ret = NCryptImportKey(prov, kek, BCRYPT_RSAFULLPRIVATE_BLOB, NULL, &restored, bad, wlen, 0);
    check(ret == NTE_INVALID_PARAMETER, "a blob with the wrong magic: NTE_INVALID_PARAMETER");
    ret = NCryptImportKey(prov, kek, BCRYPT_RSAFULLPRIVATE_BLOB, NULL, &restored, wrapped, 8, 0);
    check(ret == NTE_INVALID_PARAMETER, "a blob shorter than its header: NTE_INVALID_PARAMETER");
    ret = NCryptImportKey(prov, kek, BCRYPT_RSAFULLPRIVATE_BLOB, NULL, &restored, wrapped, wlen - 20, 0);
    check(ret == NTE_INVALID_PARAMETER, "a truncated blob: NTE_INVALID_PARAMETER");
    memcpy(bad, wrapped, wlen);
    bad[40] ^= 0xff;  /* inside the RSA-wrapped AES key */
    bad[41] ^= 0xff;
    ret = NCryptImportKey(prov, kek, BCRYPT_RSAFULLPRIVATE_BLOB, NULL, &restored, bad, wlen, 0);
    check(ret != 0, "a damaged wrapped key fails");
    free(bad);
    free(wrapped);

    /* the key policy for wrapping */
    {
        NCRYPT_KEY_HANDLE locked = make_key(prov, BCRYPT_RSA_ALGORITHM, 2048, 0);
        BYTE tmp[8192];
        DWORD tmplen;
        ret = NCryptExportKey(locked, kek, BCRYPT_RSAFULLPRIVATE_BLOB, NULL, tmp, sizeof(tmp), &tmplen, 0);
        check(ret == NTE_NOT_SUPPORTED, "a key that is not exportable at all is not wrapped either");
        NCryptFreeObject(locked);

        ret = NCryptExportKey(target, aes, BCRYPT_RSAFULLPRIVATE_BLOB, NULL, tmp, sizeof(tmp), &tmplen, 0);
        check(ret == NTE_NOT_SUPPORTED, "wrapping under an AES key: NTE_NOT_SUPPORTED");
        ret = NCryptExportKey(target, 0x1234, BCRYPT_RSAFULLPRIVATE_BLOB, NULL, tmp, sizeof(tmp), &tmplen, 0);
        check(ret == NTE_INVALID_HANDLE, "a wrapping key that is no key: NTE_INVALID_HANDLE");
    }

    /* an ECDSA key goes the same way */
    ec = make_key(prov, L"ECDSA_P256", 0, NCRYPT_ALLOW_EXPORT_FLAG);
    check(ec != 0, "an ECDSA_P256 key");
    {
        BYTE ecw[4096], ecpub1[256], ecpub2[256];
        DWORD ecwlen, p1, p2;
        check(!NCryptExportKey(ec, kek, BCRYPT_ECCPRIVATE_BLOB, NULL, ecw, sizeof(ecw), &ecwlen, 0), "ECDSA key wrapped");
        check(!NCryptImportKey(prov, kek, BCRYPT_ECCPRIVATE_BLOB, NULL, &ec2, ecw, ecwlen, 0), "ECDSA key imported");
        check(!NCryptExportKey(ec, 0, BCRYPT_ECCPUBLIC_BLOB, NULL, ecpub1, sizeof(ecpub1), &p1, 0) &&
              !NCryptExportKey(ec2, 0, BCRYPT_ECCPUBLIC_BLOB, NULL, ecpub2, sizeof(ecpub2), &p2, 0) &&
              p1 == p2 && !memcmp(ecpub1, ecpub2, p1), "same ECDSA public key");
        NCryptFreeObject(ec2);
    }

    NCryptFreeObject(ec);
    NCryptFreeObject(target);
    NCryptFreeObject(aes);
    NCryptFreeObject(other_kek);
    NCryptFreeObject(kek);
    NCryptFreeObject(prov);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
