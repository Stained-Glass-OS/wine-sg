/* Task Scheduler: security descriptors, run times, instances
 * (patches/sg/1632). These were E_NOTIMPL on both sides (taskschd and the
 * schedule service): GetSecurityDescriptor/SetSecurityDescriptor of tasks
 * and folders, NextRunTime, GetRunTimes, NumberOfMissedRuns, GetInstances,
 * ITaskService::GetRunningTasks; a running task had no instance GUID,
 * current action or process id. */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <taskschd.h>
#include <stdio.h>

static int failures;

static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

static const WCHAR xml[] =
    L"<?xml version=\"1.0\"?>\n"
    L"<Task xmlns=\"http://schemas.microsoft.com/windows/2004/02/mit/task\">\n"
    L"  <Principals><Principal id=\"Author\"><UserId>S-1-5-18</UserId></Principal></Principals>\n"
    L"  <Triggers>\n"
    L"    <CalendarTrigger><StartBoundary>2030-01-01T10:00:00</StartBoundary>\n"
    L"      <ScheduleByDay><DaysInterval>1</DaysInterval></ScheduleByDay></CalendarTrigger>\n"
    L"    <TimeTrigger><StartBoundary>2030-01-05T08:00:00</StartBoundary></TimeTrigger>\n"
    L"  </Triggers>\n"
    L"  <Settings><AllowStartOnDemand>true</AllowStartOnDemand><Enabled>true</Enabled>\n"
    L"    <MultipleInstancesPolicy>IgnoreNew</MultipleInstancesPolicy></Settings>\n"
    L"  <Actions Context=\"Author\"><Exec><Command>C:\\windows\\system32\\cmd.exe</Command>\n"
    L"    <Arguments>/c ping -n 60 127.0.0.1</Arguments></Exec></Actions>\n"
    L"</Task>\n";

