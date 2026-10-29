/* WTSQueryUserToken (patches/sg/1708), run by test/wtstoken-gate.sh as
 * several Unix users against one wineserver:
 *   hold [S]    stay S s, default 240 (the user is logged on to its session)
 *   query N     WTSQueryUserToken(N) (N = "cur" for WTS_CURRENT_SESSION,
 *               "console" for WTSGetActiveConsoleSessionId's, 5964):
 *               "QUERY 1 <user SID> <session> <type>" or "QUERY 0 <error>"
 *   console     "CONSOLE <WTSGetActiveConsoleSessionId>" */
#include <windows.h>
#include <wtsapi32.h>
#include <sddl.h>
#include <stdio.h>
#include <stdlib.h>

int main( int argc, char **argv )
{
    DWORD session = 0xdeadbeef;

    ProcessIdToSessionId( GetCurrentProcessId(), &session );
    printf( "SESSION %lu\n", session );
    fflush( stdout );
    if (argc < 2) return 1;
    if (!strcmp( argv[1], "hold" ))
    {
        printf( "HELD 1\n" );
        fflush( stdout );
        Sleep( (argc > 2 ? atoi( argv[2] ) : 240) * 1000 );
        return 0;
    }
    if (!strcmp( argv[1], "console" ))
    {
        printf( "CONSOLE %lu\n", WTSGetActiveConsoleSessionId() );
        return 0;
    }
    if (!strcmp( argv[1], "query" ) && argc > 2)
    {
        ULONG id = !strcmp( argv[2], "cur" ) ? WTS_CURRENT_SESSION :
                   !strcmp( argv[2], "console" ) ? WTSGetActiveConsoleSessionId() : strtoul( argv[2], NULL, 10 );
        HANDLE token = NULL;
        char buf[256], *sid = NULL;
        DWORD len, tsession = 0xdeadbeef;
        TOKEN_TYPE type = 0;

        SetLastError( 0xdeadbeef );
        if (!WTSQueryUserToken( id, &token ))
        {
            printf( "QUERY 0 %lu\n", GetLastError() );
            return 0;
        }
        if (GetTokenInformation( token, TokenUser, buf, sizeof(buf), &len ))
            ConvertSidToStringSidA( ((TOKEN_USER *)buf)->User.Sid, &sid );
        GetTokenInformation( token, TokenSessionId, &tsession, sizeof(tsession), &len );
        GetTokenInformation( token, TokenType, &type, sizeof(type), &len );
        printf( "QUERY 1 %s %lu %s\n", sid ? sid : "?", tsession, type == TokenPrimary ? "primary" : "impersonation" );
        LocalFree( sid );
        CloseHandle( token );
        return 0;
    }
    return 1;
}
