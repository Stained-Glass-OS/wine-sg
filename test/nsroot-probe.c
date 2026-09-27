/* nsroot-probe: what the Desktop's shell namespace lists (the file dialogs'
 * Desktop), and that a Unix path still parses (patches/sg/0443).
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#define COBJMACROS
#include <windows.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <stdio.h>

int main( void )
{
    IShellFolder *desktop;
    IEnumIDList *items;
    LPITEMIDLIST pidl;
    STRRET name;
    WCHAR buf[MAX_PATH];
    ULONG eaten;

    CoInitialize( NULL );
    if (FAILED(SHGetDesktopFolder( &desktop ))) return 1;
    if (SUCCEEDED(IShellFolder_EnumObjects( desktop, NULL, SHCONTF_FOLDERS | SHCONTF_NONFOLDERS, &items )))
    {
        while (IEnumIDList_Next( items, 1, &pidl, NULL ) == S_OK)
        {
            if (SUCCEEDED(IShellFolder_GetDisplayNameOf( desktop, pidl, SHGDN_NORMAL, &name )) &&
                SUCCEEDED(StrRetToBufW( &name, pidl, buf, MAX_PATH )))
                printf( "ITEM %ls\n", buf );
            CoTaskMemFree( pidl );
        }
        IEnumIDList_Release( items );
    }
    lstrcpyW( buf, L"/tmp" );
    printf( "PARSE %s\n", SUCCEEDED(IShellFolder_ParseDisplayName( desktop, NULL, NULL, buf, &eaten, &pidl, NULL )) ? "ok" : "failed" );
    return 0;
}
