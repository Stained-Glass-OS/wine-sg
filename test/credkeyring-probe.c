/* credkeyring-probe: CredWrite, CredRead, CredEnumerate and CredDelete from
 * a program, for test/credkeyring-gate.sh (patches/sg/1380).
 *
 *   write TARGET USER SECRET [TYPE [PERSIST]]   the secret as UTF-16, as most
 *                                               programs keep a password
 *   keep TARGET USER [TYPE]                     CRED_PRESERVE_CREDENTIAL_BLOB
 *   read TARGET [TYPE]                          OK user=.. secret=.. persist=..
 *   enum [FILTER] | enumall                     COUNT n, then target|user|secret
 *   delete TARGET [TYPE]
 *
 * Prints "ERR <GetLastError()>" on failure. */
#include <windows.h>
#include <wincred.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>

#ifndef CRED_ENUMERATE_ALL_CREDENTIALS
#define CRED_ENUMERATE_ALL_CREDENTIALS 0x1
#endif

static void out( const char *fmt, ... )
{
    char buf[4096];
    va_list ap;
    DWORD n;
    va_start( ap, fmt );
    vsnprintf( buf, sizeof(buf), fmt, ap );
    va_end( ap );
    WriteFile( GetStdHandle( STD_OUTPUT_HANDLE ), buf, (DWORD)strlen( buf ), &n, NULL );
}

static const char *u8( const WCHAR *s, char *buf, int size )
{
    if (!s) return "(null)";
    WideCharToMultiByte( CP_UTF8, 0, s, -1, buf, size, NULL, NULL );
    return buf;
}

static const char *secret8( const CREDENTIALW *c, char *buf, int size )
{
    WCHAR w[1024];
    DWORD n = c->CredentialBlobSize / sizeof(WCHAR);
    if (!c->CredentialBlob) return "(none)";
    if (n >= ARRAYSIZE(w)) n = ARRAYSIZE(w) - 1;
    memcpy( w, c->CredentialBlob, n * sizeof(WCHAR) );
    w[n] = 0;
    return u8( w, buf, size );
}

int wmain( int argc, WCHAR **argv )
{
    const WCHAR *cmd = argc > 1 ? argv[1] : L"";
    char a[1024], b[1024], c[1024];

    if (!wcscmp( cmd, L"write" ) && argc >= 5)
    {
        CREDENTIALW cred = { 0 };
        cred.Type = argc > 5 ? wcstoul( argv[5], NULL, 10 ) : CRED_TYPE_GENERIC;
        cred.Persist = argc > 6 ? wcstoul( argv[6], NULL, 10 ) : CRED_PERSIST_LOCAL_MACHINE;
        cred.TargetName = argv[2];
        cred.UserName = argv[3];
        cred.CredentialBlob = (BYTE *)argv[4];
        cred.CredentialBlobSize = (DWORD)(wcslen( argv[4] ) * sizeof(WCHAR));
        cred.Comment = (WCHAR *)L"gate comment";
        if (!CredWriteW( &cred, 0 )) { out( "ERR %lu\n", GetLastError() ); return 1; }
        out( "OK\n" );
        return 0;
    }
    if (!wcscmp( cmd, L"keep" ) && argc >= 4)
    {
        CREDENTIALW cred = { 0 };
        cred.Type = argc > 4 ? wcstoul( argv[4], NULL, 10 ) : CRED_TYPE_GENERIC;
        cred.Persist = CRED_PERSIST_LOCAL_MACHINE;
        cred.TargetName = argv[2];
        cred.UserName = argv[3];
        if (!CredWriteW( &cred, CRED_PRESERVE_CREDENTIAL_BLOB )) { out( "ERR %lu\n", GetLastError() ); return 1; }
        out( "OK\n" );
        return 0;
    }
    if (!wcscmp( cmd, L"read" ) && argc >= 3)
    {
        CREDENTIALW *cred;
        DWORD type = argc > 3 ? wcstoul( argv[3], NULL, 10 ) : CRED_TYPE_GENERIC;
        if (!CredReadW( argv[2], type, 0, &cred )) { out( "ERR %lu\n", GetLastError() ); return 1; }
        out( "OK target=%s user=%s secret=%s persist=%lu type=%lu comment=%s written=%s\n",
             u8( cred->TargetName, a, sizeof(a) ), u8( cred->UserName, b, sizeof(b) ),
             secret8( cred, c, sizeof(c) ), cred->Persist, cred->Type,
             cred->Comment ? "yes" : "no",
             cred->LastWritten.dwHighDateTime || cred->LastWritten.dwLowDateTime ? "yes" : "no" );
        CredFree( cred );
        return 0;
    }
    if (!wcscmp( cmd, L"enum" ) || !wcscmp( cmd, L"enumall" ))
    {
        CREDENTIALW **creds;
        DWORD count, i;
        BOOL all = !wcscmp( cmd, L"enumall" );
        if (!CredEnumerateW( all ? NULL : argc > 2 ? argv[2] : NULL, all ? CRED_ENUMERATE_ALL_CREDENTIALS : 0,
                             &count, &creds ))
        {
            out( "ERR %lu\n", GetLastError() );
            return 1;
        }
        out( "COUNT %lu\n", count );
        for (i = 0; i < count; i++)
        {
            if (((ULONG_PTR)creds[i]) % sizeof(void *)) out( "MISALIGNED %lu\n", i );
            out( "%s|%s|%s\n", u8( creds[i]->TargetName, a, sizeof(a) ), u8( creds[i]->UserName, b, sizeof(b) ),
                 secret8( creds[i], c, sizeof(c) ) );
        }
        CredFree( creds );
        return 0;
    }
    if (!wcscmp( cmd, L"delete" ) && argc >= 3)
    {
        DWORD type = argc > 3 ? wcstoul( argv[3], NULL, 10 ) : CRED_TYPE_GENERIC;
        if (!CredDeleteW( argv[2], type, 0 )) { out( "ERR %lu\n", GetLastError() ); return 1; }
        out( "OK\n" );
        return 0;
    }
    out( "usage\n" );
    return 2;
}
