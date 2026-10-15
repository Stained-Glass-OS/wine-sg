#define COBJMACROS
#include <windows.h>
#include <ole2.h>
#include <mstask.h>
#include <stdio.h>

static int fails, checks;
#define CHECK(c) do { checks++; if (!(c)) { fails++; printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)
#define OK(e) do { HRESULT h_ = (e); checks++; if (FAILED(h_)) { fails++; printf("FAIL line %d: %s -> %08lx\n", __LINE__, #e, (long)h_); } } while (0)

static const GUID my_CLSID_CTaskScheduler = {0x148bd52a,0xa2ab,0x11ce,{0xb1,0x1f,0x00,0xaa,0x00,0x53,0x05,0x03}};
static const GUID my_CLSID_CTask = {0x148bd520,0xa2ab,0x11ce,{0xb1,0x1f,0x00,0xaa,0x00,0x53,0x05,0x03}};

int main(void)
{
    ITaskScheduler *sched = NULL;
    ITask *task = NULL;
    ITaskTrigger *trig = NULL;
    TASK_TRIGGER tt;
    WORD idx, a, b, count;
    SYSTEMTIME begin, end, *times = NULL;
    WCHAR *str = NULL;
    DWORD prio;
    HRESULT hr;

    CoInitialize(NULL);
    hr = CoCreateInstance(&my_CLSID_CTaskScheduler, NULL, CLSCTX_INPROC_SERVER, &IID_ITaskScheduler, (void **)&sched);
    CHECK(hr == S_OK);
    if (!sched) return 1;
    OK(ITaskScheduler_NewWorkItem(sched, L"fastb-mstask-probe", &my_CLSID_CTask, &IID_ITask, (IUnknown **)&task));

    OK(ITask_SetIdleWait(task, 5, 20));
    OK(ITask_GetIdleWait(task, &a, &b));
    CHECK(a == 5 && b == 20);

    OK(ITask_SetPriority(task, HIGH_PRIORITY_CLASS));
    OK(ITask_GetPriority(task, &prio));
    CHECK(prio == HIGH_PRIORITY_CLASS);
    CHECK(ITask_SetPriority(task, 12345) == E_INVALIDARG);

    OK(ITask_CreateTrigger(task, &idx, &trig));
    memset(&tt, 0, sizeof(tt));
    tt.cbTriggerSize = sizeof(tt);
    tt.wBeginYear = 2030; tt.wBeginMonth = 1; tt.wBeginDay = 1;
    tt.wStartHour = 9; tt.wStartMinute = 30;
    tt.TriggerType = TASK_TIME_TRIGGER_DAILY;
    tt.Type.Daily.DaysInterval = 2;
    OK(ITaskTrigger_SetTrigger(trig, &tt));
    OK(ITaskTrigger_GetTriggerString(trig, &str));
    CHECK(str && wcsstr(str, L"every 2 days"));
    CoTaskMemFree(str);
    str = NULL;
    OK(ITask_GetTriggerString(task, 0, &str));
    CHECK(str && wcsstr(str, L"every 2 days"));
    CoTaskMemFree(str);
    CHECK(ITask_GetTriggerString(task, 5, &str) == SCHED_E_TRIGGER_NOT_FOUND);

    memset(&begin, 0, sizeof(begin)); memset(&end, 0, sizeof(end));
    begin.wYear = 2030; begin.wMonth = 1; begin.wDay = 1;
    end.wYear = 2030; end.wMonth = 1; end.wDay = 10;
    count = 10;
    OK(ITask_GetRunTimes(task, &begin, &end, &count, &times));
    CHECK(count == 5);       /* Jan 1, 3, 5, 7, 9 */
    if (times && count == 5)
    {
        CHECK(times[0].wDay == 1 && times[0].wHour == 9 && times[0].wMinute == 30);
        CHECK(times[1].wDay == 3 && times[4].wDay == 9);
    }
    CoTaskMemFree(times);
    times = NULL;
    count = 2;
    OK(ITask_GetRunTimes(task, &begin, NULL, &count, &times));
    CHECK(count == 2);
    CoTaskMemFree(times);
    count = 0;
    CHECK(ITask_GetRunTimes(task, &begin, &end, &count, &times) == E_INVALIDARG);

    CHECK(FAILED(ITaskScheduler_IsOfType(sched, L"fastb-no-such-task", &IID_ITask)));

    ITaskTrigger_Release(trig);
    ITask_Release(task);
    ITaskScheduler_Release(sched);
    printf("%d checks, %d failures\nRESULT: %s\n", checks, fails, fails ? "FAIL" : "PASS");
    return fails != 0;
}
