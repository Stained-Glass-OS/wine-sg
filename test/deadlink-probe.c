/* A shortcut whose program is gone (patches/sg/5966), opened with
 * ShellExecuteEx as a program, the desktop or File Explorer would:
 *   dead1.lnk: "Problem with Shortcut" asks whether to delete it, nothing
 *              else comes up (no "Path not found.", no Open with for the
 *              .lnk); Yes deletes the shortcut; ShellExecuteEx fails with
 *              ERROR_CANCELLED;
 *   dead2.lnk: No keeps it, nothing more;
 *   dead2.lnk with SEE_MASK_FLAG_NO_UI: fails with ERROR_FILE_NOT_FOUND,
 *              nothing shown;
 *   live.lnk:  a shortcut whose program is there still starts it;
 *   bad.lnk:   one whose target is there but cannot start: one error box,
 *              no Open with. */
#define COBJMACROS
#include <windows.h>
#include <shlobj.h>
#include <stdio.h>

static int answer;                 /* IDYES / IDNO for the question */
static int seen_problem, seen_error, seen_openwith;

static void make_link( const WCHAR *lnk, const WCHAR *target, const WCHAR *args )
{
    IShellLinkW *sl;
    IPersistFile *pf;
    if (FAILED(CoCreateInstance( &CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, &IID_IShellLinkW, (void **)&sl ))) return;
    IShellLinkW_SetPath( sl, target );
    if (args) IShellLinkW_SetArguments( sl, args );
    if (SUCCEEDED(IShellLinkW_QueryInterface( sl, &IID_IPersistFile, (void **)&pf )))
    {
        IPersistFile_Save( pf, lnk, TRUE );
        IPersistFile_Release( pf );
    }
    IShellLinkW_Release( sl );
}

static HWND seen[64];
static int nseen;

/* each window counted once, however long it stays up */
static BOOL first_sight( HWND w )
{
    int i;
    for (i = 0; i < nseen; i++) if (seen[i] == w) return FALSE;
    if (nseen < 64) seen[nseen++] = w;
    return TRUE;
}

static BOOL CALLBACK look( HWND w, LPARAM lp )
{
    WCHAR cls[64], title[128];
    if (!IsWindowVisible( w )) return TRUE;
    GetClassNameW( w, cls, ARRAYSIZE(cls) );
    GetWindowTextW( w, title, ARRAYSIZE(title) );
    if (!wcscmp( cls, L"SGOpenWith" ))
    {
        if (first_sight( w )) seen_openwith++;
        PostMessageW( w, WM_COMMAND, IDCANCEL, 0 );
    }
    else if (!wcscmp( cls, L"#32770" ) && !wcscmp( title, L"Problem with Shortcut" ))
    {
        if (first_sight( w )) seen_problem++;
        PostMessageW( w, WM_COMMAND, answer, 0 );
    }
    else if (!wcscmp( cls, L"#32770" ))
    {
        if (first_sight( w )) seen_error++;
        PostMessageW( w, WM_COMMAND, IDOK, 0 );
    }
    return TRUE;
}

struct run { const WCHAR *file; DWORD mask; BOOL ret; DWORD err; HANDLE process; };

static DWORD WINAPI run_thread( void *arg )
{
    struct run *r = arg;
    SHELLEXECUTEINFOW sei = { sizeof(sei) };
    CoInitialize( NULL );
    sei.fMask = r->mask;
    sei.lpFile = r->file;
    sei.nShow = SW_SHOWNORMAL;
    SetLastError( 0 );
    r->ret = ShellExecuteExW( &sei );
    r->err = GetLastError();
    r->process = sei.hProcess;
    return 0;
}

