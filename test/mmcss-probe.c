/* The multimedia class scheduler and thread ordering groups
 * (patches/sg/1636). AvSetMmThreadCharacteristics took any name and did
 * nothing; AvSetMmMaxThreadCharacteristics refused two tasks;
 * AvSetMmThreadPriority, AvRevertMmThreadCharacteristics and
 * AvQuerySystemResponsiveness were stubs; the thread ordering group
 * functions were not exported. */
#include <windows.h>
#include <avrt.h>
#include <stdio.h>

static int failures;
static HMODULE avrt;
static HANDLE (WINAPI *pSet)(const WCHAR *, DWORD *);
static HANDLE (WINAPI *pSetMax)(const WCHAR *, const WCHAR *, DWORD *);
static BOOL (WINAPI *pRevert)(HANDLE);
static BOOL (WINAPI *pSetPrio)(HANDLE, AVRT_PRIORITY);
static BOOL (WINAPI *pQuery)(HANDLE, ULONG *);
static BOOL (WINAPI *pCreate)(HANDLE *, LARGE_INTEGER *, GUID *, LARGE_INTEGER *);
static BOOL (WINAPI *pJoin)(HANDLE *, GUID *, BOOL);
static BOOL (WINAPI *pWait)(HANDLE);
static BOOL (WINAPI *pLeave)(HANDLE);
static BOOL (WINAPI *pDelete)(HANDLE);

static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

static GUID group_guid;
static char order[64];
static LONG order_pos;

struct client { BOOL before; char mark; HANDLE ready; };

static DWORD WINAPI client_thread(void *arg)
{
    struct client *c = arg;
    HANDLE ctx;
    int i;

    if (!pJoin(&ctx, &group_guid, c->before)) { SetEvent(c->ready); return 1; }
    SetEvent(c->ready);
    for (i = 0; i < 6; i++)
    {
        LONG pos;
        if (!pWait(ctx)) break;
        pos = InterlockedIncrement(&order_pos) - 1;
        if (pos < 63) order[pos] = c->mark;
    }
    pLeave(ctx);
    return 0;
}

int main(void)
{
    HANDLE h, h2, threads[2];
    DWORD index = 0, index2 = 0;
    ULONG value = 0;
    LARGE_INTEGER period, timeout;
    struct client before = { TRUE, 'B' }, after = { FALSE, 'A' };
    HANDLE parent;
    int prio, i;

    avrt = LoadLibraryA("avrt.dll");
#define GET(p, n) p = (void *)GetProcAddress(avrt, n)
    GET(pSet, "AvSetMmThreadCharacteristicsW"); GET(pSetMax, "AvSetMmMaxThreadCharacteristicsW");
    GET(pRevert, "AvRevertMmThreadCharacteristics"); GET(pSetPrio, "AvSetMmThreadPriority");
    GET(pQuery, "AvQuerySystemResponsiveness"); GET(pCreate, "AvRtCreateThreadOrderingGroup");
    GET(pJoin, "AvRtJoinThreadOrderingGroup"); GET(pWait, "AvRtWaitOnThreadOrderingGroup");
    GET(pLeave, "AvRtLeaveThreadOrderingGroup"); GET(pDelete, "AvRtDeleteThreadOrderingGroup");
    if (!pCreate || !pJoin || !pWait || !pLeave || !pDelete)
    {
        printf("FAIL  the thread ordering group functions are not exported\nRESULT: FAIL\n");
        return 1;
    }

    SetLastError(0xdeadbeef);
    h = pSet(L"No Such Task", &index);
    check(!h && GetLastError() == ERROR_INVALID_TASK_NAME, "an unknown task: ERROR_INVALID_TASK_NAME");
    h = pSet(L"Pro Audio", &index);
    prio = GetThreadPriority(GetCurrentThread());
    printf("Pro Audio: handle %p index %lu priority %d\n", h, index, prio);
    check(h && index && prio == THREAD_PRIORITY_TIME_CRITICAL, "Pro Audio raises the thread to time critical");
    check(pQuery(h, &value) && value == 20, "AvQuerySystemResponsiveness: 20%");
    check(pSetPrio(h, AVRT_PRIORITY_LOW) && GetThreadPriority(GetCurrentThread()) < THREAD_PRIORITY_TIME_CRITICAL,
          "AvSetMmThreadPriority lowers it");
    check(pSetPrio(h, AVRT_PRIORITY_CRITICAL) && GetThreadPriority(GetCurrentThread()) == THREAD_PRIORITY_TIME_CRITICAL,
          "... and raises it");
    check(pRevert(h) && GetThreadPriority(GetCurrentThread()) == THREAD_PRIORITY_NORMAL,
          "AvRevertMmThreadCharacteristics puts the priority back");
    check(!pRevert(h), "... a reverted handle is refused");
    h = pSet(L"Audio", &index2);
    prio = GetThreadPriority(GetCurrentThread());
    check(h && prio > THREAD_PRIORITY_NORMAL && prio < THREAD_PRIORITY_TIME_CRITICAL, "Audio raises it less");
    pRevert(h);
    index2 = 0;
    h2 = pSetMax(L"Audio", L"Pro Audio", &index2);
    check(h2 && GetThreadPriority(GetCurrentThread()) == THREAD_PRIORITY_TIME_CRITICAL,
          "AvSetMmMaxThreadCharacteristics takes the more important task");
    pRevert(h2);

    /* an ordering group: B (before the parent), the parent, A (after) */
    period.QuadPart = 100000;   /* 10 ms */
    timeout.QuadPart = 10000000;
    memset(&group_guid, 0, sizeof(group_guid));
    check(pCreate(&parent, &period, &group_guid, &timeout), "AvRtCreateThreadOrderingGroup");
    before.ready = CreateEventA(NULL, FALSE, FALSE, NULL);
    after.ready = CreateEventA(NULL, FALSE, FALSE, NULL);
    threads[0] = CreateThread(NULL, 0, client_thread, &before, 0, NULL);
    WaitForSingleObject(before.ready, 5000);
    threads[1] = CreateThread(NULL, 0, client_thread, &after, 0, NULL);
    WaitForSingleObject(after.ready, 5000);
    for (i = 0; i < 6; i++)
    {
        LONG pos;
        if (!pWait(parent)) break;
        pos = InterlockedIncrement(&order_pos) - 1;
        if (pos < 63) order[pos] = 'P';
    }
    pWait(parent);   /* end the last turn */
    WaitForMultipleObjects(2, threads, TRUE, 10000);
    pDelete(parent);
    printf("order: %s\n", order);
    check(!strncmp(order, "BPABPABPABPA", 12), "the threads run in order, before, parent, after, cycle after cycle");
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
