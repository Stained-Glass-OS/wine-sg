/* wine-sg 9712: LookupAccountNameLocalA/W (advapi32, sechost): LookupAccountName on the local computer. */
#include <windows.h>
#include <sddl.h>
#include <stdio.h>

static int failures;
#define CHECK(c, ...) do { if (c) printf("PASS  "); else { printf("FAIL  "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)
typedef BOOL (WINAPI *lw_t)(LPCWSTR, PSID, LPDWORD, LPWSTR, LPDWORD, PSID_NAME_USE);
typedef BOOL (WINAPI *la_t)(LPCSTR, PSID, LPDWORD, LPSTR, LPDWORD, PSID_NAME_USE);

int main(void)
{
    lw_t lw = (lw_t)GetProcAddress(LoadLibraryA("advapi32"), "LookupAccountNameLocalW");
    la_t la = (la_t)GetProcAddress(LoadLibraryA("advapi32"), "LookupAccountNameLocalA");
    lw_t sw = (lw_t)GetProcAddress(LoadLibraryA("sechost"), "LookupAccountNameLocalW");
    BYTE sid[SECURITY_MAX_SID_SIZE], sid2[SECURITY_MAX_SID_SIZE];
    WCHAR domain[64], user[64];
    char domainA[64];
    DWORD sidsize = sizeof(sid), domsize = 64, n = 64;
    SID_NAME_USE use;
    LPWSTR str = NULL;
    BOOL ret;

    CHECK(lw && la && sw, "advapi32 and sechost export LookupAccountNameLocalA/W");
    if (!lw || !la || !sw) { printf("RESULT: FAIL\n"); return 1; }
    ret = lw(L"Administrators", sid, &sidsize, domain, &domsize, &use);
    if (ret) ConvertSidToStringSidW(sid, &str);
    CHECK(ret && str && !lstrcmpW(str, L"S-1-5-32-544") && use == SidTypeAlias, "Administrators: %ls", str ? str : L"");
    GetUserNameW(user, &n);
    sidsize = sizeof(sid2); domsize = 64;
    ret = sw(user, sid2, &sidsize, domain, &domsize, &use);
    CHECK(ret && use == SidTypeUser, "the current user through sechost (%ls\\%ls)", domain, user);
    sidsize = sizeof(sid); domsize = 64;
    ret = la("Everyone", sid, &sidsize, domainA, &domsize, &use);
    CHECK(ret && use == SidTypeWellKnownGroup, "LookupAccountNameLocalA(Everyone)");
    sidsize = sizeof(sid); domsize = 64;
    ret = lw(L"sg-no-such-account-xyz", sid, &sidsize, domain, &domsize, &use);
    CHECK(!ret && GetLastError() == ERROR_NONE_MAPPED, "an unknown name: ERROR_NONE_MAPPED (%lu)", GetLastError());
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
