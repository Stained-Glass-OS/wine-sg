/* advapi32: GetEffectiveRightsFromAcl, GetAuditedPermissionsFromAcl, BuildSecurityDescriptorA,
 * LookupSecurityDescriptorParts (patch 3734; CORE review: security). A mutant that grants too much and
 * one that denies too much must both fail this probe. */
#define _WIN32_WINNT 0x0600
#include <windows.h>
#include <aclapi.h>
#include <sddl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* not declared by every set of headers */
DWORD WINAPI GetSecurityInfoExW(HANDLE, SE_OBJECT_TYPE, SECURITY_INFORMATION, LPCWSTR, LPCWSTR, PACTRL_ACCESSW *, PACTRL_AUDITW *, LPWSTR *, LPWSTR *);
DWORD WINAPI GetSecurityInfoExA(HANDLE, SE_OBJECT_TYPE, SECURITY_INFORMATION, LPCSTR, LPCSTR, PACTRL_ACCESSA *, PACTRL_AUDITA *, LPSTR *, LPSTR *);
DWORD WINAPI GetNamedSecurityInfoExW(LPCWSTR, SE_OBJECT_TYPE, SECURITY_INFORMATION, LPCWSTR, LPCWSTR, PACTRL_ACCESSW *, PACTRL_AUDITW *, LPWSTR *, LPWSTR *);
DWORD WINAPI GetNamedSecurityInfoExA(LPCSTR, SE_OBJECT_TYPE, SECURITY_INFORMATION, LPCSTR, LPCSTR, PACTRL_ACCESSA *, PACTRL_AUDITA *, LPSTR *, LPSTR *);

static int f;
#define CHECK(c, ...) do { if (!(c)) { f++; printf("FAIL line %d: %s: ", __LINE__, #c); printf(__VA_ARGS__); printf("\n"); } } while (0)

static PSID mksid(const char *s)
{
    PSID sid = NULL;
    if (!ConvertStringSidToSidA(s, &sid)) { printf("cannot make sid %s\n", s); exit(2); }
    return sid;
}

static PACL mkacl(void) { PACL a = LocalAlloc(LPTR, 1024); InitializeAcl(a, 1024, ACL_REVISION); return a; }

static TRUSTEEA tsid(PSID sid)
{
    TRUSTEEA t;
    BuildTrusteeWithSidA(&t, sid);
    return t;
}

static DWORD rights(PACL acl, PSID sid, ACCESS_MASK *m)
{
    TRUSTEEW t;
    *m = 0xdeadbeef;
    BuildTrusteeWithSidW(&t, sid);
    return GetEffectiveRightsFromAclW(acl, &t, m);
}

static DWORD rightsA(PACL acl, PSID sid, ACCESS_MASK *m)
{
    TRUSTEEA t = tsid(sid);
    *m = 0xdeadbeef;
    return GetEffectiveRightsFromAclA(acl, &t, m);
}

