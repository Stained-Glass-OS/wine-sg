/* tapi32 batch (patches/sg/3007), run by test/fasttapi-gate.sh: tapiRequestMakeCall with no
 * application registered to take the request, and the destination address checks.
 *
 *   fasttapi-probe.exe */
#include <windows.h>
#include <tapi.h>
#include <stdio.h>
#include <string.h>

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}
#define CHECK(e) check((e), #e)

int main(void)
{
    char long_addr[200];

    memset(long_addr, '1', sizeof(long_addr) - 1);
    long_addr[sizeof(long_addr) - 1] = 0;

    CHECK((LONG)tapiRequestMakeCall("5551234", "app", "Someone", "hello") == -2);   /* TAPIERR_NOREQUESTRECIPIENT */
    CHECK((LONG)tapiRequestMakeCall("5551234", NULL, NULL, NULL) == -2);
    CHECK((LONG)tapiRequestMakeCall(NULL, "app", "Someone", "hello") == -4);        /* TAPIERR_INVALDESTADDRESS */
    CHECK((LONG)tapiRequestMakeCall("", "app", "Someone", "hello") == -4);
    CHECK((LONG)tapiRequestMakeCall(long_addr, "app", "Someone", "hello") == -4);

    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures ? 1 : 0;
}
