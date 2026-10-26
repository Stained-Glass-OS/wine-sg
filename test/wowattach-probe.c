/* wowattach-probe: a 64-bit debugger attaches to a running 32-bit (wow64) program
 * and detaches again (patches/sg/7002). The break-in thread DbgUiIssueRemoteBreakin
 * makes must run in the 32-bit program: it started at the 64-bit ntdll's address cut
 * to 32 bits and faulted, so attaching winedbg (or any 64-bit debugger) killed the
 * program.
 *   wowattach-probe.exe CHILD.exe     the debugger (64-bit build)
 *   wowattach-probe.exe child         the program attached to; exits 42 when told
 * Prints key=1|0 lines.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include <windows.h>
#include <stdio.h>
#include <string.h>

static int child(void)
{
    HANDLE ev = OpenEventA(SYNCHRONIZE, FALSE, "sg_wowattach_quit");
    if (!ev) return 3;
    WaitForSingleObject(ev, 60000);
    return 42;
}

static int attach(const char *exe, const char *tag)
{
    char cmd[MAX_PATH + 16];
    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    DEBUG_EVENT de;
    HANDLE quit = CreateEventA(NULL, TRUE, FALSE, "sg_wowattach_quit");
    BOOL wow = FALSE, created = FALSE, breakpoint = FALSE, crashed = FALSE;
    DWORD code = 0, start = GetTickCount(), exit_code = 0;

    sprintf(cmd, "\"%s\" child", exe);
    if (!CreateProcessA(NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) { printf("%s_spawn=0\n", tag); return 1; }
    Sleep(1500);
    IsWow64Process(pi.hProcess, &wow);
    printf("%s_child_wow64=%d\n", tag, wow);
    if (!DebugActiveProcess(pi.dwProcessId)) { printf("%s_attached=0 (error %lu)\n", tag, GetLastError()); return 1; }
    printf("%s_attached=1\n", tag);
    while (!breakpoint && !crashed && GetTickCount() - start < 20000 && WaitForDebugEvent(&de, 5000))
    {
        DWORD status = DBG_CONTINUE;
        if (de.dwDebugEventCode == CREATE_PROCESS_DEBUG_EVENT) { created = TRUE; CloseHandle(de.u.CreateProcessInfo.hFile); }
        else if (de.dwDebugEventCode == LOAD_DLL_DEBUG_EVENT) CloseHandle(de.u.LoadDll.hFile);
        else if (de.dwDebugEventCode == EXCEPTION_DEBUG_EVENT)
        {
            code = de.u.Exception.ExceptionRecord.ExceptionCode;
            if (code == EXCEPTION_BREAKPOINT || code == 0x4000001f /* STATUS_WX86_BREAKPOINT */) breakpoint = TRUE;
            else { crashed = TRUE; status = DBG_EXCEPTION_NOT_HANDLED; }
        }
        else if (de.dwDebugEventCode == EXIT_PROCESS_DEBUG_EVENT) crashed = TRUE;
        ContinueDebugEvent(de.dwProcessId, de.dwThreadId, status);
    }
    printf("%s_exception=%#lx\n", tag, code);
    printf("%s_create_event=%d\n", tag, created);
    printf("%s_breakin=%d\n", tag, breakpoint && !crashed);
    printf("%s_detached=%d\n", tag, DebugActiveProcessStop(pi.dwProcessId));
    Sleep(500);
    SetEvent(quit);
    if (WaitForSingleObject(pi.hProcess, 10000)) TerminateProcess(pi.hProcess, 1);
    GetExitCodeProcess(pi.hProcess, &exit_code);
    printf("%s_exit_code=%lu\n", tag, exit_code);
    printf("%s_survived=%d\n", tag, exit_code == 42);
    CloseHandle(pi.hProcess); CloseHandle(pi.hThread); CloseHandle(quit);
    return 0;
}

int main(int argc, char **argv)
{
    if (argc > 1 && !strcmp(argv[1], "child")) return child();
    if (argc > 2) { attach(argv[1], "wow64"); attach(argv[2], "native"); }
    printf("done=1\n");
    return 0;
}
