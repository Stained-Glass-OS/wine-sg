/* wine-sg 9701: CredGetTargetInfoA/W (advapi32, sechost). Imported statically (Anki's pywin32 win32cred:
 * the module did not load). Gives back the target information CredReadDomainCredentials cached with
 * CRED_CACHE_TARGET_INFORMATION, by its target or server name; else ERROR_NOT_FOUND. */
#include <windows.h>
#include <wincred.h>
#include <stdio.h>

/* the 32- and 64-bit probes share one logon session (and its cache): names of their own */
#ifdef _WIN64
#define B "64"
#define BW L"64"
#else
#define B "32"
#define BW L"32"
#endif

#ifndef CRED_CACHE_TARGET_INFORMATION
#define CRED_CACHE_TARGET_INFORMATION 1
#endif
static int failures;
#define CHECK(c, ...) do { if (c) printf("PASS  "); else { printf("FAIL  "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)
static int eq(const WCHAR *a, const WCHAR *b) { return (!a && !b) || (a && b && !wcscmp(a, b)); }

int main(void)
{
    CREDENTIAL_TARGET_INFORMATIONW in = {0}, *ti = (void *)1;
    CREDENTIAL_TARGET_INFORMATIONA *tia = (void *)1;
    DWORD types[2] = { CRED_TYPE_DOMAIN_PASSWORD, CRED_TYPE_GENERIC }, n;
    CREDENTIALW **creds;
    BOOL ret;

    SetLastError(0xdead);
    ret = CredGetTargetInfoW(L"srv1.sg-probe" BW L".test", 0, &ti);
    CHECK(!ret && GetLastError() == ERROR_NOT_FOUND && !ti, "nothing cached: ERROR_NOT_FOUND (ret %d, error %lu)", ret, GetLastError());
    ret = CredGetTargetInfoW(NULL, 0, &ti);
    CHECK(!ret && GetLastError() == ERROR_INVALID_PARAMETER, "NULL target: ERROR_INVALID_PARAMETER (error %lu)", GetLastError());
    ret = CredGetTargetInfoW(L"srv1.sg-probe" BW L".test", 0x100, &ti);
    CHECK(!ret && GetLastError() == ERROR_INVALID_FLAGS, "unknown flag: ERROR_INVALID_FLAGS (error %lu)", GetLastError());

    in.TargetName = (WCHAR *)L"srv1.sg-probe" BW L".test";
    in.DnsServerName = (WCHAR *)L"srv1.sg-probe" BW L".test";
    in.NetbiosServerName = (WCHAR *)L"SRV1-" BW;
    in.DnsDomainName = (WCHAR *)L"sg-probe" BW L".test";
    in.NetbiosDomainName = (WCHAR *)L"SGPROBE";
    in.PackageName = (WCHAR *)L"NTLM";
    in.Flags = CRED_TI_SERVER_FORMAT_UNKNOWN;
    in.CredTypeCount = 2; in.CredTypes = types;
    ret = CredReadDomainCredentialsW(&in, 0, &n, &creds);
    ret = CredGetTargetInfoW(L"srv1.sg-probe" BW L".test", 0, &ti);
    CHECK(!ret && GetLastError() == ERROR_NOT_FOUND, "a read without CRED_CACHE_TARGET_INFORMATION caches nothing");
    ret = CredReadDomainCredentialsW(&in, CRED_CACHE_TARGET_INFORMATION, &n, &creds);
    if (ret) CredFree(creds);
    ti = NULL;
    ret = CredGetTargetInfoW(L"srv1.sg-probe" BW L".test", 0, &ti);
    CHECK(ret && ti, "after a caching CredReadDomainCredentials: found by target name (ret %d, error %lu)", ret, GetLastError());
    if (ret && ti)
    {
        CHECK(eq(ti->TargetName, in.TargetName) && eq(ti->DnsServerName, in.DnsServerName) &&
              eq(ti->NetbiosServerName, in.NetbiosServerName) && eq(ti->DnsDomainName, in.DnsDomainName) &&
              eq(ti->NetbiosDomainName, in.NetbiosDomainName) && eq(ti->PackageName, in.PackageName) &&
              !ti->DnsTreeName, "the names come back");
        CHECK(ti->Flags == CRED_TI_SERVER_FORMAT_UNKNOWN && ti->CredTypeCount == 2 && ti->CredTypes &&
              ti->CredTypes[0] == CRED_TYPE_DOMAIN_PASSWORD && ti->CredTypes[1] == CRED_TYPE_GENERIC,
              "flags and credential types come back");
        CredFree(ti);
    }
    ti = NULL;
    ret = CredGetTargetInfoW(L"srv1-" BW, CRED_ALLOW_NAME_RESOLUTION, &ti);
    CHECK(ret && ti && eq(ti->TargetName, in.TargetName), "found by its NetBIOS server name, any case (ret %d)", ret);
    if (ret) CredFree(ti);
    ret = CredGetTargetInfoA("SRV1.SG-PROBE" B ".TEST", 0, &tia);
    CHECK(ret && tia && tia->TargetName && !strcmp(tia->TargetName, "srv1.sg-probe" B ".test") &&
          tia->PackageName && !strcmp(tia->PackageName, "NTLM") && !tia->DnsTreeName && tia->CredTypeCount == 2 &&
          tia->CredTypes[1] == CRED_TYPE_GENERIC, "CredGetTargetInfoA (ret %d)", ret);
    if (ret) CredFree(tia);
    ret = CredGetTargetInfoW(L"other.sg-probe" BW L".test", 0, &ti);
    CHECK(!ret && GetLastError() == ERROR_NOT_FOUND, "another target: ERROR_NOT_FOUND");
    {
        HMODULE sechost = LoadLibraryA("sechost.dll");
        CHECK(sechost && GetProcAddress(sechost, "CredGetTargetInfoW") && GetProcAddress(sechost, "CredGetTargetInfoA"),
              "sechost exports both");
    }
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
