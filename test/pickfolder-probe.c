/* A folder picker's OK button with no name typed (patches/sg/7906). IFileOpenDialog with
 * FOS_PICKFOLDERS, its folder set; a helper thread presses the dialog's IDOK button the way a
 * click does (BM_CLICK). Windows returns the folder being shown; Wine did nothing (DOSBox-X's
 * first-run "Choose" button). Prints key=1 per check. */
#define COBJMACROS
#include <windows.h>
#include <shobjidl.h>
#include <stdio.h>

static const WCHAR *g_title;
static WCHAR g_type[MAX_PATH];

static DWORD WINAPI presser( void *arg )
{
    HWND dlg = NULL;
    int i;
    for (i = 0; i < 200 && !dlg; i++) { Sleep( 50 ); dlg = FindWindowW( NULL, g_title ); }
    if (!dlg) return 1;
    Sleep( 500 );
    if (g_type[0]) SetDlgItemTextW( dlg, 0x480 /* edt1 */, g_type );
    SendMessageW( GetDlgItem( dlg, IDOK ), BM_CLICK, 0, 0 );
    for (i = 0; i < 40 && IsWindow( dlg ); i++) Sleep( 50 );
    if (IsWindow( dlg )) PostMessageW( dlg, WM_COMMAND, IDCANCEL, 0 );   /* still open: give up */
    return 0;
}

static HRESULT pick( const WCHAR *folder, const WCHAR *title, const WCHAR *typed, WCHAR *out )
{
    IFileOpenDialog *dlg;
    IShellItem *item, *result;
    FILEOPENDIALOGOPTIONS opts;
    HANDLE th;
    WCHAR *path;
    HRESULT hr;

    out[0] = 0;
    if (FAILED(hr = CoCreateInstance( &CLSID_FileOpenDialog, NULL, CLSCTX_INPROC_SERVER, &IID_IFileOpenDialog, (void **)&dlg )))
        return hr;
    IFileOpenDialog_GetOptions( dlg, &opts );
    IFileOpenDialog_SetOptions( dlg, opts | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM );
    IFileOpenDialog_SetTitle( dlg, title );
    if (SUCCEEDED(SHCreateItemFromParsingName( folder, NULL, &IID_IShellItem, (void **)&item )))
    {
        IFileOpenDialog_SetFolder( dlg, item );
        IShellItem_Release( item );
    }
    g_title = title;
    lstrcpyW( g_type, typed );
    th = CreateThread( NULL, 0, presser, NULL, 0, NULL );
    hr = IFileOpenDialog_Show( dlg, NULL );
    WaitForSingleObject( th, 10000 );
    CloseHandle( th );
    if (hr == S_OK && SUCCEEDED(IFileOpenDialog_GetResult( dlg, &result )))
    {
        if (SUCCEEDED(IShellItem_GetDisplayName( result, SIGDN_FILESYSPATH, &path )))
        {
            lstrcpyW( out, path );
            CoTaskMemFree( path );
        }
        IShellItem_Release( result );
    }
    IFileOpenDialog_Release( dlg );
    return hr;
}

int main(void)
{
    WCHAR out[MAX_PATH];
    HRESULT hr;

    CoInitializeEx( NULL, COINIT_APARTMENTTHREADED );
    CreateDirectoryW( L"C:\\pick", NULL );
    CreateDirectoryW( L"C:\\pick\\sub", NULL );

    hr = pick( L"C:\\pick", L"Pick probe one", L"", out );
    printf( "emptyname=%d\n", hr == S_OK && !lstrcmpiW( out, L"C:\\pick" ) );
    if (hr != S_OK || lstrcmpiW( out, L"C:\\pick" )) printf( "  hr=%#lx out=%ls\n", hr, out );
    hr = pick( L"C:\\pick", L"Pick probe two", L"sub", out );
    printf( "typedname=%d\n", hr == S_OK && !lstrcmpiW( out, L"C:\\pick\\sub" ) );
    if (hr != S_OK || lstrcmpiW( out, L"C:\\pick\\sub" )) printf( "  hr=%#lx out=%ls\n", hr, out );
    printf( "done=1\n" );
    CoUninitialize();
    return 0;
}
