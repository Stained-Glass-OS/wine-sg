/* PowerShell script signatures and the certificate stores (0404, 0405).
 *   pssig-probe addroot CERT.der   trust a test root (the user's Root store)
 *   pssig-probe verify FILE        WinVerifyTrust's answer and the SIP chosen
 *   pssig-probe stores             the system stores of both locations */
#include <windows.h>
#include <wincrypt.h>
#include <wintrust.h>
#include <softpub.h>
#include <mssip.h>
#include <stdio.h>

static BOOL WINAPI enum_store( const void *name, DWORD flags, PCERT_SYSTEM_STORE_INFO info, void *reserved, void *arg )
{
    printf( " %ls", (const WCHAR *)name );
    return TRUE;
}

int wmain( int argc, WCHAR **argv )
{
    if (argc > 2 && !wcscmp( argv[1], L"addroot" ))
    {
        BYTE buf[8192];
        DWORD n = 0;
        HANDLE f = CreateFileW( argv[2], GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL );
        ReadFile( f, buf, sizeof(buf), &n, NULL );
        CloseHandle( f );
        printf( "addroot=%d\n", CertAddEncodedCertificateToSystemStoreW( L"Root", buf, n ) );
    }
    else if (argc > 2 && !wcscmp( argv[1], L"verify" ))
    {
        GUID action = WINTRUST_ACTION_GENERIC_VERIFY_V2, subject;
        WINTRUST_FILE_INFO file = { sizeof(file), argv[2] };
        WINTRUST_DATA data = { sizeof(data) };
        LONG r;

        data.dwUIChoice = WTD_UI_NONE;
        data.fdwRevocationChecks = WTD_REVOKE_NONE;
        data.dwUnionChoice = WTD_CHOICE_FILE;
        data.pFile = &file;
        data.dwStateAction = WTD_STATEACTION_VERIFY;
        r = WinVerifyTrust( NULL, &action, &data );
        printf( "trust=%08lx\n", r );
        data.dwStateAction = WTD_STATEACTION_CLOSE;
        WinVerifyTrust( NULL, &action, &data );
        if (CryptSIPRetrieveSubjectGuid( argv[2], NULL, &subject ))
            printf( "sip={%08lx-%04x-%04x}\n", subject.Data1, subject.Data2, subject.Data3 );
        else printf( "sip=none %08lx\n", GetLastError() );
    }
    else if (argc > 1 && !wcscmp( argv[1], L"stores" ))
    {
        printf( "machine:" );
        CertEnumSystemStore( CERT_SYSTEM_STORE_LOCAL_MACHINE, NULL, NULL, enum_store );
        printf( "\nuser:" );
        CertEnumSystemStore( CERT_SYSTEM_STORE_CURRENT_USER, NULL, NULL, enum_store );
        printf( "\n" );
    }
    return 0;
}
