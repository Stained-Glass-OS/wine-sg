/* Probe for patches/sg/5065: Wow64GetThreadSelectorEntry.
 * - a 64-bit process gets the 32-bit descriptors: the code selector 0x23 is a flat 4 GB code
 *   segment (type 0x1b, granularity 4K, DPL 3, present, 32-bit), the null selector an empty entry;
 * - a 32-bit process is refused with ERROR_NOT_SUPPORTED (documented).
 * Table driven; lines "ok"/"FAIL", last "RESULT: PASS|FAIL". */
#include <windows.h>
#include <stdio.h>

static int failures;
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

int main(void)
{
    static const struct { DWORD sel; int type, gran, limithi, pres; WORD limit; } tests[] =
    {
        {0x23, 0x1b, 1, 0xf, 1, 0xffff},
        {0x20, 0x1b, 1, 0xf, 1, 0xffff},   /* the RPL bits do not matter */
        {0x00, 0, 0, 0, 0, 0},
    };
    WOW64_LDT_ENTRY entry;
    unsigned int i;
    BOOL ret;

    for (i = 0; i < ARRAYSIZE(tests); i++)
    {
        memset(&entry, 0xcc, sizeof(entry));
        SetLastError(0xdeadbeef);
        ret = Wow64GetThreadSelectorEntry(GetCurrentThread(), tests[i].sel, &entry);
        if (sizeof(void *) == 4)
        {
            check(!ret && GetLastError() == ERROR_NOT_SUPPORTED, "32-bit: selector %#lx refused %d %lu",
                  tests[i].sel, ret, GetLastError());
            continue;
        }
        check(ret, "selector %#lx: %d %lu", tests[i].sel, ret, ret ? 0 : GetLastError());
        if (!ret) continue;
        check(entry.HighWord.Bits.Type == tests[i].type && entry.HighWord.Bits.Granularity == tests[i].gran &&
              entry.HighWord.Bits.LimitHi == tests[i].limithi && entry.HighWord.Bits.Pres == tests[i].pres &&
              entry.LimitLow == tests[i].limit && (!tests[i].pres || (entry.HighWord.Bits.Dpl == 3 &&
              entry.HighWord.Bits.Default_Big == 1)),
              "selector %#lx: type %#x gran %u limit %#x/%#x present %u dpl %u big %u", tests[i].sel,
              entry.HighWord.Bits.Type, entry.HighWord.Bits.Granularity, entry.LimitLow, entry.HighWord.Bits.LimitHi,
              entry.HighWord.Bits.Pres, entry.HighWord.Bits.Dpl, entry.HighWord.Bits.Default_Big);
    }
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
