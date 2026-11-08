/* test/pkgself-gate.sh's packaged probe (7713): a packaged desktop program's own identity.
 *   family          GetPackageFamilyName(GetCurrentProcess()), GetPackageFullName of an opened handle to
 *                   itself, of a child copy of itself, of an unpackaged process (cmd)
 *   package         Package.Current: IPackage2 DisplayName; its Id: IPackageId, class name, own identity
 *   tasks           StartupTask.GetForCurrentPackageAsync: count, each TaskId and State
 *   enable ID       GetAsync(ID), RequestEnableAsync: the state it returns
 *   disable ID      GetAsync(ID), Disable, State
 *   get ID          GetAsync(ID): the operation's status
 *   appdata         ApplicationData.Current: LocalCacheFolder (IApplicationData2), a publisher cache folder,
 *                   SharedLocalFolder, a bad folder name (IApplicationData3) -- 7717
 *   sleep           waits (the child)
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#include <windows.h>
#include <roapi.h>
#include <winstring.h>
#include <stdio.h>
#include <stdlib.h>

#define SLOT(obj, n, type) ((type)((*(void ***)(obj))[n]))
#define QI(obj, iid, out) SLOT( obj, 0, HRESULT (WINAPI *)( void *, const GUID *, void ** ) )( obj, iid, out )
#define RELEASE(obj) SLOT( obj, 2, ULONG (WINAPI *)( void * ) )( obj )

typedef LONG (WINAPI *name_func)( HANDLE, UINT32 *, WCHAR * );

static const GUID iid_unknown = {0x00000000,0,0,{0xc0,0,0,0,0,0,0,0x46}};
static const GUID iid_asyncinfo = {0x00000036,0,0,{0xc0,0,0,0,0,0,0,0x46}};
static const GUID iid_package_statics = {0x4e534bdf,0x2960,0x4878,{0x97,0xa4,0x96,0x24,0xde,0xb7,0x2f,0x2d}};
static const GUID iid_package2 = {0xa6612fb6,0x7688,0x4ace,{0x95,0xfb,0x35,0x95,0x38,0xe7,0xaa,0x01}};
static const GUID iid_package_id = {0x1adb665e,0x37c7,0x4790,{0x99,0x80,0xdd,0x7a,0xe7,0x4e,0x8b,0xb2}};
static const GUID iid_appdata_statics = {0x5612147b,0xe843,0x45e3,{0x94,0xd8,0x06,0x16,0x9e,0x3c,0x8e,0x17}};
static const GUID iid_appdata2 = {0x9e65cd69,0x0ba3,0x4e32,{0xbe,0x29,0xb0,0x2d,0xe6,0x60,0x76,0x38}};
static const GUID iid_appdata3 = {0xdc222cf4,0x2772,0x4c1d,{0xaa,0x2c,0xc9,0xf7,0x43,0xad,0xe8,0xd1}};
static const GUID iid_storage_item = {0x4207a996,0xca2f,0x42f7,{0xbd,0xe8,0x8b,0x10,0x45,0x7a,0x7f,0x30}};
static const GUID iid_task_statics = {0xee5b60bd,0xa148,0x41a7,{0xb2,0x6e,0xe8,0xb8,0x8a,0x1e,0x62,0xf8}};

static void *factory( const WCHAR *class, const GUID *iid )
{
    HSTRING name;
    void *out = NULL;
    HRESULT hr;
    WindowsCreateString( class, wcslen( class ), &name );
    hr = RoGetActivationFactory( name, iid, &out );
    printf( "Factory=0x%08lx\n", hr );
    return out;
}

static void print_hstring( const char *key, HSTRING s )
{
    printf( "%s=%ls\n", key, s ? WindowsGetStringRawBuffer( s, NULL ) : L"" );
}

static int cmd_family( const WCHAR *self )
{
    HMODULE k32 = GetModuleHandleW( L"kernel32" );
    name_func family = (name_func)GetProcAddress( k32, "GetPackageFamilyName" );
    name_func full = (name_func)GetProcAddress( k32, "GetPackageFullName" );
    WCHAR buf[256], cmd[MAX_PATH + 16];
    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    UINT32 len;
    HANDLE own;
    LONG ret;

    len = ARRAYSIZE(buf); ret = family( GetCurrentProcess(), &len, buf );
    printf( "FamilyRet=%ld\nFamily=%ls\n", ret, ret ? L"" : buf );
    len = 0; ret = family( GetCurrentProcess(), &len, NULL );
    printf( "FamilySizeRet=%ld\nFamilySize=%u\n", ret, len );
    own = OpenProcess( PROCESS_QUERY_LIMITED_INFORMATION, FALSE, GetCurrentProcessId() );
    len = ARRAYSIZE(buf); ret = full( own, &len, buf );
    printf( "FullSelf=%ls\n", ret ? L"" : buf );
    CloseHandle( own );
    GetModuleFileNameW( NULL, buf, ARRAYSIZE(buf) );
    swprintf( cmd, ARRAYSIZE(cmd), L"\"%ls\" sleep", buf );
    if (!CreateProcessW( NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi )) printf( "ChildError=%lu %ls\n", GetLastError(), cmd );
    else
    {
        Sleep( 1000 );
        len = ARRAYSIZE(buf); ret = full( pi.hProcess, &len, buf );
        printf( "FullChild=%ls\n", ret ? L"" : buf );
        TerminateProcess( pi.hProcess, 0 );
        CloseHandle( pi.hThread );
        CloseHandle( pi.hProcess );
    }
    wcscpy( cmd, L"C:\\windows\\system32\\cmd.exe /c ping -n 3 127.0.0.1 >nul" );
    if (CreateProcessW( NULL, cmd, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi ))
    {
        Sleep( 500 );
        len = ARRAYSIZE(buf); ret = full( pi.hProcess, &len, buf );
        printf( "OtherRet=%ld\n", ret );
        TerminateProcess( pi.hProcess, 0 );
        CloseHandle( pi.hThread );
        CloseHandle( pi.hProcess );
    }
    return 0;
}

static int cmd_package(void)
{
    void *statics = factory( L"Windows.ApplicationModel.Package", &iid_package_statics ), *pkg = NULL, *pkg2 = NULL;
    void *id = NULL, *id_iface = NULL, *unk_pkg = NULL, *unk_id = NULL;
    HSTRING s = NULL;
    HRESULT hr;

    if (!statics) return 1;
    hr = SLOT( statics, 6, HRESULT (WINAPI *)( void *, void ** ) )( statics, &pkg );
    printf( "Current=0x%08lx\n", hr );
    if (FAILED(hr)) return 1;
    hr = QI( pkg, &iid_package2, &pkg2 );
    printf( "Package2=0x%08lx\n", hr );
    if (SUCCEEDED(hr) && SUCCEEDED(SLOT( pkg2, 6, HRESULT (WINAPI *)( void *, HSTRING * ) )( pkg2, &s )))
        print_hstring( "DisplayName", s );
    hr = SLOT( pkg, 6, HRESULT (WINAPI *)( void *, void ** ) )( pkg, &id );
    printf( "Id=0x%08lx\n", hr );
    if (FAILED(hr)) return 1;
    printf( "IdQI=0x%08lx\n", QI( id, &iid_package_id, &id_iface ) );
    if (SUCCEEDED(SLOT( id, 4, HRESULT (WINAPI *)( void *, HSTRING * ) )( id, &s ))) print_hstring( "IdClass", s );
    QI( pkg, &iid_unknown, &unk_pkg );
    QI( id, &iid_unknown, &unk_id );
    printf( "IdOwnObject=%d\n", unk_pkg != unk_id );
    if (id_iface && SUCCEEDED(SLOT( id_iface, 13, HRESULT (WINAPI *)( void *, HSTRING * ) )( id_iface, &s )))
        print_hstring( "IdFamily", s );
    return 0;
}

static void print_folder_path( const char *key, void *folder )
{
    void *item = NULL;
    HSTRING path = NULL;
    if (folder && SUCCEEDED(QI( folder, &iid_storage_item, &item )))
        SLOT( item, 12, HRESULT (WINAPI *)( void *, HSTRING * ) )( item, &path );
    print_hstring( key, path );
}

static int cmd_appdata(void)
{
    void *statics = factory( L"Windows.Storage.ApplicationData", &iid_appdata_statics ), *data = NULL, *d2 = NULL, *d3 = NULL;
    void *folder = NULL;
    HSTRING name;
    HRESULT hr;

    if (!statics) return 1;
    hr = SLOT( statics, 6, HRESULT (WINAPI *)( void *, void ** ) )( statics, &data );
    printf( "AppData=0x%08lx\n", hr );
    if (FAILED(hr)) return 1;
    printf( "Data2=0x%08lx\n", QI( data, &iid_appdata2, &d2 ) );
    printf( "Data3=0x%08lx\n", QI( data, &iid_appdata3, &d3 ) );
    if (d2)
    {
        hr = SLOT( d2, 6, HRESULT (WINAPI *)( void *, void ** ) )( d2, &folder );
        printf( "LocalCache=0x%08lx\n", hr );
        print_folder_path( "LocalCachePath", folder );
    }
    if (d3)
    {
        folder = NULL;
        WindowsCreateString( L"SgShared", 8, &name );
        hr = SLOT( d3, 6, HRESULT (WINAPI *)( void *, HSTRING, void ** ) )( d3, name, &folder );
        printf( "Publisher=0x%08lx\n", hr );
        print_folder_path( "PublisherPath", folder );
        folder = (void *)1;
        hr = SLOT( d3, 8, HRESULT (WINAPI *)( void *, void ** ) )( d3, &folder );
        printf( "Shared=0x%08lx %d\n", hr, folder == NULL );
        WindowsCreateString( L"a\\b", 3, &name );
        folder = NULL;
        printf( "BadName=0x%08lx\n", SLOT( d3, 6, HRESULT (WINAPI *)( void *, HSTRING, void ** ) )( d3, name, &folder ) );
    }
    return 0;
}

static void *op_results( void *op )
{
    void *result = NULL;
    HRESULT hr = SLOT( op, 8, HRESULT (WINAPI *)( void *, void ** ) )( op, &result );
    printf( "Results=0x%08lx\n", hr );
    return result;
}

static void print_task( void *task )
{
    HSTRING id = NULL;
    INT32 state = -1;
    SLOT( task, 9, HRESULT (WINAPI *)( void *, HSTRING * ) )( task, &id );
    SLOT( task, 8, HRESULT (WINAPI *)( void *, INT32 * ) )( task, &state );
    printf( "Task=%ls %d\n", WindowsGetStringRawBuffer( id, NULL ), state );
}

static void *get_task( void *statics, const WCHAR *id )
{
    HSTRING str;
    void *op = NULL, *info = NULL;
    INT32 status = -1;
    HRESULT hr;

    WindowsCreateString( id, wcslen( id ), &str );
    hr = SLOT( statics, 7, HRESULT (WINAPI *)( void *, HSTRING, void ** ) )( statics, str, &op );
    printf( "GetAsync=0x%08lx\n", hr );
    if (FAILED(hr)) return NULL;
    if (SUCCEEDED(QI( op, &iid_asyncinfo, &info ))) SLOT( info, 7, HRESULT (WINAPI *)( void *, INT32 * ) )( info, &status );
    printf( "Status=%d\n", status );
    return status == 1 ? op_results( op ) : NULL;
}

int wmain( int argc, WCHAR **argv )
{
    void *statics, *op = NULL, *view, *task;
    UINT32 n = 0, i;
    INT32 state = -1;
    HRESULT hr;

    setvbuf( stdout, NULL, _IONBF, 0 );
    RoInitialize( RO_INIT_MULTITHREADED );
    if (argc < 2) return 2;
    if (!wcscmp( argv[1], L"sleep" )) { Sleep( 30000 ); return 0; }
    if (!wcscmp( argv[1], L"family" )) return cmd_family( argv[0] );
    if (!wcscmp( argv[1], L"package" )) return cmd_package();
    if (!wcscmp( argv[1], L"appdata" )) return cmd_appdata();
    if (!(statics = factory( L"Windows.ApplicationModel.StartupTask", &iid_task_statics ))) return 1;
    if (!wcscmp( argv[1], L"tasks" ))
    {
        hr = SLOT( statics, 6, HRESULT (WINAPI *)( void *, void ** ) )( statics, &op );
        printf( "ForPackage=0x%08lx\n", hr );
        if (FAILED(hr) || !(view = op_results( op ))) return 1;
        SLOT( view, 7, HRESULT (WINAPI *)( void *, UINT32 * ) )( view, &n );
        printf( "Count=%u\n", n );
        for (i = 0; i < n; i++)
            if (SUCCEEDED(SLOT( view, 6, HRESULT (WINAPI *)( void *, UINT32, void ** ) )( view, i, &task ))) print_task( task );
        return 0;
    }
    if (argc < 3) return 2;
    task = get_task( statics, argv[2] );
    if (!wcscmp( argv[1], L"get" )) return 0;
    if (!task) return 1;
    if (!wcscmp( argv[1], L"enable" ))
    {
        hr = SLOT( task, 6, HRESULT (WINAPI *)( void *, void ** ) )( task, &op );
        printf( "Enable=0x%08lx\n", hr );
        if (SUCCEEDED(hr)) SLOT( op, 8, HRESULT (WINAPI *)( void *, INT32 * ) )( op, &state );
        printf( "EnableState=%d\n", state );
    }
    else if (!wcscmp( argv[1], L"disable" ))
    {
        printf( "Disable=0x%08lx\n", SLOT( task, 7, HRESULT (WINAPI *)( void * ) )( task ) );
    }
    print_task( task );
    return 0;
}
