/* Another process's memory (wine-sg 0624): a parent reads and writes its
 * child's, as a debugger or a launcher does. */
#include <windows.h>
#include <stdio.h>
#include <string.h>

static char marker[64] = "sg-ptracecap-original";

int main(int argc, char **argv)
{
    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    char cmd[MAX_PATH + 16], buf[64] = "";
    SIZE_T n = 0;
    BOOL r, w;

    if (argc > 1 && !strcmp(argv[1], "child")) { Sleep(20000); return 0; }
    GetModuleFileNameA(NULL, cmd + 1, MAX_PATH);
    cmd[0] = '"'; strcat(cmd, "\" child");
    if (!CreateProcessA(NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) { printf("CREATE failed %lu\n", GetLastError()); return 1; }
    Sleep(1500);
    /* the child is this program: the marker is at the same address there */
    r = ReadProcessMemory(pi.hProcess, marker, buf, sizeof(marker), &n);
    printf("READ %s %lu %s\n", r ? "ok" : "failed", r ? 0 : GetLastError(), r ? buf : "");
    w = WriteProcessMemory(pi.hProcess, marker, "sg-ptracecap-written", 21, &n);
    memset(buf, 0, sizeof(buf));
    if (w) ReadProcessMemory(pi.hProcess, marker, buf, sizeof(marker), &n);
    printf("WRITE %s %lu %s\n", w ? "ok" : "failed", w ? 0 : GetLastError(), buf);
    TerminateProcess(pi.hProcess, 0);
    return 0;
}
