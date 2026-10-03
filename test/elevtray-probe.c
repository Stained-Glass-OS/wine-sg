/* elevtray-probe: an elevated program's tray icon (patches/sg/0771).
 *
 *   parent DESK SECONDS  make the desktop DESK (as sg-elevated-run's) and run
 *                        "child" on it
 *   child SECONDS        a window with a tray icon (callback WM_APP+1);
 *                        writes its handle to C:\elevtray.hwnd and every
 *                        message it gets from below to C:\elevtray.log
 *   observer             on the shell's desktop: is the icon in its tray
 *                        (Shell_NotifyIconGetRect)? then runs "sender" at
 *                        medium integrity
 *   sender               below the child: the icon's callback goes through,
 *                        another message does not, and it cannot let one
 *                        through for itself */
#include <windows.h>
#include <shellapi.h>
#include <stdio.h>

#define CALLBACK_MSG (WM_APP + 1)
#define OTHER_MSG    (WM_APP + 2)

static FILE *logf;

static LRESULT CALLBACK proc( HWND hwnd, UINT msg, WPARAM wp, LPARAM lp )
{
    if (msg == CALLBACK_MSG) { fprintf( logf, "callback %04lx\n", (unsigned long)LOWORD(lp) ); fflush( logf ); return 0; }
    if (msg == OTHER_MSG) { fprintf( logf, "other\n" ); fflush( logf ); return 0; }
    return DefWindowProcW( hwnd, msg, wp, lp );
}

static int child( int seconds )
{
    WNDCLASSW wc = { 0 };
    NOTIFYICONDATAW nid = { sizeof(nid) };
    HWND hwnd;
    MSG msg;
    DWORD end;
    FILE *f;

    logf = fopen( "C:\\elevtray.log", "w" );
    wc.lpfnWndProc = proc;
    wc.lpszClassName = L"elevtray";
    RegisterClassW( &wc );
    hwnd = CreateWindowW( L"elevtray", L"elevtray", 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL );
    nid.hWnd = hwnd;
    nid.uID = 7;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = CALLBACK_MSG;
    nid.hIcon = LoadIconW( NULL, (LPCWSTR)IDI_WARNING );
    lstrcpyW( nid.szTip, L"elevtray" );
    fprintf( logf, "add %d\n", Shell_NotifyIconW( NIM_ADD, &nid ) );
    fflush( logf );
    if ((f = fopen( "C:\\elevtray.hwnd", "w" ))) { fprintf( f, "%lu\n", (unsigned long)(ULONG_PTR)hwnd ); fclose( f ); }
    end = GetTickCount() + seconds * 1000;
    while ((int)(end - GetTickCount()) > 0)
    {
        MsgWaitForMultipleObjects( 0, NULL, FALSE, 100, QS_ALLINPUT );
        while (PeekMessageW( &msg, NULL, 0, 0, PM_REMOVE )) DispatchMessageW( &msg );
    }
    Shell_NotifyIconW( NIM_DELETE, &nid );
    fclose( logf );
    return 0;
}

static int parent( const WCHAR *desk, const WCHAR *seconds )
{
    WCHAR cmd[MAX_PATH + 64], self[MAX_PATH], name[96];
    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi;

    if (!CreateDesktopW( desk, NULL, NULL, 0, GENERIC_ALL, NULL )) { printf( "no desktop %lu\n", GetLastError() ); return 1; }
    GetModuleFileNameW( NULL, self, MAX_PATH );
    _snwprintf( cmd, ARRAYSIZE(cmd), L"\"%ls\" child %ls", self, seconds );
    _snwprintf( name, ARRAYSIZE(name), L"WinSta0\\%ls", desk );
    si.lpDesktop = name;
    if (!CreateProcessW( NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi )) { printf( "no child %lu\n", GetLastError() ); return 1; }
    WaitForSingleObject( pi.hProcess, INFINITE );
    return 0;
}

static HWND child_window(void)
{
    unsigned long h = 0;
    FILE *f = fopen( "C:\\elevtray.hwnd", "r" );
    if (f) { if (fscanf( f, "%lu", &h ) != 1) h = 0; fclose( f ); }
    return (HWND)(ULONG_PTR)h;
}

static int sender(void)
{
    HWND hwnd = child_window();
    printf( "callback-posted %d\n", PostMessageW( hwnd, CALLBACK_MSG, 7, WM_LBUTTONUP ) );
    printf( "other-posted %d\n", PostMessageW( hwnd, OTHER_MSG, 0, 0 ) );
    printf( "allow-self %d\n", SetPropW( hwnd, L"__SG_UIPI_ALLOW_8002", (HANDLE)1 ) );
    printf( "other-posted-after %d\n", PostMessageW( hwnd, OTHER_MSG, 0, 0 ) );
    return 0;
}

static int observer(void)
{
    NOTIFYICONIDENTIFIER id = { sizeof(id) };
    WCHAR cmd[MAX_PATH + 16], self[MAX_PATH];
    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    struct { TOKEN_MANDATORY_LABEL label; BYTE sid[SECURITY_MAX_SID_SIZE]; } il;
    DWORD size = sizeof(il.sid);
    HANDLE token, medium;
    RECT rc;

    id.hWnd = child_window();
    id.uID = 7;
    printf( "in-shell-tray %d\n", Shell_NotifyIconGetRect( &id, &rc ) == S_OK );
    fflush( stdout );

    OpenProcessToken( GetCurrentProcess(), TOKEN_ALL_ACCESS, &token );
    DuplicateTokenEx( token, TOKEN_ALL_ACCESS, NULL, SecurityImpersonation, TokenPrimary, &medium );
    CreateWellKnownSid( WinMediumLabelSid, NULL, il.sid, &size );
    il.label.Label.Sid = il.sid;
    il.label.Label.Attributes = SE_GROUP_INTEGRITY;
    if (!SetTokenInformation( medium, TokenIntegrityLevel, &il.label, sizeof(il.label) + size ))
    {
        printf( "no-medium %lu\n", GetLastError() );
        return 1;
    }
    GetModuleFileNameW( NULL, self, MAX_PATH );
    _snwprintf( cmd, ARRAYSIZE(cmd), L"\"%ls\" sender", self );
    if (!CreateProcessAsUserW( medium, NULL, cmd, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi ))
    {
        printf( "no-sender %lu\n", GetLastError() );
        return 1;
    }
    WaitForSingleObject( pi.hProcess, 20000 );
    return 0;
}

int wmain( int argc, WCHAR **argv )
{
    if (argc > 3 && !wcscmp( argv[1], L"parent" )) return parent( argv[2], argv[3] );
    if (argc > 2 && !wcscmp( argv[1], L"child" )) return child( _wtoi( argv[2] ) );
    if (argc > 1 && !wcscmp( argv[1], L"observer" )) return observer();
    if (argc > 1 && !wcscmp( argv[1], L"sender" )) return sender();
    return 2;
}
