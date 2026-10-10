/* AccessCheck: ACCESS_SYSTEM_SECURITY needs SeSecurityPrivilege even on an
 * object without a DACL (patches/sg/2236). */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fails;
#define CHECK(c, ...) do { if (c) printf("PASS  %s\n", #c); else { printf("FAIL  %s: ", #c); printf(__VA_ARGS__); printf("\n"); fails++; } } while (0)

static GENERIC_MAPPING map = { STANDARD_RIGHTS_READ, STANDARD_RIGHTS_WRITE, STANDARD_RIGHTS_EXECUTE, STANDARD_RIGHTS_ALL | 0x1ff };

static BOOL check(PSECURITY_DESCRIPTOR sd, HANDLE token, DWORD desired, DWORD *granted, BOOL *status)
{
    PRIVILEGE_SET ps; DWORD len = sizeof(ps);
    *granted = 0xdeadbeef; *status = 0xdeadbeef;
    return AccessCheck(sd, token, desired, &map, &ps, &len, granted, status);
}

int main(void)
{
    HANDLE tok = NULL, imp = NULL;
    BYTE ubuf[256], gbuf[256], sdbuf[SECURITY_DESCRIPTOR_MIN_LENGTH];
    DWORD len, granted;
    BOOL status, ret;
    TOKEN_PRIVILEGES tp;
    LUID luid;
    SECURITY_DESCRIPTOR *sd = (void *)sdbuf;

    ret = OpenProcessToken(GetCurrentProcess(), TOKEN_ALL_ACCESS, &tok);
    CHECK(ret, "OpenProcessToken %lu", GetLastError());
    ret = DuplicateTokenEx(tok, TOKEN_ALL_ACCESS, NULL, SecurityImpersonation, TokenImpersonation, &imp);
    CHECK(ret, "DuplicateToken %lu", GetLastError());
    GetTokenInformation(tok, TokenUser, ubuf, sizeof(ubuf), &len);
    GetTokenInformation(tok, TokenPrimaryGroup, gbuf, sizeof(gbuf), &len);

    InitializeSecurityDescriptor(sd, SECURITY_DESCRIPTOR_REVISION);
    SetSecurityDescriptorOwner(sd, ((TOKEN_USER *)ubuf)->User.Sid, FALSE);
    SetSecurityDescriptorGroup(sd, ((TOKEN_PRIMARY_GROUP *)gbuf)->PrimaryGroup, FALSE);
    ret = SetSecurityDescriptorDacl(sd, TRUE, NULL, FALSE);   /* NULL DACL: everyone, everything */
    CHECK(ret, "set NULL dacl");

    /* ordinary access on an unprotected object */
    ret = check(sd, imp, READ_CONTROL, &granted, &status);
    CHECK(ret && status && granted == READ_CONTROL, "READ_CONTROL ret %d status %d granted %#lx", ret, status, granted);
    ret = check(sd, imp, MAXIMUM_ALLOWED, &granted, &status);
    CHECK(ret && status && granted == map.GenericAll, "MAXIMUM_ALLOWED ret %d status %d granted %#lx", ret, status, granted);

    /* the security privilege is required, and it must be enabled */
    ret = check(sd, imp, ACCESS_SYSTEM_SECURITY, &granted, &status);
    CHECK(ret && !status && granted == 0, "SACL without privilege: ret %d status %d granted %#lx err %lu", ret, status, granted, GetLastError());
    ret = check(sd, imp, READ_CONTROL | ACCESS_SYSTEM_SECURITY, &granted, &status);
    CHECK(ret && !status && granted == 0, "READ_CONTROL|SACL without privilege: ret %d status %d granted %#lx", ret, status, granted);

    if (LookupPrivilegeValueA(NULL, SE_SECURITY_NAME, &luid))
    {
        tp.PrivilegeCount = 1; tp.Privileges[0].Luid = luid; tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
        SetLastError(0);
        AdjustTokenPrivileges(imp, FALSE, &tp, 0, NULL, NULL);
        if (GetLastError() == ERROR_SUCCESS)
        {
            ret = check(sd, imp, ACCESS_SYSTEM_SECURITY, &granted, &status);
            CHECK(ret && status && granted == ACCESS_SYSTEM_SECURITY, "SACL with privilege: ret %d status %d granted %#lx", ret, status, granted);
        }
        else printf("SKIP  token cannot enable SeSecurityPrivilege (%lu)\n", GetLastError());
    }
    printf("RESULT: %s\n", fails ? "FAIL" : "PASS");
    return fails != 0;
}
