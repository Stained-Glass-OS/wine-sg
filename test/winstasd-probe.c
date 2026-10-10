/* Window station and desktop security (patches/sg/1707), run by
 * test/winstasd-gate.sh as several Unix users against one wineserver:
 *   win         make a window and exit: 0 if it could
 *   hold        make a window (so WinSta0 and its desktop exist), print the
 *               session, stay 240 s
 *   own         print the session, the user's SID; the owner and the DACL (SID mask) of the
 *               process's window station and of its desktop
 *               (GetSecurityInfo SE_WINDOW_OBJECT); then add an Everyone ACE
 *               to the desktop's DACL with SetSecurityInfo and read it back
 *   open N      open session N's WinSta0 by its \Sessions path: for reading
 *               (WINSTA_ENUMDESKTOPS) and for the clipboard
 *               (WINSTA_ACCESSCLIPBOARD) */
#include <windows.h>
#include <winternl.h>
#include <aclapi.h>
#include <sddl.h>
#include <stdio.h>

#ifndef DIRECTORY_TRAVERSE
#define DIRECTORY_TRAVERSE 0x0002
#endif

typedef HWINSTA (WINAPI *open_winsta_fn)( OBJECT_ATTRIBUTES *attr, ACCESS_MASK access );
typedef NTSTATUS (WINAPI *open_dir_fn)( HANDLE *handle, ACCESS_MASK access, OBJECT_ATTRIBUTES *attr );

static void print_sd( const char *what, HANDLE obj )
{
    PSID owner = NULL;
    PACL dacl = NULL;
    PSECURITY_DESCRIPTOR sd = NULL;
    char *str;
    DWORD err, i;

    err = GetSecurityInfo( obj, SE_WINDOW_OBJECT, OWNER_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION,
                           &owner, NULL, &dacl, NULL, &sd );
    if (err) { printf( "%s ERROR %lu\n", what, err ); return; }
    if (owner && ConvertSidToStringSidA( owner, &str )) { printf( "%s OWNER %s\n", what, str ); LocalFree( str ); }
    if (!dacl) printf( "%s NODACL\n", what );
    for (i = 0; dacl && i < dacl->AceCount; i++)
    {
        ACCESS_ALLOWED_ACE *ace;
        if (!GetAce( dacl, i, (void **)&ace )) continue;
        if (ConvertSidToStringSidA( &ace->SidStart, &str ))
        {
            printf( "%s ACE %d %s %#lx\n", what, ace->Header.AceType, str, ace->Mask );
            LocalFree( str );
        }
    }
    LocalFree( sd );
}

int main( int argc, char **argv )
{
    DWORD session = 0xdeadbeef;

    ProcessIdToSessionId( GetCurrentProcessId(), &session );
    printf( "SESSION %lu\n", session );
    fflush( stdout );
    if (argc < 2) return 1;

    if (!strcmp( argv[1], "win" ))
    {
        HWND hwnd = CreateWindowA( "STATIC", "winstasd", WS_OVERLAPPEDWINDOW, 0, 0, 10, 10, NULL, NULL, NULL, NULL );
        printf( "WIN %d\n", hwnd != NULL );
        return !hwnd;
    }
    if (!strcmp( argv[1], "hold" ))
    {
        HWND hwnd = NULL;
        int i;
        for (i = 0; i < 120 && !hwnd; i++)
            if (!(hwnd = CreateWindowA( "STATIC", "winstasd", WS_OVERLAPPEDWINDOW, 0, 0, 10, 10, NULL, NULL, NULL, NULL )))
                Sleep( 1000 );
        printf( "HELD %d\n", hwnd != NULL );
        fflush( stdout );
        Sleep( 240000 );
        return 0;
    }
    if (!strcmp( argv[1], "own" ))
    {
        HDESK desk = GetThreadDesktop( GetCurrentThreadId() );
        PACL dacl = NULL, new_dacl = NULL;
        PSECURITY_DESCRIPTOR sd = NULL;
        EXPLICIT_ACCESSA ea = {0};
        DWORD err;

        {
            HANDLE token;
            char buf[256], *str;
            DWORD len;
            if (OpenProcessToken( GetCurrentProcess(), TOKEN_QUERY, &token )
                && GetTokenInformation( token, TokenUser, buf, sizeof(buf), &len )
                && ConvertSidToStringSidA( ((TOKEN_USER *)buf)->User.Sid, &str ))
            {
                printf( "USER %s\n", str );
                LocalFree( str );
            }
        }
        print_sd( "WINSTA", GetProcessWindowStation() );
        print_sd( "DESKTOP", desk );
        err = GetSecurityInfo( desk, SE_WINDOW_OBJECT, DACL_SECURITY_INFORMATION, NULL, NULL, &dacl, NULL, &sd );
        if (!err)
        {
            ea.grfAccessPermissions = DESKTOP_READOBJECTS;
            ea.grfAccessMode = GRANT_ACCESS;
            ea.Trustee.TrusteeForm = TRUSTEE_IS_NAME;
            ea.Trustee.ptstrName = (char *)"Everyone";
            if (!(err = SetEntriesInAclA( 1, &ea, dacl, &new_dacl )))
                err = SetSecurityInfo( desk, SE_WINDOW_OBJECT, DACL_SECURITY_INFORMATION, NULL, NULL, new_dacl, NULL );
        }
        printf( "SETSEC %lu\n", err );
        print_sd( "AFTER", desk );
        LocalFree( new_dacl );
        LocalFree( sd );
        return 0;
    }
    if (!strcmp( argv[1], "open" ) && argc > 2)
    {
        open_winsta_fn open_winsta = (open_winsta_fn)GetProcAddress( GetModuleHandleA( "win32u" ), "NtUserOpenWindowStation" );
        open_dir_fn open_dir = (open_dir_fn)GetProcAddress( GetModuleHandleA( "ntdll" ), "NtOpenDirectoryObject" );
        static const ACCESS_MASK rights[2] = { WINSTA_ENUMDESKTOPS, WINSTA_ACCESSCLIPBOARD };
        static const char *names[2] = { "READ", "CLIPBOARD" };
        WCHAR path[128];
        UNICODE_STRING str, name;
        OBJECT_ATTRIBUTES attr;
        HANDLE dir;
        NTSTATUS status;
        int i;

        if (!open_winsta || !open_dir) { printf( "NOAPI\n" ); return 1; }
        swprintf( path, ARRAYSIZE(path), L"\\Sessions\\%hs\\Windows\\WindowStations", argv[2] );
        RtlInitUnicodeString( &str, path );
        InitializeObjectAttributes( &attr, &str, 0, NULL, NULL );
        if ((status = open_dir( &dir, DIRECTORY_TRAVERSE, &attr ))) { printf( "NODIR %#lx\n", status ); return 1; }
        RtlInitUnicodeString( &name, L"WinSta0" );
        for (i = 0; i < 2; i++)
        {
            HWINSTA ws;
            InitializeObjectAttributes( &attr, &name, OBJ_CASE_INSENSITIVE, dir, NULL );
            SetLastError( 0 );
            ws = open_winsta( &attr, rights[i] );
            printf( "OPEN %s %d %lu\n", names[i], ws != NULL, ws ? 0 : GetLastError() );
            if (ws) CloseWindowStation( ws );
        }
        return 0;
    }
    return 1;
}
