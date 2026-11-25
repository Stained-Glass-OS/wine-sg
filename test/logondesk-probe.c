/* Logon SIDs and the desktop's descriptor (patches/sg/9060), run by test/logondesk-gate.sh (a
 * single user's prefix) and test/logondesk-users-gate.sh (several Unix users on one shared
 * prefix's wineserver):
 *   info [FILE] the session, the user's SID, the token's logon SID (SE_GROUP_LOGON_ID), and the
 *               owner and DACL (type SID mask) of the process's window station and desktop
 *   renderer    as Chromium's sandbox starts a renderer (sandbox/win/src/window.cc): an alternate
 *               desktop made with no security attributes, RESTRICTED's denial of DELETE,
 *               WRITE_DAC, WRITE_OWNER and the desktop's write rights merged into its DACL, the
 *               child on it with the initial renderer token (every SID of the token restricting,
 *               low integrity)
 *   limited     a child on this desktop with Chromium's USER_LIMITED token (restricting SIDs
 *               Users, Everyone, RESTRICTED and the logon SID; the other groups deny-only; low
 *               integrity), as its GPU, audio and utility processes run
 *   limited-alt the USER_LIMITED child on the alternate desktop of "renderer", after a renderer
 *   limited-alone the USER_LIMITED child alone on the alternate desktop: nobody there may make its
 *               desktop window (9061: no explorer is started again and again for it)
 *   open N      open session N's WinSta0 by its \Sessions path for reading (WINSTA_ENUMDESKTOPS)
 *               and for the clipboard (WINSTA_ACCESSCLIPBOARD); then the same from a child with
 *               a USER_LIMITED token
 *   hold        make a window (so WinSta0 and its desktop exist), stay 600 s
 * Children print nothing; the parent prints
 *   CHILD <exit code>   bit 0: the thread has a desktop; 1: it is the one it was started on;
 *                       2: there is a desktop window; 3: a window could be made; 4: its desktop handle
 *                       has DESKTOP_CREATEWINDOW; 5: it can open its desktop by name itself (63 when all
 *                       hold); 4096: it hung (killed after 90 s) */
#include <windows.h>
#include <winternl.h>
#include <sddl.h>
#include <aclapi.h>
#include <stdio.h>

#ifndef DIRECTORY_TRAVERSE
#define DIRECTORY_TRAVERSE 0x0002
#endif

typedef HWINSTA (WINAPI *open_winsta_fn)( OBJECT_ATTRIBUTES *attr, ACCESS_MASK access );
typedef NTSTATUS (WINAPI *open_dir_fn)( HANDLE *handle, ACCESS_MASK access, OBJECT_ATTRIBUTES *attr );

static void print_sid( const char *what, PSID sid )
{
    char *str;
    if (sid && ConvertSidToStringSidA( sid, &str )) { printf( "%s %s\n", what, str ); LocalFree( str ); }
    else printf( "%s NONE\n", what );
}

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

static TOKEN_GROUPS *token_groups( HANDLE token )
{
    TOKEN_GROUPS *groups;
    DWORD len = 0;
    GetTokenInformation( token, TokenGroups, NULL, 0, &len );
    if (!(groups = malloc( len ))) return NULL;
    if (!GetTokenInformation( token, TokenGroups, groups, len, &len )) { free( groups ); return NULL; }
    return groups;
}

static PSID logon_sid( HANDLE token )
{
    TOKEN_GROUPS *groups = token_groups( token );
    DWORD i;
    if (!groups) return NULL;
    for (i = 0; i < groups->GroupCount; i++)
        if (groups->Groups[i].Attributes & SE_GROUP_LOGON_ID) return groups->Groups[i].Sid;  /* leaked */
    return NULL;
}

