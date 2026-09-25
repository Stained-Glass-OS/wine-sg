/* scm-svc: a real Windows service for the gates (SCM access checks, sg-shell's
 * Services console): it starts, stops, pauses and continues when told to.
 * Its own name comes from the SCM. Install with
 *   sc create NAME binPath= "C:\...\scm-svc.exe" DisplayName= "..."
 *
 * Copyright 2026 Stained Glass OS contributors
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include <windows.h>

static SERVICE_STATUS_HANDLE handle;
static SERVICE_STATUS status;
static HANDLE stop_event;

static void set_state(DWORD state)
{
    status.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    status.dwCurrentState = state;
    status.dwControlsAccepted = state == SERVICE_START_PENDING ? 0 :
        SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_PAUSE_CONTINUE | SERVICE_ACCEPT_SHUTDOWN;
    status.dwWin32ExitCode = NO_ERROR;
    SetServiceStatus(handle, &status);
}

static DWORD WINAPI control(DWORD code, DWORD type, void *data, void *ctx)
{
    switch (code)
    {
    case SERVICE_CONTROL_STOP:
    case SERVICE_CONTROL_SHUTDOWN:
        set_state(SERVICE_STOP_PENDING);
        SetEvent(stop_event);
        return NO_ERROR;
    case SERVICE_CONTROL_PAUSE:
        set_state(SERVICE_PAUSED);
        return NO_ERROR;
    case SERVICE_CONTROL_CONTINUE:
        set_state(SERVICE_RUNNING);
        return NO_ERROR;
    case SERVICE_CONTROL_INTERROGATE:
        SetServiceStatus(handle, &status);
        return NO_ERROR;
    }
    return ERROR_CALL_NOT_IMPLEMENTED;
}

static void WINAPI service_main(DWORD argc, WCHAR **argv)
{
    stop_event = CreateEventW(NULL, TRUE, FALSE, NULL);
    handle = RegisterServiceCtrlHandlerExW(argc ? argv[0] : L"", control, NULL);
    if (!handle) return;
    set_state(SERVICE_START_PENDING);
    set_state(SERVICE_RUNNING);
    WaitForSingleObject(stop_event, INFINITE);
    set_state(SERVICE_STOPPED);
}

int wmain(void)
{
    SERVICE_TABLE_ENTRYW table[] = { { (WCHAR *)L"", service_main }, { NULL, NULL } };
    return StartServiceCtrlDispatcherW(table) ? 0 : 1;
}
