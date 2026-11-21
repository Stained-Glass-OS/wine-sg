/* ncrypttranslate-probe: ncrypt NCryptTranslateHandle, a spec stub (Stained Glass OS wine-sg 8260;
 * test/ncrypttranslate-gate.sh): the JDKs' sunmscapi.dll imports it. A CryptoAPI RSA key becomes a CNG key
 * that signs what CryptoAPI verifies; a key whose private half cannot be exported gives its public half.
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#include <windows.h>
#include <wincrypt.h>
#include <bcrypt.h>
#include <ncrypt.h>
#include <stdio.h>

static int failures;
static void check( int ok, const char *what, long v )
{
    printf( "%s  %s (%#lx)\n", ok ? "PASS" : "FAIL", what, v );
    if (!ok) failures++;
}

int main( void )
{
    SECURITY_STATUS (WINAPI *pTranslate)(NCRYPT_PROV_HANDLE *, NCRYPT_KEY_HANDLE *, ULONG_PTR, ULONG_PTR, DWORD, DWORD) =
        (void *)GetProcAddress( LoadLibraryA( "ncrypt.dll" ), "NCryptTranslateHandle" );
    BCRYPT_PKCS1_PADDING_INFO pad = { BCRYPT_SHA256_ALGORITHM };
    HCRYPTPROV prov;
    HCRYPTKEY key, key2;
    HCRYPTHASH hash;
    NCRYPT_PROV_HANDLE nprov = 0;
    NCRYPT_KEY_HANDLE nkey = 0, nkey2 = 0;
    BYTE digest[32], sig[256], rev[256], blob[1024];
    DWORD len = sizeof(digest), siglen = 0, i, size;
    SECURITY_STATUS st;

    if (!pTranslate) { printf( "FAIL  export\nRESULT: FAIL\n" ); return 1; }
    CryptAcquireContextW( &prov, NULL, MS_ENH_RSA_AES_PROV_W, PROV_RSA_AES, CRYPT_VERIFYCONTEXT );
    CryptGenKey( prov, AT_SIGNATURE, (1024 << 16) | CRYPT_EXPORTABLE, &key );
    CryptCreateHash( prov, CALG_SHA_256, 0, 0, &hash );
    CryptHashData( hash, (const BYTE *)"translate", 9, 0 );
    CryptGetHashParam( hash, HP_HASHVAL, digest, &len, 0 );

    st = pTranslate( &nprov, &nkey, prov, key, 0, 0 );
    check( st == 0 && nkey && nprov, "an exportable key translates", st );
    st = NCryptSignHash( nkey, &pad, digest, 32, sig, sizeof(sig), &siglen, BCRYPT_PAD_PKCS1 );
    check( st == 0 && siglen == 128, "the CNG key signs (PKCS#1, SHA-256)", st );
    for (i = 0; i < siglen; i++) rev[i] = sig[siglen - 1 - i]; /* CryptoAPI signatures are little-endian */
    check( CryptVerifySignatureW( hash, rev, siglen, key, NULL, 0 ), "CryptoAPI verifies that signature with the original key", GetLastError() );
    NCryptFreeObject( nkey );

    st = pTranslate( NULL, &nkey, prov, 0, AT_SIGNATURE, 0 );
    check( st == 0 && nkey, "the provider's signature key pair, by key spec, with no provider handle wanted", st );
    if (nkey) NCryptFreeObject( nkey );

    CryptGenKey( prov, AT_KEYEXCHANGE, 1024 << 16, &key2 ); /* not exportable */
    st = pTranslate( NULL, &nkey2, prov, key2, 0, 0 );
    check( st == 0 && nkey2, "a non-exportable key translates to its public half", st );
    size = 0;
    st = NCryptExportKey( nkey2, 0, BCRYPT_RSAPUBLIC_BLOB, NULL, blob, sizeof(blob), &size, 0 );
    check( st == 0 && size > sizeof(BCRYPT_RSAKEY_BLOB), "  whose public blob exports", st );
    st = NCryptSignHash( nkey2, &pad, digest, 32, sig, sizeof(sig), &siglen, BCRYPT_PAD_PKCS1 );
    check( st != 0, "  and which cannot sign", st );
    check( pTranslate( NULL, NULL, prov, key, 0, 0 ) == NTE_INVALID_PARAMETER, "no key pointer: NTE_INVALID_PARAMETER", 0 );
    check( pTranslate( NULL, &nkey, prov, key, 0, 1 ) == NTE_BAD_FLAGS, "flags: NTE_BAD_FLAGS", 0 );
    printf( "RESULT: %s\n", failures ? "FAIL" : "PASS" );
    return failures != 0;
}
