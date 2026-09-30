/* Create shortcut (patches/sg/0596). The shell menu's Create shortcut did
 * nothing. "here": a shortcut beside C:\file.txt ("file.txt - Shortcut.lnk").
 * "readonly": for C:\readonly\file.txt, whose folder cannot be written, the
 * user is asked about the desktop instead (the probe answers Yes) and the
 * shortcut is on the desktop. */
#define COBJMACROS
#include <windows.h>
#include <shlobj.h>
#include <stdio.h>

static DWORD WINAPI answer_yes( void *arg )
{
    HWND box = NULL;
    int i;
    for (i = 0; i < 100 && !(box = FindWindowW( L"#32770", L"Shortcut" )); i++) Sleep( 100 );
    if (box) { Sleep( 200 ); PostMessageW( box, WM_COMMAND, IDYES, 0 ); }
    return box != NULL;
}

static void invoke_link( const WCHAR *file )
{
    IShellFolder *parent;
    IContextMenu *cm;
    LPITEMIDLIST pidl;
    LPCITEMIDLIST child;
    CMINVOKECOMMANDINFO ici = { sizeof(ici) };
    HMENU menu = CreatePopupMenu();

    if (FAILED(SHParseDisplayName( file, NULL, &pidl, 0, NULL )) ||
        FAILED(SHBindToParent( pidl, &IID_IShellFolder, (void **)&parent, &child )) ||
        FAILED(IShellFolder_GetUIObjectOf( parent, NULL, 1, &child, &IID_IContextMenu, NULL, (void **)&cm )))
        return;
    IContextMenu_QueryContextMenu( cm, menu, 0, 1, 0x7fff, CMF_NORMAL );
    ici.lpVerb = "link";
    ici.nShow = SW_SHOWNORMAL;
    IContextMenu_InvokeCommand( cm, &ici );
}

int main( int argc, char **argv )
{
    WCHAR desk[MAX_PATH], path[MAX_PATH];
    BOOL ok;

    CoInitialize( NULL );
    if (argc > 1 && !strcmp( argv[1], "here" ))
    {
        CloseHandle( CreateFileW( L"C:\\file.txt", GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL ) );
        invoke_link( L"C:\\file.txt" );
        ok = GetFileAttributesW( L"C:\\file.txt - Shortcut.lnk" ) != INVALID_FILE_ATTRIBUTES;
        printf( "here: %d\n", ok );
        return !ok;
    }
    if (argc > 1 && !strcmp( argv[1], "readonly" ))
    {
        HANDLE t = CreateThread( NULL, 0, answer_yes, NULL, 0, NULL );
        DWORD asked = 0;
        invoke_link( L"C:\\readonly\\file.txt" );
        WaitForSingleObject( t, 12000 );
        GetExitCodeThread( t, &asked );
        SHGetSpecialFolderPathW( NULL, desk, CSIDL_DESKTOPDIRECTORY, FALSE );
        swprintf( path, MAX_PATH, L"%ls\\file.txt - Shortcut.lnk", desk );
        ok = asked && GetFileAttributesW( path ) != INVALID_FILE_ATTRIBUTES;
        printf( "readonly: asked=%lu on desktop=%d (%ls)\n", asked, GetFileAttributesW( path ) != INVALID_FILE_ATTRIBUTES, path );
        return !ok;
    }
    return 2;
}
