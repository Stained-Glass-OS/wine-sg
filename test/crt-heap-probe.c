/* Probe for patches/sg/4514: _aligned_recalloc, _aligned_offset_recalloc,
 * _recalloc_crt, _freea/_freea_s, _set_malloc_crt_max_wait and (msvcr80/90)
 * _get_amblksiz/_set_amblksiz of the DLL named on the command line.
 * Table driven; lines "ok"/"FAIL", last "RESULT: PASS|FAIL". */
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>

static int failures;
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

static int invalid;
static void __cdecl handler(const wchar_t *e, const wchar_t *f, const wchar_t *fn, unsigned int l, uintptr_t r) { invalid++; }

static int zero(const unsigned char *p, size_t from, size_t to)
{
    for (; from < to; from++) if (p[from]) return 0;
    return 1;
}

static int pattern(const unsigned char *p, size_t n)
{
    size_t i;
    for (i = 0; i < n; i++) if (p[i] != (unsigned char)(i * 7 + 1)) return 0;
    return 1;
}

static void fill(unsigned char *p, size_t n)
{
    size_t i;
    for (i = 0; i < n; i++) p[i] = i * 7 + 1;
}

int main(int argc, char **argv)
{
    /* steps of one block: element count, element size, alignment, offset */
    static const struct { size_t num, size, align, offset; } steps[][4] =
    {
        {{4, 8, 32, 0}, {16, 8, 32, 0}, {2, 8, 32, 0}, {40, 3, 32, 0}},
        {{3, 10, 64, 8}, {9, 10, 64, 8}, {1, 10, 64, 8}, {100, 1, 64, 8}},
        {{1, 1, 16, 0}, {1000, 4, 16, 0}, {500, 4, 16, 0}, {2000, 4, 16, 0}},
    };
    void *(__cdecl *p_recalloc)(void *, size_t, size_t, size_t);
    void *(__cdecl *p_offset_recalloc)(void *, size_t, size_t, size_t, size_t);
    void *(__cdecl *p_recalloc_crt)(void *, size_t, size_t);
    void (__cdecl *p_freea)(void *), (__cdecl *p_freea_s)(void *);
    void (__cdecl *p_aligned_free)(void *);
    void *(__cdecl *p_malloc)(size_t);
    unsigned long (__cdecl *p_max_wait)(unsigned long);
    int (__cdecl *p_get_amblksiz)(size_t *), (__cdecl *p_set_amblksiz)(size_t);
    unsigned int *(__cdecl *p_amblksiz)(void);
    intptr_t (__cdecl *p_heap)(void);
    int *(__cdecl *p_errno)(void);
    void *(__cdecl *p_handler)(void *);
    size_t marker = sizeof(void *) == 8 ? 16 : 8;
    unsigned int i, j;
    HMODULE mod;
    HANDLE heap;
    void *p;

    if (argc < 2 || !(mod = LoadLibraryA(argv[1]))) { printf("FAIL load\nRESULT: FAIL\n"); return 1; }
#define GET(v, n) v = (void *)GetProcAddress(mod, n)
    GET(p_recalloc, "_aligned_recalloc"); GET(p_offset_recalloc, "_aligned_offset_recalloc");
    GET(p_recalloc_crt, "_recalloc_crt"); GET(p_freea, "_freea"); GET(p_freea_s, "_freea_s");
    GET(p_aligned_free, "_aligned_free"); GET(p_malloc, "malloc"); GET(p_max_wait, "_set_malloc_crt_max_wait");
    GET(p_get_amblksiz, "_get_amblksiz"); GET(p_set_amblksiz, "_set_amblksiz"); GET(p_amblksiz, "__p__amblksiz");
    GET(p_heap, "_get_heap_handle"); GET(p_errno, "_errno"); GET(p_handler, "_set_invalid_parameter_handler");
    check(p_recalloc && p_offset_recalloc && p_aligned_free && p_errno && p_handler, "aligned exports");
    if (failures) { printf("RESULT: FAIL\n"); return 1; }
    p_handler(handler);

    for (i = 0; i < ARRAYSIZE(steps); i++)
    {
        size_t old = 0;
        unsigned char *b = NULL;

        for (j = 0; j < ARRAYSIZE(steps[i]); j++)
        {
            size_t n = steps[i][j].num * steps[i][j].size, keep = old < n ? old : n;

            b = steps[i][j].offset ? p_offset_recalloc(b, steps[i][j].num, steps[i][j].size, steps[i][j].align, steps[i][j].offset)
                                   : p_recalloc(b, steps[i][j].num, steps[i][j].size, steps[i][j].align);
            check(b && !(((uintptr_t)b + steps[i][j].offset) % steps[i][j].align) && pattern(b, keep) && zero(b, keep, n),
                  "block %u step %u: %Iu bytes aligned %Iu+%Iu at %p, kept %Iu, rest zero", i, j, n,
                  steps[i][j].align, steps[i][j].offset, b, keep);
            if (!b) break;
            fill(b, n);
            old = n;
        }
        p_aligned_free(b);
    }

    *p_errno() = 0;
    p = p_recalloc(NULL, ~(size_t)0 / 2, 4, 16);
    check(!p && *p_errno() == ENOMEM, "overflowing count: %p errno %d", p, *p_errno());

    if (p_recalloc_crt)  /* msvcr80 .. msvcr120 */
    {
        unsigned char *b = p_recalloc_crt(NULL, 4, 4);
        check(b && zero(b, 0, 16), "_recalloc_crt new block zeroed");
        fill(b, 16);
        b = p_recalloc_crt(b, 8, 4);
        check(b && pattern(b, 16) && zero(b, 16, 32), "_recalloc_crt grows, keeps and zeroes");
        p_recalloc_crt(b, 0, 0);
    }

    if (p_freea && p_freea_s && p_heap && p_malloc)
    {
        /* blocks laid out as _malloca does: the marker, then the user pointer */
        static const struct { unsigned int mark; int freed; } blocks[] = {{0xdddd, 1}, {0xcccc, 0}, {0x1234, 0}};
        unsigned int k;

        heap = (HANDLE)p_heap();
        for (k = 0; k < 2 * ARRAYSIZE(blocks); k++)
        {
            unsigned char *raw = p_malloc(marker + 32), stack[64];
            unsigned int b = k % ARRAYSIZE(blocks);

            *(unsigned int *)raw = blocks[b].mark;
            (k < ARRAYSIZE(blocks) ? p_freea : p_freea_s)(raw + marker);
            check(!HeapValidate(heap, 0, raw) == blocks[b].freed, "%s marker %#x: freed %d",
                  k < ARRAYSIZE(blocks) ? "_freea" : "_freea_s", blocks[b].mark, !HeapValidate(heap, 0, raw));
            if (!blocks[b].freed) HeapFree(heap, 0, raw);
            *(unsigned int *)stack = 0xcccc;
            p_freea(stack + marker);  /* a stack block is left alone */
        }
        p_freea(NULL);
    }

    if (p_max_wait)
    {
        unsigned long a = p_max_wait(500), b = p_max_wait(0), c = p_max_wait(0);
        check(a == 0 && b == 500 && c == 0, "_set_malloc_crt_max_wait returns the previous value: %lu %lu %lu", a, b, c);
    }

    if (p_get_amblksiz && p_set_amblksiz && p_amblksiz)  /* msvcr80, msvcr90 */
    {
        size_t v = 0;
        int r;

        r = p_get_amblksiz(&v);
        check(!r && v == *p_amblksiz(), "_get_amblksiz %Iu (_amblksiz %u)", v, *p_amblksiz());
        r = p_set_amblksiz(64);
        check(!r && *p_amblksiz() == 64 && !p_get_amblksiz(&v) && v == 64, "_set_amblksiz(64)");
        invalid = 0;
        r = p_get_amblksiz(NULL);
        check(r == EINVAL && invalid == 1, "_get_amblksiz(NULL): %d, handler %d", r, invalid);
        invalid = 0;
        r = p_set_amblksiz(0);
        check(r == EINVAL && invalid == 1 && *p_amblksiz() == 64, "_set_amblksiz(0): %d, handler %d", r, invalid);
        p_set_amblksiz(16);
    }

    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
