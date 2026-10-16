/* 5907 (CORE review: security): the export policy of NCryptExportKey for PKCS #8 private key blobs.
 * In the clear a key must allow plaintext export (NCRYPT_ALLOW_PLAINTEXT_EXPORT_FLAG); encrypted under a
 * secret (NCRYPTBUFFER_PKCS_SECRET) NCRYPT_ALLOW_EXPORT_FLAG is enough, as on Windows. */
#include <windows.h>
#include <ncrypt.h>
#include <stdio.h>

static int failures;

static SECURITY_STATUS export_p8(NCRYPT_KEY_HANDLE key, BOOL encrypted)
{
    static WCHAR secret[] = L"correct horse";
    NCryptBuffer buf = { sizeof(secret), 46 /* NCRYPTBUFFER_PKCS_SECRET */, secret };
    NCryptBufferDesc desc = { 0 /* NCRYPTBUFFER_VERSION */, 1, &buf };
    BYTE out[8192];
    DWORD len = 0;
    SECURITY_STATUS ret = NCryptExportKey(key, 0, L"PKCS8_PRIVATEKEY", encrypted ? &desc : NULL,
                                          out, sizeof(out), &len, 0);
    if (!ret && len < 100) { printf("FAIL short blob %lu\n", len); failures++; }
    return ret;
}

static void check(DWORD policy, BOOL plain_ok, BOOL enc_ok)
{
    NCRYPT_PROV_HANDLE prov;
    NCRYPT_KEY_HANDLE key;
    SECURITY_STATUS ret;

    if (NCryptOpenStorageProvider(&prov, MS_KEY_STORAGE_PROVIDER, 0)) { printf("FAIL provider\n"); failures++; return; }
    ret = NCryptCreatePersistedKey(prov, &key, BCRYPT_RSA_ALGORITHM, NULL, 0, 0);
    if (!ret) ret = NCryptSetProperty(key, NCRYPT_EXPORT_POLICY_PROPERTY, (BYTE *)&policy, sizeof(policy), 0);
    if (!ret) ret = NCryptFinalizeKey(key, 0);
    if (ret) { printf("FAIL key setup %#lx\n", ret); failures++; NCryptFreeObject(prov); return; }

    ret = export_p8(key, FALSE);
    if (!ret != plain_ok) { printf("FAIL policy %#lx: plaintext export %#lx\n", policy, ret); failures++; }
    else printf("ok   policy %#lx: plaintext %s\n", policy, ret ? "refused" : "exported");
    if (ret && ret != NTE_NOT_SUPPORTED) { printf("FAIL policy %#lx: plaintext error %#lx\n", policy, ret); failures++; }

    ret = export_p8(key, TRUE);
    if (!ret != enc_ok) { printf("FAIL policy %#lx: encrypted export %#lx\n", policy, ret); failures++; }
    else printf("ok   policy %#lx: encrypted %s\n", policy, ret ? "refused" : "exported");
    if (ret && ret != NTE_NOT_SUPPORTED) { printf("FAIL policy %#lx: encrypted error %#lx\n", policy, ret); failures++; }

    NCryptFreeObject(key);
    NCryptFreeObject(prov);
}

int main(void)
{
    check(0, FALSE, FALSE);
    check(NCRYPT_ALLOW_EXPORT_FLAG, FALSE, TRUE);
    check(NCRYPT_ALLOW_EXPORT_FLAG | NCRYPT_ALLOW_PLAINTEXT_EXPORT_FLAG, TRUE, TRUE);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
