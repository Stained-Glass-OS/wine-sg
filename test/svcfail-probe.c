/* Service failure actions and the other ChangeServiceConfig2 levels
 * (patches/sg/2222).  The probe is its own test service:
 *   probe.exe                       the tests
 *   probe.exe --service COUNTER     the service (ServiceMain): appends a line to
 *       COUNTER per start; control 128 crashes it, 129 stops it reporting an error,
 *       STOP stops it normally
 *   probe.exe --touch FILE TEXT     writes TEXT to FILE (the "run command" action)
 * The service is started by services.exe, which restarts it as its failure
 * actions say.  Nothing here allocates more than a few KB. */
#include <windows.h>
#include <winsvc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SVC "sgfail_probe"
#ifndef SERVICE_CONFIG_LAUNCH_PROTECTED
#define SERVICE_CONFIG_LAUNCH_PROTECTED 12
#endif

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

/* ---------------- the service ---------------- */
static SERVICE_STATUS_HANDLE status_handle;
static char counter_path[MAX_PATH], svc_name[64], svc_mode[16];
static HANDLE stop_event;

static void mark(const char *text)
{
    HANDLE f = CreateFileA(counter_path, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_ALWAYS, 0, NULL);
    DWORD w;
    if (f != INVALID_HANDLE_VALUE) { WriteFile(f, text, strlen(text), &w, NULL); CloseHandle(f); }
}

static void report(DWORD state, DWORD exit_code)
{
    DWORD accept = SERVICE_ACCEPT_STOP;
    SERVICE_STATUS st;
    if (!strcmp(svc_mode, "pre")) accept |= SERVICE_ACCEPT_PRESHUTDOWN;
    if (!strcmp(svc_mode, "shut")) accept |= SERVICE_ACCEPT_SHUTDOWN;
    st.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    st.dwCurrentState = state;
    st.dwControlsAccepted = state == SERVICE_RUNNING ? accept : 0;
    st.dwWin32ExitCode = exit_code;
    st.dwServiceSpecificExitCode = 0;
    st.dwCheckPoint = 0;
    st.dwWaitHint = 0;
    SetServiceStatus(status_handle, &st);
}

static DWORD WINAPI handler(DWORD ctl, DWORD type, void *data, void *ctx)
{
    if (ctl == SERVICE_CONTROL_STOP) { report(SERVICE_STOPPED, NO_ERROR); SetEvent(stop_event); return NO_ERROR; }
    if (ctl == SERVICE_CONTROL_PRESHUTDOWN)
    {
        mark("pre-begin\n");
        Sleep(1500);       /* a service that needs its preshutdown time */
        mark("pre-done\n");
        report(SERVICE_STOPPED, NO_ERROR);
        SetEvent(stop_event);
        return NO_ERROR;
    }
    if (ctl == SERVICE_CONTROL_SHUTDOWN) { mark("shut-called\n"); report(SERVICE_STOPPED, NO_ERROR); SetEvent(stop_event); return NO_ERROR; }
    if (ctl == 128) ExitProcess(2);                           /* a crash */
    if (ctl == 129) { report(SERVICE_STOPPED, 5); SetEvent(stop_event); return NO_ERROR; }   /* an error exit */
    return NO_ERROR;
}

static void WINAPI service_main(DWORD argc, char **argv)
{
    status_handle = RegisterServiceCtrlHandlerExA(svc_name, handler, NULL);
    mark("start\n");
    stop_event = CreateEventA(NULL, TRUE, FALSE, NULL);
    report(SERVICE_RUNNING, NO_ERROR);
    WaitForSingleObject(stop_event, INFINITE);
}

/* ---------------- the tests ---------------- */
static char self[MAX_PATH], counter[MAX_PATH], cmdfile[MAX_PATH];
static SC_HANDLE scm, svc;

