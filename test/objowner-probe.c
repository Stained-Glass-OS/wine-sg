/* objowner-probe: objects created without a security descriptor have their
 * creator as owner and group (patches/sg/0082), for test/objowner-gate.sh.
 *
 *   objowner-probe        each object kind: owner is the token's owner, group
 *                         the token's primary group, no DACL; then a child
 *                         with Administrators disabled opens the named event
 *                         and section with full access (access unchanged)
 *   objowner-probe open   (the child) open them
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include <windows.h>
#include <winternl.h>
#include <stdio.h>

NTSTATUS WINAPI NtQuerySecurityObject( HANDLE, SECURITY_INFORMATION, PSECURITY_DESCRIPTOR, ULONG, ULONG * );

static BYTE token_owner[256], token_group[256];

static void check( const char *what, HANDLE h )
{
    BYTE buf[4096];
    ULONG len = 0;
    PSID owner = NULL, group = NULL;
    BOOL defaulted, present = FALSE;
    PACL dacl = NULL;
    NTSTATUS st = NtQuerySecurityObject( h, OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION |
                                         DACL_SECURITY_INFORMATION, buf, sizeof(buf), &len );
    if (!st)
    {
        GetSecurityDescriptorOwner( buf, &owner, &defaulted );
        GetSecurityDescriptorGroup( buf, &group, &defaulted );
        GetSecurityDescriptorDacl( buf, &present, &dacl, &defaulted );
    }
    printf( "%s status=%#lx owner=%s group=%s dacl=%s\n", what, st,
            !owner ? "none" : EqualSid( owner, ((TOKEN_OWNER *)token_owner)->Owner ) ? "creator" : "other",
            !group ? "none" : EqualSid( group, ((TOKEN_PRIMARY_GROUP *)token_group)->PrimaryGroup ) ? "creator" : "other",
            present ? "present" : "absent" );
}

int main( int argc, char **argv )
{
    HANDLE token, r, w, s, c;
    DWORD len;

    if (argc == 2 && !strcmp( argv[1], "open" ))
    {
        HANDLE e = OpenEventW( EVENT_ALL_ACCESS, FALSE, L"sg-objowner-event" );
        HANDLE m = OpenFileMappingW( FILE_MAP_ALL_ACCESS, FALSE, L"sg-objowner-section" );
        printf( "restricted-open event=%d section=%d\n", e != NULL, m != NULL );
        return 0;
    }

    OpenProcessToken( GetCurrentProcess(), TOKEN_QUERY | TOKEN_DUPLICATE | TOKEN_ASSIGN_PRIMARY, &token );
    GetTokenInformation( token, TokenOwner, token_owner, sizeof(token_owner), &len );
    GetTokenInformation( token, TokenPrimaryGroup, token_group, sizeof(token_group), &len );

    CreatePipe( &r, &w, NULL, 0 );
    check( "anonymous-pipe", r );
    s = CreateNamedPipeW( L"\\\\.\\pipe\\sg-objowner", PIPE_ACCESS_DUPLEX, 0, 1, 512, 512, 0, NULL );
    check( "named-pipe-server", s );
    c = CreateFileW( L"\\\\.\\pipe\\sg-objowner", GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL );
    check( "named-pipe-client", c );
    check( "event", CreateEventW( NULL, TRUE, FALSE, NULL ) );
    check( "named-event", CreateEventW( NULL, TRUE, FALSE, L"sg-objowner-event" ) );
    check( "mutex", CreateMutexW( NULL, FALSE, NULL ) );
    check( "semaphore", CreateSemaphoreW( NULL, 0, 1, NULL ) );
    check( "named-section", CreateFileMappingW( INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, 4096, L"sg-objowner-section" ) );

    /* access is unchanged: a process without Administrators still opens them fully */
    {
        SID_IDENTIFIER_AUTHORITY nt = { SECURITY_NT_AUTHORITY };
        SID_AND_ATTRIBUTES disable = { 0 };
        STARTUPINFOW si = { sizeof(si) };
        PROCESS_INFORMATION pi;
        WCHAR cmd[MAX_PATH + 16], self[MAX_PATH];
        HANDLE restricted;

        AllocateAndInitializeSid( &nt, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &disable.Sid );
        if (!CreateRestrictedToken( token, 0, 1, &disable, 0, NULL, 0, NULL, &restricted )) { printf( "restricted token failed\n" ); return 1; }
        GetModuleFileNameW( NULL, self, MAX_PATH );
        swprintf( cmd, MAX_PATH + 16, L"\"%ls\" open", self );
        fflush( stdout );
        if (!CreateProcessAsUserW( restricted, NULL, cmd, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi )) { printf( "child failed %lu\n", GetLastError() ); return 1; }
        WaitForSingleObject( pi.hProcess, 60000 );
    }
    return 0;
}
