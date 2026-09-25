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
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#define COBJMACROS
#include <windows.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <shlwapi.h>
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
    printf( "usage: see the source\n" );
    return 2;
}
