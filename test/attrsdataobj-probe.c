/* attrsdataobj-probe: shell32 SHGetAttributesFromDataObject (ordinal 750), a stub (Stained Glass OS wine-sg
 * 8276; test/attrsdataobj-gate.sh): two files' data object has 2 items, both file system objects and neither
 * a folder; a data object that holds no shell items is S_FALSE.
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#define COBJMACROS
#include <windows.h>
#include <shlobj.h>
#include <stdio.h>

int main( void )
{
    HRESULT (WINAPI *pAttrs)(IDataObject *, DWORD, DWORD *, UINT *) = (void *)GetProcAddress( LoadLibraryA( "shell32.dll" ), (const char *)750 );
    HRESULT (WINAPI *pCreate)(PCIDLIST_ABSOLUTE, UINT, PCUITEMID_CHILD_ARRAY, IDataObject *, REFIID, void **) =
        (void *)GetProcAddress( GetModuleHandleA( "shell32.dll" ), "SHCreateDataObject" );
    WCHAR dir[MAX_PATH], a[MAX_PATH], b[MAX_PATH];
    IShellFolder *desktop, *folder;
    PIDLIST_RELATIVE pidl_dir, ka, kb;
    LPCITEMIDLIST kids[2];
    IDataObject *data = NULL, *empty = NULL;
    DWORD attrs = 0xdead;
    UINT n = 0;
    HRESULT hr;
    HANDLE f;
    int fails = 0;

    CoInitialize( NULL );
    GetTempPathW( MAX_PATH, dir );
    swprintf( a, MAX_PATH, L"%lssg-attrs-a.txt", dir ); swprintf( b, MAX_PATH, L"%lssg-attrs-b.txt", dir );
    f = CreateFileW( a, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL ); CloseHandle( f );
    f = CreateFileW( b, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL ); CloseHandle( f );
    SHGetDesktopFolder( &desktop );
    IShellFolder_ParseDisplayName( desktop, NULL, NULL, dir, NULL, &pidl_dir, NULL );
    IShellFolder_BindToObject( desktop, pidl_dir, NULL, &IID_IShellFolder, (void **)&folder );
    IShellFolder_ParseDisplayName( folder, NULL, NULL, (WCHAR *)L"sg-attrs-a.txt", NULL, &ka, NULL );
    IShellFolder_ParseDisplayName( folder, NULL, NULL, (WCHAR *)L"sg-attrs-b.txt", NULL, &kb, NULL );
    kids[0] = ka; kids[1] = kb;
    hr = IShellFolder_GetUIObjectOf( folder, NULL, 2, kids, &IID_IDataObject, NULL, (void **)&data );
    if (FAILED(hr) || !data) { printf( "FAIL  a data object for two files (%#lx)\nRESULT: FAIL\n", hr ); return 1; }

    hr = pAttrs( data, SFGAO_FILESYSTEM | SFGAO_FOLDER, &attrs, &n );
    printf( "%s  two files: S_OK, 2 items, FILESYSTEM and not FOLDER (%#lx, %u, %#lx)\n",
            hr == S_OK && n == 2 && attrs == SFGAO_FILESYSTEM ? "PASS" : "FAIL", hr, n, attrs );
    if (!(hr == S_OK && n == 2 && attrs == SFGAO_FILESYSTEM)) fails++;
    hr = pAttrs( data, 0, NULL, &n );
    printf( "%s  only the count (%u)\n", hr == S_OK && n == 2 ? "PASS" : "FAIL", n );
    if (!(hr == S_OK && n == 2)) fails++;
    if (pCreate) pCreate( NULL, 0, NULL, NULL, &IID_IDataObject, (void **)&empty );
    attrs = 0xdead; n = 7;
    hr = empty ? pAttrs( empty, SFGAO_FILESYSTEM, &attrs, &n ) : E_FAIL;
    printf( "%s  a data object with no shell items: S_FALSE, nothing (%#lx, %#lx, %u)\n", hr == S_FALSE && !attrs && !n ? "PASS" : "FAIL", hr, attrs, n );
    if (!(hr == S_FALSE && !attrs && !n)) fails++;
    DeleteFileW( a ); DeleteFileW( b );
    printf( "RESULT: %s\n", fails ? "FAIL" : "PASS" );
    return fails != 0;
}
