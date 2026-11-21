/* bcryptpubinfo-probe: crypt32 CryptExportPublicKeyInfoFromBCryptKeyHandle, which was not exported (Stained
 * Glass OS wine-sg 8266; test/bcryptpubinfo-gate.sh): the public key info of a CNG RSA or P-256 key, which
 * CryptImportPublicKeyInfoEx2 turns back into the same public key.
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#include <windows.h>
#include <wincrypt.h>
#include <bcrypt.h>
#include <stdio.h>
#include <string.h>

static int failures;
static BOOL (WINAPI *pExport)(BCRYPT_KEY_HANDLE, DWORD, LPSTR, DWORD, void *, PCERT_PUBLIC_KEY_INFO, DWORD *);

static void roundtrip( const WCHAR *alg_name, ULONG bits, const char *want_oid, const WCHAR *blob_type )
{
    BCRYPT_ALG_HANDLE alg;
    BCRYPT_KEY_HANDLE key, back = NULL;
    BYTE buf[2048], a[1024], b[1024];
    CERT_PUBLIC_KEY_INFO *info = (void *)buf;
    DWORD size = 0;
    ULONG la = 0, lb = 0;
    BOOL ret;

    BCryptOpenAlgorithmProvider( &alg, alg_name, NULL, 0 );
    BCryptGenerateKeyPair( alg, &key, bits, 0 );
    BCryptFinalizeKeyPair( key, 0 );
    ret = pExport( key, X509_ASN_ENCODING, NULL, 0, NULL, NULL, &size );
    printf( "%s  %ls: size query (%lu)\n", ret && size > sizeof(*info) ? "PASS" : "FAIL", alg_name, size );
    if (!ret || size <= sizeof(*info)) failures++;
    size = sizeof(buf);
    ret = pExport( key, X509_ASN_ENCODING, NULL, 0, NULL, info, &size );
    printf( "%s  %ls: algorithm %s\n", ret && !strcmp( info->Algorithm.pszObjId, want_oid ) ? "PASS" : "FAIL", alg_name, ret ? info->Algorithm.pszObjId : "-" );
    if (!ret || strcmp( info->Algorithm.pszObjId, want_oid )) failures++;
    ret = ret && CryptImportPublicKeyInfoEx2( X509_ASN_ENCODING, info, 0, NULL, &back );
    if (ret)
    {
        BCryptExportKey( key, NULL, blob_type, a, sizeof(a), &la, 0 );
        BCryptExportKey( back, NULL, blob_type, b, sizeof(b), &lb, 0 );
    }
    printf( "%s  %ls: CryptImportPublicKeyInfoEx2 gives the same public key\n", ret && la && la == lb && !memcmp( a, b, la ) ? "PASS" : "FAIL", alg_name );
    if (!(ret && la && la == lb && !memcmp( a, b, la ))) failures++;
}

int main( void )
{
    pExport = (void *)GetProcAddress( LoadLibraryA( "crypt32.dll" ), "CryptExportPublicKeyInfoFromBCryptKeyHandle" );
    if (!pExport) { printf( "FAIL  crypt32 exports it\nRESULT: FAIL\n" ); return 1; }
    roundtrip( BCRYPT_RSA_ALGORITHM, 1024, szOID_RSA_RSA, BCRYPT_RSAPUBLIC_BLOB );
    roundtrip( BCRYPT_ECDSA_P256_ALGORITHM, 256, szOID_ECC_PUBLIC_KEY, BCRYPT_ECCPUBLIC_BLOB );
    printf( "RESULT: %s\n", failures ? "FAIL" : "PASS" );
    return failures != 0;
}
