/* GetTempPath2 for the SYSTEM account (patches/sg/0638). Prints:
 *   user=<the token's user SID> temp2=<GetTempPath2W> temp=<GetTempPathW>
 */
#include <windows.h>
#include <sddl.h>
#include <stdio.h>

int main(void)
{
    char buf[sizeof(TOKEN_USER) + SECURITY_MAX_SID_SIZE];
    WCHAR t2[MAX_PATH] = L"", t[MAX_PATH] = L"", *sid = NULL;
    HANDLE token;
    DWORD len;

    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token) &&
        GetTokenInformation(token, TokenUser, buf, sizeof(buf), &len))
        ConvertSidToStringSidW(((TOKEN_USER *)buf)->User.Sid, &sid);
    {
        DWORD (WINAPI *pGetTempPath2W)(DWORD, WCHAR *) =
            (void *)GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "GetTempPath2W");
        if (pGetTempPath2W) pGetTempPath2W(MAX_PATH, t2);
    }
    GetTempPathW(MAX_PATH, t);
    printf("user=%ls\ntemp2=%ls\ntemp=%ls\n", sid ? sid : L"?", t2, t);
    return 0;
}
