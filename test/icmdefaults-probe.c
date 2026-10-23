/* icmdefaults-probe: WCS default profiles and rendering intents (mscms) and the color
 * management dialog SetupColorMatchingW (icmui), Stained Glass OS wine-sg 6506. Prints
 * name=value lines; see test/icmdefaults-gate.sh.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include <windows.h>
#include <icm.h>
#include <stdio.h>

#define DEV L"\\\\.\\DISPLAY9"
static const char *wine_dbgstr_w( const WCHAR *s ) { static char buf[4][300]; static int n; char *b = buf[n++ % 4]; WideCharToMultiByte( CP_ACP, 0, s, -1, b, 300, NULL, NULL ); return b; }
static int applied;

static BOOL WINAPI apply( COLORMATCHSETUPW *cms, LPARAM lparam ) { applied++; return TRUE; }

/* the dialog's hook: color management off, intent "Graphics", Apply, OK */
static INT_PTR CALLBACK hook( HWND dialog, UINT msg, WPARAM wparam, LPARAM lparam )
{
    if (msg == WM_INITDIALOG)
    {
        int n = SendDlgItemMessageW( dialog, 1002, CB_GETCOUNT, 0, 0 );
        printf( "dialog_monitor_list=%d %d profiles\n", n > 0, n );
        CheckDlgButton( dialog, 1001, BST_UNCHECKED );
        SendDlgItemMessageW( dialog, 1004, CB_SETCURSEL, INTENT_SATURATION, 0 );
        PostMessageW( dialog, WM_COMMAND, 1008 /* Apply */, 0 );
        PostMessageW( dialog, WM_COMMAND, IDOK, 0 );
    }
    return FALSE;
}

