/* A stand-in pwsh.exe for test/powershell-gate.sh: writes its command line to
 * the file named by SG_PWSH_LOG and exits 7. */
#include <windows.h>
#include <stdio.h>
int wmain(void)
{
    WCHAR log[MAX_PATH];
    FILE *f;
    if (GetEnvironmentVariableW(L"SG_PWSH_LOG", log, MAX_PATH) && (f = _wfopen(log, L"w")))
    {
        fwprintf(f, L"%ls\n", GetCommandLineW());
        fclose(f);
    }
    return 7;
}
