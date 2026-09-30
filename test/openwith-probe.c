/* Open with (patches/sg/0594). A file type with a default class (SgOwA:
 * notepad) and another in its OpenWithProgids (SgOwB: write). The file's
 * shell menu has "Open with..."; SHOpenWithDialog lists the app it opens with
 * now and the other; the probe picks the other with "Always use this app":
 * the user's choice (FileExts\.sgowtest\UserChoice) becomes SgOwB. */
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

static DWORD WINAPI dialog_thread( void *arg )
{
    OPENASINFO info = { L"C:\\file.sgowtest", NULL, OAIF_ALLOW_REGISTRATION | OAIF_REGISTER_EXT };
    return SHOpenWithDialog( NULL, &info );
}

int main( void )
{
    IShellFolder *parent;
    IContextMenu *cm;
    LPITEMIDLIST pidl;
    LPCITEMIDLIST child;
    HMENU menu = CreatePopupMenu();
    WCHAR text[256], choice[64] = L"";
    DWORD size = sizeof(choice), code = 0;
    HANDLE t;
    HWND dlg = NULL, list, always;
    int i, n, rows = 0, menu_item = 0, picked = -1;

    set( L"Software\\Classes\\.sgowtest", NULL, L"SgOwA" );
    set( L"Software\\Classes\\.sgowtest\\OpenWithProgids", L"SgOwB", L"" );
    set( L"Software\\Classes\\SgOwA\\shell\\open\\command", NULL, L"C:\\windows\\notepad.exe \"%1\"" );
    set( L"Software\\Classes\\SgOwB\\shell\\open\\command", NULL, L"C:\\windows\\system32\\write.exe \"%1\"" );
    CloseHandle( CreateFileW( L"C:\\file.sgowtest", GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL ) );
    CoInitialize( NULL );

    if (SUCCEEDED(SHParseDisplayName( L"C:\\file.sgowtest", NULL, &pidl, 0, NULL )) &&
        SUCCEEDED(SHBindToParent( pidl, &IID_IShellFolder, (void **)&parent, &child )) &&
        SUCCEEDED(IShellFolder_GetUIObjectOf( parent, NULL, 1, &child, &IID_IContextMenu, NULL, (void **)&cm )))
    {
        IContextMenu_QueryContextMenu( cm, menu, 0, 1, 0x7fff, CMF_NORMAL );
        n = GetMenuItemCount( menu );
        for (i = 0; i < n; i++)
        {
            text[0] = 0;
            GetMenuStringW( menu, i, text, ARRAYSIZE(text), MF_BYPOSITION );
            if (!wcsncmp( text, L"Open wit", 8 )) menu_item = 1;
        }
    }

    t = CreateThread( NULL, 0, dialog_thread, NULL, 0, NULL );
    for (i = 0; i < 100 && !(dlg = FindWindowW( L"SGOpenWith", NULL )); i++)
        if (WaitForSingleObject( t, 100 ) == WAIT_OBJECT_0) break;
    if (!dlg)
    {
        GetExitCodeThread( t, &code );
        printf( "menu=%d no dialog (returned %#lx)\n", menu_item, code );
        return 1;
    }
    Sleep( 500 );
    list = FindWindowExW( dlg, NULL, L"ListBox", NULL );
    always = FindWindowExW( dlg, NULL, L"Button", NULL );
    rows = SendMessageW( list, LB_GETCOUNT, 0, 0 );
    /* rows: "Keep using this app", notepad, "Other options", write, ... */
    if (rows > 3)
    {
        picked = 3;
        SendMessageW( list, LB_SETCURSEL, picked, 0 );
    }
    while (always && GetWindowLongW( always, GWL_ID ) != 101) always = FindWindowExW( dlg, always, L"Button", NULL );
    if (always) SendMessageW( always, BM_SETCHECK, BST_CHECKED, 0 );
    PostMessageW( dlg, WM_COMMAND, IDOK, 0 );
    WaitForSingleObject( t, 5000 );
    GetExitCodeThread( t, &code );
    RegGetValueW( HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts\\.sgowtest\\UserChoice",
                  L"ProgId", RRF_RT_REG_SZ, NULL, choice, &size );
    printf( "menu=%d rows=%d returned=%#lx choice=%ls\n", menu_item, rows, code, choice );
    return !(menu_item && rows > 3 && code == S_OK && !wcscmp( choice, L"SgOwB" ));
}
