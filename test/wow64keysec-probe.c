/* wow64keysec-gate.sh's probe: a registry key's security by name in the
 * 32- and 64-bit views (SE_REGISTRY_WOW64_32KEY / _64KEY), as Opera's
 * installer asks for it */
#include <windows.h>
#include <aclapi.h>
#include <stdio.h>
static void one(const char *label, SE_OBJECT_TYPE type)
{
    PACL dacl = NULL; PSECURITY_DESCRIPTOR sd = NULL; ULONG n = 0; PEXPLICIT_ACCESS_W ea = NULL;
    DWORD err = GetNamedSecurityInfoW(L"CURRENT_USER\\Software\\Classes", type, DACL_SECURITY_INFORMATION, NULL, NULL, &dacl, NULL, &sd);
    printf("%s %lu %s", label, err, dacl ? "dacl" : "nodacl");
    if (!err && dacl) {
        DWORD e2 = GetExplicitEntriesFromAclW(dacl, &n, &ea);
        printf(" entries %lu %lu", e2, n);
        e2 = SetNamedSecurityInfoW((WCHAR *)L"CURRENT_USER\\Software\\Classes", type, DACL_SECURITY_INFORMATION, NULL, NULL, dacl, NULL);
        printf(" set %lu", e2);
    }
    printf("\n");
    if (ea) LocalFree(ea);
    if (sd) LocalFree(sd);
}
int main(void)
{
    one("KEY", SE_REGISTRY_KEY);
    one("WOW32", SE_REGISTRY_WOW64_32KEY);
    one("WOW64", (SE_OBJECT_TYPE)13);   /* SE_REGISTRY_WOW64_64KEY: not in mingw's accctrl.h */
    return 0;
}
