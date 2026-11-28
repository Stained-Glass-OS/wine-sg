/* A folder shared on the network (patches/sg/9636, i0978): NetShareAdd / NetShareDel ask
 * sg-admind (the gate's stand-in), NetShareEnum / NetShareGetInfo read the shares file. */
#include <windows.h>
#include <lm.h>
#include <lmshare.h>
#include <stdio.h>

typedef void (WINAPI *rundll_fn)( HWND, HINSTANCE, LPCWSTR, int );

static NET_API_STATUS add( const WCHAR *name, const WCHAR *path, const WCHAR *remark, DWORD max, DWORD mask )
{
    SHARE_INFO_502 info = {0};
    SECURITY_DESCRIPTOR sd;
    BYTE acl_buf[256], world[SECURITY_MAX_SID_SIZE];
    ACL *acl = (ACL *)acl_buf;
    DWORD len = sizeof(world), err = 0;

    CreateWellKnownSid( WinWorldSid, NULL, world, &len );
    InitializeAcl( acl, sizeof(acl_buf), ACL_REVISION );
    AddAccessAllowedAce( acl, ACL_REVISION, mask, world );
    InitializeSecurityDescriptor( &sd, SECURITY_DESCRIPTOR_REVISION );
    SetSecurityDescriptorDacl( &sd, TRUE, acl, FALSE );
    info.shi502_netname = (WCHAR *)name;
    info.shi502_type = STYPE_DISKTREE;
    info.shi502_remark = (WCHAR *)remark;
    info.shi502_max_uses = max;
    info.shi502_path = (WCHAR *)path;
    info.shi502_security_descriptor = &sd;
    return NetShareAdd( NULL, 502, (BYTE *)&info, &err );
}

int main( int argc, char **argv )
{
    WCHAR dir[MAX_PATH];
    SHARE_INFO_2 *info = NULL;
    DWORD read = 0, total = 0, i;

    GetCurrentDirectoryW( MAX_PATH, dir );
    lstrcatW( dir, L"\\shared folder" );
    CreateDirectoryW( dir, NULL );
    printf( "ADD %lu\n", add( L"Projects", dir, L"Team files", 3, 0x1301bf ) );
    printf( "ADDREAD %lu\n", add( L"Docs", dir, L"", ~0u, 0x1200a9 ) );
    printf( "DUP %lu\n", add( L"projects", dir, L"", ~0u, 0x1200a9 ) );
    printf( "OFF %lu\n", add( L"Off", dir, L"", ~0u, 0x1200a9 ) );
    printf( "ENUM %lu\n", NetShareEnum( NULL, 2, (BYTE **)&info, MAX_PREFERRED_LENGTH, &read, &total, NULL ) );
    for (i = 0; i < read; i++)
        printf( "SHARE %ls|%ls|%ls|%ld|%s\n", info[i].shi2_netname, info[i].shi2_path, info[i].shi2_remark,
                (long)info[i].shi2_max_uses, (info[i].shi2_permissions & ACCESS_WRITE) ? "rw" : "ro" );
    NetApiBufferFree( info );
    info = NULL;
    printf( "GETINFO %lu\n", NetShareGetInfo( NULL, (WCHAR *)L"PROJECTS", 2, (BYTE **)&info ) );
    if (info) { printf( "GOT %ls\n", info->shi2_netname ); NetApiBufferFree( info ); }
    printf( "DEL %lu\n", NetShareDel( NULL, (WCHAR *)L"Projects", 0 ) );
    printf( "DEL2 %lu\n", NetShareDel( NULL, (WCHAR *)L"Projects", 0 ) );
    printf( "RUNDLL %d\n", GetProcAddress( LoadLibraryW( L"shell32.dll" ), "SgShareFolderW" ) != NULL );
    fflush( stdout );
    return 0;
}
