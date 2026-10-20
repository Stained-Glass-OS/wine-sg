/* ldap_init with no host name ("" or blanks) opens the default
 * host, as NULL does (patches/sg/5939). Microsoft Word calls
 * ldap_initW(L"", 3268) at startup; the URL built for it was a zero-byte,
 * unterminated heap block, so ldap_init failed or not by what the heap held
 * and Word died (0xc0000005, then its fail-fast 0xe0000002). */
#include <windows.h>
#include <winldap.h>
#include <stdio.h>

static int check(const char *what, LDAP *ld)
{
    WCHAR *host = NULL;
    ULONG ret;
    if (!ld) { printf("BAD %s: NULL (LdapGetLastError %lu)\n", what, LdapGetLastError()); return 1; }
    ret = ldap_get_optionW(ld, LDAP_OPT_HOST_NAME, &host);
    printf("%s %s: host %ls (ret %lu)\n", ret || !host || wcscmp(host, L"localhost:389") && wcscmp(host, L"localhost:3268") && wcscmp(host, L"localhost") ? "BAD" : "OK",
           what, host ? host : L"(none)", ret);
    if (ret || !host) return 1;
    if (wcsncmp(host, L"localhost", 9)) return 1;
    ldap_memfreeW(host);
    ldap_unbind(ld);
    return 0;
}

int main(void)
{
    int bad = 0;
    bad += check("ldap_initW(\"\", 3268)", ldap_initW((WCHAR *)L"", 3268));
    bad += check("ldap_initA(\"  \", 389)", ldap_initA((char *)"  ", 389));
    bad += check("ldap_initW(NULL, 389)", ldap_initW(NULL, 389));
    bad += check("ldap_initW(\"localhost\", 389)", ldap_initW((WCHAR *)L"localhost", 389));
    printf(bad ? "FAILED %d\n" : "ALL-OK\n", bad);
    return bad;
}
