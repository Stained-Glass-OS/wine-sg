/* winver-probe: Windows 10 22H2 (patches/sg/0175) and
 * DXGIDeclareAdapterRemovalSupport (0176).
 *
 * Wine reported Windows 10 build 19043 (21H1, out of support since 2022);
 * programs that require 21H2 or later -- Paint.NET, for one -- refused to
 * start. The build, the registry's CurrentBuild / DisplayVersion / UBR, and
 * the DXGI export Paint.NET calls first.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include <windows.h>
#include <stdio.h>

int main( void )
{
    RTL_OSVERSIONINFOEXW v = { sizeof(v) };
    LONG (WINAPI *pRtlGetVersion)( RTL_OSVERSIONINFOEXW * ) =
        (void *)GetProcAddress( GetModuleHandleA( "ntdll.dll" ), "RtlGetVersion" );
    HRESULT (WINAPI *pDeclare)( void );
    WCHAR buf[64];
    DWORD size, ubr = 0;

    pRtlGetVersion( &v );
    printf( "version=%lu.%lu.%lu\n", v.dwMajorVersion, v.dwMinorVersion, v.dwBuildNumber );
    size = sizeof(buf);
    if (!RegGetValueW( HKEY_LOCAL_MACHINE, L"Software\\Microsoft\\Windows NT\\CurrentVersion", L"CurrentBuild",
                       RRF_RT_REG_SZ, NULL, buf, &size )) printf( "CurrentBuild=%ls\n", buf );
    size = sizeof(buf);
    if (!RegGetValueW( HKEY_LOCAL_MACHINE, L"Software\\Microsoft\\Windows NT\\CurrentVersion", L"DisplayVersion",
                       RRF_RT_REG_SZ, NULL, buf, &size )) printf( "DisplayVersion=%ls\n", buf );
    size = sizeof(ubr);
    if (!RegGetValueW( HKEY_LOCAL_MACHINE, L"Software\\Microsoft\\Windows NT\\CurrentVersion", L"UBR",
                       RRF_RT_REG_DWORD, NULL, &ubr, &size )) printf( "UBR=%lu\n", ubr );

    pDeclare = (void *)GetProcAddress( LoadLibraryA( "dxgi.dll" ), "DXGIDeclareAdapterRemovalSupport" );
    if (!pDeclare) printf( "declare=missing\n" );
    else
    {
        HRESULT first = pDeclare(), second = pDeclare();
        printf( "declare=%#lx,%#lx\n", first, second );
    }
    return 0;
}