/* runs FILE; watches the windows that come up for a while after */
static void run( struct run *r, const WCHAR *file, DWORD mask, int ans )
{
    HANDLE t;
    int i;
    r->file = file; r->mask = mask; r->ret = FALSE; r->err = 0; r->process = NULL;
    answer = ans; seen_problem = seen_error = seen_openwith = 0; nseen = 0;
    t = CreateThread( NULL, 0, run_thread, r, 0, NULL );
    for (i = 0; i < 40; i++)   /* up to 10 s for the thread, then 3 s more for late windows */
    {
        EnumWindows( look, 0 );
        Sleep( 250 );
        if (WaitForSingleObject( t, 0 ) == WAIT_OBJECT_0 && i >= 12) break;
    }
    WaitForSingleObject( t, 5000 );
    CloseHandle( t );
}

int wmain( void )
{
    struct run r;
    WCHAR sys[MAX_PATH], cmd[MAX_PATH];
    HANDLE f;
    DWORD code = 1;

    CoInitialize( NULL );
    GetSystemDirectoryW( sys, MAX_PATH );
    swprintf( cmd, MAX_PATH, L"%ls\\cmd.exe", sys );
    CreateDirectoryW( L"C:\\gone", NULL );
    CloseHandle( CreateFileW( L"C:\\gone\\app.exe", GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL ) );
    make_link( L"C:\\dead1.lnk", L"C:\\gone\\app.exe", NULL );
    make_link( L"C:\\dead2.lnk", L"C:\\gone\\app.exe", NULL );
    DeleteFileW( L"C:\\gone\\app.exe" );
    RemoveDirectoryW( L"C:\\gone" );
    make_link( L"C:\\live.lnk", cmd, L"/c exit 7" );
    f = CreateFileW( L"C:\\bad.exe", GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL );
    WriteFile( f, "not a program", 13, &code, NULL );
    CloseHandle( f );
    make_link( L"C:\\bad.lnk", L"C:\\bad.exe", NULL );

    run( &r, L"C:\\dead1.lnk", 0, IDYES );
    printf( "dead_asks=%d dead_nothing_else=%d dead_yes_deletes=%d dead_cancelled=%d (ret %d err %lu problem %d error %d openwith %d)\n",
            seen_problem == 1, !seen_error && !seen_openwith,
            GetFileAttributesW( L"C:\\dead1.lnk" ) == INVALID_FILE_ATTRIBUTES,
            !r.ret && r.err == ERROR_CANCELLED, r.ret, r.err, seen_problem, seen_error, seen_openwith );
    run( &r, L"C:\\dead2.lnk", 0, IDNO );
    printf( "dead_no_keeps=%d (ret %d err %lu problem %d error %d openwith %d)\n",
            seen_problem == 1 && !seen_error && !seen_openwith && GetFileAttributesW( L"C:\\dead2.lnk" ) != INVALID_FILE_ATTRIBUTES,
            r.ret, r.err, seen_problem, seen_error, seen_openwith );
    run( &r, L"C:\\dead2.lnk", SEE_MASK_FLAG_NO_UI, IDNO );
    printf( "noui_quiet=%d (ret %d err %lu problem %d error %d openwith %d)\n",
            !r.ret && r.err == ERROR_FILE_NOT_FOUND && !seen_problem && !seen_error && !seen_openwith,
            r.ret, r.err, seen_problem, seen_error, seen_openwith );
    run( &r, L"C:\\live.lnk", SEE_MASK_NOCLOSEPROCESS, IDNO );
    code = 0;
    if (r.process) { WaitForSingleObject( r.process, 10000 ); GetExitCodeProcess( r.process, &code ); }
    printf( "live_runs=%d (ret %d exit %lu problem %d error %d)\n", r.ret && code == 7 && !seen_problem && !seen_error,
            r.ret, code, seen_problem, seen_error );
    run( &r, L"C:\\bad.lnk", 0, IDNO );
    printf( "bad_one_error=%d (ret %d err %lu problem %d error %d openwith %d)\n",
            !r.ret && r.err != ERROR_CANCELLED && seen_error == 1 && !seen_openwith && !seen_problem, r.ret, r.err, seen_problem, seen_error, seen_openwith );
    printf( "done=1\n" );
    fflush( stdout );
    return 0;
}
