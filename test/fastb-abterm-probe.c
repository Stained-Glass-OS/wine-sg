/* _abnormal_termination, _heapmin, RtlCompactHeap (patch 3759) */
#include <windows.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>

static int f;
#define CHECK(c, ...) do { if (!(c)) { f++; printf("FAIL line %d: %s: ", __LINE__, #c); printf(__VA_ARGS__); printf("\n"); } } while (0)

static int (__cdecl *p_abnormal)(void);
static int (__cdecl *p_heapmin)(void);
static void (__cdecl *p_local_unwind2)(void *frame, int trylevel);
static int *(__cdecl *p_errno)(void);

static int seen[8], nseen;

static void __cdecl record(void)
{
    if (nseen < 8) seen[nseen++] = p_abnormal();
}
static void __cdecl record_a(void) { record(); }
static void __cdecl record_b(void) { record(); }
static void __cdecl never(void) { seen[7] = 99; }

#ifdef __i386__
struct scope { int prev; void *filter; void *handler; };
struct frame { void *prev; void *handler; struct scope *scopetable; int trylevel; int ebp; void *xpointers; };

static void unwind_tests(void)
{
    struct scope st[3] = { { -1, NULL, record_a }, { 0, NULL, record_b }, { 1, never, never } };
    struct frame fr;
    void *top_before = ((NT_TIB *)NtCurrentTeb())->ExceptionList;

    nseen = 0; seen[7] = 0;
    record();
    CHECK(nseen == 1 && seen[0] == 0, "an ordinary call is not abnormal: %d", seen[0]);

    /* the finally blocks of levels 1 and 0 run from the unwinder; level 2 has a filter and is not a finally */
    memset(&fr, 0, sizeof(fr));
    fr.scopetable = st;
    fr.trylevel = 2;
    nseen = 0;
    p_local_unwind2(&fr, -1);
    CHECK(nseen == 2, "two finally blocks ran: %d", nseen);
    CHECK(seen[0] == 1 && seen[1] == 1, "from the unwinder they are abnormal: %d %d", seen[0], seen[1]);
    CHECK(seen[7] == 0, "the filter entry was not run");
    CHECK(fr.trylevel == -1, "trylevel %d", fr.trylevel);
    CHECK(((NT_TIB *)NtCurrentTeb())->ExceptionList == top_before, "the registration list is as it was");

    /* unwinding down to a level only runs the levels above it */
    fr.trylevel = 1;
    nseen = 0;
    p_local_unwind2(&fr, 0);
    CHECK(nseen == 1 && seen[0] == 1 && fr.trylevel == 0, "one level: %d seen, trylevel %d", nseen, fr.trylevel);

    /* afterwards a plain call is normal again */
    nseen = 0;
    record();
    CHECK(seen[0] == 0, "normal again: %d", seen[0]);
}
#else
static int veh_direct, veh_helper, veh_ran;

static int __attribute__((noinline)) helper(void)
{
    volatile int r = p_abnormal();   /* not a tail call: the helper stays in the chain */
    return r;
}

static LONG CALLBACK veh(EXCEPTION_POINTERS *ep)
{
    if (ep->ExceptionRecord->ExceptionCode != 0xe0fa57b1) return EXCEPTION_CONTINUE_SEARCH;
    veh_ran = 1;
    veh_direct = p_abnormal();       /* called by the exception dispatcher in ntdll */
    veh_helper = helper();           /* called by a function of ours */
    return EXCEPTION_CONTINUE_EXECUTION;
}

static void unwind_tests(void)
{
    void *h = AddVectoredExceptionHandler(1, veh);

    CHECK(p_abnormal() == 0, "an ordinary call is not abnormal");
    CHECK(helper() == 0, "a call through a helper is not abnormal");
    RaiseException(0xe0fa57b1, 0, 0, NULL);
    CHECK(veh_ran, "the handler ran");
    CHECK(veh_direct == 1, "called from the unwinder / dispatcher: %d", veh_direct);
    CHECK(veh_helper == 0, "called by our function that the dispatcher called: %d", veh_helper);
    RemoveVectoredExceptionHandler(h);
    CHECK(p_abnormal() == 0, "normal again");
}
#endif

