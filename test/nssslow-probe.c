/* nssslow-probe.c: for test/nssslow-gate.sh (7824)
 *   nssslow-probe.exe ping SECONDS   server round trips for SECONDS: prints Worst=MS Count=N
 *   nssslow-probe.exe who            prints Elevation=TYPE (1 Default, 2 Full, 3 Limited) */
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    if (argc > 2 && !strcmp(argv[1], "ping"))
    {
        DWORD end = GetTickCount() + atoi(argv[2]) * 1000, worst = 0, n = 0;
        while (GetTickCount() < end)
        {
            DWORD t = GetTickCount();
            HANDLE e = CreateEventA(NULL, FALSE, FALSE, NULL);   /* a server request each */
            CloseHandle(e);
            if (GetTickCount() - t > worst) worst = GetTickCount() - t;
            n++;
            Sleep(10);
        }
        printf("Worst=%lu Count=%lu\n", worst, n);
    }
    else
    {
        HANDLE tok; TOKEN_ELEVATION_TYPE type = 0; DWORD len;
        OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &tok);
        GetTokenInformation(tok, TokenElevationType, &type, sizeof(type), &len);
        printf("Elevation=%d\n", type);
    }
    return 0;
}
