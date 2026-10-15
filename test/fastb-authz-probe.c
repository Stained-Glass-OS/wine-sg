/* authz: resource managers, contexts, access checks, security event sources */
#include <windows.h>
#include <authz.h>
#include <sddl.h>
#include <stdio.h>
#include <string.h>

static int failures;
#define CHECK(c, ...) do { if (!(c)) { failures++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

#define R   0x00000001
#define W   0x00000002
#define X   0x00000004
#define RW  (R | W)
#define ALLBITS 0x001fffff

static BYTE sid_u[SECURITY_MAX_SID_SIZE], sid_g[SECURITY_MAX_SID_SIZE], sid_h[SECURITY_MAX_SID_SIZE],
            sid_o[SECURITY_MAX_SID_SIZE], sid_r[SECURITY_MAX_SID_SIZE], sid_everyone[SECURITY_MAX_SID_SIZE],
            sid_ownerrights[SECURITY_MAX_SID_SIZE], sid_self[SECURITY_MAX_SID_SIZE], sid_other[SECURITY_MAX_SID_SIZE];

static PSID mk(BYTE *buf, const char *str)
{
    PSID s = NULL;
    ConvertStringSidToSidA(str, &s);
    CopySid(SECURITY_MAX_SID_SIZE, buf, s);
    LocalFree(s);
    return buf;
}

struct ace
{
    BYTE type, flags;
    DWORD mask;
    PSID sid;
    int typed;         /* object ACE with an object type GUID */
    int object;        /* object ACE (without type unless typed) */
    const char *extra; /* application data after the SID */
    int extra_len;
};

static BYTE acl_buf[64][2048];
static int acl_next;

static ACL *mk_acl(const struct ace *aces, int n)
{
    ACL *acl = (ACL *)acl_buf[acl_next++];
    BYTE *ptr;
    int i;

    InitializeAcl(acl, 2048, ACL_REVISION_DS);
    ptr = (BYTE *)(acl + 1);
    for (i = 0; i < n; i++)
    {
        ACE_HEADER *h = (ACE_HEADER *)ptr;
        DWORD off = 8, sidlen = GetLengthSid(aces[i].sid);

        h->AceType = aces[i].type;
        h->AceFlags = aces[i].flags;
        *(DWORD *)(ptr + 4) = aces[i].mask;
        if (aces[i].object)
        {
            *(DWORD *)(ptr + off) = aces[i].typed ? ACE_OBJECT_TYPE_PRESENT : 0;
            off += 4;
            if (aces[i].typed)
            {
                memset(ptr + off, 0x42, sizeof(GUID));
                off += sizeof(GUID);
            }
        }
        CopySid(sidlen, ptr + off, aces[i].sid);
        off += sidlen;
        if (aces[i].extra_len)
        {
            memcpy(ptr + off, aces[i].extra, aces[i].extra_len);
            off += aces[i].extra_len;
        }
        off = (off + 3) & ~3;
        h->AceSize = off;
        ptr += off;
    }
    acl->AceCount = n;
    acl->AclSize = (WORD)(ptr - (BYTE *)acl);
    return acl;
}

static SECURITY_DESCRIPTOR sd_buf[64];
static int sd_next;

static PSECURITY_DESCRIPTOR mk_sd(PSID owner, ACL *dacl, int has_dacl)
{
    SECURITY_DESCRIPTOR *sd = &sd_buf[sd_next++];

    InitializeSecurityDescriptor(sd, SECURITY_DESCRIPTOR_REVISION);
    if (owner) SetSecurityDescriptorOwner(sd, owner, FALSE);
    if (has_dacl) SetSecurityDescriptorDacl(sd, TRUE, dacl, FALSE);
    return sd;
}

#define ALLOW(sid, mask) { ACCESS_ALLOWED_ACE_TYPE, 0, mask, sid }
#define DENY(sid, mask)  { ACCESS_DENIED_ACE_TYPE, 0, mask, sid }

static AUTHZ_RESOURCE_MANAGER_HANDLE rm, rm_cb;
static int cb_calls, cb_result, cb_fail;
static void *cb_args_seen;
static BYTE cb_ace_type;

static BOOL CALLBACK access_cb(AUTHZ_CLIENT_CONTEXT_HANDLE ctx, PACE_HEADER ace, void *args, BOOL *applicable)
{
    cb_calls++;
    cb_args_seen = args;
    cb_ace_type = ace->AceType;
    if (cb_fail)
    {
        SetLastError(ERROR_GEN_FAILURE);
        return FALSE;
    }
    *applicable = cb_result;
    return TRUE;
}

static int groups_called, groups_freed;
static BOOL CALLBACK groups_cb(AUTHZ_CLIENT_CONTEXT_HANDLE ctx, void *args, PSID_AND_ATTRIBUTES *sids, DWORD *count,
                               PSID_AND_ATTRIBUTES *rsids, DWORD *rcount)
{
    static SID_AND_ATTRIBUTES sa[1], rsa[1];

    groups_called++;
    sa[0].Sid = sid_h;
    sa[0].Attributes = SE_GROUP_ENABLED;
    rsa[0].Sid = sid_r;
    rsa[0].Attributes = SE_GROUP_ENABLED;
    *sids = sa;
    *count = 1;
    *rsids = rsa;
    *rcount = 1;
    return TRUE;
}
static VOID CALLBACK free_cb(PSID_AND_ATTRIBUTES sa) { groups_freed++; }

static AUTHZ_CLIENT_CONTEXT_HANDLE mk_ctx(PSID user, SID_AND_ATTRIBUTES *groups, int ngroups, SID_AND_ATTRIBUTES *rgroups, int nr)
{
    AUTHZ_CLIENT_CONTEXT_HANDLE c, c2;
    LUID luid = { 0 };

    if (!AuthzInitializeContextFromSid(AUTHZ_SKIP_TOKEN_GROUPS, user, rm_cb, NULL, luid, NULL, &c))
    {
        printf("FAIL mk_ctx: %lu\n", GetLastError());
        failures++;
        return NULL;
    }
    if (ngroups || nr)
    {
        if (!AuthzAddSidsToContext(c, groups, ngroups, rgroups, nr, &c2))
        {
            printf("FAIL AddSids: %lu\n", GetLastError());
            failures++;
            return c;
        }
        AuthzFreeContext(c);
        c = c2;
    }
    return c;
}

