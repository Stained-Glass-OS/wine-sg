/* sysrelevated-probe: what this process's token says about elevation */
#include <windows.h>
#include <stdio.h>

int main(void)
{
    HANDLE tok;
    TOKEN_ELEVATION elevation = {0};
    TOKEN_ELEVATION_TYPE type = 0;
    DWORD len;

    OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &tok);
    GetTokenInformation(tok, TokenElevation, &elevation, sizeof(elevation), &len);
    GetTokenInformation(tok, TokenElevationType, &type, sizeof(type), &len);
    printf("elevated %lu type %d\n", elevation.TokenIsElevated, type);
    return 0;
}
