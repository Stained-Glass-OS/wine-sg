/* userps-probe: CoGetPSClsid of an interface (patches/sg/6208); prints ps=<hr> clsid=<guid>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include <windows.h>
#include <objbase.h>
#include <stdio.h>

int wmain( int argc, WCHAR **argv )
{
    IID iid;
    CLSID clsid = { 0 };
    WCHAR str[40] = L"";
    HRESULT hr;

    if (argc < 2 || FAILED(IIDFromString( argv[1], &iid ))) return 1;
    CoInitializeEx( NULL, COINIT_MULTITHREADED );
    hr = CoGetPSClsid( &iid, &clsid );
    StringFromGUID2( &clsid, str, 40 );
    printf( "ps=%08lx clsid=%ls\n", hr, SUCCEEDED(hr) ? str : L"-" );
    CoUninitialize();
    return 0;
}
