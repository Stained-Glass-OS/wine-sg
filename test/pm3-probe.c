/* pm3-probe: PackageManager's volume methods, IPackageManager3 (Stained Glass OS wine-sg 6016).
 * Dropbox's sparse-package code asks PackageManager for IPackageManager3 and uses it unchecked.
 *   pm3-probe.exe */
#include <windows.h>
#include <roapi.h>
#include <winstring.h>
#include <stdio.h>

static int failures;

static void check( int ok, const char *what, long got )
{
    if (ok) printf( "ok    %s\n", what );
    else { printf( "FAIL  %s (got %#lx)\n", what, got ); failures++; }
}

typedef HRESULT (WINAPI *method)( void * );
#define VT(obj) (*(void ***)(obj))
#define CALL(obj, n, type, ...) ((type)VT(obj)[n])( obj, ##__VA_ARGS__ )
typedef HRESULT (WINAPI *qi_fn)( void *, const GUID *, void ** );
typedef ULONG (WINAPI *release_fn)( void * );
typedef HRESULT (WINAPI *get_ptr_fn)( void *, void ** );
typedef HRESULT (WINAPI *get_bool_fn)( void *, BOOLEAN * );
typedef HRESULT (WINAPI *get_str_fn)( void *, HSTRING * );
typedef HRESULT (WINAPI *by_name_fn)( void *, HSTRING, void ** );
typedef HRESULT (WINAPI *ptr_fn)( void *, void * );
typedef HRESULT (WINAPI *move_fn)( void *, HSTRING, UINT32, void *, void ** );
typedef HRESULT (WINAPI *add_fn)( void *, void *, void *, UINT32, void *, void ** );
typedef HRESULT (WINAPI *status_fn)( void *, HSTRING, UINT32 );
typedef HRESULT (WINAPI *async_status_fn)( void *, int * );
typedef HRESULT (WINAPI *async_error_fn)( void *, HRESULT * );

static const GUID IID_IPackageManager_ = {0x9a7d4b65,0x5e8f,0x4fc7,{0xa2,0xe5,0x7f,0x69,0x25,0xcb,0x8b,0x53}};
static const GUID IID_IPackageManager3_ = {0xdaad9948,0x36f1,0x41a7,{0x91,0x88,0xbc,0x26,0x3e,0x0d,0xcb,0x72}};
static const GUID IID_IPackageVolume_ = {0xcf2672c3,0x1a40,0x4450,{0x97,0x39,0x2a,0xce,0x2e,0x89,0x88,0x53}};
static const GUID IID_IAsyncInfo_ = {0x00000036,0x0000,0x0000,{0xc0,0x00,0x00,0x00,0x00,0x00,0x00,0x46}};

enum { PM3_AddVolume = 6, PM3_AddToVolume, PM3_ClearStatus, PM3_RegisterAppData, PM3_FindVolumeByName,
       PM3_FindVolumes, PM3_GetDefaultVolume, PM3_Move, PM3_RemoveVolume, PM3_SetDefaultVolume, PM3_SetStatus,
       PM3_Offline, PM3_Online, PM3_StageToVolume, PM3_StageUserData };
enum { PV_IsOffline = 6, PV_IsSystemVolume, PV_MountPoint, PV_Name, PV_PackageStorePath, PV_SupportsHardLinks };

static HSTRING hs( const WCHAR *s )
{
    HSTRING h = NULL;
    WindowsCreateString( s, lstrlenW( s ), &h );
    return h;
}

/* the operation's outcome: its error code when it is done */
static HRESULT wait_op( void *op )
{
    void *info = NULL;
    int status = 0, i;
    HRESULT err = E_FAIL;

    if (FAILED(CALL( op, 0, qi_fn, &IID_IAsyncInfo_, &info ))) return E_NOINTERFACE;
    for (i = 0; i < 300; i++)
    {
        CALL( info, 7, async_status_fn, &status );
        if (status) break;
        Sleep( 50 );
    }
    if (status == 1) err = S_OK;
    else if (status) CALL( info, 8, async_error_fn, &err );
    else err = E_PENDING;
    CALL( info, 2, release_fn );
    return err;
}

