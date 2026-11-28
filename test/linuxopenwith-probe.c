/* linuxopenwith-gate.sh's probe (patches/sg/9517): two Linux apps' classes, as sg-shell's
 * sg-linuxapp registers them -- the same launcher program starts both, each class says which app
 * it is (its Application key: ApplicationName, AppUserModelID) -- in a type's OpenWithProgids.
 * Opens "How do you want to open this file?" (SHOpenWithDialog) and prints its rows' words, a line
 * each, then picks the second app with "Always use this app" and prints the choice. */
#include <windows.h>
#include <shlobj.h>
#include <stdio.h>

static void set( const WCHAR *key, const WCHAR *name, const WCHAR *value )
{
    HKEY k;
    RegCreateKeyExW( HKEY_CURRENT_USER, key, 0, NULL, 0, KEY_WRITE, NULL, &k, NULL );
    RegSetValueExW( k, name, 0, REG_SZ, (const BYTE *)value, (lstrlenW( value ) + 1) * sizeof(WCHAR) );
    RegCloseKey( k );
}

static DWORD WINAPI dialog_thread( void *arg )
{
    OPENASINFO info = { L"C:\\drawing.sgsvgtest", NULL, OAIF_ALLOW_REGISTRATION | OAIF_REGISTER_EXT };
    return SHOpenWithDialog( NULL, &info );
}

int main( void )
{
    WCHAR text[256], choice[64] = L"";
    DWORD size = sizeof(choice), code = 0;
    HANDLE t;
    HWND dlg = NULL, list, always;
    int i, rows, paint = -1;

    set( L"Software\\Classes\\.sgsvgtest\\OpenWithProgids", L"SG.LinuxApp.org.gate.Draw", L"" );
    set( L"Software\\Classes\\.sgsvgtest\\OpenWithProgids", L"SG.LinuxApp.org.gate.Paint", L"" );
    set( L"Software\\Classes\\SG.LinuxApp.org.gate.Draw\\shell\\open\\command", NULL,
         L"\"C:\\windows\\system32\\launcher.exe\" --open \"Z:\\draw.desktop\" \"%1\"" );
    set( L"Software\\Classes\\SG.LinuxApp.org.gate.Draw\\Application", L"ApplicationName", L"Gate Draw" );
    set( L"Software\\Classes\\SG.LinuxApp.org.gate.Draw\\Application", L"AppUserModelID", L"SG.LinuxApp.org.gate.Draw" );
    set( L"Software\\Classes\\SG.LinuxApp.org.gate.Paint\\shell\\open\\command", NULL,
         L"\"C:\\windows\\system32\\launcher.exe\" --open \"Z:\\paint.desktop\" \"%1\"" );
    set( L"Software\\Classes\\SG.LinuxApp.org.gate.Paint\\Application", L"ApplicationName", L"Gate Paint" );
    set( L"Software\\Classes\\SG.LinuxApp.org.gate.Paint\\Application", L"AppUserModelID", L"SG.LinuxApp.org.gate.Paint" );
    CloseHandle( CreateFileW( L"C:\\drawing.sgsvgtest", GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL ) );
    CoInitialize( NULL );
    t = CreateThread( NULL, 0, dialog_thread, NULL, 0, NULL );
    for (i = 0; i < 100 && !(dlg = FindWindowW( L"SGOpenWith", NULL )); i++)
        if (WaitForSingleObject( t, 100 ) == WAIT_OBJECT_0) break;
    if (!dlg) { printf( "no dialog\n" ); return 1; }
    Sleep( 500 );
    list = FindWindowExW( dlg, NULL, L"ListBox", NULL );
    rows = SendMessageW( list, LB_GETCOUNT, 0, 0 );
    for (i = 0; i < rows; i++)
    {
        text[0] = 0;
        if (SendMessageW( list, LB_GETTEXTLEN, i, 0 ) < ARRAYSIZE(text)) SendMessageW( list, LB_GETTEXT, i, (LPARAM)text );
        printf( "row %ls\n", text );
        if (!lstrcmpW( text, L"Gate Paint" )) paint = i;
    }
    if (paint >= 0) SendMessageW( list, LB_SETCURSEL, paint, 0 );
    always = FindWindowExW( dlg, NULL, L"Button", NULL );
    while (always && GetWindowLongW( always, GWL_ID ) != 101) always = FindWindowExW( dlg, always, L"Button", NULL );
    if (always) SendMessageW( always, BM_SETCHECK, BST_CHECKED, 0 );
    PostMessageW( dlg, paint >= 0 ? WM_COMMAND : WM_CLOSE, IDOK, 0 );
    WaitForSingleObject( t, 5000 );
    GetExitCodeThread( t, &code );
    RegGetValueW( HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts\\.sgsvgtest\\UserChoice",
                  L"ProgId", RRF_RT_REG_SZ, NULL, choice, &size );
    printf( "choice %ls\n", choice );
    return 0;
}
