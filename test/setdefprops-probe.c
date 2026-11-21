/* setdefprops-probe: shell32 SHSetDefaultProperties, which was not exported (Stained Glass OS wine-sg 8272;
 * test/setdefprops-gate.sh): a file type without SetDefaultsFor, or listing only properties outside the three
 * it may set, needs nothing; one listing System.Author has it set where the item's store can take it.
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#define COBJMACROS
#include <windows.h>
#include <shobjidl.h>
#include <propkey.h>
#include <stdio.h>

int main( void )
{
    HRESULT (WINAPI *pSet)(HWND, IShellItem *, DWORD, IFileOperationProgressSink *) =
        (void *)GetProcAddress( LoadLibraryA( "shell32.dll" ), "SHSetDefaultProperties" );
    WCHAR path[MAX_PATH];
    IShellItem *item;
    HKEY key;
    HANDLE f;
    HRESULT hr;
    int fails = 0;

    if (!pSet) { printf( "FAIL  shell32 exports SHSetDefaultProperties\nRESULT: FAIL\n" ); return 1; }
    CoInitialize( NULL );
    GetTempPathW( MAX_PATH, path );
    wcscat( path, L"sg-defprops.sgdef" );
    f = CreateFileW( path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL );
    CloseHandle( f );
    SHCreateItemFromParsingName( path, NULL, &IID_IShellItem, (void **)&item );

    hr = pSet( NULL, item, 0, NULL );
    printf( "%s  no SetDefaultsFor anywhere: S_OK (%#lx)\n", hr == S_OK ? "PASS" : "FAIL", hr );
    if (hr != S_OK) fails++;
    RegSetValueA( HKEY_CLASSES_ROOT, ".sgdef", REG_SZ, "SG.DefTest", 10 );
    RegCreateKeyA( HKEY_CLASSES_ROOT, "SG.DefTest", &key );
    RegSetValueExA( key, "SetDefaultsFor", 0, REG_SZ, (const BYTE *)"prop:System.Title;System.Rating", 32 );
    hr = pSet( NULL, item, 0, NULL );
    printf( "%s  only properties it may not set: S_OK (%#lx)\n", hr == S_OK ? "PASS" : "FAIL", hr );
    if (hr != S_OK) fails++;
    RegSetValueExA( key, "SetDefaultsFor", 0, REG_SZ, (const BYTE *)"prop:System.Author", 19 );
    hr = pSet( NULL, item, 0, NULL );
    printf( "      System.Author listed: %#lx (S_OK, or the store's own error for a type it cannot write)\n", hr );
    if (hr == E_NOTIMPL) fails++;
    hr = pSet( NULL, NULL, 0, NULL );
    printf( "%s  no item: E_INVALIDARG (%#lx)\n", hr == E_INVALIDARG ? "PASS" : "FAIL", hr );
    if (hr != E_INVALIDARG) fails++;
    RegCloseKey( key );
    RegDeleteKeyA( HKEY_CLASSES_ROOT, "SG.DefTest" );
    RegDeleteKeyA( HKEY_CLASSES_ROOT, ".sgdef" );
    IShellItem_Release( item );
    DeleteFileW( path );
    printf( "RESULT: %s\n", fails ? "FAIL" : "PASS" );
    return fails != 0;
}