static void heap_tests(void)
{
    HANDLE heap = HeapCreate(0, 0, 0);
    void *a, *b, *c;
    SIZE_T largest;
    int r;

    CHECK(heap != NULL, "HeapCreate");
    a = HeapAlloc(heap, 0, 20000);
    b = HeapAlloc(heap, 0, 10000);
    c = HeapAlloc(heap, 0, 1000);
    CHECK(a && b && c, "allocations");
    SetLastError(0xdeadbeef);
    largest = HeapCompact(heap, 0);
    CHECK(GetLastError() == 0 || largest > 0, "HeapCompact: %Iu err %lu", largest, GetLastError());
    HeapFree(heap, 0, a);
    SetLastError(0xdeadbeef);
    largest = HeapCompact(heap, 0);
    CHECK(largest >= 20000, "the freed 20000 bytes are the largest free block: %Iu", largest);
    CHECK(GetLastError() == 0, "no error: %lu", GetLastError());
    HeapFree(heap, 0, b);
    largest = HeapCompact(heap, 0);
    CHECK(largest >= 30000, "freed neighbours are one block: %Iu", largest);
    CHECK(largest < 100000000, "and not more than the heap holds: %Iu", largest);
    HeapFree(heap, 0, c);
    CHECK(HeapCompact(heap, 0) >= 30000, "everything free");
    HeapDestroy(heap);

    /* the largest free block, wherever it is, and no used block counts */
    heap = HeapCreate(0, 0, 0);
    {
        void *sm = HeapAlloc(heap, 0, 500), *k1 = HeapAlloc(heap, 0, 100), *big = HeapAlloc(heap, 0, 20000);
        void *used = HeapAlloc(heap, 0, 30000), *k2 = HeapAlloc(heap, 0, 100);

        HeapFree(heap, 0, sm);
        HeapFree(heap, 0, big);
        largest = HeapCompact(heap, 0);
        CHECK(largest >= 20000, "the 20000 free bytes after a small free block: %Iu", largest);
        CHECK(largest < 30000, "a used block of 30000 does not count: %Iu", largest);
        HeapFree(heap, 0, k1); HeapFree(heap, 0, used); HeapFree(heap, 0, k2);
    }
    SetLastError(0xdeadbeef);
    CHECK(HeapCompact(NULL, 0) == 0 && GetLastError() == ERROR_INVALID_HANDLE, "a NULL heap handle: %lu", GetLastError());

    /* _heapmin: 0 for success (the C runtime's heaps), and no errno */
    *p_errno() = 0;
    r = p_heapmin();
    CHECK(r == 0, "_heapmin returns %d", r);
    CHECK(*p_errno() == 0, "errno %d", *p_errno());
    {
        void *m = malloc(300000);
        free(m);
        r = p_heapmin();
        CHECK(r == 0, "_heapmin after a free: %d", r);
    }
    /* a process heap whose blocks are all in use still compacts: the answer is a number, not an error */
    SetLastError(0xdeadbeef);
    largest = HeapCompact(GetProcessHeap(), 0);
    CHECK(largest > 0 || GetLastError() == 0, "process heap: %Iu err %lu", largest, GetLastError());
}

int main(void)
{
    HMODULE crt = LoadLibraryA("msvcrt.dll");

    p_abnormal = (void *)GetProcAddress(crt, "_abnormal_termination");
    p_heapmin = (void *)GetProcAddress(crt, "_heapmin");
    p_local_unwind2 = (void *)GetProcAddress(crt, "_local_unwind2");
    p_errno = (void *)GetProcAddress(crt, "_errno");
    CHECK(p_abnormal && p_heapmin && p_errno, "exports");
    if (!p_abnormal || !p_heapmin) { printf("RESULT: FAIL\n"); return 1; }

#ifdef __i386__
    CHECK(p_local_unwind2 != NULL, "_local_unwind2");
    if (p_local_unwind2)
#endif
    unwind_tests();
    heap_tests();
    printf("%s\n", f ? "RESULT: FAIL" : "RESULT: PASS");
    return f != 0;
}
