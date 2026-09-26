/* hkcr-probe: HKEY_CLASSES_ROOT is HKCU\Software\Classes over
 * HKLM\Software\Classes, and the shell honours the user's choices
 * (patches/sg/0178).
 *
 *   hkcr-probe machine          register the machine's classes (an administrator)
 *   hkcr-probe user             register this user's classes and choices, then check
 *   hkcr-probe observe          what another user sees of them (nothing)
 *   hkcr-probe mark NAME FILE   a handler: records NAME for this user in C:\sgmark
 *
 * Prints name=value lines; see test/hkcr-gate.sh.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#define COBJMACROS
#include <windows.h>
#include <shlwapi.h>
#include <shlobj.h>
#include <shellapi.h>
#include <stdio.h>

static WCHAR self[MAX_PATH];

static void set_sz( HKEY root, const WCHAR *path, const WCHAR *name, const WCHAR *value )
{
    HKEY key;
    if (RegCreateKeyExW( root, path, 0, NULL, 0, KEY_ALL_ACCESS, NULL, &key, NULL ))
    {
        printf( "create_failed=%ls\n", path );
        return;
    }
    RegSetValueExW( key, name, 0, REG_SZ, (const BYTE *)value, (lstrlenW( value ) + 1) * sizeof(WCHAR) );
    RegCloseKey( key );
}

static void register_handler( HKEY root, const WCHAR *progid, const WCHAR *mark )
{
    WCHAR path[200], cmd[MAX_PATH + 100];
    swprintf( path, 200, L"Software\\Classes\\%ls\\shell\\open\\command", progid );
    swprintf( cmd, MAX_PATH + 100, L"\"%ls\" mark %ls \"%%1\"", self, mark );
    set_sz( root, path, NULL, cmd );
}

static void query( const char *label, const WCHAR *path )
{
    WCHAR buf[200] = L"";
    LONG len = sizeof(buf);
    LONG ret = RegQueryValueW( HKEY_CLASSES_ROOT, path, buf, &len );
    printf( "%s=%ls\n", label, ret ? L"(none)" : buf );
}

static int has_subkey( HKEY key, const WCHAR *name )
{
    WCHAR sub[100];
    DWORD i, len;
    for (i = 0; len = ARRAYSIZE(sub), !RegEnumKeyExW( key, i, sub, &len, NULL, NULL, NULL, NULL ); i++)
        if (!lstrcmpiW( sub, name )) return 1;
    return 0;
}

static void assoc_command( const char *label, const WCHAR *assoc )
{
    WCHAR buf[MAX_PATH + 100] = L"";
    DWORD len = ARRAYSIZE(buf);
    HRESULT hr = AssocQueryStringW( 0, ASSOCSTR_COMMAND, assoc, L"open", buf, &len );
    WCHAR *mark = wcsstr( buf, L" mark " );
    printf( "%s=%ls\n", label, FAILED(hr) ? L"(none)" : mark ? mark + 6 : buf );
}

/* ShellExecute, then which handler recorded itself */
static void run_and_see( const char *label, const WCHAR *file, const WCHAR *expect_names )
{
    WCHAR user[64], path[MAX_PATH];
    DWORD len = ARRAYSIZE(user), i;
    const WCHAR *names[] = { L"machine", L"user", L"choice", L"proto", L"protochoice" };
    HINSTANCE ret;

    GetUserNameW( user, &len );
    for (i = 0; i < ARRAYSIZE(names); i++)
    {
        swprintf( path, MAX_PATH, L"C:\\sgmark\\%ls.%ls", names[i], user );
        DeleteFileW( path );
    }
    ret = ShellExecuteW( NULL, NULL, file, NULL, NULL, SW_HIDE );
    for (i = 0; i < 100; i++)
    {
        DWORD j;
        for (j = 0; j < ARRAYSIZE(names); j++)
        {
            swprintf( path, MAX_PATH, L"C:\\sgmark\\%ls.%ls", names[j], user );
            if (GetFileAttributesW( path ) != INVALID_FILE_ATTRIBUTES)
            {
                printf( "%s=%ls\n", label, names[j] );
                return;
            }
        }
        Sleep( 100 );
    }
    printf( "%s=(nothing, %Iu)\n", label, (ULONG_PTR)ret );
}

