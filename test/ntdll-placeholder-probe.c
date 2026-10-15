/* The placeholder compatibility mode of the process and of the thread (ntdll): query, set,
 * the previous mode returned, out-of-range modes refused, thread and process independent. */
#include <windows.h>
#include <stdio.h>

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

typedef char (WINAPI *query_fn)(void);
typedef char (WINAPI *set_fn)(char);

static DWORD WINAPI thread_proc(void *arg)
{
    query_fn query_thread = ((void **)arg)[0], query_process = ((void **)arg)[1];
    set_fn set_thread = ((void **)arg)[2];
    char *results = ((void **)arg)[3];

    results[0] = query_thread();   /* a new thread starts with the default */
    results[1] = query_process();  /* and sees the process mode */
    results[2] = set_thread(1);
    results[3] = query_thread();
    return 0;
}

int main(void)
{
    HMODULE ntdll = GetModuleHandleA("ntdll.dll");
    query_fn query_process = (query_fn)GetProcAddress(ntdll, "RtlQueryProcessPlaceholderCompatibilityMode");
    query_fn query_thread = (query_fn)GetProcAddress(ntdll, "RtlQueryThreadPlaceholderCompatibilityMode");
    set_fn set_process = (set_fn)GetProcAddress(ntdll, "RtlSetProcessPlaceholderCompatibilityMode");
    set_fn set_thread = (set_fn)GetProcAddress(ntdll, "RtlSetThreadPlaceholderCompatibilityMode");
    static const struct { char mode, expect_ret, expect_now; const char *name; } sets[] =
    {
        {2, 0, 2, "expose from default"},
        {1, 2, 1, "disguise returns exposed"},
        {0, 1, 0, "back to default returns disguised"},
        {3, 0, 3, "disguise-full (value 3)"},
        {-1, -1, 3, "negative mode refused"},
        {4, -1, 3, "above the maximum refused"},
        {127, -1, 3, "far above refused"},
        {0, 3, 0, "refused sets changed nothing"},
    };
    unsigned int i;
    char results[4] = {-9, -9, -9, -9};
    void *args[4] = {query_thread, query_process, set_thread, results};
    HANDLE thread;

    check(query_process && query_thread && set_process && set_thread, "the four functions are exported");
    if (failures) { printf("RESULT: FAIL\n"); return 1; }

    check(query_process() == 0, "process mode starts at the default");
    check(query_thread() == 0, "thread mode starts at the default");

    for (i = 0; i < sizeof(sets) / sizeof(sets[0]); ++i)
    {
        char ret = set_process(sets[i].mode);
        char now = query_process();
        printf("      set %d -> returned %d, now %d\n", sets[i].mode, ret, now);
        check(ret == sets[i].expect_ret && now == sets[i].expect_now, sets[i].name);
    }

    /* the thread has a mode of its own, the process mode is the same for all */
    set_process(2);
    set_thread(0);
    check(set_thread(2) == 0, "thread set returns the previous mode");
    check(set_thread(9) == -1 && query_thread() == 2, "thread: out of range refused");
    check(query_process() == 2, "thread sets leave the process mode alone");
    thread = CreateThread(NULL, 0, thread_proc, args, 0, NULL);
    WaitForSingleObject(thread, 10000);
    CloseHandle(thread);
    check(results[0] == 0, "a new thread starts at the default");
    check(results[1] == 2, "a new thread sees the process mode");
    check(results[2] == 0 && results[3] == 1, "a thread's own set works");
    check(query_thread() == 2, "other threads' sets do not change this one");

    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
