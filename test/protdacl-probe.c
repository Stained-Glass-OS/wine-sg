/* SetSecurityInfo / SetNamedSecurityInfoW with PROTECTED_DACL_SECURITY_INFORMATION
 * on a folder: the DACL is the caller's alone (no ACEs inherited from the parent)
 * and the descriptor says so (SE_DACL_PROTECTED), as .NET's
 * DirectoryInfo.SetAccessControl after SetAccessRuleProtection(true, false) expects
 * (Duplicati refuses to start when its data folder "inherits permissions from its
 * parent"). UNPROTECTED_DACL_SECURITY_INFORMATION turns inheritance back on.
 * patches/sg/9300. Prints RESULT: PASS / FAIL. */
#include <windows.h>
#include <aclapi.h>
#include <sddl.h>
#include <stdio.h>

static int fails;
#define CHECK(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); fails++; } printf(__VA_ARGS__); printf("\n"); } while (0)

static void describe(const WCHAR *path, SECURITY_DESCRIPTOR_CONTROL *control, int *aces, int *inherited)
{
    PSECURITY_DESCRIPTOR sd = NULL;
    ACL *dacl = NULL;
    DWORD rev, err, i;
    ACE_HEADER *ace;

    *control = 0; *aces = -1; *inherited = -1;
    err = GetNamedSecurityInfoW(path, SE_FILE_OBJECT, DACL_SECURITY_INFORMATION, NULL, NULL, &dacl, NULL, &sd);
    if (err) { printf("GetNamedSecurityInfoW %lu\n", err); return; }
    GetSecurityDescriptorControl(sd, control, &rev);
    if (getenv("PROTDACL_SDDL"))
    {
        WCHAR *str;
        if (ConvertSecurityDescriptorToStringSecurityDescriptorW(sd, SDDL_REVISION_1, DACL_SECURITY_INFORMATION, &str, NULL))
            printf("  %ls\n", str);
    }
    if (dacl)
    {
        *aces = dacl->AceCount; *inherited = 0;
        for (i = 0; i < dacl->AceCount; i++)
            if (GetAce(dacl, i, (void **)&ace) && (ace->AceFlags & INHERITED_ACE)) (*inherited)++;
    }
    LocalFree(sd);
}

static ACL *own_dacl(void)
{
    static const WCHAR *sids[] = { L"SY", L"BA" };
    EXPLICIT_ACCESSW ea[3];
    HANDLE token;
    BYTE buf[256];
    TOKEN_USER *user = (TOKEN_USER *)buf;
    DWORD len, i;
    PSID sid[2];
    ACL *acl = NULL;

    OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token);
    GetTokenInformation(token, TokenUser, buf, sizeof(buf), &len);
    CloseHandle(token);
    memset(ea, 0, sizeof(ea));
    for (i = 0; i < 3; i++)
    {
        ea[i].grfAccessPermissions = GENERIC_ALL;
        ea[i].grfAccessMode = SET_ACCESS;
        ea[i].grfInheritance = SUB_CONTAINERS_AND_OBJECTS_INHERIT;
        ea[i].Trustee.TrusteeForm = TRUSTEE_IS_SID;
        if (i == 0) ea[i].Trustee.ptstrName = user->User.Sid;
        else
        {
            ConvertStringSidToSidW(sids[i - 1], &sid[i - 1]);
            ea[i].Trustee.ptstrName = sid[i - 1];
        }
    }
    SetEntriesInAclW(3, ea, NULL, &acl);
    return acl;
}

