/* envblock-probe: the TEMP and USERPROFILE CreateEnvironmentBlock gives this
 * process's own token, and the token user's SID */
#include <windows.h>
#include <sddl.h>
#include <userenv.h>
#include <stdio.h>

int main(int argc, char **argv)
{
    HANDLE tok;
    BYTE buf[256];
    DWORD len;
    WCHAR *sid, *p;
    void *env;

    OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY | TOKEN_DUPLICATE, &tok);
    GetTokenInformation(tok, TokenUser, buf, sizeof(buf), &len);
    ConvertSidToStringSidW(((TOKEN_USER *)buf)->User.Sid, &sid);
    if (argc > 1) { printf("%ls\n", sid); return 0; }
    if (!CreateEnvironmentBlock(&env, tok, FALSE)) { printf("failed\n"); return 1; }
    for (p = env; *p; p += lstrlenW(p) + 1)
        if (!_wcsnicmp(p, L"TEMP=", 5) || !_wcsnicmp(p, L"USERPROFILE=", 12)) printf("%ls\n", p);
    return 0;
}
