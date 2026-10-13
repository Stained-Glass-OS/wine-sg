/* Probe for patches/sg/5008: SetServiceBits refuses the bits Windows keeps
 * for itself (ERROR_INVALID_DATA, checked first) and a NULL status handle
 * (ERROR_INVALID_HANDLE).
 * Table driven; lines "ok"/"FAIL", last "RESULT: PASS|FAIL". */
#include <windows.h>
#include <lmserver.h>
#include <stdio.h>

static int failures;
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

int main(void)
{
    static const struct { DWORD bits; DWORD err; } tests[] =
    {
        {SV_TYPE_WORKSTATION, ERROR_INVALID_DATA},
        {SV_TYPE_SERVER, ERROR_INVALID_DATA},
        {SV_TYPE_DOMAIN_CTRL, ERROR_INVALID_DATA},
        {0x80000000, ERROR_INVALID_DATA},
        {0x00004000, ERROR_INVALID_HANDLE},   /* not reserved: the handle is checked next */
        {0x00400000, ERROR_INVALID_HANDLE},
        {0, ERROR_INVALID_HANDLE},
    };
    unsigned int i;
    DWORD err;
    BOOL ret;

    for (i = 0; i < ARRAYSIZE(tests); i++)
    {
        SetLastError(0xdeadbeef);
        ret = SetServiceBits(NULL, tests[i].bits, TRUE, TRUE);
        err = GetLastError();
        check(!ret && err == tests[i].err, "bits %#lx: %d %lu (want %lu)", tests[i].bits, ret, err, tests[i].err);
    }
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
