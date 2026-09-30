/* drag-probe: File Explorer's view from outside, for test/drag-gate.sh
 * (patches/sg/0630-0632).
 *
 *   drag-probe lv TITLE              the view of the File Explorer window titled TITLE:
 *                                    view=N count=N selcount=N, then "item I X,Y STATE NAME"
 *                                    for each item (X,Y near its label's left, on the screen;
 *                                    STATE 1 selected, 2 highlighted as a drop's target)
 *   drag-probe place TITLE X Y W H   moves and sizes that window
 *   drag-probe view TITLE icons|details   the view's View > Large icons / Details
 *   drag-probe blank TITLE           blank=X,Y: a point of the view with no item, below them
 *
 * The item's rectangle and name are read with memory given to File Explorer's
 * own process (the list view reads the pointers there).
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include <windows.h>
#include <commctrl.h>
#include <shlobj.h>
#include <stdio.h>
#include <stdlib.h>

/* the shell view's own View commands (not in mingw's headers) */
#define SG_SHVIEW_BIGICON    0x7029
#define SG_SHVIEW_REPORTVIEW 0x702C

static HWND found;

static BOOL CALLBACK find_list( HWND hwnd, LPARAM lp )
{
    WCHAR cls[64];
    GetClassNameW( hwnd, cls, 64 );
    if (!wcscmp( cls, L"SysListView32" ) && IsWindowVisible( hwnd ))
    {
        WCHAR parent[64];
        GetClassNameW( GetParent( hwnd ), parent, 64 );
        if (!wcscmp( parent, L"SHELLDLL_DefView" )) { found = hwnd; return FALSE; }
    }
    return TRUE;
}

static HWND window( const char *title )
{
    WCHAR wtitle[MAX_PATH];
    MultiByteToWideChar( CP_ACP, 0, title, -1, wtitle, MAX_PATH );
    return FindWindowW( L"ExplorerWClass", wtitle );
}

static HWND list_of( const char *title )
{
    HWND win = window( title );
    found = NULL;
    if (win) EnumChildWindows( win, find_list, 0 );
    return found;
}

int main( int argc, char **argv )
{
    HWND lv;

    if (argc == 3 && !strcmp( argv[1], "lv" ))
    {
        DWORD pid;
        HANDLE proc;
        char *remote;
        int i, count, sel;

        if (!(lv = list_of( argv[2] ))) { printf( "no view\n" ); return 1; }
        GetWindowThreadProcessId( lv, &pid );
        proc = OpenProcess( PROCESS_VM_OPERATION | PROCESS_VM_READ | PROCESS_VM_WRITE, FALSE, pid );
        remote = VirtualAllocEx( proc, NULL, 4096, MEM_COMMIT, PAGE_READWRITE );
        count = SendMessageW( lv, LVM_GETITEMCOUNT, 0, 0 );
        sel = SendMessageW( lv, LVM_GETSELECTEDCOUNT, 0, 0 );
        printf( "view=%d count=%d selcount=%d\n", (int)SendMessageW( lv, LVM_GETVIEW, 0, 0 ), count, sel );
        for (i = 0; i < count && remote; i++)
        {
            RECT rc = { LVIR_LABEL };
            LVITEMW item = { 0 };
            WCHAR name[MAX_PATH] = L"";
            POINT pt;
            SIZE_T done;
            UINT state;

            WriteProcessMemory( proc, remote, &rc, sizeof(rc), &done );
            SendMessageW( lv, LVM_GETITEMRECT, i, (LPARAM)remote );
            ReadProcessMemory( proc, remote, &rc, sizeof(rc), &done );
            item.iSubItem = 0;
            item.pszText = (WCHAR *)(remote + 512);
            item.cchTextMax = MAX_PATH;
            WriteProcessMemory( proc, remote + 256, &item, sizeof(item), &done );
            SendMessageW( lv, LVM_GETITEMTEXTW, i, (LPARAM)(remote + 256) );
            ReadProcessMemory( proc, remote + 512, name, sizeof(name) - sizeof(WCHAR), &done );
            pt.x = rc.left + min( 20, (rc.right - rc.left) / 2 );
            pt.y = (rc.top + rc.bottom) / 2;
            ClientToScreen( lv, &pt );
            state = SendMessageW( lv, LVM_GETITEMSTATE, i, LVIS_SELECTED | LVIS_DROPHILITED );
            printf( "item %d %ld,%ld %d %ls\n", i, pt.x, pt.y,
                    ((state & LVIS_SELECTED) ? 1 : 0) | ((state & LVIS_DROPHILITED) ? 2 : 0), name );
        }
        if (remote) VirtualFreeEx( proc, remote, 0, MEM_RELEASE );
        CloseHandle( proc );
        return 0;
    }
    if (argc == 7 && !strcmp( argv[1], "place" ))
    {
        HWND win = window( argv[2] );
        if (!win) { printf( "no window\n" ); return 1; }
        SetWindowPos( win, HWND_TOP, atoi( argv[3] ), atoi( argv[4] ), atoi( argv[5] ), atoi( argv[6] ), 0 );
        printf( "placed\n" );
        return 0;
    }
    if (argc == 4 && !strcmp( argv[1], "view" ))
    {
        if (!(lv = list_of( argv[2] ))) { printf( "no view\n" ); return 1; }
        SendMessageW( GetParent( lv ), WM_COMMAND, !strcmp( argv[3], "details" ) ? SG_SHVIEW_REPORTVIEW : SG_SHVIEW_BIGICON, 0 );
        printf( "view=%d\n", (int)SendMessageW( lv, LVM_GETVIEW, 0, 0 ) );
        return 0;
    }
    if (argc == 3 && !strcmp( argv[1], "blank" ))
    {
        RECT rc;
        POINT pt;
        if (!(lv = list_of( argv[2] ))) { printf( "no view\n" ); return 1; }
        GetClientRect( lv, &rc );
        pt.x = rc.right - 40;
        pt.y = rc.bottom - 30;
        ClientToScreen( lv, &pt );
        printf( "blank=%ld,%ld\n", pt.x, pt.y );
        return 0;
    }
    printf( "usage: see the source\n" );
    return 2;
}
