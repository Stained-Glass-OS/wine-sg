/* Application restart and recovery (patches/sg/1624).
 *
 *   apprestart-probe.exe [OTHER.exe]   the registrations, this process's
 *                                      and another's (OTHER, else itself)
 *   apprestart-probe.exe child         registers, signals, waits
 *   apprestart-probe.exe crash DIR SECONDS
 *                                      registers, waits SECONDS, crashes:
 *                                      the recovery callback writes
 *                                      DIR\recovered; a restart runs
 *                                      "restarted DIR", writing DIR\restarted
 *
 * RegisterApplicationRestart and RegisterApplicationRecoveryCallback kept
 * nothing; GetApplicationRestartSettings was E_NOTIMPL;
 * GetApplicationRecoveryCallback and UnregisterApplicationRecoveryCallback
 * were missing; a crash neither recovered nor restarted the program.
 */
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

typedef HRESULT (WINAPI *reg_restart_t)(const WCHAR *, DWORD);
typedef HRESULT (WINAPI *get_restart_t)(HANDLE, WCHAR *, DWORD *, DWORD *);
typedef HRESULT (WINAPI *reg_recovery_t)(APPLICATION_RECOVERY_CALLBACK, void *, DWORD, DWORD);
typedef HRESULT (WINAPI *get_recovery_t)(HANDLE, APPLICATION_RECOVERY_CALLBACK *, void **, DWORD *, DWORD *);
typedef HRESULT (WINAPI *unreg_t)(void);
typedef HRESULT (WINAPI *in_progress_t)(BOOL *);
typedef void (WINAPI *finished_t)(BOOL);

static reg_restart_t pRegisterApplicationRestart;
static get_restart_t pGetApplicationRestartSettings;
static reg_recovery_t pRegisterApplicationRecoveryCallback;
static get_recovery_t pGetApplicationRecoveryCallback;
static unreg_t pUnregisterApplicationRestart, pUnregisterApplicationRecoveryCallback;
static in_progress_t pApplicationRecoveryInProgress;
static finished_t pApplicationRecoveryFinished;
static int failures;
static WCHAR dirW[MAX_PATH];

static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

static void touch(const WCHAR *name)
{
    WCHAR path[MAX_PATH];
    HANDLE f;

    swprintf(path, MAX_PATH, L"%ls\\%ls", dirW, name);
    f = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    if (f != INVALID_HANDLE_VALUE) CloseHandle(f);
}

static DWORD WINAPI recovery_cb(void *param)
{
    BOOL canceled = TRUE;

    if (pApplicationRecoveryInProgress(&canceled) == S_OK && !canceled) touch(L"recovered");
    pApplicationRecoveryFinished(TRUE);
    return 0;
}

static DWORD WINAPI other_cb(void *param) { return 0; }

