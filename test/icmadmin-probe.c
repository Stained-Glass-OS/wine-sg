/* icmadmin-probe: the system's colour profile store is for administrators (mscms, Stained Glass OS
 * wine-sg 6078). Runs each machine-wide change as the administrator it is and then impersonating a copy of
 * its own token with the Administrators group made deny-only. Prints name=value lines; see
 * test/icmadmin-gate.sh.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include <windows.h>
#include <icm.h>
#include <stdio.h>

#define DEV L"\\\\.\\DISPLAY7"

static WCHAR dir[MAX_PATH], src[MAX_PATH], copy1[MAX_PATH], copy2[MAX_PATH];

/* each machine-wide change: its result and error */
static void changes( const char *who, const WCHAR *profile, const WCHAR *installed_name )
{
    BOOL ret;

    SetLastError( 0 );
    ret = InstallColorProfileW( NULL, profile );
    printf( "%s_install=%d %lu\n", who, ret, ret ? 0 : GetLastError() );
    SetLastError( 0 );
    ret = WcsAssociateColorProfileWithDevice( WCS_PROFILE_MANAGEMENT_SCOPE_SYSTEM_WIDE,
                                              L"sRGB Color Space Profile.icm", DEV );
    printf( "%s_associate_system=%d %lu\n", who, ret, ret ? 0 : GetLastError() );
    SetLastError( 0 );
    ret = WcsSetDefaultRenderingIntent( WCS_PROFILE_MANAGEMENT_SCOPE_SYSTEM_WIDE, INTENT_SATURATION );
    printf( "%s_intent_system=%d %lu\n", who, ret, ret ? 0 : GetLastError() );
    SetLastError( 0 );
    ret = AssociateColorProfileWithDeviceW( NULL, src, DEV );
    printf( "%s_associate_legacy=%d %lu\n", who, ret, ret ? 0 : GetLastError() );
    SetLastError( 0 );
    ret = UninstallColorProfileW( NULL, installed_name, TRUE );
    printf( "%s_uninstall=%d %lu\n", who, ret, ret ? 0 : GetLastError() );
    /* the user's own settings stay open */
    SetLastError( 0 );
    ret = WcsSetDefaultRenderingIntent( WCS_PROFILE_MANAGEMENT_SCOPE_CURRENT_USER, INTENT_PERCEPTUAL );
    printf( "%s_intent_user=%d %lu\n", who, ret, ret ? 0 : GetLastError() );
    SetLastError( 0 );
    ret = WcsAssociateColorProfileWithDevice( WCS_PROFILE_MANAGEMENT_SCOPE_CURRENT_USER,
                                              L"sRGB Color Space Profile.icm", DEV );
    printf( "%s_associate_user=%d %lu\n", who, ret, ret ? 0 : GetLastError() );
}

int main( void )
{
    SID_IDENTIFIER_AUTHORITY nt = { SECURITY_NT_AUTHORITY };
    SID_AND_ATTRIBUTES disable = { 0 };
    HANDLE token, restricted = NULL;
    WCHAR tmp[MAX_PATH], name1[MAX_PATH], name2[MAX_PATH];
    DWORD len = sizeof(dir);
    PSID admins;
    BOOL member = FALSE;

    GetColorDirectoryW( NULL, dir, &len );
    wsprintfW( src, L"%s\\sRGB Color Space Profile.icm", dir );
    GetTempPathW( MAX_PATH, tmp );
    wsprintfW( copy1, L"%sprobe admin.icm", tmp );
    wsprintfW( copy2, L"%sprobe user.icm", tmp );
    CopyFileW( src, copy1, FALSE );
    CopyFileW( src, copy2, FALSE );
    wsprintfW( name1, L"%s\\probe admin.icm", dir );
    wsprintfW( name2, L"%s\\probe user.icm", dir );

    AllocateAndInitializeSid( &nt, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &admins );
    CheckTokenMembership( NULL, admins, &member );
    printf( "admin=%d\n", member );

    changes( "admin", copy1, name1 );
    printf( "admin_file_gone=%d\n", GetFileAttributesW( name1 ) == INVALID_FILE_ATTRIBUTES );

    /* the same as a standard user: the Administrators group deny-only */
    OpenProcessToken( GetCurrentProcess(), TOKEN_ALL_ACCESS, &token );
    disable.Sid = admins;
    if (!CreateRestrictedToken( token, 0, 1, &disable, 0, NULL, 0, NULL, &restricted ) ||
        !ImpersonateLoggedOnUser( restricted ))
    {
        printf( "restricted=0 %lu\n", GetLastError() );
        return 1;
    }
    member = TRUE;
    CheckTokenMembership( NULL, admins, &member );
    printf( "restricted=%d\n", !member );
    /* a profile an administrator put there, for the standard user's uninstall */
    RevertToSelf();
    InstallColorProfileW( NULL, copy2 );
    ImpersonateLoggedOnUser( restricted );
    changes( "user", copy2, name2 );
    RevertToSelf();
    printf( "user_file_kept=%d\n", GetFileAttributesW( name2 ) != INVALID_FILE_ATTRIBUTES );
    UninstallColorProfileW( NULL, name2, TRUE );
    printf( "done=1\n" );
    return 0;
}