int main(void)
{
    PSID a = mksid("S-1-5-21-111-222-333-1001"), b = mksid("S-1-5-21-111-222-333-1002"), world = mksid("S-1-1-0");
    PSID admins = mksid("S-1-5-32-544");
    PACL acl;
    ACCESS_MASK m, s, fl;
    DWORD r;
    TRUSTEEA ta;
    TRUSTEEW tw;

    /* effective rights */
    acl = mkacl();
    AddAccessAllowedAce(acl, ACL_REVISION, 0x7, a);
    r = rights(acl, a, &m);
    CHECK(r == ERROR_SUCCESS && m == 0x7, "plain allow: %lu %#lx", r, m);
    r = rights(acl, b, &m);
    CHECK(r == ERROR_SUCCESS && m == 0, "an ACE for another sid grants nothing: %lu %#lx", r, m);
    r = rightsA(acl, a, &m);
    CHECK(r == ERROR_SUCCESS && m == 0x7, "A form: %lu %#lx", r, m);
    LocalFree(acl);

    acl = mkacl();
    AddAccessDeniedAce(acl, ACL_REVISION, 0x2, a);
    AddAccessAllowedAce(acl, ACL_REVISION, 0x7, a);
    r = rights(acl, a, &m);
    CHECK(r == ERROR_SUCCESS && m == 0x5, "deny before allow: %lu %#lx", r, m);
    r = rights(acl, b, &m);
    CHECK(r == ERROR_SUCCESS && m == 0, "the deny and allow are a's only: %lu %#lx", r, m);
    LocalFree(acl);

    acl = mkacl();
    AddAccessAllowedAce(acl, ACL_REVISION, 0x7, a);
    AddAccessDeniedAce(acl, ACL_REVISION, 0x2, a);
    r = rights(acl, a, &m);
    CHECK(r == ERROR_SUCCESS && m == 0x7, "a deny after the allow does not take bits back: %lu %#lx", r, m);
    LocalFree(acl);

    acl = mkacl();
    AddAccessAllowedAce(acl, ACL_REVISION, 0x100, world);
    AddAccessAllowedAce(acl, ACL_REVISION, 0x1, a);
    AddAccessDeniedAce(acl, ACL_REVISION, 0x100, b);
    r = rights(acl, a, &m);
    CHECK(r == ERROR_SUCCESS && m == 0x101, "Everyone applies to everybody: %lu %#lx", r, m);
    r = rights(acl, b, &m);
    CHECK(r == ERROR_SUCCESS && m == 0x100, "a deny after Everyone's allow does not revoke: %lu %#lx", r, m);
    LocalFree(acl);

    acl = mkacl();
    AddAccessDeniedAce(acl, ACL_REVISION, 0x100, b);
    AddAccessAllowedAce(acl, ACL_REVISION, 0x100, world);
    AddAccessAllowedAce(acl, ACL_REVISION, 0x1, admins);
    r = rights(acl, b, &m);
    CHECK(r == ERROR_SUCCESS && m == 0, "deny first wins for its trustee: %lu %#lx", r, m);
    r = rights(acl, a, &m);
    CHECK(r == ERROR_SUCCESS && m == 0x100, "others get Everyone's bit only (no group ACE of a stranger): %lu %#lx", r, m);
    LocalFree(acl);

    /* inherit-only ACEs do not apply to the object itself */
    acl = mkacl();
    AddAccessAllowedAceEx(acl, ACL_REVISION, INHERIT_ONLY_ACE | OBJECT_INHERIT_ACE, 0x8, a);
    AddAccessAllowedAceEx(acl, ACL_REVISION, OBJECT_INHERIT_ACE, 0x10, a);
    r = rights(acl, a, &m);
    CHECK(r == ERROR_SUCCESS && m == 0x10, "inherit-only ignored: %lu %#lx", r, m);
    LocalFree(acl);

    /* an inherited deny ACE makes the ACL unusable (documented) */
    acl = mkacl();
    AddAccessDeniedAceEx(acl, ACL_REVISION, INHERITED_ACE, 0x1, a);
    AddAccessAllowedAce(acl, ACL_REVISION, 0x7, a);
    r = rights(acl, a, &m);
    CHECK(r == ERROR_INVALID_ACL, "inherited deny: %lu", r);
    LocalFree(acl);

    /* argument errors */
    acl = mkacl();
    r = rights(NULL, a, &m);
    CHECK(r == ERROR_INVALID_PARAMETER, "NULL acl: %lu", r);
    tw.TrusteeForm = TRUSTEE_IS_SID; tw.ptstrName = (WCHAR *)a; tw.pMultipleTrustee = NULL; tw.MultipleTrusteeOperation = NO_MULTIPLE_TRUSTEE;
    r = GetEffectiveRightsFromAclW(acl, NULL, &m);
    CHECK(r == ERROR_INVALID_PARAMETER, "NULL trustee: %lu", r);
    r = GetEffectiveRightsFromAclW(acl, &tw, NULL);
    CHECK(r == ERROR_INVALID_PARAMETER, "NULL result: %lu", r);
    tw.TrusteeForm = (TRUSTEE_FORM)99;
    r = GetEffectiveRightsFromAclW(acl, &tw, &m);
    CHECK(r == ERROR_INVALID_PARAMETER, "bad trustee form: %lu", r);
    r = GetEffectiveRightsFromAclA(acl, NULL, &m);
    CHECK(r == ERROR_INVALID_PARAMETER, "A NULL trustee: %lu", r);
    LocalFree(acl);

    /* trustee given by name and by objects-and-sid */
    acl = mkacl();
    AddAccessAllowedAce(acl, ACL_REVISION, 0x42, world);
    ta.pMultipleTrustee = NULL; ta.MultipleTrusteeOperation = NO_MULTIPLE_TRUSTEE;
    ta.TrusteeForm = TRUSTEE_IS_NAME; ta.TrusteeType = TRUSTEE_IS_UNKNOWN; ta.ptstrName = (char *)"Everyone";
    m = 0; r = GetEffectiveRightsFromAclA(acl, &ta, &m);
    CHECK(r == ERROR_SUCCESS && m == 0x42, "trustee by name: %lu %#lx", r, m);
    {
        OBJECTS_AND_SID os = { 0 };
        os.pSid = world;
        ta.TrusteeForm = TRUSTEE_IS_OBJECTS_AND_SID; ta.ptstrName = (char *)&os;
        m = 0; r = GetEffectiveRightsFromAclA(acl, &ta, &m);
        CHECK(r == ERROR_SUCCESS && m == 0x42, "trustee objects-and-sid: %lu %#lx", r, m);
    }
    ta.TrusteeForm = TRUSTEE_IS_NAME; ta.ptstrName = (char *)"No Such Account Fastb";
    r = GetEffectiveRightsFromAclA(acl, &ta, &m);
    CHECK(r != ERROR_SUCCESS, "unknown name: %lu", r);
    LocalFree(acl);

    /* audited permissions */
    acl = mkacl();
    AddAuditAccessAceEx(acl, ACL_REVISION, 0, 0x1, a, TRUE, FALSE);
    AddAuditAccessAceEx(acl, ACL_REVISION, 0, 0x2, a, FALSE, TRUE);
    AddAuditAccessAceEx(acl, ACL_REVISION, 0, 0x4, a, TRUE, TRUE);
    AddAuditAccessAceEx(acl, ACL_REVISION, 0, 0x8, b, TRUE, TRUE);
    AddAuditAccessAceEx(acl, ACL_REVISION, 0, 0x100, world, FALSE, TRUE);
    AddAuditAccessAceEx(acl, ACL_REVISION, INHERIT_ONLY_ACE | OBJECT_INHERIT_ACE, 0x200, a, TRUE, TRUE);
    tw.TrusteeForm = TRUSTEE_IS_SID; tw.ptstrName = (WCHAR *)a;
    s = fl = 0xdeadbeef;
    r = GetAuditedPermissionsFromAclW(acl, &tw, &s, &fl);
    CHECK(r == ERROR_SUCCESS && s == 0x5 && fl == 0x106, "audited W: %lu success %#lx failure %#lx", r, s, fl);
    ta = tsid(b);
    s = fl = 0xdeadbeef;
    r = GetAuditedPermissionsFromAclA(acl, &ta, &s, &fl);
    CHECK(r == ERROR_SUCCESS && s == 0x8 && fl == 0x108, "audited A: %lu success %#lx failure %#lx", r, s, fl);
    r = GetAuditedPermissionsFromAclW(acl, &tw, NULL, &fl);
    CHECK(r == ERROR_INVALID_PARAMETER, "audited NULL: %lu", r);
    r = GetAuditedPermissionsFromAclW(NULL, &tw, &s, &fl);
    CHECK(r == ERROR_INVALID_PARAMETER, "audited NULL acl: %lu", r);
    LocalFree(acl);

    /* BuildSecurityDescriptorA */
    {
        EXPLICIT_ACCESSA ea[2], au[1];
        TRUSTEEA owner, group;
        PSECURITY_DESCRIPTOR sd = NULL;
        ULONG len = 0;
        PSID o, g;
        BOOL def, present;
        PACL dacl, sacl;

        memset(ea, 0, sizeof(ea)); memset(au, 0, sizeof(au));
        BuildExplicitAccessWithNameA(&ea[0], (char *)"Everyone", 0x11, GRANT_ACCESS, NO_INHERITANCE);
        BuildExplicitAccessWithNameA(&ea[1], (char *)"Everyone", 0x20, DENY_ACCESS, NO_INHERITANCE);
        BuildExplicitAccessWithNameA(&au[0], (char *)"Everyone", 0x4, SET_AUDIT_FAILURE, NO_INHERITANCE);
        BuildTrusteeWithSidA(&owner, a);
        BuildTrusteeWithNameA(&group, (char *)"Everyone");
        r = BuildSecurityDescriptorA(&owner, &group, 2, ea, 1, au, NULL, &len, &sd);
        CHECK(r == ERROR_SUCCESS && sd && len, "BuildSecurityDescriptorA: %lu len %lu", r, len);
        if (r == ERROR_SUCCESS && sd)
        {
            CHECK(IsValidSecurityDescriptor(sd), "valid descriptor");
            GetSecurityDescriptorOwner(sd, &o, &def);
            GetSecurityDescriptorGroup(sd, &g, &def);
            CHECK(o && EqualSid(o, a), "owner from the sid trustee");
            CHECK(g && EqualSid(g, world), "group from the name trustee");
            GetSecurityDescriptorDacl(sd, &present, &dacl, &def);
            CHECK(present && dacl && dacl->AceCount == 2, "two DACL entries (%d)", dacl ? dacl->AceCount : -1);
            r = rights(dacl, b, &m);
            CHECK(r == ERROR_SUCCESS && m == 0x11, "built DACL grants 0x11 and denies 0x20: %lu %#lx", r, m);
            GetSecurityDescriptorSacl(sd, &present, &sacl, &def);
            CHECK(present && sacl && sacl->AceCount == 1, "one SACL entry");
            tw.TrusteeForm = TRUSTEE_IS_SID; tw.ptstrName = (WCHAR *)world;
            r = GetAuditedPermissionsFromAclW(sacl, &tw, &s, &fl);
            CHECK(r == ERROR_SUCCESS && s == 0 && fl == 0x4, "built SACL audits failures only: %lu %#lx %#lx", r, s, fl);

            /* LookupSecurityDescriptorParts */
            {
                PTRUSTEEW ow = NULL, gw = NULL;
                PTRUSTEEA oa = NULL, ga = NULL;
                PEXPLICIT_ACCESSW al = NULL, ul = NULL;
                PEXPLICIT_ACCESSA ala = NULL, ula = NULL;
                ULONG ac = 99, uc = 99;
                WCHAR sidstr[128];
                LPWSTR str;

                r = LookupSecurityDescriptorPartsW(&ow, &gw, &ac, &al, &uc, &ul, sd);
                CHECK(r == ERROR_SUCCESS, "LookupSecurityDescriptorPartsW %lu", r);
                if (r == ERROR_SUCCESS)
                {
                    ConvertSidToStringSidW(a, &str); lstrcpyW(sidstr, str); LocalFree(str);
                    CHECK(ow && ow->TrusteeForm == TRUSTEE_IS_NAME && ow->ptstrName && !wcscmp(ow->ptstrName, sidstr),
                          "owner name %ls", ow && ow->ptstrName ? ow->ptstrName : L"(null)");
                    CHECK(gw && gw->TrusteeForm == TRUSTEE_IS_NAME && gw->ptstrName && !wcscmp(gw->ptstrName, L"Everyone"),
                          "group name %ls", gw && gw->ptstrName ? gw->ptstrName : L"(null)");
                    CHECK(gw && gw->TrusteeType == TRUSTEE_IS_WELL_KNOWN_GROUP, "group type %d", gw ? gw->TrusteeType : -1);
                    CHECK(ac == 2 && al && al[1].grfAccessMode == GRANT_ACCESS && al[1].grfAccessPermissions == 0x11 &&
                          al[0].grfAccessMode == DENY_ACCESS && al[0].grfAccessPermissions == 0x20, "access entries %lu", ac);
                    CHECK(ac == 2 && al && al[0].Trustee.TrusteeForm == TRUSTEE_IS_NAME &&
                          !wcscmp(al[0].Trustee.ptstrName, L"Everyone"), "access trustee is the name");
                    CHECK(uc == 1 && ul && ul[0].grfAccessMode == SET_AUDIT_FAILURE && ul[0].grfAccessPermissions == 0x4,
                          "audit entries %lu", uc);
                    LocalFree(ow); LocalFree(gw); LocalFree(al); LocalFree(ul);
                }
                ac = uc = 99; ow = gw = NULL;
                r = LookupSecurityDescriptorPartsW(&ow, NULL, NULL, NULL, NULL, NULL, sd);
                CHECK(r == ERROR_SUCCESS && ow && !gw, "owner only: %lu", r);
                LocalFree(ow);
                r = LookupSecurityDescriptorPartsW(NULL, NULL, NULL, &al, NULL, NULL, sd);
                CHECK(r == ERROR_INVALID_PARAMETER, "list without count: %lu", r);
                r = LookupSecurityDescriptorPartsW(NULL, NULL, NULL, NULL, NULL, NULL, NULL);
                CHECK(r != ERROR_SUCCESS, "NULL descriptor: %lu", r);

                ac = uc = 99;
                r = LookupSecurityDescriptorPartsA(&oa, &ga, &ac, &ala, &uc, &ula, sd);
                CHECK(r == ERROR_SUCCESS, "LookupSecurityDescriptorPartsA %lu", r);
                if (r == ERROR_SUCCESS)
                {
                    CHECK(ga && ga->TrusteeForm == TRUSTEE_IS_NAME && !strcmp(ga->ptstrName, "Everyone"), "A group name");
                    CHECK(ac == 2 && ala && ala[0].grfAccessMode == DENY_ACCESS && ala[0].grfAccessPermissions == 0x20 &&
                          !strcmp(ala[0].Trustee.ptstrName, "Everyone"), "A access entries");
                    CHECK(uc == 1 && ula && ula[0].grfAccessMode == SET_AUDIT_FAILURE, "A audit entries");
                    LocalFree(oa); LocalFree(ga); LocalFree(ala); LocalFree(ula);
                }
            }
            LocalFree(sd);
        }
        /* an absolute descriptor is rejected */
        {
            SECURITY_DESCRIPTOR abs;
            PTRUSTEEW ow;
            InitializeSecurityDescriptor(&abs, SECURITY_DESCRIPTOR_REVISION);
            r = LookupSecurityDescriptorPartsW(&ow, NULL, NULL, NULL, NULL, NULL, &abs);
            CHECK(r == ERROR_INVALID_SECURITY_DESCR, "absolute descriptor: %lu", r);
        }
        /* a bad trustee fails and leaves no descriptor */
        {
            TRUSTEEA bad;
            sd = (PSECURITY_DESCRIPTOR)1; len = 0;
            BuildTrusteeWithNameA(&bad, (char *)"No Such Account Fastb");
            r = BuildSecurityDescriptorA(&bad, NULL, 0, NULL, 0, NULL, NULL, &len, &sd);
            CHECK(r != ERROR_SUCCESS, "unknown owner name: %lu", r);
        }
    }

    /* GetSecurityInfoEx / GetNamedSecurityInfoEx (the Windows provider) */
    {
        SECURITY_ATTRIBUTES sa = { sizeof(sa) };
        HANDLE ev;
        PACTRL_ACCESSW accw = NULL; PACTRL_AUDITW audw = NULL; LPWSTR ownw = NULL, grpw = NULL;
        PACTRL_ACCESSA acca = NULL; PACTRL_AUDITA auda = NULL; LPSTR owna = NULL, grpa = NULL;
        ACTRL_ACCESS_ENTRYW *ew;
        HANDLE fh;

        ConvertStringSecurityDescriptorToSecurityDescriptorA("O:BAG:SYD:(D;;0x2;;;BA)(A;;0x1f0003;;;WD)", SDDL_REVISION_1, &sa.lpSecurityDescriptor, NULL);
        ev = CreateEventA(&sa, TRUE, FALSE, NULL);
        r = GetSecurityInfoExW(ev, SE_KERNEL_OBJECT, DACL_SECURITY_INFORMATION | OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION,
                               NULL, NULL, &accw, &audw, &ownw, &grpw);
        CHECK(r == ERROR_SUCCESS, "GetSecurityInfoExW %lu", r);
        if (r == ERROR_SUCCESS)
        {
            CHECK(accw && accw->cEntries == 1 && accw->pPropertyAccessList && !accw->pPropertyAccessList->lpProperty &&
                  accw->pPropertyAccessList->pAccessEntryList && accw->pPropertyAccessList->pAccessEntryList->cEntries == 2, "access list shape");
            if (accw && accw->pPropertyAccessList && accw->pPropertyAccessList->pAccessEntryList &&
                accw->pPropertyAccessList->pAccessEntryList->cEntries == 2)
            {
                ew = accw->pPropertyAccessList->pAccessEntryList->pAccessList;
                CHECK(ew[0].fAccessFlags == ACTRL_ACCESS_DENIED && ew[0].Access == 0x2 && ew[0].Trustee.TrusteeForm == TRUSTEE_IS_NAME &&
                      !wcscmp(ew[0].Trustee.ptstrName, L"BUILTIN\\Administrators"), "entry 0: flags %lu access %#lx name %ls", ew[0].fAccessFlags, ew[0].Access, ew[0].Trustee.ptstrName);
                CHECK(ew[1].fAccessFlags == ACTRL_ACCESS_ALLOWED && ew[1].Access == 0x1f0003 && !wcscmp(ew[1].Trustee.ptstrName, L"Everyone"),
                      "entry 1: flags %lu access %#lx name %ls", ew[1].fAccessFlags, ew[1].Access, ew[1].Trustee.ptstrName);
            }
            CHECK(!audw, "no audit list when the SACL was not asked for");
            CHECK(ownw && !wcscmp(ownw, L"BUILTIN\\Administrators"), "owner %ls", ownw ? ownw : L"(null)");
            CHECK(grpw && wcsstr(grpw, L"SYSTEM"), "group %ls", grpw ? grpw : L"(null)");
            LocalFree(accw); LocalFree(ownw); LocalFree(grpw);
        }
        accw = NULL;
        r = GetSecurityInfoExA(ev, SE_KERNEL_OBJECT, DACL_SECURITY_INFORMATION | OWNER_SECURITY_INFORMATION, NULL, NULL, &acca, &auda, &owna, &grpa);
        CHECK(r == ERROR_SUCCESS && acca && acca->pPropertyAccessList->pAccessEntryList->cEntries == 2 &&
              !strcmp(acca->pPropertyAccessList->pAccessEntryList->pAccessList[1].Trustee.ptstrName, "Everyone") &&
              acca->pPropertyAccessList->pAccessEntryList->pAccessList[1].fAccessFlags == ACTRL_ACCESS_ALLOWED &&
              owna && !strcmp(owna, "BUILTIN\\Administrators") && !grpa, "GetSecurityInfoExA %lu", r);
        LocalFree(acca); LocalFree(owna);
        r = GetSecurityInfoExW(ev, SE_KERNEL_OBJECT, DACL_SECURITY_INFORMATION, L"No Such Provider", NULL, &accw, NULL, NULL, NULL);
        CHECK(r == ERROR_BAD_PROVIDER, "unknown provider: %lu", r);
        r = GetSecurityInfoExW(ev, SE_KERNEL_OBJECT, DACL_SECURITY_INFORMATION, L"Windows Provider", NULL, &accw, NULL, NULL, NULL);
        CHECK(r == ERROR_SUCCESS && accw, "named provider: %lu", r);
        LocalFree(accw); accw = NULL;
        r = GetSecurityInfoExW(ev, SE_KERNEL_OBJECT, DACL_SECURITY_INFORMATION, NULL, L"prop", &accw, NULL, NULL, NULL);
        CHECK(r == ERROR_INVALID_PARAMETER, "property: %lu", r);
        r = GetSecurityInfoExW((HANDLE)0x1234, SE_KERNEL_OBJECT, DACL_SECURITY_INFORMATION, NULL, NULL, &accw, NULL, NULL, NULL);
        CHECK(r != ERROR_SUCCESS && !accw, "bad handle: %lu", r);
        CloseHandle(ev);
        LocalFree(sa.lpSecurityDescriptor);

        fh = CreateFileA("fastb-secacl.tmp", GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
        CloseHandle(fh);
        r = GetNamedSecurityInfoExW(L"fastb-secacl.tmp", SE_FILE_OBJECT, DACL_SECURITY_INFORMATION | OWNER_SECURITY_INFORMATION, NULL, NULL, &accw, NULL, &ownw, NULL);
        CHECK(r == ERROR_SUCCESS && accw && ownw && accw->pPropertyAccessList->pAccessEntryList->cEntries >= 1, "GetNamedSecurityInfoExW on a file: %lu", r);
        LocalFree(accw); LocalFree(ownw); accw = NULL; ownw = NULL;
        r = GetNamedSecurityInfoExA("fastb-secacl.tmp", SE_FILE_OBJECT, DACL_SECURITY_INFORMATION, NULL, NULL, &acca, NULL, NULL, NULL);
        CHECK(r == ERROR_SUCCESS && acca, "GetNamedSecurityInfoExA on a file: %lu", r);
        LocalFree(acca);
        r = GetNamedSecurityInfoExW(L"fastb-no-such-file.tmp", SE_FILE_OBJECT, DACL_SECURITY_INFORMATION, NULL, NULL, &accw, NULL, NULL, NULL);
        CHECK(r != ERROR_SUCCESS && !accw, "missing file: %lu", r);
        r = GetNamedSecurityInfoExW(NULL, SE_FILE_OBJECT, DACL_SECURITY_INFORMATION, NULL, NULL, &accw, NULL, NULL, NULL);
        CHECK(r == ERROR_INVALID_PARAMETER, "NULL name: %lu", r);
        DeleteFileA("fastb-secacl.tmp");
    }

    printf("%s\n", f ? "RESULT: FAIL" : "RESULT: PASS");
    return f != 0;
}
