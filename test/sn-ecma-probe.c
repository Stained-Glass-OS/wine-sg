/* 5906 (CORE review: security): an assembly whose public key field is the ECMA neutral key passes
 * StrongNameSignatureVerificationEx only inside a Wine Mono installation (by location), never
 * elsewhere, whatever its path spells.  Arguments: pairs of EXPECTED(0|1) PATH. */
#include <windows.h>
#include <stdio.h>

typedef BOOLEAN (WINAPI *verify_ex_t)(const WCHAR *, BOOLEAN, BOOLEAN *);

int main(int argc, char **argv)
{
    HMODULE mscoree = LoadLibraryA("mscoree.dll");
    verify_ex_t verify = mscoree ? (verify_ex_t)GetProcAddress(mscoree, "StrongNameSignatureVerificationEx") : NULL;
    int i, failures = 0;

    if (!verify) { printf("FAIL no StrongNameSignatureVerificationEx\nRESULT: FAIL\n"); return 1; }
    for (i = 1; i + 1 < argc; i += 2)
    {
        WCHAR path[MAX_PATH];
        BOOLEAN verified = 0xcc, ret;
        int expect = atoi(argv[i]);

        MultiByteToWideChar(CP_ACP, 0, argv[i + 1], -1, path, MAX_PATH);
        SetLastError(0xdeadbeef);
        ret = verify(path, TRUE, &verified);
        if (!!ret != expect)
        {
            printf("FAIL %s: verification %d (expected %d), error %#lx\n", argv[i + 1], ret, expect, GetLastError());
            failures++;
        }
        else if (ret && !verified)
        {
            printf("FAIL %s: passed but not reported as verified\n", argv[i + 1]);
            failures++;
        }
        else printf("ok   %s: %s\n", argv[i + 1], ret ? "trusted" : "refused");
    }
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
