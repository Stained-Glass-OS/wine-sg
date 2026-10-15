/* authz.dll (patches/sg/5513): resource manager, client contexts from a token
 * and AuthzAccessCheck against security descriptors. One "ok NAME" or
 * "FAIL NAME detail" line per check, for test/authz-gate.sh. */
#include <windows.h>
#include <sddl.h>
#include <authz.h>
#include <stdio.h>

static int failures;

#define CHECK(name, cond, ...) do { if (cond) printf("ok %s\n", name); else { printf("FAIL %s ", name); printf(__VA_ARGS__); printf("\n"); failures++; } } while (0)

static PSECURITY_DESCRIPTOR make_sd(const WCHAR *sddl)
{
    PSECURITY_DESCRIPTOR sd = NULL;

    if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(sddl, SDDL_REVISION_1, &sd, NULL))
        printf("FAIL sddl %ls error=%lu\n", sddl, GetLastError());
    return sd;
}

/* the result of a check: the function's, the granted mask and the error */
static BOOL check(AUTHZ_CLIENT_CONTEXT_HANDLE ctx, ACCESS_MASK desired, PSID self, PSECURITY_DESCRIPTOR sd,
                  PSECURITY_DESCRIPTOR *optional, DWORD count, ACCESS_MASK *granted, DWORD *error)
{
    AUTHZ_ACCESS_REQUEST request = {0};
    AUTHZ_ACCESS_REPLY reply = {0};
    DWORD sacl = 0;
    BOOL ret;

    request.DesiredAccess = desired;
    request.PrincipalSelfSid = self;
    reply.ResultListLength = 1;
    reply.GrantedAccessMask = granted;
    reply.Error = error;
    reply.SaclEvaluationResults = &sacl;
    *granted = 0xdeadbeef;
    *error = 0xdeadbeef;
    ret = AuthzAccessCheck(0, ctx, &request, NULL, sd, optional, count, &reply, NULL);
    return ret;
}

