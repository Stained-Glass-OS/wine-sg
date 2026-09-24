/* trust-probe: WinVerifyTrust on a file, the way package tools call it
 * (WINTRUST_ACTION_GENERIC_VERIFY_V2, no UI, whole-chain revocation from the
 * cache only), for test/appx-gate.sh. Prints "WinVerifyTrust=<hresult>".
 *
 *   trust-probe FILE             verify FILE
 *   trust-probe --root CERT.der  add a (test) root to the machine's Root store
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include <windows.h>
#include <wintrust.h>
#include <softpub.h>
#include <stdio.h>
#include <string.h>

int main( int argc, char **argv )
{
    WCHAR path[MAX_PATH];
    WINTRUST_FILE_INFO file = { sizeof(file) };
    WINTRUST_DATA data = { sizeof(data) };
    GUID action = WINTRUST_ACTION_GENERIC_VERIFY_V2;
    LONG hr;

    if (argc < 2) return 2;
    if (!strcmp( argv[1], "--root" ) && argc > 2)
    {
        BYTE der[8192];
        DWORD len;
        FILE *f = fopen( argv[2], "rb" );
        HCERTSTORE store;
        if (!f) { printf( "root=ERROR open\n" ); return 1; }
        len = fread( der, 1, sizeof(der), f );
        fclose( f );
        store = CertOpenStore( CERT_STORE_PROV_SYSTEM_W, 0, 0, CERT_SYSTEM_STORE_LOCAL_MACHINE, L"Root" );
        if (!store || !CertAddEncodedCertificateToStore( store, X509_ASN_ENCODING, der, len,
                                                          CERT_STORE_ADD_REPLACE_EXISTING, NULL ))
        { printf( "root=ERROR %#lx\n", GetLastError() ); return 1; }
        CertCloseStore( store, 0 );
        printf( "root=added\n" );
        return 0;
    }
    MultiByteToWideChar( CP_UTF8, 0, argv[1], -1, path, MAX_PATH );
    file.pcwszFilePath = path;
    data.dwUIChoice = WTD_UI_NONE;
    data.fdwRevocationChecks = WTD_REVOKE_WHOLECHAIN;
    data.dwUnionChoice = WTD_CHOICE_FILE;
    data.dwStateAction = WTD_STATEACTION_VERIFY;
    data.dwProvFlags = WTD_CACHE_ONLY_URL_RETRIEVAL;
    data.pFile = &file;
    hr = WinVerifyTrust( INVALID_HANDLE_VALUE, &action, &data );
    printf( "WinVerifyTrust=%#lx\n", (unsigned long)hr );
    data.dwStateAction = WTD_STATEACTION_CLOSE;
    WinVerifyTrust( INVALID_HANDLE_VALUE, &action, &data );
    return 0;
}
