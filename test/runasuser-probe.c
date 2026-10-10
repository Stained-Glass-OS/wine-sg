/* Running a program as another account (patches/sg/1711), run by
 * test/runasuser-gate.sh as several Unix users of a shared prefix:
 *   withlogon USER          CreateProcessWithLogonW (the password: a line of
 *                           standard input), the program "report OUT"
 *   withtoken USER          LogonUserW, then CreateProcessWithTokenW
 *   asuser SESSION          WTSQueryUserToken (SYSTEM), then
 *                           CreateProcessAsUserW
 *   report OUT              (the started program) write to OUT: its user's
 *                           SID, its session, USERNAME's, USERPROFILE's and SG_PROBE_SECRET's
 *                           values; exit 7
 * The first three print "STARTED 1 <pid>" and then "EXIT <code>" once the
 * program has ended, or "STARTED 0 <error>". */
#include <windows.h>
#include <wtsapi32.h>
#include <sddl.h>
#include <stdio.h>

static void read_password( WCHAR *pass, DWORD len )
{
    char line[128] = "", *nl;
    fgets( line, sizeof(line), stdin );
    if ((nl = strchr( line, '\n' ))) *nl = 0;
    MultiByteToWideChar( CP_ACP, 0, line, -1, pass, len );
    SecureZeroMemory( line, sizeof(line) );
}

static void started( BOOL ok, PROCESS_INFORMATION *pi )
{
    DWORD code = 0xdeadbeef;
    if (!ok) { printf( "STARTED 0 %lu\n", GetLastError() ); return; }
    printf( "STARTED 1 %lu\n", pi->dwProcessId );
    fflush( stdout );
    if (WaitForSingleObject( pi->hProcess, 90000 )) printf( "WAIT %lu\n", GetLastError() );
    GetExitCodeProcess( pi->hProcess, &code );
    printf( "EXIT %lu\n", code );
    CloseHandle( pi->hProcess );
    CloseHandle( pi->hThread );
}

int main( int argc, char **argv )
{
    WCHAR cmd[512], user[64], pass[128];
    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi = {0};
    HANDLE token;

    if (argc > 2 && !strcmp( argv[1], "report" ))
    {
        FILE *f = fopen( argv[2], "w" );
        char buf[256], *sid = NULL, val[256];
        DWORD len, session = 0;

        if (!f) return 1;
        if (GetTokenInformation( GetCurrentProcessToken(), TokenUser, buf, sizeof(buf), &len ))
            ConvertSidToStringSidA( ((TOKEN_USER *)buf)->User.Sid, &sid );
        ProcessIdToSessionId( GetCurrentProcessId(), &session );
        fprintf( f, "SID %s\nSESSION %lu\n", sid ? sid : "?", session );
        if (!GetEnvironmentVariableA( "USERNAME", val, sizeof(val) )) strcpy( val, "-" );
        fprintf( f, "USERNAME %s\n", val );
        if (!GetEnvironmentVariableA( "USERPROFILE", val, sizeof(val) )) strcpy( val, "-" );
        fprintf( f, "PROFILE %s\n", val );
        if (!GetEnvironmentVariableA( "SG_PROBE_SECRET", val, sizeof(val) )) strcpy( val, "-" );
        fprintf( f, "SECRET %s\n", val );
        fclose( f );
        return 7;
    }
    if (argc < 4) return 1;
    swprintf( cmd, ARRAYSIZE(cmd), L"C:\\runasuser-probe.exe report %hs", argv[3] );
    if (!strcmp( argv[1], "withlogon" ))
    {
        MultiByteToWideChar( CP_ACP, 0, argv[2], -1, user, ARRAYSIZE(user) );
        read_password( pass, ARRAYSIZE(pass) );
        started( CreateProcessWithLogonW( user, L".", pass, LOGON_WITH_PROFILE, NULL, cmd, 0, NULL, L"C:\\",
                                          &si, &pi ), &pi );
        return 0;
    }
    if (!strcmp( argv[1], "withtoken" ))
    {
        MultiByteToWideChar( CP_ACP, 0, argv[2], -1, user, ARRAYSIZE(user) );
        read_password( pass, ARRAYSIZE(pass) );
        if (!LogonUserW( user, L".", pass, LOGON32_LOGON_INTERACTIVE, LOGON32_PROVIDER_DEFAULT, &token ))
        {
            printf( "LOGON 0 %lu\n", GetLastError() );
            return 0;
        }
        started( CreateProcessWithTokenW( token, 0, NULL, cmd, 0, NULL, L"C:\\", &si, &pi ), &pi );
        return 0;
    }
    if (!strcmp( argv[1], "asuser" ))
    {
        if (!WTSQueryUserToken( atoi( argv[2] ), &token ))
        {
            printf( "TOKEN 0 %lu\n", GetLastError() );
            return 0;
        }
        started( CreateProcessAsUserW( token, NULL, cmd, NULL, NULL, FALSE, 0, NULL, L"C:\\", &si, &pi ), &pi );
        return 0;
    }
    return 1;
}
