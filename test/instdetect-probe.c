/* instdetect-probe: for instdetect-gate.sh (wine-sg 9000, Installer Detection).
 *   instdetect-probe.exe create PROGRAM     CreateProcess, print "CREATE ok N" or "CREATE err N"
 *   instdetect-probe.exe createsvc PROGRAM  the same onto a service's window station
 *                                           (lpDesktop __wineservice_winstation\Default)
 *   instdetect-probe.exe shell PROGRAM      ShellExecuteEx "open", print "SHELL ok|err N"
 * Run without arguments it exits 7 (the gate's 32-bit programs are built from it too). */
#include <windows.h>
#include <shellapi.h>
#include <stdio.h>

int main(int argc, char **argv)
{
    WCHAR prog[MAX_PATH];
    if (argc < 3) return 7;
    MultiByteToWideChar(CP_ACP, 0, argv[2], -1, prog, MAX_PATH);
    if (!strcmp(argv[1], "create") || !strcmp(argv[1], "createsvc"))
    {
        STARTUPINFOW si = { sizeof(si) }; PROCESS_INFORMATION pi; DWORD code = 0;
        if (!strcmp(argv[1], "createsvc")) si.lpDesktop = (WCHAR *)L"__wineservice_winstation\\Default";
        if (CreateProcessW(prog, NULL, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
        {
            WaitForSingleObject(pi.hProcess, 20000); GetExitCodeProcess(pi.hProcess, &code);
            printf("CREATE ok %lu\n", code);
        }
        else printf("CREATE err %lu\n", GetLastError());
    }
    else if (!strcmp(argv[1], "shell"))
    {
        SHELLEXECUTEINFOW sei = { sizeof(sei) };
        sei.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_FLAG_NO_UI;
        sei.lpFile = prog; sei.nShow = SW_SHOWNORMAL;
        if (ShellExecuteExW(&sei)) { if (sei.hProcess) WaitForSingleObject(sei.hProcess, 20000); printf("SHELL ok\n"); }
        else printf("SHELL err %lu\n", GetLastError());
    }
    return 0;
}
