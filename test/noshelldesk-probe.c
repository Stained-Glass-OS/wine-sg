/* noshelldesk-probe (wine-sg 9803): a process that may start no others -- Chromium's sandboxed renderer:
 * its child process policy is PROCESS_CREATION_CHILD_PROCESS_RESTRICTED, on an alternate desktop --
 * still has a desktop window, made with no shell behind it (as win32k makes every desktop window on
 * Windows), and asking for it is no error. Without an argument the probe makes the desktop and starts
 * itself "child" there with that policy; the child prints the lines. */
#include <windows.h>
#include <stdio.h>

#ifndef PROC_THREAD_ATTRIBUTE_CHILD_PROCESS_POLICY
#define PROC_THREAD_ATTRIBUTE_CHILD_PROCESS_POLICY 0x0002000e
#endif
#ifndef PROCESS_CREATION_CHILD_PROCESS_RESTRICTED
#define PROCESS_CREATION_CHILD_PROCESS_RESTRICTED 0x01
#endif

static int child(void)
{
    WCHAR name[64] = L"";
    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    HWND desk = GetDesktopWindow();
    RECT rc = {0};
    BOOL ok;

    GetUserObjectInformationW( GetThreadDesktop( GetCurrentThreadId() ), UOI_NAME, name, sizeof(name), NULL );
    printf( "child_desktop=%ls\n", name );
    printf( "desktop_window=%d %d\n", desk != NULL, IsWindow( desk ) );
    GetWindowRect( desk, &rc );
    printf( "desktop_rect=%d\n", rc.right > rc.left && rc.bottom > rc.top );
    ok = CreateProcessW( L"C:\\windows\\system32\\cmd.exe", (WCHAR *)L"cmd /c exit", NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi );
    printf( "child_create=%d %lu\n", ok, ok ? 0 : GetLastError() );
    fflush( stdout );
    return 0;
}

int main(int argc, char **argv)
{
    WCHAR self[MAX_PATH], cmd[MAX_PATH + 16], desktop[] = L"WinSta0\\sg-noshell";
    STARTUPINFOEXW si = { { sizeof(si) } };
    PROCESS_INFORMATION pi;
    DWORD policy = PROCESS_CREATION_CHILD_PROCESS_RESTRICTED, code = 1;
    SIZE_T size = 0;
    HDESK hdesk;

    if (argc > 1 && !strcmp( argv[1], "child" )) return child();

    hdesk = CreateDesktopW( L"sg-noshell", NULL, NULL, 0, GENERIC_ALL, NULL );
    printf( "create_desktop=%d\n", hdesk != NULL );
    GetModuleFileNameW( NULL, self, MAX_PATH );
    swprintf( cmd, ARRAYSIZE(cmd), L"\"%ls\" child", self );
    InitializeProcThreadAttributeList( NULL, 1, 0, &size );
    si.lpAttributeList = HeapAlloc( GetProcessHeap(), 0, size );
    InitializeProcThreadAttributeList( si.lpAttributeList, 1, 0, &size );
    UpdateProcThreadAttribute( si.lpAttributeList, 0, PROC_THREAD_ATTRIBUTE_CHILD_PROCESS_POLICY, &policy, sizeof(policy), NULL, NULL );
    si.StartupInfo.lpDesktop = desktop;
    si.StartupInfo.hStdOutput = GetStdHandle( STD_OUTPUT_HANDLE );
    si.StartupInfo.hStdError = GetStdHandle( STD_ERROR_HANDLE );
    si.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
    if (!CreateProcessW( self, cmd, NULL, NULL, TRUE, EXTENDED_STARTUPINFO_PRESENT, NULL, NULL, &si.StartupInfo, &pi ))
    {
        printf( "start_child=0 %lu\n", GetLastError() );
        return 1;
    }
    WaitForSingleObject( pi.hProcess, 60000 );
    GetExitCodeProcess( pi.hProcess, &code );
    printf( "child_exit=%lu\n", code );
    return 0;
}
