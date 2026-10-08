/* Job object limits (patches/sg/1626).
 *
 *  - the limits a job is given read back (they read as zeros, even the
 *    limit flags);
 *  - JobObjectBasicUIRestrictions is kept and read back, bad bits refused;
 *  - the active process limit holds: a process cannot be assigned past it,
 *    and a process in the job cannot start another (ERROR_NOT_ENOUGH_QUOTA,
 *    JOB_OBJECT_MSG_ACTIVE_PROCESS_LIMIT on the completion port);
 *  - a priority class limit applies to the job's processes.
 *
 *   joblimits-probe.exe            the tests
 *   joblimits-probe.exe spawn      tries to start a process; exit code is
 *                                  the error (0 when it could)
 */
#include <windows.h>
#include <stdio.h>
#include <string.h>

static int failures;

static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

static BOOL start(const char *mode, DWORD flags, PROCESS_INFORMATION *pi)
{
    STARTUPINFOA si = { sizeof(si) };
    char exe[MAX_PATH], line[MAX_PATH + 32];

    GetModuleFileNameA(NULL, exe, MAX_PATH);
    sprintf(line, "\"%s\" %s", exe, mode);
    return CreateProcessA(exe, line, NULL, NULL, FALSE, flags, NULL, NULL, &si, pi);
}

int main(int argc, char **argv)
{
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION ext, got;
    JOBOBJECT_BASIC_LIMIT_INFORMATION basic;
    JOBOBJECT_BASIC_UI_RESTRICTIONS ui;
    JOBOBJECT_ASSOCIATE_COMPLETION_PORT port;
    PROCESS_INFORMATION a, b;
    HANDLE job, iocp;
    DWORD code, msg, len;
    ULONG_PTR key;
    OVERLAPPED *ovl;
    BOOL ok, limit_msg = FALSE;

    if (argc >= 2 && !strcmp(argv[1], "spawn"))
    {
        PROCESS_INFORMATION pi;
        if (!start("sleep", 0, &pi)) return GetLastError();
        TerminateProcess(pi.hProcess, 0);
        return 0;
    }
    if (argc >= 2 && !strcmp(argv[1], "sleep"))
    {
        Sleep(30000);
        return 0;
    }

    job = CreateJobObjectA(NULL, NULL);
    memset(&ext, 0, sizeof(ext));
    ext.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_ACTIVE_PROCESS | JOB_OBJECT_LIMIT_PRIORITY_CLASS |
            JOB_OBJECT_LIMIT_PROCESS_MEMORY | JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    ext.BasicLimitInformation.ActiveProcessLimit = 1;
    ext.BasicLimitInformation.PriorityClass = BELOW_NORMAL_PRIORITY_CLASS;
    ext.ProcessMemoryLimit = 256 << 20;
    ok = SetInformationJobObject(job, JobObjectExtendedLimitInformation, &ext, sizeof(ext));
    check(ok, "SetInformationJobObject(JobObjectExtendedLimitInformation)");

    memset(&got, 0xcc, sizeof(got));
    ok = QueryInformationJobObject(job, JobObjectExtendedLimitInformation, &got, sizeof(got), &len);
    printf("limits: flags %#lx active %lu priority %#lx memory %Iu\n", got.BasicLimitInformation.LimitFlags,
           got.BasicLimitInformation.ActiveProcessLimit, got.BasicLimitInformation.PriorityClass, got.ProcessMemoryLimit);
    check(ok && got.BasicLimitInformation.LimitFlags == ext.BasicLimitInformation.LimitFlags &&
          got.BasicLimitInformation.ActiveProcessLimit == 1 &&
          got.BasicLimitInformation.PriorityClass == BELOW_NORMAL_PRIORITY_CLASS &&
          got.ProcessMemoryLimit == (256 << 20), "the extended limits read back");
    memset(&basic, 0xcc, sizeof(basic));
    ok = QueryInformationJobObject(job, JobObjectBasicLimitInformation, &basic, sizeof(basic), &len);
    check(ok && basic.ActiveProcessLimit == 1 &&
          basic.LimitFlags == (JOB_OBJECT_LIMIT_ACTIVE_PROCESS | JOB_OBJECT_LIMIT_PRIORITY_CLASS),
          "the basic limits read back (basic flags only)");

    ui.UIRestrictionsClass = JOB_OBJECT_UILIMIT_HANDLES | JOB_OBJECT_UILIMIT_DESKTOP;
    check(SetInformationJobObject(job, JobObjectBasicUIRestrictions, &ui, sizeof(ui)), "UI restrictions set");
    ui.UIRestrictionsClass = 0;
    ok = QueryInformationJobObject(job, JobObjectBasicUIRestrictions, &ui, sizeof(ui), &len);
    check(ok && ui.UIRestrictionsClass == (JOB_OBJECT_UILIMIT_HANDLES | JOB_OBJECT_UILIMIT_DESKTOP),
          "... and read back");
    ui.UIRestrictionsClass = 0x100;
    SetLastError(0xdeadbeef);
    check(!SetInformationJobObject(job, JobObjectBasicUIRestrictions, &ui, sizeof(ui)) &&
          GetLastError() == ERROR_INVALID_PARAMETER, "... unknown restrictions are refused");

    iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 1);
    port.CompletionKey = job;
    port.CompletionPort = iocp;
    SetInformationJobObject(job, JobObjectAssociateCompletionPortInformation, &port, sizeof(port));

    /* one process in the job */
    ok = start("spawn", CREATE_SUSPENDED, &a) && AssignProcessToJobObject(job, a.hProcess);
    check(ok, "a process is assigned to the job");
    printf("its priority class: %#lx\n", GetPriorityClass(a.hProcess));
    check(GetPriorityClass(a.hProcess) == BELOW_NORMAL_PRIORITY_CLASS, "... and takes the job's priority class");

    /* a second is refused */
    if (start("sleep", CREATE_SUSPENDED, &b))
    {
        SetLastError(0xdeadbeef);
        ok = AssignProcessToJobObject(job, b.hProcess);
        printf("second assignment: %d err %lu\n", ok, ok ? 0 : GetLastError());
        check(!ok && GetLastError() == ERROR_NOT_ENOUGH_QUOTA, "a second process is refused (ERROR_NOT_ENOUGH_QUOTA)");
        TerminateProcess(b.hProcess, 0);
        CloseHandle(b.hProcess);
        CloseHandle(b.hThread);
    }

    /* and the one in the job cannot start another */
    ResumeThread(a.hThread);
    WaitForSingleObject(a.hProcess, 30000);
    GetExitCodeProcess(a.hProcess, &code);
    printf("the process in the job, starting another: %lu\n", code);
    check(code == ERROR_NOT_ENOUGH_QUOTA, "a process in the job cannot start another past the limit");

    while (GetQueuedCompletionStatus(iocp, &msg, &key, &ovl, 0))
        if (msg == JOB_OBJECT_MSG_ACTIVE_PROCESS_LIMIT) limit_msg = TRUE;
    check(limit_msg, "the completion port heard JOB_OBJECT_MSG_ACTIVE_PROCESS_LIMIT");

    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