static int child( const char *desk_name )
{
    HDESK desk = GetThreadDesktop( GetCurrentThreadId() );
    char name[64] = "";
    DWORD n;
    int ret = 0;
    HWND hwnd;

    if (desk)
    {
        OBJECT_BASIC_INFORMATION info;
        ret |= 1;
        /* the rights it was connected with: its own token's (MAXIMUM_ALLOWED), as on Windows */
        if (!NtQueryObject( desk, ObjectBasicInformation, &info, sizeof(info), NULL ) &&
            (info.GrantedAccess & DESKTOP_CREATEWINDOW)) ret |= 16;
    }
    if (desk && GetUserObjectInformationA( desk, UOI_NAME, name, sizeof(name), &n ) &&
        (!desk_name[0] || !lstrcmpiA( name, desk_name ))) ret |= 2;
    if (GetDesktopWindow()) ret |= 4;
    if (name[0])
    {
        /* the child's own token against the desktop's DACL (OpenDesktop checks it; the server
         * connected the process with its parent's) */
        HDESK again = OpenDesktopA( name, 0, FALSE, DESKTOP_READOBJECTS );
        if (again) { ret |= 32; CloseDesktop( again ); }
    }
    hwnd = CreateWindowExA( 0, "STATIC", "probe", WS_POPUP, 0, 0, 10, 10, NULL, NULL, NULL, NULL );
    if (hwnd) { ret |= 8; DestroyWindow( hwnd ); }
    return ret;
}

/* Chromium's alternate desktop: made with no security attributes, then RESTRICTED's denial merged
 * into whatever DACL it got */
static int make_alt_desktop( const char *alt )
{
    PACL old = NULL, merged = NULL;
    PSECURITY_DESCRIPTOR sd = NULL;
    EXPLICIT_ACCESSA ea;
    PSID rc_sid;
    HDESK desk;
    DWORD err;

    if (!(desk = CreateDesktopA( alt, NULL, NULL, 0, GENERIC_ALL, NULL )))
    {
        printf( "CHILD nodesktop %lu\n", GetLastError() );
        return 0;
    }
    ConvertStringSidToSidA( "S-1-5-12", &rc_sid );
    GetSecurityInfo( desk, SE_WINDOW_OBJECT, DACL_SECURITY_INFORMATION, NULL, NULL, &old, NULL, &sd );
    memset( &ea, 0, sizeof(ea) );
    ea.grfAccessPermissions = 0xd013e;
    ea.grfAccessMode = DENY_ACCESS;
    ea.Trustee.TrusteeForm = TRUSTEE_IS_SID;
    ea.Trustee.ptstrName = rc_sid;
    if ((err = SetEntriesInAclA( 1, &ea, old, &merged )) ||
        (err = SetSecurityInfo( desk, SE_WINDOW_OBJECT, DACL_SECURITY_INFORMATION, NULL, NULL, merged, NULL )))
        printf( "MERGE fail %lu\n", err );
    print_sd( "ALT", desk );
    return 1;
}

/* the initial renderer token (USER_RESTRICTED_SAME_ACCESS): every SID restricting */
static HANDLE renderer_token( HANDLE token )
{
    TOKEN_GROUPS *groups = token_groups( token );
    SID_AND_ATTRIBUTES *rs;
    BYTE buf[256];
    DWORD len, i, n = 0;
    HANDLE restricted = NULL;

    if (!groups) return NULL;
    rs = calloc( groups->GroupCount + 1, sizeof(*rs) );
    GetTokenInformation( token, TokenUser, buf, sizeof(buf), &len );
    rs[n++].Sid = ((TOKEN_USER *)buf)->User.Sid;
    for (i = 0; i < groups->GroupCount; i++)
        if ((groups->Groups[i].Attributes & SE_GROUP_ENABLED) && !(groups->Groups[i].Attributes & SE_GROUP_INTEGRITY))
            rs[n++].Sid = groups->Groups[i].Sid;
    if (!CreateRestrictedToken( token, 0, 0, NULL, 0, NULL, n, rs, &restricted )) restricted = NULL;
    return restricted;
}

/* USER_LIMITED (sandbox/win/src/restricted_token_utils.cc): all groups but Users, Everyone and
 * Interactive deny-only; restricting SIDs Users, Everyone, RESTRICTED and the logon SID */
