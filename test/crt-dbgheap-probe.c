/* Probe for patches/sg/4524: msvcrtd's debug heap functions on Wine's heap
 * (no debug block headers): pointer checks, memory states and their
 * difference, statistics, report files, hooks. Table driven where it can be.
 * Lines "ok"/"FAIL", last "RESULT: PASS|FAIL". */
#include <windows.h>
#include <stdio.h>
#include <string.h>

typedef struct { void *head; ULONG counts[5], sizes[5], high, total; } memstate;  /* unsigned long, as crtdbg.h */

static int failures;
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

static int visits;
static void __cdecl visit(void *p, void *ctx) { visits++; }

int main(int argc, char **argv)
{
    int (__cdecl *p_validptr)(const void *, unsigned int, int);
    int (__cdecl *p_validheap)(const void *);
    int (__cdecl *p_isblock)(const void *, unsigned int, long *, char **, int *);
    void (__cdecl *p_checkpoint)(memstate *);
    int (__cdecl *p_difference)(memstate *, const memstate *, const memstate *);
    void (__cdecl *p_stats)(const memstate *), (__cdecl *p_dumpall)(const memstate *);
    void (__cdecl *p_forall)(void *, void *);
    void *(__cdecl *p_hook)(void *);
    void (__cdecl *p_blocktype)(void *, int);
    HANDLE (__cdecl *p_reportfile)(int, HANDLE);
    void *(__cdecl *p_malloc)(size_t);
    void (__cdecl *p_free)(void *);
    memstate s1, s2, d;
    static char global[16];
    char local[8], *block, *blocks[10];
    long req = 7; char *file = (char *)1; int line = 7;
    HMODULE mod;
    unsigned int i;
    int r;

    if (!(mod = LoadLibraryA("msvcrtd.dll"))) { printf("FAIL load\nRESULT: FAIL\n"); return 1; }
#define GET(v, n) v = (void *)GetProcAddress(mod, n); check(v != NULL, "%s exported", n)
    GET(p_validptr, "_CrtIsValidPointer"); GET(p_validheap, "_CrtIsValidHeapPointer"); GET(p_isblock, "_CrtIsMemoryBlock");
    GET(p_checkpoint, "_CrtMemCheckpoint"); GET(p_difference, "_CrtMemDifference"); GET(p_stats, "_CrtMemDumpStatistics");
    GET(p_dumpall, "_CrtMemDumpAllObjectsSince"); GET(p_forall, "_CrtDoForAllClientObjects"); GET(p_hook, "_CrtSetAllocHook");
    GET(p_blocktype, "_CrtSetDbgBlockType"); GET(p_reportfile, "_CrtSetReportFile");
    check(GetProcAddress(mod, "_CrtDbgBreak") != NULL, "_CrtDbgBreak exported");
    p_malloc = (void *)GetProcAddress(mod, "malloc");
    p_free = (void *)GetProcAddress(mod, "free");
    if (failures) { printf("RESULT: FAIL\n"); return 1; }

    block = p_malloc(40);
    {
        static const struct { int which; unsigned int size; int access; int want; } ptrs[] =
        {
            {0, 16, 1, 1}, {1, 8, 1, 1}, {2, 40, 1, 1}, {3, 4, 0, 0}, {4, 4, 0, 1}, {4, 4, 1, 0},
        };
        const void *p[5] = {global, local, NULL, NULL, (const void *)GetModuleHandleA(NULL)};
        p[2] = block;
        for (i = 0; i < ARRAYSIZE(ptrs); i++)
        {
            r = p_validptr(p[ptrs[i].which], ptrs[i].size, ptrs[i].access);
            check(!r == !ptrs[i].want, "_CrtIsValidPointer case %u: %d", i, r);
        }
    }
    check(p_validheap(block) && !p_validheap(global) && !p_validheap(local) && !p_validheap(NULL),
          "_CrtIsValidHeapPointer: heap block only");
    r = p_isblock(block, 40, &req, &file, &line);
    check(r && req == 0 && !file && !line, "_CrtIsMemoryBlock right size: %d", r);
    check(!p_isblock(block, 41, NULL, NULL, NULL) && !p_isblock(global, 16, NULL, NULL, NULL), "_CrtIsMemoryBlock wrong size or not heap");

    /* states: ten more blocks make a difference, freeing them none */
    p_checkpoint(&s1);
    check(s1.counts[1] > 0 && s1.sizes[1] >= 40 && s1.high >= s1.sizes[1], "checkpoint: %lu normal blocks, %lu bytes",
          (unsigned long)s1.counts[1], (unsigned long)s1.sizes[1]);
    for (i = 0; i < 10; i++) blocks[i] = p_malloc(100);
    p_checkpoint(&s2);
    r = p_difference(&d, &s1, &s2);
    check(r && d.counts[1] == 10 && d.sizes[1] >= 1000 && d.sizes[1] < 1200, "10 blocks of 100 bytes: %d, %ld blocks %ld bytes",
          r, (long)d.counts[1], (long)d.sizes[1]);
    for (i = 0; i < 10; i++) p_free(blocks[i]);
    p_checkpoint(&s2);
    r = p_difference(&d, &s1, &s2);
    check(!r && !d.counts[1] && !d.sizes[1], "after freeing them: %d, %ld %ld", r, (long)d.counts[1], (long)d.sizes[1]);

    p_stats(&s1);
    p_dumpall(&s1);
    visits = 0;
    p_forall(visit, NULL);
    check(!visits, "no client blocks to visit");

    check(p_hook((void *)visit) == NULL && p_hook(NULL) == (void *)visit, "_CrtSetAllocHook returns the previous hook");
    p_blocktype(block, 1);

    {
        HANDLE old = p_reportfile(0, (HANDLE)-5);
        check(old == (HANDLE)-1, "report file default %p", old);
        check(p_reportfile(0, (HANDLE)-6) == (HANDLE)-5, "report file query");
        check(p_reportfile(0, old) == (HANDLE)-5 && p_reportfile(1, (HANDLE)-6) == (HANDLE)-1, "report file per type");
        check(p_reportfile(3, (HANDLE)-5) == (HANDLE)-2 && p_reportfile(-1, (HANDLE)-5) == (HANDLE)-2, "bad report type");
    }
    p_free(block);

    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
