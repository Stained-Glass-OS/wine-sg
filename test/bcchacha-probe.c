/* BCrypt ChaCha20-Poly1305 (the AEAD of RFC 8439): open by name, properties,
 * a 256-bit key, encrypt/decrypt with BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO
 * (12-byte nonce, 16-byte tag) against the RFC's section 2.8.2 vector, a bad
 * tag refused. .NET's ChaCha20Poly1305.IsSupported opens it (Duplicati 2.4
 * logged "CHACHA20_POLY1305 not supported"). patches/sg/9304. */
#include <windows.h>
#include <bcrypt.h>
#include <stdio.h>
#include <string.h>

#ifndef STATUS_AUTH_TAG_MISMATCH
#define STATUS_AUTH_TAG_MISMATCH ((NTSTATUS)0xC000A002)
#endif

static int fails;
#define CHECK(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); fails++; } printf(__VA_ARGS__); printf("\n"); } while (0)

static const char plain[] = "Ladies and Gentlemen of the class of '99: If I could offer you only one tip for the future, sunscreen would be it.";
static const UCHAR nonce[12] = { 0x07,0,0,0, 0x40,0x41,0x42,0x43,0x44,0x45,0x46,0x47 };
static const UCHAR aad[12] = { 0x50,0x51,0x52,0x53,0xc0,0xc1,0xc2,0xc3,0xc4,0xc5,0xc6,0xc7 };
static const UCHAR cipher_start[16] = { 0xd3,0x1a,0x8d,0x34,0x64,0x8e,0x60,0xdb,0x7b,0x86,0xaf,0xbc,0x53,0xef,0x7e,0xc2 };
static const UCHAR cipher_end[2] = { 0x61, 0x16 };
static const UCHAR tag_expected[16] = { 0x1a,0xe1,0x0b,0x59,0x4f,0x09,0xe2,0x6a,0x7e,0x90,0x2e,0xcb,0xd0,0x60,0x06,0x91 };

static void init_info(BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO *info, UCHAR *tag, ULONG tag_len)
{
    BCRYPT_INIT_AUTH_MODE_INFO(*info);
    info->pbNonce = (UCHAR *)nonce; info->cbNonce = sizeof(nonce);
    info->pbAuthData = (UCHAR *)aad; info->cbAuthData = sizeof(aad);
    info->pbTag = tag; info->cbTag = tag_len;
}

int main(void)
{
    BCRYPT_ALG_HANDLE alg = NULL;
    BCRYPT_KEY_HANDLE key = NULL;
    BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO info;
    BCRYPT_KEY_LENGTHS_STRUCT lengths;
    UCHAR secret[32], out[sizeof(plain)], back[sizeof(plain)], tag[16];
    ULONG len = sizeof(plain) - 1, size, i, block = 0;
    NTSTATUS status;

    for (i = 0; i < 32; i++) secret[i] = 0x80 + i;

    status = BCryptOpenAlgorithmProvider(&alg, L"CHACHA20_POLY1305", NULL, 0);
    CHECK(!status && alg, "BCryptOpenAlgorithmProvider CHACHA20_POLY1305 (%#lx)", status);
    if (!alg) { printf("RESULT: FAIL\n"); return 1; }

    status = BCryptGetProperty(alg, BCRYPT_KEY_LENGTHS, (UCHAR *)&lengths, sizeof(lengths), &size, 0);
    CHECK(!status && lengths.dwMinLength == 256 && lengths.dwMaxLength == 256, "key lengths %lu-%lu (%#lx)",
          lengths.dwMinLength, lengths.dwMaxLength, status);
    status = BCryptGetProperty(alg, BCRYPT_AUTH_TAG_LENGTH, (UCHAR *)&lengths, sizeof(lengths), &size, 0);
    CHECK(!status && lengths.dwMinLength == 16 && lengths.dwMaxLength == 16, "tag lengths %lu-%lu (%#lx)",
          lengths.dwMinLength, lengths.dwMaxLength, status);
    status = BCryptGetProperty(alg, BCRYPT_BLOCK_LENGTH, (UCHAR *)&block, sizeof(block), &size, 0);
    CHECK(!status && block == 1, "block length %lu (a stream cipher)", block);

    status = BCryptGenerateSymmetricKey(alg, &key, NULL, 0, secret, sizeof(secret), 0);
    CHECK(!status && key, "a 256-bit key (%#lx)", status);
    if (!key) { printf("RESULT: FAIL\n"); return 1; }

    memset(out, 0, sizeof(out)); memset(tag, 0, sizeof(tag));
    init_info(&info, tag, sizeof(tag));
    status = BCryptEncrypt(key, (UCHAR *)plain, len, &info, NULL, 0, out, len, &size, 0);
    CHECK(!status && size == len, "encrypt (%#lx, %lu bytes)", status, size);
    CHECK(!memcmp(out, cipher_start, 16) && !memcmp(out + len - 2, cipher_end, 2), "ciphertext as RFC 8439 2.8.2");
    CHECK(!memcmp(tag, tag_expected, 16), "tag as RFC 8439 2.8.2");

    memset(back, 0, sizeof(back));
    init_info(&info, tag, sizeof(tag));
    status = BCryptDecrypt(key, out, len, &info, NULL, 0, back, len, &size, 0);
    CHECK(!status && size == len && !memcmp(back, plain, len), "decrypt gives the text back (%#lx)", status);

    tag[0] ^= 1;
    init_info(&info, tag, sizeof(tag));
    status = BCryptDecrypt(key, out, len, &info, NULL, 0, back, len, &size, 0);
    CHECK(status == STATUS_AUTH_TAG_MISMATCH, "a wrong tag is refused (%#lx)", status);
    tag[0] ^= 1;

    init_info(&info, tag, 12);
    status = BCryptEncrypt(key, (UCHAR *)plain, len, &info, NULL, 0, out, len, &size, 0);
    CHECK(status == STATUS_INVALID_PARAMETER, "a 12-byte tag is refused (%#lx)", status);

    BCryptDestroyKey(key);
    BCryptCloseAlgorithmProvider(alg, 0);
    printf("RESULT: %s\n", fails ? "FAIL" : "PASS");
    return fails != 0;
}