static HANDLE limited_token( HANDLE token )
{
    TOKEN_GROUPS *groups = token_groups( token );
    SID_AND_ATTRIBUTES deny[64], rs[4];
    PSID users, world, interactive, restricted_sid;
    DWORD i, nd = 0, nr = 0;
    HANDLE restricted = NULL;

    if (!groups) return NULL;
    ConvertStringSidToSidA( "S-1-5-32-545", &users );
    ConvertStringSidToSidA( "S-1-1-0", &world );
    ConvertStringSidToSidA( "S-1-5-4", &interactive );
    ConvertStringSidToSidA( "S-1-5-12", &restricted_sid );
    for (i = 0; i < groups->GroupCount && nd < 64; i++)
    {
        PSID s = groups->Groups[i].Sid;
        if (groups->Groups[i].Attributes & (SE_GROUP_INTEGRITY | SE_GROUP_LOGON_ID)) continue;
        if (EqualSid( s, users ) || EqualSid( s, world ) || EqualSid( s, interactive )) continue;
        deny[nd].Sid = s; deny[nd++].Attributes = 0;
    }
    rs[nr++].Sid = users;
    rs[nr++].Sid = world;
    rs[nr++].Sid = restricted_sid;
    if ((rs[nr].Sid = logon_sid( token ))) nr++;
    for (i = 0; i < nr; i++) rs[i].Attributes = 0;
    if (!CreateRestrictedToken( token, 0, nd, deny, 0, NULL, nr, rs, &restricted )) restricted = NULL;
    return restricted;
}

static void set_low( HANDLE token )
{
    TOKEN_MANDATORY_LABEL label;
    PSID low;
    ConvertStringSidToSidA( "S-1-16-4096", &low );
    label.Label.Sid = low;
    label.Label.Attributes = SE_GROUP_INTEGRITY;
    if (!SetTokenInformation( token, TokenIntegrityLevel, &label, sizeof(label) + GetLengthSid( low ) ))
        printf( "LOW fail %lu\n", GetLastError() );
}

static DWORD run_child( HANDLE token, const char *desktop, const char *args )
{
    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    char self[MAX_PATH], cmd[MAX_PATH + 64];
    DWORD code = 0xdeadbeef;

    GetModuleFileNameA( NULL, self, MAX_PATH );
    snprintf( cmd, sizeof(cmd), "\"%s\" %s", self, args );
    si.lpDesktop = (char *)desktop;
    if (!CreateProcessAsUserA( token, NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi ))
    {
        printf( "CHILD nostart %lu\n", GetLastError() );
        return ~0u;
    }
    if (WaitForSingleObject( pi.hProcess, 90000 ))
    {
        TerminateProcess( pi.hProcess, 99 );
        WaitForSingleObject( pi.hProcess, 5000 );
        code = 0x1000;  /* hung */
    }
    else GetExitCodeProcess( pi.hProcess, &code );
    CloseHandle( pi.hProcess );
    CloseHandle( pi.hThread );
    return code;
}

static int open_session( const char *session, const char *tag )
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
    int i, ret = 0;

    if (!open_winsta || !open_dir) { printf( "NOAPI\n" ); return 0; }
    swprintf( path, ARRAYSIZE(path), L"\\Sessions\\%hs\\Windows\\WindowStations", session );
    RtlInitUnicodeString( &str, path );
    InitializeObjectAttributes( &attr, &str, 0, NULL, NULL );
    if ((status = open_dir( &dir, DIRECTORY_TRAVERSE, &attr ))) { printf( "%sNODIR %#lx\n", tag, status ); return 0; }
    RtlInitUnicodeString( &name, L"WinSta0" );
    for (i = 0; i < 2; i++)
    {
        HWINSTA ws;
        InitializeObjectAttributes( &attr, &name, OBJ_CASE_INSENSITIVE, dir, NULL );
        SetLastError( 0 );
        ws = open_winsta( &attr, rights[i] );
        if (tag[0]) { if (ws) ret |= 1 << i; }
        else printf( "OPEN %s %d %lu\n", names[i], ws != NULL, ws ? 0 : GetLastError() );
        if (ws) CloseWindowStation( ws );
    }
    return ret;
}

