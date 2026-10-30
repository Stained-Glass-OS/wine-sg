/* A file whose type has no program, opened from the shell (patches/sg/6542).
 * The probe hosts the shell's folder view (ExplorerBrowser, as File Explorer
 * does) on C:\sgow and double-clicks files of made-up types there with real
 * mouse input:
 *   doc.sgzq: Open with ("How do you want to open this file?") comes up, not
 *             the error box; it lists the test app and "Look for an app in the
 *             Store"; the app with "Always use this app" opens the file and
 *             becomes the type's (FileExts UserChoice and the user's class,
 *             as Settings' Default apps sets it); a second double-click opens
 *             it with that app, no dialog;
 *   doc.sgzr: "Look for an app in the Store" starts SG Store (App Paths
 *             sg-store.exe: this probe as C:\store.exe) searching for the
 *             type (--search sgzr) and chooses nothing;
 *   doc.sgzs: Cancel shows nothing more; a program's own ShellExecuteEx of it
 *             succeeds and Open with comes up in a process of its own (as
 *             Windows' OpenWith does, patches/sg/5965); with
 *             SEE_MASK_FLAG_NO_UI it fails with SE_ERR_NOASSOC, nothing shown.
 * Modes: "opened FILE" (the test app), and as store.exe (the Store). */
#define COBJMACROS
#include <windows.h>
#include <commctrl.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <stdio.h>
#include <stdlib.h>

static HWND host;
static IExplorerBrowser *browser;

static void set( HKEY root, const WCHAR *key, const WCHAR *name, const WCHAR *value )
{
    HKEY k;
    if (RegCreateKeyExW( root, key, 0, NULL, 0, KEY_WRITE, NULL, &k, NULL )) return;
    RegSetValueExW( k, name, 0, REG_SZ, (const BYTE *)value, (lstrlenW( value ) + 1) * sizeof(WCHAR) );
    RegCloseKey( k );
}

static BOOL get( HKEY root, const WCHAR *key, const WCHAR *name, WCHAR *out, DWORD len )
{
    DWORD size = len * sizeof(WCHAR);
    out[0] = 0;
    return !RegGetValueW( root, key, name, RRF_RT_REG_SZ, NULL, out, &size );
}

static void touch( const WCHAR *path )
{
    CloseHandle( CreateFileW( path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL ) );
}

static BOOL exists( const WCHAR *path )
{
    return GetFileAttributesW( path ) != INVALID_FILE_ATTRIBUTES;
}

static BOOL wait_file( const WCHAR *path, int ms )
{
    for (; ms > 0; ms -= 100) { if (exists( path )) return TRUE; Sleep( 100 ); }
    return exists( path );
}

static BOOL CALLBACK find_list( HWND hwnd, LPARAM lp )
{
    WCHAR cls[64];
    GetClassNameW( hwnd, cls, ARRAYSIZE(cls) );
    if (!wcscmp( cls, WC_LISTVIEWW )) { *(HWND *)lp = hwnd; return FALSE; }
    return TRUE;
}

static void click( int x, int y, int times )
{
    INPUT in[2] = {{ 0 }};
    int i;
    SetCursorPos( x, y );
    Sleep( 100 );
    for (i = 0; i < times; i++)
    {
        in[0].type = in[1].type = INPUT_MOUSE;
        in[0].mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
        in[1].mi.dwFlags = MOUSEEVENTF_LEFTUP;
        SendInput( 2, in, sizeof(INPUT) );
        Sleep( 30 );
    }
}

/* double-click a file's icon in the folder view */
static BOOL double_click( const WCHAR *name )
{
    LVFINDINFOW find = { LVFI_STRING };
    HWND list = NULL;
    WCHAR base[64], *dot;
    RECT rc;
    int index;

    EnumChildWindows( host, find_list, (LPARAM)&list );
    if (!list) { printf( "no folder view\n" ); return FALSE; }
    find.psz = name;
    if ((index = SendMessageW( list, LVM_FINDITEMW, -1, (LPARAM)&find )) < 0)
    {
        lstrcpyW( base, name );
        if ((dot = wcsrchr( base, '.' ))) *dot = 0;
        find.psz = base;
        index = SendMessageW( list, LVM_FINDITEMW, -1, (LPARAM)&find );
    }
    if (index < 0) { printf( "no item %ls\n", name ); return FALSE; }
    rc.left = LVIR_ICON;
    SendMessageW( list, LVM_GETITEMRECT, index, (LPARAM)&rc );
    MapWindowPoints( list, NULL, (POINT *)&rc, 2 );
    SetForegroundWindow( host );
    click( (rc.left + rc.right) / 2, (rc.top + rc.bottom) / 2, 2 );
    return TRUE;
}

