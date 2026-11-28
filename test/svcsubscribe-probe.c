/* Service change subscriptions and a service process that ends while it starts
 * (patches/sg/9629, 9630; WireGuard's manager service, i0977).  The probe is its own
 * test service:
 *   probe.exe                      the tests
 *   probe.exe --service MODE       the service: MODE "die" reports START_PENDING and
 *       ends without stopping (as WireGuard's manager did on a missing export);
 *       "run" runs until it is told to stop
 * Nothing here allocates more than a few KB. */
#include <windows.h>
#include <winsvc.h>
#include <stdio.h>
#include <string.h>

#define SVC_DIE "sgsub_die"
#define SVC_RUN "sgsub_run"
#define SVC_NEW "sgsub_new"

#ifndef SERVICE_NOTIFY_CREATED
#define SERVICE_NOTIFY_CREATED 0x80
#define SERVICE_NOTIFY_DELETED 0x100
#endif

typedef VOID (CALLBACK *sub_cb)(DWORD, PVOID);
typedef DWORD (WINAPI *subscribe_fn)(SC_HANDLE, DWORD, sub_cb, PVOID, void **);
typedef VOID (WINAPI *unsubscribe_fn)(void *);

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

/* ---------------- the service ---------------- */
static SERVICE_STATUS_HANDLE status_handle;
static char mode[16];
static HANDLE stop_event;

static void report(DWORD state)
{
    SERVICE_STATUS st;
    memset(&st, 0, sizeof(st));
    st.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    st.dwCurrentState = state;
    st.dwControlsAccepted = state == SERVICE_RUNNING ? SERVICE_ACCEPT_STOP : 0;
    st.dwWaitHint = state == SERVICE_START_PENDING ? 3000 : 0;
    SetServiceStatus(status_handle, &st);
}

static DWORD WINAPI handler(DWORD ctl, DWORD type, void *data, void *ctx)
{
    if (ctl == SERVICE_CONTROL_STOP) { report(SERVICE_STOP_PENDING); SetEvent(stop_event); }
    return NO_ERROR;
}

static void WINAPI service_main(DWORD argc, char **argv)
{
    status_handle = RegisterServiceCtrlHandlerExA(argc ? argv[0] : "", handler, NULL);
    stop_event = CreateEventA(NULL, TRUE, FALSE, NULL);
    report(SERVICE_START_PENDING);
    if (!strcmp(mode, "die"))
    {
        Sleep(300);
        ExitProcess(1);                 /* ends while "starting" */
    }
    report(SERVICE_RUNNING);
    WaitForSingleObject(stop_event, 60000);
    report(SERVICE_STOPPED);
}

static int run_service(void)
{
    SERVICE_TABLE_ENTRYA table[] = { { (char *)"", service_main }, { NULL, NULL } };
    StartServiceCtrlDispatcherA(table);
    return 0;
}

/* ---------------- the tests ---------------- */
struct seen
{
    volatile LONG count;
    volatile LONG bits;
    HANDLE event;
};

static void CALLBACK on_change(DWORD notify, PVOID ctx)
{
    struct seen *s = ctx;
    InterlockedOr(&s->bits, notify);
    InterlockedIncrement(&s->count);
    SetEvent(s->event);
}

static BOOL wait_bits(struct seen *s, LONG bits, DWORD ms)
{
    DWORD end = GetTickCount() + ms;
    while ((s->bits & bits) != bits)
    {
        LONG left = (LONG)(end - GetTickCount());
        if (left <= 0) return FALSE;
        WaitForSingleObject(s->event, left);
    }
    return TRUE;
}

static SC_HANDLE make_service(SC_HANDLE scm, const char *name, const char *self, const char *svc_mode)
{
    char path[2 * MAX_PATH];
    SC_HANDLE svc;

    if ((svc = OpenServiceA(scm, name, SERVICE_ALL_ACCESS)))
    {
        DeleteService(svc);
        CloseServiceHandle(svc);
    }
    snprintf(path, sizeof(path), "\"%s\" --service %s", self, svc_mode);
    return CreateServiceA(scm, name, name, SERVICE_ALL_ACCESS, SERVICE_WIN32_OWN_PROCESS, SERVICE_DEMAND_START,
                          SERVICE_ERROR_NORMAL, path, NULL, NULL, NULL, NULL, NULL);
}

static DWORD state_of(SC_HANDLE svc, DWORD *exit_code)
{
    SERVICE_STATUS st;
    if (!QueryServiceStatus(svc, &st)) return 0;
    if (exit_code) *exit_code = st.dwWin32ExitCode;
    return st.dwCurrentState;
}

