/* scmsingle-probe: one Service Control Manager (patches/sg/0530). A second
 * services.exe served the same \pipe\svcctl with a database of its own; a
 * service's status could reach it and its starter timed out (1053) --
 * Microsoft Office's Click-to-Run installer ended there. The probe starts
 * services.exe again while the SCM runs: it must leave at once
 * (ERROR_SERVICE_ALREADY_RUNNING), leaving one, which still starts a
 * service. name=value lines.
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#include <windows.h>
#include <tlhelp32.h>
#include <stdio.h>

static int count_services(void)
{
    PROCESSENTRY32W pe = { sizeof(pe) };
    HANDLE snap = CreateToolhelp32Snapshot( TH32CS_SNAPPROCESS, 0 );
    int n = 0;
    if (Process32FirstW( snap, &pe ))
        do if (!lstrcmpiW( pe.szExeFile, L"services.exe" )) n++; while (Process32NextW( snap, &pe ));
    CloseHandle( snap );
    return n;
}

int main(void)
{
    WCHAR cmd[] = L"C:\\windows\\system32\\services.exe";
    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    SC_HANDLE scm, svc;
    SERVICE_STATUS st = { 0 };
    DWORD code = 0, wait, i;

    printf( "before=%d\n", count_services() );
    if (!CreateProcessW( NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi ))
    {
        printf( "create=%lu\n", GetLastError() );
        return 0;
    }
    wait = WaitForSingleObject( pi.hProcess, 15000 );
    GetExitCodeProcess( pi.hProcess, &code );
    printf( "second=%s %lu\n", wait == WAIT_OBJECT_0 ? "exited" : "running", code );
    if (wait != WAIT_OBJECT_0) Sleep( 2000 );
    printf( "after=%d\n", count_services() );

    /* the SCM that stays still starts a service */
    scm = OpenSCManagerW( NULL, NULL, SC_MANAGER_ALL_ACCESS );
    svc = OpenServiceW( scm, L"MSIServer", SERVICE_START | SERVICE_QUERY_STATUS | SERVICE_STOP );
    printf( "start=%d\n", StartServiceW( svc, 0, NULL ) || GetLastError() == ERROR_SERVICE_ALREADY_RUNNING );
    for (i = 0; i < 50; i++)
    {
        QueryServiceStatus( svc, &st );
        if (st.dwCurrentState == SERVICE_RUNNING) break;
        Sleep( 200 );
    }
    printf( "state=%lu\n", st.dwCurrentState );
    ControlService( svc, SERVICE_CONTROL_STOP, &st );
    if (wait != WAIT_OBJECT_0) TerminateProcess( pi.hProcess, 0 );
    return 0;
}
