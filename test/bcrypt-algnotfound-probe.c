/* Probe for patches/sg/5014: BCryptOpenAlgorithmProvider fails with
 * STATUS_NOT_FOUND ("No provider was found for the specified algorithm ID")
 * for an algorithm or an implementation it does not have; known ones open.
 * Table driven; lines "ok"/"FAIL", last "RESULT: PASS|FAIL". */
#include <windows.h>
#include <bcrypt.h>
#include <stdio.h>

static int failures;
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

#ifndef STATUS_NOT_FOUND
#define STATUS_NOT_FOUND ((NTSTATUS)0xc0000225)
#endif

int main(void)
{
    static const struct { const WCHAR *id, *impl; NTSTATUS want; } tests[] =
    {
        {L"SHA256", NULL, 0},
        {L"AES", L"Microsoft Primitive Provider", 0},
        {L"SG_NO_SUCH_ALGORITHM", NULL, STATUS_NOT_FOUND},
        {L"SG_NO_SUCH_ALGORITHM", L"Microsoft Primitive Provider", STATUS_NOT_FOUND},
        {L"SHA256", L"SG No Such Provider", STATUS_NOT_FOUND},
        {L"", NULL, STATUS_NOT_FOUND},
    };
    BCRYPT_ALG_HANDLE alg;
    unsigned int i;
    NTSTATUS st;

    for (i = 0; i < ARRAYSIZE(tests); i++)
    {
        alg = NULL;
        st = BCryptOpenAlgorithmProvider(&alg, tests[i].id, tests[i].impl, 0);
        check(st == tests[i].want && (!st) == (alg != NULL), "\"%ls\" / %ls: %#lx (want %#lx)", tests[i].id,
              tests[i].impl ? tests[i].impl : L"(null)", st, tests[i].want);
        if (!st) BCryptCloseAlgorithmProvider(alg, 0);
    }
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
