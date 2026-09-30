/* A program's own right-click verbs show their names (patches/sg/0591): a
 * file type with a verb named by its key's default value and one named by
 * MUIVerb; the file's shell menu must show the names, not the keys. */
#define COBJMACROS
#include <windows.h>
#include <shlobj.h>
#include <stdio.h>

static void set( const WCHAR *key, const WCHAR *name, const WCHAR *value )
{
    HKEY k;
    RegCreateKeyExW( HKEY_CURRENT_USER, key, 0, NULL, 0, KEY_WRITE, NULL, &k, NULL );
    RegSetValueExW( k, name, 0, REG_SZ, (const BYTE *)value, (lstrlenW( value ) + 1) * sizeof(WCHAR) );
    RegCloseKey( k );
}

int main( void )
{
    IShellFolder *desk, *parent;
    IContextMenu *cm;
    LPITEMIDLIST pidl;
    LPCITEMIDLIST child;
    HMENU menu = CreatePopupMenu();
    WCHAR text[256];
    int i, n, byvalue = 0, bymui = 0, bykey = 0;

    set( L"Software\\Classes\\.sgverbtest", NULL, L"SgVerbTest" );
    set( L"Software\\Classes\\SgVerbTest\\shell\\sgdefault", NULL, L"Named By Default Value" );
    set( L"Software\\Classes\\SgVerbTest\\shell\\sgdefault\\command", NULL, L"notepad.exe \"%1\"" );
    set( L"Software\\Classes\\SgVerbTest\\shell\\sgmui", L"MUIVerb", L"Named By MUIVerb" );
    set( L"Software\\Classes\\SgVerbTest\\shell\\sgmui\\command", NULL, L"notepad.exe \"%1\"" );
    CloseHandle( CreateFileW( L"C:\\verb.sgverbtest", GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL ) );
    CoInitialize( NULL );
    SHGetDesktopFolder( &desk );
    if (FAILED(SHParseDisplayName( L"C:\\verb.sgverbtest", NULL, &pidl, 0, NULL )) ||
        FAILED(SHBindToParent( pidl, &IID_IShellFolder, (void **)&parent, &child )) ||
        FAILED(IShellFolder_GetUIObjectOf( parent, NULL, 1, &child, &IID_IContextMenu, NULL, (void **)&cm )))
    { printf( "no menu\n" ); return 2; }
    IContextMenu_QueryContextMenu( cm, menu, 0, 1, 0x7fff, CMF_NORMAL );
    n = GetMenuItemCount( menu );
    for (i = 0; i < n; i++)
    {
        text[0] = 0;
        GetMenuStringW( menu, i, text, ARRAYSIZE(text), MF_BYPOSITION );
        if (!*text) continue;
        printf( "item: %ls\n", text );
        if (!wcscmp( text, L"Named By Default Value" )) byvalue = 1;
        if (!wcscmp( text, L"Named By MUIVerb" )) bymui = 1;
        if (!wcscmp( text, L"sgdefault" ) || !wcscmp( text, L"sgmui" )) bykey = 1;
    }
    printf( "value=%d mui=%d key=%d\n", byvalue, bymui, bykey );
    return !(byvalue && bymui && !bykey);
}
