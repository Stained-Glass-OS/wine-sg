/* advapi32 security functions that were stubs (patches/sg/1619):
 * LookupPrivilegeDisplayName, GetExplicitEntriesFromAclA, the Safer levels
 * and TreeSetNamedSecurityInfoW / TreeResetNamedSecurityInfoW. */
#include <windows.h>
#include <aclapi.h>
#include <winsafer.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

typedef BOOL (WINAPI *get_level_info_t)(SAFER_LEVEL_HANDLE, int, void *, DWORD, DWORD *);
typedef DWORD (WINAPI *tree_set_t)(WCHAR *, SE_OBJECT_TYPE, SECURITY_INFORMATION, PSID, PSID, PACL, PACL, DWORD,
                                   FN_PROGRESS, PROG_INVOKE_SETTING, void *);
typedef DWORD (WINAPI *tree_reset_t)(WCHAR *, SE_OBJECT_TYPE, SECURITY_INFORMATION, PSID, PSID, PACL, PACL, BOOL,
                                     FN_PROGRESS, PROG_INVOKE_SETTING, void *);

static int failures, progress_calls;

static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) failures++;
}

static void progress(LPWSTR name, DWORD status, PPROG_INVOKE_SETTING pis, PVOID args, BOOL set)
{
    progress_calls++;
    printf("  progress %ls %lu\n", name, status);
}

/* whether a key's DACL has an ACE for sid, inherited or not */
static int key_has_ace(const WCHAR *path, PSID sid, BOOL inherited)
{
    PSECURITY_DESCRIPTOR sd;
    PACL dacl = NULL;
    ACE_HEADER *ace;
    DWORD i;
    int found = 0;

    if (GetNamedSecurityInfoW(path, SE_REGISTRY_KEY, DACL_SECURITY_INFORMATION, NULL, NULL, &dacl, NULL, &sd)) return -1;
    for (i = 0; dacl && i < dacl->AceCount; i++)
    {
        if (!GetAce(dacl, i, (void **)&ace) || ace->AceType != ACCESS_ALLOWED_ACE_TYPE) continue;
        if (EqualSid((PSID)&((ACCESS_ALLOWED_ACE *)ace)->SidStart, sid) && !!(ace->AceFlags & INHERITED_ACE) == !!inherited)
            found = 1;
    }
    LocalFree(sd);
    return found;
}

/* what Everyone may do to a file */
static DWORD everyone_mask(const WCHAR *path)
{
    SID_IDENTIFIER_AUTHORITY world = {SECURITY_WORLD_SID_AUTHORITY};
    PSECURITY_DESCRIPTOR sd;
    PACL dacl = NULL;
    ACE_HEADER *ace;
    PSID everyone;
    DWORD i, mask = 0;

    AllocateAndInitializeSid(&world, 1, SECURITY_WORLD_RID, 0, 0, 0, 0, 0, 0, 0, &everyone);
    if (!GetNamedSecurityInfoW(path, SE_FILE_OBJECT, DACL_SECURITY_INFORMATION, NULL, NULL, &dacl, NULL, &sd))
    {
        for (i = 0; dacl && i < dacl->AceCount; i++)
            if (GetAce(dacl, i, (void **)&ace) && ace->AceType == ACCESS_ALLOWED_ACE_TYPE &&
                EqualSid((PSID)&((ACCESS_ALLOWED_ACE *)ace)->SidStart, everyone))
                mask |= ((ACCESS_ALLOWED_ACE *)ace)->Mask;
        LocalFree(sd);
    }
    FreeSid(everyone);
    return mask;
}