int main(int argc, char **argv)
{
    HMODULE k32 = GetModuleHandleA("kernel32.dll");
    WCHAR buf[64], *cmdW = GetCommandLineW();
    DWORD size, flags, ping;
    APPLICATION_RECOVERY_CALLBACK cb;
    void *param;
    HRESULT hr;
    int i;

    pRegisterApplicationRestart = (void *)GetProcAddress(k32, "RegisterApplicationRestart");
    pGetApplicationRestartSettings = (void *)GetProcAddress(k32, "GetApplicationRestartSettings");
    pRegisterApplicationRecoveryCallback = (void *)GetProcAddress(k32, "RegisterApplicationRecoveryCallback");
    pGetApplicationRecoveryCallback = (void *)GetProcAddress(k32, "GetApplicationRecoveryCallback");
    pUnregisterApplicationRestart = (void *)GetProcAddress(k32, "UnregisterApplicationRestart");
    pUnregisterApplicationRecoveryCallback = (void *)GetProcAddress(k32, "UnregisterApplicationRecoveryCallback");
    pApplicationRecoveryInProgress = (void *)GetProcAddress(k32, "ApplicationRecoveryInProgress");
    pApplicationRecoveryFinished = (void *)GetProcAddress(k32, "ApplicationRecoveryFinished");

    if (argc >= 3 && !strcmp(argv[1], "restarted"))
    {
        MultiByteToWideChar(CP_ACP, 0, argv[2], -1, dirW, MAX_PATH);
        touch(L"restarted");
        return 0;
    }
    if (argc >= 4 && !strcmp(argv[1], "crash"))
    {
        WCHAR line[MAX_PATH + 16];

        MultiByteToWideChar(CP_ACP, 0, argv[2], -1, dirW, MAX_PATH);
        swprintf(line, ARRAY_SIZE(line), L"restarted %ls", dirW);
        pRegisterApplicationRestart(line, 0);
        pRegisterApplicationRecoveryCallback(recovery_cb, NULL, 0, 0);
        SetErrorMode(SEM_NOGPFAULTERRORBOX);
        Sleep(atoi(argv[3]) * 1000);
        *(volatile int *)0 = 1;
        return 0;
    }
    if (argc >= 2 && !strcmp(argv[1], "child"))
    {
        HANDLE ready = OpenEventA(EVENT_MODIFY_STATE, FALSE, "sg_apprestart_ready");

        pRegisterApplicationRestart(L"/from-child", RESTART_NO_REBOOT);
        pRegisterApplicationRecoveryCallback(other_cb, (void *)0x1234, 30000, 0);
        SetEvent(ready);
        Sleep(60000);
        return 0;
    }
    if (!pGetApplicationRecoveryCallback || !pUnregisterApplicationRecoveryCallback)
    {
        check(0, "kernel32 exports GetApplicationRecoveryCallback and UnregisterApplicationRecoveryCallback");
        printf("RESULT: FAIL\n");
        return 1;
    }

    /* this process */
    size = ARRAY_SIZE(buf);
    hr = pGetApplicationRestartSettings(GetCurrentProcess(), buf, &size, &flags);
    check(hr == HRESULT_FROM_WIN32(ERROR_NOT_FOUND), "nothing registered: ERROR_NOT_FOUND");
    check(pRegisterApplicationRestart(L"x", 0x10) == E_INVALIDARG, "unknown restart flags are refused");
    {
        static WCHAR big[1100];
        for (i = 0; i < 1050; i++) big[i] = 'a';
        check(pRegisterApplicationRestart(big, 0) == E_INVALIDARG, "a command line of 1024 or more is refused");
    }
    hr = pRegisterApplicationRestart(L"/restarted 7", RESTART_NO_PATCH);
    check(hr == S_OK, "RegisterApplicationRestart");
    size = 0;
    hr = pGetApplicationRestartSettings(GetCurrentProcess(), NULL, &size, NULL);
    check(hr == S_OK && size == 13, "GetApplicationRestartSettings(NULL) gives the length with its terminator");
    size = 4;
    hr = pGetApplicationRestartSettings(GetCurrentProcess(), buf, &size, &flags);
    check(hr == HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER) && size == 13, "... a short buffer: ERROR_INSUFFICIENT_BUFFER");
    size = ARRAY_SIZE(buf);
    flags = 0xdead;
    hr = pGetApplicationRestartSettings(GetCurrentProcess(), buf, &size, &flags);
    printf("restart settings: %#lx %ls %lu %#lx\n", hr, buf, size, flags);
    check(hr == S_OK && !wcscmp(buf, L"/restarted 7") && flags == RESTART_NO_PATCH, "... the command line and flags read back");
    pUnregisterApplicationRestart();
    size = ARRAY_SIZE(buf);
    check(pGetApplicationRestartSettings(GetCurrentProcess(), buf, &size, &flags) == HRESULT_FROM_WIN32(ERROR_NOT_FOUND),
          "UnregisterApplicationRestart forgets it");

    check(pRegisterApplicationRecoveryCallback(recovery_cb, NULL, RECOVERY_MAX_PING_INTERVAL + 1, 0) == E_INVALIDARG,
          "a ping interval over five minutes is refused");
    check(pRegisterApplicationRecoveryCallback(recovery_cb, (void *)0x42, 0, 0) == S_OK, "RegisterApplicationRecoveryCallback");
    hr = pGetApplicationRecoveryCallback(GetCurrentProcess(), &cb, &param, &ping, &flags);
    check(hr == S_OK && cb == recovery_cb && param == (void *)0x42 && ping == RECOVERY_DEFAULT_PING_INTERVAL,
          "GetApplicationRecoveryCallback: the callback, parameter and default ping interval");
    pUnregisterApplicationRecoveryCallback();
    check(pGetApplicationRecoveryCallback(GetCurrentProcess(), &cb, &param, &ping, &flags) == HRESULT_FROM_WIN32(ERROR_NOT_FOUND),
          "UnregisterApplicationRecoveryCallback forgets it");
    {
        BOOL canceled = 7;
        check(pApplicationRecoveryInProgress(&canceled) == E_FAIL && canceled == 7, "ApplicationRecoveryInProgress outside a recovery fails");
    }

    /* another process (OTHER, or this program again) */
    {
        STARTUPINFOW si = { sizeof(si) };
        PROCESS_INFORMATION pi;
        HANDLE ready = CreateEventA(NULL, TRUE, FALSE, "sg_apprestart_ready");
        WCHAR exe[MAX_PATH], line[MAX_PATH + 16];

        if (argc >= 2) MultiByteToWideChar(CP_ACP, 0, argv[1], -1, exe, MAX_PATH);
        else GetModuleFileNameW(NULL, exe, MAX_PATH);
        swprintf(line, ARRAY_SIZE(line), L"\"%ls\" child", exe);
        if (!CreateProcessW(exe, line, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
            check(0, "the other process started");
        else
        {
            check(WaitForSingleObject(ready, 30000) == WAIT_OBJECT_0, "the other process registered");
            size = ARRAY_SIZE(buf);
            flags = 0;
            hr = pGetApplicationRestartSettings(pi.hProcess, buf, &size, &flags);
            printf("other process: %#lx %ls %#lx\n", hr, hr == S_OK ? buf : L"", flags);
            check(hr == S_OK && !wcscmp(buf, L"/from-child") && flags == RESTART_NO_REBOOT,
                  "GetApplicationRestartSettings reads another process's");
            hr = pGetApplicationRecoveryCallback(pi.hProcess, &cb, &param, &ping, &flags);
            check(hr == S_OK && param == (void *)0x1234 && ping == 30000, "GetApplicationRecoveryCallback reads another process's");
            TerminateProcess(pi.hProcess, 0);
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
        }
    }

    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
