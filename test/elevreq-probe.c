/* elevreq-probe: for elevreq-gate.sh.
 *   elevreq-probe.exe create PROGRAM   CreateProcess, print "CREATE ok" or "CREATE err N"
 *   elevreq-probe.exe shell PROGRAM    ShellExecuteEx "open", print "SHELL ok|err N"
 * The PROGRAMs are this same source built with different manifests; run
 * without arguments they print "RAN" and exit 7. */
#include <windows.h>
#include <shellapi.h>
#include <stdio.h>

int main(int argc, char **argv)
{
    WCHAR prog[MAX_PATH];
    if (argc < 3) { printf("RAN\n"); return 7; }
    MultiByteToWideChar(CP_ACP, 0, argv[2], -1, prog, MAX_PATH);
    if (!strcmp(argv[1], "create"))
    {
        STARTUPINFOW si = { sizeof(si) }; PROCESS_INFORMATION pi; DWORD code = 0;
        if (CreateProcessW(prog, NULL, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
        {
            WaitForSingleObject(pi.hProcess, 20000); GetExitCodeProcess(pi.hProcess, &code);
            printf("CREATE ok %lu\n", code);
        }
        else printf("CREATE err %lu\n", GetLastError());
    }
    else
    {
        SHELLEXECUTEINFOW sei = { sizeof(sei) };
        sei.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_FLAG_NO_UI;
        sei.lpVerb = L"open"; sei.lpFile = prog; sei.nShow = SW_SHOWNORMAL;
        if (ShellExecuteExW(&sei)) { if (sei.hProcess) WaitForSingleObject(sei.hProcess, 20000); printf("SHELL ok\n"); }
        else printf("SHELL err %lu\n", GetLastError());
    }
    fflush(stdout);
    return 0;
}
