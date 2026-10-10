/* Probe for patches/sg/2438: the SASL profile functions of secur32 (they were stubs). */
#include <windows.h>
#define SECURITY_WIN32
#include <security.h>
#include <sspi.h>
#include <stdio.h>
#include <string.h>

static int fails;
static void check(const char *name, int ok) { printf("      %s  %s\n", ok ? "PASS" : "FAIL", name); if (!ok) fails++; }
static void checkst(const char *name, LONG got, LONG want)
{
    char buf[128];
    snprintf(buf, sizeof(buf), "%s (%08lx, want %08lx)", name, (unsigned long)got, (unsigned long)want);
    check(buf, got == want);
}

#define SEC_E_BADARG ((LONG)SEC_E_INVALID_PARAMETER)

int main(void)
{
    char *la = NULL; WCHAR *lw = NULL; ULONG n = 0, i;
    SecPkgInfoA *ia = NULL; SecPkgInfoW *iw = NULL;
    SecBufferDesc desc; SecBuffer buf;
    LONG st, want;
    const char *p;
    int has_spnego = 0, has_gssapi = 0, has_digest = 0, termed = 0;

    st = SaslEnumerateProfilesA(&la, &n);
    checkst("SaslEnumerateProfilesA", st, 0);
    check("three profiles", n == 3);
    if (!st)
    {
        for (p = la, i = 0; *p; p += strlen(p) + 1, i++)
        {
            if (!strcmp(p, "GSS-SPNEGO")) has_spnego = 1;
            if (!strcmp(p, "GSSAPI")) has_gssapi = 1;
            if (!strcmp(p, "DIGEST-MD5")) has_digest = 1;
        }
        termed = (i == n);
        check("GSS-SPNEGO, GSSAPI, DIGEST-MD5 listed", has_spnego && has_gssapi && has_digest);
        check("list ends with an empty string", termed);
        FreeContextBuffer(la);
    }
    st = SaslEnumerateProfilesW(&lw, &n);
    checkst("SaslEnumerateProfilesW", st, 0);
    check("W: first profile GSS-SPNEGO, three profiles", !st && n == 3 && !wcscmp(lw, L"GSS-SPNEGO"));
    if (!st) FreeContextBuffer(lw);
    checkst("enumerate with NULL list", SaslEnumerateProfilesA(NULL, &n), SEC_E_BADARG);
    checkst("enumerate with NULL count", SaslEnumerateProfilesW(&lw, NULL), SEC_E_BADARG);

    /* profile -> package, and it is the very package QuerySecurityPackageInfo gives */
    want = QuerySecurityPackageInfoA("Negotiate", &ia);
    if (!want) FreeContextBuffer(ia);
    ia = NULL;
    st = SaslGetProfilePackageA("GSS-SPNEGO", &ia);
    checkst("GetProfilePackageA GSS-SPNEGO", st, want);
    check("package is Negotiate", st != 0 || (ia && !strcmp(ia->Name, "Negotiate")));
    if (!st) FreeContextBuffer(ia);
    st = SaslGetProfilePackageW(L"gssapi", &iw);
    want = QuerySecurityPackageInfoW((SEC_WCHAR *)L"Kerberos", &iw);
    if (!want) FreeContextBuffer(iw);
    iw = NULL;
    st = SaslGetProfilePackageW(L"gssapi", &iw);
    checkst("GetProfilePackageW is case-insensitive", st, want);
    if (!st) { check("package is Kerberos", !wcscmp(iw->Name, L"Kerberos")); FreeContextBuffer(iw); }
    checkst("unknown profile", SaslGetProfilePackageA("NOPE", &ia), SEC_E_SECPKG_NOT_FOUND);
    checkst("NULL profile", SaslGetProfilePackageA(NULL, &ia), SEC_E_BADARG);
    checkst("NULL result", SaslGetProfilePackageW(L"GSSAPI", NULL), SEC_E_BADARG);

    /* identify from a server's mechanism list */
    desc.ulVersion = SECBUFFER_VERSION; desc.cBuffers = 1; desc.pBuffers = &buf;
    buf.BufferType = SECBUFFER_TOKEN;
    buf.pvBuffer = "EXTERNAL GSSAPI GSS-SPNEGO DIGEST-MD5"; buf.cbBuffer = strlen(buf.pvBuffer);
    want = QuerySecurityPackageInfoA("Negotiate", &ia); if (!want) FreeContextBuffer(ia); ia = NULL;
    st = SaslIdentifyPackageA(&desc, &ia);
    checkst("identify picks the best (GSS-SPNEGO)", st, want);
    check("identified Negotiate", st != 0 || (ia && !strcmp(ia->Name, "Negotiate")));
    if (!st) FreeContextBuffer(ia);
    buf.pvBuffer = "EXTERNAL,gssapi"; buf.cbBuffer = strlen(buf.pvBuffer);
    want = QuerySecurityPackageInfoA("Kerberos", &ia); if (!want) FreeContextBuffer(ia); ia = NULL;
    st = SaslIdentifyPackageW(&desc, &iw);
    checkst("identify with commas, any case (GSSAPI)", st, want);
    if (!st) FreeContextBuffer(iw);
    buf.pvBuffer = "EXTERNAL PLAIN"; buf.cbBuffer = strlen(buf.pvBuffer);
    checkst("no mechanism we know", SaslIdentifyPackageA(&desc, &ia), SEC_E_SECPKG_NOT_FOUND);
    buf.pvBuffer = NULL; buf.cbBuffer = 0;
    checkst("empty token", SaslIdentifyPackageA(&desc, &ia), SEC_E_INVALID_TOKEN);
    checkst("no input", SaslIdentifyPackageA(NULL, &ia), SEC_E_BADARG);
    buf.BufferType = SECBUFFER_DATA; buf.pvBuffer = "GSSAPI"; buf.cbBuffer = 6;
    checkst("no token buffer", SaslIdentifyPackageA(&desc, &ia), SEC_E_BADARG);

    /* the exchange calls reach the SSPI ones: same answer for an NULL credential */
    {
        CredHandle out_ctx = { 0, 0 };
        SecBufferDesc od = { SECBUFFER_VERSION, 0, NULL };
        ULONG attrs = 0; TimeStamp ts;
        LONG a = InitializeSecurityContextW(NULL, NULL, (SEC_WCHAR *)L"host", 0, 0, 0, NULL, 0, &out_ctx, &od, &attrs, &ts);
        LONG b = SaslInitializeSecurityContextW(NULL, NULL, (SEC_WCHAR *)L"host", 0, 0, 0, NULL, 0, &out_ctx, &od, &attrs, &ts);
        LONG c = SaslInitializeSecurityContextA(NULL, NULL, (SEC_CHAR *)"host", 0, 0, 0, NULL, 0, &out_ctx, &od, &attrs, &ts);
        LONG d = AcceptSecurityContext(NULL, NULL, NULL, 0, 0, &out_ctx, &od, &attrs, &ts);
        LONG e = SaslAcceptSecurityContext(NULL, NULL, NULL, 0, 0, &out_ctx, &od, &attrs, &ts);
        printf("      (NULL credential: %08lx)\n", (unsigned long)a);
        check("NULL credential is refused", a != 0);
        checkst("SaslInitializeSecurityContextW matches", b, a);
        checkst("SaslInitializeSecurityContextA matches", c, a);
        checkst("SaslAcceptSecurityContext matches", e, d);
    }
    puts(fails ? "RESULT: FAIL" : "RESULT: PASS");
    return fails != 0;
}
