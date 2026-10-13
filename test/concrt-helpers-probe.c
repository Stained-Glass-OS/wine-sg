/* Probe for patches/sg/4528: Concurrency runtime helpers of the DLL named on
 * the command line (msvcr100 .. msvcr120, concrt140): processor and node
 * counts, OS version, scheduler and context ids, Log2, the shared timer
 * queue, _UnderlyingYield, tracing switches, _SpinWait assignment and the
 * _SpinLock guard between two threads. Lines "ok"/"FAIL", last "RESULT". */
#include <windows.h>
#include <stdio.h>
#include <string.h>

static int failures;
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

#ifdef _WIN64
#define THISCALL
#define N(a64, a32) a64
#else
#define THISCALL __attribute__((thiscall))
#define N(a64, a32) a32
#endif

static void *(THISCALL *p_lock_ctor)(void *, LONG *);
static void (THISCALL *p_lock_dtor)(void *);
static LONG flag;
static DWORD got_lock_at;

static DWORD WINAPI locker(void *arg)
{
    void *lock[1];
    p_lock_ctor(lock, &flag);
    got_lock_at = GetTickCount();
    p_lock_dtor(lock);
    return 0;
}

static void __cdecl yield1(void) {}
static void __cdecl yield2(void) {}

int main(int argc, char **argv)
{
    unsigned int (__cdecl *p_cpus)(void), (__cdecl *p_nodes)(void), (__cdecl *p_sched)(void), (__cdecl *p_ctx)(void);
    int (__cdecl *p_osver)(void);
    ULONG (__cdecl *p_log2)(ULONG_PTR);
    void *(__cdecl *p_timerq)(void);
    void (__cdecl *p_yield)(void);
    HRESULT (__cdecl *p_enable)(void), (__cdecl *p_disable)(void);
    void *(THISCALL *p_sw_ctor)(void *, void *), *(THISCALL *p_sw_ctor_y)(void *, void *), *(THISCALL *p_sw_assign)(void *, const void *);
    SYSTEM_INFO si;
    ULONG highest;
    HMODULE mod;
    unsigned int a, b, i;

    if (argc < 2 || !(mod = LoadLibraryA(argv[1]))) { printf("FAIL load\nRESULT: FAIL\n"); return 1; }
#define GET(v, n) v = (void *)GetProcAddress(mod, n); check(v != NULL, "%s exported", n)
    GET(p_cpus, "?GetProcessorCount@Concurrency@@YAIXZ");
    GET(p_nodes, "?GetProcessorNodeCount@Concurrency@@YAIXZ");
    GET(p_osver, "?GetOSVersion@Concurrency@@YA?AW4OSVersion@IResourceManager@1@XZ");
    GET(p_sched, "?GetSchedulerId@Concurrency@@YAIXZ");
    GET(p_ctx, "?GetExecutionContextId@Concurrency@@YAIXZ");
    GET(p_log2, N("?Log2@details@Concurrency@@YAK_K@Z", "?Log2@details@Concurrency@@YAKI@Z"));
    GET(p_timerq, N("?GetSharedTimerQueue@details@Concurrency@@YAPEAXXZ", "?GetSharedTimerQueue@details@Concurrency@@YAPAXXZ"));
    /* msvcr100 has it on 32-bit only */
    p_yield = (void *)GetProcAddress(mod, "?_UnderlyingYield@details@Concurrency@@YAXXZ");
    check(p_yield || (sizeof(void *) == 8 && !strcmp(argv[1], "msvcr100")), "_UnderlyingYield exported");
    GET(p_enable, "?EnableTracing@Concurrency@@YAJXZ");
    GET(p_disable, "?DisableTracing@Concurrency@@YAJXZ");
    GET(p_sw_ctor, N("??0?$_SpinWait@$0A@@details@Concurrency@@QEAA@P6AXXZ@Z", "??0?$_SpinWait@$0A@@details@Concurrency@@QAE@P6AXXZ@Z"));
    GET(p_sw_ctor_y, N("??0?$_SpinWait@$00@details@Concurrency@@QEAA@P6AXXZ@Z", "??0?$_SpinWait@$00@details@Concurrency@@QAE@P6AXXZ@Z"));
    GET(p_sw_assign, N("??4?$_SpinWait@$00@details@Concurrency@@QEAAAEAV012@AEBV012@@Z", "??4?$_SpinWait@$00@details@Concurrency@@QAEAAV012@ABV012@@Z"));
    GET(p_lock_ctor, N("??0_SpinLock@details@Concurrency@@QEAA@AECJ@Z", "??0_SpinLock@details@Concurrency@@QAE@ACJ@Z"));
    GET(p_lock_dtor, N("??1_SpinLock@details@Concurrency@@QEAA@XZ", "??1_SpinLock@details@Concurrency@@QAE@XZ"));
    if (failures) { printf("RESULT: FAIL\n"); return 1; }

    GetSystemInfo(&si);
    check(p_cpus() == si.dwNumberOfProcessors, "GetProcessorCount %u (%lu)", p_cpus(), si.dwNumberOfProcessors);
    highest = 0;
    GetNumaHighestNodeNumber(&highest);
    check(p_nodes() == highest + 1, "GetProcessorNodeCount %u", p_nodes());
    check(p_osver() == 4, "GetOSVersion %d (Win8OrLater for the reported version)", p_osver());
    a = p_sched(); b = p_sched();
    check(b != a, "GetSchedulerId gives new ids: %u %u", a, b);
    a = p_ctx(); b = p_ctx();
    check(b != a, "GetExecutionContextId gives new ids: %u %u", a, b);
    {
        static const struct { ULONG_PTR n; ULONG want; } logs[] =
        {
            {1, 0}, {2, 1}, {3, 1}, {4, 2}, {1023, 9}, {1024, 10}, {0xffffffff, 31},
#ifdef _WIN64
            {(ULONG_PTR)1 << 40, 40}, {~(ULONG_PTR)0, 63},
#endif
        };
        for (i = 0; i < ARRAYSIZE(logs); i++)
            check(p_log2(logs[i].n) == logs[i].want, "Log2(%Iu) = %lu", logs[i].n, p_log2(logs[i].n));
    }
    {
        void *q = p_timerq();
        HANDLE t;
        check(q && q == p_timerq(), "GetSharedTimerQueue %p, the same each time", q);
        check(CreateTimerQueueTimer(&t, q, (WAITORTIMERCALLBACK)yield1, NULL, 10000, 0, 0) &&
              DeleteTimerQueueTimer(q, t, INVALID_HANDLE_VALUE), "a timer on the shared queue");
    }
    if (p_yield) p_yield();
    check(p_enable() == S_OK && p_disable() == S_OK, "EnableTracing/DisableTracing");

    {
        char sw1[32], sw2[32];
        memset(sw1, 0x11, sizeof(sw1)); memset(sw2, 0x22, sizeof(sw2));
        p_sw_ctor_y(sw1, yield1);
        p_sw_ctor(sw2, yield2);
        check(p_sw_assign(sw2, sw1) == sw2 && !memcmp(sw1, sw2, 3 * sizeof(ULONG) + sizeof(void *)), "_SpinWait assignment copies");
    }
    {
        void *lock[1];
        HANDLE th;
        DWORD released;

        flag = 0;
        p_lock_ctor(lock, &flag);
        check(flag == 1, "_SpinLock takes the flag");
        th = CreateThread(NULL, 0, locker, NULL, 0, NULL);
        Sleep(300);
        released = GetTickCount();
        check(!got_lock_at, "the other thread waits");
        p_lock_dtor(lock);
        check(!WaitForSingleObject(th, 5000) && got_lock_at >= released && flag == 0, "it gets the lock after the release");
        CloseHandle(th);
    }

    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
