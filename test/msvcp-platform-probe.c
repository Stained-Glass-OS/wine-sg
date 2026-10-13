/* Probe for patches/sg/4551: the Concurrency platform helpers of the DLL
 * named on the command line: GetNextAsyncId (_GetNextAsyncId in msvcp110;
 * one more each call, unique across threads), and in msvcp140/msvcp_win
 * CaptureCallstack (the return addresses from the caller up, skipping the
 * given number of frames) and __set_stl_sync_api_mode.
 * Table driven; lines "ok"/"FAIL", last "RESULT: PASS|FAIL". */
/* the frames are walked through the frame pointers on i386 */
#pragma GCC optimize("no-omit-frame-pointer")
#include <windows.h>
#include <stdio.h>
#include <string.h>

static int failures;
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

#ifdef _WIN64
#define N(a, b) a
#else
#define N(a, b) b
#endif

static unsigned int (__cdecl *p_next_id)(void);
static size_t (__cdecl *p_capture)(void **, size_t, size_t);

#define IDS 2000
static unsigned int ids[4][IDS];
static DWORD WINAPI id_thread(void *arg)
{
    unsigned int i, *out = arg;
    for (i = 0; i < IDS; i++) out[i] = p_next_id();
    return 0;
}

static int cmp(const void *a, const void *b)
{
    unsigned int x = *(const unsigned int *)a, y = *(const unsigned int *)b;
    return x < y ? -1 : x > y;
}

/* captures from inside itself, through two levels of callers */
static size_t __attribute__((noinline)) level2(void **frames, size_t skip, size_t count)
{
    size_t ret = p_capture(frames, skip, count);
    __asm__ __volatile__("" ::: "memory");
    return ret;
}
static size_t __attribute__((noinline)) level1(void **frames, size_t skip, size_t count)
{
    size_t ret = level2(frames, skip, count);
    __asm__ __volatile__("" ::: "memory");
    return ret;
}

static int in_func(void *addr, void *func)
{
    return (char *)addr > (char *)func && (char *)addr < (char *)func + 0x100;
}

int main(int argc, char **argv)
{
    HMODULE mod;
    unsigned int i, a, b;
    int is140;

    if (argc < 2 || !(mod = LoadLibraryA(argv[1]))) { printf("FAIL load\nRESULT: FAIL\n"); return 1; }
    is140 = !strcmp(argv[1], "msvcp140") || !strcmp(argv[1], "msvcp_win");
    p_next_id = (void *)GetProcAddress(mod, !strcmp(argv[1], "msvcp110") ? "?_GetNextAsyncId@details@Concurrency@@YAIXZ"
                                                                       : "?GetNextAsyncId@platform@details@Concurrency@@YAIXZ");
    check(p_next_id != NULL, "GetNextAsyncId exported");
    if (!p_next_id) { printf("RESULT: FAIL\n"); return 1; }

    a = p_next_id(); b = p_next_id();
    check(a && b == a + 1, "GetNextAsyncId: %u then %u", a, b);
    {
        HANDLE threads[4];
        unsigned int dup = 0, *all = &ids[0][0];

        for (i = 0; i < 4; i++) threads[i] = CreateThread(NULL, 0, id_thread, ids[i], 0, NULL);
        WaitForMultipleObjects(4, threads, TRUE, INFINITE);
        for (i = 0; i < 4; i++) CloseHandle(threads[i]);
        qsort(all, 4 * IDS, sizeof(unsigned int), cmp);
        for (i = 1; i < 4 * IDS; i++) if (all[i] == all[i - 1]) dup++;
        check(!dup && all[0] > b && all[4 * IDS - 1] == all[0] + 4 * IDS - 1, "GetNextAsyncId on 4 threads: %u duplicates, %u..%u",
              dup, all[0], all[4 * IDS - 1]);
    }

    if (is140)
    {
        static const struct { size_t skip, count, want_max; int first; } tests[] =
        {
            /* first: 2 = inside level2, 1 = inside level1, 0 = inside main */
            {0, 8, 8, 2}, {1, 8, 8, 1}, {2, 8, 8, 0}, {0, 1, 1, 2}, {0, 0, 0, -1}, {1, 2, 2, 1}, {100000, 4, 0, -1},
        };
        void (__cdecl *p_mode)(int) = (void *)GetProcAddress(mod, "__set_stl_sync_api_mode");

        p_capture = (void *)GetProcAddress(mod, N("?CaptureCallstack@platform@details@Concurrency@@YA_KPEAPEAX_K1@Z",
                                                  "?CaptureCallstack@platform@details@Concurrency@@YAIPAPAXII@Z"));
        check(p_capture && p_mode, "CaptureCallstack, __set_stl_sync_api_mode exported");
        if (!p_capture || !p_mode) { printf("RESULT: FAIL\n"); return 1; }
        for (i = 0; i < ARRAYSIZE(tests); i++)
        {
            void *frames[16];
            size_t n;
            int ok;

            memset(frames, 0, sizeof(frames));
            n = level1(frames, tests[i].skip, tests[i].count);
            ok = n <= tests[i].want_max && !frames[tests[i].count];
            if (tests[i].first == 2) ok = ok && n >= 1 && in_func(frames[0], level2) && (n < 2 || in_func(frames[1], level1));
            if (tests[i].first == 1) ok = ok && n >= 1 && in_func(frames[0], level1);
            if (tests[i].first == 0) ok = ok && n >= 1 && (char *)frames[0] > (char *)main && (char *)frames[0] < (char *)main + 0x1000;
            if (tests[i].first == -1) ok = ok && n == 0;
            check(ok, "CaptureCallstack(skip %u, count %u) = %u, first %p (level2 %p level1 %p main %p)",
                  (unsigned)tests[i].skip, (unsigned)tests[i].count, (unsigned)n, frames[0], level2, level1, main);
        }
        for (i = 0; i < 4; i++) p_mode(i);
        check(1, "__set_stl_sync_api_mode(0..3) returns");
    }

    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
