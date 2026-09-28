/* logonvars-probe: the logon variables this process was given */
#include <windows.h>
#include <stdio.h>

int main(void)
{
    static const char *names[] = { "USERPROFILE", "APPDATA", "LOCALAPPDATA", "HOMEDRIVE", "HOMEPATH" };
    char value[MAX_PATH];
    int i;

    for (i = 0; i < 5; i++)
    {
        if (!GetEnvironmentVariableA(names[i], value, sizeof(value))) value[0] = 0;
        printf("%s=%s\n", names[i], value);
    }
    return 0;
}
