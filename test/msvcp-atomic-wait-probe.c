/* Probe for patches/sg/4519: msvcp140_atomic_wait's indirect waits and
 * notifications, wait deadlines, api level, the mutexes of lock-based
 * atomics, the 128-bit compare-exchange and the parallel algorithms' byte
 * wait. Threads wait and the main thread changes the value and notifies.
 * Lines "ok"/"FAIL", last "RESULT: PASS|FAIL". */
#include <windows.h>
#include <stdio.h>
#include <string.h>

static int failures;
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

typedef BOOL (__stdcall *equal_func)(const void *, void *, SIZE_T, void *);
static BOOL (__stdcall *p_wait_indirect)(const void *, void *, SIZE_T, void *, equal_func, DWORD);
static void (__stdcall *p_notify_one)(const void *), (__stdcall *p_notify_all)(const void *);
static ULONGLONG (__stdcall *p_deadline)(ULONGLONG);
static DWORD (__stdcall *p_remaining)(ULONGLONG);
static int (__stdcall *p_api_level)(int);
static SRWLOCK *(__stdcall *p_get_mutex)(const void *);
static BOOL (__stdcall *p_has16b)(void);
static BOOL (__stdcall *p_cas128)(LONGLONG *, LONGLONG, LONGLONG, LONGLONG *);
static void (__stdcall *p_wait_uchar)(const volatile unsigned char *, unsigned char);
static void (__stdcall *p_wake_all)(const volatile void *);

struct big { volatile LONG v; char pad[20]; };
static struct big shared;
static LONG equal_calls;

static BOOL __stdcall equal(const void *addr, void *cmp, SIZE_T size, void *param)
{
    InterlockedIncrement(&equal_calls);
    return ((const struct big *)addr)->v == ((struct big *)cmp)->v && size == sizeof(struct big) && param == (void *)0x55;
}

/* waits until shared.v changes from 0, the STL way: check, wait, check again */
static DWORD WINAPI waiter(void *arg)
{
    ULONGLONG deadline = p_deadline(5000);
    struct big cmp = {0};

    while (shared.v == 0)
    {
        DWORD left = p_remaining(deadline);
        if (!left) return 1;  /* timed out */
        p_wait_indirect(&shared, &cmp, sizeof(cmp), (void *)0x55, equal, left);
    }
    return 0;
}

static volatile unsigned char flag;
static DWORD WINAPI uchar_waiter(void *arg)
{
    while (flag == 0) p_wait_uchar(&flag, 0);
    return 0;
}

