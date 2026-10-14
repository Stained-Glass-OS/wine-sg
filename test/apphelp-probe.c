/* apphelp.dll (patches/sg/5502): ShimFlushCache needs an administrator,
 * SdbGetAppPatchDir gives the AppPatch directory or the empty string.
 * Prints one "name=value" line per fact for test/apphelp-gate.sh. */
#include <windows.h>
#include <stdio.h>

static BOOL (WINAPI *pShimFlushCache)(HWND, HINSTANCE, LPCSTR, int);
static void (WINAPI *pSdbGetAppPatchDir)(void *, WCHAR *, DWORD);

static BOOL is_admin(void)
{
    SID_IDENTIFIER_AUTHORITY authority = { SECURITY_NT_AUTHORITY };
    BOOL member = FALSE;
    PSID sid;

    if (!AllocateAndInitializeSid(&authority, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS,
                                  0, 0, 0, 0, 0, 0, &sid)) return FALSE;
    if (!CheckTokenMembership(NULL, sid, &member)) member = FALSE;
    FreeSid(sid);
    return member;
}

int main(void)
{
    HMODULE apphelp = LoadLibraryA("apphelp.dll");
    HANDLE token, restricted, imp;
    SID_IDENTIFIER_AUTHORITY authority = { SECURITY_NT_AUTHORITY };
    SID_AND_ATTRIBUTES disable;
    WCHAR windir[MAX_PATH], path[MAX_PATH], expect[MAX_PATH];
    BOOL ret;

    if (!apphelp) { printf("load=FAIL\n"); return 1; }
    pShimFlushCache = (void *)GetProcAddress(apphelp, "ShimFlushCache");
    pSdbGetAppPatchDir = (void *)GetProcAddress(apphelp, "SdbGetAppPatchDir");

    printf("is-admin=%d\n", is_admin());
    SetLastError(0);
    ret = pShimFlushCache(NULL, NULL, NULL, 0);
    printf("flush-admin=%d error=%lu\n", ret, ret ? 0 : GetLastError());

    /* the same call as a user who is not an administrator */
    if (!AllocateAndInitializeSid(&authority, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS,
                                  0, 0, 0, 0, 0, 0, &disable.Sid)) return 1;
    disable.Attributes = 0;
    OpenProcessToken(GetCurrentProcess(), TOKEN_ALL_ACCESS, &token);
    if (CreateRestrictedToken(token, 0, 1, &disable, 0, NULL, 0, NULL, &restricted) &&
        DuplicateTokenEx(restricted, TOKEN_ALL_ACCESS, NULL, SecurityImpersonation, TokenImpersonation, &imp) &&
        SetThreadToken(NULL, imp))
    {
        printf("restricted-is-admin=%d\n", is_admin());
        SetLastError(0);
        ret = pShimFlushCache(NULL, NULL, NULL, 0);
        printf("flush-user=%d error=%lu\n", ret, ret ? 0 : GetLastError());
        SetThreadToken(NULL, NULL);
    }
    else printf("restricted=FAIL error=%lu\n", GetLastError());

    GetSystemWindowsDirectoryW(windir, MAX_PATH);
    swprintf(expect, MAX_PATH, L"%ls\\AppPatch", windir);
    wcscpy(path, L"junk");
    pSdbGetAppPatchDir(NULL, path, MAX_PATH);
    printf("patchdir-ok=%d\n", !wcsicmp(path, expect));
    wcscpy(path, L"junk");
    pSdbGetAppPatchDir(NULL, path, 5);
    printf("patchdir-small-empty=%d\n", path[0] == 0);
    wcscpy(path, L"junk");
    pSdbGetAppPatchDir(NULL, path, 0);
    printf("patchdir-zero-untouched=%d\n", !wcscmp(path, L"junk"));
    return 0;
}
