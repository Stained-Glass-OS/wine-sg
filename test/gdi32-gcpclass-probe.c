/* Probe for patches/sg/5202: GetCharacterPlacement fills GCP_RESULTS.lpClass with the
 * character classification (Latin, Latin digit, Hebrew, Arabic, local digit) and, with
 * GCP_CLASSIN, keeps the classes the caller preset and computes only the zero ones.
 * Table driven; last line "RESULT: PASS|FAIL". */
#include <windows.h>
#include <stdio.h>

static int failures;
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

static const struct
{
    const char *name;
    const WCHAR *str;
    DWORD flags;
    char preset[8];
    char expect[8];
} cases[] = {
    { "latin and digits", L"ab1 ", 0, {0}, {GCPCLASS_LATIN, GCPCLASS_LATIN, GCPCLASS_LATINNUMBER, GCPCLASS_LATIN} },
    { "hebrew",           L"a\x05d0\x05d1", 0, {0}, {GCPCLASS_LATIN, GCPCLASS_HEBREW, GCPCLASS_HEBREW} },
    { "arabic",           L"\x0627\x0628z", 0, {0}, {GCPCLASS_ARABIC, GCPCLASS_ARABIC, GCPCLASS_LATIN} },
    { "local digits",     L"1\x0661\x06f2", 0, {0}, {GCPCLASS_LATINNUMBER, GCPCLASS_LOCALNUMBER, GCPCLASS_LOCALNUMBER} },
    { "presets ignored",  L"ab", 0, {GCPCLASS_HEBREW, GCPCLASS_ARABIC}, {GCPCLASS_LATIN, GCPCLASS_LATIN} },
    { "presets kept",     L"abc1", GCP_CLASSIN, {GCPCLASS_NEUTRAL, 0, GCPCLASS_HEBREW, 0},
      {GCPCLASS_NEUTRAL, GCPCLASS_LATIN, GCPCLASS_HEBREW, GCPCLASS_LATINNUMBER} },
};

int main(void)
{
    HDC dc = GetDC(0);
    unsigned int i, k;
    GCP_RESULTSW res;
    WCHAR out[8];
    char cls[8];
    int dx[8];
    DWORD ret;

    for (i = 0; i < ARRAYSIZE(cases); i++)
    {
        int len = lstrlenW(cases[i].str);
        memset(&res, 0, sizeof(res));
        memcpy(cls, cases[i].preset, sizeof(cls));
        res.lStructSize = sizeof(res);
        res.lpOutString = out;
        res.lpClass = cls;
        res.lpDx = dx;
        res.nGlyphs = len;
        ret = GetCharacterPlacementW(dc, cases[i].str, len, 0, &res, cases[i].flags);
        check(ret != 0, "%s: returned %08lx", cases[i].name, ret);
        for (k = 0; k < (unsigned)len; k++)
            check(cls[k] == cases[i].expect[k], "%s: char %u class %d, expected %d", cases[i].name, k, cls[k], cases[i].expect[k]);
    }
    /* fewer glyphs than characters: only those are classified */
    memset(&res, 0, sizeof(res));
    memset(cls, 0x55, sizeof(cls));
    res.lStructSize = sizeof(res);
    res.lpClass = cls;
    res.nGlyphs = 2;
    ret = GetCharacterPlacementW(dc, L"abcd", 4, 0, &res, 0);
    check(ret != 0 && cls[0] == GCPCLASS_LATIN && cls[1] == GCPCLASS_LATIN && cls[2] == 0x55,
          "nGlyphs limits the classes: %d %d %d", cls[0], cls[1], cls[2]);
    ReleaseDC(0, dc);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
