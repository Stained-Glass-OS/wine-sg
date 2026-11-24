/* elevobj-probe: what an elevated program's objects look like (patches/sg/9150).
 *   tok                 the token's user, owner and default DACL (SDDL)
 *   mk KEY              create KEY (HKLM\... or HKCU\...) with no descriptor of its own
 *   sd KEY              KEY's owner and DACL (SDDL)
 *   del KEY             delete KEY (RegDeleteKeyEx) -> DEL <win32 error>
 *   open KEY ACCESS     open KEY with ACCESS (hex) -> OPEN <win32 error>
 *   setval KEY          write a value into KEY -> SET <win32 error>
 *   owner KEY           set KEY's owner only (SetSecurityInfo, OWNER), as Office's service does
 *   mkdir PATH          create a folder with no descriptor of its own
 *   fsd PATH            PATH's owner and DACL (SDDL)
 *   rread               a restricted token (Everyone, Users, RESTRICTED) of this user opens its
 *                       HKEY_USERS\<SID>\Software to read, then to write
 *   pid [SECS]          print the process id, then wait SECS
 *   proc PID            open process PID for writing its memory and reserve a page in it
 */
#include <windows.h>
#include <sddl.h>
#include <aclapi.h>
#include <stdio.h>

static HKEY root_of(const char **path)
{
    if (!_strnicmp(*path, "HKLM\\", 5)) { *path += 5; return HKEY_LOCAL_MACHINE; }
    if (!_strnicmp(*path, "HKCU\\", 5)) { *path += 5; return HKEY_CURRENT_USER; }
    if (!_strnicmp(*path, "HKU\\", 4)) { *path += 4; return HKEY_USERS; }
    return NULL;
}

static void print_sd(PSECURITY_DESCRIPTOR sd)
{
    char *str = NULL;
    if (ConvertSecurityDescriptorToStringSecurityDescriptorA(sd, SDDL_REVISION_1,
            OWNER_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION, &str, NULL))
        printf("SD %s\n", str);
    else printf("SD ? %lu\n", GetLastError());
    LocalFree(str);
}

