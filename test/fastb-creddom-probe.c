/* CredReadDomainCredentialsW/A (patch 3743) */
#include <windows.h>
#include <wincred.h>
#include <stdio.h>
#include <string.h>

static int f;
#define CHECK(c, ...) do { if (!(c)) { f++; printf("FAIL line %d: %s: ", __LINE__, #c); printf(__VA_ARGS__); printf("\n"); } } while (0)

#ifndef CRED_TYPE_DOMAIN_CERTIFICATE
#define CRED_TYPE_DOMAIN_CERTIFICATE 3
#endif
typedef struct { LPWSTR TargetName, NetbiosServerName, DnsServerName, NetbiosDomainName, DnsDomainName, DnsTreeName, PackageName;
                 ULONG Flags; DWORD CredTypeCount; LPDWORD CredTypes; } TI_W;
typedef struct { LPSTR TargetName, NetbiosServerName, DnsServerName, NetbiosDomainName, DnsDomainName, DnsTreeName, PackageName;
                 ULONG Flags; DWORD CredTypeCount; LPDWORD CredTypes; } TI_A;

static BOOL (WINAPI *pReadW)(TI_W *, DWORD, DWORD *, PCREDENTIALW **);
static BOOL (WINAPI *pReadA)(TI_A *, DWORD, DWORD *, PCREDENTIALA **);

static void store(const WCHAR *target, const WCHAR *user, const char *blob)
{
    CREDENTIALW c = { 0 };
    c.Type = CRED_TYPE_DOMAIN_PASSWORD;
    c.TargetName = (WCHAR *)target;
    c.UserName = (WCHAR *)user;
    c.Comment = (WCHAR *)L"the comment";
    c.CredentialBlob = (BYTE *)blob;
    c.CredentialBlobSize = strlen(blob);
    c.Persist = CRED_PERSIST_SESSION;
    CHECK(CredWriteW(&c, 0), "write %ls: %lu", target, GetLastError());
}

