/* ncryptecp8-probe (wine-sg 8003): PKCS #8 for EC keys in ncrypt. NCryptExportKey(PKCS8_PRIVATEKEY) of a
 * P-256/P-384 key encrypted under a secret (NCRYPTBUFFER_PKCS_SECRET, optionally NCRYPTBUFFER_PKCS_ALG_OID)
 * was NTE_NOT_SUPPORTED, and NCryptImportKey of an EC PrivateKeyInfo (plain or encrypted) too -- .NET's
 * ExportEncryptedPkcs8PrivateKey/ImportPkcs8PrivateKey and PFX export of EC certificates use them. The key
 * is exported, imported back and the imported key verifies the original's signature. */
#include <windows.h>
#include <ncrypt.h>
#include <stdio.h>

static int failures;
static void check(int ok, const char *what, long v)
{
    printf("%s %s (%#lx)\n", ok ? "ok  " : "FAIL", what, v);
    if (!ok) failures++;
}

static SECURITY_STATUS export_p8(NCRYPT_KEY_HANDLE key, WCHAR *secret, char *alg_oid, BYTE *out, DWORD *len)
{
    NCryptBuffer bufs[2];
    NCryptBufferDesc desc = { 0, 0, bufs };
    if (secret)
    {
        bufs[desc.cBuffers].cbBuffer = (lstrlenW(secret) + 1) * sizeof(WCHAR);
        bufs[desc.cBuffers].BufferType = NCRYPTBUFFER_PKCS_SECRET;
        bufs[desc.cBuffers++].pvBuffer = secret;
    }
    if (alg_oid)
    {
        bufs[desc.cBuffers].cbBuffer = lstrlenA(alg_oid) + 1;
        bufs[desc.cBuffers].BufferType = NCRYPTBUFFER_PKCS_ALG_OID;
        bufs[desc.cBuffers++].pvBuffer = alg_oid;
    }
    return NCryptExportKey(key, 0, NCRYPT_PKCS8_PRIVATE_KEY_BLOB, desc.cBuffers ? &desc : NULL, out, 8192, len, 0);
}

static SECURITY_STATUS import_p8(NCRYPT_PROV_HANDLE prov, WCHAR *secret, BYTE *data, DWORD len, NCRYPT_KEY_HANDLE *key)
{
    NCryptBuffer buf = { 0, NCRYPTBUFFER_PKCS_SECRET, secret };
    NCryptBufferDesc desc = { 0, 1, &buf };
    if (secret) buf.cbBuffer = (lstrlenW(secret) + 1) * sizeof(WCHAR);
    return NCryptImportKey(prov, 0, NCRYPT_PKCS8_PRIVATE_KEY_BLOB, secret ? &desc : NULL, key, data, len, 0);
}

static void test_curve(NCRYPT_PROV_HANDLE prov, const WCHAR *alg, const char *name, DWORD policy)
{
    static WCHAR secret[] = L"correct horse", wrong[] = L"wrong horse";
    NCRYPT_KEY_HANDLE key = 0, imported = 0, plain = 0;
    BYTE out[8192], hash[32], sig[256];
    DWORD len = 0, sig_len = 0;
    char what[128];
    SECURITY_STATUS ret;
    int i;

    for (i = 0; i < 32; i++) hash[i] = i * 3;
    ret = NCryptCreatePersistedKey(prov, &key, alg, NULL, 0, 0);
    if (!ret) ret = NCryptSetProperty(key, NCRYPT_EXPORT_POLICY_PROPERTY, (BYTE *)&policy, sizeof(policy), 0);
    if (!ret) ret = NCryptFinalizeKey(key, 0);
    snprintf(what, sizeof(what), "%s key", name);
    check(!ret, what, ret);
    if (ret) return;
    ret = NCryptSignHash(key, NULL, hash, sizeof(hash), sig, sizeof(sig), &sig_len, 0);
    snprintf(what, sizeof(what), "%s signs", name);
    check(!ret && sig_len, what, ret);

    ret = export_p8(key, secret, NULL, out, &len);
    snprintf(what, sizeof(what), "%s: encrypted PKCS #8 export (PBES2 default)", name);
    check(!ret && len > 100 && out[0] == 0x30, what, ret);
    if (!ret)
    {
        ret = import_p8(prov, wrong, out, len, &imported);
        snprintf(what, sizeof(what), "%s: the wrong secret does not import it", name);
        check(ret != ERROR_SUCCESS, what, ret);
        if (!ret) NCryptFreeObject(imported);
        imported = 0;
        ret = import_p8(prov, secret, out, len, &imported);
        snprintf(what, sizeof(what), "%s: imported back with the secret", name);
        check(!ret && imported, what, ret);
        if (!ret)
        {
            ret = NCryptVerifySignature(imported, NULL, hash, sizeof(hash), sig, sig_len, 0);
            snprintf(what, sizeof(what), "%s: the imported key verifies the original's signature", name);
            check(!ret, what, ret);
            NCryptFreeObject(imported);
        }
    }
    ret = export_p8(key, secret, (char *)"2.16.840.1.101.3.4.1.2", out, &len);
    snprintf(what, sizeof(what), "%s: encrypted export with AES-128-CBC named", name);
    check(!ret && len > 100, what, ret);
    if (!ret)
    {
        imported = 0;
        ret = import_p8(prov, secret, out, len, &imported);
        snprintf(what, sizeof(what), "%s: and imported back", name);
        check(!ret, what, ret);
        if (!ret) NCryptFreeObject(imported);
    }
    if (policy & NCRYPT_ALLOW_PLAINTEXT_EXPORT_FLAG)
    {
        ret = export_p8(key, NULL, NULL, out, &len);
        snprintf(what, sizeof(what), "%s: plain PKCS #8 export", name);
        check(!ret && len > 50, what, ret);
        if (!ret)
        {
            ret = import_p8(prov, NULL, out, len, &plain);
            snprintf(what, sizeof(what), "%s: plain PKCS #8 import verifies the signature", name);
            check(!ret && !NCryptVerifySignature(plain, NULL, hash, sizeof(hash), sig, sig_len, 0), what, ret);
            if (!ret) NCryptFreeObject(plain);
        }
    }
    else
    {
        ret = export_p8(key, NULL, NULL, out, &len);
        snprintf(what, sizeof(what), "%s: no plaintext export without the plaintext flag", name);
        check(ret == NTE_NOT_SUPPORTED, what, ret);
    }
    NCryptFreeObject(key);
}

int main(void)
{
    NCRYPT_PROV_HANDLE prov;

    if (NCryptOpenStorageProvider(&prov, MS_KEY_STORAGE_PROVIDER, 0)) { printf("FAIL provider\nRESULT: FAIL\n"); return 1; }
    test_curve(prov, BCRYPT_ECDSA_P256_ALGORITHM, "P-256", NCRYPT_ALLOW_EXPORT_FLAG | NCRYPT_ALLOW_PLAINTEXT_EXPORT_FLAG);
    test_curve(prov, BCRYPT_ECDSA_P384_ALGORITHM, "P-384", NCRYPT_ALLOW_EXPORT_FLAG);
    NCryptFreeObject(prov);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