int main(int argc, char **argv)
{
    const char *path = argc > 2 ? argv[2] : "";
    HKEY root = argc > 2 ? root_of(&path) : NULL, key;
    LONG rc;

    if (argc > 1 && !strcmp(argv[1], "tok"))
    {
        HANDLE tok;
        BYTE buf[2048];
        DWORD len;
        char *str = NULL;
        SECURITY_DESCRIPTOR sd;

        OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &tok);
        if (GetTokenInformation(tok, TokenUser, buf, sizeof(buf), &len) &&
            ConvertSidToStringSidA(((TOKEN_USER *)buf)->User.Sid, &str)) printf("USER %s\n", str);
        LocalFree(str);
        str = NULL;
        if (GetTokenInformation(tok, TokenOwner, buf, sizeof(buf), &len) &&
            ConvertSidToStringSidA(((TOKEN_OWNER *)buf)->Owner, &str)) printf("OWNER %s\n", str);
        LocalFree(str);
        if (GetTokenInformation(tok, TokenDefaultDacl, buf, sizeof(buf), &len))
        {
            InitializeSecurityDescriptor(&sd, SECURITY_DESCRIPTOR_REVISION);
            SetSecurityDescriptorDacl(&sd, TRUE, ((TOKEN_DEFAULT_DACL *)buf)->DefaultDacl, FALSE);
            str = NULL;
            if (ConvertSecurityDescriptorToStringSecurityDescriptorA(&sd, SDDL_REVISION_1,
                    DACL_SECURITY_INFORMATION, &str, NULL)) printf("DEFDACL %s\n", str);
            LocalFree(str);
        }
        return 0;
    }
    if (argc > 2 && !strcmp(argv[1], "proc"))
    {
        /* may this token change another process (an elevated one): open it for writing its memory */
        HANDLE h = OpenProcess(PROCESS_VM_OPERATION | PROCESS_VM_WRITE, FALSE, strtoul(argv[2], NULL, 0));
        void *mem = h ? VirtualAllocEx(h, NULL, 4096, MEM_RESERVE, PAGE_NOACCESS) : NULL;
        printf("PROC %lu ALLOC %d\n", h ? 0 : GetLastError(), mem ? 1 : 0);
        if (mem) VirtualFreeEx(h, mem, 0, MEM_RELEASE);
        if (h) CloseHandle(h);
        return 0;
    }
    if (argc > 1 && !strcmp(argv[1], "pid"))
    {
        printf("PID %lu\n", GetCurrentProcessId());
        fflush(stdout);
        if (argc > 2) Sleep(strtoul(argv[2], NULL, 0) * 1000);
        return 0;
    }
    if (argc > 1 && !strcmp(argv[1], "rread"))
    {
        /* a sandboxed process of the user's (Chromium's USER_LIMITED: restricting SIDs Everyone,
         * Users, RESTRICTED) reads its user's HKEY_USERS\<SID>\Software */
        SID_IDENTIFIER_AUTHORITY world = { SECURITY_WORLD_SID_AUTHORITY }, nt = { SECURITY_NT_AUTHORITY };
        SID_AND_ATTRIBUTES rs[3] = { { 0 } };
        HANDLE tok, rtok;
        BYTE buf[256];
        DWORD len;
        char *sid = NULL, sub[256];

        OpenProcessToken(GetCurrentProcess(), TOKEN_ALL_ACCESS, &tok);
        GetTokenInformation(tok, TokenUser, buf, sizeof(buf), &len);
        ConvertSidToStringSidA(((TOKEN_USER *)buf)->User.Sid, &sid);
        AllocateAndInitializeSid(&world, 1, SECURITY_WORLD_RID, 0, 0, 0, 0, 0, 0, 0, &rs[0].Sid);
        AllocateAndInitializeSid(&nt, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_USERS, 0, 0, 0, 0, 0, 0, &rs[1].Sid);
        AllocateAndInitializeSid(&nt, 1, SECURITY_RESTRICTED_CODE_RID, 0, 0, 0, 0, 0, 0, 0, &rs[2].Sid);
        if (!CreateRestrictedToken(tok, 0, 0, NULL, 0, NULL, 3, rs, &rtok)) { printf("RREAD ? %lu\n", GetLastError()); return 0; }
        snprintf(sub, sizeof(sub), "%s\\Software", sid);
        if (!ImpersonateLoggedOnUser(rtok)) { printf("RREAD ? imp %lu\n", GetLastError()); return 0; }
        rc = RegOpenKeyExA(HKEY_USERS, sub, 0, KEY_READ, &key);
        if (!rc) RegCloseKey(key);
        printf("RREAD %ld\n", rc);
        rc = RegOpenKeyExA(HKEY_USERS, sub, 0, KEY_SET_VALUE, &key);
        if (!rc) RegCloseKey(key);
        printf("RWRITE %ld\n", rc);
        RevertToSelf();
        return 0;
    }
    if (argc > 2 && !strcmp(argv[1], "mkdir"))
    {
        printf("MKDIR %lu\n", CreateDirectoryA(argv[2], NULL) ? 0 : GetLastError());
        return 0;
    }
    if (argc > 2 && !strcmp(argv[1], "fsd"))
    {
        PSECURITY_DESCRIPTOR sd = NULL;
        DWORD err = GetNamedSecurityInfoA(argv[2], SE_FILE_OBJECT, OWNER_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION,
                                          NULL, NULL, NULL, NULL, &sd);
        if (err) printf("SD ? %lu\n", err); else print_sd(sd);
        LocalFree(sd);
        return 0;
    }
    if (!root) { printf("usage: elevobj-probe tok | mk|sd|del|setval|owner KEY | open KEY ACCESS | mkdir|fsd PATH\n"); return 2; }
    if (!strcmp(argv[1], "mk"))
    {
        rc = RegCreateKeyExA(root, path, 0, NULL, 0, KEY_ALL_ACCESS, NULL, &key, NULL);
        printf("MK %ld\n", rc);
        if (!rc) RegCloseKey(key);
    }
    else if (!strcmp(argv[1], "sd"))
    {
        BYTE buf[4096];
        DWORD len = sizeof(buf);
        if ((rc = RegOpenKeyExA(root, path, 0, READ_CONTROL, &key))) { printf("SD ? open %ld\n", rc); return 0; }
        rc = RegGetKeySecurity(key, OWNER_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION, buf, &len);
        if (rc) printf("SD ? %ld\n", rc); else print_sd(buf);
        RegCloseKey(key);
    }
    else if (!strcmp(argv[1], "del"))
        printf("DEL %ld\n", RegDeleteKeyExA(root, path, 0, 0));
    else if (!strcmp(argv[1], "open") && argc > 3)
    {
        rc = RegOpenKeyExA(root, path, 0, strtoul(argv[3], NULL, 16), &key);
        printf("OPEN %ld\n", rc);
        if (!rc) RegCloseKey(key);
    }
    else if (!strcmp(argv[1], "setval"))
    {
        DWORD one = 1;
        if (!(rc = RegOpenKeyExA(root, path, 0, KEY_SET_VALUE, &key)))
        {
            rc = RegSetValueExA(key, "SgElevObj", 0, REG_DWORD, (BYTE *)&one, sizeof(one));
            RegCloseKey(key);
        }
        printf("SET %ld\n", rc);
    }
    else if (!strcmp(argv[1], "owner"))
    {
        HANDLE tok;
        BYTE buf[256];
        DWORD len;
        OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &tok);
        GetTokenInformation(tok, TokenOwner, buf, sizeof(buf), &len);
        if ((rc = RegOpenKeyExA(root, path, 0, WRITE_OWNER, &key))) { printf("OWNERSET %ld\n", rc); return 0; }
        rc = SetSecurityInfo(key, SE_REGISTRY_KEY, OWNER_SECURITY_INFORMATION, ((TOKEN_OWNER *)buf)->Owner, NULL, NULL, NULL);
        printf("OWNERSET %ld\n", rc);
        RegCloseKey(key);
    }
    return 0;
}
