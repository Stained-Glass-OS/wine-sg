/* Power requests and execution state (patches/sg/1678), run by
 * test/powerreq-gate.sh against a stand-in org.freedesktop.ScreenSaver
 * service whose state file is named by SG_POWERREQ_STATE (a Windows path):
 * a set power request, or a continuous ES_DISPLAY_REQUIRED/
 * ES_SYSTEM_REQUIRED, holds an Inhibit until it is cleared; a one-off
 * execution state is SimulateUserActivity; a process that exits holding
 * one lets it go. These were stubs. */
#include <windows.h>
#include <stdio.h>
#include <string.h>

static int failures;
static char state_path[MAX_PATH];

static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

static int value(const char *key)
{
    char line[256];
    int v = -1;
    FILE *f = fopen(state_path, "r");

    if (!f) return -1;
    while (fgets(line, sizeof(line), f))
        if (!strncmp(line, key, strlen(key)) && line[strlen(key)] == ' ') v = atoi(line + strlen(key) + 1);
    fclose(f);
    return v;
}

/* the service is told asynchronously of a process going: wait for it */
static int wait_value(const char *key, int want)
{
    int i;
    for (i = 0; i < 50; i++)
    {
        if (value(key) == want) return 1;
        Sleep(100);
    }
    return value(key) == want;
}

static int child(void)
{
    REASON_CONTEXT ctx = { POWER_REQUEST_CONTEXT_VERSION, POWER_REQUEST_CONTEXT_SIMPLE_STRING };
    HANDLE req;

    ctx.Reason.SimpleReasonString = (WCHAR *)L"child";
    req = PowerCreateRequest(&ctx);
    PowerSetRequest(req, PowerRequestDisplayRequired);
    return value("held") == 1 ? 0 : 3;  /* and exits holding it */
}

int main(int argc, char **argv)
{
    REASON_CONTEXT ctx = { POWER_REQUEST_CONTEXT_VERSION, POWER_REQUEST_CONTEXT_SIMPLE_STRING };
    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    char cmd[MAX_PATH + 16];
    EXECUTION_STATE old;
    DWORD code = 99;
    HANDLE req;
    int act;

    if (!GetEnvironmentVariableA("SG_POWERREQ_STATE", state_path, sizeof(state_path)))
    {
        printf("FAIL  no SG_POWERREQ_STATE\n");
        return 1;
    }
    if (argc > 1 && !strcmp(argv[1], "child")) return child();

    check(value("held") == 0, "nothing held at the start");

    ctx.Reason.SimpleReasonString = (WCHAR *)L"playing a video";
    req = PowerCreateRequest(&ctx);
    check(req != INVALID_HANDLE_VALUE && req, "PowerCreateRequest");
    check(value("held") == 0, "a request made but not set holds nothing");
    check(PowerSetRequest(req, PowerRequestDisplayRequired), "PowerSetRequest(display)");
    check(value("held") == 1, "it holds the screen on (Inhibit)");
    check(PowerSetRequest(req, PowerRequestSystemRequired), "PowerSetRequest(system)");
    check(value("held") == 1, "sharing the one inhibition");
    check(PowerClearRequest(req, PowerRequestDisplayRequired), "PowerClearRequest(display)");
    check(value("held") == 1, "the system request still holds it");
    check(PowerClearRequest(req, PowerRequestSystemRequired) && value("held") == 0, "both cleared: released (UnInhibit)");
    SetLastError(0xdeadbeef);
    check(!PowerClearRequest(req, PowerRequestSystemRequired) && GetLastError() == ERROR_INVALID_PARAMETER,
          "clearing what is not set: ERROR_INVALID_PARAMETER");
    check(!PowerSetRequest(req, 9) && GetLastError() == ERROR_INVALID_PARAMETER, "an unknown type: ERROR_INVALID_PARAMETER");
    SetLastError(0xdeadbeef);
    check(PowerCreateRequest(NULL) == INVALID_HANDLE_VALUE && GetLastError() == ERROR_INVALID_PARAMETER,
          "PowerCreateRequest(NULL): ERROR_INVALID_PARAMETER");
    CloseHandle(req);

    /* closing a request's handle ends it, as on Windows */
    req = PowerCreateRequest(&ctx);
    PowerSetRequest(req, PowerRequestDisplayRequired);
    check(value("held") == 1, "a request set again holds it");
    CloseHandle(req);
    check(value("held") == 0, "CloseHandle on the request releases it");

    /* execution state */
    old = SetThreadExecutionState(ES_CONTINUOUS | ES_DISPLAY_REQUIRED);
    check(old != 0 && value("held") == 1, "SetThreadExecutionState(ES_CONTINUOUS | ES_DISPLAY_REQUIRED) holds it");
    old = SetThreadExecutionState(ES_CONTINUOUS);
    check(old == (ES_CONTINUOUS | ES_DISPLAY_REQUIRED) && value("held") == 0, "ES_CONTINUOUS alone releases it");
    act = value("activity");
    SetThreadExecutionState(ES_DISPLAY_REQUIRED);
    check(value("activity") == act + 1 && value("held") == 0, "a one-off ES_DISPLAY_REQUIRED resets the idle time");

    /* a process that exits holding one */
    GetModuleFileNameA(NULL, cmd + 1, MAX_PATH);
    cmd[0] = '"';
    strcat(cmd, "\" child");
    if (CreateProcessA(NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
    {
        WaitForSingleObject(pi.hProcess, 30000);
        GetExitCodeProcess(pi.hProcess, &code);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
    check(code == 0, "a child's request held the screen on");
    check(wait_value("held", 0), "and went with the child");

    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
