/* Probe for patches/sg/5201: ScriptApplyLogicalWidth turns logical character widths
 * into glyph advances (clusters add up, one character over several glyphs is shared
 * out, zero-width glyphs take none, right-to-left runs keep glyph order) and updates
 * the ABC width; ScriptJustify gives the width change to kashida points first, then
 * blanks, then characters, and returns S_FALSE with the advances copied when no
 * glyph can be justified. Table driven; last line "RESULT: PASS|FAIL". */
#include <windows.h>
#include <usp10.h>
#include <stdio.h>

static int failures;
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

static const struct apply_case
{
    const char *name;
    int chars, glyphs, rtl;
    int dx[4];
    WORD clust[4];
    int zero_mask;       /* bit i: glyph i is zero width */
    int expect[4];
} apply_cases[] = {
    { "one to one",         3, 3, 0, {10, 20, 30},  {0, 1, 2}, 0, {10, 20, 30} },
    { "ligature",           3, 2, 0, {5, 6, 7},     {0, 0, 1}, 0, {11, 7} },
    { "char over 2 glyphs", 2, 3, 0, {9, 4},        {0, 2},    0, {4, 5, 4} },
    { "zero width glyph",   2, 3, 0, {9, 4},        {0, 2},    2, {9, 0, 4} },
    { "right to left",      3, 3, 1, {10, 20, 30},  {2, 1, 0}, 0, {30, 20, 10} },
    { "empty",              0, 0, 0, {0},           {0},       0, {0} },
};

static const struct just_case
{
    const char *name;
    int glyphs;
    unsigned char klass[4];
    int dx, min_kashida;
    int expect[4];
    HRESULT hr;
} just_cases[] = {
    { "nothing justifiable",   3, {0, 0, 0},                      9, 0, {10, 10, 10},   S_FALSE },
    { "characters equally",    4, {2, 2, 2, 2},                   8, 0, {12, 12, 12, 12}, S_OK },
    { "characters remainder",  4, {2, 2, 2, 2},                  10, 0, {12, 12, 12, 14}, S_OK },
    { "blanks before chars",   4, {2, 4, 2, 4},                   6, 0, {10, 13, 10, 13}, S_OK },
    { "arabic blank is blank", 3, {2, 1, 2},                      4, 0, {10, 14, 10},   S_OK },
    { "kashida first",         3, {4, 8, 4},                     10, 0, {10, 20, 10},   S_OK },
    { "minimum kashida",       3, {8, 8, 8},                      9, 5, {10, 10, 19},   S_OK },
    { "narrowing blanks",      3, {4, 2, 4},                     -4, 0, {8, 10, 8},     S_OK },
    { "never below zero",      2, {4, 4},                       -30, 0, {0, 0},         S_OK },
    { "no change",             3, {4, 4, 4},                      0, 0, {10, 10, 10},   S_OK },
};

int main(void)
{
    unsigned int i, k;
    HRESULT hr;

    for (i = 0; i < ARRAYSIZE(apply_cases); i++)
    {
        const struct apply_case *c = &apply_cases[i];
        SCRIPT_VISATTR sva[4] = {{0}};
        SCRIPT_ANALYSIS sa = {0};
        int adv[4] = {1, 1, 1, 1}, out[4] = {99, 99, 99, 99};
        ABC abc = {1, 50, 2};
        int total = 0;

        for (k = 0; k < 4; k++) sva[k].fZeroWidth = (c->zero_mask >> k) & 1;
        sa.fRTL = c->rtl;
        hr = ScriptApplyLogicalWidth(c->dx, c->chars, c->glyphs, c->clust, sva, adv, &sa, &abc, out);
        check(hr == S_OK, "apply %s: hr %08lx", c->name, hr);
        for (k = 0; k < (unsigned)c->glyphs; k++)
        {
            check(out[k] == c->expect[k], "apply %s: glyph %u advance %d, expected %d", c->name, k, out[k], c->expect[k]);
            total += c->expect[k];
        }
        if (c->glyphs)
            check(abc.abcA == 1 && abc.abcC == 2 && abc.abcB == total - 3, "apply %s: ABC %d %u %d for total %d",
                  c->name, abc.abcA, abc.abcB, abc.abcC, total);
    }
    {
        SCRIPT_VISATTR sva[1] = {{0}};
        SCRIPT_ANALYSIS sa = {0};
        int adv[1] = {1}, out[1];
        WORD clust[1] = {0};
        int dx[1] = {5};
        hr = ScriptApplyLogicalWidth(NULL, 1, 1, clust, sva, adv, &sa, NULL, out);
        check(hr == E_INVALIDARG, "apply without widths: %08lx", hr);
        clust[0] = 3;
        hr = ScriptApplyLogicalWidth(dx, 1, 1, clust, sva, adv, &sa, NULL, out);
        check(hr == E_INVALIDARG, "apply with a cluster beyond the glyphs: %08lx", hr);
    }

    for (i = 0; i < ARRAYSIZE(just_cases); i++)
    {
        const struct just_case *c = &just_cases[i];
        SCRIPT_VISATTR sva[4] = {{0}};
        int adv[4] = {10, 10, 10, 10}, out[4] = {-1, -1, -1, -1};

        for (k = 0; k < (unsigned)c->glyphs; k++) sva[k].uJustification = c->klass[k];
        hr = ScriptJustify(sva, adv, c->glyphs, c->dx, c->min_kashida, out);
        check(hr == c->hr, "justify %s: hr %08lx, expected %08lx", c->name, hr, c->hr);
        for (k = 0; k < (unsigned)c->glyphs; k++)
            check(out[k] == c->expect[k], "justify %s: glyph %u is %d, expected %d", c->name, k, out[k], c->expect[k]);
    }
    hr = ScriptJustify(NULL, NULL, 1, 1, 0, NULL);
    check(hr == E_INVALIDARG, "justify without buffers: %08lx", hr);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
