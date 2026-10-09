/* ldapsendtimeout-probe: LDAP_OPT_SEND_TIMEOUT (patches/sg/1527). Windows
 * takes the time allowed for sending a request (an l_timeval) and gives it
 * back; Office sets it on the connection to the global catalog it opens as
 * it starts, and gave up with LDAP_NOT_SUPPORTED. No server is contacted.
 * Prints "init=<1|0> set=<err> get=<err> sec=<n> usec=<n> setA=<err> null=<err>" */
#include <windows.h>
#include <stdio.h>
#include <winldap.h>

ULONG LDAPAPI ldap_set_optionA(LDAP *ld, int option, const void *invalue); /* not in every winldap.h */

#ifndef LDAP_OPT_SEND_TIMEOUT
#define LDAP_OPT_SEND_TIMEOUT 0x42
#endif

int main(void)
{
    LDAP *ld = ldap_initW(NULL, 3268);
    struct l_timeval tv = { 20, 500000 }, out = { -1, -1 }, tvA = { 7, 0 };
    ULONG set = 99, get = 99, setA = 99, nul = 99;

    if (ld)
    {
        set = ldap_set_optionW(ld, LDAP_OPT_SEND_TIMEOUT, &tv);
        get = ldap_get_optionW(ld, LDAP_OPT_SEND_TIMEOUT, &out);
        setA = ldap_set_optionA(ld, LDAP_OPT_SEND_TIMEOUT, &tvA);
        nul = ldap_set_optionW(ld, LDAP_OPT_SEND_TIMEOUT, NULL);
        ldap_unbind(ld);
    }
    printf("init=%d set=%lu get=%lu sec=%ld usec=%ld setA=%lu null=%lu\n", ld != NULL, set, get, out.tv_sec, out.tv_usec, setA, nul);
    return 0;
}