int main(void)
{
    ITaskService *service;
    ITaskFolder *root;
    IRegisteredTask *task;
    IRunningTask *running = NULL;
    IRunningTaskCollection *instances;
    VARIANT empty, sddl;
    BSTR str, name = SysAllocString(L"SG Task Info Probe");
    SYSTEMTIME start = { 2030, 1, 0, 1, 0, 0, 0, 0 }, end = { 2030, 1, 0, 4, 0, 0, 0, 0 }, *times = NULL, st;
    DWORD count, pid = 0;
    LONG n = -1, missed = -1;
    DATE next = 0;
    HRESULT hr;
    int i;

    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    hr = CoCreateInstance(&CLSID_TaskScheduler, NULL, CLSCTX_INPROC_SERVER, &IID_ITaskService, (void **)&service);
    if (FAILED(hr)) { printf("FAIL  no task service %#lx\nRESULT: FAIL\n", hr); return 1; }
    VariantInit(&empty);
    hr = ITaskService_Connect(service, empty, empty, empty, empty);
    hr = ITaskService_GetFolder(service, (BSTR)L"\\", &root);
    check(hr == S_OK, "connected, root folder");
    ITaskFolder_DeleteTask(root, name, 0);

    V_VT(&sddl) = VT_BSTR;
    V_BSTR(&sddl) = SysAllocString(L"O:BAG:SYD:(A;;FA;;;BA)(A;;FRFX;;;WD)");
    hr = ITaskFolder_RegisterTask(root, name, (BSTR)xml, TASK_CREATE_OR_UPDATE, empty, empty,
                                  TASK_LOGON_SERVICE_ACCOUNT, sddl, &task);
    printf("RegisterTask: %#lx\n", hr);
    check(hr == S_OK, "a system task registered, with a security descriptor");
    if (FAILED(hr)) { printf("RESULT: FAIL\n"); return 1; }

    /* security descriptors */
    str = NULL;
    hr = IRegisteredTask_GetSecurityDescriptor(task, DACL_SECURITY_INFORMATION, &str);
    printf("task SD: %#lx %ls\n", hr, str ? str : L"");
    check(hr == S_OK && str && wcsstr(str, L";;;WD)"), "the registration's security descriptor reads back");
    SysFreeString(str);
    hr = IRegisteredTask_SetSecurityDescriptor(task, (BSTR)L"D:(A;;FA;;;SY)(A;;FR;;;AU)", 0);
    str = NULL;
    IRegisteredTask_GetSecurityDescriptor(task, DACL_SECURITY_INFORMATION, &str);
    check(hr == S_OK && str && wcsstr(str, L";;;AU)") && !wcsstr(str, L";;;WD)"), "SetSecurityDescriptor replaces it");
    SysFreeString(str);
    str = NULL;
    hr = ITaskFolder_GetSecurityDescriptor(root, DACL_SECURITY_INFORMATION, &str);
    printf("root SD: %#lx %ls\n", hr, str ? str : L"");
    check(hr == S_OK && str && wcsstr(str, L"D:"), "the root folder has a security descriptor");
    SysFreeString(str);
    hr = IRegisteredTask_SetSecurityDescriptor(task, (BSTR)L"this is not SDDL", 0);
    check(FAILED(hr), "bad SDDL is refused");

    /* run times */
    hr = IRegisteredTask_get_NextRunTime(task, &next);
    VariantTimeToSystemTime(next, &st);
    printf("next run: %#lx %04u-%02u-%02u %02u:%02u\n", hr, st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute);
    check(hr == S_OK && st.wYear == 2030 && st.wMonth == 1 && st.wDay == 1 && st.wHour == 10, "NextRunTime: the first daily run");
    count = 0;
    hr = IRegisteredTask_GetRunTimes(task, &start, &end, &count, &times);
    printf("run times: %#lx %lu\n", hr, count);
    for (i = 0; i < (int)count; i++) printf("  %04u-%02u-%02u %02u:%02u\n", times[i].wYear, times[i].wMonth, times[i].wDay, times[i].wHour, times[i].wMinute);
    check(hr == S_OK && count == 3 && times[2].wDay == 3 && times[2].wHour == 10, "GetRunTimes: the three daily runs in the range");
    CoTaskMemFree(times);
    end.wDay = 6;
    count = 0; times = NULL;
    IRegisteredTask_GetRunTimes(task, &start, &end, &count, &times);
    check(count == 6, "... with the one-time trigger in a wider range");
    CoTaskMemFree(times);
    hr = IRegisteredTask_get_NumberOfMissedRuns(task, &missed);
    check(hr == S_OK && missed >= 0, "NumberOfMissedRuns answers");

    /* instances */
    {
        VARIANT params;
        VariantInit(&params);
        hr = IRegisteredTask_Run(task, params, &running);
    }
    printf("Run: %#lx\n", hr);
    check(hr == S_OK && running, "Run");
    if (running)
    {
        str = NULL;
        IRunningTask_get_InstanceGuid(running, &str);
        printf("instance %ls\n", str);
        check(str && wcscmp(str, L"{00000000-0000-0000-0000-000000000000}"), "the running task has an instance GUID");
        SysFreeString(str);
        for (i = 0; i < 50 && !pid; i++) { IRunningTask_get_EnginePID(running, &pid); if (!pid) Sleep(100); }
        check(pid != 0, "... and its action's process id");
        str = NULL;
        IRunningTask_get_CurrentAction(running, &str);
        printf("current action: %ls\n", str ? str : L"(none)");
        check(str && wcsstr(str, L"cmd.exe"), "... and its current action");
        SysFreeString(str);
        hr = IRegisteredTask_GetInstances(task, 0, &instances);
        n = -1;
        if (hr == S_OK) { IRunningTaskCollection_get_Count(instances, &n); IRunningTaskCollection_Release(instances); }
        check(hr == S_OK && n == 1, "GetInstances: one");
        hr = ITaskService_GetRunningTasks(service, TASK_ENUM_HIDDEN, &instances);
        n = -1;
        if (hr == S_OK) { IRunningTaskCollection_get_Count(instances, &n); IRunningTaskCollection_Release(instances); }
        check(hr == S_OK && n >= 1, "ITaskService::GetRunningTasks lists it");
        hr = IRunningTask_Stop(running);
        check(hr == S_OK, "IRunningTask::Stop");
        for (i = 0; i < 50; i++)
        {
            n = -1;
            if (IRegisteredTask_GetInstances(task, 0, &instances) == S_OK)
            {
                IRunningTaskCollection_get_Count(instances, &n);
                IRunningTaskCollection_Release(instances);
            }
            if (!n) break;
            Sleep(100);
        }
        check(n == 0, "... and the instance is gone");
        IRunningTask_Release(running);
    }

    ITaskFolder_DeleteTask(root, name, 0);
    IRegisteredTask_Release(task);
    ITaskFolder_Release(root);
    ITaskService_Release(service);
    CoUninitialize();
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
