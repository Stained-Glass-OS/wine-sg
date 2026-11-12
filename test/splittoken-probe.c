/* splittoken gate probe: what the process token says about elevation, as a program checks it
 * (Mozilla's NSIS UAC plugin, installers): prints KEY value lines */
#include <windows.h>
#include <shlobj.h>
#include <stdio.h>

int main(void)
{
    SID_IDENTIFIER_AUTHORITY nt = SECURITY_NT_AUTHORITY;
    TOKEN_ELEVATION_TYPE type = 0; TOKEN_ELEVATION elev = {0}; TOKEN_LINKED_TOKEN linked = {0};
    BYTE buf[8192]; TOKEN_GROUPS *groups = (TOKEN_GROUPS *)buf;
    HANDLE token, dup = NULL;
    PSID admins;
    BOOL member = FALSE;
    DWORD len, i, attrs = 0xffffffff;

    OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token);
    AllocateAndInitializeSid(&nt, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS, 0,0,0,0,0,0, &admins);
    GetTokenInformation(token, TokenElevationType, &type, sizeof(type), &len);
    GetTokenInformation(token, TokenElevation, &elev, sizeof(elev), &len);
    CheckTokenMembership(NULL, admins, &member);
    if (GetTokenInformation(token, TokenGroups, groups, sizeof(buf), &len))
        for (i = 0; i < groups->GroupCount; i++) if (EqualSid(groups->Groups[i].Sid, admins)) attrs = groups->Groups[i].Attributes;
    printf("TYPE %d\nELEVATED %lu\nMEMBER %d\nISADMIN %d\nADMINS %s %#lx\n", type, elev.TokenIsElevated, member,
           IsUserAnAdmin(), attrs == 0xffffffff ? "absent" : "present", attrs == 0xffffffff ? 0 : attrs);
    if (GetTokenInformation(token, TokenLinkedToken, &linked, sizeof(linked), &len) && linked.LinkedToken)
    {
        TOKEN_ELEVATION_TYPE ltype = 0; TOKEN_ELEVATION lelev = {0}; SECURITY_IMPERSONATION_LEVEL level = 0;
        BOOL lmember = FALSE;
        GetTokenInformation(linked.LinkedToken, TokenElevationType, &ltype, sizeof(ltype), &len);
        GetTokenInformation(linked.LinkedToken, TokenElevation, &lelev, sizeof(lelev), &len);
        GetTokenInformation(linked.LinkedToken, TokenImpersonationLevel, &level, sizeof(level), &len);
        CheckTokenMembership(linked.LinkedToken, admins, &lmember);
        printf("LINKED %d %lu level %d admins %d\n", ltype, lelev.TokenIsElevated, level, lmember);
        /* a person's linked token may be read, not used: no impersonation, no process token */
        printf("LINKED_IMPERSONATE %d\n", SetThreadToken(NULL, linked.LinkedToken));
        printf("LINKED_DUPLICATE %d\n", DuplicateTokenEx(linked.LinkedToken, TOKEN_ALL_ACCESS, NULL, SecurityImpersonation, TokenPrimary, &dup));
        RevertToSelf();
    }
    else printf("LINKED none %lu\n", GetLastError());
    fflush(stdout);
    return 0;
}