int main(void)
{
    WCHAR base[MAX_PATH], parent[MAX_PATH], dir[MAX_PATH], dir2[MAX_PATH];
    SECURITY_DESCRIPTOR_CONTROL control;
    int aces, inherited;
    ACL *acl = own_dacl(), *everyone = NULL;
    EXPLICIT_ACCESSW ea;
    PSID world;
    HANDLE h;
    DWORD err;

    GetTempPathW(MAX_PATH, base);
    if (base[wcslen(base) - 1] == L'\\') base[wcslen(base) - 1] = 0;
    swprintf(parent, MAX_PATH, L"%ls\\protdacl-parent", base);
    swprintf(dir, MAX_PATH, L"%ls\\data", parent);
    swprintf(dir2, MAX_PATH, L"%ls\\data2", parent);
    CreateDirectoryW(parent, NULL);

    /* the parent hands an inheritable ACE for Everyone down */
    ConvertStringSidToSidW(L"WD", &world);
    memset(&ea, 0, sizeof(ea));
    ea.grfAccessPermissions = GENERIC_READ;
    ea.grfAccessMode = SET_ACCESS;
    ea.grfInheritance = SUB_CONTAINERS_AND_OBJECTS_INHERIT;
    ea.Trustee.TrusteeForm = TRUSTEE_IS_SID;
    ea.Trustee.ptstrName = world;
    SetEntriesInAclW(1, &ea, acl, &everyone);
    err = SetNamedSecurityInfoW(parent, SE_FILE_OBJECT, DACL_SECURITY_INFORMATION | PROTECTED_DACL_SECURITY_INFORMATION,
                                NULL, NULL, everyone, NULL);
    CHECK(!err, "parent DACL set (%lu)", err);

    /* by name (DirectoryInfo.SetAccessControl) */
    CreateDirectoryW(dir, NULL);
    err = SetNamedSecurityInfoW(dir, SE_FILE_OBJECT, DACL_SECURITY_INFORMATION | PROTECTED_DACL_SECURITY_INFORMATION,
                                NULL, NULL, acl, NULL);
    CHECK(!err, "SetNamedSecurityInfoW PROTECTED_DACL (%lu)", err);
    describe(dir, &control, &aces, &inherited);
    CHECK(control & SE_DACL_PROTECTED, "by name: SE_DACL_PROTECTED set (control %#x)", control);
    CHECK(aces == 3 && inherited == 0, "by name: the caller's 3 ACEs only (%d, %d inherited)", aces, inherited);

    /* turning inheritance back on brings the parent's ACE in, marked inherited */
    err = SetNamedSecurityInfoW(dir, SE_FILE_OBJECT, DACL_SECURITY_INFORMATION | UNPROTECTED_DACL_SECURITY_INFORMATION,
                                NULL, NULL, acl, NULL);
    CHECK(!err, "SetNamedSecurityInfoW UNPROTECTED_DACL (%lu)", err);
    describe(dir, &control, &aces, &inherited);
    CHECK(!(control & SE_DACL_PROTECTED), "unprotected: SE_DACL_PROTECTED clear (control %#x)", control);
    /* (a file's DACL is its Unix mode here: what came from the parent is not marked INHERITED_ACE) */
    CHECK(aces > 3, "unprotected: the parent's Everyone grant reaches it again (%d ACEs)", aces);

    /* protecting it again drops them */
    err = SetNamedSecurityInfoW(dir, SE_FILE_OBJECT, DACL_SECURITY_INFORMATION | PROTECTED_DACL_SECURITY_INFORMATION,
                                NULL, NULL, acl, NULL);
    describe(dir, &control, &aces, &inherited);
    CHECK(!err && (control & SE_DACL_PROTECTED) && aces == 3 && inherited == 0,
          "protected again: control %#x, %d ACEs, %d inherited", control, aces, inherited);

    /* by handle */
    CreateDirectoryW(dir2, NULL);
    h = CreateFileW(dir2, READ_CONTROL | WRITE_DAC, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL,
                    OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
    err = SetSecurityInfo(h, SE_FILE_OBJECT, DACL_SECURITY_INFORMATION | PROTECTED_DACL_SECURITY_INFORMATION,
                          NULL, NULL, acl, NULL);
    CloseHandle(h);
    CHECK(!err, "SetSecurityInfo PROTECTED_DACL (%lu)", err);
    describe(dir2, &control, &aces, &inherited);
    CHECK(control & SE_DACL_PROTECTED, "by handle: SE_DACL_PROTECTED set (control %#x)", control);
    CHECK(aces == 3 && inherited == 0, "by handle: the caller's 3 ACEs only (%d, %d inherited)", aces, inherited);

    RemoveDirectoryW(dir2);
    RemoveDirectoryW(dir);
    RemoveDirectoryW(parent);
    printf("RESULT: %s\n", fails ? "FAIL" : "PASS");
    return fails != 0;
}
