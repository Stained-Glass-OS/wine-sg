/* Probe for patches/sg/2439: behaviours of taskschd/schedsvc recorded from Windows by Wine's scheduler test. */
#define COBJMACROS
#include <initguid.h>
#include <windows.h>
#include <taskschd.h>
#include <stdio.h>
#include <string.h>

#ifndef SCHED_E_UNEXPECTEDNODE
#define SCHED_E_UNEXPECTEDNODE ((HRESULT)0x80041316)
#endif
#ifndef SCHED_E_MISSINGNODE
#define SCHED_E_MISSINGNODE ((HRESULT)0x80041319)
#endif
static int fails;
static void check(const char *name, int ok) { printf("      %s  %s\n", ok ? "PASS" : "FAIL", name); if (!ok) fails++; }
static void checkhr(const char *name, HRESULT got, HRESULT want)
{
    char buf[160];
    snprintf(buf, sizeof(buf), "%s (%08lx, want %08lx)", name, (unsigned long)got, (unsigned long)want);
    check(buf, got == want);
}

static const WCHAR *xml_off =
    L"<Task xmlns=\"http://schemas.microsoft.com/windows/2004/02/mit/task\">"
    L"<RegistrationInfo/><Settings><Enabled>false</Enabled></Settings>"
    L"<Actions><Exec><Command>cmd.exe</Command></Exec></Actions></Task>";

