/* elevid-probe: who an elevated program is (patches/sg/7601).
 *   id               its token's user SID, elevation type, Administrators, integrity, session,
 *                    USERNAME and USERPROFILE
 *   hkcu-write V     HKCU\Software\SgElevId "By" = V
 *   hkcu-read        HKCU\Software\SgElevId "By"
 *   files PATH       PATH's DACL: does Administrators get full access, and may this token write there
 */
#include <windows.h>
#include <sddl.h>
#include <aclapi.h>
#include <stdio.h>

static void print_id(void)
{
    HANDLE tok;
    BYTE buf[512];
    DWORD len, sid_ok = 0;
    char *sidstr = NULL, env[MAX_PATH];
    TOKEN_ELEVATION_TYPE type = 0;
    SID_IDENTIFIER_AUTHORITY nt = { SECURITY_NT_AUTHORITY };
    PSID admins = NULL;
    BOOL member = FALSE;
    DWORD rid = 0, session = 0;

    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &tok)) { printf("ERROR token\n"); return; }
    if (GetTokenInformation(tok, TokenUser, buf, sizeof(buf), &len))
        sid_ok = ConvertSidToStringSidA(((TOKEN_USER *)buf)->User.Sid, &sidstr);
    printf("SID %s\n", sid_ok ? sidstr : "?");
    GetTokenInformation(tok, TokenElevationType, &type, sizeof(type), &len);
    printf("ELEVATION %d\n", (int)type);
    AllocateAndInitializeSid(&nt, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &admins);
    CheckTokenMembership(NULL, admins, &member);
    printf("ADMIN %d\n", member ? 1 : 0);
    if (GetTokenInformation(tok, TokenIntegrityLevel, buf, sizeof(buf), &len))
    {
        PSID label = ((TOKEN_MANDATORY_LABEL *)buf)->Label.Sid;
        rid = *GetSidSubAuthority(label, *GetSidSubAuthorityCount(label) - 1);
    }
    printf("INTEGRITY %lx\n", rid);
    ProcessIdToSessionId(GetCurrentProcessId(), &session);
    printf("SESSION %lu\n", session);
    printf("USERNAME %s\n", GetEnvironmentVariableA("USERNAME", env, sizeof(env)) ? env : "-");
    printf("USERPROFILE %s\n", GetEnvironmentVariableA("USERPROFILE", env, sizeof(env)) ? env : "-");
    LocalFree(sidstr);
    FreeSid(admins);
    CloseHandle(tok);
}

static int files(const char *path)
{
    PACL dacl = NULL;
    PSECURITY_DESCRIPTOR sd = NULL;
    SID_IDENTIFIER_AUTHORITY nt = { SECURITY_NT_AUTHORITY };
    PSID admins = NULL;
    GENERIC_MAPPING map = { FILE_GENERIC_READ, FILE_GENERIC_WRITE, FILE_GENERIC_EXECUTE, FILE_ALL_ACCESS };
    PRIVILEGE_SET privs;
    DWORD privlen = sizeof(privs), granted = 0, i, full = 0;
    BOOL ok = FALSE;
    HANDLE tok, imp;

    if (GetNamedSecurityInfoA(path, SE_FILE_OBJECT, DACL_SECURITY_INFORMATION | OWNER_SECURITY_INFORMATION |
                              GROUP_SECURITY_INFORMATION, NULL, NULL, &dacl, NULL, &sd))
    { printf("ERROR security info\n"); return 1; }
    AllocateAndInitializeSid(&nt, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &admins);
    for (i = 0; dacl && i < dacl->AceCount; i++)
    {
        ACCESS_ALLOWED_ACE *ace;
        if (!GetAce(dacl, i, (void **)&ace) || ace->Header.AceType != ACCESS_ALLOWED_ACE_TYPE) continue;
        if (EqualSid((PSID)&ace->SidStart, admins) && (ace->Mask & FILE_ALL_ACCESS) == FILE_ALL_ACCESS) full = 1;
    }
    printf("ADMINS_FULL %lu\n", full);
    OpenProcessToken(GetCurrentProcess(), TOKEN_DUPLICATE | TOKEN_QUERY, &tok);
    DuplicateToken(tok, SecurityImpersonation, &imp);
    AccessCheck(sd, imp, FILE_ADD_FILE | FILE_ADD_SUBDIRECTORY, &map, &privs, &privlen, &granted, &ok);
    printf("MAY_WRITE %d\n", ok ? 1 : 0);
    FreeSid(admins);
    LocalFree(sd);
    return 0;
}

int main(int argc, char **argv)
{
    HKEY key;
    char v[64];
    DWORD len = sizeof(v);

    if (argc > 1 && !strcmp(argv[1], "id")) { print_id(); return 0; }
    if (argc > 2 && !strcmp(argv[1], "hkcu-write"))
    {
        if (RegCreateKeyExA(HKEY_CURRENT_USER, "Software\\SgElevId", 0, NULL, 0, KEY_SET_VALUE, NULL, &key, NULL))
        { printf("WROTE 0\n"); return 1; }
        RegSetValueExA(key, "By", 0, REG_SZ, (BYTE *)argv[2], strlen(argv[2]) + 1);
        RegCloseKey(key);
        printf("WROTE 1\n");
        return 0;
    }
    if (argc > 1 && !strcmp(argv[1], "hkcu-read"))
    {
        if (RegGetValueA(HKEY_CURRENT_USER, "Software\\SgElevId", "By", RRF_RT_REG_SZ, NULL, v, &len)) strcpy(v, "-");
        printf("BY %s\n", v);
        return 0;
    }
    if (argc > 2 && !strcmp(argv[1], "files")) return files(argv[2]);
    printf("usage: elevid-probe id | hkcu-write V | hkcu-read | files PATH\n");
    return 2;
}
