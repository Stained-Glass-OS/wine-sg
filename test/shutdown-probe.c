/* shutdown-probe: ExitWindowsEx(shutdown|reboot|poweroff|logoff), for
 * shutdown-gate.sh. Prints the result. */
#include <windows.h>
#include <stdio.h>
#include <string.h>

int main(int argc, char **argv)
{
    UINT flags;
    BOOL ok;
    if (argc < 2) return 2;
    if (!strcmp(argv[1], "shutdown")) flags = EWX_SHUTDOWN;
    else if (!strcmp(argv[1], "reboot")) flags = EWX_REBOOT;
    else if (!strcmp(argv[1], "poweroff")) flags = EWX_SHUTDOWN | EWX_POWEROFF;
    else if (!strcmp(argv[1], "logoff")) flags = EWX_LOGOFF;
    else return 2;
    ok = ExitWindowsEx(flags, SHTDN_REASON_FLAG_PLANNED);
    printf("EXIT %s %d\n", argv[1], ok);
    fflush(stdout);
    return ok ? 0 : 1;
}