/* one check: the granted mask and error of a flat request */
static BOOL do_check(AUTHZ_CLIENT_CONTEXT_HANDLE ctx, PSECURITY_DESCRIPTOR sd, DWORD desired, PSID self, DWORD *granted, DWORD *err,
                     PSECURITY_DESCRIPTOR *opt, DWORD nopt, void *args)
{
    AUTHZ_ACCESS_REQUEST req = { desired, self, NULL, 0, args };
    DWORD g = 0xdead, e = 0xdead, sacl = 0xdead;
    AUTHZ_ACCESS_REPLY reply = { 1, &g, &sacl, &e };
    BOOL ret = AuthzAccessCheck(0, ctx, &req, NULL, sd, opt, nopt, &reply, NULL);

    *granted = g;
    *err = e;
    return ret;
}

#define EXPECT(ctx, sd, desired, want_mask, want_err) do { \
    DWORD g_, e_; BOOL r_ = do_check(ctx, sd, desired, NULL, &g_, &e_, NULL, 0, NULL); \
    CHECK(r_ && g_ == (want_mask) && e_ == (want_err), "check %#x: ret %d mask %#lx err %lu (want %#x err %d)", \
          (unsigned)(desired), r_, g_, e_, (unsigned)(want_mask), (int)(want_err)); } while (0)

static void test_rm_and_events(void)
{
    AUTHZ_RESOURCE_MANAGER_HANDLE h;
    HANDLE token;
    BYTE buf[2048];
    TOKEN_PRIVILEGES *tp = (TOKEN_PRIVILEGES *)buf;
    DWORD len, i;
    LUID audit;
    BOOL audit_on = FALSE, ret;
    HKEY key;
    WCHAR val[64];
    AUTHZ_SOURCE_SCHEMA_REGISTRATION reg;

    SetLastError(0xdeadbeef);
    ret = AuthzInitializeResourceManager(0x10, NULL, NULL, NULL, NULL, &h);
    CHECK(!ret && GetLastError() == ERROR_INVALID_PARAMETER, "bad flags %d %lu", ret, GetLastError());
    SetLastError(0xdeadbeef);
    ret = AuthzInitializeResourceManager(AUTHZ_RM_FLAG_NO_AUDIT, NULL, NULL, NULL, NULL, NULL);
    CHECK(!ret && GetLastError() == ERROR_INVALID_PARAMETER, "null handle %d %lu", ret, GetLastError());

    OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token);
    LookupPrivilegeValueA(NULL, "SeAuditPrivilege", &audit);
    GetTokenInformation(token, TokenPrivileges, buf, sizeof(buf), &len);
    for (i = 0; i < tp->PrivilegeCount; i++)
        if (tp->Privileges[i].Luid.LowPart == audit.LowPart && (tp->Privileges[i].Attributes & SE_PRIVILEGE_ENABLED))
            audit_on = TRUE;
    CloseHandle(token);
    SetLastError(0xdeadbeef);
    ret = AuthzInitializeResourceManager(0, NULL, NULL, NULL, L"probe", &h);
    if (audit_on) CHECK(ret, "audit manager with the privilege: %lu", GetLastError());
    else CHECK(!ret && GetLastError() == ERROR_PRIVILEGE_NOT_HELD, "audit manager without SeAuditPrivilege: %d %lu", ret, GetLastError());
    if (ret) AuthzFreeResourceManager(h);

    ret = AuthzInitializeResourceManager(AUTHZ_RM_FLAG_NO_AUDIT, NULL, NULL, NULL, NULL, &h);
    CHECK(ret, "no-audit manager %lu", GetLastError());
    CHECK(AuthzFreeResourceManager(h), "free manager");
    SetLastError(0xdeadbeef);
    CHECK(!AuthzFreeResourceManager(h) && GetLastError() == ERROR_INVALID_PARAMETER, "free manager twice");
    CHECK(!AuthzFreeResourceManager(NULL), "free NULL manager");

    /* security event sources */
    memset(&reg, 0, sizeof(reg));
    reg.szEventSourceName = L"SgAuthzProbeSource";
    reg.szEventMessageFile = L"C:\\messages.dll";
    reg.szEventAccessStringsFile = L"C:\\strings.dll";
    reg.szEventSourceXmlSchemaFile = L"C:\\schema.xml";
    SetLastError(0xdeadbeef);
    CHECK(!AuthzInstallSecurityEventSource(1, &reg) && GetLastError() == ERROR_INVALID_PARAMETER, "install flags");
    CHECK(!AuthzInstallSecurityEventSource(0, NULL), "install NULL");
    {
        AUTHZ_SOURCE_SCHEMA_REGISTRATION bad = reg;
        bad.szEventSourceName = NULL;
        CHECK(!AuthzInstallSecurityEventSource(0, &bad), "install without a name");
        bad = reg;
        bad.szEventMessageFile = NULL;
        CHECK(!AuthzInstallSecurityEventSource(0, &bad), "install without a message file");
    }
    CHECK(AuthzInstallSecurityEventSource(0, &reg), "install %lu", GetLastError());
    if (!RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"System\\CurrentControlSet\\Services\\EventLog\\Security\\SgAuthzProbeSource", 0, KEY_READ, &key))
    {
        len = sizeof(val);
        CHECK(!RegQueryValueExW(key, L"EventMessageFile", NULL, NULL, (BYTE *)val, &len) && !wcscmp(val, L"C:\\messages.dll"), "EventMessageFile");
        len = sizeof(val);
        CHECK(!RegQueryValueExW(key, L"ParameterMessageFile", NULL, NULL, (BYTE *)val, &len) && !wcscmp(val, L"C:\\strings.dll"), "ParameterMessageFile");
        len = sizeof(val);
        CHECK(!RegQueryValueExW(key, L"XmlSchemaFile", NULL, NULL, (BYTE *)val, &len) && !wcscmp(val, L"C:\\schema.xml"), "XmlSchemaFile");
        RegCloseKey(key);
    }
    else CHECK(0, "event source key missing");
    CHECK(!AuthzUninstallSecurityEventSource(1, L"SgAuthzProbeSource"), "uninstall flags");
    CHECK(AuthzUninstallSecurityEventSource(0, L"SgAuthzProbeSource"), "uninstall %lu", GetLastError());
    CHECK(RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"System\\CurrentControlSet\\Services\\EventLog\\Security\\SgAuthzProbeSource", 0, KEY_READ, &key) != 0,
          "event source key still there");
    reg.szEventSourceXmlSchemaFile = NULL;
    CHECK(AuthzInstallSecurityEventSource(0, &reg), "install without schema");
    if (!RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"System\\CurrentControlSet\\Services\\EventLog\\Security\\SgAuthzProbeSource", 0, KEY_READ, &key))
    {
        CHECK(RegQueryValueExW(key, L"XmlSchemaFile", NULL, NULL, NULL, NULL) != 0, "XmlSchemaFile written for NULL");
        RegCloseKey(key);
    }
    AuthzUninstallSecurityEventSource(0, L"SgAuthzProbeSource");
}

