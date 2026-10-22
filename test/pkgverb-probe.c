/* pkgverb-probe: File Explorer's context menu for some items, as shell32 builds it, for
 * test/pkgverb-gate.sh (Stained Glass OS wine-sg 6029).
 *   pkgverb-probe menu PATH...           every item of the menu: "item=TITLE|flags", sub-items "sub=TITLE|flags"
 *   pkgverb-probe invoke TITLE PATH...   invoke the item of that title (or sub-item)
 *   pkgverb-probe create CLSID           CoCreateInstance(IExplorerCommand): "create=<hr>"
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#define COBJMACROS
#include <windows.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <stdio.h>

static IContextMenu *menu_for( int count, char **paths )
{
    WCHAR path[MAX_PATH], folder[MAX_PATH];
    ITEMIDLIST *parent = NULL, *children[16];
    IShellFolder *desktop, *sf = NULL;
    IContextMenu *cm = NULL;
    WCHAR *slash;
    int i;

    if (count > 16) count = 16;
    SHGetDesktopFolder( &desktop );
    MultiByteToWideChar( CP_ACP, 0, paths[0], -1, folder, MAX_PATH );
    if ((slash = wcsrchr( folder, '\\' ))) *slash = 0;
    if (FAILED(SHParseDisplayName( folder, NULL, &parent, 0, NULL ))) return NULL;
    IShellFolder_BindToObject( desktop, parent, NULL, &IID_IShellFolder, (void **)&sf );
    if (!sf) return NULL;
    for (i = 0; i < count; i++)
    {
        WCHAR *name;
        MultiByteToWideChar( CP_ACP, 0, paths[i], -1, path, MAX_PATH );
        name = wcsrchr( path, '\\' ) + 1;
        if (FAILED(IShellFolder_ParseDisplayName( sf, NULL, NULL, name, NULL, &children[i], NULL ))) return NULL;
    }
    IShellFolder_GetUIObjectOf( sf, NULL, count, (LPCITEMIDLIST *)children, &IID_IContextMenu, NULL, (void **)&cm );
    return cm;
}

static void dump( HMENU menu, const char *key )
{
    int i, n = GetMenuItemCount( menu );
    for (i = 0; i < n; i++)
    {
        WCHAR text[256] = L"";
        MENUITEMINFOW mii = { sizeof(mii), MIIM_STRING | MIIM_STATE | MIIM_SUBMENU | MIIM_ID | MIIM_FTYPE };
        mii.dwTypeData = text;
        mii.cch = ARRAYSIZE(text);
        GetMenuItemInfoW( menu, i, TRUE, &mii );
        if (mii.fType & MFT_SEPARATOR) continue;
        printf( "%s=%ls|%s%s\n", key, text, (mii.fState & MFS_DISABLED) ? "disabled" : "enabled",
                (mii.fState & MFS_CHECKED) ? ",checked" : "" );
        if (mii.hSubMenu) dump( mii.hSubMenu, "sub" );
    }
}

static UINT find( HMENU menu, const WCHAR *title )
{
    int i, n = GetMenuItemCount( menu );
    for (i = 0; i < n; i++)
    {
        WCHAR text[256] = L"";
        MENUITEMINFOW mii = { sizeof(mii), MIIM_STRING | MIIM_SUBMENU | MIIM_ID };
        UINT id;
        mii.dwTypeData = text;
        mii.cch = ARRAYSIZE(text);
        GetMenuItemInfoW( menu, i, TRUE, &mii );
        if (!lstrcmpW( text, title ) && !mii.hSubMenu) return mii.wID;
        if (mii.hSubMenu && (id = find( mii.hSubMenu, title ))) return id;
    }
    return 0;
}

int main( int argc, char **argv )
{
    IContextMenu *cm;
    HMENU menu;
    HRESULT hr;

    setvbuf( stdout, NULL, _IONBF, 0 );
    CoInitialize( NULL );
    if (argc > 2 && !strcmp( argv[1], "create" ))
    {
        WCHAR str[64];
        CLSID clsid;
        void *obj = NULL;
        MultiByteToWideChar( CP_ACP, 0, argv[2], -1, str, 64 );
        CLSIDFromString( str, &clsid );
        hr = CoCreateInstance( &clsid, NULL, CLSCTX_INPROC_SERVER, &IID_IExplorerCommand, &obj );
        printf( "create=%08lx\n", hr );
        return 0;
    }
    if (argc > 2 && !strcmp( argv[1], "menu" ))
    {
        if (!(cm = menu_for( argc - 2, argv + 2 ))) { printf( "menu=none\n" ); return 1; }
        menu = CreatePopupMenu();
        hr = IContextMenu_QueryContextMenu( cm, menu, 0, 1, 0x7fff, CMF_NORMAL );
        printf( "query=%08lx\n", hr );
        dump( menu, "item" );
        return 0;
    }
    if (argc > 3 && !strcmp( argv[1], "invoke" ))
    {
        CMINVOKECOMMANDINFO ici = { sizeof(ici) };
        WCHAR title[256];
        UINT id;

        if (!(cm = menu_for( argc - 3, argv + 3 ))) { printf( "menu=none\n" ); return 1; }
        menu = CreatePopupMenu();
        IContextMenu_QueryContextMenu( cm, menu, 0, 1, 0x7fff, CMF_NORMAL );
        MultiByteToWideChar( CP_ACP, 0, argv[2], -1, title, 256 );
        if (!(id = find( menu, title ))) { printf( "invoke=notfound\n" ); return 1; }
        ici.lpVerb = MAKEINTRESOURCEA( id - 1 );
        ici.nShow = SW_SHOWNORMAL;
        hr = IContextMenu_InvokeCommand( cm, &ici );
        printf( "invoke=%08lx\n", hr );
        return 0;
    }
    return 2;
}
