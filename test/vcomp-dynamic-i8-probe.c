/* Probe for patches/sg/4523: _vcomp_for_dynamic_init_i8/_next_i8 (64-bit
 * schedule(dynamic/guided/static) loops) and omp_get_wtick of the vcomp DLL
 * named on the command line. Loops past 2^32, alone and in a team of four
 * threads from _vcomp_fork: every iteration runs exactly once.
 * Lines "ok"/"FAIL", last "RESULT: PASS|FAIL". */
#include <windows.h>
#include <stdio.h>
#include <string.h>

static int failures;
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

#define BASE 0x100000000ull
#define FLAG_STATIC 1
#define FLAG_CHUNKED 2
#define FLAG_GUIDED 3
#define FLAG_INCREMENT 0x40

static void (__cdecl *p_init)(unsigned int, ULONG64, ULONG64, LONG64, ULONG64);
static int (__cdecl *p_next)(ULONG64 *, ULONG64 *);
static void (WINAPIV *p_fork)(BOOL, int, void *, ...);
static void (__cdecl *p_set_threads)(int);

static LONG hits[64];
static LONG chunks;

struct loop { unsigned int flags; ULONG64 first, last; LONG64 step; ULONG64 chunk; };

static void run_loop(const struct loop *l)
{
    ULONG64 b, e, i;
    int n = 0;

    p_init(l->flags, l->first, l->last, l->step, l->chunk);
    while (n++ < 200 && p_next(&b, &e))
    {
        InterlockedIncrement(&chunks);
        if (l->flags & FLAG_INCREMENT)
            for (i = b; i <= e && i - BASE < 64; i += l->step) InterlockedIncrement(&hits[i - BASE]);
        else
            for (i = b; i >= e && i - BASE < 64; i -= l->step) InterlockedIncrement(&hits[i - BASE]);
    }
}

static void __cdecl wrapper(const struct loop *l)
{
    run_loop(l);
}

int main(int argc, char **argv)
{
    static const struct loop loops[] =
    {
        {FLAG_CHUNKED | FLAG_INCREMENT, BASE, BASE + 9, 1, 3},
        {FLAG_GUIDED | FLAG_INCREMENT, BASE, BASE + 40, 1, 2},
        {FLAG_STATIC | FLAG_INCREMENT, BASE, BASE + 20, 1, 1},
        {FLAG_CHUNKED | FLAG_INCREMENT, BASE + 1, BASE + 61, 3, 4},
        {FLAG_CHUNKED, BASE + 30, BASE, 2, 5},
    };
    double (__cdecl *p_wtick)(void);
    HMODULE mod;
    unsigned int i, j, team;

    if (argc < 2 || !(mod = LoadLibraryA(argv[1]))) { printf("FAIL load\nRESULT: FAIL\n"); return 1; }
    p_init = (void *)GetProcAddress(mod, "_vcomp_for_dynamic_init_i8");
    p_next = (void *)GetProcAddress(mod, "_vcomp_for_dynamic_next_i8");
    p_fork = (void *)GetProcAddress(mod, "_vcomp_fork");
    p_set_threads = (void *)GetProcAddress(mod, "omp_set_num_threads");
    p_wtick = (void *)GetProcAddress(mod, "omp_get_wtick");
    check(p_init && p_next && p_fork && p_set_threads && p_wtick, "exports");
    if (failures) { printf("RESULT: FAIL\n"); return 1; }

    check(p_wtick() > 0 && p_wtick() <= 0.02, "omp_get_wtick %g", p_wtick());

    p_set_threads(4);
    for (team = 0; team < 2; team++)
    {
        for (i = 0; i < ARRAYSIZE(loops); i++)
        {
            const struct loop *l = &loops[i];
            ULONG64 lo = l->flags & FLAG_INCREMENT ? l->first : l->last, hi = l->flags & FLAG_INCREMENT ? l->last : l->first;
            int ok = 1;

            memset(hits, 0, sizeof(hits));
            chunks = 0;
            if (team) p_fork(TRUE, 1, wrapper, l);
            else run_loop(l);
            for (j = 0; j < 64; j++)
            {
                ULONG64 v = BASE + j;
                int want = v >= lo && v <= hi && !((l->flags & FLAG_INCREMENT ? v - l->first : l->first - v) % l->step);
                if (hits[j] != want) ok = 0;
            }
            check(ok && chunks > 0, "%s loop %u (flags %#x): each iteration once in %ld chunks", team ? "team" : "alone",
                  i, l->flags, chunks);
        }
    }

    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