int main( void )
{
    WCHAR name[MAX_PATH], first[MAX_PATH] = L"", *p;
    ENUMTYPEW record = { sizeof(record), ENUM_TYPE_VERSION, 0 };
    DWORD intent = 99, size = 0, count = 0;
    BOOL ret, per_user = 7;
    BYTE buffer[8192];

    /* rendering intents */
    ret = WcsGetDefaultRenderingIntent( WCS_PROFILE_MANAGEMENT_SCOPE_CURRENT_USER, &intent );
    printf( "intent_default=%d %d %lu\n", ret && intent == INTENT_PERCEPTUAL, ret, intent );
    WcsSetDefaultRenderingIntent( WCS_PROFILE_MANAGEMENT_SCOPE_SYSTEM_WIDE, INTENT_RELATIVE_COLORIMETRIC );
    WcsGetDefaultRenderingIntent( WCS_PROFILE_MANAGEMENT_SCOPE_CURRENT_USER, &intent );
    printf( "intent_user_from_system=%d %lu\n", intent == INTENT_RELATIVE_COLORIMETRIC, intent );
    ret = WcsSetDefaultRenderingIntent( WCS_PROFILE_MANAGEMENT_SCOPE_CURRENT_USER, INTENT_SATURATION );
    WcsGetDefaultRenderingIntent( WCS_PROFILE_MANAGEMENT_SCOPE_CURRENT_USER, &intent );
    printf( "intent_user=%d %d %lu\n", ret && intent == INTENT_SATURATION, ret, intent );
    WcsGetDefaultRenderingIntent( WCS_PROFILE_MANAGEMENT_SCOPE_SYSTEM_WIDE, &intent );
    printf( "intent_system=%d %lu\n", intent == INTENT_RELATIVE_COLORIMETRIC, intent );
    SetLastError( 0 );
    ret = WcsSetDefaultRenderingIntent( WCS_PROFILE_MANAGEMENT_SCOPE_CURRENT_USER, 9 );
    printf( "intent_bad=%d %lu\n", !ret && GetLastError() == ERROR_INVALID_PARAMETER, GetLastError() );

    /* a profile of our own installed, to tell from sRGB */
    {
        WCHAR dir[MAX_PATH], src[MAX_PATH], tmp[MAX_PATH];
        DWORD len = sizeof(dir);
        GetColorDirectoryW( NULL, dir, &len );
        wsprintfW( src, L"%s\\sRGB Color Space Profile.icm", dir );
        GetTempPathW( MAX_PATH, tmp );
        lstrcatW( tmp, L"probe display.icm" );
        CopyFileW( src, tmp, FALSE );
        printf( "installed=%d\n", InstallColorProfileW( NULL, tmp ) );
    }

    /* an installed profile to use */
    ret = WcsEnumColorProfiles( WCS_PROFILE_MANAGEMENT_SCOPE_SYSTEM_WIDE, &record, buffer, sizeof(buffer), &count );
    printf( "enum=%d %d count %lu\n", ret && count > 1, ret, count );
    lstrcpyW( first, L"probe display.icm" );
    for (p = (WCHAR *)buffer; ret && *p; p += lstrlenW( p ) + 1)
        if (!lstrcmpiW( p, first )) break;
    printf( "enum_own=%d\n", ret && *p );

    /* default profiles */
    ret = WcsGetDefaultColorProfileSize( WCS_PROFILE_MANAGEMENT_SCOPE_CURRENT_USER, DEV, CPT_ICC, CPST_NONE, 0, &size );
    ret = ret && WcsGetDefaultColorProfile( WCS_PROFILE_MANAGEMENT_SCOPE_CURRENT_USER, DEV, CPT_ICC, CPST_NONE, 0, size, name );
    printf( "default_srgb=%d %s\n", ret && !lstrcmpiW( name, L"sRGB Color Space Profile.icm" ) &&
            size == (lstrlenW( name ) + 1) * sizeof(WCHAR), wine_dbgstr_w( name ) );
    SetLastError( 0 );
    ret = WcsGetDefaultColorProfile( WCS_PROFILE_MANAGEMENT_SCOPE_CURRENT_USER, DEV, CPT_ICC, CPST_NONE, 0, 4, name );
    printf( "default_small=%d %lu\n", !ret && GetLastError() == ERROR_INSUFFICIENT_BUFFER, GetLastError() );

    ret = WcsAssociateColorProfileWithDevice( WCS_PROFILE_MANAGEMENT_SCOPE_SYSTEM_WIDE, first, DEV );
    WcsGetDefaultColorProfile( WCS_PROFILE_MANAGEMENT_SCOPE_SYSTEM_WIDE, DEV, CPT_ICC, CPST_NONE, 0, sizeof(name), name );
    printf( "associated=%d %d %s\n", ret && !lstrcmpiW( name, first ), ret, wine_dbgstr_w( name ) );
    SetLastError( 0 );
    ret = WcsAssociateColorProfileWithDevice( WCS_PROFILE_MANAGEMENT_SCOPE_SYSTEM_WIDE, L"no such profile.icm", DEV );
    printf( "associate_missing=%d %lu\n", !ret && GetLastError() == ERROR_FILE_NOT_FOUND, GetLastError() );

    /* the user's own list counts once the device uses per-user profiles */
    ret = WcsSetDefaultColorProfile( WCS_PROFILE_MANAGEMENT_SCOPE_CURRENT_USER, DEV, CPT_ICC, CPST_NONE, 0, first );
    WcsGetUsePerUserProfiles( DEV, CLASS_MONITOR, &per_user );
    printf( "per_user_off=%d %d %d\n", ret && per_user == FALSE, ret, per_user );
    WcsSetUsePerUserProfiles( DEV, CLASS_MONITOR, TRUE );
    WcsGetUsePerUserProfiles( DEV, CLASS_MONITOR, &per_user );
    WcsGetDefaultColorProfile( WCS_PROFILE_MANAGEMENT_SCOPE_CURRENT_USER, DEV, CPT_ICC, CPST_NONE, 0, sizeof(name), name );
    printf( "per_user_on=%d %d %s\n", per_user == TRUE && !lstrcmpiW( name, first ), per_user, wine_dbgstr_w( name ) );

    ret = WcsDisassociateColorProfileFromDevice( WCS_PROFILE_MANAGEMENT_SCOPE_SYSTEM_WIDE, first, DEV );
    WcsGetDefaultColorProfile( WCS_PROFILE_MANAGEMENT_SCOPE_SYSTEM_WIDE, DEV, CPT_ICC, CPST_NONE, 0, sizeof(name), name );
    printf( "disassociated=%d %d %s\n", ret && !lstrcmpiW( name, L"sRGB Color Space Profile.icm" ), ret, wine_dbgstr_w( name ) );
    SetLastError( 0 );
    ret = WcsDisassociateColorProfileFromDevice( WCS_PROFILE_MANAGEMENT_SCOPE_SYSTEM_WIDE, first, DEV );
    printf( "disassociate_again=%d %lu\n", !ret && GetLastError() == ERROR_FILE_NOT_FOUND, GetLastError() );

    /* the dialog */
    {
        COLORMATCHSETUPW cms;
        WCHAR monitor[MAX_PATH] = L"", target[MAX_PATH] = L"", small[2];

        memset( &cms, 0, sizeof(cms) );
        SetLastError( 0xdeadbeef );
        ret = SetupColorMatchingW( &cms );
        printf( "dialog_invalid=%d %lu\n", !ret && GetLastError() == ERROR_INVALID_PARAMETER, GetLastError() );

        cms.dwSize = sizeof(cms);
        cms.dwVersion = COLOR_MATCH_VERSION;
        cms.dwFlags = CMS_USEHOOK | CMS_USEAPPLYCALLBACK | CMS_ENABLEPROOFING;
        cms.pSourceName = L"probe";
        cms.pMonitorProfile = monitor;
        cms.ccMonitorProfile = MAX_PATH;
        cms.pTargetProfile = target;
        cms.ccTargetProfile = MAX_PATH;
        cms.lpfnHook = hook;
        cms.lpfnApplyCallback = apply;
        ret = SetupColorMatchingW( &cms );
        printf( "dialog_ok=%d %d flags %#lx intent %lu monitor %s applied %d\n", ret && (cms.dwFlags & CMS_DISABLEICM) &&
                (cms.dwFlags & CMS_ENABLEPROOFING) && cms.dwRenderIntent == INTENT_SATURATION && *monitor && applied == 1,
                ret, cms.dwFlags, cms.dwRenderIntent, wine_dbgstr_w( monitor ), applied );

        cms.dwFlags = CMS_USEHOOK;
        cms.pMonitorProfile = small;
        cms.ccMonitorProfile = ARRAYSIZE(small);
        SetLastError( 0 );
        ret = SetupColorMatchingW( &cms );
        printf( "dialog_overflow=%d %d flags %#lx err %lu\n", !ret && (cms.dwFlags & CMS_MONITOROVERFLOW) &&
                GetLastError() == ERROR_INSUFFICIENT_BUFFER, ret, cms.dwFlags, GetLastError() );
    }
    printf( "done=1\n" );
    return 0;
}
