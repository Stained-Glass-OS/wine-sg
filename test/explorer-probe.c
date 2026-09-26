/* explorer-probe: look at File Explorer from outside (patches/sg/0110-0112),
 * for test/explorer-gate.sh.
 *
 *   explorer-probe title                 title=... of the File Explorer window
 *   explorer-probe wait-title TEXT SECS  until File Explorer's title is TEXT: title=...
 *   explorer-probe origin                origin=X,Y: its client area's top left on the screen
 *   explorer-probe watch CLASS SECS      seen=1 if a window of that class shows up in that time
 *   explorer-probe find CLASS            found=1 if a visible window of that class exists
 *   explorer-probe columns PATH          the details view's columns for a folder, in process:
 *                                        columns=Name|Date modified|..., then its sort after
 *                                        asking for Size descending, and a selection's count
 *   explorer-probe types FILE...          type=NAME for each (SHGetFileInfo's type name)
 *   explorer-probe groups PATH PID        the view grouped by that FMTID_Storage property, in
 *                                        process: groups=N enabled=E, then header=... each
 *   explorer-probe tiles PATH             the Tiles and Content views, in process: the list
 *                                        view's view, the first tile's lines and width
 *   explorer-probe verb PATH VERB         invokes a context menu verb (pintohome) on PATH
 *   explorer-probe panetext               the preview pane's text: panetext=<first line>
 *   explorer-probe findchild CLASS        found=1 if File Explorer has a visible child of that class
 *   explorer-probe thumb PATH SIZE        IShellItemImageFactory's picture: thumb=WxH top=RRGGBB bottom=RRGGBB
 *   explorer-probe recent PATH            SHAddToRecentDocs(SHARD_PATHW) of a path, in Unicode
 *   explorer-probe close-all              closes every File Explorer window and waits for them to go
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#define COBJMACROS
#include <windows.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <shlwapi.h>
#include <commctrl.h>
#include <stdio.h>

static HWND explorer(void)
{
    return FindWindowW( L"ExplorerWClass", NULL );
}

static int columns( const char *path )
{
    IExplorerBrowser *eb;
    IFolderView2 *fv;
    IShellFolder2 *sf;
    ITEMIDLIST *pidl;
    FOLDERSETTINGS fs = { FVM_DETAILS, 0 };
    SHELLDETAILS sd;
    SORTCOLUMN sc = {{{0}}};
    WCHAR wpath[MAX_PATH], name[80];
    RECT rc = { 0, 0, 600, 400 };
    HWND hwnd;
    MSG msg;
    int i, count = -1;
    DWORD state;
    HRESULT hr;

    MultiByteToWideChar( CP_ACP, 0, path, -1, wpath, MAX_PATH );
    OleInitialize( NULL );
    hwnd = CreateWindowW( L"Static", L"probe", WS_OVERLAPPEDWINDOW, 0, 0, 600, 400, NULL, NULL, NULL, NULL );
    if (FAILED(CoCreateInstance( &CLSID_ExplorerBrowser, NULL, CLSCTX_INPROC_SERVER, &IID_IExplorerBrowser, (void **)&eb )))
    {
        printf( "no ExplorerBrowser\n" );
        return 1;
    }
    IExplorerBrowser_Initialize( eb, hwnd, &rc, &fs );
    pidl = ILCreateFromPathW( wpath );
    IExplorerBrowser_BrowseToIDList( eb, pidl, SBSP_ABSOLUTE );
    while (PeekMessageW( &msg, NULL, 0, 0, PM_REMOVE )) DispatchMessageW( &msg );
    if (FAILED(IExplorerBrowser_GetCurrentView( eb, &IID_IFolderView2, (void **)&fv )))
    {
        printf( "no view\n" );
        return 1;
    }
    IFolderView2_GetFolder( fv, &IID_IShellFolder2, (void **)&sf );
    printf( "columns=" );
    for (i = 0; i < 12; i++)
    {
        if (FAILED(IShellFolder2_GetDetailsOf( sf, NULL, i, &sd ))) break;
        /* the view shows the columns a folder shows by default */
        if (SUCCEEDED(IShellFolder2_GetDefaultColumnState( sf, i, &state )) && !(state & SHCOLSTATE_ONBYDEFAULT)) break;
        StrRetToBufW( &sd.str, NULL, name, 80 );
        printf( "%s%ls", i ? "|" : "", name );
    }
    printf( "\n" );

    sc.propkey.fmtid = FMTID_Storage;
    sc.propkey.pid = 12; /* PID_STG_SIZE */
    sc.direction = SORT_DESCENDING;
    hr = IFolderView2_SetSortColumns( fv, &sc, 1 );
    memset( &sc, 0, sizeof(sc) );
    IFolderView2_GetSortColumns( fv, &sc, 1 );
    printf( "sort hr=%#lx pid=%lu direction=%d\n", hr, sc.propkey.pid, sc.direction );

    IFolderView2_SelectItem( fv, 0, SVSI_SELECT | SVSI_DESELECTOTHERS );
    IFolderView2_ItemCount( fv, SVGIO_SELECTION, &count );
    printf( "selected=%d\n", count );
    IExplorerBrowser_Destroy( eb );
    return 0;
}


