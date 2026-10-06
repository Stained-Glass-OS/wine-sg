/* Probe for test/savename-gate.sh (wine-sg 1129): GetSaveFileName, through
 * the item dialog (0582), with "Document" suggested and the second of two
 * types (Word Document, *.docx) chosen.
 *   savename-probe sel        the name box once shown: "sel=START-END len=N"
 *   savename-probe NAME       NAME put in the name box and Save pressed:
 *                             "file=<the name the program got>"
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include <windows.h>
#include <commdlg.h>
#include <stdio.h>
#include <string.h>

static const char *arg;

static DWORD WINAPI drive( void *unused )
{
    HWND dlg = NULL, edit;
    GUITHREADINFO gi = { sizeof(gi) };
    DWORD s = 0, e = 0;
    WCHAR name[MAX_PATH];
    int i;

    for (i = 0; i < 100 && !(dlg = FindWindowW( L"#32770", L"Save As" )); i++) Sleep( 100 );
    Sleep( 1500 );
    if (!dlg) { printf( "none\n" ); fflush( stdout ); ExitProcess( 1 ); }
    GetGUIThreadInfo( GetWindowThreadProcessId( dlg, NULL ), &gi );
    edit = gi.hwndFocus;
    if (!strcmp( arg, "sel" ))
    {
        SendMessageW( edit, EM_GETSEL, (WPARAM)&s, (LPARAM)&e );
        printf( "sel=%lu-%lu len=%d\n", s, e, GetWindowTextLengthW( edit ) );
        fflush( stdout );
        PostMessageW( dlg, WM_COMMAND, IDCANCEL, 0 );
        return 0;
    }
    MultiByteToWideChar( CP_ACP, 0, arg, -1, name, MAX_PATH );
    SetWindowTextW( edit, name );
    PostMessageW( dlg, WM_COMMAND, IDOK, 0 );
    return 0;
}

int main( int argc, char **argv )
{
    WCHAR file[MAX_PATH] = L"Document", path[MAX_PATH];
    OPENFILENAMEW ofn = { sizeof(ofn) };
    HWND owner;

    if (argc < 2) return 2;
    arg = argv[1];
    owner = CreateWindowW( L"STATIC", L"owner", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 0, 0, 300, 200, 0, 0, 0, 0 );
    GetTempPathW( MAX_PATH, path );
    ofn.hwndOwner = owner;
    ofn.lpstrFile = file;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrInitialDir = path;
    ofn.lpstrFilter = L"Rich Text Format (RTF)\0*.rtf\0Word Document\0*.docx\0Text Document\0*.txt\0";
    ofn.nFilterIndex = 2;
    ofn.lpstrDefExt = L"docx";
    ofn.lpstrTitle = L"Save As";
    ofn.Flags = OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
    CloseHandle( CreateThread( NULL, 0, drive, NULL, 0, NULL ) );
    if (GetSaveFileNameW( &ofn ))
    {
        const WCHAR *base = wcsrchr( file, '\\' );
        printf( "file=%ls\n", base ? base + 1 : file );
    }
    else if (strcmp( arg, "sel" )) printf( "cancelled\n" );
    return 0;
}
