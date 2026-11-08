/* test/msixapps-gate.sh's probe: a package's applications as the shell sees them.
 *
 *   activate AUMID [ARGS]        IApplicationActivationManager::ActivateApplication, wait for it
 *   activatefile AUMID PATH      ...::ActivateForFile with that file
 *   exec FILE [PARAMS]           ShellExecuteEx (no UI), wait for the process it started
 *   reg ROOT PATH [NAME]         a registry value (ROOT: HKCR, HKCU, HKLM), "Value=..." or "Value=(missing)"
 *   haskey ROOT PATH             "Key=1" or "Key=0"
 *   lnk PATH                     a shortcut's AppUserModel.ID, icon file and the icon's images
 *   icon PATH                    icons ExtractIconEx finds in the file
 *   findfamily FAMILY FILTER     kernel32 FindPackagesByPackageFamily (FILTER hex): Count=, Name=, Props=
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#define COBJMACROS
#include <windows.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <shellapi.h>
#include <propkey.h>
#include <propvarutil.h>
#include <stdio.h>

static const CLSID clsid_manager = {0x45ba127d,0x10a8,0x46ea,{0x8a,0xb7,0x56,0xea,0x90,0x78,0x94,0x3c}};
static const IID iid_manager = {0x2e941141,0x7f97,0x4756,{0xba,0x1d,0x9d,0xec,0xde,0x89,0x4a,0x3d}};

static void wait_pid( DWORD pid )
{
    HANDLE process = pid ? OpenProcess( SYNCHRONIZE, FALSE, pid ) : NULL;
    printf( "Pid=%s\n", pid ? "set" : "none" );
    if (process)
    {
        printf( "Wait=%lu\n", WaitForSingleObject( process, 60000 ) );
        CloseHandle( process );
    }
}

static HKEY root_key( const char *name )
{
    if (!strcmp( name, "HKCR" )) return HKEY_CLASSES_ROOT;
    if (!strcmp( name, "HKCU" )) return HKEY_CURRENT_USER;
    return HKEY_LOCAL_MACHINE;
}

int wmain( int argc, WCHAR **argv )
{
    IApplicationActivationManager *manager = NULL;
    HRESULT hr;
    DWORD pid = 0;

    CoInitializeEx( NULL, COINIT_APARTMENTTHREADED );
    if (argc < 2) return 2;
    if (!wcscmp( argv[1], L"activate" ) || !wcscmp( argv[1], L"activatefile" ) || !wcscmp( argv[1], L"activatelocal" ))
    {
        DWORD ctx = wcscmp( argv[1], L"activatelocal" ) ? CLSCTX_INPROC_SERVER : CLSCTX_LOCAL_SERVER;
        hr = CoCreateInstance( &clsid_manager, NULL, ctx, &iid_manager, (void **)&manager );
        printf( "Create=0x%08lx\n", hr );
        if (FAILED(hr) || argc < 3) return 1;
        if (wcscmp( argv[1], L"activatefile" ))
            hr = IApplicationActivationManager_ActivateApplication( manager, argv[2], argc > 3 ? argv[3] : NULL, AO_NONE, &pid );
        else
        {
            IShellItem *item = NULL;
            IShellItemArray *items = NULL;

            if (argc < 4) return 2;
            hr = SHCreateItemFromParsingName( argv[3], NULL, &IID_IShellItem, (void **)&item );
            if (SUCCEEDED(hr)) hr = SHCreateShellItemArrayFromShellItem( item, &IID_IShellItemArray, (void **)&items );
            if (SUCCEEDED(hr)) hr = IApplicationActivationManager_ActivateForFile( manager, argv[2], items, NULL, &pid );
            if (items) IShellItemArray_Release( items );
            if (item) IShellItem_Release( item );
        }
        printf( "Activate=0x%08lx\n", hr );
        wait_pid( pid );
        IApplicationActivationManager_Release( manager );
    }
    else if (!wcscmp( argv[1], L"exec" ) && argc >= 3)
    {
        SHELLEXECUTEINFOW sei = { sizeof(sei) };
        BOOL ok;

        sei.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_FLAG_NO_UI;
        sei.lpFile = argv[2];
        sei.lpParameters = argc > 3 ? argv[3] : NULL;
        sei.nShow = SW_SHOWNORMAL;
        ok = ShellExecuteExW( &sei );
        printf( "Exec=%d\nInstApp=%Iu\n", ok, (ULONG_PTR)sei.hInstApp );
        if (sei.hProcess)
        {
            printf( "Process=1\nWait=%lu\n", WaitForSingleObject( sei.hProcess, 60000 ) );
            CloseHandle( sei.hProcess );
        }
        else printf( "Process=0\n" );
    }
    else if (!wcscmp( argv[1], L"reg" ) && argc >= 4)
    {
        char root[8], path[512], name[256] = "";
        WCHAR value[1024];
        DWORD size = sizeof(value), type;
        LSTATUS res;

        WideCharToMultiByte( CP_ACP, 0, argv[2], -1, root, sizeof(root), NULL, NULL );
        WideCharToMultiByte( CP_ACP, 0, argv[3], -1, path, sizeof(path), NULL, NULL );
        if (argc > 4) WideCharToMultiByte( CP_ACP, 0, argv[4], -1, name, sizeof(name), NULL, NULL );
        res = RegGetValueW( root_key( root ), argv[3], argc > 4 ? argv[4] : NULL, RRF_RT_ANY, &type, value, &size );
        if (res) printf( "Value=(missing)\n" );
        else if (type == REG_SZ || type == REG_EXPAND_SZ) printf( "Value=%ls\n", value );
        else if (type == REG_NONE) printf( "Value=(none)\n" );
        else printf( "Value=(type %lu)\n", type );
    }
    else if (!wcscmp( argv[1], L"haskey" ) && argc >= 4)
    {
        char root[8];
        HKEY key;

        WideCharToMultiByte( CP_ACP, 0, argv[2], -1, root, sizeof(root), NULL, NULL );
        if (!RegOpenKeyExW( root_key( root ), argv[3], 0, KEY_READ, &key ))
        {
            printf( "Key=1\n" );
            RegCloseKey( key );
        }
        else printf( "Key=0\n" );
    }
    else if (!wcscmp( argv[1], L"lnk" ) && argc >= 3)
    {
        IShellLinkW *link;
        IPersistFile *file;
        IPropertyStore *store;
        PROPVARIANT value;
        WCHAR icon[MAX_PATH] = L"";
        int index = 0;

        if (FAILED(CoCreateInstance( &CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, &IID_IShellLinkW, (void **)&link ))) return 1;
        IShellLinkW_QueryInterface( link, &IID_IPersistFile, (void **)&file );
        hr = IPersistFile_Load( file, argv[2], STGM_READ );
        printf( "Load=0x%08lx\n", hr );
        if (SUCCEEDED(IShellLinkW_QueryInterface( link, &IID_IPropertyStore, (void **)&store )))
        {
            PropVariantInit( &value );
            if (SUCCEEDED(IPropertyStore_GetValue( store, &PKEY_AppUserModel_ID, &value )) && value.vt == VT_LPWSTR)
                printf( "AUMID=%ls\n", value.pwszVal );
            else printf( "AUMID=(none)\n" );
            PropVariantClear( &value );
            IPropertyStore_Release( store );
        }
        IShellLinkW_GetIconLocation( link, icon, MAX_PATH, &index );
        printf( "Icon=%ls\n", icon[0] ? icon : L"(none)" );
        if (icon[0])
        {
            HANDLE f = CreateFileW( icon, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL );
            WORD header[3] = { 0 };
            DWORD read;
            if (f != INVALID_HANDLE_VALUE)
            {
                ReadFile( f, header, sizeof(header), &read, NULL );
                CloseHandle( f );
            }
            printf( "IconImages=%u\n", header[1] == 1 ? header[2] : 0 );
        }
        IPersistFile_Release( file );
        IShellLinkW_Release( link );
    }
    else if (!wcscmp( argv[1], L"icon" ) && argc >= 3)
    {
        HICON large = NULL;
        UINT n = ExtractIconExW( argv[2], 0, &large, NULL, 1 );
        ICONINFO info;
        BITMAP bm = { 0 };

        printf( "Icons=%u\n", n );
        if (large && GetIconInfo( large, &info ))
        {
            GetObjectW( info.hbmColor ? info.hbmColor : info.hbmMask, sizeof(bm), &bm );
            printf( "Width=%ld\n", bm.bmWidth );
        }
    }
    else if (!wcscmp( argv[1], L"findfamily" ) && argc >= 4)
    {
        typedef LONG (WINAPI *find_func)( const WCHAR *, UINT32, UINT32 *, WCHAR **, UINT32 *, WCHAR *, UINT32 * );
        find_func find = (find_func)GetProcAddress( GetModuleHandleW( L"kernel32" ), "FindPackagesByPackageFamily" );
        UINT32 count = 0, len = 0, props[8], i, filter = wcstoul( argv[3], NULL, 16 );
        WCHAR *names[8], buffer[1024];
        LONG ret;

        if (!find) { printf( "Find=(missing)\n" ); return 1; }
        ret = find( argv[2], filter, &count, NULL, &len, NULL, NULL );
        printf( "Sizing=%ld\nCount=%u\n", ret, count );
        if (count > 8 || len > ARRAYSIZE(buffer)) return 1;
        ret = find( argv[2], filter, &count, names, &len, buffer, props );
        printf( "Find=%ld\n", ret );
        for (i = 0; !ret && i < count; i++) printf( "Name=%ls\nProps=%#x\n", names[i], props[i] );
    }
    else return 2;
    return 0;
}
