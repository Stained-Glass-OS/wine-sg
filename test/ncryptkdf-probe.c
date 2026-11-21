/* ncryptkdf-probe: ncrypt's BCryptKeyDerivation export (forwarded to bcrypt; Stained Glass OS wine-sg 8245;
 * test/ncryptkdf-gate.sh): PBKDF2-HMAC-SHA256 of "password"/"salt"/1 iteration gives RFC 7914's vector.
 * It was a spec stub in ncrypt.
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#include <windows.h>
#include <bcrypt.h>
#include <stdio.h>

int main( void )
{
    static const BYTE expect[32] = { 0x12,0x0f,0xb6,0xcf,0xfc,0xf8,0xb3,0x2c,0x43,0xe7,0x22,0x52,0x56,0xc4,0xf8,0x37,
                                     0xa8,0x65,0x48,0xc9,0x2c,0xcc,0x35,0x48,0x08,0x05,0x98,0x7c,0xb7,0x0b,0xe1,0x7b };
    NTSTATUS (WINAPI *pKdf)(BCRYPT_KEY_HANDLE, BCryptBufferDesc *, UCHAR *, ULONG, ULONG *, ULONG) =
        (void *)GetProcAddress( LoadLibraryA( "ncrypt.dll" ), "BCryptKeyDerivation" );
    BCRYPT_ALG_HANDLE alg = NULL;
    BCRYPT_KEY_HANDLE key = NULL;
    ULONGLONG iterations = 1;
    BCryptBuffer buf[3] = { { sizeof(L"SHA256"), KDF_HASH_ALGORITHM, (void *)L"SHA256" },
                            { 4, KDF_SALT, (void *)"salt" }, { sizeof(iterations), KDF_ITERATION_COUNT, &iterations } };
    BCryptBufferDesc desc = { BCRYPTBUFFER_VERSION, 3, buf };
    BYTE out[32];
    ULONG got = 0;
    NTSTATUS status;
    int fail = 0;

    if (!pKdf) { printf( "FAIL  ncrypt exports BCryptKeyDerivation\nRESULT: FAIL\n" ); return 1; }
    printf( "PASS  ncrypt exports BCryptKeyDerivation\n" );
    status = BCryptOpenAlgorithmProvider( &alg, BCRYPT_PBKDF2_ALGORITHM, NULL, 0 );
    if (!status) status = BCryptGenerateSymmetricKey( alg, &key, NULL, 0, (UCHAR *)"password", 8, 0 );
    if (!status) status = pKdf( key, &desc, out, sizeof(out), &got, 0 );
    if (status || got != 32 || memcmp( out, expect, 32 )) fail = 1;
    printf( "%s  PBKDF2-HMAC-SHA256 through ncrypt's export: RFC 7914 vector (%#lx)\n", fail ? "FAIL" : "PASS", status );
    printf( "RESULT: %s\n", fail ? "FAIL" : "PASS" );
    return fail;
}
