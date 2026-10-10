/* LogonUser (patches/sg/1709), run by test/logonuser-gate.sh as a Unix user
 * of a shared prefix:
 *   logon USER PASSWORD TYPE   LogonUserExW (PASSWORD "-": a line of standard input): "LOGON 1 <user SID> <session>
 *                              <primary|impersonation>" and "LOGONSID 0|1",
 *                              then, impersonating it, "IMPERSONATE <name>";
 *                              or "LOGON 0 <error>"
 *   me                         "ME <user SID>" */
#include <windows.h>
#include <sddl.h>
#include <stdio.h>

static void print_user( const char *what, HANDLE token )
{
    char buf[256], *sid = NULL;
    DWORD len;

    if (GetTokenInformation( token, TokenUser, buf, sizeof(buf), &len ))
        ConvertSidToStringSidA( ((TOKEN_USER *)buf)->User.Sid, &sid );
    printf( "%s %s", what, sid ? sid : "?" );
    LocalFree( sid );
}

int main( int argc, char **argv )
{
    if (argc > 1 && !strcmp( argv[1], "me" ))
    {
        print_user( "ME", GetCurrentProcessToken() );
        printf( "\n" );
        return 0;
    }
    if (argc > 4 && !strcmp( argv[1], "logon" ))
    {
        WCHAR user[128], pass[128], name[128];
        DWORD type = atoi( argv[4] ), session = 0, len = 0, size = ARRAYSIZE(name);
        TOKEN_TYPE ttype = 0;
        HANDLE token = NULL;
        PSID logon_sid = NULL;

        MultiByteToWideChar( CP_ACP, 0, argv[2], -1, user, ARRAYSIZE(user) );
        if (!strcmp( argv[3], "-" ))   /* from standard input: not on the command line */
        {
            char line[128] = "", *nl;
            fgets( line, sizeof(line), stdin );
            if ((nl = strchr( line, '\n' ))) *nl = 0;
            MultiByteToWideChar( CP_ACP, 0, line, -1, pass, ARRAYSIZE(pass) );
            SecureZeroMemory( line, sizeof(line) );
        }
        else MultiByteToWideChar( CP_ACP, 0, argv[3], -1, pass, ARRAYSIZE(pass) );
        SetLastError( 0xdeadbeef );
        if (!LogonUserExW( user, L".", pass, type, LOGON32_PROVIDER_DEFAULT, &token, &logon_sid, NULL, NULL, NULL ))
        {
            printf( "LOGON 0 %lu\n", GetLastError() );
            return 0;
        }
        GetTokenInformation( token, TokenSessionId, &session, sizeof(session), &len );
        GetTokenInformation( token, TokenType, &ttype, sizeof(ttype), &len );
        print_user( "LOGON 1", token );
        printf( " %lu %s\n", session, ttype == TokenPrimary ? "primary" : "impersonation" );
        printf( "LOGONSID %d\n", logon_sid != NULL );
        LocalFree( logon_sid );
        if (ImpersonateLoggedOnUser( token ))
        {
            if (GetUserNameW( name, &size )) printf( "IMPERSONATE %ls\n", name );
            RevertToSelf();
        }
        CloseHandle( token );
        return 0;
    }
    return 1;
}
