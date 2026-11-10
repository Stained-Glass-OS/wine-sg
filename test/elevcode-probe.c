/* elevcode gate probe (wine-sg 7729): an elevated program's full exit code.
 *   elevcode-probe.exe runas CODE     ShellExecuteEx "runas" this program
 *                                     with "exit CODE", wait, print its code
 *   elevcode-probe.exe exit CODE      (the elevated side) end with CODE, after
 *                                     checking that neither it nor a child of
 *                                     it sees SG_EXIT_CODE_FILE (99 if one does)
 *   elevcode-probe.exe envcheck       end with 5 if SG_EXIT_CODE_FILE is set */
#include <windows.h>
#include <shellapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int sees_var(void)
{
    return GetEnvironmentVariableA("SG_EXIT_CODE_FILE", NULL, 0) != 0;
}

int main(int argc, char **argv)
{
    WCHAR self[MAX_PATH], params[64];

    if (argc > 1 && !strcmp(argv[1], "envcheck")) return sees_var() ? 5 : 0;
    if (argc > 2 && !strcmp(argv[1], "exit"))
    {
        STARTUPINFOW si = { sizeof(si) };
        PROCESS_INFORMATION pi;
        WCHAR cmd[MAX_PATH + 16];
        DWORD child = 0;

        if (sees_var()) return 99;
        GetModuleFileNameW(NULL, self, MAX_PATH);
        lstrcpyW(cmd, L"\""); lstrcatW(cmd, self); lstrcatW(cmd, L"\" envcheck");
        if (CreateProcessW(NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
        {
            WaitForSingleObject(pi.hProcess, 30000);
            GetExitCodeProcess(pi.hProcess, &child);
            CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
            if (child) return 99;
        }
        return (int)strtoul(argv[2], NULL, 10);
    }
    if (argc > 2 && !strcmp(argv[1], "runas"))
    {
        SHELLEXECUTEINFOW sei = { sizeof(sei) };
        DWORD code = 0xdeadbeef;

        GetModuleFileNameW(NULL, self, MAX_PATH);
        lstrcpyW(params, L"exit "); MultiByteToWideChar(CP_ACP, 0, argv[2], -1, params + 5, ARRAYSIZE(params) - 5);
        sei.fMask = SEE_MASK_NOCLOSEPROCESS;
        sei.lpVerb = L"runas";
        sei.lpFile = self;
        sei.lpParameters = params;
        sei.nShow = SW_SHOWNORMAL;
        if (!ShellExecuteExW(&sei) || !sei.hProcess) { printf("RUNAS failed %lu\n", GetLastError()); return 1; }
        WaitForSingleObject(sei.hProcess, 120000);
        GetExitCodeProcess(sei.hProcess, &code);
        printf("RUNAS code %lu\n", code);
        return 0;
    }
    return 2;
}
