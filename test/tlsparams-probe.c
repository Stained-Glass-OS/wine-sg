/* tlsparams-probe: schannel honours the protocols an SCH_CREDENTIALS' TLS parameters disable (Stained Glass
 * OS wine-sg 8295; test/tlsparams-gate.sh).  Qt's TLS backend asks this way (SCH_CREDENTIALS with
 * grbitDisabledProtocols); the parameters were ignored with a FIXME.
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#define SECURITY_WIN32
#define SCHANNEL_USE_BLACKLISTS
#include <windows.h>
#include <winternl.h>
#include <security.h>
#include <schannel.h>
#include <stdio.h>

static int fails;

static SECURITY_STATUS acquire( DWORD disabled, DWORD alpn, DWORD *protocols )
{
    SCH_CREDENTIALS cred = { SCH_CREDENTIALS_VERSION };
    TLS_PARAMETERS params = { 0 };
    UNICODE_STRING id = { 4, 4, (WCHAR *)L"h2" };
    SecPkgCred_SupportedProtocols sp = { 0 };
    CredHandle h;
    SECURITY_STATUS st;

    params.grbitDisabledProtocols = disabled;
    if (alpn) { params.cAlpnIds = 1; params.rgstrAlpnIds = &id; }
    cred.cTlsParameters = 1;
    cred.pTlsParameters = &params;
    cred.dwFlags = SCH_CRED_NO_DEFAULT_CREDS | SCH_CRED_MANUAL_CRED_VALIDATION;
    st = AcquireCredentialsHandleW( NULL, (WCHAR *)UNISP_NAME_W, SECPKG_CRED_OUTBOUND, NULL, &cred, NULL, NULL, &h, NULL );
    *protocols = 0;
    if (st == SEC_E_OK)
    {
        QueryCredentialsAttributesW( &h, SECPKG_ATTR_SUPPORTED_PROTOCOLS, &sp );
        *protocols = sp.grbitProtocol;
        FreeCredentialsHandle( &h );
    }
    return st;
}

static void check( int ok, const char *what, DWORD a, DWORD b )
{
    printf( "%s  %s (%#lx %#lx)\n", ok ? "PASS" : "FAIL", what, a, b );
    if (!ok) fails++;
}

int main( void )
{
    DWORD all, p;
    SECURITY_STATUS st;

    st = acquire( 0, 0, &all );
    check( st == SEC_E_OK && (all & SP_PROT_TLS1_2_CLIENT), "nothing disabled: TLS 1.2 among the defaults", st, all );
    st = acquire( SP_PROT_TLS1_2_CLIENT, 0, &p );
    check( st == SEC_E_OK && !(p & SP_PROT_TLS1_2_CLIENT) && p == (all & ~SP_PROT_TLS1_2_CLIENT),
           "TLS 1.2 disabled: not offered, the rest kept", st, p );
    st = acquire( SP_PROT_TLS1_2_CLIENT, 1, &p );
    check( st == SEC_E_OK && p == all, "parameters for an ALPN ID only: the defaults", st, p );
    st = acquire( SP_PROT_ALL, 0, &p );
    check( st == SEC_E_ALGORITHM_MISMATCH, "everything disabled: no protocol to use", st, p );
    printf( "RESULT: %s\n", fails ? "FAIL" : "PASS" );
    return fails != 0;
}