int main(void)
{
    HMODULE adv = LoadLibraryA("advapi32.dll");
    DWORD types_dp[] = { CRED_TYPE_DOMAIN_PASSWORD }, types_cert[] = { CRED_TYPE_DOMAIN_CERTIFICATE };
    TI_W ti = { 0 };
    TI_A tia = { 0 };
    PCREDENTIALW *creds;
    PCREDENTIALA *credsA;
    DWORD size, err;
    BOOL ok;

    pReadW = (void *)GetProcAddress(adv, "CredReadDomainCredentialsW");
    pReadA = (void *)GetProcAddress(adv, "CredReadDomainCredentialsA");

    size = 77; creds = (void *)1;
    SetLastError(0xdeadbeef);
    ok = pReadW(NULL, 0, &size, &creds);
    CHECK(!ok && GetLastError() == ERROR_INVALID_PARAMETER && size == 0 && creds == NULL, "NULL info: %d %lu %lu %p", ok, GetLastError(), size, creds);

    ti.DnsServerName = (WCHAR *)L"server.fastb.example";
    SetLastError(0xdeadbeef);
    ok = pReadW(&ti, 0x10, &size, &creds);
    CHECK(!ok && GetLastError() == ERROR_INVALID_FLAGS, "bad flags: %d %lu", ok, GetLastError());

    memset(&ti, 0, sizeof(ti));
    SetLastError(0xdeadbeef);
    ok = pReadW(&ti, 0, &size, &creds);
    CHECK(!ok && GetLastError() == ERROR_INVALID_PARAMETER, "no names: %d %lu", ok, GetLastError());

    ti.DnsServerName = (WCHAR *)L"server.fastb.example";
    ti.CredTypeCount = 1; ti.CredTypes = types_dp;
    SetLastError(0xdeadbeef);
    ok = pReadW(&ti, 0, &size, &creds);
    CHECK(!ok && GetLastError() == ERROR_NOT_FOUND && size == 0 && creds == NULL, "nothing stored: %d %lu", ok, GetLastError());

    store(L"server.fastb.example", L"alice", "secret1");
    ok = pReadW(&ti, CRED_CACHE_TARGET_INFORMATION, &size, &creds);
    CHECK(ok && size == 1 && creds, "stored by the DNS server name: %d %lu size %lu", ok, GetLastError(), size);
    if (ok && size == 1)
    {
        CREDENTIALW *c = creds[0];
        char *lo = (char *)creds, *hi = lo + 4096;
        CHECK(c->Type == CRED_TYPE_DOMAIN_PASSWORD && !lstrcmpW(c->TargetName, L"server.fastb.example"), "target %ls", c->TargetName);
        CHECK(c->UserName && !lstrcmpW(c->UserName, L"alice"), "user %ls", c->UserName);
        CHECK(c->Comment && !lstrcmpW(c->Comment, L"the comment"), "comment");
        CHECK(c->CredentialBlobSize == 0 && c->CredentialBlob == NULL, "a domain password comes without its blob: %lu", c->CredentialBlobSize);
        /* one block: everything the entry points to lies right behind the array */
        CHECK((char *)c > lo && (char *)c < hi && (char *)c->TargetName > lo && (char *)c->TargetName < hi &&
              (char *)c->UserName < hi && (char *)c->Comment < hi, "not one block");
        CredFree(creds);
    }

    /* a different type than the stored one finds nothing */
    ti.CredTypes = types_cert;
    SetLastError(0xdeadbeef);
    ok = pReadW(&ti, 0, &size, &creds);
    CHECK(!ok && GetLastError() == ERROR_NOT_FOUND, "other type: %d %lu", ok, GetLastError());
    ti.CredTypes = types_dp;

    /* a wildcard target matches the servers of the domain */
    store(L"*.wild.fastb.example", L"bob", "secret2");
    memset(&ti, 0, sizeof(ti));
    ti.DnsServerName = (WCHAR *)L"host.wild.fastb.example";
    ti.CredTypeCount = 1; ti.CredTypes = types_dp;
    ok = pReadW(&ti, 0, &size, &creds);
    CHECK(ok && size == 1 && !lstrcmpW(creds[0]->UserName, L"bob"), "wildcard: %d %lu", ok, GetLastError());
    if (ok) CredFree(creds);

    /* the server name beats the domain name */
    store(L"dom.fastb.example", L"carol", "secret3");
    store(L"srv2.dom.fastb.example", L"dave", "secret4");
    memset(&ti, 0, sizeof(ti));
    ti.DnsServerName = (WCHAR *)L"srv2.dom.fastb.example";
    ti.DnsDomainName = (WCHAR *)L"dom.fastb.example";
    ti.CredTypeCount = 1; ti.CredTypes = types_dp;
    ok = pReadW(&ti, 0, &size, &creds);
    CHECK(ok && size == 1 && !lstrcmpW(creds[0]->UserName, L"dave"), "server before domain: %d %lu", ok, GetLastError());
    if (ok) CredFree(creds);
    /* with only the domain given, the domain's */
    ti.DnsServerName = NULL;
    ok = pReadW(&ti, 0, &size, &creds);
    CHECK(ok && size == 1 && !lstrcmpW(creds[0]->UserName, L"carol"), "domain only: %d %lu", ok, GetLastError());
    if (ok) CredFree(creds);
    /* the domain is the fall-back for a server that has none */
    ti.DnsServerName = (WCHAR *)L"srv3.nowhere.fastb.example";
    ok = pReadW(&ti, 0, &size, &creds);
    CHECK(ok && size == 1 && !lstrcmpW(creds[0]->UserName, L"carol"), "fall back to the domain: %d %lu", ok, GetLastError());
    if (ok) CredFree(creds);

    /* the ANSI form */
    memset(&tia, 0, sizeof(tia));
    tia.DnsServerName = "srv2.dom.fastb.example";
    tia.CredTypeCount = 1; tia.CredTypes = types_dp;
    ok = pReadA(&tia, 0, &size, &credsA);
    CHECK(ok && size == 1 && !strcmp(credsA[0]->UserName, "dave"), "ANSI: %d %lu", ok, GetLastError());
    if (ok) CredFree(credsA);

    /* no types listed: the domain types */
    memset(&ti, 0, sizeof(ti));
    ti.DnsServerName = (WCHAR *)L"srv2.dom.fastb.example";
    ok = pReadW(&ti, 0, &size, &creds);
    CHECK(ok && size >= 1 && !lstrcmpW(creds[0]->UserName, L"dave"), "no types: %d %lu", ok, GetLastError());
    if (ok) CredFree(creds);

    CredDeleteW(L"server.fastb.example", CRED_TYPE_DOMAIN_PASSWORD, 0);
    CredDeleteW(L"*.wild.fastb.example", CRED_TYPE_DOMAIN_PASSWORD, 0);
    CredDeleteW(L"dom.fastb.example", CRED_TYPE_DOMAIN_PASSWORD, 0);
    CredDeleteW(L"srv2.dom.fastb.example", CRED_TYPE_DOMAIN_PASSWORD, 0);
    printf("%s\n", f ? "RESULT: FAIL" : "RESULT: PASS");
    return f != 0;
}