static void test_contexts(void)
{
    AUTHZ_CLIENT_CONTEXT_HANDLE c, c2, c3;
    LUID luid = { 0x1234, 0x56 }, zero = { 0 };
    LARGE_INTEGER expire;
    BYTE buf[2048];
    DWORD need, size;
    TOKEN_USER *user = (TOKEN_USER *)buf;
    TOKEN_GROUPS *groups = (TOKEN_GROUPS *)buf;
    TOKEN_PRIVILEGES *privs = (TOKEN_PRIVILEGES *)buf;
    SID_AND_ATTRIBUTES add[1];
    HANDLE token;
    TOKEN_USER *tuser;
    TOKEN_GROUPS *tgroups;
    BYTE tbuf[4096], gbuf[4096];
    DWORD tlen;
    BOOL ret;
    int before;

    expire.QuadPart = 0x123456789LL;
    SetLastError(0xdeadbeef);
    ret = AuthzInitializeContextFromSid(0x100, sid_u, rm, NULL, zero, NULL, &c);
    CHECK(!ret && GetLastError() == ERROR_INVALID_PARAMETER, "bad flags %d %lu", ret, GetLastError());
    ret = AuthzInitializeContextFromSid(AUTHZ_SKIP_TOKEN_GROUPS, NULL, rm, NULL, zero, NULL, &c);
    CHECK(!ret, "NULL sid");
    ret = AuthzInitializeContextFromSid(AUTHZ_SKIP_TOKEN_GROUPS, sid_u, rm, NULL, zero, NULL, NULL);
    CHECK(!ret, "NULL out");
    SetLastError(0xdeadbeef);
    ret = AuthzInitializeContextFromSid(AUTHZ_SKIP_TOKEN_GROUPS | AUTHZ_REQUIRE_S4U_LOGON, sid_u, rm, NULL, zero, NULL, &c);
    CHECK(!ret, "S4U logon cannot be done");
    SetLastError(0xdeadbeef);
    ret = AuthzInitializeContextFromSid(AUTHZ_SKIP_TOKEN_GROUPS, sid_u, (AUTHZ_RESOURCE_MANAGER_HANDLE)0x1234, NULL, zero, NULL, &c);
    CHECK(!ret, "bogus manager handle accepted");

    ret = AuthzInitializeContextFromSid(AUTHZ_SKIP_TOKEN_GROUPS, sid_u, rm, &expire, luid, NULL, &c);
    CHECK(ret, "from sid %lu", GetLastError());
    if (!ret) return;
    size = 0;
    SetLastError(0xdeadbeef);
    ret = AuthzGetInformationFromContext(c, AuthzContextInfoUserSid, 0, &need, NULL);
    CHECK(!ret && GetLastError() == ERROR_INSUFFICIENT_BUFFER && need > sizeof(TOKEN_USER), "user size query %d %lu need %lu", ret, GetLastError(), need);
    ret = AuthzGetInformationFromContext(c, AuthzContextInfoUserSid, sizeof(buf), &need, buf);
    CHECK(ret && EqualSid(user->User.Sid, sid_u), "user sid %d", ret);
    ret = AuthzGetInformationFromContext(c, AuthzContextInfoGroupsSids, sizeof(buf), &need, buf);
    CHECK(ret && groups->GroupCount == 0, "skip token groups: %lu groups", groups->GroupCount);
    ret = AuthzGetInformationFromContext(c, AuthzContextInfoRestrictedSids, sizeof(buf), &need, buf);
    CHECK(ret && groups->GroupCount == 0, "restricted");
    ret = AuthzGetInformationFromContext(c, AuthzContextInfoPrivileges, sizeof(buf), &need, buf);
    CHECK(ret && privs->PrivilegeCount == 0, "privileges of a SKIP context");
    ret = AuthzGetInformationFromContext(c, AuthzContextInfoExpirationTime, sizeof(buf), &need, buf);
    CHECK(ret && ((LARGE_INTEGER *)buf)->QuadPart == expire.QuadPart, "expiration");
    ret = AuthzGetInformationFromContext(c, AuthzContextInfoIdentifier, sizeof(buf), &need, buf);
    CHECK(ret && ((LUID *)buf)->LowPart == 0x1234 && ((LUID *)buf)->HighPart == 0x56, "identifier");
    SetLastError(0xdeadbeef);
    ret = AuthzGetInformationFromContext(c, AuthzContextInfoServerContext, sizeof(buf), &need, buf);
    CHECK(!ret && GetLastError() == ERROR_INVALID_PARAMETER, "reserved class %d %lu", ret, GetLastError());
    ret = AuthzGetInformationFromContext(c, AuthzContextInfoDeviceSids, sizeof(buf), &need, buf);
    CHECK(ret && groups->GroupCount == 0, "device sids");
    ret = AuthzGetInformationFromContext(c, AuthzContextInfoUserClaims, sizeof(buf), &need, buf);
    CHECK(ret && ((AUTHZ_SECURITY_ATTRIBUTES_INFORMATION *)buf)->AttributeCount == 0, "claims");

    /* AddSidsToContext: a copy, the original unchanged */
    add[0].Sid = sid_g;
    add[0].Attributes = SE_GROUP_ENABLED;
    SetLastError(0xdeadbeef);
    CHECK(!AuthzAddSidsToContext(c, NULL, 0, NULL, 0, &c2), "adding nothing");
    CHECK(AuthzAddSidsToContext(c, add, 1, NULL, 0, &c2), "add sids %lu", GetLastError());
    AuthzGetInformationFromContext(c2, AuthzContextInfoGroupsSids, sizeof(buf), &need, buf);
    CHECK(groups->GroupCount == 1 && EqualSid(groups->Groups[0].Sid, sid_g) && groups->Groups[0].Attributes == SE_GROUP_ENABLED, "added group");
    AuthzGetInformationFromContext(c, AuthzContextInfoGroupsSids, sizeof(buf), &need, buf);
    CHECK(groups->GroupCount == 0, "original gained a group");
    AuthzGetInformationFromContext(c2, AuthzContextInfoUserSid, sizeof(buf), &need, buf);
    CHECK(EqualSid(user->User.Sid, sid_u), "copy keeps the user");

    /* from a context: a copy; the group callback only runs with arguments */
    groups_called = groups_freed = 0;
    {
        AUTHZ_RESOURCE_MANAGER_HANDLE rmg;
        int arg;
        AuthzInitializeResourceManager(AUTHZ_RM_FLAG_NO_AUDIT, NULL, groups_cb, free_cb, NULL, &rmg);
        AuthzInitializeContextFromSid(AUTHZ_SKIP_TOKEN_GROUPS, sid_u, rmg, NULL, zero, NULL, &c3);
        CHECK(groups_called == 1 && groups_freed == 2, "dynamic groups at creation: called %d freed %d", groups_called, groups_freed);
        AuthzGetInformationFromContext(c3, AuthzContextInfoGroupsSids, sizeof(buf), &need, buf);
        CHECK(groups->GroupCount == 1 && EqualSid(groups->Groups[0].Sid, sid_h), "dynamic group added");
        AuthzGetInformationFromContext(c3, AuthzContextInfoRestrictedSids, sizeof(buf), &need, buf);
        CHECK(groups->GroupCount == 1 && EqualSid(groups->Groups[0].Sid, sid_r), "dynamic restricted sid added");
        AuthzFreeContext(c3);
        groups_called = 0;
        AuthzInitializeContextFromSid(AUTHZ_SKIP_TOKEN_GROUPS, sid_u, rmg, NULL, zero, &arg, &c3);
        CHECK(groups_called == 1, "callback with arguments");
        AuthzFreeContext(c3);
        AuthzInitializeContextFromSid(AUTHZ_SKIP_TOKEN_GROUPS, sid_u, rmg, NULL, zero, NULL, &c3);
        groups_called = 0;
        CHECK(AuthzInitializeContextFromAuthzContext(0, c3, NULL, zero, NULL, &c), "from context");
        CHECK(groups_called == 0, "callback ran without arguments (%d)", groups_called);
        AuthzFreeContext(c);
        CHECK(AuthzInitializeContextFromAuthzContext(0, c3, NULL, zero, &arg, &c), "from context with args");
        CHECK(groups_called == 1, "callback did not run with arguments (%d)", groups_called);
        AuthzGetInformationFromContext(c, AuthzContextInfoGroupsSids, sizeof(buf), &need, buf);
        CHECK(groups->GroupCount == 2, "copied and dynamic groups: %lu", groups->GroupCount);
        AuthzFreeContext(c);
        AuthzFreeContext(c3);
        AuthzFreeResourceManager(rmg);
    }

    /* account groups of the current user: the world and authenticated users */
    OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token);
    GetTokenInformation(token, TokenUser, tbuf, sizeof(tbuf), &tlen);
    tuser = (TOKEN_USER *)tbuf;
    ret = AuthzInitializeContextFromSid(0, tuser->User.Sid, rm, NULL, zero, NULL, &c3);
    CHECK(ret, "from the account's sid %lu", GetLastError());
    if (ret)
    {
        BOOL world = FALSE, auth = FALSE;
        DWORD i;
        AuthzGetInformationFromContext(c3, AuthzContextInfoGroupsSids, sizeof(buf), &need, buf);
        for (i = 0; i < groups->GroupCount; i++)
        {
            if (EqualSid(groups->Groups[i].Sid, sid_everyone)) world = TRUE;
            if (EqualSid(groups->Groups[i].Sid, mk(sid_other, "S-1-5-11"))) auth = TRUE;
        }
        CHECK(world && auth, "Everyone %d, Authenticated Users %d among %lu groups", world, auth, groups->GroupCount);
        AuthzFreeContext(c3);
    }
    ret = AuthzInitializeContextFromSid(AUTHZ_COMPUTE_PRIVILEGES | AUTHZ_SKIP_TOKEN_GROUPS, tuser->User.Sid, rm, NULL, zero, NULL, &c3);
    CHECK(ret, "compute privileges");
    if (ret)
    {
        AuthzGetInformationFromContext(c3, AuthzContextInfoPrivileges, sizeof(buf), &need, buf);
        CHECK(privs->PrivilegeCount >= 1, "no privileges computed");
        AuthzFreeContext(c3);
    }
    mk(sid_other, "S-1-5-21-111-222-333-9999");
    SetLastError(0xdeadbeef);
    ret = AuthzInitializeContextFromSid(0, sid_other, rm, NULL, zero, NULL, &c3);
    CHECK(!ret && GetLastError() == ERROR_NONE_MAPPED, "unmapped account %d %lu", ret, GetLastError());

    /* from the process token */
    SetLastError(0xdeadbeef);
    ret = AuthzInitializeContextFromToken(1, token, rm, NULL, zero, NULL, &c3);
    CHECK(!ret, "token flags are reserved");
    SetLastError(0xdeadbeef);
    ret = AuthzInitializeContextFromToken(0, (HANDLE)0xdeadbeef, rm, NULL, zero, NULL, &c3);
    CHECK(!ret, "bad token handle");
    ret = AuthzInitializeContextFromToken(0, token, rm, NULL, zero, NULL, &c3);
    CHECK(ret, "from token %lu", GetLastError());
    if (ret)
    {
        AuthzGetInformationFromContext(c3, AuthzContextInfoUserSid, sizeof(buf), &need, buf);
        CHECK(EqualSid(user->User.Sid, tuser->User.Sid), "token user");
        AuthzGetInformationFromContext(c3, AuthzContextInfoGroupsSids, sizeof(buf), &need, buf);
        GetTokenInformation(token, TokenGroups, gbuf, sizeof(gbuf), &tlen);
        tgroups = (TOKEN_GROUPS *)gbuf;
        CHECK(groups->GroupCount == tgroups->GroupCount && groups->GroupCount > 0, "token groups %lu vs %lu", groups->GroupCount, tgroups->GroupCount);
        if (groups->GroupCount == tgroups->GroupCount && groups->GroupCount)
            CHECK(EqualSid(groups->Groups[0].Sid, tgroups->Groups[0].Sid) && groups->Groups[0].Attributes == tgroups->Groups[0].Attributes, "first group");
        AuthzGetInformationFromContext(c3, AuthzContextInfoPrivileges, sizeof(buf), &need, buf);
        GetTokenInformation(token, TokenPrivileges, gbuf, sizeof(gbuf), &tlen);
        CHECK(privs->PrivilegeCount == ((TOKEN_PRIVILEGES *)gbuf)->PrivilegeCount, "token privileges");
        AuthzFreeContext(c3);
    }
    CloseHandle(token);

    /* handles */
    SetLastError(0xdeadbeef);
    CHECK(!AuthzFreeContext(NULL) && GetLastError() == ERROR_INVALID_PARAMETER, "free NULL context");
    before = AuthzFreeContext(c2);
    CHECK(before, "free context");
    CHECK(!AuthzFreeContext(c2), "free context twice");
    {
        BYTE b[64];
        SetLastError(0xdeadbeef);
        CHECK(!AuthzGetInformationFromContext(c2, AuthzContextInfoUserSid, sizeof(b), &need, b), "info from a freed context");
    }
    AuthzFreeContext(c);
}

