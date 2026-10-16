/* Probe for patches/sg/5024: DeleteTimerQueueEx with an event returns at
 * once ("the function returns immediately and the event is signaled when all
 * callback functions have completed"); the event is set when the running
 * callback is done. NULL also returns at once, INVALID_HANDLE_VALUE waits.
 * Table driven; lines "ok"/"FAIL", last "RESULT: PASS|FAIL". */
#include <windows.h>
#include <stdio.h>

static int failures;
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

static HANDLE started;
static LONG done;

static void CALLBACK slow_cb(void *param, BOOLEAN fired)
{
    SetEvent(started);
    Sleep(600);
    InterlockedIncrement(&done);
}

int main(void)
{
    static const struct { const char *what; int event; BOOL waits; } tests[] =
    {
        {"an event", 1, FALSE}, {"NULL", 0, FALSE}, {"INVALID_HANDLE_VALUE", 2, TRUE},
    };
    unsigned int i;

    started = CreateEventA(NULL, FALSE, FALSE, NULL);
    for (i = 0; i < ARRAYSIZE(tests); i++)
    {
        HANDLE q = CreateTimerQueue(), t, e = CreateEventA(NULL, TRUE, FALSE, NULL), arg;
        DWORD t0, took;
        BOOL ret;

        done = 0;
        CreateTimerQueueTimer(&t, q, slow_cb, NULL, 10, 0, 0);
        WaitForSingleObject(started, 2000);
        arg = tests[i].event == 1 ? e : tests[i].event == 2 ? INVALID_HANDLE_VALUE : NULL;
        t0 = GetTickCount();
        ret = DeleteTimerQueueEx(q, arg);
        took = GetTickCount() - t0;
        if (tests[i].waits)
            check(ret && took >= 300 && done == 1, "%s: waits for the callback (%lu ms, %d, done %ld)", tests[i].what, took, ret, done);
        else
            /* Vista and later may return TRUE (kernel32 sync test) */
            check((ret || GetLastError() == ERROR_IO_PENDING) && took < 300 && !done,
                  "%s: returns at once (%lu ms, %d, %lu)", tests[i].what, took, ret, GetLastError());
        if (tests[i].event == 1)
        {
            check(WaitForSingleObject(e, 0) == WAIT_TIMEOUT, "%s: not set while the callback runs", tests[i].what);
            check(WaitForSingleObject(e, 3000) == WAIT_OBJECT_0 && done == 1, "%s: set when it is done (done %ld)", tests[i].what, done);
        }
        else Sleep(800);
        CloseHandle(e);
    }
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
