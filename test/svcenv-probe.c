/* A service's own environment variables (patches/sg/5963): a service's key
 * may carry Environment (REG_MULTI_SZ of "NAME=value"), which Windows adds to
 * the environment it starts the service with. Wine's services.exe gave every
 * service the same block. "svcenv-probe.exe" makes SGSvcEnv (with variables)
 * and SGSvcPlain (without), starts both and reads what each wrote;
 * "svcenv-probe.exe svc FILE" is the service. */
#include <windows.h>
#include <stdio.h>
#include <string.h>

static int failures;
static SERVICE_STATUS_HANDLE status_handle;
static SERVICE_STATUS status = { SERVICE_WIN32_OWN_PROCESS };
static char out_file[MAX_PATH];

static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) failures++;
}

static DWORD WINAPI handler(DWORD control, DWORD type, void *data, void *ctx)
{
    return NO_ERROR;
}

static void WINAPI service_main(DWORD argc, char **argv)
{
    char val[256];
    FILE *f;
    const char *names[] = { "SG_SVC_VAR", "SG_SVC_EQ", "SystemRoot" };
    int i;

    status_handle = RegisterServiceCtrlHandlerExA("", handler, NULL);
    status.dwCurrentState = SERVICE_RUNNING;
    SetServiceStatus(status_handle, &status);
    if ((f = fopen(out_file, "w")))
    {
        for (i = 0; i < 3; i++)
            fprintf(f, "%s=%s\n", names[i], GetEnvironmentVariableA(names[i], val, sizeof(val)) ? val : "(none)");
        fclose(f);
    }
    status.dwCurrentState = SERVICE_STOPPED;
    SetServiceStatus(status_handle, &status);
}

static SC_HANDLE make(SC_HANDLE scm, const char *name, const char *exe, const char *file)
{
    char cmd[MAX_PATH * 2];
    SC_HANDLE s = OpenServiceA(scm, name, DELETE);
    if (s) { DeleteService(s); CloseServiceHandle(s); }
    snprintf(cmd, sizeof(cmd), "\"%s\" svc %s", exe, file);
    return CreateServiceA(scm, name, name, SERVICE_ALL_ACCESS, SERVICE_WIN32_OWN_PROCESS, SERVICE_DEMAND_START,
                          SERVICE_ERROR_IGNORE, cmd, NULL, NULL, NULL, NULL, NULL);
}

/* the service's output, once it has written it */
static int run(SC_HANDLE s, const char *file, char *text, int size)
{
    FILE *f;
    int n = 0, i;
    DeleteFileA(file);
    if (!StartServiceA(s, 0, NULL)) { printf("StartService failed %lu\n", GetLastError()); return 0; }
    for (i = 0; i < 200 && GetFileAttributesA(file) == INVALID_FILE_ATTRIBUTES; i++) Sleep(100);
    Sleep(300);
    if (!(f = fopen(file, "r"))) return 0;
    n = fread(text, 1, size - 1, f);
    text[n] = 0;
    fclose(f);
    printf("%s", text);
    return 1;
}

int main(int argc, char **argv)
{
    static const WCHAR vars[] = L"SG_SVC_VAR=hello\0SG_SVC_EQ=a=b\0";
    SERVICE_TABLE_ENTRYA table[] = { { (char *)"", service_main }, { NULL, NULL } };
    char exe[MAX_PATH], text[1024];
    SC_HANDLE scm, env, plain;
    HKEY key;

    if (argc > 2 && !strcmp(argv[1], "svc"))
    {
        lstrcpynA(out_file, argv[2], sizeof(out_file));
        return !StartServiceCtrlDispatcherA(table);
    }
    GetModuleFileNameA(NULL, exe, sizeof(exe));
    scm = OpenSCManagerA(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    env = make(scm, "SGSvcEnv", exe, "C:\\svcenv-env.txt");
    plain = make(scm, "SGSvcPlain", exe, "C:\\svcenv-plain.txt");
    if (!env || !plain) { printf("FAIL  services not made (%lu)\nRESULT: FAIL\n", GetLastError()); return 1; }
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "System\\CurrentControlSet\\Services\\SGSvcEnv", 0, KEY_SET_VALUE, &key) ||
        RegSetValueExW(key, L"Environment", 0, REG_MULTI_SZ, (const BYTE *)vars, sizeof(vars)))
    { printf("FAIL  Environment not set\nRESULT: FAIL\n"); return 1; }
    RegCloseKey(key);

    check(run(env, "C:\\svcenv-env.txt", text, sizeof(text)), "SGSvcEnv ran");
    check(strstr(text, "SG_SVC_VAR=hello\n") != NULL, "SGSvcEnv has its own SG_SVC_VAR=hello");
    check(strstr(text, "SG_SVC_EQ=a=b\n") != NULL, "... and SG_SVC_EQ=a=b (an '=' in the value)");
    check(strstr(text, "SystemRoot=(none)") == NULL && strstr(text, "SystemRoot=") != NULL,
          "... and still every service's variables (SystemRoot)");
    check(run(plain, "C:\\svcenv-plain.txt", text, sizeof(text)), "SGSvcPlain ran");
    check(strstr(text, "SG_SVC_VAR=(none)") != NULL, "SGSvcPlain has none of SGSvcEnv's variables");
    check(strstr(text, "SystemRoot=(none)") == NULL, "... but has every service's (SystemRoot)");

    DeleteService(env); DeleteService(plain);
    CloseServiceHandle(env); CloseServiceHandle(plain); CloseServiceHandle(scm);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
