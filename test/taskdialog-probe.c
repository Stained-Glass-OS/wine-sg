/* A task dialog shown by a program with no common controls manifest
 * (patches/sg/0401): its command links must be version 6 controls with room
 * for their text, as KeePass's first-run question needs. Prints what it saw.
 * Built without a manifest on purpose. */
#include <windows.h>
#include <commctrl.h>
#include <stdio.h>

static int links, min_height = 10000, notes;

static BOOL CALLBACK child( HWND hwnd, LPARAM lparam )
{
    WCHAR cls[64];
    RECT r;
    GetClassNameW( hwnd, cls, 64 );
    if (wcsicmp( cls, L"Button" ) || (GetWindowLongW( hwnd, GWL_STYLE ) & BS_TYPEMASK) < BS_COMMANDLINK) return TRUE;
    GetWindowRect( hwnd, &r );
    links++;
    if (r.bottom - r.top < min_height) min_height = r.bottom - r.top;
    {
        SIZE size = { 0, 0 };
        /* only version 6 buttons answer BCM_GETIDEALSIZE */
        if (SendMessageW( hwnd, BCM_GETIDEALSIZE, 0, (LPARAM)&size ) && size.cy > 0) notes++;
    }
    return TRUE;
}

static HRESULT CALLBACK callback( HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, LONG_PTR data )
{
    if (msg == TDN_CREATED)
    {
        EnumChildWindows( hwnd, child, 0 );
        PostMessageW( hwnd, WM_CLOSE, 0, 0 );
    }
    return S_OK;
}

int main( void )
{
    TASKDIALOG_BUTTON buttons[2] = { { 100, L"Enable (recommended)\nCheck for updates at each start." },
                                     { 101, L"Disable" } };
    TASKDIALOGCONFIG config = { sizeof(config) };
    int button = 0;
    HRESULT hr;

    config.dwFlags = TDF_USE_COMMAND_LINKS | TDF_ALLOW_DIALOG_CANCELLATION;
    config.pszWindowTitle = L"probe";
    config.pszMainInstruction = L"Enable automatic update check?";
    config.cButtons = 2;
    config.pButtons = buttons;
    config.pfCallback = callback;
    hr = TaskDialogIndirect( &config, &button, NULL, NULL );
    printf( "hr=%08lx links=%d min_height=%d v6=%d\n", hr, links, links ? min_height : 0, notes );
    return 0;
}
