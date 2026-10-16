/* Task Scheduler monthly triggers (patches/sg/5528): ScheduleByMonth and
 * ScheduleByMonthDayOfWeek, read by the schedule service and listed by
 * GetRunTimes. Dates are in 2030 (Jan 1 2030 is a Tuesday; 2030 is not a
 * leap year; Mar 1 2030 is a Friday). */
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

static const WCHAR head[] =
    L"<?xml version=\"1.0\"?>\n<Task xmlns=\"http://schemas.microsoft.com/windows/2004/02/mit/task\">\n"
    L"  <Principals><Principal id=\"Author\"><UserId>S-1-5-18</UserId></Principal></Principals>\n  <Triggers>\n";
static const WCHAR tail[] =
    L"  </Triggers>\n  <Settings><AllowStartOnDemand>true</AllowStartOnDemand><Enabled>true</Enabled></Settings>\n"
    L"  <Actions Context=\"Author\"><Exec><Command>C:\\windows\\system32\\cmd.exe</Command></Exec></Actions>\n</Task>\n";

static IRegisteredTask *register_task(ITaskFolder *root, const WCHAR *name, const WCHAR *trigger)
{
    WCHAR xml[4096];
    IRegisteredTask *task = NULL;
    VARIANT empty, sddl;
    HRESULT hr;

    swprintf(xml, ARRAYSIZE(xml), L"%ls%ls%ls", head, trigger, tail);
    VariantInit(&empty);
    VariantInit(&sddl);
    ITaskFolder_DeleteTask(root, (BSTR)name, 0);
    hr = ITaskFolder_RegisterTask(root, (BSTR)name, xml, TASK_CREATE_OR_UPDATE, empty, empty, TASK_LOGON_SERVICE_ACCOUNT, sddl, &task);
    if (FAILED(hr)) printf("RegisterTask %ls: %#lx\n", name, hr);
    return task;
}

/* the run times in [from, to] as "M/D H:M" strings, joined with ';' */
static int run_times(IRegisteredTask *task, int fm, int fd, int tm, int td, char *out, size_t size)
{
    SYSTEMTIME from = { 2030, fm, 0, fd }, to = { 2030, tm, 0, td, 23, 59 }, *times = NULL;
    DWORD count = 0, i;
    HRESULT hr = IRegisteredTask_GetRunTimes(task, &from, &to, &count, &times);

    out[0] = 0;
    if (FAILED(hr)) return -1;
    for (i = 0; i < count; i++)
    {
        char one[32];
        snprintf(one, sizeof(one), "%s%u/%u %u:%02u", i ? ";" : "", times[i].wMonth, times[i].wDay, times[i].wHour, times[i].wMinute);
        strncat(out, one, size - strlen(out) - 1);
    }
    CoTaskMemFree(times);
    return count;
}