static HWND find_error_box( void )
{
    HWND box = NULL;
    WCHAR text[256];
    while ((box = FindWindowExW( NULL, box, L"#32770", NULL )))
    {
        HWND st = NULL;
        while ((st = FindWindowExW( box, st, L"Static", NULL )))
        {
            GetWindowTextW( st, text, ARRAYSIZE(text) );
            if (wcsstr( text, L"no program configured" )) return box;
        }
    }
    return NULL;
}

/* what came up within MS: 1 Open with, 2 the error box, 0 nothing */
static int wait_window( HWND *out, int ms )
{
    for (; ms > 0; ms -= 100)
    {
        if ((*out = FindWindowW( L"SGOpenWith", NULL )) && IsWindowVisible( *out )) return 1;
        if ((*out = find_error_box())) return 2;
        Sleep( 100 );
    }
    return 0;
}

static int row_of( HWND list, const WCHAR *text )
{
    WCHAR buf[256];
    int i, n = SendMessageW( list, LB_GETCOUNT, 0, 0 );
    for (i = 0; i < n; i++)
        if (SendMessageW( list, LB_GETTEXTLEN, i, 0 ) < ARRAYSIZE(buf) &&
            SendMessageW( list, LB_GETTEXT, i, (LPARAM)buf ) > 0 && wcsstr( buf, text )) return i;
    return -1;
}

static HWND always_box( HWND dlg )
{
    HWND b = NULL;
    while ((b = FindWindowExW( dlg, b, L"Button", NULL ))) if (GetWindowLongW( b, GWL_ID ) == 101) return b;
    return NULL;
}

static void close_box( HWND box )
{
    PostMessageW( box, WM_COMMAND, IDOK, 0 );
    PostMessageW( box, WM_CLOSE, 0, 0 );
}

static BOOL program_ret, noui_ret;
static DWORD noui_err;
static INT_PTR noui_inst;

static DWORD WINAPI program_open( void *arg )
{
    SHELLEXECUTEINFOW sei = { sizeof(sei) };
    sei.lpFile = L"C:\\sgow\\doc.sgzs";
    sei.nShow = SW_SHOWNORMAL;
    program_ret = ShellExecuteExW( &sei );
    return program_ret;
}

static DWORD WINAPI noui_open( void *arg )
{
    SHELLEXECUTEINFOW sei = { sizeof(sei) };
    sei.fMask = SEE_MASK_FLAG_NO_UI;
    sei.lpFile = L"C:\\sgow\\doc.sgzs";
    sei.nShow = SW_SHOWNORMAL;
    SetLastError( 0 );
    noui_ret = ShellExecuteExW( &sei );
    noui_err = GetLastError();
    noui_inst = (INT_PTR)sei.hInstApp;
    return noui_ret;
}

