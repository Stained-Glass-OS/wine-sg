/* oidcng-probe: the CNG algorithm names of built-in OIDs (patches/sg/1532).
 * Windows gives the hash, encryption, public key and signature OIDs a
 * pwszCNGAlgid (a signature's public key in pwszCNGExtraAlgid, "" where
 * there is none); Access reads the hash's as it checks a database's
 * signature. Prints "<oid> <cng>|<extra>" per line. */
#define CRYPT_OID_INFO_HAS_EXTRA_FIELDS
#include <windows.h>
#include <wincrypt.h>
#include <stdio.h>

static void show(const char *oid, DWORD group)
{
    PCCRYPT_OID_INFO info = CryptFindOIDInfo(CRYPT_OID_INFO_OID_KEY, (void *)oid, group);
    if (!info) { printf("%s none\n", oid); return; }
    printf("%s %ls|%ls\n", oid, info->pwszCNGAlgid ? info->pwszCNGAlgid : L"(null)",
           info->pwszCNGExtraAlgid ? info->pwszCNGExtraAlgid : L"(null)");
}

int main(void)
{
    show(szOID_NIST_sha256, CRYPT_HASH_ALG_OID_GROUP_ID);
    show(szOID_OIWSEC_sha1, CRYPT_HASH_ALG_OID_GROUP_ID);
    show(szOID_RSA_MD5, CRYPT_HASH_ALG_OID_GROUP_ID);
    show(szOID_RSA_SHA256RSA, CRYPT_SIGN_ALG_OID_GROUP_ID);
    show(szOID_RSA_RSA, CRYPT_PUBKEY_ALG_OID_GROUP_ID);
    show(szOID_RSA_DES_EDE3_CBC, CRYPT_ENCRYPT_ALG_OID_GROUP_ID);
    return 0;
}
