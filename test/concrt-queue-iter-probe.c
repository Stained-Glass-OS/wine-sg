/* Probe for patches/sg/4529: _Concurrent_queue_iterator_base_v4 (the
 * concurrent_queue unsafe iterator) and the NFS_* allocator of the DLL
 * named on the command line (msvcp100 .. msvcp120, concrt140). A queue of
 * ints over several pages, partly popped, iterated in order; an iterator
 * copied in the middle goes on by itself. Lines "ok"/"FAIL", last "RESULT". */
#include <windows.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN64
#define THISCALL
#define N(a64, a32) a64
#else
#define THISCALL __attribute__((thiscall))
#define N(a64, a32) a32
#endif

static int failures;
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

/* a queue of ints: the page is two pointers then the items */
struct page { struct page *next; size_t mask; int items[32]; };
struct queue { const void **vtable; void *data; size_t alloc_count, item_size; };
struct iter { void *rep; int *item; };

static void THISCALL q_move(struct queue *q, struct page *p, size_t idx, void *src) { p->items[idx] = *(int *)src; }
static void THISCALL q_copy(struct queue *q, struct page *p, size_t idx, const void *src) { p->items[idx] = *(const int *)src; }
static void THISCALL q_assign(struct queue *q, void *dst, struct page *p, size_t idx) { *(int *)dst = p->items[idx]; }
static void *THISCALL q_dtor(struct queue *q, unsigned int flags) { return q; }
static struct page *THISCALL q_alloc(struct queue *q) { return calloc(1, sizeof(struct page)); }
static void THISCALL q_free(struct queue *q, struct page *p) { free(p); }
static const void *queue_vtable[] = {q_move, q_copy, q_assign, q_dtor, q_alloc, q_free};
static const void *queue_vtable100[] = {q_copy, q_assign, q_dtor, q_alloc, q_free};

int main(int argc, char **argv)
{
    void (THISCALL *ctor)(struct queue *, size_t);
    void (THISCALL *push)(struct queue *, const void *);
    unsigned char (THISCALL *pop)(struct queue *, void *);
    void (THISCALL *it_ctor)(struct iter *, const struct queue *), (THISCALL *it_dtor)(struct iter *);
    void (THISCALL *it_assign)(struct iter *, const struct iter *), (THISCALL *it_advance)(struct iter *);
    size_t (__cdecl *line)(void);
    void *(__cdecl *nfs_alloc)(size_t, size_t, void *);
    void (__cdecl *nfs_free)(void *);
    struct queue q;
    struct iter it, it2;
    HMODULE mod;
    int i, v, expect, ok, steps;

    if (argc < 2 || !(mod = LoadLibraryA(argv[1]))) { printf("FAIL load\nRESULT: FAIL\n"); return 1; }
#define GET(v, a64, a32) v = (void *)GetProcAddress(mod, N(a64, a32)); check(v != NULL, "%s exported", #v)
    GET(ctor, "??0_Concurrent_queue_base_v4@details@Concurrency@@IEAA@_K@Z", "??0_Concurrent_queue_base_v4@details@Concurrency@@IAE@I@Z");
    GET(push, "?_Internal_push@_Concurrent_queue_base_v4@details@Concurrency@@IEAAXPEBX@Z", "?_Internal_push@_Concurrent_queue_base_v4@details@Concurrency@@IAEXPBX@Z");
    GET(pop, "?_Internal_pop_if_present@_Concurrent_queue_base_v4@details@Concurrency@@IEAA_NPEAX@Z", "?_Internal_pop_if_present@_Concurrent_queue_base_v4@details@Concurrency@@IAE_NPAX@Z");
    GET(it_ctor, "??0_Concurrent_queue_iterator_base_v4@details@Concurrency@@IEAA@AEBV_Concurrent_queue_base_v4@12@@Z",
                 "??0_Concurrent_queue_iterator_base_v4@details@Concurrency@@IAE@ABV_Concurrent_queue_base_v4@12@@Z");
    GET(it_dtor, "??1_Concurrent_queue_iterator_base_v4@details@Concurrency@@IEAA@XZ", "??1_Concurrent_queue_iterator_base_v4@details@Concurrency@@IAE@XZ");
    GET(it_assign, "?_Assign@_Concurrent_queue_iterator_base_v4@details@Concurrency@@IEAAXAEBV123@@Z", "?_Assign@_Concurrent_queue_iterator_base_v4@details@Concurrency@@IAEXABV123@@Z");
    GET(it_advance, "?_Advance@_Concurrent_queue_iterator_base_v4@details@Concurrency@@IEAAXXZ", "?_Advance@_Concurrent_queue_iterator_base_v4@details@Concurrency@@IAEXXZ");
    GET(line, "?NFS_GetLineSize@details@Concurrency@@YA_KXZ", "?NFS_GetLineSize@details@Concurrency@@YAIXZ");
    GET(nfs_alloc, "?NFS_Allocate@details@Concurrency@@YAPEAX_K0PEAX@Z", "?NFS_Allocate@details@Concurrency@@YAPAXIIPAX@Z");
    GET(nfs_free, "?NFS_Free@details@Concurrency@@YAXPEAX@Z", "?NFS_Free@details@Concurrency@@YAXPAX@Z");
    if (failures) { printf("RESULT: FAIL\n"); return 1; }

    ctor(&q, sizeof(int));
    q.vtable = strcmp(argv[1], "msvcp100") ? queue_vtable : queue_vtable100;

    /* an empty queue: the iterator is at the end */
    it_ctor(&it, &q);
    check(!it.item, "empty queue: no item");
    it_dtor(&it);

    for (i = 0; i < 600; i++) push(&q, &i);
    for (i = 0; i < 50; i++) pop(&q, &v);
    check(v == 49, "popped up to %d", v);

    it_ctor(&it, &q);
    memset(&it2, 0, sizeof(it2));
    ok = 1; steps = 0;
    for (expect = 50; it.item && steps < 1000; expect++, steps++)
    {
        if (*it.item != expect) ok = 0;
        if (expect == 300) it_assign(&it2, &it);
        it_advance(&it);
    }
    check(ok && expect == 600 && !it.item, "iteration 50 .. 599 in order (ended at %d)", expect);
    ok = 1;
    for (expect = 300; it2.item && expect < 700; expect++)
    {
        if (*it2.item != expect) ok = 0;
        it_advance(&it2);
    }
    check(ok && expect == 600, "the copy made at 300 goes on to the end (%d)", expect);
    it_advance(&it);
    check(!it.item, "advancing at the end stays at the end");
    it_dtor(&it);
    it_dtor(&it2);
    while (pop(&q, &v));

    check(line() == 128, "NFS_GetLineSize %Iu", line());
    {
        static const size_t sizes[][2] = {{1, 1}, {10, 7}, {1000, 16}, {0, 8}};
        for (i = 0; i < ARRAYSIZE(sizes); i++)
        {
            char *p = nfs_alloc(sizes[i][0], sizes[i][1], NULL);
            check(p && !((ULONG_PTR)p % line()), "NFS_Allocate(%Iu, %Iu) = %p", sizes[i][0], sizes[i][1], p);
            if (p) memset(p, 0xab, sizes[i][0] * sizes[i][1]);
            nfs_free(p);
        }
        nfs_free(NULL);
    }

    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
