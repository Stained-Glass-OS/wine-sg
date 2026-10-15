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

    st = DnsQueryConfig(DnsConfigPrimaryHostNameRegistrationEnabled, 0, NULL, NULL, &value, &len);
    check(st == ERROR_SUCCESS && len == sizeof(DWORD) && value == 0, "primary host name registration is off");
    value = 77; len = sizeof(value);
    st = DnsQueryConfig(DnsConfigAdapterHostNameRegistrationEnabled, 0, NULL, NULL, &value, &len);
    check(st == ERROR_SUCCESS && value == 0, "adapter host name registration is off");
    value = 77; len = sizeof(value);
    st = DnsQueryConfig(DnsConfigAddressRegistrationMaxCount, 0, NULL, NULL, &value, &len);
    check(st == ERROR_SUCCESS && value == 1, "one address registered at most");
    len = 0;
    st = DnsQueryConfig(DnsConfigAddressRegistrationMaxCount, 0, NULL, NULL, &value, &len);
    check(st == ERROR_MORE_DATA && len == sizeof(DWORD), "a too small buffer asks for four bytes");
    len = 0;
    st = DnsQueryConfig(DnsConfigAddressRegistrationMaxCount, 0, NULL, NULL, NULL, &len);
    check(st == ERROR_SUCCESS && len == sizeof(DWORD), "no buffer reports the size");
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