/* an ExplorerBrowser of our own on a folder, and its view's list */
static IExplorerBrowser *open_browser(const char *path, UINT mode, HWND *list, IFolderView2 **fv)
{
    IExplorerBrowser *eb;
    ITEMIDLIST *pidl;
    FOLDERSETTINGS fs = { mode, 0 };
    WCHAR wpath[MAX_PATH];
    RECT rc = { 0, 0, 800, 500 };
    IShellView *sv;
    HWND hwnd, view = NULL;
    MSG msg;

    MultiByteToWideChar( CP_ACP, 0, path, -1, wpath, MAX_PATH );
    OleInitialize( NULL );
    hwnd = CreateWindowW( L"Static", L"probe", WS_OVERLAPPEDWINDOW, 0, 0, 800, 500, NULL, NULL, NULL, NULL );
    if (FAILED(CoCreateInstance( &CLSID_ExplorerBrowser, NULL, CLSCTX_INPROC_SERVER, &IID_IExplorerBrowser, (void **)&eb )))
        return NULL;
    IExplorerBrowser_Initialize( eb, hwnd, &rc, &fs );
    pidl = ILCreateFromPathW( wpath );
    IExplorerBrowser_BrowseToIDList( eb, pidl, SBSP_ABSOLUTE );
    while (PeekMessageW( &msg, NULL, 0, 0, PM_REMOVE )) DispatchMessageW( &msg );
    if (FAILED(IExplorerBrowser_GetCurrentView( eb, &IID_IFolderView2, (void **)fv ))) return NULL;
    if (SUCCEEDED(IExplorerBrowser_GetCurrentView( eb, &IID_IShellView, (void **)&sv )))
    {
        IShellView_GetWindow( sv, &view );
        IShellView_Release( sv );
    }
    *list = FindWindowExW( view, NULL, WC_LISTVIEWW, NULL );
    return eb;
}

static void pump(void)
{
    MSG msg;
    DWORD end = GetTickCount() + 300;
    while (GetTickCount() < end)
    {
        while (PeekMessageW( &msg, NULL, 0, 0, PM_REMOVE )) DispatchMessageW( &msg );
        Sleep( 10 );
    }
}

static int groups( const char *path, int pid )
{
    IExplorerBrowser *eb;
    IFolderView2 *fv;
    PROPERTYKEY key;
    HWND list;
    int i, n;

    if (!(eb = open_browser( path, FVM_DETAILS, &list, &fv ))) { printf( "no view\n" ); return 1; }
    key.fmtid = FMTID_Storage;
    key.pid = pid;
    IFolderView2_SetGroupBy( fv, &key, TRUE );
    pump();
    n = SendMessageW( list, LVM_GETGROUPCOUNT, 0, 0 );
    printf( "groups=%d enabled=%d\n", n, (int)SendMessageW( list, LVM_ISGROUPVIEWENABLED, 0, 0 ) );
    for (i = 0; i < n; i++)
    {
        WCHAR header[128] = L"";
        LVGROUP group = { sizeof(group), LVGF_HEADER | LVGF_GROUPID };
        group.pszHeader = header;
        group.cchHeader = 128;
        SendMessageW( list, LVM_GETGROUPINFOBYINDEX, i, (LPARAM)&group );
        printf( "header=%ls\n", header );
    }
    IExplorerBrowser_Destroy( eb );
    return 0;
}

