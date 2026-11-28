/* Running a program as another account (patches/sg/1711), run by
 * test/runasuser-gate.sh as several Unix users of a shared prefix:
 *   withlogon USER          CreateProcessWithLogonW (the password: a line of
 *                           standard input), the program "report OUT"
 *   withtoken USER          LogonUserW, then CreateProcessWithTokenW
 *   asuser SESSION          WTSQueryUserToken (SYSTEM), then
 *                           CreateProcessAsUserW
 *   inherit SESSION MODE    (i1010) as asuser, with an inheritable pipe and an inheritable
 *                           event: MODE "list" passes the pipe alone in
 *                           PROC_THREAD_ATTRIBUTE_HANDLE_LIST, "all" bInheritHandles
 *                           alone, "none" neither; prints "INHERIT <exit> <pipe text>"
 *   handles PIPE EVENT      (the started program) writes "inherited" to PIPE when it
 *                           can; exits 10 + 1 (it could) + 2 (EVENT is a handle)
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
    if (argc > 3 && !strcmp( argv[1], "handles" ))
    {
        HANDLE pipe = (HANDLE)(ULONG_PTR)strtoul( argv[2], NULL, 0 ), event = (HANDLE)(ULONG_PTR)strtoul( argv[3], NULL, 0 );
        DWORD written = 0, flags, code = 10, len = 64;
        char text[80] = "inherited:";
        GetUserNameA( text + 10, &len );
        if (argc > 4) CloseHandle( CreateFileA( argv[4], GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL ) );
        if (WriteFile( pipe, text, strlen( text ), &written, NULL ) && written == strlen( text )) code += 1;
        if (GetHandleInformation( event, &flags )) code += 2;
        return code;
    }
    if (argc > 3 && !strcmp( argv[1], "inherit" ))
    {
        SECURITY_ATTRIBUTES sa = { sizeof(sa), NULL, TRUE };
        STARTUPINFOEXW six = {{ sizeof(six) }};
        HANDLE rd, wr, event, list[1];
        SIZE_T size = 0;
        DWORD code = 0xdeadbeef, got = 0;
        char text[96] = "";
        BOOL inherit = strcmp( argv[3], "none" ) != 0, ok;

        int i;

        if (!WTSQueryUserToken( atoi( argv[2] ), &token )) { printf( "TOKEN 0 %lu\n", GetLastError() ); return 0; }
        /* high values, which the new program's own handles do not reach */
        for (i = 0; i < 400; i++) CreateEventW( NULL, TRUE, FALSE, NULL );
        CreatePipe( &rd, &wr, &sa, 0 );
        SetHandleInformation( rd, HANDLE_FLAG_INHERIT, 0 );
        event = CreateEventW( &sa, TRUE, FALSE, NULL );
        swprintf( cmd, ARRAYSIZE(cmd), L"C:\\runasuser-probe.exe handles %lu %lu C:\\out\\inherit-%hs",
                  (unsigned long)(ULONG_PTR)wr, (unsigned long)(ULONG_PTR)event, argv[3] );
        if (!strcmp( argv[3], "list" ))
        {
            list[0] = wr;
            InitializeProcThreadAttributeList( NULL, 1, 0, &size );
            six.lpAttributeList = HeapAlloc( GetProcessHeap(), 0, size );
            InitializeProcThreadAttributeList( six.lpAttributeList, 1, 0, &size );
            UpdateProcThreadAttribute( six.lpAttributeList, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST, list, sizeof(list), NULL, NULL );
            ok = CreateProcessAsUserW( token, NULL, cmd, NULL, NULL, TRUE, EXTENDED_STARTUPINFO_PRESENT, NULL, L"C:\\",
                                       &six.StartupInfo, &pi );
        }
        else ok = CreateProcessAsUserW( token, NULL, cmd, NULL, NULL, inherit, 0, NULL, L"C:\\", &si, &pi );
        CloseHandle( wr );
        if (!ok) { printf( "STARTED 0 %lu\n", GetLastError() ); return 0; }
        WaitForSingleObject( pi.hProcess, 90000 );
        GetExitCodeProcess( pi.hProcess, &code );
        if (!ReadFile( rd, text, sizeof(text) - 1, &got, NULL )) got = 0;
        text[got] = 0;
        printf( "INHERIT %lu %s\n", code, got ? text : "-" );
        return 0;
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
