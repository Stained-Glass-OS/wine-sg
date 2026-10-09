/* Protected credentials and the other credential calls (patches/sg/1702),
 * run by test/credprotect-gate.sh: CredProtect, CredUnprotect,
 * CredIsProtected, CredFindBestCredential, CredWriteDomainCredentials and
 * CredRename were not exported. A protected string is read back by a
 * second process (the same user), and a wildcard target is the best
 * credential for a host under it.
 *
 *   credprotect-probe.exe [unprotect PROTECTED] */
#include <windows.h>
#include <wincred.h>
#include <stdio.h>

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

typedef BOOL (WINAPI *protect_fn)(BOOL, WCHAR *, DWORD, WCHAR *, DWORD *, int *);
typedef BOOL (WINAPI *protect_a_fn)(BOOL, char *, DWORD, char *, DWORD *, int *);
typedef BOOL (WINAPI *unprotect_fn)(BOOL, WCHAR *, DWORD, WCHAR *, DWORD *);
typedef BOOL (WINAPI *unprotect_a_fn)(BOOL, char *, DWORD, char *, DWORD *);
typedef BOOL (WINAPI *isprot_fn)(WCHAR *, int *);
typedef BOOL (WINAPI *best_fn)(const WCHAR *, DWORD, DWORD, CREDENTIALW **);
typedef BOOL (WINAPI *writedom_fn)(CREDENTIAL_TARGET_INFORMATIONW *, CREDENTIALW *, DWORD);
typedef BOOL (WINAPI *rename_fn)(const WCHAR *, const WCHAR *, DWORD, DWORD);

static void write_generic(const WCHAR *target, const WCHAR *user, const WCHAR *secret)
{
    CREDENTIALW cred = { 0 };

    cred.Type = CRED_TYPE_GENERIC;
    cred.TargetName = (WCHAR *)target;
    cred.UserName = (WCHAR *)user;
    cred.CredentialBlob = (BYTE *)secret;
    cred.CredentialBlobSize = lstrlenW(secret) * sizeof(WCHAR);
    cred.Persist = CRED_PERSIST_LOCAL_MACHINE;
    CredWriteW(&cred, 0);
}

