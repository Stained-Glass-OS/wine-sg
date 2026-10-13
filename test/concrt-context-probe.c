/* Probe for patches/sg/4560, in the DLL named on the command line
 * (msvcr110, msvcr120, concrt140): details::_Context's constructor from a
 * Context pointer and its default constructor closure (no context),
 * _Oversubscribe, and _StackGuard::_GetCurrentInlineDepth (a counter of
 * the current thread's context: the same one each call, one per thread,
 * starting at 0). Table driven; lines "ok"/"FAIL", last "RESULT: PASS|FAIL". */
#include <windows.h>
#include <stdio.h>

#ifdef _WIN64
#define THISCALL
#define N(a, b) a
#else
#define THISCALL __attribute__((thiscall))
#define N(a, b) b
#endif

static int failures;
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

static size_t *(__cdecl *p_depth)(void);

static DWORD WINAPI depth_thread(void *arg)
{
    size_t *d = p_depth();

    ((size_t **)arg)[0] = d;
    ((size_t *)arg)[1] = *d;  /* starts at 0 */
    *d += 5;
    ((size_t *)arg)[2] = *p_depth();
    return 0;
}

int main(int argc, char **argv)
{
    void *(THISCALL *ctor)(void **, void *), (THISCALL *closure)(void **);
    void **(__cdecl *current)(void **);
    void *(__cdecl *ctx_current)(void);
    void (__cdecl *oversub)(int);
    HMODULE mod;
    unsigned int i;

    if (argc < 2 || !(mod = LoadLibraryA(argv[1]))) { printf("FAIL load\nRESULT: FAIL\n"); return 1; }
    ctor = (void *)GetProcAddress(mod, N("??0_Context@details@Concurrency@@QEAA@PEAVContext@2@@Z", "??0_Context@details@Concurrency@@QAE@PAVContext@2@@Z"));
    closure = (void *)GetProcAddress(mod, N("??_F_Context@details@Concurrency@@QEAAXXZ", "??_F_Context@details@Concurrency@@QAEXXZ"));
    current = (void *)GetProcAddress(mod, "?_CurrentContext@_Context@details@Concurrency@@SA?AV123@XZ");
    ctx_current = (void *)GetProcAddress(mod, "?CurrentContext@Context@Concurrency@@SAPAV12@XZ");
    if (!ctx_current) ctx_current = (void *)GetProcAddress(mod, "?CurrentContext@Context@Concurrency@@SAPEAV12@XZ");
    oversub = (void *)GetProcAddress(mod, "?_Oversubscribe@_Context@details@Concurrency@@SAX_N@Z");
    p_depth = (void *)GetProcAddress(mod, N("?_GetCurrentInlineDepth@_StackGuard@details@Concurrency@@CAAEA_KXZ",
                                            "?_GetCurrentInlineDepth@_StackGuard@details@Concurrency@@CAAAIXZ"));
    check(ctor && closure && current && ctx_current && oversub && p_depth, "exports %p %p %p %p %p %p",
          ctor, closure, current, ctx_current, oversub, p_depth);
    if (failures) { printf("RESULT: FAIL\n"); return 1; }

    {
        void *values[] = {NULL, (void *)0x1234, ctx_current(), (void *)-1};

        for (i = 0; i < ARRAYSIZE(values); i++)
        {
            void *c = (void *)0xdead;

            check(ctor(&c, values[i]) == &c && c == values[i], "_Context(%p): %p", values[i], c);
        }
    }
    {
        void *c = (void *)0xdead, *cur = (void *)0xdead;

        closure(&c);
        check(c == NULL, "default constructor closure: %p", c);
        current(&cur);
        check(cur == ctx_current() && cur, "_CurrentContext %p", cur);
    }
    oversub(1);
    oversub(0);
    check(1, "_Oversubscribe(true/false) returns");

    {
        size_t *d1 = p_depth(), *d2 = p_depth();
        static const size_t steps[] = {1, 2, 0, 10};
        size_t res[4][3];
        HANDLE threads[4];

        check(d1 && d1 == d2 && *d1 == 0, "_GetCurrentInlineDepth: %p %p, %Iu", d1, d2, d1 ? *d1 : 99);
        for (i = 0; i < ARRAYSIZE(steps); i++)
        {
            *d1 += steps[i];
            check(*p_depth() == *d2, "depth after +%Iu: %Iu", steps[i], *p_depth());
        }
        for (i = 0; i < 4; i++) threads[i] = CreateThread(NULL, 0, depth_thread, res[i], 0, NULL);
        WaitForMultipleObjects(4, threads, TRUE, INFINITE);
        for (i = 0; i < 4; i++)
        {
            check((size_t *)res[i][0] != d1 && res[i][1] == 0 && res[i][2] == 5, "thread %u: its own depth %p, %Iu then %Iu",
                  i, (void *)res[i][0], res[i][1], res[i][2]);
            CloseHandle(threads[i]);
        }
        check(*p_depth() == 13, "this thread's depth kept: %Iu", *p_depth());
        *d1 = 0;
    }

    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