int main(int argc, char **argv)
{
    HMODULE mod = LoadLibraryA("msvcp140_atomic_wait.dll");
    ULONGLONG now, d;
    struct big cmp = {0};
    HANDLE threads[3];
    DWORD r, start, code;
    BOOL ret;
    unsigned int i;

    if (!mod) { printf("FAIL load\nRESULT: FAIL\n"); return 1; }
#define GET(v, n) v = (void *)GetProcAddress(mod, n); check(v != NULL, "%s exported", n)
    GET(p_wait_indirect, "__std_atomic_wait_indirect"); GET(p_notify_one, "__std_atomic_notify_one_indirect");
    GET(p_notify_all, "__std_atomic_notify_all_indirect"); GET(p_deadline, "__std_atomic_wait_get_deadline");
    GET(p_remaining, "__std_atomic_wait_get_remaining_timeout"); GET(p_api_level, "__std_atomic_set_api_level");
    GET(p_get_mutex, "__std_atomic_get_mutex"); GET(p_has16b, "__std_atomic_has_cmpxchg16b");
    GET(p_cas128, "__std_atomic_compare_exchange_128"); GET(p_wait_uchar, "__std_execution_wait_on_uchar");
    GET(p_wake_all, "__std_execution_wake_by_address_all");
    if (failures) { printf("RESULT: FAIL\n"); return 1; }

    check(p_api_level(0) == 3 && p_api_level(2) == 3, "api level: WaitOnAddress");

    /* deadlines */
    check(p_deadline(~0ull) == ~0ull, "no deadline");
    now = GetTickCount64();
    d = p_deadline(1000);
    check(d >= now + 1000 && d <= GetTickCount64() + 1000, "deadline now + 1000");
    r = p_remaining(d);
    check(r <= 1000 && r > 900, "remaining %lu", r);
    check(p_remaining(~0ull) == INFINITE, "remaining without deadline");
    check(p_remaining(now > 10 ? now - 10 : 0) == 0, "remaining after the deadline");
    check(p_remaining(GetTickCount64() + 0x200000000ull) == INFINITE - 1, "a far deadline stays finite");

    /* indirect waits by themselves */
    shared.v = 1;
    equal_calls = 0;
    ret = p_wait_indirect(&shared, &cmp, sizeof(cmp), (void *)0x55, equal, 5000);
    check(ret && equal_calls == 1, "different value: no wait (%d, %ld calls)", ret, equal_calls);
    shared.v = 0;
    start = GetTickCount();
    SetLastError(0xdeadbeef);
    ret = p_wait_indirect(&shared, &cmp, sizeof(cmp), (void *)0x55, equal, 100);
    r = GetTickCount() - start;
    check(!ret && GetLastError() == ERROR_TIMEOUT && r >= 80, "equal value: times out (%d, %lu, %lu ms)", ret, GetLastError(), r);

    /* one waiter, notify_one; then two waiters, notify_all */
    for (i = 0; i < 2; i++)
    {
        unsigned int n = i ? 2 : 1, k;

        shared.v = 0;
        for (k = 0; k < n; k++) threads[k] = CreateThread(NULL, 0, waiter, NULL, 0, NULL);
        Sleep(200);
        start = GetTickCount();
        InterlockedExchange(&shared.v, 1);
        if (i) p_notify_all(&shared); else p_notify_one(&shared);
        r = WaitForMultipleObjects(n, threads, TRUE, 10000);
        for (k = 0; k < n; k++)
        {
            GetExitCodeThread(threads[k], &code);
            check(!r && !code && GetTickCount() - start < 3000, "%s: waiter %u woken (exit %lu, %lu ms)",
                  i ? "notify_all" : "notify_one", k, code, GetTickCount() - start);
            CloseHandle(threads[k]);
        }
    }

    /* lock-based atomics */
    {
        static char a[16], b[16];
        SRWLOCK *m1 = p_get_mutex(a), *m2 = p_get_mutex(a), *m3 = p_get_mutex(b);

        check(m1 && m1 == m2 && m3, "__std_atomic_get_mutex: same key, same lock (%p %p %p)", m1, m2, m3);
        AcquireSRWLockExclusive(m1);
        check(!TryAcquireSRWLockExclusive(m2), "the lock is held");
        ReleaseSRWLockExclusive(m1);
    }

    /* 128-bit compare-exchange */
    {
        static LONGLONG DECLSPEC_ALIGN(16) value[2] = {1, 2};
        LONGLONG DECLSPEC_ALIGN(16) comparand[2] = {1, 2};

        ret = p_cas128(value, 20, 10, comparand);
        check(ret && value[0] == 10 && value[1] == 20, "cmpxchg128 equal: %d %I64d %I64d", ret, value[0], value[1]);
        comparand[0] = 1; comparand[1] = 2;
        ret = p_cas128(value, 40, 30, comparand);
        check(!ret && value[0] == 10 && value[1] == 20 && comparand[0] == 10 && comparand[1] == 20,
              "cmpxchg128 different: %d, comparand %I64d %I64d", ret, comparand[0], comparand[1]);
    }
#ifdef _WIN64
    check(p_has16b() == !!IsProcessorFeaturePresent(PF_COMPARE_EXCHANGE128), "has_cmpxchg16b");
#else
    check(!p_has16b(), "has_cmpxchg16b on 32-bit");
#endif

    /* the byte wait of the parallel algorithms */
    flag = 1;
    p_wait_uchar(&flag, 0);
    check(1, "byte wait returns at once when the value differs");
    flag = 0;
    threads[0] = CreateThread(NULL, 0, uchar_waiter, NULL, 0, NULL);
    Sleep(100);
    flag = 1;
    p_wake_all(&flag);
    r = WaitForSingleObject(threads[0], 5000);
    check(!r, "byte waiter woken (%lu)", r);
    CloseHandle(threads[0]);

    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
