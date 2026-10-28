/* SHGetKnownFolderPath with the package/app-data redirection flags (patches/sg/6174): an unpackaged
 * program gets the folder as without them (Windows Terminal asks for its LocalAppData with
 * KF_FLAG_FORCE_APP_DATA_REDIRECTION, 0x80000); and the 64-bit program directories under their
 * ProgramW6432Dir/CommonW6432Dir names (wine.inf), which it reads to rebuild a shell's environment. */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <knownfolders.h>
#include <shlobj.h>
#include <stdio.h>

static void folder( DWORD flags )
{
    WCHAR *path = NULL, *plain = NULL;
    HRESULT hr = SHGetKnownFolderPath( &FOLDERID_LocalAppData, flags, NULL, &path );
    SHGetKnownFolderPath( &FOLDERID_LocalAppData, 0, NULL, &plain );
    printf( "flags %#lx: %08lx %d\n", flags, hr, path && plain && !lstrcmpiW( path, plain ) );
    CoTaskMemFree( path );
    CoTaskMemFree( plain );
}

static void value( const WCHAR *name, const WCHAR *expect )
{
    WCHAR buf[MAX_PATH];
    DWORD size = sizeof(buf);
    LONG ret = RegGetValueW( HKEY_LOCAL_MACHINE, L"Software\\Microsoft\\Windows\\CurrentVersion", name,
                             RRF_RT_REG_SZ | RRF_SUBKEY_WOW6464KEY, NULL, buf, &size );
    printf( "%ls: %ld %d\n", name, ret, !ret && !lstrcmpiW( buf, expect ) );
}

int main(void)
{
    folder( 0x80000 );   /* KF_FLAG_FORCE_APP_DATA_REDIRECTION */
    folder( 0x40000 );   /* KF_FLAG_RETURN_FILTER_REDIRECTION_TARGET */
    folder( 0x20000 );   /* KF_FLAG_FORCE_PACKAGE_REDIRECTION */
    folder( 0x10000 );   /* KF_FLAG_NO_PACKAGE_REDIRECTION */
    value( L"ProgramW6432Dir", L"C:\\Program Files" );
    value( L"CommonW6432Dir", L"C:\\Program Files\\Common Files" );
    return 0;
}
