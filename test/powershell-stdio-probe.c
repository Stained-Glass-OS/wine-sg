/* A stand-in for PowerShell 7's pwsh.exe (test/powershell-stdio-gate.sh):
 * echoes its command line, copies its standard input, writes to standard
 * error and exits 7, so the gate can see whether powershell.exe passed its
 * own standard handles and exit code through. */
#include <windows.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    const char *cmd = GetCommandLineA();
    const char *args = strstr(cmd, " -");
    char line[256];

    printf("ARGS:%s\n", args ? args + 1 : "");
    while (fgets(line, sizeof(line), stdin))
    {
        line[strcspn(line, "\r\n")] = 0;
        printf("IN:%s\n", line);
    }
    fprintf(stderr, "ERR:stand-in\n");
    fflush(stdout);
    fflush(stderr);
    return 7;
}
