/* mstsclaunch-target.c: for test/mstsclaunch-gate.sh (7825) -- what App Paths names for mstsc.exe:
 * writes its command line to the file named by MSTSCLAUNCH_OUT, sleeps 3 s, exits with 7 */
#include <windows.h>
#include <stdio.h>
int main(void)
{
    char out[MAX_PATH];
    FILE *f;
    if (GetEnvironmentVariableA("MSTSCLAUNCH_OUT", out, sizeof(out)) && (f = fopen(out, "w")))
    { fprintf(f, "%s\n", GetCommandLineA()); fclose(f); }
    Sleep(3000);
    return 7;
}
