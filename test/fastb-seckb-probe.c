/* kernelbase / ntdll / advapi32 security wrappers (patch 3735; CORE review: security):
 * AccessCheckByType, SetAclInformation, SetPrivateObjectSecurity(Ex), GetWindowsAccountDomainSid,
 * the object ACE functions, RtlDefaultNpAcl and the audit alarm functions. Both a mutant that grants too much
 * and one that denies too much must fail this probe. */
#define _WIN32_WINNT 0x0600
#include <windows.h>
#include <sddl.h>
#include <winternl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int f;
#define CHECK(c, ...) do { if (!(c)) { f++; printf("FAIL line %d: %s: ", __LINE__, #c); printf(__VA_ARGS__); printf("\n"); } } while (0)

typedef LONG NTSTATUS;
static NTSTATUS (WINAPI *pRtlAddAccessAllowedObjectAce)(PACL, DWORD, DWORD, DWORD, GUID *, GUID *, PSID);
static NTSTATUS (WINAPI *pRtlAddAccessDeniedObjectAce)(PACL, DWORD, DWORD, DWORD, GUID *, GUID *, PSID);
static NTSTATUS (WINAPI *pRtlAddAuditAccessObjectAce)(PACL, DWORD, DWORD, DWORD, GUID *, GUID *, PSID, BOOL, BOOL);
static NTSTATUS (WINAPI *pRtlDefaultNpAcl)(PACL *);
static BOOL (WINAPI *pAccessCheckByType)(PSECURITY_DESCRIPTOR, PSID, HANDLE, DWORD, POBJECT_TYPE_LIST, DWORD,
        PGENERIC_MAPPING, PPRIVILEGE_SET, LPDWORD, LPDWORD, LPBOOL);
static BOOL (WINAPI *pSetPrivateObjectSecurityEx)(SECURITY_INFORMATION, PSECURITY_DESCRIPTOR, PSECURITY_DESCRIPTOR *,
        ULONG, PGENERIC_MAPPING, HANDLE);
static BOOL (WINAPI *pObjectOpenAuditAlarmA)(LPCSTR, LPVOID, LPSTR, LPSTR, PSECURITY_DESCRIPTOR, HANDLE, DWORD, DWORD,
        PPRIVILEGE_SET, BOOL, BOOL, LPBOOL);
static BOOL (WINAPI *pObjectCloseAuditAlarmA)(LPCSTR, LPVOID, BOOL);
static BOOL (WINAPI *pObjectPrivilegeAuditAlarmA)(LPCSTR, LPVOID, HANDLE, DWORD, PPRIVILEGE_SET, BOOL);
static BOOL (WINAPI *pPrivilegedServiceAuditAlarmA)(LPCSTR, LPCSTR, HANDLE, PPRIVILEGE_SET, BOOL);
static BOOL (WINAPI *pObjectOpenAuditAlarmW)(LPCWSTR, LPVOID, LPWSTR, LPWSTR, PSECURITY_DESCRIPTOR, HANDLE, DWORD, DWORD,
        PPRIVILEGE_SET, BOOL, BOOL, LPBOOL);

static PSID mksid(const char *s)
{
    PSID sid = NULL;
    if (!ConvertStringSidToSidA(s, &sid)) { printf("cannot make sid %s\n", s); exit(2); }
    return sid;
}

static const GENERIC_MAPPING map = { 0x1, 0x2, 0x4, 0xffff };

static GUID R = { 0x11111111, 0x1111, 0x1111, { 1, 1, 1, 1, 1, 1, 1, 1 } };
static GUID C = { 0x22222222, 0x2222, 0x2222, { 2, 2, 2, 2, 2, 2, 2, 2 } };
static GUID D = { 0x33333333, 0x3333, 0x3333, { 3, 3, 3, 3, 3, 3, 3, 3 } };
static GUID X = { 0x44444444, 0x4444, 0x4444, { 4, 4, 4, 4, 4, 4, 4, 4 } };

/* a self-relative SD with owner = the user, and the given DACL */
static PSECURITY_DESCRIPTOR mksd(PSID owner, PACL dacl)
{
    SECURITY_DESCRIPTOR abs;
    PSECURITY_DESCRIPTOR rel;
    DWORD len = 0;
    InitializeSecurityDescriptor(&abs, SECURITY_DESCRIPTOR_REVISION);
    SetSecurityDescriptorOwner(&abs, owner, FALSE);
    SetSecurityDescriptorGroup(&abs, owner, FALSE);
    SetSecurityDescriptorDacl(&abs, TRUE, dacl, FALSE);
    MakeSelfRelativeSD(&abs, NULL, &len);
    rel = LocalAlloc(LPTR, len);
    if (!MakeSelfRelativeSD(&abs, rel, &len)) { printf("cannot make sd %lu\n", GetLastError()); exit(2); }
    return rel;
}