static void test_access(void)
{
    SID_AND_ATTRIBUTES g_en[1], g_deny_only[1], g_disabled[1], restricted[1];
    AUTHZ_CLIENT_CONTEXT_HANDLE cu, cg, cdeny, cdis, cr;
    PSECURITY_DESCRIPTOR sd;
    ACL *acl;
    DWORD g, e;
    BOOL ret;

    g_en[0].Sid = sid_g;
    g_en[0].Attributes = SE_GROUP_ENABLED;
    g_deny_only[0].Sid = sid_g;
    g_deny_only[0].Attributes = SE_GROUP_USE_FOR_DENY_ONLY;
    g_disabled[0].Sid = sid_g;
    g_disabled[0].Attributes = 0;
    cu = mk_ctx(sid_u, NULL, 0, NULL, 0);
    cg = mk_ctx(sid_u, g_en, 1, NULL, 0);
    cdeny = mk_ctx(sid_u, g_deny_only, 1, NULL, 0);
    cdis = mk_ctx(sid_u, g_disabled, 1, NULL, 0);

    {   /* allow the user */
        struct ace a[] = { ALLOW(sid_u, R) };
        acl = mk_acl(a, 1);
        sd = mk_sd(sid_o, acl, 1);
        EXPECT(cu, sd, R, R, 0);
        EXPECT(cu, sd, W, 0, ERROR_ACCESS_DENIED);
        EXPECT(cu, sd, RW, 0, ERROR_ACCESS_DENIED);
        EXPECT(cu, sd, MAXIMUM_ALLOWED, R, 0);
        EXPECT(cu, sd, MAXIMUM_ALLOWED | R, R, 0);
        EXPECT(cu, sd, MAXIMUM_ALLOWED | W, 0, ERROR_ACCESS_DENIED);
        EXPECT(cu, sd, 0, 0, ERROR_ACCESS_DENIED);
        EXPECT(cdis, sd, R, R, 0);
    }
    {   /* a deny before an allow wins for its bits; after an allow it does not */
        struct ace a[] = { DENY(sid_g, W), ALLOW(sid_u, RW) };
        struct ace b[] = { ALLOW(sid_u, RW), DENY(sid_g, W) };
        sd = mk_sd(sid_o, mk_acl(a, 2), 1);
        EXPECT(cg, sd, R, R, 0);
        EXPECT(cg, sd, W, 0, ERROR_ACCESS_DENIED);
        EXPECT(cg, sd, MAXIMUM_ALLOWED, R, 0);
        EXPECT(cu, sd, W, W, 0);  /* without the group the deny does not match */
        sd = mk_sd(sid_o, mk_acl(b, 2), 1);
        EXPECT(cg, sd, RW, RW, 0);
        EXPECT(cg, sd, MAXIMUM_ALLOWED, RW, 0);
    }
    {
        struct ace a[] = { ALLOW(sid_g, RW) };
        struct ace b[] = { DENY(sid_g, W), ALLOW(sid_u, RW) };
        struct ace c[] = { ALLOW(sid_u, RW), DENY(sid_g, W) };
        sd = mk_sd(sid_o, mk_acl(a, 1), 1);
        EXPECT(cg, sd, RW, RW, 0);
        EXPECT(cdeny, sd, R, 0, ERROR_ACCESS_DENIED);    /* a deny-only group never allows */
        EXPECT(cdis, sd, R, 0, ERROR_ACCESS_DENIED);     /* nor a disabled one */
        sd = mk_sd(sid_o, mk_acl(b, 2), 1);
        EXPECT(cg, sd, W, 0, ERROR_ACCESS_DENIED);
        EXPECT(cg, sd, MAXIMUM_ALLOWED, R, 0);
        EXPECT(cdeny, sd, W, 0, ERROR_ACCESS_DENIED);    /* a deny-only group does deny */
        EXPECT(cdeny, sd, MAXIMUM_ALLOWED, R, 0);
        EXPECT(cdis, sd, W, W, 0);                       /* a disabled group does not deny */
        sd = mk_sd(sid_o, mk_acl(c, 2), 1);
        EXPECT(cg, sd, MAXIMUM_ALLOWED, RW, 0);          /* the allow came first */
        EXPECT(cdeny, sd, W, W, 0);
    }
    {   /* the owner */
        struct ace none[1];
        struct ace rights[] = { ALLOW(sid_ownerrights, R) };
        struct ace rights_deny[] = { DENY(sid_ownerrights, WRITE_DAC), ALLOW(sid_ownerrights, R) };
        AUTHZ_CLIENT_CONTEXT_HANDLE co = mk_ctx(sid_o, NULL, 0, NULL, 0);
        AUTHZ_CLIENT_CONTEXT_HANDLE cow = mk_ctx(sid_u, g_en, 1, NULL, 0);
        PSECURITY_DESCRIPTOR sd_group_owner;

        sd = mk_sd(sid_o, mk_acl(none, 0), 1);
        EXPECT(co, sd, MAXIMUM_ALLOWED, READ_CONTROL | WRITE_DAC, 0);
        EXPECT(co, sd, READ_CONTROL, READ_CONTROL, 0);
        EXPECT(co, sd, WRITE_DAC | R, 0, ERROR_ACCESS_DENIED);
        EXPECT(cu, sd, MAXIMUM_ALLOWED, 0, ERROR_ACCESS_DENIED);
        EXPECT(cu, sd, READ_CONTROL, 0, ERROR_ACCESS_DENIED);
        sd_group_owner = mk_sd(sid_g, mk_acl(none, 0), 1);
        EXPECT(cow, sd_group_owner, MAXIMUM_ALLOWED, READ_CONTROL | WRITE_DAC, 0);   /* owned by an enabled group */
        EXPECT(cdeny, sd_group_owner, READ_CONTROL, 0, ERROR_ACCESS_DENIED);         /* deny-only does not own */
        EXPECT(cdis, sd_group_owner, READ_CONTROL, 0, ERROR_ACCESS_DENIED);
        sd = mk_sd(sid_o, mk_acl(rights, 1), 1);
        EXPECT(co, sd, MAXIMUM_ALLOWED, R, 0);            /* OWNER RIGHTS replaces the implicit rights */
        EXPECT(co, sd, READ_CONTROL, 0, ERROR_ACCESS_DENIED);
        EXPECT(cu, sd, R, 0, ERROR_ACCESS_DENIED);        /* and only the owner matches it */
        sd = mk_sd(sid_o, mk_acl(rights_deny, 2), 1);
        EXPECT(co, sd, MAXIMUM_ALLOWED, R, 0);
        AuthzFreeContext(co);
        AuthzFreeContext(cow);
    }
    {   /* NULL DACL: everything */
        PSECURITY_DESCRIPTOR sdn = mk_sd(sid_o, NULL, 1);
        EXPECT(cu, sdn, ALLBITS & ~SYNCHRONIZE, ALLBITS & ~SYNCHRONIZE, 0);
        EXPECT(cu, sdn, MAXIMUM_ALLOWED, ALLBITS, 0);
    }
    {   /* principal self */
        struct ace a[] = { ALLOW(sid_self, X) };
        sd = mk_sd(sid_o, mk_acl(a, 1), 1);
        {
            DWORD gm, em;
            ret = do_check(cu, sd, X, NULL, &gm, &em, NULL, 0, NULL);
            CHECK(ret && gm == 0 && em == ERROR_ACCESS_DENIED, "SELF without a self sid: %#lx %lu", gm, em);
            ret = do_check(cu, sd, X, sid_u, &gm, &em, NULL, 0, NULL);
            CHECK(ret && gm == X && em == 0, "SELF with the user as self: %#lx %lu", gm, em);
            ret = do_check(cu, sd, X, sid_other, &gm, &em, NULL, 0, NULL);
            CHECK(ret && gm == 0 && em == ERROR_ACCESS_DENIED, "SELF with another sid: %#lx %lu", gm, em);
        }
    }
    {   /* inherit-only entries and typed object entries do not apply; an untyped object entry does */
        struct ace a[] = { { ACCESS_ALLOWED_ACE_TYPE, INHERIT_ONLY_ACE | OBJECT_INHERIT_ACE, R, sid_u } };
        struct ace b[] = { { ACCESS_ALLOWED_OBJECT_ACE_TYPE, 0, R, sid_u, 1, 1 } };
        struct ace c[] = { { ACCESS_ALLOWED_OBJECT_ACE_TYPE, 0, R, sid_u, 0, 1 } };
        struct ace d[] = { { ACCESS_DENIED_OBJECT_ACE_TYPE, 0, R, sid_u, 1, 1 }, ALLOW(sid_u, R) };
        sd = mk_sd(sid_o, mk_acl(a, 1), 1);
        EXPECT(cu, sd, R, 0, ERROR_ACCESS_DENIED);
        sd = mk_sd(sid_o, mk_acl(b, 1), 1);
        EXPECT(cu, sd, R, 0, ERROR_ACCESS_DENIED);
        sd = mk_sd(sid_o, mk_acl(c, 1), 1);
        EXPECT(cu, sd, R, R, 0);
        sd = mk_sd(sid_o, mk_acl(d, 2), 1);
        EXPECT(cu, sd, R, R, 0);   /* the typed deny covers one property, not the object */
    }
    {   /* callback entries */
        struct ace a[] = { { ACCESS_ALLOWED_CALLBACK_ACE_TYPE, 0, R, sid_u, 0, 0, "\1\2\3\4", 4 } };
        struct ace b[] = { { ACCESS_DENIED_CALLBACK_ACE_TYPE, 0, W, sid_u, 0, 0, "\1\2\3\4", 4 }, ALLOW(sid_u, RW) };
        struct ace c[] = { { ACCESS_ALLOWED_CALLBACK_ACE_TYPE, 0, R, sid_u, 0, 0, "artx\1\2\3\4", 8 } };
        struct ace d[] = { { ACCESS_DENIED_CALLBACK_ACE_TYPE, 0, W, sid_u, 0, 0, "artx\1\2\3\4", 8 }, ALLOW(sid_u, RW) };
        struct ace e_[] = { { ACCESS_ALLOWED_CALLBACK_OBJECT_ACE_TYPE, 0, R, sid_u, 0, 1, "\1\2", 2 } };
        int arg = 7;
        AUTHZ_CLIENT_CONTEXT_HANDLE cc = mk_ctx(sid_u, NULL, 0, NULL, 0);   /* made with rm_cb */
        AUTHZ_CLIENT_CONTEXT_HANDLE cn;
        AUTHZ_RESOURCE_MANAGER_HANDLE rm_plain;
        LUID zero = { 0 };

        AuthzInitializeResourceManager(AUTHZ_RM_FLAG_NO_AUDIT, NULL, NULL, NULL, NULL, &rm_plain);
        AuthzInitializeContextFromSid(AUTHZ_SKIP_TOKEN_GROUPS, sid_u, rm_plain, NULL, zero, NULL, &cn);

        cb_calls = 0; cb_result = 1; cb_fail = 0;
        sd = mk_sd(sid_o, mk_acl(a, 1), 1);
        ret = do_check(cc, sd, R, NULL, &g, &e, NULL, 0, &arg);
        CHECK(ret && g == R && e == 0 && cb_calls == 1 && cb_args_seen == &arg && cb_ace_type == ACCESS_ALLOWED_CALLBACK_ACE_TYPE,
              "allow callback true: %d %#lx %lu calls %d", ret, g, e, cb_calls);
        cb_result = 0;
        EXPECT(cc, sd, R, 0, ERROR_ACCESS_DENIED);
        cb_result = 1; cb_fail = 1;
        SetLastError(0xdeadbeef);
        ret = do_check(cc, sd, R, NULL, &g, &e, NULL, 0, NULL);
        CHECK(!ret && GetLastError() == ERROR_GEN_FAILURE, "callback failure fails the check: %d %lu", ret, GetLastError());
        cb_fail = 0;
        sd = mk_sd(sid_o, mk_acl(b, 2), 1);
        cb_result = 1;
        EXPECT(cc, sd, W, 0, ERROR_ACCESS_DENIED);
        cb_result = 0;
        EXPECT(cc, sd, W, W, 0);
        /* no callback function: an allow entry is unknown, left out; a deny entry stands */
        cb_calls = 0;
        sd = mk_sd(sid_o, mk_acl(a, 1), 1);
        EXPECT(cn, sd, R, 0, ERROR_ACCESS_DENIED);
        sd = mk_sd(sid_o, mk_acl(b, 2), 1);
        EXPECT(cn, sd, W, 0, ERROR_ACCESS_DENIED);
        EXPECT(cn, sd, R, R, 0);
        CHECK(cb_calls == 0, "callback called for a manager without one");
        /* conditional entries are not evaluated, whatever the callback says */
        cb_result = 1;
        sd = mk_sd(sid_o, mk_acl(c, 1), 1);
        EXPECT(cc, sd, R, 0, ERROR_ACCESS_DENIED);
        sd = mk_sd(sid_o, mk_acl(d, 2), 1);
        EXPECT(cc, sd, W, 0, ERROR_ACCESS_DENIED);
        cb_result = 1;
        sd = mk_sd(sid_o, mk_acl(e_, 1), 1);
        EXPECT(cc, sd, R, R, 0);
        AuthzFreeContext(cc);
        AuthzFreeContext(cn);
        AuthzFreeResourceManager(rm_plain);
    }
    {   /* optional security descriptors: the ACLs are concatenated, a NULL one is empty */
        struct ace a[] = { ALLOW(sid_u, R) };
        struct ace b[] = { ALLOW(sid_u, X) };
        struct ace none[1];
        PSECURITY_DESCRIPTOR prim = mk_sd(sid_o, mk_acl(a, 1), 1);
        PSECURITY_DESCRIPTOR opt[2];
        PSECURITY_DESCRIPTOR prim_empty = mk_sd(sid_o, mk_acl(none, 0), 1);

        opt[0] = mk_sd(NULL, mk_acl(b, 1), 1);
        opt[1] = mk_sd(NULL, NULL, 1);   /* NULL DACL: empty here */
        ret = do_check(cu, prim, R | X, NULL, &g, &e, opt, 2, NULL);
        CHECK(ret && g == (R | X) && e == 0, "concatenated: %d %#lx %lu", ret, g, e);
        ret = do_check(cu, prim_empty, X, NULL, &g, &e, opt, 1, NULL);
        CHECK(ret && g == X && e == 0, "optional only: %d %#lx %lu", ret, g, e);
        ret = do_check(cu, prim_empty, MAXIMUM_ALLOWED, NULL, &g, &e, &opt[1], 1, NULL);
        CHECK(ret && g == 0 && e == ERROR_ACCESS_DENIED, "NULL optional DACL is not full access: %d %#lx %lu", ret, g, e);
    }
    {   /* privileges */
        struct ace a[] = { ALLOW(sid_u, ALLBITS) };
        sd = mk_sd(sid_o, mk_acl(a, 1), 1);
        EXPECT(cu, sd, ACCESS_SYSTEM_SECURITY, 0, ERROR_PRIVILEGE_NOT_HELD);
        EXPECT(cu, sd, R | ACCESS_SYSTEM_SECURITY, 0, ERROR_PRIVILEGE_NOT_HELD);
        EXPECT(cu, sd, WRITE_OWNER, WRITE_OWNER, 0);   /* the entry grants it */
    }
    {   /* restricting sids: the second pass */
        struct ace a[] = { ALLOW(sid_u, 7), ALLOW(sid_r, 3) };
        struct ace b[] = { ALLOW(sid_u, 7) };
        restricted[0].Sid = sid_r;
        restricted[0].Attributes = SE_GROUP_ENABLED;
        cr = mk_ctx(sid_u, NULL, 0, restricted, 1);
        sd = mk_sd(sid_o, mk_acl(a, 2), 1);
        EXPECT(cr, sd, MAXIMUM_ALLOWED, 3, 0);
        EXPECT(cr, sd, X, 0, ERROR_ACCESS_DENIED);
        EXPECT(cr, sd, RW, RW, 0);
        sd = mk_sd(sid_o, mk_acl(b, 1), 1);
        EXPECT(cr, sd, R, 0, ERROR_ACCESS_DENIED);       /* the restricting sid has no entry */
        AuthzFreeContext(cr);
    }
    {   /* request and descriptor validation */
        struct ace a[] = { ALLOW(sid_u, R) };
        PSECURITY_DESCRIPTOR s1 = mk_sd(sid_o, mk_acl(a, 1), 1), no_owner = mk_sd(NULL, mk_acl(a, 1), 1),
                             no_dacl = mk_sd(sid_o, NULL, 0);
        DWORD gm[2], em[2], sacl[2];
        AUTHZ_ACCESS_REQUEST req = { R, NULL, NULL, 0, NULL };
        AUTHZ_ACCESS_REPLY reply = { 1, gm, sacl, em };
        AUTHZ_ACCESS_CHECK_RESULTS_HANDLE res;
        OBJECT_TYPE_LIST otl = { 0, 0, (GUID *)&GUID_NULL };

        SetLastError(0xdeadbeef);
        ret = AuthzAccessCheck(0, cu, &req, NULL, no_owner, NULL, 0, &reply, NULL);
        CHECK(!ret && GetLastError() == ERROR_INVALID_PARAMETER, "no owner %d %lu", ret, GetLastError());
        SetLastError(0xdeadbeef);
        ret = AuthzAccessCheck(0, cu, &req, NULL, no_dacl, NULL, 0, &reply, NULL);
        CHECK(!ret && GetLastError() == ERROR_INVALID_PARAMETER, "no dacl %d %lu", ret, GetLastError());
        reply.ResultListLength = 2;
        SetLastError(0xdeadbeef);
        ret = AuthzAccessCheck(0, cu, &req, NULL, s1, NULL, 0, &reply, NULL);
        CHECK(!ret && GetLastError() == ERROR_INVALID_PARAMETER, "result length 2 %d %lu", ret, GetLastError());
        reply.ResultListLength = 1;
        req.ObjectTypeList = &otl;
        req.ObjectTypeListLength = 1;
        SetLastError(0xdeadbeef);
        ret = AuthzAccessCheck(0, cu, &req, NULL, s1, NULL, 0, &reply, NULL);
        CHECK(!ret && GetLastError() == ERROR_NOT_SUPPORTED, "object type list %d %lu", ret, GetLastError());
        req.ObjectTypeList = NULL;
        req.ObjectTypeListLength = 0;
        SetLastError(0xdeadbeef);
        ret = AuthzAccessCheck(2, cu, &req, NULL, s1, NULL, 0, &reply, NULL);
        CHECK(!ret && GetLastError() == ERROR_INVALID_PARAMETER, "flags 2 %d %lu", ret, GetLastError());
        SetLastError(0xdeadbeef);
        ret = AuthzAccessCheck(AUTHZ_ACCESS_CHECK_NO_DEEP_COPY_SD, cu, &req, NULL, s1, NULL, 0, &reply, NULL);
        CHECK(!ret && GetLastError() == ERROR_INVALID_PARAMETER, "no-deep-copy without a handle %d %lu", ret, GetLastError());
        SetLastError(0xdeadbeef);
        ret = AuthzAccessCheck(0, cu, &req, (AUTHZ_AUDIT_EVENT_HANDLE)0x1234, s1, NULL, 0, &reply, NULL);
        CHECK(!ret, "bogus audit event accepted");
        SetLastError(0xdeadbeef);
        ret = AuthzAccessCheck(0, (AUTHZ_CLIENT_CONTEXT_HANDLE)0x1234, &req, NULL, s1, NULL, 0, &reply, NULL);
        CHECK(!ret && GetLastError() == ERROR_INVALID_PARAMETER, "bogus context %d %lu", ret, GetLastError());
        ret = AuthzAccessCheck(0, cu, &req, NULL, s1, NULL, 0, &reply, NULL);
        CHECK(ret && gm[0] == R && em[0] == 0, "plain check %d", ret);

        /* cached results: a deep copy survives changes to the original, the shallow one follows them */
        ret = AuthzAccessCheck(0, cu, &req, NULL, s1, NULL, 0, &reply, &res);
        CHECK(ret && res, "cached results");
        if (ret)
        {
            ACL *acl2 = NULL;
            BOOL p, d;
            ACCESS_ALLOWED_ACE *ace;

            GetSecurityDescriptorDacl(s1, &p, &acl2, &d);
            ace = (ACCESS_ALLOWED_ACE *)(acl2 + 1);
            ace->Mask = 0;   /* the original no longer grants anything */
            req.DesiredAccess = R;
            reply.GrantedAccessMask = gm;
            ret = AuthzCachedAccessCheck(0, res, &req, NULL, &reply);
            CHECK(ret && gm[0] == R && em[0] == 0, "cached check after the original changed: %d %#lx %lu", ret, gm[0], em[0]);
            req.DesiredAccess = W;
            ret = AuthzCachedAccessCheck(0, res, &req, NULL, &reply);
            CHECK(ret && gm[0] == 0 && em[0] == ERROR_ACCESS_DENIED, "cached check, bit not granted");
            SetLastError(0xdeadbeef);
            CHECK(!AuthzCachedAccessCheck(1, res, &req, NULL, &reply), "cached flags are reserved");
            ace->Mask = R;
            CHECK(AuthzFreeHandle(res), "free handle");
            CHECK(!AuthzFreeHandle(res), "free handle twice");
            CHECK(!AuthzCachedAccessCheck(0, res, &req, NULL, &reply), "cached check on a freed handle");
            CHECK(!AuthzFreeHandle(NULL), "free NULL handle");
        }
        req.DesiredAccess = R;
        ret = AuthzAccessCheck(AUTHZ_ACCESS_CHECK_NO_DEEP_COPY_SD, cu, &req, NULL, s1, NULL, 0, &reply, &res);
        CHECK(ret && res, "no-deep-copy results");
        if (ret)
        {
            ACL *acl2 = NULL;
            BOOL p, d;
            ACCESS_ALLOWED_ACE *ace;

            GetSecurityDescriptorDacl(s1, &p, &acl2, &d);
            ace = (ACCESS_ALLOWED_ACE *)(acl2 + 1);
            ace->Mask = 0;
            ret = AuthzCachedAccessCheck(0, res, &req, NULL, &reply);
            CHECK(ret && gm[0] == 0 && em[0] == ERROR_ACCESS_DENIED, "shallow cache follows the original");
            ace->Mask = R;
            AuthzFreeHandle(res);
        }
    }
    AuthzFreeContext(cu);
    AuthzFreeContext(cg);
    AuthzFreeContext(cdeny);
    AuthzFreeContext(cdis);
}

