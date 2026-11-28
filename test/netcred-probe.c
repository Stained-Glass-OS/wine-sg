/* The names and passwords Credential Manager keeps for servers go to the logon session
 * (sg-netmountd's CRED / UNCRED), as Windows' LSA holds them for the redirector (wine-sg
 * 9631, i0976).  The gate's stand-in for sg-netmountd records each request line in a file;
 * the probe writes, rewrites, deletes and enumerates credentials and prints what it did:
 * the gate compares the lines. */
#include <windows.h>
#include <wincred.h>
#include <stdio.h>

BOOL WINAPI CredProfileLoaded(void);

static void put(const WCHAR *target, DWORD type, const WCHAR *user, const WCHAR *password, DWORD flags)
{
    CREDENTIALW c;
    memset(&c, 0, sizeof(c));
    c.Type = type;
    c.TargetName = (WCHAR *)target;
    c.UserName = (WCHAR *)user;
    c.Persist = CRED_PERSIST_LOCAL_MACHINE;
    if (password)
    {
        c.CredentialBlob = (BYTE *)password;
        c.CredentialBlobSize = lstrlenW(password) * sizeof(WCHAR);
    }
    printf("write %ls: %d\n", target, CredWriteW(&c, flags));
    fflush(stdout);
}

int main(int argc, char **argv)
{
    const char *step = argc > 1 ? argv[1] : "";

    if (!strcmp(step, "write"))
    {
        put(L"FILESRV", CRED_TYPE_DOMAIN_PASSWORD, L"FILESRV\\bob", L"Pw 1,2=3!", 0);
        put(L"nas.example.lan", CRED_TYPE_DOMAIN_PASSWORD, L"alice", L"s3cr\x00e9t", 0);
        put(L"git:https://example.com", CRED_TYPE_GENERIC, L"me", L"token", 0);
        put(L"TERMSRV/rdp1", CRED_TYPE_DOMAIN_PASSWORD, L"rdp\\me", L"rdppw", 0);
        put(L"*.corp.lan", CRED_TYPE_DOMAIN_PASSWORD, L"corp\\me", L"corppw", 0);
    }
    else if (!strcmp(step, "preserve"))
        put(L"FILESRV", CRED_TYPE_DOMAIN_PASSWORD, L"FILESRV\\robert", NULL, CRED_PRESERVE_CREDENTIAL_BLOB);
    else if (!strcmp(step, "profile"))
        printf("profile: %d\n", CredProfileLoaded());
    else if (!strcmp(step, "delete"))
    {
        printf("delete FILESRV: %d\n", CredDeleteW(L"FILESRV", CRED_TYPE_DOMAIN_PASSWORD, 0));
        printf("delete git: %d\n", CredDeleteW(L"git:https://example.com", CRED_TYPE_GENERIC, 0));
    }
    else if (!strcmp(step, "nodaemon"))
    {
        DWORD start = GetTickCount();
        put(L"OTHER", CRED_TYPE_DOMAIN_PASSWORD, L"OTHER\\x", L"y", 0);
        printf("nodaemon took %lu ms\n", GetTickCount() - start);
    }
    fflush(stdout);
    return 0;
}
