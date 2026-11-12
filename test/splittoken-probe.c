/* splittoken gate probe: what the process token says about elevation, as a program checks it
 * (Mozilla's NSIS UAC plugin, installers): prints KEY value lines */
#include <windows.h>
#include <shlobj.h>
#include <stdio.h>
#include <string.h>

int main(int argc, char **argv)
{
    SID_IDENTIFIER_AUTHORITY nt = SECURITY_NT_AUTHORITY;
    TOKEN_ELEVATION_TYPE type = 0; TOKEN_ELEVATION elev = {0}; TOKEN_LINKED_TOKEN linked = {0};
    BYTE buf[8192]; TOKEN_GROUPS *groups = (TOKEN_GROUPS *)buf;
    HANDLE token, dup = NULL;
    PSID admins;
    BOOL member = FALSE;
    DWORD len, i, attrs = 0xffffffff;

    if (argc > 2 && !strcmp(argv[1], "create"))
    {
        /* a program whose manifest says requireAdministrator: a filtered administrator gets
         * ERROR_ELEVATION_REQUIRED (consent), as on Windows, not an unelevated run */
        STARTUPINFOA si = { sizeof(si) };
        PROCESS_INFORMATION pi;
        DWORD code = 0;

        if (CreateProcessA(argv[2], NULL, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
        {
            WaitForSingleObject(pi.hProcess, 60000);
            GetExitCodeProcess(pi.hProcess, &code);
            printf("CREATE ok %lu\n", code);
        }
        else printf("CREATE err %lu\n", GetLastError());
        fflush(stdout);
        return 0;
    }
    if (argc > 1 && !strcmp(argv[1], "exit7")) return 7;
    if (argc > 1 && !strcmp(argv[1], "runas"))
    {
        /* "Run as administrator", as an installer relaunches itself */
        SHELLEXECUTEINFOW sei = { sizeof(sei) };
        DWORD code = 0;
        BOOL ok;

        sei.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_FLAG_NO_UI;
        sei.lpVerb = L"runas";
        sei.lpFile = L"C:\\windows\\system32\\cmd.exe";
        sei.lpParameters = L"/c exit 7";
        sei.nShow = SW_HIDE;
        ok = ShellExecuteExW(&sei);
        printf("RUNAS ok %d err %lu", ok, ok ? 0 : GetLastError());
        if (ok && sei.hProcess) { WaitForSingleObject(sei.hProcess, 60000); GetExitCodeProcess(sei.hProcess, &code); }
        printf(" code %lu\n", code);
        fflush(stdout);
        return 0;
    }
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