int main(int argc, char **argv)
{
    HMODULE sechost = LoadLibraryA("sechost.dll");
    subscribe_fn subscribe = (subscribe_fn)GetProcAddress(sechost, "SubscribeServiceChangeNotifications");
    unsubscribe_fn unsubscribe = (unsubscribe_fn)GetProcAddress(sechost, "UnsubscribeServiceChangeNotifications");
    char self[MAX_PATH], line[160];
    struct seen db = { 0 }, st = { 0 }, after = { 0 };
    void *db_sub = NULL, *st_sub = NULL, *after_sub = NULL;
    SC_HANDLE scm, die, run, created;
    DWORD err, state, code, i;

    if (argc >= 3 && !strcmp(argv[1], "--service"))
    {
        lstrcpynA(mode, argv[2], sizeof(mode));
        return run_service();
    }
    GetModuleFileNameA(NULL, self, sizeof(self));
    if (!(scm = OpenSCManagerA(NULL, NULL, SC_MANAGER_ALL_ACCESS)))
    {
        printf("FAIL  OpenSCManager %lu\nRESULT: FAIL\n", GetLastError());
        return 1;
    }

    /* 1. a service whose process ends while it is starting is stopped (1067), as on Windows */
    die = make_service(scm, SVC_DIE, self, "die");
    check(die != NULL, "the test service is created");
    if (die)
    {
        BOOL started = StartServiceA(die, 0, NULL);
        err = started ? 0 : GetLastError();
        snprintf(line, sizeof(line), "StartService of a service that reports START_PENDING and ends returns (%lu)", err);
        check(1, line);
        state = 0;
        for (i = 0; i < 100; i++)
        {
            if ((state = state_of(die, &code)) == SERVICE_STOPPED) break;
            Sleep(100);
        }
        snprintf(line, sizeof(line), "its process gone, the service is stopped (state %lu, exit code %lu, want 1 and 1067)",
                 state, code);
        check(state == SERVICE_STOPPED && code == ERROR_PROCESS_ABORTED, line);
        /* and it can be started again (it was "already running" for ever) */
        started = StartServiceA(die, 0, NULL);
        err = started ? 0 : GetLastError();
        snprintf(line, sizeof(line), "it can be started again (error %lu, not 1056)", err);
        check(err != ERROR_SERVICE_ALREADY_RUNNING, line);
        for (i = 0; i < 100 && state_of(die, NULL) != SERVICE_STOPPED; i++) Sleep(100);
    }

    /* 2. the exports exist (they were stubs: calling one ended the process) */
    check(subscribe && unsubscribe, "sechost exports SubscribeServiceChangeNotifications and Unsubscribe...");
    if (!subscribe || !unsubscribe) goto done;

    /* 3. the database: a service created and one deleted are reported on a manager handle */
    db.event = CreateEventA(NULL, FALSE, FALSE, NULL);
    err = subscribe(scm, 0 /* SC_EVENT_DATABASE_CHANGE */, on_change, &db, &db_sub);
    snprintf(line, sizeof(line), "a manager handle subscribes to database changes (error %lu)", err);
    check(!err && db_sub, line);
    if (!err)
    {
        Sleep(200);
        check(db.count == 0, "nothing is reported before anything changes");
        created = make_service(scm, SVC_NEW, self, "run");
        check(created != NULL, "a service is created");
        check(wait_bits(&db, SERVICE_NOTIFY_CREATED, 5000), "the subscription is told a service was created");
        if (created)
        {
            DeleteService(created);
            CloseServiceHandle(created);
        }
        check(wait_bits(&db, SERVICE_NOTIFY_DELETED, 5000), "...and that one was deleted");
    }

    /* 4. a service's status: start and stop are reported */
    run = make_service(scm, SVC_RUN, self, "run");
    check(run != NULL, "a running test service is created");
    if (run)
    {
        st.event = CreateEventA(NULL, FALSE, FALSE, NULL);
        err = subscribe(run, 2 /* SC_EVENT_STATUS_CHANGE */, on_change, &st, &st_sub);
        snprintf(line, sizeof(line), "a service handle subscribes to status changes (error %lu)", err);
        check(!err && st_sub, line);
        st.bits = 0;
        StartServiceA(run, 0, NULL);
        check(wait_bits(&st, SERVICE_NOTIFY_RUNNING, 10000), "its start is reported (SERVICE_NOTIFY_RUNNING)");
        st.bits = 0;
        {
            SERVICE_STATUS ss;
            ControlService(run, SERVICE_CONTROL_STOP, &ss);
        }
        check(wait_bits(&st, SERVICE_NOTIFY_STOPPED, 10000), "its stop is reported (SERVICE_NOTIFY_STOPPED)");

        /* 5. after Unsubscribe the callback is not called again */
        unsubscribe(st_sub);
        Sleep(200);
        i = st.count;
        /* a second subscription on the same handle, before anything changes: the first's
         * registration must not hold the handle (it is told of the start, as the first would be) */
        after.event = CreateEventA(NULL, FALSE, FALSE, NULL);
        err = subscribe(run, 2, on_change, &after, &after_sub);
        check(!err, "a second subscription is made on the same handle");
        Sleep(500);
        after.bits = 0;
        StartServiceA(run, 0, NULL);
        check(wait_bits(&after, SERVICE_NOTIFY_RUNNING, 10000), "...and is told of the start (the handle is not taken by the first)");
        Sleep(500);
        snprintf(line, sizeof(line), "an unsubscribed callback is not called again (%ld calls, was %lu)", st.count, i);
        check(st.count == (LONG)i, line);
        {
            SERVICE_STATUS ss;
            ControlService(run, SERVICE_CONTROL_STOP, &ss);
        }
        for (i = 0; i < 100 && state_of(run, NULL) != SERVICE_STOPPED; i++) Sleep(100);
        if (after_sub) unsubscribe(after_sub);
        DeleteService(run);
        CloseServiceHandle(run);
    }
    if (db_sub) unsubscribe(db_sub);

done:
    if (die)
    {
        DeleteService(die);
        CloseServiceHandle(die);
    }
    CloseServiceHandle(scm);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    fflush(stdout);
    ExitProcess(failures ? 1 : 0);
}