int main(void)
{
    AUTHZ_RESOURCE_MANAGER_HANDLE rm = NULL;
    AUTHZ_CLIENT_CONTEXT_HANDLE user = NULL, restricted_ctx = NULL;
    HANDLE token, restricted, imp;
    SID_IDENTIFIER_AUTHORITY authority = { SECURITY_NT_AUTHORITY };
    SID_AND_ATTRIBUTES disable;
    LARGE_INTEGER no_expiry = {{0}};
    LUID luid = {0};
    PSECURITY_DESCRIPTOR sd, sd2, optional[2];
    TOKEN_USER *tu;
    DWORD len, error;
    ACCESS_MASK granted;
    WCHAR *user_sid, sddl[512];
    BOOL ret;

    OpenProcessToken(GetCurrentProcess(), TOKEN_ALL_ACCESS, &token);
    GetTokenInformation(token, TokenUser, NULL, 0, &len);
    tu = HeapAlloc(GetProcessHeap(), 0, len);
    GetTokenInformation(token, TokenUser, tu, len, &len);
    ConvertSidToStringSidW(tu->User.Sid, &user_sid);

    /* resource manager */
    SetLastError(0xdeadbeef);
    ret = AuthzInitializeResourceManager(0, NULL, NULL, NULL, L"probe", NULL);
    CHECK("rm-null-handle", !ret && GetLastError() == ERROR_INVALID_PARAMETER, "ret=%d error=%lu", ret, GetLastError());
    SetLastError(0xdeadbeef);
    ret = AuthzInitializeResourceManager(0x80, NULL, NULL, NULL, L"probe", &rm);
    CHECK("rm-bad-flags", !ret && GetLastError() == ERROR_INVALID_PARAMETER, "ret=%d error=%lu", ret, GetLastError());
    ret = AuthzInitializeResourceManager(AUTHZ_RM_FLAG_NO_AUDIT, NULL, NULL, NULL, L"probe", &rm);
    CHECK("rm-create", ret && rm, "ret=%d error=%lu", ret, GetLastError());

    /* SeAuditPrivilege is needed without NO_AUDIT: not for a token that has lost its privileges */
    if (CreateRestrictedToken(token, DISABLE_MAX_PRIVILEGE, 0, NULL, 0, NULL, 0, NULL, &restricted) &&
        DuplicateTokenEx(restricted, TOKEN_ALL_ACCESS, NULL, SecurityImpersonation, TokenImpersonation, &imp) &&
        SetThreadToken(NULL, imp))
    {
        AUTHZ_RESOURCE_MANAGER_HANDLE rm2 = NULL;

        SetLastError(0xdeadbeef);
        ret = AuthzInitializeResourceManager(AUTHZ_RM_FLAG_INITIALIZE_UNDER_IMPERSONATION, NULL, NULL, NULL, NULL, &rm2);
        CHECK("rm-audit-privilege-needed", !ret && GetLastError() == ERROR_PRIVILEGE_NOT_HELD, "ret=%d error=%lu", ret, GetLastError());
        ret = AuthzInitializeResourceManager(AUTHZ_RM_FLAG_INITIALIZE_UNDER_IMPERSONATION | AUTHZ_RM_FLAG_NO_AUDIT,
                                             NULL, NULL, NULL, NULL, &rm2);
        CHECK("rm-no-audit-needs-none", ret && rm2, "ret=%d error=%lu", ret, GetLastError());
        if (rm2) AuthzFreeResourceManager(rm2);
        SetThreadToken(NULL, NULL);
    }
    else printf("FAIL restricted-token error=%lu\n", GetLastError());

    /* contexts */
    SetLastError(0xdeadbeef);
    ret = AuthzInitializeContextFromToken(0, token, NULL, NULL, luid, NULL, &user);
    CHECK("ctx-null-rm", !ret && GetLastError() == ERROR_INVALID_PARAMETER, "ret=%d error=%lu", ret, GetLastError());
    ret = AuthzInitializeContextFromToken(0, token, rm, &no_expiry, luid, NULL, &user);
    CHECK("ctx-from-token", ret && user, "ret=%d error=%lu", ret, GetLastError());
    SetLastError(0xdeadbeef);
    ret = AuthzInitializeContextFromToken(0, (HANDLE)0xdead0000, rm, NULL, luid, NULL, &restricted_ctx);
    CHECK("ctx-bad-token", !ret, "ret=%d", ret);

    /* the client is the owner of nothing: SY owns the object, the ACEs decide */
    swprintf(sddl, ARRAYSIZE(sddl), L"O:SYG:SYD:(A;;0x3;;;%ls)(D;;0x4;;;%ls)(A;;0x8;;;WD)", user_sid, user_sid);
    sd = make_sd(sddl);
    ret = check(user, 0x1, NULL, sd, NULL, 0, &granted, &error);
    CHECK("allow-direct", ret && granted == 0x1 && error == 0, "ret=%d granted=%#lx error=%lu", ret, granted, error);
    ret = check(user, 0x4, NULL, sd, NULL, 0, &granted, &error);
    CHECK("deny-direct", ret && granted == 0 && error == ERROR_ACCESS_DENIED, "ret=%d granted=%#lx error=%lu", ret, granted, error);
    ret = check(user, 0x8, NULL, sd, NULL, 0, &granted, &error);
    CHECK("allow-everyone", ret && granted == 0x8 && error == 0, "ret=%d granted=%#lx error=%lu", ret, granted, error);
    ret = check(user, 0x1 | 0x4, NULL, sd, NULL, 0, &granted, &error);
    CHECK("partly-denied-all-denied", ret && granted == 0 && error == ERROR_ACCESS_DENIED, "ret=%d granted=%#lx error=%lu", ret, granted, error);
    ret = check(user, 0x10, NULL, sd, NULL, 0, &granted, &error);
    CHECK("no-ace-no-access", ret && granted == 0 && error == ERROR_ACCESS_DENIED, "ret=%d granted=%#lx error=%lu", ret, granted, error);
    ret = check(user, MAXIMUM_ALLOWED, NULL, sd, NULL, 0, &granted, &error);
    CHECK("maximum-allowed", ret && granted == (0x3 | 0x8) && error == 0, "ret=%d granted=%#lx error=%lu", ret, granted, error);

    /* a different client: administrators disabled */
    {
        PSID admins;
        AllocateAndInitializeSid(&authority, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS,
                                 0, 0, 0, 0, 0, 0, &admins);
        disable.Sid = admins;
        disable.Attributes = 0;
        LocalFree(sd);
        sd = make_sd(L"O:SYG:SYD:(A;;0x10;;;BA)");
        ret = check(user, 0x10, NULL, sd, NULL, 0, &granted, &error);
        printf("info user-is-admin-in-sd=%d\n", ret && granted == 0x10);
        if (ret && granted == 0x10 && CreateRestrictedToken(token, 0, 1, &disable, 0, NULL, 0, NULL, &restricted))
        {
            ret = AuthzInitializeContextFromToken(0, restricted, rm, NULL, luid, NULL, &restricted_ctx);
            CHECK("ctx-restricted", ret && restricted_ctx, "ret=%d error=%lu", ret, GetLastError());
            ret = check(restricted_ctx, 0x10, NULL, sd, NULL, 0, &granted, &error);
            CHECK("context-decides-not-process", ret && granted == 0 && error == ERROR_ACCESS_DENIED,
                  "ret=%d granted=%#lx error=%lu", ret, granted, error);
            ret = check(user, 0x10, NULL, sd, NULL, 0, &granted, &error);
            CHECK("other-context-unchanged", ret && granted == 0x10 && error == 0, "ret=%d granted=%#lx error=%lu", ret, granted, error);
            AuthzFreeContext(restricted_ctx);
        }
        else printf("info no-admin-to-restrict\n");
        FreeSid(admins);
    }
    LocalFree(sd);

    /* optional descriptors: their ACEs follow the primary's */
    swprintf(sddl, ARRAYSIZE(sddl), L"O:SYG:SYD:(D;;0x1;;;%ls)", user_sid);
    sd2 = make_sd(sddl);
    optional[0] = sd2;
    sd = make_sd(L"O:SYG:SYD:(A;;0x1;;;WD)");
    ret = check(user, 0x1, NULL, sd, optional, 1, &granted, &error);
    CHECK("optional-after-primary-allow-first", ret && granted == 0x1 && error == 0, "ret=%d granted=%#lx error=%lu", ret, granted, error);
    LocalFree(sd);
    sd = make_sd(L"O:SYG:SYD:");
    ret = check(user, 0x1, NULL, sd, NULL, 0, &granted, &error);
    CHECK("empty-dacl-denies", ret && granted == 0 && error == ERROR_ACCESS_DENIED, "ret=%d granted=%#lx error=%lu", ret, granted, error);
    LocalFree(sd);
    swprintf(sddl, ARRAYSIZE(sddl), L"O:SYG:SYD:(A;;0x1;;;%ls)", user_sid);
    sd = make_sd(sddl);
    LocalFree(sd2);
    sd2 = make_sd(L"O:SYG:SYD:(A;;0x2;;;WD)");
    optional[0] = sd2;
    ret = check(user, 0x2, NULL, sd, optional, 1, &granted, &error);
    CHECK("optional-grants-add", ret && granted == 0x2 && error == 0, "ret=%d granted=%#lx error=%lu", ret, granted, error);
    ret = check(user, 0x2, NULL, sd, NULL, 0, &granted, &error);
    CHECK("without-optional-denied", ret && granted == 0 && error == ERROR_ACCESS_DENIED, "ret=%d granted=%#lx error=%lu", ret, granted, error);
    LocalFree(sd);
    LocalFree(sd2);

    /* NULL DACL: everything, whatever follows */
    {
        SECURITY_DESCRIPTOR null_dacl;
        PSECURITY_DESCRIPTOR deny;

        InitializeSecurityDescriptor(&null_dacl, SECURITY_DESCRIPTOR_REVISION);
        SetSecurityDescriptorOwner(&null_dacl, tu->User.Sid, FALSE);
        SetSecurityDescriptorDacl(&null_dacl, TRUE, NULL, FALSE);
        swprintf(sddl, ARRAYSIZE(sddl), L"O:SYG:SYD:(D;;0x1;;;%ls)", user_sid);
        deny = make_sd(sddl);
        optional[0] = deny;
        ret = check(user, 0x1, NULL, &null_dacl, optional, 1, &granted, &error);
        CHECK("null-dacl-allows", ret && granted == 0x1 && error == 0, "ret=%d granted=%#lx error=%lu", ret, granted, error);
        LocalFree(deny);
    }

    /* the principal's own SID */
    sd = make_sd(L"O:SYG:SYD:(A;;0x20;;;PS)");
    ret = check(user, 0x20, tu->User.Sid, sd, NULL, 0, &granted, &error);
    CHECK("principal-self", ret && granted == 0x20 && error == 0, "ret=%d granted=%#lx error=%lu", ret, granted, error);
    ret = check(user, 0x20, NULL, sd, NULL, 0, &granted, &error);
    CHECK("no-principal-self-sid", ret && granted == 0 && error == ERROR_ACCESS_DENIED, "ret=%d granted=%#lx error=%lu", ret, granted, error);
    LocalFree(sd);

    /* arguments */
    {
        AUTHZ_ACCESS_REQUEST request = {0x1};
        AUTHZ_ACCESS_REPLY reply = {1, &granted, NULL, &error};
        SECURITY_DESCRIPTOR no_owner;

        sd = make_sd(L"O:SYG:SYD:(A;;0x1;;;WD)");
        SetLastError(0xdeadbeef);
        ret = AuthzAccessCheck(0, NULL, &request, NULL, sd, NULL, 0, &reply, NULL);
        CHECK("check-null-context", !ret && GetLastError() == ERROR_INVALID_PARAMETER, "ret=%d error=%lu", ret, GetLastError());
        InitializeSecurityDescriptor(&no_owner, SECURITY_DESCRIPTOR_REVISION);
        SetSecurityDescriptorDacl(&no_owner, TRUE, NULL, FALSE);
        SetLastError(0xdeadbeef);
        ret = AuthzAccessCheck(0, user, &request, NULL, &no_owner, NULL, 0, &reply, NULL);
        CHECK("check-no-owner", !ret && GetLastError() == ERROR_INVALID_PARAMETER, "ret=%d error=%lu", ret, GetLastError());
        LocalFree(sd);
    }

    /* freeing */
    ret = AuthzFreeContext(user);
    CHECK("free-context", ret, "error=%lu", GetLastError());
    SetLastError(0xdeadbeef);
    ret = AuthzFreeContext(NULL);
    CHECK("free-null-context", !ret && GetLastError() == ERROR_INVALID_PARAMETER, "ret=%d error=%lu", ret, GetLastError());
    ret = AuthzFreeResourceManager(rm);
    CHECK("free-rm", ret, "error=%lu", GetLastError());
    SetLastError(0xdeadbeef);
    ret = AuthzFreeResourceManager(NULL);
    CHECK("free-null-rm", !ret && GetLastError() == ERROR_INVALID_PARAMETER, "ret=%d error=%lu", ret, GetLastError());

    LocalFree(user_sid);
    printf(failures ? "authz-probe: %d FAILED\n" : "authz-probe: all ok\n", failures);
    return failures != 0;
}