static PACL newacl(void) { PACL a = LocalAlloc(LPTR, 2048); InitializeAcl(a, 2048, 4); return a; }

static HANDLE imp_token;

/* returns 1 if granted, 0 if denied, -1 on API failure */
static int abt(PSECURITY_DESCRIPTOR sd, PSID self, DWORD access, POBJECT_TYPE_LIST types, DWORD n, DWORD *granted_out)
{
    PRIVILEGE_SET ps; DWORD pslen = sizeof(ps), granted = 0xdeadbeef; BOOL status = 0xdead;
    SetLastError(0xdeadbeef);
    if (!pAccessCheckByType(sd, self, imp_token, access, types, n, (GENERIC_MAPPING *)&map, &ps, &pslen, &granted, &status))
        return -1;
    if (granted_out) *granted_out = granted;
    if (!status) CHECK(GetLastError() == ERROR_ACCESS_DENIED, "denied sets ERROR_ACCESS_DENIED, got %lu", GetLastError());
    return status ? 1 : 0;
}

int main(void)
{
    HMODULE ntdll = GetModuleHandleA("ntdll.dll"), kb = GetModuleHandleA("kernelbase.dll"), adv = LoadLibraryA("advapi32.dll");
    PSID world = mksid("S-1-1-0"), self_sid = mksid("S-1-5-10"), admins = mksid("S-1-5-32-544"), user;
    BYTE ubuf[256];
    DWORD len, r, g;
    HANDLE ptok;
    PSECURITY_DESCRIPTOR sd;
    PACL acl;
    int ret;

#define GETP(var, mod, name) p##var = (void *)GetProcAddress(mod, #name)
    pRtlAddAccessAllowedObjectAce = (void *)GetProcAddress(ntdll, "RtlAddAccessAllowedObjectAce");
    pRtlAddAccessDeniedObjectAce = (void *)GetProcAddress(ntdll, "RtlAddAccessDeniedObjectAce");
    pRtlAddAuditAccessObjectAce = (void *)GetProcAddress(ntdll, "RtlAddAuditAccessObjectAce");
    pRtlDefaultNpAcl = (void *)GetProcAddress(ntdll, "RtlDefaultNpAcl");
    pAccessCheckByType = (void *)GetProcAddress(adv, "AccessCheckByType");
    pSetPrivateObjectSecurityEx = (void *)GetProcAddress(adv, "SetPrivateObjectSecurityEx");
    pObjectOpenAuditAlarmA = (void *)GetProcAddress(adv, "ObjectOpenAuditAlarmA");
    pObjectOpenAuditAlarmW = (void *)GetProcAddress(adv, "ObjectOpenAuditAlarmW");
    pObjectCloseAuditAlarmA = (void *)GetProcAddress(adv, "ObjectCloseAuditAlarmA");
    pObjectPrivilegeAuditAlarmA = (void *)GetProcAddress(adv, "ObjectPrivilegeAuditAlarmA");
    pPrivilegedServiceAuditAlarmA = (void *)GetProcAddress(adv, "PrivilegedServiceAuditAlarmA");
    (void)kb;

    OpenProcessToken(GetCurrentProcess(), TOKEN_ALL_ACCESS, &ptok);
    GetTokenInformation(ptok, TokenUser, ubuf, sizeof(ubuf), &len);
    user = ((TOKEN_USER *)ubuf)->User.Sid;
    DuplicateTokenEx(ptok, TOKEN_ALL_ACCESS, NULL, SecurityImpersonation, TokenImpersonation, &imp_token);

    /* ---- AccessCheckByType without a type list behaves as AccessCheck ---- */
    acl = newacl();
    AddAccessAllowedAce(acl, 4, 0x1, world);
    sd = mksd(user, acl);
    ret = abt(sd, NULL, 0x1, NULL, 0, &g);
    CHECK(ret == 1 && g == 0x1, "plain allow: %d granted %#lx", ret, g);
    ret = abt(sd, NULL, 0x2, NULL, 0, &g);
    CHECK(ret == 0 && g == 0, "right not in the ACL: %d granted %#lx", ret, g);
    ret = abt(sd, NULL, 0x3, NULL, 0, &g);
    CHECK(ret == 0, "one right of two missing: %d", ret);
    LocalFree(sd); LocalFree(acl);

    acl = newacl();
    AddAccessDeniedAce(acl, 4, 0x1, user);
    AddAccessAllowedAce(acl, 4, 0x7, world);
    sd = mksd(user, acl);
    ret = abt(sd, NULL, 0x1, NULL, 0, &g);
    CHECK(ret == 0, "deny first: %d", ret);
    ret = abt(sd, NULL, 0x6, NULL, 0, &g);
    CHECK(ret == 1, "the other bits are granted: %d", ret);
    LocalFree(sd); LocalFree(acl);

    /* ---- object ACEs against a type list ---- */
    {
        OBJECT_TYPE_LIST tl[3] = { { 0, 0, &R }, { 1, 0, &C }, { 1, 0, &D } };
        DWORD a;

        /* an allow for the child type does not grant the root */
        acl = newacl();
        pRtlAddAccessAllowedObjectAce(acl, 4, 0, 0x4, &C, NULL, world);
        sd = mksd(user, acl);
        ret = abt(sd, NULL, 0x4, tl, 3, &g);
        CHECK(ret == 0, "a child's allow does not grant the parent: %d", ret);
        ret = abt(sd, NULL, 0x4, NULL, 0, &g);
        CHECK(ret == 0, "without a type list an object allow grants nothing: %d", ret);
        LocalFree(sd); LocalFree(acl);

        /* an allow for the root type grants it */
        acl = newacl();
        pRtlAddAccessAllowedObjectAce(acl, 4, 0, 0x4, &R, NULL, world);
        sd = mksd(user, acl);
        ret = abt(sd, NULL, 0x4, tl, 3, &g);
        CHECK(ret == 1 && g == 0x4, "allow for the root type: %d %#lx", ret, g);
        ret = abt(sd, NULL, 0x4, &tl[1], 1, &g);
        CHECK(ret == -1 && GetLastError() == ERROR_INVALID_PARAMETER, "a list not starting at level 0: %d %lu", ret, GetLastError());
        LocalFree(sd); LocalFree(acl);

        /* an allow for an unrelated type grants nothing */
        acl = newacl();
        pRtlAddAccessAllowedObjectAce(acl, 4, 0, 0x4, &X, NULL, world);
        sd = mksd(user, acl);
        ret = abt(sd, NULL, 0x4, tl, 3, &g);
        CHECK(ret == 0, "an allow for a type not in the list: %d", ret);
        LocalFree(sd); LocalFree(acl);

        /* a deny for a child removes the right from the root */
        acl = newacl();
        pRtlAddAccessDeniedObjectAce(acl, 4, 0, 0x8, &C, NULL, world);
        AddAccessAllowedAce(acl, 4, 0xf, world);
        sd = mksd(user, acl);
        ret = abt(sd, NULL, 0x8, tl, 3, &g);
        CHECK(ret == 0, "a child's deny takes the right from the parent: %d", ret);
        ret = abt(sd, NULL, 0x7, tl, 3, &g);
        CHECK(ret == 1, "other bits are still granted: %d", ret);
        LocalFree(sd); LocalFree(acl);

        /* a deny for an unrelated type does not apply */
        acl = newacl();
        pRtlAddAccessDeniedObjectAce(acl, 4, 0, 0x8, &X, NULL, world);
        AddAccessAllowedAce(acl, 4, 0xf, world);
        sd = mksd(user, acl);
        ret = abt(sd, NULL, 0x8, tl, 3, &g);
        CHECK(ret == 1, "a deny for a type not in the list: %d", ret);
        LocalFree(sd); LocalFree(acl);

        /* a deny object ACE with a type still denies without a list (the conservative reading) */
        acl = newacl();
        pRtlAddAccessDeniedObjectAce(acl, 4, 0, 0x8, &X, NULL, world);
        AddAccessAllowedAce(acl, 4, 0xf, world);
        sd = mksd(user, acl);
        ret = abt(sd, NULL, 0x8, NULL, 0, &g);
        CHECK(ret == 0, "object deny without a list: %d", ret);
        LocalFree(sd); LocalFree(acl);

        /* an object ACE without a type is a plain ACE */
        acl = newacl();
        pRtlAddAccessAllowedObjectAce(acl, 4, 0, 0x10, NULL, NULL, world);
        sd = mksd(user, acl);
        ret = abt(sd, NULL, 0x10, tl, 3, &g);
        CHECK(ret == 1, "typeless object ACE: %d", ret);
        LocalFree(sd); LocalFree(acl);

        /* argument errors */
        a = 0;
        ret = abt(sd, NULL, 0x1, tl, 0, &g);
        CHECK(ret == -1 && GetLastError() == ERROR_INVALID_PARAMETER, "list without a count: %d", ret);
        ret = abt(sd, NULL, 0x1, NULL, 2, &g);
        CHECK(ret == -1 && GetLastError() == ERROR_INVALID_PARAMETER, "count without a list: %d", ret);
        tl[2].Level = 5;
        ret = abt(sd, NULL, 0x1, tl, 3, &g);
        CHECK(ret == -1 && GetLastError() == ERROR_INVALID_PARAMETER, "level jumps by more than one: %d", ret);
        tl[2].Level = 1;
        (void)a;
    }

    /* ---- PRINCIPAL_SELF ---- */
    acl = newacl();
    AddAccessAllowedAce(acl, 4, 0x20, self_sid);
    sd = mksd(user, acl);
    ret = abt(sd, user, 0x20, NULL, 0, &g);
    CHECK(ret == 1, "self sid given: %d", ret);
    ret = abt(sd, mksid("S-1-5-21-9-9-9-1234"), 0x20, NULL, 0, &g);
    CHECK(ret == 0, "self sid of someone else: %d", ret);
    ret = abt(sd, NULL, 0x20, NULL, 0, &g);
    CHECK(ret == 0, "no self sid: %d", ret);
    LocalFree(sd); LocalFree(acl);

    /* ---- SetAclInformation ---- */
    {
        ACL_REVISION_INFORMATION ri;
        ACL_SIZE_INFORMATION si = { 0 };
        acl = newacl();
        ri.AclRevision = 4;
        CHECK(SetAclInformation(acl, &ri, sizeof(ri), AclRevisionInformation), "raise the revision: %lu", GetLastError());
        memset(&ri, 0, sizeof(ri));
        GetAclInformation(acl, &ri, sizeof(ri), AclRevisionInformation);
        CHECK(ri.AclRevision == 4, "revision %lu", ri.AclRevision);
        ri.AclRevision = 2;
        SetLastError(0xdeadbeef);
        CHECK(!SetAclInformation(acl, &ri, sizeof(ri), AclRevisionInformation) && GetLastError() == ERROR_INVALID_PARAMETER,
              "lower revision: %lu", GetLastError());
        ri.AclRevision = 9;
        SetLastError(0xdeadbeef);
        CHECK(!SetAclInformation(acl, &ri, sizeof(ri), AclRevisionInformation), "revision 9 is not valid");
        SetLastError(0xdeadbeef);
        CHECK(!SetAclInformation(acl, &si, sizeof(si), AclSizeInformation) && GetLastError() == ERROR_INVALID_PARAMETER,
              "size information cannot be set: %lu", GetLastError());
        SetLastError(0xdeadbeef);
        CHECK(!SetAclInformation(acl, &ri, 2, AclRevisionInformation) && GetLastError() == ERROR_INVALID_PARAMETER,
              "short buffer: %lu", GetLastError());
        LocalFree(acl);
    }

    /* ---- SetPrivateObjectSecurity(Ex) ---- */
    {
        PSECURITY_DESCRIPTOR obj, mod, rel;
        PSID o, gr;
        PACL dacl, sacl;
        BOOL present, def;
        SECURITY_DESCRIPTOR_CONTROL ctl;
        DWORD rev;

        ConvertStringSecurityDescriptorToSecurityDescriptorA("O:BAG:SYD:(A;;GA;;;WD)", SDDL_REVISION_1, &obj, NULL);
        ConvertStringSecurityDescriptorToSecurityDescriptorA("O:LAG:BUD:(A;;GR;;;BA)(A;;GW;;;SY)S:(AU;SA;GA;;;WD)", SDDL_REVISION_1, &mod, NULL);

        CHECK(SetPrivateObjectSecurity(DACL_SECURITY_INFORMATION, mod, &obj, (GENERIC_MAPPING *)&map, NULL),
              "set the DACL: %lu", GetLastError());
        GetSecurityDescriptorControl(obj, &ctl, &rev);
        CHECK(ctl & SE_SELF_RELATIVE, "result is self-relative");
        GetSecurityDescriptorOwner(obj, &o, &def);
        CHECK(o && EqualSid(o, admins), "the owner is kept (%d)", o ? EqualSid(o, admins) : -1);
        GetSecurityDescriptorGroup(obj, &gr, &def);
        {
            PSID sy = mksid("S-1-5-18");
            CHECK(gr && EqualSid(gr, sy), "the group is kept");
        }
        GetSecurityDescriptorDacl(obj, &present, &dacl, &def);
        CHECK(present && dacl && dacl->AceCount == 2, "new DACL with two ACEs (%d)", dacl ? dacl->AceCount : -1);
        GetSecurityDescriptorSacl(obj, &present, &sacl, &def);
        CHECK(!present || !sacl, "the SACL is not touched");

        CHECK(SetPrivateObjectSecurity(OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION, mod, &obj, (GENERIC_MAPPING *)&map, NULL),
              "set owner and group: %lu", GetLastError());
        GetSecurityDescriptorOwner(obj, &o, &def);
        {
            PSID la = mksid("S-1-5-21-1-2-3-500");
            (void)la;
            CHECK(o != NULL, "owner present");
        }
        GetSecurityDescriptorDacl(obj, &present, &dacl, &def);
        CHECK(present && dacl && dacl->AceCount == 2, "the DACL is kept when the owner changes");

        CHECK(SetPrivateObjectSecurity(SACL_SECURITY_INFORMATION, mod, &obj, (GENERIC_MAPPING *)&map, NULL),
              "set the SACL: %lu", GetLastError());
        GetSecurityDescriptorSacl(obj, &present, &sacl, &def);
        CHECK(present && sacl && sacl->AceCount == 1, "SACL set");

        /* a modification without a DACL gives a NULL DACL */
        {
            SECURITY_DESCRIPTOR none;
            InitializeSecurityDescriptor(&none, SECURITY_DESCRIPTOR_REVISION);
            rel = LocalAlloc(LPTR, 64);
            len = 64;
            MakeSelfRelativeSD(&none, rel, &len);
            CHECK(SetPrivateObjectSecurity(DACL_SECURITY_INFORMATION, rel, &obj, (GENERIC_MAPPING *)&map, NULL), "NULL DACL");
            GetSecurityDescriptorDacl(obj, &present, &dacl, &def);
            CHECK(present && !dacl, "a NULL DACL is present");
            SetLastError(0xdeadbeef);
            CHECK(!SetPrivateObjectSecurity(OWNER_SECURITY_INFORMATION, rel, &obj, (GENERIC_MAPPING *)&map, NULL),
                  "owner info with no owner in the modification");
            CHECK(GetLastError() == ERROR_INVALID_OWNER, "error %lu", GetLastError());
            SetLastError(0xdeadbeef);
            CHECK(!SetPrivateObjectSecurity(GROUP_SECURITY_INFORMATION, rel, &obj, (GENERIC_MAPPING *)&map, NULL),
                  "group info with no group in the modification");
            CHECK(GetLastError() == ERROR_INVALID_PRIMARY_GROUP, "error %lu", GetLastError());
            LocalFree(rel);
        }
        /* the Ex flags */
        CHECK(pSetPrivateObjectSecurityEx(DACL_SECURITY_INFORMATION, mod, &obj, 1 /* SEF_DACL_AUTO_INHERIT */, (GENERIC_MAPPING *)&map, NULL),
              "Ex: %lu", GetLastError());
        GetSecurityDescriptorControl(obj, &ctl, &rev);
        CHECK(ctl & SE_DACL_AUTO_INHERITED, "SEF_DACL_AUTO_INHERIT sets SE_DACL_AUTO_INHERITED (%#x)", ctl);
        CHECK(pSetPrivateObjectSecurityEx(DACL_SECURITY_INFORMATION, mod, &obj, 0, (GENERIC_MAPPING *)&map, NULL), "Ex 0");
        GetSecurityDescriptorControl(obj, &ctl, &rev);
        CHECK(!(ctl & SE_DACL_AUTO_INHERITED), "and without the flag it is clear (%#x)", ctl);
        SetLastError(0xdeadbeef);
        CHECK(!SetPrivateObjectSecurity(DACL_SECURITY_INFORMATION, NULL, &obj, (GENERIC_MAPPING *)&map, NULL), "NULL modification");
        LocalFree(mod);
        LocalFree(obj);
    }

    /* ---- GetWindowsAccountDomainSid ---- */
    {
        BYTE buf[SECURITY_MAX_SID_SIZE];
        DWORD size = sizeof(buf);
        PSID acct = mksid("S-1-5-21-111-222-333-500"), short_acct = mksid("S-1-5-21-111-222"), dom = mksid("S-1-5-21-111-222-333");
        SetLastError(0xdeadbeef);
        CHECK(GetWindowsAccountDomainSid(acct, buf, &size) && EqualSid(buf, dom), "domain of an account sid: %lu", GetLastError());
        size = sizeof(buf);
        SetLastError(0xdeadbeef);
        CHECK(!GetWindowsAccountDomainSid(world, buf, &size) && GetLastError() == ERROR_NON_ACCOUNT_SID, "Everyone: %lu", GetLastError());
        size = sizeof(buf);
        SetLastError(0xdeadbeef);
        CHECK(!GetWindowsAccountDomainSid(admins, buf, &size) && GetLastError() == ERROR_NON_ACCOUNT_SID, "BUILTIN\\Administrators: %lu", GetLastError());
        size = sizeof(buf);
        SetLastError(0xdeadbeef);
        CHECK(!GetWindowsAccountDomainSid(short_acct, buf, &size) && GetLastError() == ERROR_INVALID_SID, "a domain-less 5-21 sid: %lu", GetLastError());
    }

    /* ---- the object ACEs ---- */
    {
        PACL a = LocalAlloc(LPTR, 512);
        ACE_HEADER *h;
        BYTE *p;
        NTSTATUS st;
        DWORD sidlen = GetLengthSid(world);

        InitializeAcl(a, 512, 4);
        st = pRtlAddAccessAllowedObjectAce(a, 4, 0x3, 0x1f, &R, &C, world);
        CHECK(st == 0, "allowed object ace %#lx", st);
        GetAce(a, 0, (void **)&h);
        CHECK(h->AceType == ACCESS_ALLOWED_OBJECT_ACE_TYPE && h->AceFlags == 0x3 && h->AceSize == 8 + 4 + 32 + sidlen,
              "type %d flags %#x size %d", h->AceType, h->AceFlags, h->AceSize);
        p = (BYTE *)h;
        CHECK(*(DWORD *)(p + 4) == 0x1f && *(DWORD *)(p + 8) == 3, "mask and flags");
        CHECK(!memcmp(p + 12, &R, 16) && !memcmp(p + 28, &C, 16) && EqualSid(p + 44, world), "guids and sid in place");

        st = pRtlAddAccessDeniedObjectAce(a, 4, 0, 0x2, &R, NULL, world);
        CHECK(st == 0, "denied object ace with one guid %#lx", st);
        GetAce(a, 1, (void **)&h);
        p = (BYTE *)h;
        CHECK(h->AceType == ACCESS_DENIED_OBJECT_ACE_TYPE && h->AceSize == 8 + 4 + 16 + sidlen && *(DWORD *)(p + 8) == 1 &&
              !memcmp(p + 12, &R, 16) && EqualSid(p + 28, world), "one guid layout (size %d)", h->AceSize);
        st = pRtlAddAccessDeniedObjectAce(a, 4, 0, 0x2, NULL, &C, world);
        GetAce(a, 2, (void **)&h);
        p = (BYTE *)h;
        CHECK(st == 0 && *(DWORD *)(p + 8) == 2 && !memcmp(p + 12, &C, 16) && EqualSid(p + 28, world), "inherited guid only");
        st = pRtlAddAuditAccessObjectAce(a, 4, 0, 0x2, &R, NULL, world, TRUE, FALSE);
        GetAce(a, 3, (void **)&h);
        CHECK(st == 0 && h->AceType == SYSTEM_AUDIT_OBJECT_ACE_TYPE && (h->AceFlags & SUCCESSFUL_ACCESS_ACE_FLAG) &&
              !(h->AceFlags & FAILED_ACCESS_ACE_FLAG), "audit object ace %#lx type %d flags %#x", st, h->AceType, h->AceFlags);
        st = pRtlAddAuditAccessObjectAce(a, 4, 0, 0x2, NULL, NULL, world, FALSE, TRUE);
        GetAce(a, 4, (void **)&h);
        CHECK(st == 0 && (h->AceFlags & FAILED_ACCESS_ACE_FLAG) && h->AceSize == 8 + 4 + sidlen, "audit ace with no guids");
        CHECK(a->AceCount == 5 && a->AclRevision == 4, "count %d revision %d", a->AceCount, a->AclRevision);
        CHECK(IsValidAcl(a), "valid");
        {
            PACL old = LocalAlloc(LPTR, 512);
            InitializeAcl(old, 512, 2);
            st = pRtlAddAccessAllowedObjectAce(old, 2, 0, 1, &R, NULL, world);
            CHECK(st == (NTSTATUS)0xC0000059, "revision 2 ACL: %#lx", st);
            st = pRtlAddAccessAllowedObjectAce(a, 3, 0, 1, &R, NULL, world);
            CHECK(st == (NTSTATUS)0xC0000059, "revision 3 ace: %#lx", st);
            LocalFree(old);
        }
        {
            PACL small = LocalAlloc(LPTR, 64);
            InitializeAcl(small, 32, 4);
            st = pRtlAddAccessAllowedObjectAce(small, 4, 0, 1, &R, &C, world);
            CHECK(st == (NTSTATUS)0xC0000099, "no room: %#lx", st);
            CHECK(small->AceCount == 0, "nothing added");
            LocalFree(small);
        }
        st = pRtlAddAccessAllowedObjectAce(a, 4, 0, 1, &R, NULL, (PSID)"\x05\x00");
        CHECK(st == (NTSTATUS)0xC0000078, "bad sid: %#lx", st);
        LocalFree(a);
    }

    /* ---- RtlDefaultNpAcl ---- */
    {
        PACL np = (PACL)0x1;
        NTSTATUS st = pRtlDefaultNpAcl(&np);
        CHECK(st == 0 && np && np != (PACL)0x1, "RtlDefaultNpAcl %#lx %p", st, np);
        if (st == 0 && np)
        {
            struct { PSID sid; DWORD mask; int seen; } want[5];
            PSID sys = mksid("S-1-5-18"), anon = mksid("S-1-5-7");
            int i, j;
            want[0].sid = sys; want[0].mask = GENERIC_ALL;
            want[1].sid = admins; want[1].mask = GENERIC_ALL;
            want[2].sid = user; want[2].mask = GENERIC_ALL;
            want[3].sid = world; want[3].mask = GENERIC_READ;
            want[4].sid = anon; want[4].mask = GENERIC_READ;
            for (i = 0; i < 5; i++) want[i].seen = 0;
            CHECK(IsValidAcl(np) && np->AceCount == 5, "valid, five ACEs (%d)", np->AceCount);
            for (i = 0; i < np->AceCount; i++)
            {
                ACCESS_ALLOWED_ACE *ace;
                GetAce(np, i, (void **)&ace);
                CHECK(ace->Header.AceType == ACCESS_ALLOWED_ACE_TYPE, "ace %d is an allow ACE", i);
                for (j = 0; j < 5; j++)
                    if (EqualSid(&ace->SidStart, want[j].sid) && ace->Mask == want[j].mask) want[j].seen++;
            }
            for (j = 0; j < 5; j++) CHECK(want[j].seen >= 1, "ACE %d (mask %#lx) present", j, want[j].mask);
            HeapFree(GetProcessHeap(), 0, np);
        }
    }

    /* ---- audit alarms ---- */
    {
        HANDLE tok;
        TOKEN_PRIVILEGES tp;
        LUID luid;
        BOOL have = FALSE, on_close = 0xdead, ok;
        PSECURITY_DESCRIPTOR asd;
        PRIVILEGE_SET ps = { 0 };

        /* take the privilege away first (it may be enabled by default) */
        LookupPrivilegeValueA(NULL, "SeAuditPrivilege", &luid);
        tp.PrivilegeCount = 1; tp.Privileges[0].Luid = luid; tp.Privileges[0].Attributes = SE_PRIVILEGE_REMOVED;
        tok = ptok;
        /* work on an impersonation copy so the process token stays as it is */
        SetThreadToken(NULL, imp_token);
        tp.Privileges[0].Attributes = 0;
        AdjustTokenPrivileges(imp_token, FALSE, &tp, 0, NULL, NULL);   /* disable */
        ConvertStringSecurityDescriptorToSecurityDescriptorA("O:BAG:SYD:(A;;GA;;;WD)S:(AU;SAFA;0x1;;;WD)", SDDL_REVISION_1, &asd, NULL);

        SetLastError(0xdeadbeef);
        ok = pObjectOpenAuditAlarmA("sub", (void *)1, (char *)"File", (char *)"x", asd, imp_token, 0x1, 0x1, &ps, FALSE, TRUE, &on_close);
        CHECK(!ok && GetLastError() == ERROR_PRIVILEGE_NOT_HELD, "open without SeAuditPrivilege: %d %lu", ok, GetLastError());
        SetLastError(0xdeadbeef);
        ok = pObjectCloseAuditAlarmA("sub", (void *)1, TRUE);
        CHECK(!ok && GetLastError() == ERROR_PRIVILEGE_NOT_HELD, "close without it: %d %lu", ok, GetLastError());
        SetLastError(0xdeadbeef);
        ok = pObjectPrivilegeAuditAlarmA("sub", (void *)1, imp_token, 1, &ps, TRUE);
        CHECK(!ok && GetLastError() == ERROR_PRIVILEGE_NOT_HELD, "privilege alarm without it: %d %lu", ok, GetLastError());
        SetLastError(0xdeadbeef);
        ok = pPrivilegedServiceAuditAlarmA("sub", "svc", imp_token, &ps, TRUE);
        CHECK(!ok && GetLastError() == ERROR_PRIVILEGE_NOT_HELD, "service alarm without it: %d %lu", ok, GetLastError());

        tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
        AdjustTokenPrivileges(imp_token, FALSE, &tp, 0, NULL, NULL);
        have = GetLastError() == ERROR_SUCCESS;
        if (!have)  /* make a token that has the privilege */
        {
            NTSTATUS (WINAPI *pNtCreateToken)(HANDLE *, ACCESS_MASK, OBJECT_ATTRIBUTES *, TOKEN_TYPE, LUID *, LARGE_INTEGER *,
                    TOKEN_USER *, TOKEN_GROUPS *, TOKEN_PRIVILEGES *, TOKEN_OWNER *, TOKEN_PRIMARY_GROUP *, TOKEN_DEFAULT_DACL *, TOKEN_SOURCE *)
                = (void *)GetProcAddress(ntdll, "NtCreateToken");
            OBJECT_ATTRIBUTES oa = { sizeof(oa) };
            TOKEN_PRIVILEGES ntp;
            TOKEN_GROUPS tg;
            TOKEN_OWNER to;
            TOKEN_PRIMARY_GROUP tpg;
            TOKEN_SOURCE src = { "fastb", { 0, 0 } };
            LARGE_INTEGER exp;
            LUID auth;
            HANDLE newtok = NULL, imp2 = NULL;
            NTSTATUS st;
            exp.QuadPart = -1;
            AllocateLocallyUniqueId(&auth);
            ntp.PrivilegeCount = 1; ntp.Privileges[0].Luid = luid; ntp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
            tg.GroupCount = 1; tg.Groups[0].Sid = world; tg.Groups[0].Attributes = SE_GROUP_ENABLED | SE_GROUP_MANDATORY;
            to.Owner = user; tpg.PrimaryGroup = world;
            st = pNtCreateToken ? pNtCreateToken(&newtok, TOKEN_ALL_ACCESS, &oa, TokenPrimary, &auth, &exp,
                                                 (TOKEN_USER *)ubuf, &tg, &ntp, &to, &tpg, NULL, &src) : -1;
            if (!st && DuplicateTokenEx(newtok, TOKEN_ALL_ACCESS, NULL, SecurityImpersonation, TokenImpersonation, &imp2))
            {
                imp_token = imp2;
                SetThreadToken(NULL, imp_token);
                have = TRUE;
            }
            else printf("note: no token with SeAuditPrivilege (NtCreateToken %#lx)\n", (unsigned long)st);
        }
        if (!have) have = FALSE;
        if (!have) printf("note: SeAuditPrivilege cannot be had here (%lu): the success side is not exercised\n", GetLastError());
        else
        {
            on_close = 0xdead;
            SetLastError(0xdeadbeef);
            ok = pObjectOpenAuditAlarmA("sub", (void *)1, (char *)"File", (char *)"x", asd, imp_token, 0x1, 0x1, &ps, FALSE, TRUE, &on_close);
            CHECK(ok && on_close == TRUE, "open with the privilege, SACL audits success: %d on_close %d err %lu", ok, on_close, GetLastError());
            on_close = 0xdead;
            ok = pObjectOpenAuditAlarmA("sub", (void *)1, (char *)"File", (char *)"x", asd, imp_token, 0x1, 0x0, &ps, FALSE, FALSE, &on_close);
            CHECK(ok && on_close == FALSE, "access refused: the SACL asks for failures only on success flag... on_close %d", on_close);
            ok = pObjectOpenAuditAlarmA("sub", (void *)1, (char *)"File", (char *)"x", NULL, imp_token, 0x1, 0x1, &ps, FALSE, TRUE, &on_close);
            CHECK(ok && on_close == FALSE, "no SACL: nothing to generate on close (%d)", on_close);
            {
                static const char *sacls[] = { "O:BAG:SYD:(A;;GA;;;WD)S:(AU;SAFA;0x1;;;LA)",   /* not a member */
                                               "O:BAG:SYD:(A;;GA;;;WD)S:(AU;SAFA;0x2;;;WD)",   /* other rights */
                                               "O:BAG:SYD:(A;;GA;;;WD)S:(AU;FA;0x1;;;WD)" };   /* failures only */
                int k;
                for (k = 0; k < 3; k++)
                {
                    PSECURITY_DESCRIPTOR sd2;
                    ConvertStringSecurityDescriptorToSecurityDescriptorA(sacls[k], SDDL_REVISION_1, &sd2, NULL);
                    on_close = 0xdead;
                    ok = pObjectOpenAuditAlarmA("sub", (void *)1, (char *)"File", (char *)"x", sd2, imp_token, 0x1, 0x1, &ps, FALSE, TRUE, &on_close);
                    CHECK(ok && on_close == FALSE, "SACL %d does not apply: on_close %d", k, on_close);
                    LocalFree(sd2);
                }
            }
            CHECK(pObjectCloseAuditAlarmA("sub", (void *)1, TRUE), "close with it");
            CHECK(pObjectPrivilegeAuditAlarmA("sub", (void *)1, imp_token, 1, &ps, TRUE), "privilege alarm with it");
            CHECK(pPrivilegedServiceAuditAlarmA("sub", "svc", imp_token, &ps, TRUE), "service alarm with it");
        }
        SetThreadToken(NULL, NULL);
        LocalFree(asd);
        (void)tok; (void)pObjectOpenAuditAlarmW;
    }

    printf("%s\n", f ? "RESULT: FAIL" : "RESULT: PASS");
    return f != 0;
}
