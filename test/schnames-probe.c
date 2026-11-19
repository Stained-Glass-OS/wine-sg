/* schnames-probe (wine-sg 8007): schannel's QueryCredentialsAttributesW(SECPKG_CRED_ATTR_NAMES) of
 * credentials without a certificate is SEC_E_NO_CREDENTIALS, as the A function (and Windows, per the
 * secur32 schannel tests); it was SEC_E_UNSUPPORTED_FUNCTION. */
#define SECURITY_WIN32
#include <windows.h>
#include <sspi.h>
#include <schannel.h>
#include <stdio.h>

int main(void)
{
    SCHANNEL_CRED cred = { SCHANNEL_CRED_VERSION };
    SecPkgCredentials_NamesW namesw;
    SecPkgCredentials_NamesA namesa;
    CredHandle h;
    TimeStamp exp;
    SECURITY_STATUS st, a, w;
    int failures = 0;

    cred.dwFlags = SCH_CRED_NO_DEFAULT_CREDS;
    st = AcquireCredentialsHandleW(NULL, (WCHAR *)UNISP_NAME_W, SECPKG_CRED_OUTBOUND, NULL, &cred, NULL, NULL, &h, &exp);
    printf("%s AcquireCredentialsHandleW (%#lx)\n", st ? "FAIL" : "ok  ", st);
    if (st) { printf("RESULT: FAIL\n"); return 1; }
    a = QueryCredentialsAttributesA(&h, SECPKG_CRED_ATTR_NAMES, &namesa);
    w = QueryCredentialsAttributesW(&h, SECPKG_CRED_ATTR_NAMES, &namesw);
    if (a != SEC_E_NO_CREDENTIALS) failures++;
    printf("%s A: SEC_E_NO_CREDENTIALS (%#lx)\n", a == SEC_E_NO_CREDENTIALS ? "ok  " : "FAIL", a);
    if (w != SEC_E_NO_CREDENTIALS) failures++;
    printf("%s W: SEC_E_NO_CREDENTIALS, as A (%#lx)\n", w == SEC_E_NO_CREDENTIALS ? "ok  " : "FAIL", w);
    FreeCredentialsHandle(&h);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
