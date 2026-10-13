/* Probe for patches/sg/4550: msvcp140's (and msvcp_win's)
 * __ExceptionPtrSwap: both exception_ptr values are exchanged, nothing
 * else (no reference count change), checked against __ExceptionPtrCompare
 * and __ExceptionPtrCopy. Table driven; lines "ok"/"FAIL", last
 * "RESULT: PASS|FAIL". */
#include <windows.h>
#include <stdio.h>

static int failures;
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

#ifdef _WIN64
#define N(a, b) a
#else
#define N(a, b) b
#endif

typedef struct { void *rec; LONG *ref; } eptr;

int main(int argc, char **argv)
{
    static LONG refs[3] = {1, 1, 1};
    static const struct { void *a0; LONG *a1; void *b0; LONG *b1; } tests[] =
    {
        {(void *)1, NULL, (void *)2, NULL},
        {NULL, NULL, (void *)0x1234, &refs[0]},
        {(void *)0x10, &refs[1], (void *)0x20, &refs[2]},
        {(void *)0x30, &refs[1], (void *)0x30, &refs[1]},
        {NULL, NULL, NULL, NULL},
    };
    void (__cdecl *p_swap)(eptr *, eptr *);
    BOOL (__cdecl *p_compare)(const eptr *, const eptr *);
    void (__cdecl *p_create)(eptr *);
    HMODULE mod;
    unsigned int i;

    if (argc < 2 || !(mod = LoadLibraryA(argv[1]))) { printf("FAIL load\nRESULT: FAIL\n"); return 1; }
    p_swap = (void *)GetProcAddress(mod, N("?__ExceptionPtrSwap@@YAXPEAX0@Z", "?__ExceptionPtrSwap@@YAXPAX0@Z"));
    p_compare = (void *)GetProcAddress(mod, N("?__ExceptionPtrCompare@@YA_NPEBX0@Z", "?__ExceptionPtrCompare@@YA_NPBX0@Z"));
    p_create = (void *)GetProcAddress(mod, N("?__ExceptionPtrCreate@@YAXPEAX@Z", "?__ExceptionPtrCreate@@YAXPAX@Z"));
    check(p_swap && p_compare && p_create, "exports");
    if (failures) { printf("RESULT: FAIL\n"); return 1; }

    for (i = 0; i < ARRAYSIZE(tests); i++)
    {
        eptr a = {tests[i].a0, tests[i].a1}, b = {tests[i].b0, tests[i].b1};
        eptr a0 = a, b0 = b;

        p_swap(&a, &b);
        check(a.rec == b0.rec && a.ref == b0.ref && b.rec == a0.rec && b.ref == a0.ref, "case %u: swapped", i);
        check(p_compare(&a, &b0) && p_compare(&b, &a0), "case %u: compares equal to the other", i);
        check(refs[0] == 1 && refs[1] == 1 && refs[2] == 1, "case %u: no reference count change", i);
        p_swap(&a, &a);
        check(a.rec == b0.rec && a.ref == b0.ref, "case %u: swap with itself", i);
    }
    {
        eptr a, b = {(void *)5, NULL};

        p_create(&a);
        p_swap(&a, &b);
        check(a.rec == (void *)5 && !b.rec && !b.ref, "with a null exception_ptr");
    }

    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
