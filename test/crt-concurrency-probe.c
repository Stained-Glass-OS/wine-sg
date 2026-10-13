/* Probe for patches/sg/4506:
 *   probe.exe swap DLL        _Concurrent_queue_base_v4::_Internal_swap (concrt140, msvcp110, msvcp120)
 *   probe.exe nonblocking DLL _Task_impl_base::_IsNonBlockingThread (msvcp140, msvcp_win)
 *   probe.exe report DLL      _ExceptionHolder::ReportUnhandledError must end the program */
#define COBJMACROS
#include <windows.h>
#include <objbase.h>
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

static void THISCALL q_move(struct queue *q, struct page *p, size_t idx, void *src) { p->items[idx] = *(int *)src; }
static void THISCALL q_copy(struct queue *q, struct page *p, size_t idx, const void *src) { p->items[idx] = *(const int *)src; }
static void THISCALL q_assign(struct queue *q, void *dst, struct page *p, size_t idx) { *(int *)dst = p->items[idx]; }
static void *THISCALL q_dtor(struct queue *q, unsigned int flags) { return q; }
static struct page *THISCALL q_alloc(struct queue *q) { return calloc(1, sizeof(struct page)); }
static void THISCALL q_free(struct queue *q, struct page *p) { free(p); }
static const void *queue_vtable[] = {q_move, q_copy, q_assign, q_dtor, q_alloc, q_free};

static void test_swap(HMODULE mod)
{
    void (THISCALL *ctor)(struct queue *, size_t) = (void *)GetProcAddress(mod, N("??0_Concurrent_queue_base_v4@details@Concurrency@@IEAA@_K@Z", "??0_Concurrent_queue_base_v4@details@Concurrency@@IAE@I@Z"));
    void (THISCALL *push)(struct queue *, const void *) = (void *)GetProcAddress(mod, N("?_Internal_push@_Concurrent_queue_base_v4@details@Concurrency@@IEAAXPEBX@Z", "?_Internal_push@_Concurrent_queue_base_v4@details@Concurrency@@IAEXPBX@Z"));
    unsigned char (THISCALL *pop)(struct queue *, void *) = (void *)GetProcAddress(mod, N("?_Internal_pop_if_present@_Concurrent_queue_base_v4@details@Concurrency@@IEAA_NPEAX@Z", "?_Internal_pop_if_present@_Concurrent_queue_base_v4@details@Concurrency@@IAE_NPAX@Z"));
    size_t (THISCALL *size)(const struct queue *) = (void *)GetProcAddress(mod, N("?_Internal_size@_Concurrent_queue_base_v4@details@Concurrency@@IEBA_KXZ", "?_Internal_size@_Concurrent_queue_base_v4@details@Concurrency@@IBEIXZ"));
    void (THISCALL *swap)(struct queue *, struct queue *) = (void *)GetProcAddress(mod, N("?_Internal_swap@_Concurrent_queue_base_v4@details@Concurrency@@IEAAXAEAV123@@Z", "?_Internal_swap@_Concurrent_queue_base_v4@details@Concurrency@@IAEXAAV123@@Z"));
    static const int a0[] = {1, 2}, b0[] = {9}, a1[] = {5};
    static const struct { const int *a; unsigned int na; const int *b; unsigned int nb; } tests[] =
    {
        {a0, 2, b0, 1},
        {a1, 1, NULL, 0},
        {NULL, 0, NULL, 0},
    };
    struct queue q1, q2;
    unsigned int i, j;
    int v;

    check(ctor && push && pop && size && swap, "exports");
    if (!ctor || !push || !pop || !size || !swap) return;
    for (i = 0; i < ARRAYSIZE(tests); i++)
    {
        BOOL same = TRUE;

        ctor(&q1, sizeof(int)); q1.vtable = queue_vtable;
        ctor(&q2, sizeof(int)); q2.vtable = queue_vtable;
        for (j = 0; j < tests[i].na; j++) push(&q1, &tests[i].a[j]);
        for (j = 0; j < tests[i].nb; j++) push(&q2, &tests[i].b[j]);
        swap(&q1, &q2);
        check(size(&q1) == tests[i].nb && size(&q2) == tests[i].na, "swap %u: sizes %u %u", i,
              (unsigned)size(&q1), (unsigned)size(&q2));
        for (j = 0; j < tests[i].nb; j++) if (!pop(&q1, &v) || v != tests[i].b[j]) same = FALSE;
        for (j = 0; j < tests[i].na; j++) if (!pop(&q2, &v) || v != tests[i].a[j]) same = FALSE;
        check(same && !pop(&q1, &v) && !pop(&q2, &v), "swap %u: items in order", i);
    }
    swap(&q1, &q1);
    check(size(&q1) == 0, "self swap");
}

static void test_nonblocking(HMODULE mod)
{
    unsigned char (__cdecl *nonblocking)(void) = (void *)GetProcAddress(mod, "?_IsNonBlockingThread@_Task_impl_base@details@Concurrency@@SA_NXZ");

    check(nonblocking != NULL, "export");
    if (!nonblocking) return;
    check(!nonblocking(), "no apartment: may block");
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    check(!nonblocking(), "a desktop STA may block (only an application STA may not)");
    CoUninitialize();
    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    check(!nonblocking(), "the MTA may block");
    CoUninitialize();
}

int main(int argc, char **argv)
{
    HMODULE mod;

    if (argc < 3 || !(mod = LoadLibraryA(argv[2]))) { printf("FAIL usage/load\nRESULT: FAIL\n"); return 1; }
    if (!strcmp(argv[1], "report"))
    {
        void (THISCALL *report)(void *) = (void *)GetProcAddress(mod, N("?ReportUnhandledError@_ExceptionHolder@details@Concurrency@@AEAAXXZ", "?ReportUnhandledError@_ExceptionHolder@details@Concurrency@@AAEXXZ"));
        static char holder[64];

        if (!report) { printf("FAIL no export\nRESULT: FAIL\n"); return 1; }
        fflush(stdout);
        report(holder);
        printf("RETURNED\nRESULT: FAIL\n");
        return 0;
    }
    if (!strcmp(argv[1], "swap")) test_swap(mod);
    else if (!strcmp(argv[1], "nonblocking")) test_nonblocking(mod);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
