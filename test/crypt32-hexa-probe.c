/* Probe for patches/sg/5013: CryptBinaryToStringA with CRYPT_STRING_HEX gives
 * the layout CryptBinaryToStringW gives (Wine's base64 tests check the W layout
 * on Windows): same length query, same text, ERROR_MORE_DATA when short.
 * Table driven; lines "ok"/"FAIL", last "RESULT: PASS|FAIL". */
#include <windows.h>
#include <wincrypt.h>
#include <stdio.h>

static int failures;
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

int main(void)
{
    static const DWORD sizes[] = {1, 2, 7, 8, 9, 15, 16, 17, 31, 32, 33, 64};
    static const DWORD flags[] = {0, CRYPT_STRING_NOCR, CRYPT_STRING_NOCRLF};
    BYTE in[64];
    unsigned int i, j, k;

    for (i = 0; i < sizeof(in); i++) in[i] = i * 37 + 5;
    for (i = 0; i < ARRAYSIZE(sizes); i++)
        for (j = 0; j < ARRAYSIZE(flags); j++)
        {
            char a[512];
            WCHAR w[512];
            DWORD la = 0, lw = 0, la2, lw2;
            BOOL ra, rw, same;

            ra = CryptBinaryToStringA(in, sizes[i], CRYPT_STRING_HEX | flags[j], NULL, &la);
            rw = CryptBinaryToStringW(in, sizes[i], CRYPT_STRING_HEX | flags[j], NULL, &lw);
            check(ra && rw && la == lw, "size %lu flags %#lx: length A %lu W %lu (%d)", sizes[i], flags[j], la, lw, ra);
            la2 = la; lw2 = lw;
            memset(a, 0x55, sizeof(a));
            ra = CryptBinaryToStringA(in, sizes[i], CRYPT_STRING_HEX | flags[j], a, &la2);
            rw = CryptBinaryToStringW(in, sizes[i], CRYPT_STRING_HEX | flags[j], w, &lw2);
            same = ra && rw && la2 == lw2 && !a[la2];
            for (k = 0; same && k <= lw2; k++) if ((WCHAR)(BYTE)a[k] != w[k]) same = FALSE;
            check(same, "size %lu flags %#lx: text \"%.40s\" (%lu/%lu)", sizes[i], flags[j], ra ? a : "", la2, lw2);
            la2 = la - 1;
            SetLastError(0xdeadbeef);
            ra = CryptBinaryToStringA(in, sizes[i], CRYPT_STRING_HEX | flags[j], a, &la2);
            check(!ra && GetLastError() == ERROR_MORE_DATA, "size %lu flags %#lx: short buffer %lu", sizes[i], flags[j],
                  GetLastError());
        }
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
