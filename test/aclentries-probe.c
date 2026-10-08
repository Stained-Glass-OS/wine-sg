/* SetEntriesInAclW builds the ACL as Windows does (patches/sg/1609):
 * explicit denies, then explicit allows, then inherited ACEs; SET_ACCESS
 * replaces the trustee's explicit allowed and denied ACEs, REVOKE_ACCESS
 * removes its allowed ones and keeps its denied ones, GRANT_ACCESS folds
 * the trustee's allowed ACE into the new one. */
#include <windows.h>
#include <aclapi.h>
#include <stdio.h>

static int failures;
static PSID users, everyone;

static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) failures++;
}

static PACL make_acl(void)
{
    PACL acl = LocalAlloc(LPTR, 512);
    InitializeAcl(acl, 512, ACL_REVISION);
    return acl;
}

static void entry(EXPLICIT_ACCESSW *ea, ACCESS_MODE mode, DWORD mask, PSID sid)
{
    memset(ea, 0, sizeof(*ea));
    ea->grfAccessMode = mode;
    ea->grfAccessPermissions = mask;
    ea->grfInheritance = NO_INHERITANCE;
    ea->Trustee.TrusteeForm = TRUSTEE_IS_SID;
    ea->Trustee.TrusteeType = TRUSTEE_IS_GROUP;
    ea->Trustee.ptstrName = sid;
}

static void dump(const char *name, PACL acl)
{
    ACE_HEADER *ace;
    DWORD i;
    printf("%s:", name);
    for (i = 0; acl && i < acl->AceCount && GetAce(acl, i, (void **)&ace); i++)
        printf(" [%s%s %s %#lx]", ace->AceType == ACCESS_DENIED_ACE_TYPE ? "deny" : ace->AceType == ACCESS_ALLOWED_ACE_TYPE ? "allow" : "other",
               ace->AceFlags & INHERITED_ACE ? "(inh)" : "",
               EqualSid((PSID)&((ACCESS_ALLOWED_ACE *)ace)->SidStart, users) ? "Users" : "Everyone",
               ((ACCESS_ALLOWED_ACE *)ace)->Mask);
    printf("\n");
}

static BOOL ace_is(PACL acl, DWORD idx, BYTE type, PSID sid, DWORD mask, BYTE flags)
{
    ACCESS_ALLOWED_ACE *ace;
    if (!acl || idx >= acl->AceCount || !GetAce(acl, idx, (void **)&ace)) return FALSE;
    return ace->Header.AceType == type && EqualSid((PSID)&ace->SidStart, sid) && ace->Mask == mask &&
           ace->Header.AceFlags == flags;
}

int main(void)
{
    SID_IDENTIFIER_AUTHORITY nt = {SECURITY_NT_AUTHORITY}, world = {SECURITY_WORLD_SID_AUTHORITY};
    EXPLICIT_ACCESSW ea;
    PACL old, acl;
    DWORD res;

    AllocateAndInitializeSid(&nt, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_USERS, 0, 0, 0, 0, 0, 0, &users);
    AllocateAndInitializeSid(&world, 1, SECURITY_WORLD_RID, 0, 0, 0, 0, 0, 0, 0, &everyone);

    /* a grant goes after an existing deny */
    old = make_acl();
    AddAccessDeniedAce(old, ACL_REVISION, FILE_WRITE_DATA, everyone);
    entry(&ea, GRANT_ACCESS, FILE_READ_DATA, users);
    acl = NULL;
    res = SetEntriesInAclW(1, &ea, old, &acl);
    dump("grant after deny", acl);
    check(!res && acl->AceCount == 2 && ace_is(acl, 0, ACCESS_DENIED_ACE_TYPE, everyone, FILE_WRITE_DATA, 0) &&
          ace_is(acl, 1, ACCESS_ALLOWED_ACE_TYPE, users, FILE_READ_DATA, 0), "a new grant goes after the existing deny");
    LocalFree(acl); LocalFree(old);

    /* SET_ACCESS replaces the trustee's allowed and denied ACEs */
    old = make_acl();
    AddAccessDeniedAce(old, ACL_REVISION, FILE_WRITE_DATA, users);
    AddAccessAllowedAce(old, ACL_REVISION, FILE_READ_DATA, users);
    AddAccessAllowedAce(old, ACL_REVISION, FILE_READ_DATA, everyone);
    entry(&ea, SET_ACCESS, FILE_EXECUTE, users);
    acl = NULL;
    res = SetEntriesInAclW(1, &ea, old, &acl);
    dump("set", acl);
    check(!res && acl->AceCount == 2 && ace_is(acl, 0, ACCESS_ALLOWED_ACE_TYPE, users, FILE_EXECUTE, 0) &&
          ace_is(acl, 1, ACCESS_ALLOWED_ACE_TYPE, everyone, FILE_READ_DATA, 0), "SET_ACCESS replaces the trustee's ACEs");
    LocalFree(acl);

    /* REVOKE_ACCESS removes the allowed ACE, keeps the denied one */
    entry(&ea, REVOKE_ACCESS, 0, users);
    acl = NULL;
    res = SetEntriesInAclW(1, &ea, old, &acl);
    dump("revoke", acl);
    check(!res && acl->AceCount == 2 && ace_is(acl, 0, ACCESS_DENIED_ACE_TYPE, users, FILE_WRITE_DATA, 0) &&
          ace_is(acl, 1, ACCESS_ALLOWED_ACE_TYPE, everyone, FILE_READ_DATA, 0), "REVOKE_ACCESS keeps the deny, drops the allow");
    LocalFree(acl); LocalFree(old);

    /* GRANT_ACCESS folds the existing allowed ACE in */
    old = make_acl();
    AddAccessAllowedAce(old, ACL_REVISION, FILE_READ_DATA, users);
    entry(&ea, GRANT_ACCESS, FILE_WRITE_DATA, users);
    acl = NULL;
    res = SetEntriesInAclW(1, &ea, old, &acl);
    dump("grant merge", acl);
    check(!res && acl->AceCount == 1 && ace_is(acl, 0, ACCESS_ALLOWED_ACE_TYPE, users, FILE_READ_DATA | FILE_WRITE_DATA, 0),
          "GRANT_ACCESS folds the trustee's allowed ACE into one");
    LocalFree(acl); LocalFree(old);

    /* inherited ACEs stay last */
    old = make_acl();
    AddAccessAllowedAceEx(old, ACL_REVISION, INHERITED_ACE, FILE_READ_DATA, users);
    entry(&ea, DENY_ACCESS, FILE_WRITE_DATA, everyone);
    acl = NULL;
    res = SetEntriesInAclW(1, &ea, old, &acl);
    dump("inherited", acl);
    check(!res && acl->AceCount == 2 && ace_is(acl, 0, ACCESS_DENIED_ACE_TYPE, everyone, FILE_WRITE_DATA, 0) &&
          ace_is(acl, 1, ACCESS_ALLOWED_ACE_TYPE, users, FILE_READ_DATA, INHERITED_ACE), "inherited ACEs stay after the explicit ones");
    LocalFree(acl); LocalFree(old);

    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
