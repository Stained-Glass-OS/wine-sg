/* Probe for patches/sg/5004: RtlFindLastBackwardRunSet / RtlFindLastBackwardRunClear
 * find the run nearest below or at the starting index and return its length and
 * lowest index. Table driven; lines "ok"/"FAIL", last "RESULT: PASS|FAIL". */
#include <windows.h>
#include <winternl.h>
#include <stdio.h>

static int failures;
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

typedef struct { ULONG SizeOfBitMap; ULONG *Buffer; } BITMAP_;

int main(int argc, char **argv)
{
    HMODULE nt = GetModuleHandleA("ntdll.dll");
    ULONG (WINAPI *find_set)(BITMAP_ *, ULONG, ULONG *) = (void *)GetProcAddress(nt, "RtlFindLastBackwardRunSet");
    ULONG (WINAPI *find_clear)(BITMAP_ *, ULONG, ULONG *) = (void *)GetProcAddress(nt, "RtlFindLastBackwardRunClear");
    /* bits 0..63: set at 3-5, 10, 20-39, 62-63 */
    static ULONG bits[2];
    BITMAP_ bm = {64, bits};
    static const struct { int set; ULONG from; ULONG len; ULONG pos; } cases[] =
    {
        {1, 5, 3, 3}, {1, 4, 2, 3}, {1, 9, 3, 3}, {1, 10, 1, 10}, {1, 45, 20, 20}, {1, 63, 2, 62},
        {1, 2, 0, 0}, {1, 64, 0, 0},
        {0, 2, 3, 0}, {0, 5, 3, 0}, {0, 9, 4, 6}, {0, 19, 9, 11}, {0, 61, 22, 40}, {0, 63, 22, 40},
    };
    unsigned int i;

    for (i = 3; i <= 5; i++) bits[i / 32] |= 1u << (i % 32);
    bits[0] |= 1u << 10;
    for (i = 20; i <= 39; i++) bits[i / 32] |= 1u << (i % 32);
    bits[1] |= 3u << 30;

    for (i = 0; i < ARRAYSIZE(cases); i++)
    {
        ULONG pos = 0xdead, len = (cases[i].set ? find_set : find_clear)(&bm, cases[i].from, &pos);
        check(len == cases[i].len && (!len || pos == cases[i].pos), "%s from %lu: length %lu at %lu, want %lu at %lu",
              cases[i].set ? "set" : "clear", cases[i].from, len, pos, cases[i].len, cases[i].pos);
    }
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