int main( int argc, char **argv )
{
    DWORD session = 0xdeadbeef;
    HANDLE token, restricted;

    if (argc > 2 && !strcmp( argv[1], "child" )) return child( argv[2][0] == '-' ? "" : argv[2] );
    if (argc > 2 && !strcmp( argv[1], "childopen" )) return open_session( argv[2], "child" );

    setvbuf( stdout, NULL, _IONBF, 0 );  /* nothing lost when a gate's timeout kills it */
    ProcessIdToSessionId( GetCurrentProcessId(), &session );
    printf( "SESSION %lu\n", session );
    fflush( stdout );
    if (argc < 2) return 1;
    OpenProcessToken( GetCurrentProcess(), TOKEN_ALL_ACCESS, &token );

    if (!strcmp( argv[1], "info" ))
    {
        BYTE buf[256];
        DWORD len;
        if (argc > 2) freopen( argv[2], "w", stdout );  /* info FILE: an elevated run's report */
        GetTokenInformation( token, TokenUser, buf, sizeof(buf), &len );
        print_sid( "USER", ((TOKEN_USER *)buf)->User.Sid );
        print_sid( "LOGON", logon_sid( token ) );
        {
            TOKEN_ELEVATION_TYPE type = 0;
            GetTokenInformation( token, TokenElevationType, &type, sizeof(type), &len );
            printf( "ELEVATION %d\n", type );
        }
        print_sd( "WINSTA", GetProcessWindowStation() );
        print_sd( "DESKTOP", GetThreadDesktop( GetCurrentThreadId() ) );
        return 0;
    }
    if (!strcmp( argv[1], "hold" ))
    {
        HWND hwnd = NULL;
        int i;
        for (i = 0; i < 120 && !hwnd; i++)
            if (!(hwnd = CreateWindowA( "STATIC", "logondesk", WS_OVERLAPPEDWINDOW, 0, 0, 10, 10, NULL, NULL, NULL, NULL )))
                Sleep( 1000 );
        printf( "HELD %d\n", hwnd != NULL );
        fflush( stdout );
        Sleep( 600000 );
        return 0;
    }
    if (!strcmp( argv[1], "renderer" ) || !strcmp( argv[1], "limited-alt" ) || !strcmp( argv[1], "limited-alone" ))
    {
        int limited = strcmp( argv[1], "renderer" ) != 0, alone = !strcmp( argv[1], "limited-alone" );
        /* limited-alone: a desktop of its own, nobody else ever on it */
        const char *alt = alone ? "sgsbox1" : "sgsbox";
        char args[32];
        snprintf( args, sizeof(args), "child %s", alt );
        if (!make_alt_desktop( alt )) return 0;
        if (!(restricted = limited ? limited_token( token ) : renderer_token( token )))
        {
            printf( "CHILD notoken %lu\n", GetLastError() );
            return 0;
        }
        set_low( restricted );
        if (limited && !alone)
        {
            /* as in Chromium, a renderer is there first: in Wine a desktop's window is made by an
             * explorer started with its first user's token, and RESTRICTED may not make windows
             * there (on Windows the system makes it) */
            HANDLE first = renderer_token( token );
            if (first) { set_low( first ); printf( "FIRST %lu\n", run_child( first, "sgsbox", "child sgsbox" ) ); }
        }
        printf( "CHILD %lu\n", run_child( restricted, alt, args ) );
        return 0;
    }
    if (!strcmp( argv[1], "limited" ))
    {
        if (!(restricted = limited_token( token ))) { printf( "CHILD notoken %lu\n", GetLastError() ); return 0; }
        set_low( restricted );
        printf( "CHILD %lu\n", run_child( restricted, NULL, "child -" ) );
        return 0;
    }
    if (!strcmp( argv[1], "open" ) && argc > 2)
    {
        char args[64];
        open_session( argv[2], "" );
        if (!(restricted = limited_token( token ))) { printf( "LIMITED notoken\n" ); return 0; }
        snprintf( args, sizeof(args), "childopen %s", argv[2] );
        printf( "LIMITED OPEN %lu\n", run_child( restricted, NULL, args ) );
        return 0;
    }
    return 1;
}
