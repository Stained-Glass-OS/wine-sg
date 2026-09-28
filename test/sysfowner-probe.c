/* sysfowner-probe [make]: a user makes C:\users\Public\payload; SYSTEM moves it
 * into the Package Cache and resets its permissions, as WiX Burn does */
#include <windows.h>
#include <aclapi.h>
#include <stdio.h>
/* what WiX Burn's ResetPathPermissions does to a cached payload (per-machine) */
static void try_reset(const char *label, const WCHAR *path, DWORD info)
{
    BYTE sidbuf[SECURITY_MAX_SID_SIZE]; DWORD sidlen = sizeof(sidbuf); ACL acl;
    CreateWellKnownSid(WinBuiltinAdministratorsSid, NULL, sidbuf, &sidlen);
    InitializeAcl(&acl, sizeof(ACL), ACL_REVISION);
    printf("%s %lu\n", label, SetNamedSecurityInfoW((WCHAR *)path, SE_FILE_OBJECT, info,
           (info & OWNER_SECURITY_INFORMATION) ? sidbuf : NULL, NULL, &acl, NULL));
}
int main(int argc, char **argv)
{
    if (argc > 1 && !strcmp(argv[1], "make"))
    {
        HANDLE f = CreateFileW(L"C:\\users\\Public\\payload", GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
        WriteFile(f, "x", 1, NULL, NULL); CloseHandle(f);
        printf("made %d\n", f != INVALID_HANDLE_VALUE);
        return 0;
    }
    const WCHAR *dir = L"C:\\ProgramData\\Package Cache\\.unverified", *file = L"C:\\ProgramData\\Package Cache\\.unverified\\payload";
    HANDLE h;
    CreateDirectoryW(L"C:\\ProgramData\\Package Cache", NULL);
    CreateDirectoryW(dir, NULL);
    (void)h;
    printf("move %d\n", MoveFileExW(L"C:\\users\\Public\\payload", file, MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED));
    try_reset("dacl-only", file, DACL_SECURITY_INFORMATION | UNPROTECTED_DACL_SECURITY_INFORMATION);
    try_reset("owner+dacl", file, OWNER_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION | UNPROTECTED_DACL_SECURITY_INFORMATION);
    try_reset("owner", file, OWNER_SECURITY_INFORMATION);
    return 0;
}