int main(void)
{
    HMODULE adv = GetModuleHandleA("advapi32.dll");
    get_level_info_t pSaferGetLevelInformation = (void *)GetProcAddress(adv, "SaferGetLevelInformation");
    tree_set_t pTreeSetNamedSecurityInfoW = (void *)GetProcAddress(adv, "TreeSetNamedSecurityInfoW");
    tree_reset_t pTreeResetNamedSecurityInfoW = (void *)GetProcAddress(adv, "TreeResetNamedSecurityInfoW");
    WCHAR display[128], root[MAX_PATH], sub[MAX_PATH], file[MAX_PATH];
    char displayA[128];
    DWORD len, lang = 0, level_id = 0, needed, size, i;
    SID_IDENTIFIER_AUTHORITY world = {SECURITY_WORLD_SID_AUTHORITY}, nt = {SECURITY_NT_AUTHORITY};
    PSID everyone, users;
    EXPLICIT_ACCESSW ea;
    EXPLICIT_ACCESSA *entriesA;
    ULONG count;
    PACL acl = NULL, file_acl = NULL;
    SAFER_LEVEL_HANDLE level;
    HANDLE token = (HANDLE)0xdeadbeef, f;
    HKEY key;
    BYTE buf[4096];
    TOKEN_GROUPS *groups = (TOKEN_GROUPS *)buf;
    TOKEN_MANDATORY_LABEL *label = (TOKEN_MANDATORY_LABEL *)buf;
    PSID admins;
    BOOL ok, admin_deny_only = FALSE;
    DWORD err;

    AllocateAndInitializeSid(&world, 1, SECURITY_WORLD_RID, 0, 0, 0, 0, 0, 0, 0, &everyone);
    AllocateAndInitializeSid(&nt, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_USERS, 0, 0, 0, 0, 0, 0, &users);
    AllocateAndInitializeSid(&nt, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &admins);

    /* privilege display names */
    len = ARRAYSIZE(display);
    ok = LookupPrivilegeDisplayNameW(NULL, L"SeShutdownPrivilege", display, &len, &lang);
    printf("display name: ok %d '%ls' len %lu lang %#lx\n", ok, ok ? display : L"", len, lang);
    check(ok && !wcscmp(display, L"Shut down the system") && len == 20 && lang == 0x409, "LookupPrivilegeDisplayNameW");
    len = 4;
    SetLastError(0xdeadbeef);
    ok = LookupPrivilegeDisplayNameW(NULL, L"SeShutdownPrivilege", display, &len, &lang);
    check(!ok && GetLastError() == ERROR_INSUFFICIENT_BUFFER && len == 21, "... a small buffer gives the length needed");
    len = ARRAYSIZE(display);
    SetLastError(0xdeadbeef);
    ok = LookupPrivilegeDisplayNameW(NULL, L"SeNoSuchPrivilege", display, &len, &lang);
    check(!ok && GetLastError() == ERROR_NO_SUCH_PRIVILEGE, "... an unknown privilege: ERROR_NO_SUCH_PRIVILEGE");
    len = sizeof(displayA);
    ok = LookupPrivilegeDisplayNameA(NULL, "SeBackupPrivilege", displayA, &len, &lang);
    check(ok && !strcmp(displayA, "Back up files and directories"), "LookupPrivilegeDisplayNameA");

    /* GetExplicitEntriesFromAclA */
    memset(&ea, 0, sizeof(ea));
    ea.grfAccessMode = GRANT_ACCESS;
    ea.grfAccessPermissions = GENERIC_ALL;
    ea.grfInheritance = SUB_CONTAINERS_AND_OBJECTS_INHERIT;
    ea.Trustee.TrusteeForm = TRUSTEE_IS_SID;
    ea.Trustee.ptstrName = everyone;
    SetEntriesInAclW(1, &ea, NULL, &acl);
    err = GetExplicitEntriesFromAclA(acl, &count, &entriesA);
    check(!err && count == 1 && EqualSid((PSID)entriesA[0].Trustee.ptstrName, everyone), "GetExplicitEntriesFromAclA");
    if (!err) LocalFree(entriesA);

    /* Safer levels */
    ok = SaferCreateLevel(SAFER_SCOPEID_MACHINE, SAFER_LEVELID_NORMALUSER, SAFER_LEVEL_OPEN, &level, NULL);
    check(ok && level && level != (SAFER_LEVEL_HANDLE)0xdeadbeef, "SaferCreateLevel(normal user)");
    ok = pSaferGetLevelInformation && pSaferGetLevelInformation(level, 1 /* SaferObjectLevelId */, &level_id, sizeof(level_id), &needed);
    check(ok && level_id == SAFER_LEVELID_NORMALUSER, "SaferGetLevelInformation gives the level");
    ok = SaferComputeTokenFromLevel(level, NULL, &token, 0, NULL);
    printf("normal user token: ok %d token %p\n", ok, token);
    check(ok && token && token != (HANDLE)0xdeadbeef, "SaferComputeTokenFromLevel makes a token");
    if (ok && GetTokenInformation(token, TokenGroups, buf, sizeof(buf), &size))
        for (i = 0; i < groups->GroupCount; i++)
            if (EqualSid(groups->Groups[i].Sid, admins) && (groups->Groups[i].Attributes & SE_GROUP_USE_FOR_DENY_ONLY))
                admin_deny_only = TRUE;
    check(admin_deny_only, "... where Administrators is for deny only");
    if (ok && GetTokenInformation(token, TokenIntegrityLevel, buf, sizeof(buf), &size))
        check(*GetSidSubAuthority(label->Label.Sid, 0) == SECURITY_MANDATORY_MEDIUM_RID, "... at medium integrity");
    if (ok) CloseHandle(token);
    check(SaferCloseLevel(level), "SaferCloseLevel");
    SetLastError(0xdeadbeef);
    check(!SaferCloseLevel(level) && GetLastError() == ERROR_INVALID_HANDLE, "... twice: ERROR_INVALID_HANDLE");
    SaferCreateLevel(SAFER_SCOPEID_USER, SAFER_LEVELID_FULLYTRUSTED, SAFER_LEVEL_OPEN, &level, NULL);
    token = (HANDLE)1;
    ok = SaferComputeTokenFromLevel(level, NULL, &token, SAFER_TOKEN_NULL_IF_EQUAL, NULL);
    check(ok && !token, "fully trusted with SAFER_TOKEN_NULL_IF_EQUAL: no token needed");
    SaferCloseLevel(level);
    SaferCreateLevel(SAFER_SCOPEID_USER, SAFER_LEVELID_DISALLOWED, SAFER_LEVEL_OPEN, &level, NULL);
    SetLastError(0xdeadbeef);
    ok = SaferComputeTokenFromLevel(level, NULL, &token, 0, NULL);
    check(!ok && GetLastError() == ERROR_ACCESS_DISABLED_BY_POLICY, "disallowed: no token");
    SaferCloseLevel(level);
    level_id = 0;
    ok = SaferGetPolicyInformation(SAFER_SCOPEID_MACHINE, SaferPolicyDefaultLevel, sizeof(level_id), &level_id, &needed, NULL);
    check(ok && level_id == SAFER_LEVELID_FULLYTRUSTED, "SaferGetPolicyInformation: everything fully trusted");

    /* down a tree of folders: file permissions here are the Unix ones, so
     * what reaches the files is Everyone's access, not each ACE */
    GetTempPathW(MAX_PATH, root);
    wcscat(root, L"treesec");
    swprintf(sub, MAX_PATH, L"%ls\\sub", root);
    swprintf(file, MAX_PATH, L"%ls\\file.txt", sub);
    CreateDirectoryW(root, NULL);
    CreateDirectoryW(sub, NULL);
    f = CreateFileW(file, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    CloseHandle(f);
    progress_calls = 0;
    err = pTreeSetNamedSecurityInfoW(root, SE_FILE_OBJECT, DACL_SECURITY_INFORMATION, NULL, NULL, acl, NULL,
                                     1 /* TREE_SEC_INFO_SET */, progress, ProgressInvokeEveryObject, NULL);
    printf("tree set: %lu, %d progress calls\n", err, progress_calls);
    check(!err && progress_calls == 3, "TreeSetNamedSecurityInfoW reaches the folder, its subfolder and the file");
    check(everyone_mask(file) & FILE_WRITE_DATA, "... and Everyone may write the file now");
    DeleteFileW(file); RemoveDirectoryW(sub); RemoveDirectoryW(root);

    /* and down a tree of keys, where every ACE is kept */
    RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\SGTreeSec\\sub\\leaf", 0, NULL, 0, KEY_ALL_ACCESS, NULL, &key, NULL);
    RegCloseKey(key);
    ea.Trustee.ptstrName = users;
    ea.grfAccessPermissions = KEY_READ;
    ea.grfInheritance = NO_INHERITANCE;
    SetEntriesInAclW(1, &ea, NULL, &file_acl);
    SetNamedSecurityInfoW((WCHAR *)L"CURRENT_USER\\Software\\SGTreeSec\\sub\\leaf", SE_REGISTRY_KEY,
                          DACL_SECURITY_INFORMATION, NULL, NULL, file_acl, NULL);
    err = pTreeSetNamedSecurityInfoW((WCHAR *)L"CURRENT_USER\\Software\\SGTreeSec", SE_REGISTRY_KEY,
                                     DACL_SECURITY_INFORMATION, NULL, NULL, acl, NULL, 1, NULL, ProgressInvokeNever, NULL);
    check(!err && key_has_ace(L"CURRENT_USER\\Software\\SGTreeSec\\sub", everyone, TRUE) == 1,
          "a subkey inherits the entry");
    check(key_has_ace(L"CURRENT_USER\\Software\\SGTreeSec\\sub\\leaf", everyone, TRUE) == 1 &&
          key_has_ace(L"CURRENT_USER\\Software\\SGTreeSec\\sub\\leaf", users, FALSE) == 1,
          "... and so does the key below it, which keeps its own entry");
    err = pTreeResetNamedSecurityInfoW((WCHAR *)L"CURRENT_USER\\Software\\SGTreeSec", SE_REGISTRY_KEY,
                                       DACL_SECURITY_INFORMATION, NULL, NULL, acl, NULL, FALSE, NULL, ProgressInvokeNever, NULL);
    check(!err && key_has_ace(L"CURRENT_USER\\Software\\SGTreeSec\\sub\\leaf", users, FALSE) == 0 &&
          key_has_ace(L"CURRENT_USER\\Software\\SGTreeSec\\sub\\leaf", everyone, TRUE) == 1,
          "TreeResetNamedSecurityInfoW drops the explicit entries below");
    RegDeleteTreeW(HKEY_CURRENT_USER, L"Software\\SGTreeSec");

    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