int main( void )
{
    void *factory_unk = NULL, *manager = NULL, *pm3 = NULL, *vol = NULL, *vol2 = NULL, *volumes = NULL, *it = NULL;
    void *op = NULL, *cur = NULL;
    HSTRING cls, name = NULL, mount = NULL, store = NULL;
    BOOLEAN b = 2;
    HRESULT hr;

    RoInitialize( RO_INIT_MULTITHREADED );
    cls = hs( L"Windows.Management.Deployment.PackageManager" );
    hr = RoActivateInstance( cls, (IInspectable **)&factory_unk );
    WindowsDeleteString( cls );
    if (FAILED(hr)) { printf( "FAIL  no PackageManager (%#lx)\nRESULT: FAIL\n", hr ); return 1; }
    CALL( factory_unk, 0, qi_fn, &IID_IPackageManager_, &manager );

    hr = CALL( manager, 0, qi_fn, &IID_IPackageManager3_, &pm3 );
    check( hr == S_OK && pm3, "PackageManager has IPackageManager3", hr );
    if (!pm3) { printf( "RESULT: FAIL\n" ); return 1; }

    hr = CALL( pm3, PM3_GetDefaultVolume, get_ptr_fn, &vol );
    check( hr == S_OK && vol, "GetDefaultPackageVolume", hr );
    if (vol)
    {
        hr = CALL( vol, 0, qi_fn, &IID_IPackageVolume_, &vol2 );
        check( hr == S_OK, "it is an IPackageVolume", hr );
        if (vol2) CALL( vol2, 2, release_fn );
        vol2 = NULL;
        b = 2; hr = CALL( vol, PV_IsOffline, get_bool_fn, &b );
        check( hr == S_OK && b == FALSE, "the volume is online", b );
        b = 2; hr = CALL( vol, PV_IsSystemVolume, get_bool_fn, &b );
        check( hr == S_OK && b == TRUE, "it is the system volume", b );
        hr = CALL( vol, PV_MountPoint, get_str_fn, &mount );
        check( hr == S_OK && !lstrcmpiW( WindowsGetStringRawBuffer( mount, NULL ), L"C:\\" ), "its mount point is C:\\", hr );
        hr = CALL( vol, PV_Name, get_str_fn, &name );
        check( hr == S_OK && !wcsncmp( WindowsGetStringRawBuffer( name, NULL ), L"\\\\?\\Volume{", 11 ),
               "its name is the volume's \\\\?\\Volume{...}", hr );
        hr = CALL( vol, PV_PackageStorePath, get_str_fn, &store );
        check( hr == S_OK && wcsstr( WindowsGetStringRawBuffer( store, NULL ), L"\\WindowsApps" ),
               "its package store is ...\\WindowsApps", hr );
        printf( "      %ls %ls %ls\n", WindowsGetStringRawBuffer( name, NULL ), WindowsGetStringRawBuffer( mount, NULL ),
                WindowsGetStringRawBuffer( store, NULL ) );
    }

    hr = CALL( pm3, PM3_FindVolumes, get_ptr_fn, &volumes );
    check( hr == S_OK && volumes, "FindPackageVolumes", hr );
    if (volumes)
    {
        int count = 0;
        hr = CALL( volumes, 6, get_ptr_fn, &it );
        while (hr == S_OK && it && CALL( it, 7, get_bool_fn, &b ) == S_OK && b && count < 5)
        {
            if (CALL( it, 6, get_ptr_fn, &cur ) == S_OK) { count++; CALL( cur, 2, release_fn ); }
            CALL( it, 8, get_bool_fn, &b );
        }
        check( count == 1, "one volume", count );
        if (it) CALL( it, 2, release_fn );
        CALL( volumes, 2, release_fn );
    }

    hr = CALL( pm3, PM3_FindVolumeByName, by_name_fn, name, &vol2 );
    check( hr == S_OK && vol2, "FindPackageVolumeByName of its name", hr );
    if (vol2) { CALL( vol2, 2, release_fn ); vol2 = NULL; }
    hr = CALL( pm3, PM3_FindVolumeByName, by_name_fn, mount, &vol2 );
    check( hr == S_OK && vol2, "and of its mount point", hr );
    if (vol2) { CALL( vol2, 2, release_fn ); vol2 = NULL; }
    {
        HSTRING other = hs( L"\\\\?\\Volume{01234567-89ab-cdef-0123-456789abcdef}\\" );
        hr = CALL( pm3, PM3_FindVolumeByName, by_name_fn, other, &vol2 );
        check( hr == HRESULT_FROM_WIN32( ERROR_NOT_FOUND ) && !vol2, "another volume is not found", hr );
        WindowsDeleteString( other );
    }

    hr = CALL( pm3, PM3_SetDefaultVolume, ptr_fn, vol );
    check( hr == S_OK, "SetDefaultPackageVolume of it", hr );

    {
        HSTRING missing = hs( L"Nobody.Missing_1.0.0.0_x64__0000000000000" );
        hr = CALL( pm3, PM3_Move, move_fn, missing, 0, vol, &op );
        check( hr == S_OK && op, "MovePackageToVolumeAsync gives an operation", hr );
        if (op)
        {
            hr = wait_op( op );
            check( hr == HRESULT_FROM_WIN32( ERROR_NOT_FOUND ), "which fails for a package that is not there", hr );
            CALL( op, 2, release_fn );
            op = NULL;
        }
        hr = CALL( pm3, PM3_ClearStatus, status_fn, missing, 1 );
        check( FAILED(hr), "ClearPackageStatus of a package that is not there fails", hr );
        WindowsDeleteString( missing );
    }
    op = NULL;
    {
        typedef HRESULT (WINAPI *vol_op_fn)( void *, void *, void ** );
        hr = CALL( pm3, PM3_RemoveVolume, vol_op_fn, vol, &op );
        check( hr == S_OK && op && wait_op( op ) == HRESULT_FROM_WIN32( ERROR_NOT_SUPPORTED ),
               "the system volume is not removed", hr );
        if (op) { CALL( op, 2, release_fn ); op = NULL; }
        hr = CALL( pm3, PM3_Online, vol_op_fn, vol, &op );
        check( hr == S_OK && op && wait_op( op ) == S_OK, "SetPackageVolumeOnlineAsync", hr );
        if (op) { CALL( op, 2, release_fn ); op = NULL; }
    }
    hr = CALL( pm3, PM3_AddVolume, by_name_fn, store, &op );
    check( hr == HRESULT_FROM_WIN32( ERROR_NOT_SUPPORTED ), "AddPackageVolumeAsync: one volume only", hr );
    hr = CALL( pm3, PM3_AddToVolume, add_fn, NULL, NULL, 0, vol, &op );
    check( hr == E_INVALIDARG, "AddPackageToVolumeAsync of no package: E_INVALIDARG", hr );

    printf( "RESULT: %s\n", failures ? "FAIL" : "PASS" );
    return failures != 0;
}