static DWORD WINAPI worker( void *arg )
{
    WCHAR value[MAX_PATH], text[MAX_PATH];
    HWND w, list;
    HANDLE t;
    int kind, r;
    FILE *f;

    Sleep( 1500 );

    /* A: the first double-click asks */
    double_click( L"doc.sgzq" );
    kind = wait_window( &w, 10000 );
    printf( "first_dialog=%d error_box=%d\n", kind == 1, kind == 2 );
    if (kind == 2) close_box( w );
    if (kind == 1)
    {
        HWND always = always_box( w );
        list = FindWindowExW( w, NULL, L"ListBox", NULL );
        printf( "store_row=%d always_default=%d\n", row_of( list, L"Look for an app in the Store" ) >= 0,
                always && SendMessageW( always, BM_GETCHECK, 0, 0 ) == BST_CHECKED );
        r = row_of( list, L"SG Open-With Test App" );
        printf( "app_listed=%d\n", r >= 0 );
        if (r >= 0) SendMessageW( list, LB_SETCURSEL, r, 0 );
        if (always) SendMessageW( always, BM_SETCHECK, BST_CHECKED, 0 );
        PostMessageW( w, WM_COMMAND, IDOK, 0 );
    }
    r = wait_file( L"C:\\sgow\\opened.txt", 8000 );
    text[0] = 0;
    if (r && (f = _wfopen( L"C:\\sgow\\opened.txt", L"r" ))) { fgetws( text, ARRAYSIZE(text), f ); fclose( f ); }
    printf( "launched=%d\n", r && wcsstr( text, L"doc.sgzq" ) != NULL );
    get( HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts\\.sgzq\\UserChoice",
         L"ProgId", value, ARRAYSIZE(value) );
    printf( "userchoice=%d (%ls)\n", !lstrcmpiW( value, L"Applications\\sgowapp.exe" ), value );
    get( HKEY_CURRENT_USER, L"Software\\Classes\\.sgzq", NULL, value, ARRAYSIZE(value) );
    printf( "userclass=%d (%ls)\n", !lstrcmpiW( value, L"Applications\\sgowapp.exe" ), value );

    /* B: the choice is kept */
    Sleep( 500 );
    DeleteFileW( L"C:\\sgow\\opened.txt" );
    double_click( L"doc.sgzq" );
    r = wait_file( L"C:\\sgow\\opened.txt", 8000 );
    kind = wait_window( &w, 300 );
    printf( "always_used=%d\n", r && kind == 0 );
    if (kind == 1) PostMessageW( w, WM_COMMAND, IDCANCEL, 0 );
    if (kind == 2) close_box( w );

    /* C: the Store */
    Sleep( 500 );
    double_click( L"doc.sgzr" );
    if ((kind = wait_window( &w, 10000 )) == 1)
    {
        list = FindWindowExW( w, NULL, L"ListBox", NULL );
        if ((r = row_of( list, L"Look for an app in the Store" )) >= 0) SendMessageW( list, LB_SETCURSEL, r, 0 );
        PostMessageW( w, WM_COMMAND, IDOK, 0 );
    }
    else if (kind == 2) close_box( w );
    r = wait_file( L"C:\\sgow\\store.txt", 8000 );
    Sleep( 500 );
    text[0] = 0;
    if (r && (f = _wfopen( L"C:\\sgow\\store.txt", L"r" ))) { fgetws( text, ARRAYSIZE(text), f ); fclose( f ); }
    printf( "store_search=%d (%ls)\n", wcsstr( text, L"--search sgzr" ) != NULL, text );
    value[0] = 0;
    get( HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts\\.sgzr\\UserChoice",
         L"ProgId", value, ARRAYSIZE(value) );
    printf( "store_opened=%d store_no_choice=%d dialog_closed=%d\n", kind == 1 && r, !value[0],
            !FindWindowW( L"SGOpenWith", NULL ) );

    /* E: Cancel says nothing more */
    double_click( L"doc.sgzs" );
    if ((kind = wait_window( &w, 10000 )) == 1) PostMessageW( w, WM_COMMAND, IDCANCEL, 0 );
    else if (kind == 2) close_box( w );
    Sleep( 1500 );
    printf( "cancel_quiet=%d\n", kind == 1 && !find_error_box() && !FindWindowW( L"SGOpenWith", NULL ) );

    /* D: a program's own ShellExecuteEx asks too, from a process of its own */
    t = CreateThread( NULL, 0, program_open, NULL, 0, NULL );
    kind = wait_window( &w, 10000 );
    {
        DWORD pid = 0;
        if (kind == 1) GetWindowThreadProcessId( w, &pid );
        WaitForSingleObject( t, 5000 );
        printf( "program_open_with=%d program_own_process=%d program_ok=%d error_box=%d\n", kind == 1,
                kind == 1 && pid && pid != GetCurrentProcessId(), program_ret, kind == 2 );
        /* rundll32's hidden window is no owner: centred on the screen, a taskbar window */
        if (kind == 1)
        {
            RECT rc, wa;
            GetWindowRect( w, &rc );
            SystemParametersInfoW( SPI_GETWORKAREA, 0, &wa, 0 );
            printf( "program_unowned=%d program_centred=%d (x %ld..%ld of %ld..%ld)\n", !GetWindow( w, GW_OWNER ),
                    abs( (int)((rc.left + rc.right) / 2 - (wa.left + wa.right) / 2) ) < (wa.right - wa.left) / 20,
                    rc.left, rc.right, wa.left, wa.right );
        }
    }
    if (kind == 1) PostMessageW( w, WM_COMMAND, IDCANCEL, 0 );
    if (kind == 2) close_box( w );
    Sleep( 1500 );
    /* and with SEE_MASK_FLAG_NO_UI: the error, nothing shown */
    t = CreateThread( NULL, 0, noui_open, NULL, 0, NULL );
    WaitForSingleObject( t, 10000 );
    kind = wait_window( &w, 2000 );
    printf( "noui_error=%d (ret %d err %lu inst %Id) noui_quiet=%d\n", !noui_ret && noui_inst == SE_ERR_NOASSOC,
            noui_ret, noui_err, noui_inst, kind == 0 );
    if (kind == 1) PostMessageW( w, WM_COMMAND, IDCANCEL, 0 );
    if (kind == 2) close_box( w );
    printf( "done=1\n" );
    fflush( stdout );
    PostMessageW( host, WM_CLOSE, 0, 0 );
    return 0;
}

