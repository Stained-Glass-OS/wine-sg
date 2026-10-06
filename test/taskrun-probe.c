/* taskrun-probe: drives the Task Scheduler through its COM API (taskschd)
 * for test/taskrun-gate.sh.
 *
 *   taskrun-probe register NAME XMLFILE   registers the task (XML in UTF-8)
 *   taskrun-probe run NAME                IRegisteredTask::Run
 *   taskrun-probe info NAME               state=, enabled=, lastrun=, result=
 *   taskrun-probe delete NAME
 *   taskrun-probe xml NAME                the registered task's XML (IRegisteredTask::get_Xml)
 *   taskrun-probe disable NAME            IRegisteredTask::put_Enabled(FALSE)
 *   taskrun-probe newtask NAME START COMMAND ARGS
 *       through the object model, as Chromium's updater registers its task:
 *       a time trigger at START repeating every minute, an Exec action, run
 *       as SYSTEM (RegisterTaskDefinition's user, a service account)
 *
 * Prints name=value lines; exit code 0 when the call worked.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <taskschd.h>
#include <stdio.h>

DEFINE_GUID(probe_CLSID_TaskScheduler, 0x0f87369f, 0xa4e5, 0x4cfc, 0xbd, 0x3e, 0x73, 0xe6, 0x15, 0x45, 0x72, 0xdd);
DEFINE_GUID(probe_IID_ITaskService, 0x2faba4c7, 0x4da9, 0x4013, 0x96, 0x97, 0x20, 0xcc, 0x3f, 0xd4, 0x0f, 0x85);

static ITaskFolder *root_folder(void)
{
    ITaskService *svc;
    ITaskFolder *root;
    VARIANT v;
    BSTR slash;
    HRESULT hr;

    if (FAILED(hr = CoCreateInstance(&probe_CLSID_TaskScheduler, NULL, CLSCTX_INPROC_SERVER, &probe_IID_ITaskService,
                                     (void **)&svc)))
    {
        printf("create=%#lx\n", hr);
        return NULL;
    }
    VariantInit(&v);
    if (FAILED(hr = ITaskService_Connect(svc, v, v, v, v)))
    {
        printf("connect=%#lx\n", hr);
        return NULL;
    }
    slash = SysAllocString(L"\\");
    hr = ITaskService_GetFolder(svc, slash, &root);
    if (FAILED(hr)) { printf("folder=%#lx\n", hr); return NULL; }
    return root;
}

static WCHAR *wide(const char *s)
{
    int n = MultiByteToWideChar(CP_UTF8, 0, s, -1, NULL, 0);
    WCHAR *w = malloc(n * sizeof(WCHAR));
    MultiByteToWideChar(CP_UTF8, 0, s, -1, w, n);
    return w;
}

int main(int argc, char **argv)
{
    ITaskFolder *root;
    IRegisteredTask *task = NULL;
    BSTR name;
    VARIANT empty;
    HRESULT hr;

    if (argc < 3) return 2;
    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (!(root = root_folder())) return 1;
    name = SysAllocString(wide(argv[2]));
    VariantInit(&empty);

    if (!strcmp(argv[1], "newtask") && argc > 5)
    {
        ITaskService *svc;
        ITaskDefinition *def;
        ITriggerCollection *triggers;
        ITrigger *trigger;
        IRepetitionPattern *rep;
        IActionCollection *actions;
        IAction *action;
        IExecAction *exec;
        IRegistrationInfo *info;
        VARIANT user;
        TASK_TRIGGER_TYPE2 type = -1;
        LONG count = 0;

        CoCreateInstance(&probe_CLSID_TaskScheduler, NULL, CLSCTX_INPROC_SERVER, &probe_IID_ITaskService, (void **)&svc);
        ITaskService_Connect(svc, empty, empty, empty, empty);
        if (FAILED(hr = ITaskService_NewTask(svc, 0, &def))) { printf("newtask=%#lx\n", hr); return 1; }
        ITaskDefinition_get_RegistrationInfo(def, &info);
        IRegistrationInfo_put_Description(info, SysAllocString(L"probe & <test>"));
        ITaskDefinition_get_Triggers(def, &triggers);
        if (FAILED(hr = ITriggerCollection_Create(triggers, TASK_TRIGGER_TIME, &trigger))) { printf("trigger=%#lx\n", hr); return 1; }
        ITrigger_put_StartBoundary(trigger, SysAllocString(wide(argv[3])));
        ITrigger_get_Repetition(trigger, &rep);
        IRepetitionPattern_put_Interval(rep, SysAllocString(L"PT1M"));
        ITrigger_get_Type(trigger, &type);
        ITriggerCollection_get_Count(triggers, &count);
        printf("trigger_type=%d count=%ld\n", type, count);
        ITaskDefinition_get_Actions(def, &actions);
        if (FAILED(hr = IActionCollection_Create(actions, TASK_ACTION_EXEC, &action))) { printf("action=%#lx\n", hr); return 1; }
        IAction_QueryInterface(action, &IID_IExecAction, (void **)&exec);
        IExecAction_put_Path(exec, SysAllocString(wide(argv[4])));
        IExecAction_put_Arguments(exec, SysAllocString(wide(argv[5])));
        V_VT(&user) = VT_BSTR;
        V_BSTR(&user) = SysAllocString(L"SYSTEM");
        hr = ITaskFolder_RegisterTaskDefinition(root, name, def, TASK_CREATE_OR_UPDATE, user, empty,
                                                TASK_LOGON_SERVICE_ACCOUNT, empty, &task);
        printf("register=%#lx\n", hr);
        return FAILED(hr);
    }
    if (!strcmp(argv[1], "register") && argc > 3)
    {
        FILE *f = fopen(argv[3], "rb");
        char buf[65536];
        size_t n;
        BSTR xml;

        if (!f) { printf("xmlfile=missing\n"); return 1; }
        n = fread(buf, 1, sizeof(buf) - 1, f);
        buf[n] = 0;
        fclose(f);
        xml = SysAllocString(wide(buf));
        hr = ITaskFolder_RegisterTask(root, name, xml, TASK_CREATE_OR_UPDATE, empty, empty, TASK_LOGON_NONE, empty, &task);
        printf("register=%#lx\n", hr);
        return FAILED(hr);
    }
    if (FAILED(hr = ITaskFolder_GetTask(root, name, &task)))
    {
        printf("gettask=%#lx\n", hr);
        return 1;
    }
    if (!strcmp(argv[1], "run"))
    {
        IRunningTask *running = NULL;
        hr = IRegisteredTask_Run(task, empty, &running);
        printf("run=%#lx\n", hr);
        return FAILED(hr);
    }
    if (!strcmp(argv[1], "info"))
    {
        TASK_STATE state = 0;
        VARIANT_BOOL enabled = 0;
        DATE last = 0;
        LONG result = 0;
        SYSTEMTIME st = { 0 };

        hr = IRegisteredTask_get_State(task, &state);
        printf("state=%d (%#lx)\n", state, hr);
        hr = IRegisteredTask_get_Enabled(task, &enabled);
        printf("enabled=%d (%#lx)\n", enabled ? 1 : 0, hr);
        hr = IRegisteredTask_get_LastRunTime(task, &last);
        if (SUCCEEDED(hr) && last) VariantTimeToSystemTime(last, &st);
        printf("lastrun=%04d-%02d-%02dT%02d:%02d:%02d (%#lx)\n", st.wYear, st.wMonth, st.wDay, st.wHour,
               st.wMinute, st.wSecond, hr);
        hr = IRegisteredTask_get_LastTaskResult(task, &result);
        printf("result=%ld (%#lx)\n", result, hr);
        return 0;
    }
    if (!strcmp(argv[1], "xml"))
    {
        BSTR xml = NULL;
        hr = IRegisteredTask_get_Xml(task, &xml);
        printf("xml=%#lx\n", hr);
        if (xml)
        {
            int n = WideCharToMultiByte(CP_UTF8, 0, xml, -1, NULL, 0, NULL, NULL);
            char *u = malloc(n);
            WideCharToMultiByte(CP_UTF8, 0, xml, -1, u, n, NULL, NULL);
            fputs(u, stdout);
        }
        return FAILED(hr);
    }
    if (!strcmp(argv[1], "disable"))
    {
        hr = IRegisteredTask_put_Enabled(task, VARIANT_FALSE);
        printf("disable=%#lx\n", hr);
        return FAILED(hr);
    }
    if (!strcmp(argv[1], "delete"))
    {
        hr = ITaskFolder_DeleteTask(root, name, 0);
        printf("delete=%#lx\n", hr);
        return FAILED(hr);
    }
    return 2;
}
