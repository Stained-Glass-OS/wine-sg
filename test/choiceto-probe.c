/* wine-sg 8414: CHOICE /T with no one at the console. Squirrel uninstallers (Mudlet, Trilium) delete
 * themselves with "cmd /C choice /C Y /N /D Y /T 3 & Del Update.exe", started without a window; the
 * timeout never fired (CHOICE read the console synchronously) and the cmd stayed forever. Starts
 * that command line with CREATE_NO_WINDOW (and, as a control, with stdin redirected from NUL) and
 * reports how long until the marker file is written. */
#include <windows.h>
#include <stdio.h>

static int run(const char *tag, DWORD flags, BOOL nul_stdin)
{
    char cmd[MAX_PATH * 2], marker[MAX_PATH];
    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    SECURITY_ATTRIBUTES sa = { sizeof(sa), NULL, TRUE };
    HANDLE nul = INVALID_HANDLE_VALUE;
    DWORD start, waited;
    int done;

    sprintf(marker, "C:\\choiceto-%s.txt", tag);
    DeleteFileA(marker);
    sprintf(cmd, "cmd.exe /C choice /C Y /N /D Y /T 2 & echo done > %s", marker);
    if (nul_stdin)
    {
        nul = CreateFileA("NUL", GENERIC_READ, FILE_SHARE_READ, &sa, OPEN_EXISTING, 0, NULL);
        si.dwFlags = STARTF_USESTDHANDLES;
        si.hStdInput = nul; si.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE); si.hStdError = GetStdHandle(STD_ERROR_HANDLE);
    }
    start = GetTickCount();
    if (!CreateProcessA(NULL, cmd, NULL, NULL, nul_stdin, flags, NULL, NULL, &si, &pi)) { printf("%s create failed %lu\n", tag, GetLastError()); return 0; }
    waited = WaitForSingleObject(pi.hProcess, 20000);
    done = GetFileAttributesA(marker) != INVALID_FILE_ATTRIBUTES;
    printf("%s: %s after %lu ms, marker %s\n", tag, waited == WAIT_OBJECT_0 ? "exited" : "STILL RUNNING", GetTickCount() - start, done ? "written" : "missing");
    if (waited != WAIT_OBJECT_0) TerminateProcess(pi.hProcess, 1);
    CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
    if (nul != INVALID_HANDLE_VALUE) CloseHandle(nul);
    return waited == WAIT_OBJECT_0 && done;
}

int main(void)
{
    int a = run("nowindow", CREATE_NO_WINDOW, FALSE);
    int b = run("newconsole", CREATE_NEW_CONSOLE, FALSE);
    int c = run("nulstdin", CREATE_NO_WINDOW, TRUE);
    printf("RESULT: %s\n", a && b && c ? "PASS" : "FAIL");
    return 0;
}
