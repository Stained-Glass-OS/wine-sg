/* A service whose security descriptor grants SERVICE_SET_STATUS to nobody (6108). CCleaner 7's
 * installer sets such a descriptor on its CCleaner7 service (SYSTEM 0x201FD, Administrators
 * 0xF01FF, Interactive and Service 0x2008D): Windows does not check it when the service's own
 * process reports its status, Wine's dispatcher opened the service for SERVICE_SET_STATUS through
 * the descriptor, was refused, and the service never said it was running (start failed, 1053).
 *   probe test     installs the service (this exe), sets the descriptor, starts it, checks it runs, stops and deletes it
 *   probe service  is the service */
#include <windows.h>
#include <sddl.h>
#include <stdio.h>
#include <string.h>

static SERVICE_STATUS_HANDLE status_handle;
static SERVICE_STATUS status;
static HANDLE stop_event;

static DWORD WINAPI handler(DWORD control, DWORD type, void *data, void *ctx)
{
    if (control == SERVICE_CONTROL_STOP)
    {
        status.dwCurrentState = SERVICE_STOP_PENDING;
        SetServiceStatus(status_handle, &status);
        SetEvent(stop_event);
    }
    return NO_ERROR;
}

static void WINAPI service_main(DWORD argc, WCHAR **argv)
{
    status_handle = RegisterServiceCtrlHandlerExW(L"SgSetStatus", handler, NULL);
    status.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    status.dwControlsAccepted = SERVICE_ACCEPT_STOP;
    status.dwCurrentState = SERVICE_RUNNING;
    SetServiceStatus(status_handle, &status);
    WaitForSingleObject(stop_event, 60000);
    status.dwCurrentState = SERVICE_STOPPED;
    SetServiceStatus(status_handle, &status);
}

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

static int test(void)
{
    /* CCleaner 7's descriptor for its service */
    static const WCHAR sddl[] = L"D:(A;;0x201FD;;;SY)(A;;0xF01FF;;;BA)(A;;0x2008D;;;IU)(A;;0x2008D;;;SU)";
    WCHAR path[MAX_PATH + 16], buf[MAX_PATH];
    SC_HANDLE scm, svc;
    SERVICE_STATUS st;
    PSECURITY_DESCRIPTOR sd = NULL;
    char msg[160];
    int i;
    BOOL ok;

    GetModuleFileNameW(NULL, buf, MAX_PATH);
    lstrcpyW(path, L"\"");
    lstrcatW(path, buf);
    lstrcatW(path, L"\" service");
    scm = OpenSCManagerW(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if ((svc = OpenServiceW(scm, L"SgSetStatus", SERVICE_ALL_ACCESS)))
    {
        ControlService(svc, SERVICE_CONTROL_STOP, &st);
        DeleteService(svc);
        CloseServiceHandle(svc);
        Sleep(500);
    }
    svc = CreateServiceW(scm, L"SgSetStatus", L"SG set status probe", SERVICE_ALL_ACCESS, SERVICE_WIN32_OWN_PROCESS,
                         SERVICE_DEMAND_START, SERVICE_ERROR_IGNORE, path, NULL, NULL, NULL, NULL, NULL);
    check(svc != NULL, "the service is created");
    if (!svc) return 1;
    ConvertStringSecurityDescriptorToSecurityDescriptorW(sddl, SDDL_REVISION_1, &sd, NULL);
    check(SetServiceObjectSecurity(svc, DACL_SECURITY_INFORMATION, sd), "its descriptor grants SERVICE_SET_STATUS to nobody");
    LocalFree(sd);
    CloseServiceHandle(svc);

    svc = OpenServiceW(scm, L"SgSetStatus", SERVICE_START | SERVICE_QUERY_STATUS | SERVICE_STOP | DELETE);
    check(svc != NULL, "an administrator opens it to start, query, stop and delete");
    ok = StartServiceW(svc, 0, NULL);
    sprintf(msg, "StartService succeeds (error %lu)", ok ? 0 : GetLastError());
    check(ok, msg);
    for (i = 0; i < 50; i++)
    {
        if (QueryServiceStatus(svc, &st) && st.dwCurrentState == SERVICE_RUNNING) break;
        Sleep(100);
    }
    sprintf(msg, "the service says it is running (state %lu)", st.dwCurrentState);
    check(st.dwCurrentState == SERVICE_RUNNING, msg);
    ControlService(svc, SERVICE_CONTROL_STOP, &st);
    for (i = 0; i < 50; i++)
    {
        if (QueryServiceStatus(svc, &st) && st.dwCurrentState == SERVICE_STOPPED) break;
        Sleep(100);
    }
    check(st.dwCurrentState == SERVICE_STOPPED, "it stops when told to");
    DeleteService(svc);
    CloseServiceHandle(svc);
    CloseServiceHandle(scm);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}

int main(int argc, char **argv)
{
    static const SERVICE_TABLE_ENTRYW table[] = { { (WCHAR *)L"SgSetStatus", service_main }, { NULL, NULL } };

    if (argc > 1 && !strcmp(argv[1], "service"))
    {
        stop_event = CreateEventW(NULL, TRUE, FALSE, NULL);
        return !StartServiceCtrlDispatcherW(table);
    }
    return test();
}