static int tiles( const char *path )
{
    IExplorerBrowser *eb;
    IFolderView2 *fv;
    HWND list;
    UINT columns[4];
    LVTILEINFO info = { sizeof(info) };
    RECT rc;
    int size;
    FOLDERVIEWMODE mode;

    if (!(eb = open_browser( path, FVM_DETAILS, &list, &fv ))) { printf( "no view\n" ); return 1; }
    IFolderView2_SetCurrentViewMode( fv, FVM_TILE );
    pump();
    info.iItem = 0;
    info.cColumns = 4;
    info.puColumns = columns;
    SendMessageW( list, LVM_GETTILEINFO, 0, (LPARAM)&info );
    rc.left = LVIR_BOUNDS;
    SendMessageW( list, LVM_GETITEMRECT, 0, (LPARAM)&rc );
    IFolderView2_GetViewModeAndIconSize( fv, &mode, &size );
    printf( "tiles view=%d lines=%u width=%ld mode=%d size=%d\n", (int)SendMessageW( list, LVM_GETVIEW, 0, 0 ),
            info.cColumns, rc.right - rc.left, mode, size );
    IFolderView2_SetCurrentViewMode( fv, FVM_CONTENT );
    pump();
    info.cColumns = 4;
    SendMessageW( list, LVM_GETTILEINFO, 0, (LPARAM)&info );
    rc.left = LVIR_BOUNDS;
    SendMessageW( list, LVM_GETITEMRECT, 0, (LPARAM)&rc );
    printf( "content view=%d lines=%u width=%ld\n", (int)SendMessageW( list, LVM_GETVIEW, 0, 0 ), info.cColumns, rc.right - rc.left );
    IFolderView2_SetViewModeAndIconSize( fv, FVM_ICON, 256 );
    pump();
    IFolderView2_GetViewModeAndIconSize( fv, &mode, &size );
    printf( "icons view=%d size=%d\n", (int)SendMessageW( list, LVM_GETVIEW, 0, 0 ), size );
    IExplorerBrowser_Destroy( eb );
    return 0;
}

static int verb( const char *path, const char *name )
{
    WCHAR wpath[MAX_PATH];
    ITEMIDLIST *pidl;
    IShellFolder *parent;
    const ITEMIDLIST *child;
    IContextMenu *cm;
    HRESULT hr = E_FAIL;

    MultiByteToWideChar( CP_ACP, 0, path, -1, wpath, MAX_PATH );
    OleInitialize( NULL );
    pidl = ILCreateFromPathW( wpath );
    if (pidl && SUCCEEDED(SHBindToParent( pidl, &IID_IShellFolder, (void **)&parent, &child )))
    {
        if (SUCCEEDED(IShellFolder_GetUIObjectOf( parent, NULL, 1, &child, &IID_IContextMenu, NULL, (void **)&cm )))
        {
            HMENU menu = CreatePopupMenu();
            CMINVOKECOMMANDINFO ici = { sizeof(ici) };
            IContextMenu_QueryContextMenu( cm, menu, 0, 1, 0x7fff, CMF_NORMAL );
            ici.lpVerb = name;
            ici.nShow = SW_SHOWNORMAL;
            hr = IContextMenu_InvokeCommand( cm, &ici );
            DestroyMenu( menu );
            IContextMenu_Release( cm );
        }
        IShellFolder_Release( parent );
    }
    printf( "verb hr=%#lx\n", hr );
    return 0;
}


static int thumb( const char *path, int size )
{
    IShellItemImageFactory *factory;
    WCHAR wpath[MAX_PATH];
    SIZE sz = { size, size };
    HBITMAP bitmap = NULL;
    BITMAP bm;
    HRESULT hr;
    DWORD top = 0, bottom = 0;

    MultiByteToWideChar( CP_ACP, 0, path, -1, wpath, MAX_PATH );
    CoInitialize( NULL );
    hr = SHCreateItemFromParsingName( wpath, NULL, &IID_IShellItemImageFactory, (void **)&factory );
    if (SUCCEEDED(hr))
    {
        hr = IShellItemImageFactory_GetImage( factory, sz, SIIGBF_THUMBNAILONLY, &bitmap );
        IShellItemImageFactory_Release( factory );
    }
    if (FAILED(hr) || !bitmap || !GetObjectW( bitmap, sizeof(bm), &bm ))
    {
        printf( "thumb hr=%#lx\n", hr );
        return 0;
    }
    {
        BITMAPINFO bmi = {{ sizeof(BITMAPINFOHEADER), bm.bmWidth, -bm.bmHeight, 1, 32, BI_RGB }};
        DWORD *px = malloc( bm.bmWidth * bm.bmHeight * 4 );
        HDC dc = GetDC( 0 );
        GetDIBits( dc, bitmap, 0, bm.bmHeight, px, &bmi, DIB_RGB_COLORS );
        ReleaseDC( 0, dc );
        top = px[bm.bmWidth / 2 + (bm.bmHeight / 8) * bm.bmWidth] & 0xffffff;
        bottom = px[bm.bmWidth / 2 + (bm.bmHeight * 7 / 8) * bm.bmWidth] & 0xffffff;
        free( px );
    }
    printf( "thumb=%dx%d top=%06lx bottom=%06lx\n", bm.bmWidth, bm.bmHeight, top, bottom );
    return 0;
}

