/* shellfolders-probe: the Desktop folder as a standard user resolves it
 * (patches/sg/0070), for test/shellfolders-gate.sh.
 *
 *   shellfolders-probe run     make the machine's profile list read-only for
 *                              users (as the shared system prefix has it: only
 *                              administrators may write HKLM), clear the
 *                              "Shell Folders" cache's Desktop, then run
 *                              "child" with Administrators disabled
 *   shellfolders-probe child   can this token write HKLM? SHGetFolderPath's
 *                              Desktop, and what the cache holds afterwards
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include <windows.h>
#include <sddl.h>
#include <aclapi.h>
#include <shlobj.h>
#include <stdio.h>

static const WCHAR shell_folders[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Shell Folders";

int main( int argc, char **argv )
{
    if (argc == 2 && !strcmp( argv[1], "child" ))
    {
        WCHAR path[MAX_PATH] = L"", cached[MAX_PATH] = L"";
        DWORD size = sizeof(cached);
        HKEY key;
        LONG w = RegCreateKeyExW( HKEY_LOCAL_MACHINE, L"Software\\Microsoft\\Windows NT\\CurrentVersion\\ProfileList",
                                  0, NULL, 0, KEY_ALL_ACCESS, NULL, &key, NULL );
        HRESULT hr;

        if (!w) RegCloseKey( key );
        hr = SHGetFolderPathW( NULL, CSIDL_DESKTOPDIRECTORY, NULL, SHGFP_TYPE_CURRENT, path );
        RegGetValueW( HKEY_CURRENT_USER, shell_folders, L"Desktop", RRF_RT_REG_SZ, NULL, cached, &size );
        printf( "hklm_write=%ld\nhr=%#lx\npath=%ls\ncached=%ls\n", w, hr, path, cached );
        return 0;
    }
    if (argc == 2 && !strcmp( argv[1], "run" ))
    {
        SID_IDENTIFIER_AUTHORITY nt = { SECURITY_NT_AUTHORITY };
        SID_AND_ATTRIBUTES disable = { 0 };
        STARTUPINFOW si = { sizeof(si) };
        PROCESS_INFORMATION pi;
        WCHAR cmd[MAX_PATH + 16], exe[MAX_PATH];
        HANDLE token, restricted;
        HKEY key;

        PSECURITY_DESCRIPTOR sd;

        if (!ConvertStringSecurityDescriptorToSecurityDescriptorW( L"D:P(A;;KA;;;BA)(A;;KA;;;SY)(A;;KR;;;WD)",
                                                                   SDDL_REVISION_1, &sd, NULL ) ||
            RegCreateKeyExW( HKEY_LOCAL_MACHINE, L"Software\\Microsoft\\Windows NT\\CurrentVersion\\ProfileList",
                             0, NULL, 0, KEY_ALL_ACCESS, NULL, &key, NULL ) ||
            RegSetKeySecurity( key, DACL_SECURITY_INFORMATION, sd ))
        {
            printf( "could not protect the profile list\n" );
            return 1;
        }
        RegCloseKey( key );
        if (!RegOpenKeyExW( HKEY_CURRENT_USER, shell_folders, 0, KEY_SET_VALUE, &key ))
        {
            RegDeleteValueW( key, L"Desktop" );
            RegCloseKey( key );
        }
        AllocateAndInitializeSid( &nt, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &disable.Sid );
        OpenProcessToken( GetCurrentProcess(), TOKEN_ALL_ACCESS, &token );
        if (!CreateRestrictedToken( token, 0, 1, &disable, 0, NULL, 0, NULL, &restricted ))
        {
            printf( "restricted token failed %lu\n", GetLastError() );
            return 1;
        }
        GetModuleFileNameW( NULL, exe, MAX_PATH );
        swprintf( cmd, ARRAYSIZE(cmd), L"\"%ls\" child", exe );
        if (!CreateProcessAsUserW( restricted, NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi ))
        {
            printf( "child failed %lu\n", GetLastError() );
            return 1;
        }
        WaitForSingleObject( pi.hProcess, 60000 );
        return 0;
    }
    return 2;
}