int main(void)
{
    ITaskService *svc;
    ITaskFolder *root = NULL, *sub = NULL;
    IRegisteredTask *task = NULL;
    ITaskDefinition *def;
    VARIANT vnull, vbool;
    HRESULT hr;
    BSTR b;
    TASK_STATE state;
    VARIANT_BOOL en;

    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    hr = CoCreateInstance(&CLSID_TaskScheduler, NULL, CLSCTX_INPROC_SERVER, &IID_ITaskService, (void **)&svc);
    if (FAILED(hr)) { printf("      FAIL  no task service %08lx\n", (unsigned long)hr); puts("RESULT: FAIL"); return 1; }
    V_VT(&vnull) = VT_NULL;
    hr = ITaskService_Connect(svc, vnull, vnull, vnull, vnull);
    checkhr("Connect", hr, S_OK);

    /* folder names */
    b = SysAllocString(L"/");
    checkhr("GetFolder(/)", ITaskService_GetFolder(svc, b, &root), HRESULT_FROM_WIN32(ERROR_INVALID_NAME));
    SysFreeString(b);
    b = SysAllocString(L".");
    checkhr("GetFolder(.)", ITaskService_GetFolder(svc, b, &root), HRESULT_FROM_WIN32(ERROR_INVALID_NAME));
    SysFreeString(b);
    b = SysAllocString(L"\\");
    checkhr("GetFolder(root)", ITaskService_GetFolder(svc, b, &root), S_OK);
    SysFreeString(b);
    if (!root) { puts("RESULT: FAIL"); return 1; }
    b = SysAllocString(L"/");
    checkhr("CreateFolder(/)", ITaskFolder_CreateFolder(root, b, vnull, &sub), HRESULT_FROM_WIN32(ERROR_INVALID_NAME));
    checkhr("DeleteFolder(/)", ITaskFolder_DeleteFolder(root, b, 0), HRESULT_FROM_WIN32(ERROR_INVALID_NAME));
    SysFreeString(b);
    b = SysAllocString(L"a:b");
    checkhr("CreateFolder(a:b)", ITaskFolder_CreateFolder(root, b, vnull, &sub), HRESULT_FROM_WIN32(ERROR_INVALID_NAME));
    SysFreeString(b);
    b = SysAllocString(L"\\");
    checkhr("CreateFolder(\\)", ITaskFolder_CreateFolder(root, b, vnull, &sub), E_INVALIDARG);
    SysFreeString(b);

    /* a folder with a task: DeleteTask without a name is the folder, which is not empty */
    b = SysAllocString(L"SgGroundFolder");
    ITaskFolder_DeleteFolder(root, b, 0);
    checkhr("CreateFolder", ITaskFolder_CreateFolder(root, b, vnull, &sub), S_OK);
    SysFreeString(b);
    b = SysAllocString((WCHAR *)xml_off);
    {
        BSTR name = SysAllocString(L"SgGroundTask");
        hr = ITaskFolder_RegisterTask(sub, name, b, TASK_CREATE_OR_UPDATE, vnull, vnull, TASK_LOGON_NONE, vnull, &task);
        checkhr("RegisterTask (XML says Enabled false)", hr, S_OK);
        SysFreeString(name);
    }
    SysFreeString(b);
    if (task)
    {
        IRegisteredTask_get_State(task, &state);
        IRegisteredTask_get_Enabled(task, &en);
        check("registered disabled", state == TASK_STATE_DISABLED && en == VARIANT_FALSE);
        checkhr("put_Enabled(true)", IRegisteredTask_put_Enabled(task, VARIANT_TRUE), S_OK);
        hr = IRegisteredTask_get_State(task, &state);
        check("state is Ready after enabling", hr == S_OK && state == TASK_STATE_READY);
        IRegisteredTask_get_Enabled(task, &en);
        check("Enabled is true after enabling", en == VARIANT_TRUE);
        checkhr("put_Enabled(false)", IRegisteredTask_put_Enabled(task, VARIANT_FALSE), S_OK);
        IRegisteredTask_get_State(task, &state);
        check("disabled again", state == TASK_STATE_DISABLED);
        IRegisteredTask_put_Enabled(task, VARIANT_TRUE);
        IRegisteredTask_Release(task);
    }
    checkhr("DeleteTask(NULL) on a folder with a task", ITaskFolder_DeleteTask(sub, NULL, 0),
            HRESULT_FROM_WIN32(ERROR_DIR_NOT_EMPTY));
    b = SysAllocString(L"SgGroundTask");
    checkhr("DeleteTask", ITaskFolder_DeleteTask(sub, b, 0), S_OK);
    /* registered again: the old EnableTask does not carry over */
    {
        BSTR x = SysAllocString((WCHAR *)xml_off);
        task = NULL;
        hr = ITaskFolder_RegisterTask(sub, b, x, TASK_CREATE_OR_UPDATE, vnull, vnull, TASK_LOGON_NONE, vnull, &task);
        checkhr("RegisterTask again", hr, S_OK);
        SysFreeString(x);
        if (task)
        {
            IRegisteredTask_get_State(task, &state);
            check("a new registration is disabled again", state == TASK_STATE_DISABLED);
            IRegisteredTask_Release(task);
        }
    }
    ITaskFolder_DeleteTask(sub, b, 0);
    SysFreeString(b);
    ITaskFolder_Release(sub);
    b = SysAllocString(L"SgGroundFolder");
    checkhr("DeleteFolder", ITaskFolder_DeleteFolder(root, b, 0), S_OK);
    SysFreeString(b);
    ITaskFolder_Release(root);

    /* task XML */
    hr = ITaskService_NewTask(svc, 0, &def);
    checkhr("NewTask", hr, S_OK);
    b = SysAllocString(L"<TASK><Actions><Exec><Command>x</Command></Exec></Actions></TASK>");
    checkhr("root element TASK (case)", ITaskDefinition_put_XmlText(def, b), SCHED_E_UNEXPECTEDNODE);
    SysFreeString(b);
    b = SysAllocString(L"<Task xmlns=\"http://schemas.microsoft.com/windows/2004/02/mit/task\"><Actions/></Task>");
    checkhr("empty Actions", ITaskDefinition_put_XmlText(def, b), SCHED_E_MISSINGNODE);
    SysFreeString(b);
    b = SysAllocString((WCHAR *)xml_off);
    checkhr("good XML after those", ITaskDefinition_put_XmlText(def, b), S_OK);
    SysFreeString(b);
    ITaskDefinition_Release(def);
    ITaskService_Release(svc);
    CoUninitialize();
    puts(fails ? "RESULT: FAIL" : "RESULT: PASS");
    return fails != 0;
}