int main( int argc, char **argv )
{
    WCHAR title[MAX_PATH] = L"", want[MAX_PATH], cls[64];
    DWORD end;

    if (argc == 2 && !strcmp( argv[1], "title" ))
    {
        GetWindowTextW( explorer(), title, MAX_PATH );
        printf( "title=%ls\n", title );
        return 0;
    }
    if (argc == 4 && !strcmp( argv[1], "wait-title" ))
    {
        MultiByteToWideChar( CP_ACP, 0, argv[2], -1, want, MAX_PATH );
        end = GetTickCount() + atoi( argv[3] ) * 1000;
        do
        {
            title[0] = 0;
            GetWindowTextW( explorer(), title, MAX_PATH );
            if (!wcscmp( title, want )) break;
            Sleep( 100 );
        } while (GetTickCount() < end);
        printf( "title=%ls\n", title );
        return 0;
    }
    if (argc == 2 && !strcmp( argv[1], "origin" ))
    {
        POINT pt = { 0, 0 };
        HWND hwnd = explorer();
        if (!hwnd) { printf( "no window\n" ); return 1; }
        ClientToScreen( hwnd, &pt );
        printf( "origin=%ld,%ld\n", pt.x, pt.y );
        return 0;
    }
    if (argc == 4 && !strcmp( argv[1], "watch" ))
    {
        MultiByteToWideChar( CP_ACP, 0, argv[2], -1, cls, 64 );
        end = GetTickCount() + atoi( argv[3] ) * 1000;
        do
        {
            HWND hwnd = FindWindowW( cls, NULL );
            if (hwnd && IsWindowVisible( hwnd ))
            {
                GetWindowTextW( hwnd, title, MAX_PATH );
                printf( "seen=1 title=%ls\n", title );
                return 0;
            }
            Sleep( 50 );
        } while (GetTickCount() < end);
        printf( "seen=0\n" );
        return 0;
    }
    if (argc == 3 && !strcmp( argv[1], "find" ))
    {
        HWND hwnd;
        MultiByteToWideChar( CP_ACP, 0, argv[2], -1, cls, 64 );
        hwnd = FindWindowW( cls, NULL );
        printf( "found=%d\n", hwnd && IsWindowVisible( hwnd ) );
        return 0;
    }
    if (argc == 3 && !strcmp( argv[1], "columns" )) return columns( argv[2] );
    if (argc >= 3 && !strcmp( argv[1], "types" ))
    {
        int i;
        for (i = 2; i < argc; i++)
        {
            SHFILEINFOA info;
            info.szTypeName[0] = 0;
            SHGetFileInfoA( argv[i], 0, &info, sizeof(info), SHGFI_TYPENAME );
            printf( "type=%s\n", info.szTypeName );
        }
        return 0;
    }
    if (argc == 4 && !strcmp( argv[1], "groups" )) return groups( argv[2], atoi( argv[3] ) );
    if (argc == 3 && !strcmp( argv[1], "tiles" )) return tiles( argv[2] );
    if (argc == 4 && !strcmp( argv[1], "verb" )) return verb( argv[2], argv[3] );
    if (argc == 3 && !strcmp( argv[1], "recent" ))
    {
        int n;
        WCHAR **wargv = CommandLineToArgvW( GetCommandLineW(), &n );  /* argv[] went through the code page */
        CoInitialize( NULL );
        SHAddToRecentDocs( SHARD_PATHW, wargv[n - 1] );
        printf( "recent done\n" );
        return 0;
    }
    if (argc == 4 && !strcmp( argv[1], "thumb" )) return thumb( argv[2], atoi( argv[3] ) );
    if (argc == 3 && !strcmp( argv[1], "findchild" ))
    {
        HWND hwnd;
        MultiByteToWideChar( CP_ACP, 0, argv[2], -1, cls, 64 );
        hwnd = FindWindowExW( explorer(), NULL, cls, NULL );
        printf( "found=%d\n", hwnd && IsWindowVisible( hwnd ) );
        return 0;
    }
    if (argc == 2 && !strcmp( argv[1], "close-all" ))
    {
        HWND hwnd;
        int i;
        for (i = 0; i < 100 && (hwnd = explorer()); i++)
        {
            PostMessageW( hwnd, WM_CLOSE, 0, 0 );
            Sleep( 100 );
        }
        printf( "closed=%d\n", explorer() ? 0 : 1 );
        return 0;
    }
    if (argc == 2 && !strcmp( argv[1], "panetext" ))
    {
        HWND pane = FindWindowExW( explorer(), NULL, L"SGExplorerPane", NULL ), edit;
        WCHAR text[256] = L"", *nl;
        edit = pane ? FindWindowExW( pane, NULL, L"Edit", NULL ) : NULL;
        if (edit && IsWindowVisible( edit )) SendMessageW( edit, WM_GETTEXT, 256, (LPARAM)text );
        if ((nl = wcschr( text, '\r' ))) *nl = 0;
        printf( "panetext=%ls\n", text );
        return 0;
    }
    printf( "usage: see the source\n" );
    return 2;
}