static LRESULT CALLBACK host_proc( HWND hwnd, UINT msg, WPARAM wp, LPARAM lp )
{
    if (msg == WM_DESTROY) PostQuitMessage( 0 );
    return DefWindowProcW( hwnd, msg, wp, lp );
}

int wmain( int argc, WCHAR **argv )
{
    WNDCLASSW wc = { 0 };
    WCHAR self[MAX_PATH], cmd[MAX_PATH + 32];
    FOLDERSETTINGS fs = { FVM_ICON, 0 };
    ITEMIDLIST *pidl;
    RECT rc = { 0, 0, 760, 520 };
    HANDLE t;
    MSG msg;
    FILE *f;

    GetModuleFileNameW( NULL, self, ARRAYSIZE(self) );
    if (!lstrcmpiW( wcsrchr( self, '\\' ) + 1, L"store.exe" ))
    {
        /* what it was asked to search for */
        if ((f = _wfopen( L"C:\\sgow\\store.tmp", L"w" )))
        {
            int i;
            for (i = 1; i < argc; i++) fwprintf( f, L"%ls ", argv[i] );
            fclose( f );
        }
        MoveFileW( L"C:\\sgow\\store.tmp", L"C:\\sgow\\store.txt" );
        return 0;
    }
    if (argc > 2 && !wcscmp( argv[1], L"opened" ))
    {
        if ((f = _wfopen( L"C:\\sgow\\opened.tmp", L"w" ))) { fputws( argv[2], f ); fclose( f ); }
        MoveFileW( L"C:\\sgow\\opened.tmp", L"C:\\sgow\\opened.txt" );
        return 0;
    }

    CreateDirectoryW( L"C:\\sgow", NULL );
    touch( L"C:\\sgow\\doc.sgzq" );
    touch( L"C:\\sgow\\doc.sgzr" );
    touch( L"C:\\sgow\\doc.sgzs" );
    set( HKEY_CURRENT_USER, L"Software\\Classes\\Applications\\sgowapp.exe\\shell\\open\\command", NULL,
         L"\"C:\\sgowapp.exe\" opened \"%1\"" );
    set( HKEY_CURRENT_USER, L"Software\\Classes\\Applications\\sgowapp.exe", L"FriendlyAppName", L"SG Open-With Test App" );
    set( HKEY_CURRENT_USER, L"Software\\Classes\\Applications\\sgowapp.exe\\SupportedTypes", L".sgzq", L"" );
    set( HKEY_LOCAL_MACHINE, L"Software\\Microsoft\\Windows\\CurrentVersion\\App Paths\\sg-store.exe", NULL, L"C:\\store.exe" );
    swprintf( cmd, ARRAYSIZE(cmd), L"%ls", L"C:\\store.exe" );
    printf( "store_registered=%d\n", get( HKEY_LOCAL_MACHINE,
            L"Software\\Microsoft\\Windows\\CurrentVersion\\App Paths\\sg-store.exe", NULL, cmd, ARRAYSIZE(cmd) ) );

    OleInitialize( NULL );
    wc.lpfnWndProc = host_proc;
    wc.lpszClassName = L"SGNoAssocHost";
    wc.hCursor = LoadCursorW( NULL, (LPCWSTR)IDC_ARROW );
    RegisterClassW( &wc );
    host = CreateWindowExW( 0, wc.lpszClassName, L"Folder", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 40, 40, 800, 580,
                            NULL, NULL, NULL, NULL );
    if (FAILED(CoCreateInstance( &CLSID_ExplorerBrowser, NULL, CLSCTX_INPROC_SERVER, &IID_IExplorerBrowser,
                                 (void **)&browser )) ||
        FAILED(IExplorerBrowser_Initialize( browser, host, &rc, &fs )) ||
        FAILED(SHParseDisplayName( L"C:\\sgow", NULL, &pidl, 0, NULL )) ||
        FAILED(IExplorerBrowser_BrowseToIDList( browser, pidl, SBSP_ABSOLUTE )))
    {
        printf( "no folder view\n" );
        return 1;
    }
    t = CreateThread( NULL, 0, worker, NULL, 0, NULL );
    while (GetMessageW( &msg, NULL, 0, 0 ))
    {
        TranslateMessage( &msg );
        DispatchMessageW( &msg );
    }
    WaitForSingleObject( t, 5000 );
    IExplorerBrowser_Destroy( browser );
    return 0;
}