static void check_view( void )
{
    HKEY key, sub;
    DWORD subkeys = 0;
    WCHAR *choice = NULL;
    IApplicationAssociationRegistration *aar;

    query( "ext_class", L".sgtest" );                           /* the user's */
    printf( "machine_subkey=%d\n", !RegOpenKeyExW( HKEY_CLASSES_ROOT, L".sgtest\\ShellNew", 0, KEY_READ, &key ) );
    if (!RegOpenKeyExW( HKEY_CLASSES_ROOT, L".sgtest", 0, KEY_READ, &key ))
    {
        printf( "relative_subkey=%d\n", !RegOpenKeyExW( key, L"ShellNew", 0, KEY_READ, &sub ) );
        RegQueryInfoKeyW( key, NULL, NULL, NULL, &subkeys, NULL, NULL, NULL, NULL, NULL, NULL, NULL );
        printf( "enum_both=%d%d count=%lu\n", has_subkey( key, L"ShellNew" ), has_subkey( key, L"OpenWithProgids" ),
                subkeys );
        RegCloseKey( key );
    }
    query( "user_progid", L"SgUser.File\\shell\\open\\command" );
    printf( "root_has_user_key=%d\n", has_subkey( HKEY_CLASSES_ROOT, L".sguseronly" ) );
    printf( "root_has_machine_key=%d\n", has_subkey( HKEY_CLASSES_ROOT, L".sgtest" ) );
    assoc_command( "assoc_nochoice", L".sgnochoice" );
    assoc_command( "assoc_choice", L".sgtest" );
    assoc_command( "assoc_protocol", L"sgproto" );

    CoInitialize( NULL );
    if (SUCCEEDED( CoCreateInstance( &CLSID_ApplicationAssociationRegistration, NULL, CLSCTX_INPROC_SERVER,
                                     &IID_IApplicationAssociationRegistration, (void **)&aar ) ))
    {
        if (SUCCEEDED( IApplicationAssociationRegistration_QueryCurrentDefault( aar, L".sgtest", AT_FILEEXTENSION,
                                                                                AL_EFFECTIVE, &choice ) ))
            printf( "current_default=%ls\n", choice );
        IApplicationAssociationRegistration_Release( aar );
    }
    run_and_see( "open_nochoice", L"C:\\sgmark\\doc.sgnochoice", NULL );
    run_and_see( "open_choice", L"C:\\sgmark\\doc.sgtest", NULL );
    run_and_see( "open_protocol", L"sgproto:hello", NULL );
}