static void test_token_restricted(void)
{
    /* a context from a restricted token applies the restricting pass */
    HANDLE token, restricted;
    AUTHZ_CLIENT_CONTEXT_HANDLE c;
    BYTE tbuf[1024];
    DWORD len;
    TOKEN_USER *tu = (TOKEN_USER *)tbuf;
    SID_AND_ATTRIBUTES rs;
    LUID zero = { 0 };
    struct ace a[2];
    PSECURITY_DESCRIPTOR sd;
    DWORD g, e;

    OpenProcessToken(GetCurrentProcess(), TOKEN_ALL_ACCESS, &token);
    GetTokenInformation(token, TokenUser, tbuf, sizeof(tbuf), &len);
    rs.Sid = sid_everyone;
    rs.Attributes = 0;
    if (!CreateRestrictedToken(token, 0, 0, NULL, 0, NULL, 1, &rs, &restricted))
    {
        printf("note: CreateRestrictedToken failed %lu\n", GetLastError());
        CloseHandle(token);
        return;
    }
    CHECK(AuthzInitializeContextFromToken(0, restricted, rm, NULL, zero, NULL, &c), "context from a restricted token %lu", GetLastError());
    {
        BYTE buf[1024];
        DWORD need;
        TOKEN_GROUPS *groups = (TOKEN_GROUPS *)buf;
        AuthzGetInformationFromContext(c, AuthzContextInfoRestrictedSids, sizeof(buf), &need, buf);
        CHECK(groups->GroupCount == 1 && EqualSid(groups->Groups[0].Sid, sid_everyone), "restricted sids of the token: %lu", groups->GroupCount);
    }
    a[0] = (struct ace){ ACCESS_ALLOWED_ACE_TYPE, 0, 7, tu->User.Sid };
    a[1] = (struct ace){ ACCESS_ALLOWED_ACE_TYPE, 0, 1, sid_everyone };
    sd = mk_sd(sid_o, mk_acl(a, 2), 1);
    EXPECT(c, sd, MAXIMUM_ALLOWED, 1, 0);
    EXPECT(c, sd, X, 0, ERROR_ACCESS_DENIED);
    AuthzFreeContext(c);
    CloseHandle(restricted);
    CloseHandle(token);
}

