/* The application of test/msixapps-gate.sh's package: records how it was
 * started -- its arguments after the program name, its package family name
 * (the identity a process inside a package's location has) and its working
 * directory -- in C:\msixapps-out.txt, then exits.
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#include <windows.h>
#include <stdio.h>

typedef LONG (WINAPI *family_func)( UINT32 *, WCHAR * );

int main( void )
{
    WCHAR *cmd = GetCommandLineW(), family[256] = L"(none)", dir[MAX_PATH];
    UINT32 len = ARRAYSIZE(family);
    family_func get_family = (family_func)GetProcAddress( GetModuleHandleA( "kernel32" ), "GetCurrentPackageFamilyName" );
    BOOL quoted = FALSE;
    FILE *f;

    /* the arguments: what follows the program's name */
    for (; *cmd && (quoted || (*cmd != ' ' && *cmd != '\t')); cmd++) if (*cmd == '"') quoted = !quoted;
    while (*cmd == ' ' || *cmd == '\t') cmd++;
    if (!get_family || get_family( &len, family )) wcscpy( family, L"(none)" );
    GetCurrentDirectoryW( MAX_PATH, dir );
    if (!(f = _wfopen( L"C:\\msixapps-out.txt", L"w" ))) return 1;
    fwprintf( f, L"args=%ls\nfamily=%ls\ndir=%ls\n", cmd, family, dir );
    fclose( f );
    return 0;
}
