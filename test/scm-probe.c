/* scm-probe: one SCM operation, its Win32 result printed as "RESULT <op> <err>".
 *
 *   scm-probe enum                    OpenSCManager(ENUMERATE) + EnumServicesStatusEx
 *   scm-probe query NAME              OpenService(QUERY_STATUS|QUERY_CONFIG) + QueryServiceStatus
 *   scm-probe open NAME HEXACCESS     OpenService with that access
 *   scm-probe start|stop|pause|continue NAME
 *   scm-probe config NAME auto|demand|disabled
 *   scm-probe delete NAME
 *   scm-probe create NAME PATH
 *   scm-probe state NAME              prints "STATE <n>"
 *   scm-probe sleep                   keeps the prefix's services alive
 *
 * Copyright 2026 Stained Glass OS contributors
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static SC_HANDLE scm(DWORD access)
{
    return OpenSCManagerA(NULL, NULL, access);
}

int main(int argc, char **argv)
{
    const char *op = argc > 1 ? argv[1] : "";
    const char *name = argc > 2 ? argv[2] : "";
    SC_HANDLE m, s = NULL;
    DWORD err = 0;
    SERVICE_STATUS st;

    if (!strcmp(op, "sleep")) { Sleep(INFINITE); return 0; }
    if (!strcmp(op, "enum"))
    {
        DWORD needed = 0, count = 0, resume = 0;
        BYTE *buf;
        if (!(m = scm(SC_MANAGER_ENUMERATE_SERVICE))) err = GetLastError();
        else
        {
            EnumServicesStatusExA(m, SC_ENUM_PROCESS_INFO, SERVICE_WIN32, SERVICE_STATE_ALL, NULL, 0, &needed, &count, &resume, NULL);
            buf = malloc(needed);
            resume = 0;
            if (!EnumServicesStatusExA(m, SC_ENUM_PROCESS_INFO, SERVICE_WIN32, SERVICE_STATE_ALL, buf, needed, &needed, &count, &resume, NULL))
                err = GetLastError();
            printf("COUNT %lu\n", count);
        }
        printf("RESULT enum %lu\n", err);
        return 0;
    }
    if (!strcmp(op, "create"))
    {
        if (!(m = scm(SC_MANAGER_CREATE_SERVICE))) err = GetLastError();
        else if (!(s = CreateServiceA(m, name, name, SERVICE_ALL_ACCESS, SERVICE_WIN32_OWN_PROCESS, SERVICE_DEMAND_START,
                                      SERVICE_ERROR_NORMAL, argv[3], NULL, NULL, NULL, NULL, NULL)))
            err = GetLastError();
        printf("RESULT create %lu\n", err);
        return 0;
    }
    if (!(m = scm(SC_MANAGER_CONNECT))) { printf("RESULT %s %lu\n", op, GetLastError()); return 0; }

    if (!strcmp(op, "query") || !strcmp(op, "state"))
    {
        if (!(s = OpenServiceA(m, name, SERVICE_QUERY_STATUS | SERVICE_QUERY_CONFIG))) err = GetLastError();
        else if (!QueryServiceStatus(s, &st)) err = GetLastError();
        else if (!strcmp(op, "state")) printf("STATE %lu\n", st.dwCurrentState);
    }
    else if (!strcmp(op, "open"))
    {
        if (!(s = OpenServiceA(m, name, strtoul(argv[3], NULL, 16)))) err = GetLastError();
    }
    else if (!strcmp(op, "start"))
    {
        if (!(s = OpenServiceA(m, name, SERVICE_START))) err = GetLastError();
        else if (!StartServiceA(s, 0, NULL)) err = GetLastError();
    }
    else if (!strcmp(op, "stop") || !strcmp(op, "pause") || !strcmp(op, "continue"))
    {
        DWORD code = !strcmp(op, "stop") ? SERVICE_CONTROL_STOP : !strcmp(op, "pause") ? SERVICE_CONTROL_PAUSE : SERVICE_CONTROL_CONTINUE;
        DWORD access = code == SERVICE_CONTROL_STOP ? SERVICE_STOP : SERVICE_PAUSE_CONTINUE;
        if (!(s = OpenServiceA(m, name, access))) err = GetLastError();
        else if (!ControlService(s, code, &st)) err = GetLastError();
    }
    else if (!strcmp(op, "config"))
    {
        DWORD type = !strcmp(argv[3], "auto") ? SERVICE_AUTO_START : !strcmp(argv[3], "disabled") ? SERVICE_DISABLED : SERVICE_DEMAND_START;
        if (!(s = OpenServiceA(m, name, SERVICE_CHANGE_CONFIG))) err = GetLastError();
        else if (!ChangeServiceConfigA(s, SERVICE_NO_CHANGE, type, SERVICE_NO_CHANGE, NULL, NULL, NULL, NULL, NULL, NULL, NULL))
            err = GetLastError();
    }
    else if (!strcmp(op, "delete"))
    {
        if (!(s = OpenServiceA(m, name, DELETE))) err = GetLastError();
        else if (!DeleteService(s)) err = GetLastError();
    }
    else err = ERROR_INVALID_PARAMETER;
    printf("RESULT %s %lu\n", op, err);
    return 0;
}
