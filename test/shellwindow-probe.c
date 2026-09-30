/* shellwindow-probe: GetShellWindow() on the shell's desktop is explorer's
 * (patches/sg/0526). Installers find the signed-in user's shell by it, to
 * run what they installed as that user; it was NULL. name=value lines.
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#include <windows.h>
#include <stdio.h>
int main( void )
{
    HWND shell = GetShellWindow();
    DWORD pid = 0, n = MAX_PATH;
    WCHAR image[MAX_PATH] = L"", desk[64] = L"";
    HANDLE process;

    GetUserObjectInformationW( GetThreadDesktop( GetCurrentThreadId() ), UOI_NAME, desk, sizeof(desk), NULL );
    if (shell && GetWindowThreadProcessId( shell, &pid ) &&
        (process = OpenProcess( PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid )))
    {
        QueryFullProcessImageNameW( process, 0, image, &n );
        CloseHandle( process );
    }
    printf( "desktop=%ls\n", desk );
    printf( "shell=%d\n", shell != NULL );
    printf( "owner=%ls\n", wcsrchr( image, '\\' ) ? wcsrchr( image, '\\' ) + 1 : image );
    return 0;
}