int main(void)
{
    ITaskService *service;
    ITaskFolder *root;
    IRegisteredTask *task;
    VARIANT empty;
    char buf[512];
    HRESULT hr;

    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    hr = CoCreateInstance(&CLSID_TaskScheduler, NULL, CLSCTX_INPROC_SERVER, &IID_ITaskService, (void **)&service);
    if (FAILED(hr)) { printf("FAIL  no task service %#lx\nRESULT: FAIL\n", hr); return 1; }
    VariantInit(&empty);
    ITaskService_Connect(service, empty, empty, empty, empty);
    ITaskService_GetFolder(service, (BSTR)L"\\", &root);

    task = register_task(root, L"SG Monthly Days",
        L"<CalendarTrigger><StartBoundary>2030-01-01T10:00:00Z</StartBoundary><ScheduleByMonth>"
        L"<DaysOfMonth><Day>1</Day><Day>15</Day></DaysOfMonth><Months><January/><March/></Months>"
        L"</ScheduleByMonth></CalendarTrigger>");
    check(task != NULL, "a ScheduleByMonth task registers");
    if (task)
    {
        int n = run_times(task, 1, 1, 4, 30, buf, sizeof(buf));
        printf("days: %d %s\n", n, buf);
        check(n == 4 && !strcmp(buf, "1/1 10:00;1/15 10:00;3/1 10:00;3/15 10:00"), "days 1 and 15 of January and March, at the start's time");
        n = run_times(task, 1, 2, 1, 14, buf, sizeof(buf));
        check(n == 0, "no run between two chosen days");
        IRegisteredTask_Release(task);
    }

    task = register_task(root, L"SG Monthly Last",
        L"<CalendarTrigger><StartBoundary>2030-01-01T06:30:00Z</StartBoundary><ScheduleByMonth>"
        L"<DaysOfMonth><Day>Last</Day></DaysOfMonth><Months><February/><April/></Months>"
        L"</ScheduleByMonth></CalendarTrigger>");
    if (task)
    {
        int n = run_times(task, 1, 1, 5, 31, buf, sizeof(buf));
        printf("last: %d %s\n", n, buf);
        check(n == 2 && !strcmp(buf, "2/28 6:30;4/30 6:30"), "the last day of February (28 in 2030) and April (30)");
        IRegisteredTask_Release(task);
    }

    task = register_task(root, L"SG Monthly DOW",
        L"<CalendarTrigger><StartBoundary>2030-01-01T09:00:00Z</StartBoundary><ScheduleByMonthDayOfWeek>"
        L"<Weeks><Week>2</Week><Week>Last</Week></Weeks><DaysOfWeek><Tuesday/><Friday/></DaysOfWeek>"
        L"<Months><January/><March/></Months></ScheduleByMonthDayOfWeek></CalendarTrigger>");
    check(task != NULL, "a ScheduleByMonthDayOfWeek task registers");
    if (task)
    {
        int n = run_times(task, 1, 1, 3, 31, buf, sizeof(buf));
        printf("dow: %d %s\n", n, buf);
        /* Jan 2030: Tuesdays 1 8 15 22 29, Fridays 4 11 18 25 -> second Tuesday 8, last Tuesday 29, second Friday 11, last Friday 25;
         * Mar 2030: Tuesdays 5 12 19 26, Fridays 1 8 15 22 29 -> 12, 26, 8, 29 */
        check(n == 8 && !strcmp(buf, "1/8 9:00;1/11 9:00;1/25 9:00;1/29 9:00;3/8 9:00;3/12 9:00;3/26 9:00;3/29 9:00"),
              "the second and last Tuesday and Friday of January and March");
        IRegisteredTask_Release(task);
    }

    task = register_task(root, L"SG Monthly Start",
        L"<CalendarTrigger><StartBoundary>2030-01-20T10:00:00Z</StartBoundary><ScheduleByMonth>"
        L"<DaysOfMonth><Day>15</Day></DaysOfMonth><Months><January/><February/></Months>"
        L"</ScheduleByMonth></CalendarTrigger>");
    if (task)
    {
        int n = run_times(task, 1, 1, 3, 1, buf, sizeof(buf));
        printf("start: %d %s\n", n, buf);
        check(n == 1 && !strcmp(buf, "2/15 10:00"), "nothing runs before the start boundary (Jan 15 is earlier than Jan 20)");
        IRegisteredTask_Release(task);
    }

    ITaskFolder_DeleteTask(root, (BSTR)L"SG Monthly Days", 0);
    ITaskFolder_DeleteTask(root, (BSTR)L"SG Monthly Last", 0);
    ITaskFolder_DeleteTask(root, (BSTR)L"SG Monthly DOW", 0);
    ITaskFolder_DeleteTask(root, (BSTR)L"SG Monthly Start", 0);
    ITaskFolder_Release(root);
    ITaskService_Release(service);
    CoUninitialize();
    printf(failures ? "RESULT: FAIL\n" : "RESULT: PASS\n");
    return failures != 0;
}
