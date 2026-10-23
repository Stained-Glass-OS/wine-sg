/* restartreason-probe: shell32 RestartDialogEx shows the caller's reason (Stained Glass OS wine-sg 6072), for
 * test/restartreason-gate.sh. Prints KEY=VALUE lines. */
#include <windows.h>
#include <stdio.h>

static int (WINAPI *pRestartDialogEx)(HWND, const WCHAR *, DWORD, DWORD);
static int result = -1;
static const WCHAR *reason;

static DWORD WINAPI run( void *arg )
{
    result = pRestartDialogEx( NULL, reason, EWX_REBOOT, 0 );
    return 0;
}

static HWND found;
static BOOL CALLBACK find( HWND hwnd, LPARAM lp )
{
    WCHAR cls[32];
    GetClassNameW( hwnd, cls, 32 );
    if (!wcscmp( cls, L"#32770" ) && IsWindowVisible( hwnd )) { found = hwnd; return FALSE; }
    return TRUE;
}

static void ask( const char *key, const WCHAR *why )
{
    WCHAR text[512] = L"";
    DWORD tid;
    HANDLE t;
    int i;

    reason = why;
    result = -1;
    t = CreateThread( NULL, 0, run, NULL, 0, &tid );
    for (i = 0, found = NULL; i < 100 && !found; i++) { EnumThreadWindows( tid, find, 0 ); if (!found) Sleep( 100 ); }
    if (found)
    {
        GetDlgItemTextW( found, 0xffff, text, 512 );
        PostMessageW( found, WM_COMMAND, MAKEWPARAM( IDNO, BN_CLICKED ), (LPARAM)GetDlgItem( found, IDNO ) );
    }
    WaitForSingleObject( t, 10000 );
    printf( "%s=%d %d %d %d\n", key, found != NULL, why ? !wcsncmp( text, why, wcslen( why ) ) : 1,
            wcsstr( text, L"restart" ) != NULL, result );
}

int main( void )
{
    setvbuf( stdout, NULL, _IONBF, 0 );
    pRestartDialogEx = (void *)GetProcAddress( LoadLibraryA( "shell32.dll" ), MAKEINTRESOURCEA(730) );
    if (!pRestartDialogEx) return 1;
    ask( "Reason", L"The new driver takes effect after a restart." );
    ask( "NoReason", NULL );
    printf( "done=1\n" );
    return 0;
}
