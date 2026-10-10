/* Probe for test/recentdocs-gate.sh (patches/sg/2056): GetOpenFileName through
 * the item dialog, the file typed and Open pressed; then whether the Recent
 * folder has a shortcut to it.
 *   recentdocs-probe default     a plain open: "recent=1"
 *   recentdocs-probe norecent    OFN_DONTADDTORECENT: "recent=0"
 *   recentdocs-probe cancel      Cancel pressed: "recent=0"
 *   recentdocs-probe multi       two files picked: "recent=2"
 *   recentdocs-probe readonly    OFN_NOREADONLYRETURN with a read-only file: the dialog
 *                                says so and stays: "message=1 result=cancelled"
 *   recentdocs-probe readonlyok  the same without the flag: "message=0 result=recentprobe-ro.txt"
 */
#include <windows.h>
#include <commdlg.h>
#include <shlobj.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

static const char *arg;
static WCHAR names[MAX_PATH];
static int message_seen;

static BOOL CALLBACK find_message( HWND w, LPARAM lp )
{
    HWND dlg = (HWND)lp;
    WCHAR cls[20];
    if (GetWindow( w, GW_OWNER ) == dlg && GetClassNameW( w, cls, 20 ) && !wcscmp( cls, L"#32770" ) && IsWindowVisible( w ))
    {
        message_seen = 1;
        PostMessageW( w, WM_COMMAND, IDOK, 0 );
    }
    return TRUE;
}

static DWORD WINAPI drive( void *unused )
{
    HWND dlg = NULL, edit;
    GUITHREADINFO gi = { sizeof(gi) };
    int i;

    for (i = 0; i < 100 && !(dlg = FindWindowW( L"#32770", L"Open it" )); i++) Sleep( 100 );
    Sleep( 1500 );
    if (!dlg) { printf( "none\n" ); fflush( stdout ); ExitProcess( 1 ); }
    GetGUIThreadInfo( GetWindowThreadProcessId( dlg, NULL ), &gi );
    edit = gi.hwndFocus;
    if (!strcmp( arg, "cancel" )) { PostMessageW( dlg, WM_COMMAND, IDCANCEL, 0 ); return 0; }
    SetWindowTextW( edit, names );
    PostMessageW( dlg, WM_COMMAND, IDOK, 0 );
    if (!strncmp( arg, "readonly", 8 ))
    {
        /* the message of a refused file, closed; then the dialog, if it is still there */
        Sleep( 1500 );
        EnumWindows( find_message, (LPARAM)dlg );
        Sleep( 800 );
        if (IsWindow( dlg )) PostMessageW( dlg, WM_COMMAND, IDCANCEL, 0 );
    }
    return 0;
}

static int recent_links( const WCHAR *recent, const WCHAR *a, const WCHAR *b )
{
    WCHAR path[MAX_PATH];
    int n = 0;
    swprintf( path, MAX_PATH, L"%ls\\%ls.lnk", recent, a );
    if (GetFileAttributesW( path ) != INVALID_FILE_ATTRIBUTES) n++;
    swprintf( path, MAX_PATH, L"%ls\\%ls.lnk", recent, b );
    if (GetFileAttributesW( path ) != INVALID_FILE_ATTRIBUTES) n++;
    return n;
}

int main( int argc, char **argv )
{
    WCHAR file[MAX_PATH * 2] = L"", dir[MAX_PATH], recent[MAX_PATH], path[MAX_PATH];
    OPENFILENAMEW ofn = { sizeof(ofn) };
    HWND owner;
    HANDLE h;

    if (argc < 2) return 2;
    arg = argv[1];
    GetTempPathW( MAX_PATH, dir );
    SHGetSpecialFolderPathW( NULL, recent, CSIDL_RECENT, TRUE );
    swprintf( path, MAX_PATH, L"%lsrecentprobe-a.txt", dir );
    h = CreateFileW( path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL ); CloseHandle( h );
    swprintf( path, MAX_PATH, L"%lsrecentprobe-b.txt", dir );
    h = CreateFileW( path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL ); CloseHandle( h );
    swprintf( path, MAX_PATH, L"%ls\\recentprobe-a.txt.lnk", recent ); DeleteFileW( path );
    swprintf( path, MAX_PATH, L"%ls\\recentprobe-b.txt.lnk", recent ); DeleteFileW( path );

    if (!strncmp( arg, "readonly", 8 ))
    {
        swprintf( path, MAX_PATH, L"%lsrecentprobe-ro.txt", dir );
        SetFileAttributesW( path, FILE_ATTRIBUTE_NORMAL );
        h = CreateFileW( path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL ); CloseHandle( h );
        SetFileAttributesW( path, FILE_ATTRIBUTE_READONLY );
        wcscpy( names, L"recentprobe-ro.txt" );
    }
    else if (!strcmp( arg, "multi" )) wcscpy( names, L"\"recentprobe-a.txt\" \"recentprobe-b.txt\"" );
    else wcscpy( names, L"recentprobe-a.txt" );

    owner = CreateWindowW( L"STATIC", L"owner", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 0, 0, 300, 200, 0, 0, 0, 0 );
    ofn.hwndOwner = owner;
    ofn.lpstrFile = file;
    ofn.nMaxFile = ARRAYSIZE( file );
    ofn.lpstrInitialDir = dir;
    ofn.lpstrFilter = L"Text\0*.txt\0";
    ofn.lpstrTitle = L"Open it";
    ofn.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
    if (!strcmp( arg, "norecent" )) ofn.Flags |= OFN_DONTADDTORECENT;
    if (!strcmp( arg, "multi" )) ofn.Flags |= OFN_ALLOWMULTISELECT;
    CloseHandle( CreateThread( NULL, 0, drive, NULL, 0, NULL ) );
    if (!strncmp( arg, "readonly", 8 ))
    {
        ofn.Flags &= ~OFN_FILEMUSTEXIST;
        if (!strcmp( arg, "readonly" )) ofn.Flags |= OFN_NOREADONLYRETURN;
        if (GetSaveFileNameW( &ofn ))
        {
            const WCHAR *base = wcsrchr( file, '\\' );
            printf( "message=%d result=%ls\n", message_seen, base ? base + 1 : file );
        }
        else printf( "message=%d result=cancelled\n", message_seen );
        swprintf( path, MAX_PATH, L"%lsrecentprobe-ro.txt", dir );
        SetFileAttributesW( path, FILE_ATTRIBUTE_NORMAL );
        DeleteFileW( path );
        return 0;
    }
    GetOpenFileNameW( &ofn );
    Sleep( 500 );
    printf( "recent=%d\n", recent_links( recent, L"recentprobe-a.txt", L"recentprobe-b.txt" ) );
    return 0;
}