int main(int argc, char **argv)
{
    HMODULE advapi = GetModuleHandleA("advapi32.dll");
    protect_fn pProtect = (void *)GetProcAddress(advapi, "CredProtectW");
    protect_a_fn pProtectA = (void *)GetProcAddress(advapi, "CredProtectA");
    unprotect_fn pUnprotect = (void *)GetProcAddress(advapi, "CredUnprotectW");
    unprotect_a_fn pUnprotectA = (void *)GetProcAddress(advapi, "CredUnprotectA");
    isprot_fn pIsProtected = (void *)GetProcAddress(advapi, "CredIsProtectedW");
    best_fn pBest = (void *)GetProcAddress(advapi, "CredFindBestCredentialW");
    writedom_fn pWriteDomain = (void *)GetProcAddress(advapi, "CredWriteDomainCredentialsW");
    rename_fn pRename = (void *)GetProcAddress(advapi, "CredRenameW");
    WCHAR secret[] = L"Stained Glass secret", out[1024], back[256];
    char outA[1024], backA[256];
    DWORD len, len2;
    int type = -1;
    CREDENTIALW *cred = NULL;

    if (argc > 2 && !strcmp(argv[1], "unprotect"))
    {
        MultiByteToWideChar(CP_ACP, 0, argv[2], -1, out, 1024);
        len = 256;
        if (pUnprotect && pUnprotect(FALSE, out, lstrlenW(out) + 1, back, &len) && !lstrcmpW(back, secret)) return 0;
        return 1;
    }

    check(pProtect && pProtectA && pUnprotect && pUnprotectA && pIsProtected && pBest && pWriteDomain && pRename,
          "the calls are there");
    if (failures) goto done;

    len = 0;
    check(!pProtect(FALSE, secret, lstrlenW(secret) + 1, NULL, &len, NULL) && GetLastError() == ERROR_INSUFFICIENT_BUFFER && len > 4,
          "CredProtect: the size needed");
    len = ARRAY_SIZE(out);
    check(pProtect(FALSE, secret, lstrlenW(secret) + 1, out, &len, &type) && type == CredUserProtection
          && len == lstrlenW(out) + 1, "CredProtect");
    check(!wcsstr(out, L"Stained") && !memcmp(out, L"@@D", 6), "a protected string, not the secret");
    {
        WCHAR again[1024];
        DWORD len3 = ARRAY_SIZE(again);
        check(pProtect(FALSE, secret, lstrlenW(secret) + 1, again, &len3, NULL) && lstrcmpW(again, out),
              "encrypted: the same secret protects differently each time");
    }
    type = -1;
    check(pIsProtected(out, &type) && type == CredUserProtection, "CredIsProtected: protected");
    check(pIsProtected(secret, &type) && type == CredUnprotected, "and the secret is not");

    len2 = 5;
    check(!pUnprotect(FALSE, out, len, back, &len2) && GetLastError() == ERROR_INSUFFICIENT_BUFFER
          && len2 == lstrlenW(secret) + 1, "CredUnprotect: a small buffer");
    len2 = ARRAY_SIZE(back);
    check(pUnprotect(FALSE, out, len, back, &len2) && !lstrcmpW(back, secret), "CredUnprotect: the secret back");

    {
        char cmd[MAX_PATH + 1100];
        STARTUPINFOA si = { sizeof(si) };
        PROCESS_INFORMATION pi;
        DWORD code = 99;

        cmd[0] = '"';
        GetModuleFileNameA(NULL, cmd + 1, MAX_PATH);
        strcat(cmd, "\" unprotect ");
        WideCharToMultiByte(CP_ACP, 0, out, -1, cmd + strlen(cmd), 1024, NULL, NULL);
        if (CreateProcessA(NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
        {
            WaitForSingleObject(pi.hProcess, 20000);
            GetExitCodeProcess(pi.hProcess, &code);
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
        }
        check(code == 0, "another process of the user unprotects it");
    }

    len = sizeof(outA);
    check(pProtectA(FALSE, (char *)"ansi secret", 12, outA, &len, &type) && !strncmp(outA, "@@D", 3), "CredProtectA");
    len2 = sizeof(backA);
    check(pUnprotectA(FALSE, outA, len, backA, &len2) && !strcmp(backA, "ansi secret"), "CredUnprotectA");

    /* best credential */
    write_generic(L"*.sgprobe.example", L"wild", L"w");
    write_generic(L"exact.sgprobe.example", L"exact", L"e");
    check(pBest(L"exact.sgprobe.example", CRED_TYPE_GENERIC, 0, &cred) && !lstrcmpW(cred->UserName, L"exact"),
          "CredFindBestCredential: the exact target");
    if (cred) CredFree(cred);
    cred = NULL;
    check(pBest(L"host.sgprobe.example", CRED_TYPE_GENERIC, 0, &cred) && !lstrcmpW(cred->UserName, L"wild"),
          "a host under the wildcard target");
    if (cred) CredFree(cred);
    cred = NULL;
    check(!pBest(L"elsewhere.example", CRED_TYPE_GENERIC, 0, &cred) && GetLastError() == ERROR_NOT_FOUND,
          "nothing matching: ERROR_NOT_FOUND");
    CredDeleteW(L"*.sgprobe.example", CRED_TYPE_GENERIC, 0);
    CredDeleteW(L"exact.sgprobe.example", CRED_TYPE_GENERIC, 0);

    /* domain credentials */
    {
        CREDENTIAL_TARGET_INFORMATIONW info = { 0 };
        CREDENTIALW dom = { 0 };
        WCHAR pass[] = L"p";

        info.TargetName = (WCHAR *)L"fileserver";
        info.DnsServerName = (WCHAR *)L"fileserver.sgprobe.example";
        dom.Type = CRED_TYPE_DOMAIN_PASSWORD;
        dom.UserName = (WCHAR *)L"SGPROBE\\user";
        dom.CredentialBlob = (BYTE *)pass;
        dom.CredentialBlobSize = 2;
        dom.Persist = CRED_PERSIST_LOCAL_MACHINE;
        check(pWriteDomain(&info, &dom, 0), "CredWriteDomainCredentials");
        check(CredReadW(L"fileserver.sgprobe.example", CRED_TYPE_DOMAIN_PASSWORD, 0, &cred)
              && !lstrcmpW(cred->UserName, L"SGPROBE\\user"), "written for the server's DNS name");
        if (cred) CredFree(cred);
        CredDeleteW(L"fileserver.sgprobe.example", CRED_TYPE_DOMAIN_PASSWORD, 0);
        dom.Type = CRED_TYPE_GENERIC;
        check(!pWriteDomain(&info, &dom, 0) && GetLastError() == ERROR_INVALID_PARAMETER, "a generic one: refused");
    }

    check(!pRename(L"a", L"b", CRED_TYPE_GENERIC, 0) && GetLastError() == ERROR_NOT_SUPPORTED,
          "CredRename: ERROR_NOT_SUPPORTED, as since Vista");
done:
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