int main(void)
{
    LUID zero = { 0 };

    mk(sid_u, "S-1-5-21-1000-2000-3000-1001");
    mk(sid_g, "S-1-5-21-1000-2000-3000-1002");
    mk(sid_h, "S-1-5-21-1000-2000-3000-1003");
    mk(sid_o, "S-1-5-21-1000-2000-3000-1004");
    mk(sid_r, "S-1-5-21-1000-2000-3000-1005");
    mk(sid_everyone, "S-1-1-0");
    mk(sid_ownerrights, "S-1-3-4");
    mk(sid_self, "S-1-5-10");
    mk(sid_other, "S-1-5-21-1000-2000-3000-1006");

    AuthzInitializeResourceManager(AUTHZ_RM_FLAG_NO_AUDIT, NULL, NULL, NULL, L"plain", &rm);
    AuthzInitializeResourceManager(AUTHZ_RM_FLAG_NO_AUDIT, access_cb, NULL, NULL, L"callbacks", &rm_cb);
    if (!rm || !rm_cb)
    {
        printf("FAIL: no resource manager (%lu)\nRESULT: FAIL\n", GetLastError());
        return 1;
    }
    (void)zero;
    test_rm_and_events();
    test_contexts();
    test_access();
    test_token_restricted();
    AuthzFreeResourceManager(rm);
    AuthzFreeResourceManager(rm_cb);
    printf(failures ? "RESULT: FAIL (%d)\n" : "RESULT: PASS\n", failures);
    return failures != 0;
}
