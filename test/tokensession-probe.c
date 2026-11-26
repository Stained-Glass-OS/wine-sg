/* SetTokenInformation(TokenSessionId) and a service's helper started in a
 * person's session (patches/sg/9624, 9625), run by test/tokensession-gate.sh
 * as several Unix users against one wineserver:
 *   hold [S]        stay S s (the user is logged on to its session)
 *   set N           duplicate the own token, give the copy session N:
 *                   "SET 1 <session the copy has>" or "SET 0 <error> <session>"
 *   spawn N CMD     the same, then CreateProcessAsUser(copy, CMD) on
 *                   winsta0\default, as a remote-support service starts its
 *                   helper: "SPAWN 1 <exit code>" or "SPAWN 0 <error>"
 *   whoami [FILE]   "WHO <session> <user SID> <SG_COMPOSITOR_PRIV or ->", to
 *                   FILE if given (a started helper has no stdout of ours)
 * The first line is always "SESSION <own session>". */
#include <windows.h>
#include <sddl.h>
#include <stdio.h>
#include <stdlib.h>

static HANDLE copy_with_session( DWORD id, DWORD *err, DWORD *now )
{
    HANDLE own, copy = NULL;
    DWORD len;

    *err = 0;
    *now = 0xdeadbeef;
    if (!OpenProcessToken( GetCurrentProcess(), TOKEN_ALL_ACCESS, &own )) { *err = GetLastError(); return NULL; }
    if (!DuplicateTokenEx( own, TOKEN_ALL_ACCESS, NULL, SecurityImpersonation, TokenPrimary, &copy ))
    {
        *err = GetLastError();
        CloseHandle( own );
        return NULL;
    }
    CloseHandle( own );
    SetLastError( 0xdeadbeef );
    if (!SetTokenInformation( copy, TokenSessionId, &id, sizeof(id) )) *err = GetLastError();
    GetTokenInformation( copy, TokenSessionId, now, sizeof(*now), &len );
    return copy;
}

int main( int argc, char **argv )
{
    DWORD session = 0xdeadbeef, err, now;

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
    if (!strcmp( argv[1], "whoami" ))
    {
        HANDLE token;
        char buf[256], *sid = NULL;
        const char *priv = getenv( "SG_COMPOSITOR_PRIV" );
        DWORD len;

        if (OpenProcessToken( GetCurrentProcess(), TOKEN_QUERY, &token ) &&
            GetTokenInformation( token, TokenUser, buf, sizeof(buf), &len ))
            ConvertSidToStringSidA( ((TOKEN_USER *)buf)->User.Sid, &sid );
        FILE *out = argc > 2 ? fopen( argv[2], "w" ) : stdout;
        if (!out) return 2;
        fprintf( out, "WHO %lu %s %s\n", session, sid ? sid : "?", priv && *priv ? priv : "-" );
        if (out != stdout) fclose( out );
        return 0;
    }
    if (!strcmp( argv[1], "set" ) && argc > 2)
    {
        HANDLE copy = copy_with_session( strtoul( argv[2], NULL, 10 ), &err, &now );
        if (err) printf( "SET 0 %lu %lu\n", err, now );
        else printf( "SET 1 %lu\n", now );
        if (copy) CloseHandle( copy );
        return 0;
    }
    if (!strcmp( argv[1], "spawn" ) && argc > 3)
    {
        HANDLE copy = copy_with_session( strtoul( argv[2], NULL, 10 ), &err, &now );
        STARTUPINFOA si = { sizeof(si) };
        PROCESS_INFORMATION pi;
        DWORD code = 0xdeadbeef;
        char cmd[1024];

        if (!copy || err) { printf( "SPAWN 0 %lu\n", err ); return 0; }
        snprintf( cmd, sizeof(cmd), "%s", argv[3] );
        si.lpDesktop = (char *)"winsta0\\default";
        if (!CreateProcessAsUserA( copy, NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi ))
        {
            printf( "SPAWN 0 %lu\n", GetLastError() );
            return 0;
        }
        WaitForSingleObject( pi.hProcess, 60000 );
        GetExitCodeProcess( pi.hProcess, &code );
        printf( "SPAWN 1 %lu\n", code );
        return 0;
    }
    return 1;
}
