/* dnsapi, fast mode (patch 3506): DnsQueryConfig's registration settings. */
#include <windows.h>
#include <windns.h>
#include <stdio.h>

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

int main(void)
{
    DWORD value = 77, len = sizeof(value);
    DNS_STATUS st;

    /* "host name registration is off" (primary and adapter): removed at
     * integration, superseded by 3284 (documented: the registration values
     * come from the TCP/IP parameters, Windows default on). */
    st = DnsQueryConfig(DnsConfigAddressRegistrationMaxCount, 0, NULL, NULL, &value, &len);
    check(st == ERROR_SUCCESS && value == 1, "one address registered at most");
    /* "a too small buffer asks for four bytes" (ERROR_MORE_DATA) and "no
     * buffer reports the size" (ERROR_SUCCESS): removed at integration,
     * superseded by 3276 (documented: ERROR_INSUFFICIENT_BUFFER, as every
     * other DnsQueryConfig type in Wine and Wine's dnsapi tests). */
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