int wmain( int argc, WCHAR **argv )
{
    HKEY key;

    GetModuleFileNameW( NULL, self, MAX_PATH );
    CreateDirectoryW( L"C:\\sgmark", NULL );

    if (argc >= 3 && !lstrcmpW( argv[1], L"mark" ))
    {
        WCHAR user[64], path[MAX_PATH];
        DWORD len = ARRAYSIZE(user);
        HANDLE file;
        GetUserNameW( user, &len );
        swprintf( path, MAX_PATH, L"C:\\sgmark\\%ls.%ls", argv[2], user );
        file = CreateFileW( path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL );
        CloseHandle( file );
        return 0;
    }
    if (argc >= 2 && !lstrcmpW( argv[1], L"machine" ))
    {
        /* the machine's: .sgtest and .sgnochoice are SgMachine.File, with ShellNew */
        set_sz( HKEY_LOCAL_MACHINE, L"Software\\Classes\\.sgtest", NULL, L"SgMachine.File" );
        set_sz( HKEY_LOCAL_MACHINE, L"Software\\Classes\\.sgtest\\ShellNew", L"NullFile", L"" );
        set_sz( HKEY_LOCAL_MACHINE, L"Software\\Classes\\.sgnochoice", NULL, L"SgMachine.File" );
        register_handler( HKEY_LOCAL_MACHINE, L"SgMachine.File", L"machine" );
        register_handler( HKEY_LOCAL_MACHINE, L"SgChoice.File", L"choice" );
        set_sz( HKEY_LOCAL_MACHINE, L"Software\\Classes\\sgproto", L"URL Protocol", L"" );
        register_handler( HKEY_LOCAL_MACHINE, L"sgproto", L"proto" );
        register_handler( HKEY_LOCAL_MACHINE, L"SgProto.Handler", L"protochoice" );
        CloseHandle( CreateFileW( L"C:\\sgmark\\doc.sgtest", GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL ) );
        CloseHandle( CreateFileW( L"C:\\sgmark\\doc.sgnochoice", GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL ) );
        /* a new key through HKCR goes to the machine's classes (an administrator) */
        if (!RegCreateKeyExW( HKEY_CLASSES_ROOT, L"SgNew.ByAdmin", 0, NULL, 0, KEY_ALL_ACCESS, NULL, &key, NULL ))
            RegCloseKey( key );
        printf( "admin_create_machine=%d\n",
                !RegOpenKeyExW( HKEY_LOCAL_MACHINE, L"Software\\Classes\\SgNew.ByAdmin", 0, KEY_READ, &key ) );
        printf( "admin_create_not_user=%d\n",
                !!RegOpenKeyExW( HKEY_CURRENT_USER, L"Software\\Classes\\SgNew.ByAdmin", 0, KEY_READ, &key ) );
        return 0;
    }
    if (argc >= 2 && !lstrcmpW( argv[1], L"user" ))
    {
        /* this user's: .sgtest is SgUser.File for them, a user-only type, and choices */
        set_sz( HKEY_CURRENT_USER, L"Software\\Classes\\.sgtest", NULL, L"SgUser.File" );
        set_sz( HKEY_CURRENT_USER, L"Software\\Classes\\.sgtest\\OpenWithProgids", L"SgUser.File", L"" );
        set_sz( HKEY_CURRENT_USER, L"Software\\Classes\\.sguseronly", NULL, L"SgUser.File" );
        register_handler( HKEY_CURRENT_USER, L"SgUser.File", L"user" );
        set_sz( HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts\\.sgtest\\UserChoice",
                L"ProgId", L"SgChoice.File" );
        set_sz( HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\Shell\\Associations\\UrlAssociations\\sgproto\\UserChoice",
                L"ProgId", L"SgProto.Handler" );
        /* a value written through HKCR to a key the user has goes to the user's */
        if (!RegOpenKeyExW( HKEY_CLASSES_ROOT, L".sgtest", 0, KEY_ALL_ACCESS, &key ))
        {
            RegSetValueExW( key, L"SgWritten", 0, REG_SZ, (const BYTE *)L"1", 4 );
            RegCloseKey( key );
        }
        printf( "write_went_user=%d\n", !RegGetValueW( HKEY_CURRENT_USER, L"Software\\Classes\\.sgtest", L"SgWritten",
                                                       RRF_RT_REG_SZ, NULL, NULL, NULL ) );
        /* a key a user cannot create in the machine's classes is theirs */
        if (!RegCreateKeyExW( HKEY_CLASSES_ROOT, L"SgNew.ByUser", 0, NULL, 0, KEY_ALL_ACCESS, NULL, &key, NULL ))
            RegCloseKey( key );
        printf( "create_somewhere=%d\n", !RegOpenKeyExW( HKEY_CLASSES_ROOT, L"SgNew.ByUser", 0, KEY_READ, &key ) );
        check_view();
        return 0;
    }
    if (argc >= 2 && !lstrcmpW( argv[1], L"observe" ))
    {
        query( "other_ext_class", L".sgtest" );
        printf( "other_sees_user_key=%d\n", has_subkey( HKEY_CLASSES_ROOT, L".sguseronly" ) );
        assoc_command( "other_assoc", L".sgtest" );
        return 0;
    }
    return 2;
}