static int count_starts(void)
{
    char buf[512];
    DWORD got = 0;
    HANDLE f = CreateFileA(counter, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    int n = 0, i;

    if (f == INVALID_HANDLE_VALUE) return 0;
    ReadFile(f, buf, sizeof(buf), &got, NULL);
    CloseHandle(f);
    for (i = 0; i < (int)got; i++) if (buf[i] == '\n') n++;
    return n;
}

static DWORD state(void)
{
    SERVICE_STATUS_PROCESS s;
    DWORD need;
    if (!QueryServiceStatusEx(svc, SC_STATUS_PROCESS_INFO, (BYTE *)&s, sizeof(s), &need)) return 0;
    return s.dwCurrentState;
}

static int wait_state(DWORD want, int ms)
{
    int t;
    for (t = 0; t < ms; t += 50) { if (state() == want) return 1; Sleep(50); }
    return state() == want;
}

static int wait_starts(int n, int ms)
{
    int t;
    for (t = 0; t < ms; t += 50) { if (count_starts() >= n) return 1; Sleep(50); }
    return count_starts() >= n;
}

static void stop_service(void)
{
    SERVICE_STATUS st;
    if (state() != SERVICE_STOPPED) ControlService(svc, SERVICE_CONTROL_STOP, &st);
    wait_state(SERVICE_STOPPED, 5000);
}

static void set_actions(DWORD reset, const SC_ACTION *acts, DWORD n, const char *cmd, const char *msg)
{
    SERVICE_FAILURE_ACTIONSA fa = { reset, (char *)msg, (char *)cmd, n, (SC_ACTION *)acts };
    BOOL ret = ChangeServiceConfig2A(svc, SERVICE_CONFIG_FAILURE_ACTIONS, &fa);
    if (!ret) printf("      ChangeServiceConfig2A(failure actions) error %lu\n", (unsigned long)GetLastError());
}

static int start_fresh(void)
{
    stop_service();
    DeleteFileA(counter);
    if (!StartServiceA(svc, 0, NULL)) { printf("      StartService error %lu\n", (unsigned long)GetLastError()); return 0; }
    return wait_state(SERVICE_RUNNING, 8000) && wait_starts(1, 2000);
}

static void crash(void)
{
    SERVICE_STATUS st;
    ControlService(svc, 128, &st);
}

static int read_cmdfile(char *out, int max)
{
    DWORD got = 0;
    HANDLE f = CreateFileA(cmdfile, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    if (f == INVALID_HANDLE_VALUE) return 0;
    ReadFile(f, out, max - 1, &got, NULL);
    CloseHandle(f);
    out[got] = 0;
    return got;
}

static void test_all(void)
{
    char path[MAX_PATH * 2], buf[1024];
    BYTE blob[1024];
    DWORD need, i;
    SC_ACTION a3[3], a1[1];
    SERVICE_FAILURE_ACTIONSA *fa = (SERVICE_FAILURE_ACTIONSA *)blob;
    SERVICE_FAILURE_ACTIONSW *faw = (SERVICE_FAILURE_ACTIONSW *)blob;
    BOOL ret;

    scm = OpenSCManagerA(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (!scm) { printf("FAIL  OpenSCManager %lu\n", (unsigned long)GetLastError()); exit(1); }
    svc = OpenServiceA(scm, SVC, SERVICE_ALL_ACCESS);
    if (svc) { stop_service(); DeleteService(svc); CloseServiceHandle(svc); }
    snprintf(path, sizeof(path), "\"%s\" --service \"%s\" " SVC " std", self, counter);
    svc = CreateServiceA(scm, SVC, "SG failure probe", SERVICE_ALL_ACCESS, SERVICE_WIN32_OWN_PROCESS, SERVICE_DEMAND_START,
                         SERVICE_ERROR_IGNORE, path, NULL, NULL, NULL, NULL, NULL);
    if (!svc) { printf("FAIL  CreateService %lu\n", (unsigned long)GetLastError()); exit(1); }

    /* --- defaults of a fresh service --- */
    memset(blob, 0xcc, sizeof(blob));
    ret = QueryServiceConfig2A(svc, SERVICE_CONFIG_FAILURE_ACTIONS, blob, sizeof(blob), &need);
    check(ret && fa->dwResetPeriod == 0 && fa->cActions == 0 && !fa->lpsaActions && !fa->lpRebootMsg && !fa->lpCommand,
          "a fresh service has no failure actions");
    check(need == sizeof(SERVICE_FAILURE_ACTIONSA) || need == ((sizeof(SERVICE_FAILURE_ACTIONSA) + 3) & ~3), "...and the answer is the bare structure");
    SetLastError(0);
    ret = QueryServiceConfig2A(svc, SERVICE_CONFIG_FAILURE_ACTIONS, blob, 4, &need);
    check(!ret && GetLastError() == ERROR_INSUFFICIENT_BUFFER && need >= sizeof(SERVICE_FAILURE_ACTIONSA), "a buffer that is too small is ERROR_INSUFFICIENT_BUFFER with the size needed");
    memset(blob, 0xcc, sizeof(blob));
    ret = QueryServiceConfig2A(svc, SERVICE_CONFIG_FAILURE_ACTIONS_FLAG, blob, sizeof(blob), &need);
    check(ret && need == sizeof(DWORD) && *(DWORD *)blob == 0, "the non-crash flag is FALSE");
    ret = QueryServiceConfig2A(svc, SERVICE_CONFIG_SERVICE_SID_INFO, blob, sizeof(blob), &need);
    check(ret && *(DWORD *)blob == 0, "the service SID type is none (0)");
    ret = QueryServiceConfig2A(svc, SERVICE_CONFIG_LAUNCH_PROTECTED, blob, sizeof(blob), &need);
    check(ret && *(DWORD *)blob == 0, "the launch protection is none (0)");
    ret = QueryServiceConfig2A(svc, SERVICE_CONFIG_REQUIRED_PRIVILEGES_INFO, blob, sizeof(blob), &need);
    check(ret && ((SERVICE_REQUIRED_PRIVILEGES_INFOA *)blob)->pmszRequiredPrivileges == NULL, "there are no required privileges");

    /* --- storing and reading back --- */
    a3[0].Type = SC_ACTION_RESTART; a3[0].Delay = 100;
    a3[1].Type = SC_ACTION_RUN_COMMAND; a3[1].Delay = 200;
    a3[2].Type = SC_ACTION_NONE; a3[2].Delay = 300;
    set_actions(86400, a3, 3, "cmd.exe /c exit 0", "going down");
    memset(blob, 0xcc, sizeof(blob));
    ret = QueryServiceConfig2A(svc, SERVICE_CONFIG_FAILURE_ACTIONS, blob, sizeof(blob), &need);
    check(ret && fa->dwResetPeriod == 86400 && fa->cActions == 3 && fa->lpsaActions &&
          fa->lpsaActions[0].Type == SC_ACTION_RESTART && fa->lpsaActions[0].Delay == 100 &&
          fa->lpsaActions[1].Type == SC_ACTION_RUN_COMMAND && fa->lpsaActions[1].Delay == 200 &&
          fa->lpsaActions[2].Type == SC_ACTION_NONE && fa->lpsaActions[2].Delay == 300,
          "failure actions are read back (A)");
    check(ret && fa->lpCommand && !strcmp(fa->lpCommand, "cmd.exe /c exit 0") && fa->lpRebootMsg && !strcmp(fa->lpRebootMsg, "going down"),
          "...with the command and the reboot message");
    check(need >= sizeof(*fa) + 3 * sizeof(SC_ACTION) + 18 + 11, "...and the size covers the actions and both strings");
    memset(blob, 0xcc, sizeof(blob));
    ret = QueryServiceConfig2W(svc, SERVICE_CONFIG_FAILURE_ACTIONS, blob, sizeof(blob), &need);
    check(ret && faw->dwResetPeriod == 86400 && faw->cActions == 3 && faw->lpCommand && !wcscmp(faw->lpCommand, L"cmd.exe /c exit 0") &&
          faw->lpRebootMsg && !wcscmp(faw->lpRebootMsg, L"going down"), "...and in wide characters");
    /* NULL strings and NULL actions leave things alone; "" removes a string */
    {
        SERVICE_FAILURE_ACTIONSA keep = { 0, NULL, NULL, 0, NULL };
        ChangeServiceConfig2A(svc, SERVICE_CONFIG_FAILURE_ACTIONS, &keep);
        memset(blob, 0xcc, sizeof(blob));
        QueryServiceConfig2A(svc, SERVICE_CONFIG_FAILURE_ACTIONS, blob, sizeof(blob), &need);
        check(fa->dwResetPeriod == 86400 && fa->cActions == 3 && fa->lpCommand && fa->lpRebootMsg, "NULL actions and NULL strings change nothing");
        keep.lpCommand = (char *)"";
        ChangeServiceConfig2A(svc, SERVICE_CONFIG_FAILURE_ACTIONS, &keep);
        memset(blob, 0xcc, sizeof(blob));
        QueryServiceConfig2A(svc, SERVICE_CONFIG_FAILURE_ACTIONS, blob, sizeof(blob), &need);
        check(fa->cActions == 3 && !fa->lpCommand && fa->lpRebootMsg, "an empty command removes only the command");
        keep.lpCommand = NULL;
        keep.lpsaActions = a3;
        keep.cActions = 0;
        ChangeServiceConfig2A(svc, SERVICE_CONFIG_FAILURE_ACTIONS, &keep);
        memset(blob, 0xcc, sizeof(blob));
        QueryServiceConfig2A(svc, SERVICE_CONFIG_FAILURE_ACTIONS, blob, sizeof(blob), &need);
        check(fa->cActions == 0 && fa->dwResetPeriod == 0 && !fa->lpsaActions && fa->lpRebootMsg, "an empty array removes the actions and the reset period");
    }
    {
        SERVICE_FAILURE_ACTIONSA clr = { 0, (char *)"", (char *)"", 0, a3 };
        ChangeServiceConfig2A(svc, SERVICE_CONFIG_FAILURE_ACTIONS, &clr);
        memset(blob, 0xcc, sizeof(blob));
        ret = QueryServiceConfig2A(svc, SERVICE_CONFIG_FAILURE_ACTIONS, blob, sizeof(blob), &need);
        check(ret && !fa->lpRebootMsg && !fa->lpCommand && !fa->cActions, "everything removed leaves the bare structure");
    }

    /* --- the other levels --- */
    {
        SERVICE_FAILURE_ACTIONS_FLAG flag = { TRUE };
        SERVICE_SID_INFO sid = { 1 };
        struct { DWORD dwLaunchProtected; } lp = { 2 };
        SERVICE_REQUIRED_PRIVILEGES_INFOA priv = { (char *)"SeChangeNotifyPrivilege\0SeBackupPrivilege\0" };
        SERVICE_REQUIRED_PRIVILEGES_INFOW privw = { (WCHAR *)L"SeShutdownPrivilege\0" };

        check(ChangeServiceConfig2A(svc, SERVICE_CONFIG_FAILURE_ACTIONS_FLAG, &flag), "set the non-crash flag");
        QueryServiceConfig2A(svc, SERVICE_CONFIG_FAILURE_ACTIONS_FLAG, blob, sizeof(blob), &need);
        check(*(DWORD *)blob == 1, "...and read it back");
        flag.fFailureActionsOnNonCrashFailures = 7;
        ChangeServiceConfig2A(svc, SERVICE_CONFIG_FAILURE_ACTIONS_FLAG, &flag);
        QueryServiceConfig2A(svc, SERVICE_CONFIG_FAILURE_ACTIONS_FLAG, blob, sizeof(blob), &need);
        check(*(DWORD *)blob == 1, "...any non-zero value is TRUE");
        flag.fFailureActionsOnNonCrashFailures = 0;
        ChangeServiceConfig2A(svc, SERVICE_CONFIG_FAILURE_ACTIONS_FLAG, &flag);

        check(ChangeServiceConfig2A(svc, SERVICE_CONFIG_SERVICE_SID_INFO, &sid), "set the service SID type to unrestricted (1)");
        QueryServiceConfig2A(svc, SERVICE_CONFIG_SERVICE_SID_INFO, blob, sizeof(blob), &need);
        check(*(DWORD *)blob == 1, "...and read it back");
        sid.dwServiceSidType = 3;
        check(ChangeServiceConfig2A(svc, SERVICE_CONFIG_SERVICE_SID_INFO, &sid), "restricted (3) is accepted");
        sid.dwServiceSidType = 2;
        SetLastError(0);
        check(!ChangeServiceConfig2A(svc, SERVICE_CONFIG_SERVICE_SID_INFO, &sid) && GetLastError() == ERROR_INVALID_PARAMETER, "2 is not a service SID type");

        check(ChangeServiceConfig2A(svc, SERVICE_CONFIG_LAUNCH_PROTECTED, &lp), "set the launch protection");
        QueryServiceConfig2A(svc, SERVICE_CONFIG_LAUNCH_PROTECTED, blob, sizeof(blob), &need);
        check(*(DWORD *)blob == 2, "...and read it back");
        lp.dwLaunchProtected = 9;
        SetLastError(0);
        check(!ChangeServiceConfig2A(svc, SERVICE_CONFIG_LAUNCH_PROTECTED, &lp) && GetLastError() == ERROR_INVALID_PARAMETER, "9 is not a launch protection");
        lp.dwLaunchProtected = 0;
        ChangeServiceConfig2A(svc, SERVICE_CONFIG_LAUNCH_PROTECTED, &lp);

        check(ChangeServiceConfig2A(svc, SERVICE_CONFIG_REQUIRED_PRIVILEGES_INFO, &priv), "set the required privileges (A)");
        memset(blob, 0xcc, sizeof(blob));
        ret = QueryServiceConfig2A(svc, SERVICE_CONFIG_REQUIRED_PRIVILEGES_INFO, blob, sizeof(blob), &need);
        {
            const char *p = ((SERVICE_REQUIRED_PRIVILEGES_INFOA *)blob)->pmszRequiredPrivileges;
            check(ret && p && !strcmp(p, "SeChangeNotifyPrivilege") && !strcmp(p + 24, "SeBackupPrivilege") && !p[24 + 18],
                  "...and read them back as a multi-string");
        }
        check(ChangeServiceConfig2W(svc, SERVICE_CONFIG_REQUIRED_PRIVILEGES_INFO, &privw), "set them again (W)");
        memset(blob, 0xcc, sizeof(blob));
        ret = QueryServiceConfig2W(svc, SERVICE_CONFIG_REQUIRED_PRIVILEGES_INFO, blob, sizeof(blob), &need);
        check(ret && ((SERVICE_REQUIRED_PRIVILEGES_INFOW *)blob)->pmszRequiredPrivileges &&
              !wcscmp(((SERVICE_REQUIRED_PRIVILEGES_INFOW *)blob)->pmszRequiredPrivileges, L"SeShutdownPrivilege"), "...replacing the old ones");
        privw.pmszRequiredPrivileges = (WCHAR *)L"\0";
        ChangeServiceConfig2W(svc, SERVICE_CONFIG_REQUIRED_PRIVILEGES_INFO, &privw);
        ret = QueryServiceConfig2W(svc, SERVICE_CONFIG_REQUIRED_PRIVILEGES_INFO, blob, sizeof(blob), &need);
        check(ret && ((SERVICE_REQUIRED_PRIVILEGES_INFOW *)blob)->pmszRequiredPrivileges == NULL, "an empty list removes them");
    }

    /* --- failure actions at work --- */
    /* A: restart twice, then nothing */
    a3[0].Type = SC_ACTION_RESTART; a3[0].Delay = 100;
    a3[1].Type = SC_ACTION_RESTART; a3[1].Delay = 100;
    a3[2].Type = SC_ACTION_NONE; a3[2].Delay = 0;
    set_actions(3600, a3, 3, NULL, NULL);
    check(start_fresh(), "the test service starts");
    crash();
    check(wait_starts(2, 8000) && wait_state(SERVICE_RUNNING, 3000), "a crash restarts the service (first action)");
    crash();
    check(wait_starts(3, 8000) && wait_state(SERVICE_RUNNING, 3000), "...and again (second action)");
    crash();
    check(wait_state(SERVICE_STOPPED, 3000), "the service is stopped after the third crash");
    Sleep(1500);
    check(state() == SERVICE_STOPPED && count_starts() == 3, "...and the third action (none) leaves it stopped");

    /* B: the reset period */
    a1[0].Type = SC_ACTION_RESTART; a1[0].Delay = 100;
    a3[0].Type = SC_ACTION_RESTART; a3[0].Delay = 50;
    a3[1].Type = SC_ACTION_NONE; a3[1].Delay = 0;
    set_actions(1, a3, 2, NULL, NULL);
    check(start_fresh(), "(restart for the reset-period test)");
    crash();
    check(wait_starts(2, 8000) && wait_state(SERVICE_RUNNING, 3000), "first crash restarts");
    Sleep(2300);   /* longer than the reset period of 1 second */
    crash();
    check(wait_starts(3, 8000) && wait_state(SERVICE_RUNNING, 3000), "after the reset period the failure count starts again (first action again)");
    crash();
    Sleep(1500);
    check(state() == SERVICE_STOPPED && count_starts() == 3, "two failures within the reset period reach the second action (none)");

    /* C: errors that are not crashes */
    set_actions(3600, a1, 1, NULL, NULL);
    check(start_fresh(), "(start for the non-crash test)");
    {
        SERVICE_STATUS st;
        ControlService(svc, 129, &st);
    }
    check(wait_state(SERVICE_STOPPED, 3000), "the service stops reporting an error");
    Sleep(1500);
    check(state() == SERVICE_STOPPED && count_starts() == 1, "...without the flag that is no failure");
    {
        SERVICE_FAILURE_ACTIONS_FLAG flag = { TRUE };
        SERVICE_STATUS st;
        ChangeServiceConfig2A(svc, SERVICE_CONFIG_FAILURE_ACTIONS_FLAG, &flag);
        check(start_fresh(), "(start again with the flag)");
        ControlService(svc, 129, &st);
        check(wait_starts(2, 8000) && wait_state(SERVICE_RUNNING, 3000), "...with the flag it restarts");
        flag.fFailureActionsOnNonCrashFailures = FALSE;
        ChangeServiceConfig2A(svc, SERVICE_CONFIG_FAILURE_ACTIONS_FLAG, &flag);
    }

    /* D: a normal stop is no failure */
    check(start_fresh(), "(start for the stop test)");
    stop_service();
    Sleep(1000);
    check(state() == SERVICE_STOPPED && count_starts() == 1, "a normal stop runs no failure action");

    /* E: a command, with the failure count in it */
    {
        char cmd[MAX_PATH * 2];
        DeleteFileA(cmdfile);
        snprintf(cmd, sizeof(cmd), "\"%s\" --touch \"%s\" count%%1%%", self, cmdfile);
        a1[0].Type = SC_ACTION_RUN_COMMAND; a1[0].Delay = 0;
        set_actions(1, a1, 1, cmd, NULL);   /* a short reset period: earlier failures are forgotten */
        Sleep(1300);
        check(start_fresh(), "(start for the command test)");
        crash();
        for (i = 0; i < 100 && !read_cmdfile(buf, sizeof(buf)); i++) Sleep(100);
        check(read_cmdfile(buf, sizeof(buf)) && !strcmp(buf, "count1"), "the command runs, %1% is the failure count (1)");
        if (strcmp(buf, "count1")) printf("      command file has \"%s\"\n", buf);
        check(wait_state(SERVICE_STOPPED, 2000) && count_starts() == 1, "...and the service is not restarted by it");
        DeleteFileA(cmdfile);
        set_actions(3600, a1, 1, cmd, NULL);
        check(start_fresh(), "(start again)");
        crash();
        for (i = 0; i < 100 && !read_cmdfile(buf, sizeof(buf)); i++) Sleep(100);
        check(read_cmdfile(buf, sizeof(buf)) && !strcmp(buf, "count2"), "the last action repeats, the count is 2");
    }

    stop_service();
    DeleteService(svc);
    CloseServiceHandle(svc);
    CloseServiceHandle(scm);
    DeleteFileA(counter);
    DeleteFileA(cmdfile);
}

int main(int argc, char **argv)
{
    char *slash;

    if (argc >= 5 && !strcmp(argv[1], "--service"))
    {
        SERVICE_TABLE_ENTRYA table[] = { { argv[3], service_main }, { NULL, NULL } };
        lstrcpynA(counter_path, argv[2], MAX_PATH);
        lstrcpynA(svc_name, argv[3], sizeof(svc_name));
        lstrcpynA(svc_mode, argv[4], sizeof(svc_mode));
        return StartServiceCtrlDispatcherA(table) ? 0 : 1;
    }
    if (argc >= 2 && !strcmp(argv[1], "--shutdown-setup"))
    {
        /* two services that want the shutdown controls, left running; the gate then
         * shuts the prefix down with wineboot and looks at the marks they wrote */
        static const char *names[2] = { "sgfail_pre", "sgfail_shut" }, *modes[2] = { "pre", "shut" };
        SC_HANDLE m = OpenSCManagerA(NULL, NULL, SC_MANAGER_ALL_ACCESS);
        int i, ok = 1;

        GetModuleFileNameA(NULL, self, MAX_PATH);
        for (i = 0; i < 2; i++)
        {
            char path[MAX_PATH * 3], marks[MAX_PATH];
            SERVICE_PRESHUTDOWN_INFO pre = { 6000 };
            SC_HANDLE sv = OpenServiceA(m, names[i], SERVICE_ALL_ACCESS);
            int t;

            lstrcpyA(marks, self);
            if ((slash = strrchr(marks, '\\'))) *slash = 0;
            lstrcatA(marks, i ? "\\sgfail_shut.marks" : "\\sgfail_pre.marks");
            DeleteFileA(marks);
            if (sv) { DeleteService(sv); CloseServiceHandle(sv); }
            snprintf(path, sizeof(path), "\"%s\" --service \"%s\" %s %s", self, marks, names[i], modes[i]);
            sv = CreateServiceA(m, names[i], names[i], SERVICE_ALL_ACCESS, SERVICE_WIN32_OWN_PROCESS, SERVICE_DEMAND_START,
                                SERVICE_ERROR_IGNORE, path, NULL, NULL, NULL, NULL, NULL);
            if (!sv) { printf("FAIL  CreateService %s %lu\n", names[i], (unsigned long)GetLastError()); return 1; }
            if (i == 0) ChangeServiceConfig2A(sv, SERVICE_CONFIG_PRESHUTDOWN_INFO, &pre);
            if (!StartServiceA(sv, 0, NULL)) { printf("FAIL  StartService %lu\n", (unsigned long)GetLastError()); return 1; }
            for (t = 0; t < 100; t++)
            {
                SERVICE_STATUS_PROCESS sp; DWORD need;
                if (QueryServiceStatusEx(sv, SC_STATUS_PROCESS_INFO, (BYTE *)&sp, sizeof(sp), &need) && sp.dwCurrentState == SERVICE_RUNNING) break;
                Sleep(100);
            }
            ok &= t < 100;
            CloseServiceHandle(sv);
        }
        printf("%s  the shutdown test services are running\n", ok ? "PASS" : "FAIL");
        return ok ? 0 : 1;
    }
    if (argc >= 4 && !strcmp(argv[1], "--touch"))
    {
        HANDLE f = CreateFileA(argv[2], GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, 0, NULL);
        DWORD w;
        if (f != INVALID_HANDLE_VALUE) { WriteFile(f, argv[3], strlen(argv[3]), &w, NULL); CloseHandle(f); }
        return 0;
    }

    GetModuleFileNameA(NULL, self, MAX_PATH);
    lstrcpyA(counter, self);
    if ((slash = strrchr(counter, '\\'))) *slash = 0;
    lstrcpyA(cmdfile, counter);
    lstrcatA(cmdfile, "\\sgfail_cmd.txt");
    lstrcatA(counter, "\\sgfail_counter.txt");
    test_all();
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures ? 1 : 0;
}
